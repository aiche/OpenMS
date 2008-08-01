// -*- mode: C++; tab-width: 2; -*-
// vi: set ts=2:
//
// --------------------------------------------------------------------------
//                   OpenMS Mass Spectrometry Framework
// --------------------------------------------------------------------------
//  Copyright (C) 2003-2008 -- Oliver Kohlbacher, Knut Reinert
//
//  This library is free software; you can redistribute it and/or
//  modify it under the terms of the GNU Lesser General Public
//  License as published by the Free Software Foundation; either
//  version 2.1 of the License, or (at your option) any later version.
//
//  This library is distributed in the hope that it will be useful,
//  but WITHOUT ANY WARRANTY; without even the implied warranty of
//  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
//  Lesser General Public License for more details.
//
//  You should have received a copy of the GNU Lesser General Public
//  License along with this library; if not, write to the Free Software
//  Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA
//
// --------------------------------------------------------------------------
// $Maintainer: Marc Sturm$
// --------------------------------------------------------------------------

#ifndef OPENMS_TRANSFORMATIONS_FEATUREFINDER_FEATUREFINDERALGORITHMPICKED_H
#define OPENMS_TRANSFORMATIONS_FEATUREFINDER_FEATUREFINDERALGORITHMPICKED_H

#include <OpenMS/TRANSFORMATIONS/FEATUREFINDER/FeatureFinderAlgorithm.h>
#include <OpenMS/FORMAT/MzDataFile.h>
#include <OpenMS/FORMAT/FeatureXMLFile.h>
#include <OpenMS/FORMAT/TextFile.h>
#include <OpenMS/CHEMISTRY/IsotopeDistribution.h>
#include <OpenMS/MATH/STATISTICS/Histogram.h>
#include <OpenMS/MATH/STATISTICS/StatisticFunctions.h>

#include <numeric>
#include <fstream>

#include <gsl/gsl_rng.h>
#include <gsl/gsl_vector.h>
#include <gsl/gsl_multifit_nlin.h>
#include <gsl/gsl_blas.h>

#include <QtCore/QDir>

namespace OpenMS
{
	/** 
		@brief FeatureFinderAlgorithm for picked peaks.

    @ref FeatureFinderAlgorithmPicked_Parameters are explained on a separate page.

		@todo Add RT model with tailing/fronting (Marc)
		@todo Add choise monoisotopic vs. average feature mass (Marc)
		@improvement The elution profile could be integrated into the trace score (Marc)
				
		@experimental This algorithm is work in progress and might change.

		@ingroup FeatureFinder
	*/
	template<class PeakType, class FeatureType> class FeatureFinderAlgorithmPicked 
		: public FeatureFinderAlgorithm<PeakType, FeatureType>,
			public FeatureFinderDefs
	{
		public:
			///@name Type definitions
			//@{
			typedef typename FeatureFinderAlgorithm<PeakType, FeatureType>::MapType MapType;
			typedef typename MapType::SpectrumType SpectrumType;
			typedef typename SpectrumType::MetaDataArrays MetaDataArrays;
			//@}
			
			using FeatureFinderAlgorithm<PeakType, FeatureType>::param_;
				
		protected:
			///Helper structure for seeds used in FeatureFinderAlgorithmPicked
			struct Seed
			{
				///Spectrum index
				UInt spectrum;
				///Peak index
				UInt peak;
				///Intensity
				Real intensity;
				
				/// Comparison operator
				bool operator<(const Seed& rhs) const
				{
					return intensity<rhs.intensity;
				}
			};
			
			///Helper struct for mass traces used in FeatureFinderAlgorithmPicked
			struct MassTrace
			{
				///Maximum peak pointer
				const PeakType* max_peak;
				///RT of maximum peak
				DoubleReal max_rt;

				///Theoretical intensity value (scaled to [0,1])
				DoubleReal theoretical_int;
				
				///Contained peaks (pair of RT and pointer to peak)
				std::vector<std::pair<DoubleReal, const PeakType*> > peaks;
				
				///determindes the convex hull of the trace
				ConvexHull2D getConvexhull() const
				{
					ConvexHull2D::PointArrayType hull_points(peaks.size());
					for (UInt i=0; i<peaks.size(); ++i)
					{
						hull_points[i][0] = peaks[i].first;
						hull_points[i][1] = peaks[i].second->getMZ();
					}
					return hull_points;
				}
				
				///Sets the maximum to the highest contained peak of the trace
				void updateMaximum()
				{
					if (peaks.size()==0) return;

					max_rt = peaks.begin()->first;					
					max_peak = peaks.begin()->second;
					
					for (UInt i=1; i<peaks.size(); ++i)
					{
						if (peaks[i].second->getIntensity()>max_peak->getIntensity())
						{
							max_rt = peaks[i].first;					
							max_peak = peaks[i].second;
						}
					}
				}

				///Returns the average m/z of all peaks in this trace
				DoubleReal getAvgMZ() const
				{
					DoubleReal sum = 0.0;
					for (UInt i=0; i<peaks.size(); ++i)
					{
						sum += peaks[i].second->getMZ();
					}
					return sum / peaks.size();
				}
				
				///Checks if this Trace is valid (has more than 2 points)
				bool isValid() const
				{
					return (peaks.size()>=3);
				}
				
			};
			
			///Helper struct for a collection of mass traces used in FeatureFinderAlgorithmPicked
			struct MassTraces
				: public std::vector<MassTrace>
			{
				/// Constructor
				MassTraces()
					: max_trace(0)
				{
				}
				
				/// Returns the peak count of all traces
				UInt getPeakCount() const
				{
					UInt sum = 0;
					for (UInt i=0; i<this->size(); ++i)
					{
						sum += this->at(i).peaks.size();
					}
					return sum;
				}
				
				///Checks if still valid (seed still contained and enough traces)
				bool isValid(DoubleReal seed_mz, DoubleReal trace_tolerance)
				{
					//Abort if too few traces were found
					if (this->size()<2) return false;

					//Abort if the seed was removed
					for (UInt j=0; j<this->size(); ++j)
					{
						if (std::fabs(seed_mz-this->at(j).getAvgMZ())<=trace_tolerance)
						{
							return true;
						}
					}
					return false;
				}
				
				/**
				  @brief Returns the theoretical maximum trace index

				  @exception Exception::Precondition is thrown if there are not mass traces (not only in debug mode)
				*/
				UInt getTheoreticalMax() const
				{
					if (!this->size())
					{
						throw Exception::Precondition(__FILE__,__LINE__,__PRETTY_FUNCTION__,"There must be at least one trace to determine the theoretical maximum trace!");
					}
					
					UInt max=0;
					DoubleReal max_int=this->at(0).theoretical_int;
					for (UInt i=1; i<this->size(); ++i)
					{
						if (this->at(i).theoretical_int>max_int)
						{
							max_int = this->at(i).theoretical_int;
							max = i;
						}
					}
					return max;
				}

				///Sets the baseline to the lowest contained peak of the trace
				void updateBaseline()
				{
					if (this->size()==0)
					{
						baseline = 0.0;
						return;
					}
					bool first = true;					
					for (UInt i=0; i<this->size(); ++i)
					{
						for (UInt j=0; j<this->at(i).peaks.size(); ++j)
						{
							if (first)
							{
								baseline = baseline = this->at(i).peaks[j].second->getIntensity();
								first = false;
							}
							if (this->at(i).peaks[j].second->getIntensity()<baseline)
							{
								baseline = this->at(i).peaks[j].second->getIntensity();
							}
						}
					}
				}

				/**
				  @brief Returns the RT boundaries of the mass traces

				  @exception Exception::Precondition is thrown if there are no mass traces (not only in debug mode)
				*/
				std::pair<DoubleReal,DoubleReal> getRTBounds() const
				{
					if (!this->size())
					{
						throw Exception::Precondition(__FILE__,__LINE__,__PRETTY_FUNCTION__,"There must be at least one trace to determine the RT boundaries!");
					}
					
					DoubleReal min = std::numeric_limits<DoubleReal>::max();
					DoubleReal max = -std::numeric_limits<DoubleReal>::max();
					//Abort if the seed was removed
					for (UInt i=0; i<this->size(); ++i)
					{
						for (UInt j=0; j<this->at(i).peaks.size(); ++j)
						{
							DoubleReal rt = this->at(i).peaks[j].first;
							if (rt>max) max = rt;
							if (rt<min) min = rt;
						}
					}
					return std::make_pair(min,max);
				}

				/// Maximum intensity trace
				UInt max_trace;
				/// Estimated baseline in the region of the feature (used for the fit)
				DoubleReal baseline;
			};
			
			///Helper structure for a theoretical isotope pattern used in FeatureFinderAlgorithmPicked
			struct TheoreticalIsotopePattern
			{
				///Vector of intensity contributions 
				std::vector<DoubleReal> intensity;
				///Number of optional peaks at the beginning of the pattern
				UInt optional_begin;
				///Number of optional peaks at the end of the pattern
				UInt optional_end;
				///The maximum intensity contribution before scaling the pattern to 1
				DoubleReal max;
				/// Returns the size
				UInt size() const
				{
					return intensity.size();
				}
			};

			///Helper structure for a found isotope pattern used in FeatureFinderAlgorithmPicked
			struct IsotopePattern
			{
				///Peak index (-1 if peak was not found, -2 if it was removed to improve the isotope fit)
				std::vector<Int> peak;
				///Spectrum index (undefined if peak index is -1 or -2)
				std::vector<UInt> spectrum;
				///Peak intensity (0 if peak index is -1 or -2)
				std::vector<DoubleReal> intensity;
				///m/z score of peak (0 if peak index is -1 or -2)
				std::vector<DoubleReal> mz_score;
				///Theoretical m/z value of the isotope peak
				std::vector<DoubleReal> theoretical_mz;
				///Theoretical isotope pattern
				TheoreticalIsotopePattern theoretical_ints;
				
				/// Constructor that resizes the internal vectors
				IsotopePattern(UInt size)
					: peak(size),
						spectrum(size),
						intensity(size),
						mz_score(size),
						theoretical_mz(size)
				{
				}
			};
			

		public:			
			/// default constructor 
			FeatureFinderAlgorithmPicked() 
				: FeatureFinderAlgorithm<PeakType,FeatureType>(),
					map_(),
					log_("featurefinder.log")
			{
				//debugging
				this->defaults_.setValue("debug","false","When debug mode is activated, several files with intermediate results are written to the folder 'debug'.");
				this->defaults_.setValidStrings("debug",StringList::create("true,false"));
				//intensity
				this->defaults_.setValue("intensity:bins",10,"Number of bins per dimension (RT and m/z).");
				this->defaults_.setMinInt("intensity:bins",1);
				this->defaults_.setSectionDescription("intensity","Settings for the calculation of a score indicating if a peak's intensity is significant in the local environment (between 0 and 1)");
				//mass trace search parameters
				this->defaults_.setValue("mass_trace:mz_tolerance",0.03,"m/z difference tolerance of peaks belonging to the same mass trace.\n This value must be smaller than that 1/charge_high!");
				this->defaults_.setMinFloat("mass_trace:mz_tolerance",0.0);
				this->defaults_.setValue("mass_trace:min_spectra",14,"Number of spectra the have to show the same peak mass for a mass trace.");
				this->defaults_.setMinInt("mass_trace:min_spectra",1);
				this->defaults_.setValue("mass_trace:max_missing",4,"Number of spectra where a high mass deviation or missing peak is acceptable.");
				this->defaults_.setMinInt("mass_trace:max_missing",0);
				this->defaults_.setValue("mass_trace:slope_bound",0.1,"The maximum slope of mass trace intensities when extending from the highest peak", true);
				this->defaults_.setMinFloat("mass_trace:slope_bound",0.0);
				this->defaults_.setSectionDescription("mass_trace","Settings for the calculation of a score indicating if a peak is part of a mass trace (between 0 and 1).");
				//Isotopic pattern search parameters
				this->defaults_.setValue("isotopic_pattern:charge_low",1,"Lowest charge to search for.");
				this->defaults_.setMinInt("isotopic_pattern:charge_low",1);
				this->defaults_.setValue("isotopic_pattern:charge_high",4,"Highest charge to search for.");
				this->defaults_.setMinInt("isotopic_pattern:charge_high",1);
				this->defaults_.setValue("isotopic_pattern:mz_tolerance",0.03,"Tolerated mass deviation from the theoretical isotopic pattern.\nThis value must be smaller than that 1/charge_high!");		
				this->defaults_.setMinFloat("isotopic_pattern:mz_tolerance",0.0);
				this->defaults_.setValue("isotopic_pattern:intensity_percentage",10.0,"Isotopic peaks that contribute more than this percentage to the overall isotope pattern intensity must be present.", true);
				this->defaults_.setMinFloat("isotopic_pattern:intensity_percentage",0.0);
				this->defaults_.setMaxFloat("isotopic_pattern:intensity_percentage",100.0);
				this->defaults_.setValue("isotopic_pattern:intensity_percentage_optional",0.1,"Isotopic peaks that contribute more than this percentage to the overall isotope pattern intensity can be missing.", true);
				this->defaults_.setMinFloat("isotopic_pattern:intensity_percentage_optional",0.0);
				this->defaults_.setMaxFloat("isotopic_pattern:intensity_percentage_optional",100.0);
				this->defaults_.setValue("isotopic_pattern:optional_fit_improvement",2.0,"Minimal percental improvement of isotope fit to allow leaving out an optional peak.", true);
				this->defaults_.setMinFloat("isotopic_pattern:optional_fit_improvement",0.0);
				this->defaults_.setMaxFloat("isotopic_pattern:optional_fit_improvement",100.0);
				this->defaults_.setValue("isotopic_pattern:mass_window_width",100.0,"Window width in Dalton for precalcuation of estimated isotope distribtions.", true);
				this->defaults_.setMinFloat("isotopic_pattern:mass_window_width",1.0);
				this->defaults_.setMaxFloat("isotopic_pattern:mass_window_width",200.0);
				this->defaults_.setSectionDescription("isotopic_pattern","Settings for the calculation of a score indicating if a peak is part of a isotoipic pattern (between 0 and 1).");
				//Seed settings
				this->defaults_.setValue("seed:min_score",0.8,"Minimum seed score a peak has to reach to be used as seed.\nThe seed score is the geometric mean of intensity score, mass trace score and isotope pattern score.");
				this->defaults_.setMinFloat("seed:min_score",0.0);
				this->defaults_.setMaxFloat("seed:min_score",1.0);
				this->defaults_.setSectionDescription("seed","Settings that determine which peaks are considered a seed");
				//Feature settings
				this->defaults_.setValue("feature:min_score",0.7, "Feature score threshold for a feature to be reported.\nThe feature score is the geometric mean of the average relative deviation and the correlation between the model and the observed peaks.");
				this->defaults_.setMinFloat("feature:min_score",0.0);
				this->defaults_.setMaxFloat("feature:min_score",1.0);
				this->defaults_.setValue("feature:min_isotope_fit",0.8,"Minimum isotope fit of the feature before model fitting.", true);
				this->defaults_.setMinFloat("feature:min_isotope_fit",0.0);
				this->defaults_.setMaxFloat("feature:min_isotope_fit",1.0);
				this->defaults_.setValue("feature:min_trace_score",0.5, "Trace score threshold.\nTraces below this threshold are removed after the model fitting.", true);
				this->defaults_.setMinFloat("feature:min_trace_score",0.0);
				this->defaults_.setMaxFloat("feature:min_trace_score",1.0);
				this->defaults_.setValue("feature:min_rt_span",0.333, "Mimum RT span in relation to extended area that has to remain after model fitting.", true);
				this->defaults_.setMinFloat("feature:min_rt_span",0.0);
				this->defaults_.setMaxFloat("feature:min_rt_span",1.0);
				this->defaults_.setValue("feature:max_intersection",0.35, "Maximum allowed intersection of features.", true);
				this->defaults_.setMinFloat("feature:max_intersection",0.0);
				this->defaults_.setMaxFloat("feature:max_intersection",1.0);
				this->defaults_.setSectionDescription("feature","Settings for the features (intensity, quality assessment, ...)");
						
				this->defaultsToParam_();
			}
			
			/// Main method for actual FeatureFinder
			virtual void run()
			{
				//-------------------------------------------------------------------------
				//General initialization
				
				//quality estimation
				DoubleReal min_feature_score = param_.getValue("feature:min_score");
				//charges to look at				
				UInt charge_low = param_.getValue("isotopic_pattern:charge_low");
				UInt charge_high = param_.getValue("isotopic_pattern:charge_high");
				
				//copy the input map
				map_ = *(FeatureFinderAlgorithm<PeakType, FeatureType>::map_);
				
				//reserve space for calculated scores
				UInt meta_array_count = 5 + charge_high - charge_low;
				for (UInt s=0; s<map_.size(); ++s)
				{
					UInt scan_size = map_[s].size();
					map_[s].getMetaDataArrays().resize(meta_array_count);
					map_[s].getMetaDataArrays()[0].setName("trace_score");
					map_[s].getMetaDataArrays()[0].assign(scan_size,0.0);
					map_[s].getMetaDataArrays()[1].setName("intensity_score");
					map_[s].getMetaDataArrays()[1].assign(scan_size,0.0);
					map_[s].getMetaDataArrays()[2].setName("overall_score");
					map_[s].getMetaDataArrays()[2].assign(scan_size,0.0);
					map_[s].getMetaDataArrays()[3].setName("local_max");
					map_[s].getMetaDataArrays()[3].assign(scan_size,0.0);
					UInt charge = charge_low;
					for (UInt i = 4; i< meta_array_count; ++i)
					{
						map_[s].getMetaDataArrays()[i].setName(String("pattern_score_")+charge);
						map_[s].getMetaDataArrays()[i].assign(scan_size,0.0);
						++charge;
					}
				}
				
				bool debug = ( (String)(param_.getValue("debug"))=="true" );
				//clean up / create folders for debug information
				if (debug)
				{
					QDir dir(".");
					dir.mkpath("debug/features");
				}

				//---------------------------------------------------------------------------
				//Step 1:
				//Precalculate intensity scores for peaks
				//---------------------------------------------------------------------------
				log_ << "Precalculating intensity thresholds ..." << std::endl;
				//new scope to make local variables disappear
				{
					this->ff_->startProgress(0, intensity_bins_*intensity_bins_, "Precalculating intensity scores");
					DoubleReal rt_start = map_.getMinRT();
					DoubleReal mz_start = map_.getMinMZ();
					intensity_rt_step_ = (map_.getMaxRT() - rt_start ) / (DoubleReal)intensity_bins_;
				 	intensity_mz_step_ = (map_.getMaxMZ() - mz_start ) / (DoubleReal)intensity_bins_;
					intensity_thresholds_.resize(intensity_bins_);
					for (UInt rt=0; rt<intensity_bins_; ++rt)
					{
						intensity_thresholds_[rt].resize(intensity_bins_);
						DoubleReal min_rt = rt_start + rt * intensity_rt_step_;
						DoubleReal max_rt = rt_start + ( rt + 1 ) * intensity_rt_step_;
						std::vector<DoubleReal> tmp;
						for (UInt mz=0; mz<intensity_bins_; ++mz)
						{
							this->ff_->setProgress(rt*intensity_bins_+ mz);
							DoubleReal min_mz = mz_start + mz * intensity_mz_step_;
							DoubleReal max_mz = mz_start + ( mz + 1 ) * intensity_mz_step_;
							//std::cout << "rt range: " << min_rt << " - " << max_rt << std::endl;
							//std::cout << "mz range: " << min_mz << " - " << max_mz << std::endl;
							tmp.clear();
							for (typename MapType::ConstAreaIterator it = map_.areaBeginConst(min_rt,max_rt,min_mz,max_mz); it!=map_.areaEndConst(); ++it)
							{
								tmp.push_back(it->getIntensity());
							}
							//init vector
							intensity_thresholds_[rt][mz].assign(21, 0.0);
							//store quantiles (20)
							if (tmp.size()!=0)
							{
								std::sort(tmp.begin(), tmp.end());
								for (UInt i=0;i<21;++i)
								{
									UInt index = (UInt)std::floor(0.05*i*(tmp.size()-1));
									intensity_thresholds_[rt][mz][i] = tmp[index];
								}
							}
						}
					}
					
					//store intensity score in PeakInfo
					for (UInt s=0; s<map_.size(); ++s)
					{
						for (UInt p=0; p<map_[s].size(); ++p)
						{
							map_[s].getMetaDataArrays()[1][p] = intensityScore_(s,p);
						}
					}
					this->ff_->endProgress();
				}
				
				//---------------------------------------------------------------------------
				//Step 2:
				//Prealculate mass trace scores and local trace maximum for each peak
				//---------------------------------------------------------------------------
				//new scope to make local variables disappear
				{
					this->ff_->startProgress(0, map_.size(), "Precalculating mass trace scores");
					for (UInt s=0; s<map_.size(); ++s)
					{
						this->ff_->setProgress(s);
						//do nothing for the first few and last few spectra as the scans required to search for traces are missing
						if (s<min_spectra_ || s>=map_.size()-min_spectra_)
						{
							continue;
						}
						
						const SpectrumType& spectrum = map_[s];
						//iterate over all peaks of the scan
						for (UInt p=0; p<spectrum.size(); ++p)
						{
							std::vector<DoubleReal> scores;
							scores.reserve(2*min_spectra_);
							
							DoubleReal pos = spectrum[p].getMZ();
							Real inte = spectrum[p].getIntensity();
							
							//log_ << std::endl << "Peak: " << pos << std::endl;
							bool is_max_peak = true; //checking the maximum intensity peaks -> use them later as feature seeds.
							for (UInt i=1; i<=min_spectra_; ++i)
							{
								try
								{
									UInt spec_index = map_[s+i].findNearest(pos);
									DoubleReal position_score = positionScore_(pos, map_[s+i][spec_index].getMZ(), trace_tolerance_);
									if (position_score >0 && map_[s+i][spec_index].getIntensity()>inte) is_max_peak = false;
									scores.push_back(position_score);
								}
								catch(...) //no peaks in the spectrum
								{
									scores.push_back(0.0);
								}
							}
							for (UInt i=1; i<=min_spectra_; ++i)
							{
								try
								{
									UInt spec_index = map_[s-i].findNearest(pos);
									DoubleReal position_score = positionScore_(pos, map_[s-i][spec_index].getMZ(), trace_tolerance_);
									if (position_score>0 && map_[s-i][spec_index].getIntensity()>inte) is_max_peak = false;
									scores.push_back(position_score);
								}
								catch(...) //no peaks in the spectrum
								{
									scores.push_back(0.0);
								}
							}
							//Calculate a consensus score out of the scores calculated before
							DoubleReal trace_score = std::accumulate(scores.begin(), scores.end(),0.0) / scores.size();
							
							//store final score for later use
							map_[s].getMetaDataArrays()[0][p] = trace_score;
							map_[s].getMetaDataArrays()[3][p] = is_max_peak;
						}
					}
					this->ff_->endProgress();
				}

				//-------------------------------------------------------------------------
				//Step 3:
				//Charge loop (create seeds and features for each charge separately)
				//-------------------------------------------------------------------------
				Int plot_nr = -1; //counter for the number of plots (debug info)
				for (UInt c=charge_low; c<=charge_high; ++c)
				{
					UInt charge_index_meta = 4 + c - charge_low;
					UInt feature_count_before = this->features_->size();
					std::vector<Seed> seeds;
					
					//-----------------------------------------------------------
					//Step 3.1: Precalculate IsotopePattern score
					//-----------------------------------------------------------
					this->ff_->startProgress(0, map_.size(), String("Calculating isotope pattern scores for charge ")+c);
					for (UInt s=0; s<map_.size(); ++s)
					{
						this->ff_->setProgress(s);
						const SpectrumType& spectrum = map_[s];
						for (UInt p=0; p<spectrum.size(); ++p)
						{
							DoubleReal mz = spectrum[p].getMZ();
							
							//get isotope distribution for this mass
							const TheoreticalIsotopePattern& isotopes = getIsotopeDistribution_(mz*c);
							//determine highest peak in isopope distribution
							UInt max_isotope = std::max_element(isotopes.intensity.begin(), isotopes.intensity.end()) - isotopes.intensity.begin();
							//Look up expected isotopic peaks (in the current spectrum or adjacent spectra)
							UInt peak_index = (UInt)spectrum.findNearest(mz-((DoubleReal)(isotopes.size()+1)/c));
							IsotopePattern pattern(isotopes.size());
							for (UInt i=0; i<isotopes.size(); ++i)
							{
								DoubleReal isotope_pos = mz + ((DoubleReal)i-max_isotope)/c;
								findIsotope_(isotope_pos, s, pattern, i, false, peak_index);
							}
							DoubleReal pattern_score = isotopeScore_(isotopes, pattern, true, false);
							
							//update pattern scores of all contained peaks (if necessary)
							if (pattern_score > 0.0)
							{
								for (UInt i=0; i<pattern.peak.size(); ++i)
								{
									if (pattern.peak[i]>=0 && pattern_score>map_[pattern.spectrum[i]].getMetaDataArrays()[charge_index_meta][pattern.peak[i]])
									{
										map_[pattern.spectrum[i]].getMetaDataArrays()[charge_index_meta][pattern.peak[i]] = pattern_score;
									}
								}
							}
						}
					}
					this->ff_->endProgress();

					//-----------------------------------------------------------
					//Step 3.2:
					//Find seeds for this charge
					//-----------------------------------------------------------		
					this->ff_->startProgress(0, map_.size(), String("Finding seeds for charge ")+c);
					DoubleReal min_seed_score = param_.getValue("seed:min_score");
					for (UInt s=0; s<map_.size(); ++s)
					{
						this->ff_->setProgress(s);
						//do nothing for the first few and last few spectra as the scans required to search for traces are missing
						if (s<min_spectra_ || s>=map_.size()-min_spectra_)
						{
							continue;
						}
						//iterate over peaks
						for (UInt p=0; p<map_[s].size(); ++p)
						{	
							MetaDataArrays& meta = map_[s].getMetaDataArrays();
							meta[2][p] = std::pow(meta[0][p]*meta[1][p]*meta[charge_index_meta][p], 1.0f/3.0f);
							//add seed to vector if certain conditions are fullfilled
							if (meta[3][p]!=0.0 && meta[2][p]>min_seed_score)
							{
								Seed seed;
								seed.spectrum = s;
								seed.peak = p;
								seed.intensity = map_[s][p].getIntensity();								
								seeds.push_back(seed);
							}
						}
					}
					//sort seeds according to intensity
					std::sort(seeds.rbegin(),seeds.rend());
					//create and store seeds map and selected peak map
					if (debug)
					{
						//seeds
						FeatureMap<> seed_map;
						seed_map.reserve(seeds.size());
						for (UInt i=0; i<seeds.size(); ++i)
						{
							UInt spectrum = seeds[i].spectrum;
							UInt peak = seeds[i].peak;
							const MetaDataArrays& meta = map_[spectrum].getMetaDataArrays();
							Feature tmp;
							tmp.setIntensity(seeds[i].intensity);
							tmp.setOverallQuality(meta[2][peak]);
							tmp.setRT(map_[spectrum].getRT());
							tmp.setMZ(map_[spectrum][peak].getMZ());
							tmp.setMetaValue("intensity_score",	meta[1][peak]);
							tmp.setMetaValue("pattern_score",	meta[charge_index_meta][peak]);
							tmp.setMetaValue("trace_score",	meta[0][peak]);
							seed_map.push_back(tmp);
						}
						FeatureXMLFile().store(String("debug/seeds_")+c+".featureXML", seed_map);
					}
					this->ff_->endProgress();
					std::cout << "Found " << seeds.size() << " seeds for charge " << c << "." << std::endl;
					
					//------------------------------------------------------------------
					//Step 3.3:
					//Extension of seeds
					//------------------------------------------------------------------
					this->ff_->startProgress(0,seeds.size(), String("Extending seeds for charge ")+c);
					for (UInt i=0; i<seeds.size(); ++i)
					{
						//------------------------------------------------------------------
						//Step 3.3.1:
						//Extend all mass traces
						//------------------------------------------------------------------
						this->ff_->setProgress(i);
						log_ << std::endl << "Seed " << i << ":" << std::endl;
						//If the intensity is zero this seed is already uses in another feature
						const SpectrumType& spectrum = map_[seeds[i].spectrum];
						const PeakType& peak = spectrum[seeds[i].peak];
						log_ << " - Int: " << peak.getIntensity() << std::endl;
						log_ << " - RT: " << spectrum.getRT() << std::endl;
						log_ << " - MZ: " << peak.getMZ() << std::endl;
						if (seeds[i].intensity == 0.0)
						{
							log_ << "Already used in another feature => aborting!" << std::endl;
							continue;
						}
						
						//----------------------------------------------------------------
						//Find best fitting isotope pattern for this charge (using averagene)
						IsotopePattern best_pattern(0);
						DoubleReal isotope_fit_quality = findBestIsotopeFit_(seeds[i], c, best_pattern);
						if (isotope_fit_quality<min_isotope_fit_)
						{
							abort_(debug, seeds[i], "Could not find good enough isotope pattern containing the seed");
							continue;
						}
						
						//extend the convex hull in RT dimension (starting from the trace peaks)
						log_ << "Collecting mass traces" << std::endl;
						MassTraces traces;
						traces.reserve(best_pattern.peak.size());
						extendMassTraces_(best_pattern, traces);

						//check if the traces are still valid
						DoubleReal seed_mz = map_[seeds[i].spectrum][seeds[i].peak].getMZ();
						if (!traces.isValid(seed_mz, trace_tolerance_))
						{
							abort_(debug, seeds[i], "Could not extend seed");
							continue;
						}
						
						//------------------------------------------------------------------
						//Step 3.3.2:
						//Gauss fit (first fit to find the feature boundaries)
						//------------------------------------------------------------------
						++plot_nr;
						log_ << "Fitting model" << std::endl;
					  const gsl_multifit_fdfsolver_type *T;
					  gsl_multifit_fdfsolver *s;
					  int status;
					  const size_t param_count = 3;
						const size_t data_count = traces.getPeakCount();
					  gsl_multifit_function_fdf func;

					  //parameter estimates (height, x0, sigma)
						traces[traces.max_trace].updateMaximum();
						DoubleReal height = traces[traces.max_trace].max_peak->getIntensity();
						DoubleReal x0 = traces[traces.max_trace].max_rt;
						DoubleReal sigma = (traces[traces.max_trace].peaks.back().first-traces[traces.max_trace].peaks[0].first)/4.0;			
					  double x_init[param_count] = {height, x0, sigma};
						log_ << " - estimates - height: " << height << " x0: " << x0 <<  " sigma: " << sigma  << std::endl;

						//baseline estimate
						traces.updateBaseline();
						traces.baseline = 0.75 * traces.baseline;

						//fit					  
					  gsl_vector_view x = gsl_vector_view_array(x_init, param_count);	
					  const gsl_rng_type * type;
					  gsl_rng * r;
					  gsl_rng_env_setup();
					  type = gsl_rng_default;
					  r = gsl_rng_alloc(type);
					  func.f = &gaussF_;
					  func.df = &gaussDF_;
					  func.fdf = &gaussFDF_;
					  func.n = data_count;
					  func.p = param_count;
					  func.params = &traces;
					  T = gsl_multifit_fdfsolver_lmsder;
					  s = gsl_multifit_fdfsolver_alloc(T, data_count, param_count);
					  gsl_multifit_fdfsolver_set(s, &func, &x.vector);
					  size_t iter = 0;					
					  do
					  {
					    iter++;
					    status = gsl_multifit_fdfsolver_iterate(s);
					    if (status) break;
					    status = gsl_multifit_test_delta(s->dx, s->x, 0.0001, 0.0001);
					  } 
					  while (status == GSL_CONTINUE && iter < 5000);
						
						height = gsl_vector_get(s->x, 0);
						x0 = gsl_vector_get(s->x, 1);
						sigma = std::fabs(gsl_vector_get(s->x, 2));						
						gsl_multifit_fdfsolver_free(s);
						log_ << " - fit - height: " << height  << " x0: " << x0 << " sigma: " << sigma << std::endl;
						
						//------------------------------------------------------------------
						//Step 3.3.3:
						//Crop feature according to RT fit (2.5*sigma) and remove badly fitting traces
						//------------------------------------------------------------------
						MassTraces new_traces;
						DoubleReal low_bound = x0 - 2.5 * sigma;
						DoubleReal high_bound = x0 + 2.5 * sigma;
						log_ << "    => RT bounds: " << low_bound << " - " << high_bound << std::endl;
						for (UInt t=0; t< traces.size(); ++t)
						{
							MassTrace& trace = traces[t];
							log_ << "   - Trace " << t << ": (" << trace.theoretical_int << ")" << std::endl;

							MassTrace new_trace;
							//compute average relative deviation and correlation
							DoubleReal deviation = 0.0;
							std::vector<DoubleReal> v_theo, v_real;
							for (UInt k=0; k<trace.peaks.size(); ++k)
							{
								//consider peaks when inside RT bounds only
								if (trace.peaks[k].first>=low_bound && trace.peaks[k].first<= high_bound)
								{
									new_trace.peaks.push_back(trace.peaks[k]);
									DoubleReal theo = traces.baseline + trace.theoretical_int *  height * exp(-0.5 * pow(trace.peaks[k].first - x0, 2) / pow(sigma, 2) );
									v_theo.push_back(theo);
									DoubleReal real = trace.peaks[k].second->getIntensity();
									v_real.push_back(real);
									deviation += std::fabs(real-theo)/theo;
								}
							}
							DoubleReal fit_score = 0.0;
							DoubleReal correlation = 0.0;							
							DoubleReal final_score = 0.0;
							if (new_trace.peaks.size()!=0)
							{
								fit_score = deviation / new_trace.peaks.size();
								correlation = std::max(0.0, Math::pearsonCorrelationCoefficient(v_theo.begin(),v_theo.end(),v_real.begin(), v_real.end()));
								final_score = std::sqrt(correlation * std::max(0.0, 1.0-fit_score));
							}
							log_ << "     - peaks: " << new_trace.peaks.size() << " / " << trace.peaks.size() << " - relative deviation: " << fit_score << " - correlation: " << correlation << " - final score: " << correlation << std::endl;
							//remove badly fitting traces
							if ( !new_trace.isValid() || final_score < min_trace_score_)
							{
								if (t<traces.max_trace)
								{
									new_traces = MassTraces();
									log_ << "     - removed this and previous traces due to bad fit" << std::endl;
									new_traces.clear(); //remove earlier traces
									continue;
								}
								else if (t==traces.max_trace)
								{
									new_traces = MassTraces();
									log_ << "     - aborting (max trace was removed)" << std::endl;
									break;
								}
								else if (t>traces.max_trace)
								{
									log_ << "     - removed due to bad fit => omitting the rest" << std::endl;
		            	break; //no more traces are possible
								}
							}
							//add new trace
							else
							{
								new_trace.theoretical_int = trace.theoretical_int;
								new_traces.push_back(new_trace);
								if (t==traces.max_trace)
								{
									new_traces.max_trace = new_traces.size()-1;
								}
							}
						}
						new_traces.baseline = traces.baseline;			

						//------------------------------------------------------------------
						//Step 3.3.4:
						//Check if feature is ok
						//------------------------------------------------------------------
					  bool feature_ok = true;
					  String error_msg = "";
					  //check if the feature is valid
					  if (!new_traces.isValid(seed_mz, trace_tolerance_))
					  {
					  	feature_ok = false;
					  	error_msg = "Invalid feature after fit";
					  }
						//check if x0 is inside feature bounds
						if (feature_ok)
						{
							std::pair<DoubleReal,DoubleReal> rt_bounds = new_traces.getRTBounds();
							if (x0<rt_bounds.first || x0>rt_bounds.second)
							{
						  	feature_ok = false;
						  	error_msg = "Invalid fit: Center outside of feature bounds";
							}
						}
						//check if the remaining traces fill out at least a third of the RT span
						if (feature_ok)
						{
							std::pair<DoubleReal,DoubleReal> rt_bounds = new_traces.getRTBounds();
							if ((rt_bounds.second-rt_bounds.first)<min_rt_span_*5.0*sigma )
							{
						  	feature_ok = false;
						  	error_msg = "Invalid fit: Less than 'min_rt_span' left after fit";
							}
						}
					  //check if feature quality is high enough (average relative deviation and correlation of the whole feature)
						DoubleReal fit_score = 0.0;
						DoubleReal correlation = 0.0;
						DoubleReal final_score = 0.0;
						if(feature_ok)
						{
							std::vector<DoubleReal> v_theo, v_real;
							DoubleReal deviation = 0.0;
							for (UInt t=0; t< new_traces.size(); ++t)
							{
								MassTrace& trace = new_traces[t];
								for (UInt k=0; k<trace.peaks.size(); ++k)
								{
									DoubleReal theo = new_traces.baseline + trace.theoretical_int *  height * exp(-0.5 * pow(trace.peaks[k].first - x0, 2) / pow(sigma, 2) );
									v_theo.push_back(theo);
									DoubleReal real = trace.peaks[k].second->getIntensity();
									v_real.push_back(real);
									deviation += std::fabs(real-theo)/theo;
								}
							}
							fit_score = std::max(0.0, 1.0 - (deviation / new_traces.getPeakCount()));
							correlation = std::max(0.0,Math::pearsonCorrelationCoefficient(v_theo.begin(),v_theo.end(),v_real.begin(), v_real.end()));
							final_score = std::sqrt(correlation * fit_score);
						  if (final_score<min_feature_score)
						  {
						  	feature_ok = false;
						  	error_msg = "Feature quality too low after fit";
						  }
							//quality output
							log_ << "Quality estimation:" << std::endl;
							log_ << " - relative deviation: " << fit_score << std::endl;
							log_ << " - correlation: " << correlation << std::endl;
							log_ << " => final score: " << final_score << std::endl;
						}
					  				  
						//write debug output of feature
						if (debug)
						{
							TextFile tf;
							//gnuplot script	
							String script = String("plot \"debug/features/") + plot_nr + ".dta\" title 'before fit (RT:" + x0 + " m/z:" + peak.getMZ() + ")' with points 1";
							//feature before fit
							for (UInt k=0; k<traces.size(); ++k)
							{
								for (UInt j=0; j<traces[k].peaks.size(); ++j)
								{
									tf.push_back(String(500.0*k+traces[k].peaks[j].first) + "	" + traces[k].peaks[j].second->getIntensity());
								}
							}
							tf.store(String("debug/features/") + plot_nr + ".dta");
							//fitted feature
							if (new_traces.getPeakCount()!=0)
							{
								tf.clear();
								for (UInt k=0; k<new_traces.size(); ++k)
								{
									for (UInt j=0; j<new_traces[k].peaks.size(); ++j)
									{
										tf.push_back(String(500.0*k+new_traces[k].peaks[j].first) + "	" + new_traces[k].peaks[j].second->getIntensity());
									}
								}
								tf.store(String("debug/features/") + plot_nr + "_cropped.dta");
								script = script + ", \"debug/features/" + plot_nr + "_cropped.dta\" title 'feature ";
								if (!feature_ok)
								{
									script = script + " - " + error_msg;
								}
								else
								{
									script = script + (this->features_->size()+1) + " (score: " + final_score + ")";
								}
								script = script + "' with points 3";
							}
							//fitted functions
							tf.clear();
							for (UInt k=0; k<traces.size(); ++k)
							{
								char fun = 'f';
								fun += k;
								tf.push_back(String(fun)+"(x)= " + traces.baseline + " + " + (traces[k].theoretical_int*height) + " * exp(-0.5*(x-" + (500.0*k+x0) + ")**2/(" + sigma + ")**2)");
								script =  script + ", " + fun + "(x) title 'Trace " + k + " (m/z:" + traces[k].getAvgMZ() + ")'";
							}
							//output
							tf.push_back("set xlabel \"pseudo RT (mass traces side-by-side)\"");
							tf.push_back("set ylabel \"intensity\"");
							tf.push_back(script);
							tf.push_back("pause -1");
							tf.store(String("debug/features/") + plot_nr + ".plot");
						}
						traces = new_traces;
						
						log_ << "Feature label: " << plot_nr << std::endl;
						
						//validity output
						if (!feature_ok)
						{
							abort_(debug, seeds[i], error_msg);
							continue;
						}
						
						//------------------------------------------------------------------
						//Step 3.3.5:
						//Feature creation
						//------------------------------------------------------------------
						Feature f;
						//set label
						f.setMetaValue(3,plot_nr);
						f.setCharge(c);
						f.setOverallQuality(final_score);
						if (debug)
						{
							f.setMetaValue("score_fit",fit_score);
							f.setMetaValue("score_correlation",correlation);
						}
						f.setRT(x0);
						f.setMZ(traces[traces.getTheoreticalMax()].getAvgMZ());
						//Calculate intensity based on model only
						// - the model does not include the baseline, so we ignore it here
						// - as we scaled the isotope distribution to 
						f.setIntensity(2.5 * height * sigma / getIsotopeDistribution_(f.getMZ()).max);
						//add convex hulls of mass traces
						for (UInt j=0; j<traces.size(); ++j)
						{
							f.getConvexHulls().push_back(traces[j].getConvexhull());
						}
						this->features_->push_back(f);
	
						//----------------------------------------------------------------
						//Remove all seeds that lie inside the convex hull of the new feature
						DBoundingBox<2> bb = f.getConvexHull().getBoundingBox();
						for (UInt j=i+1; j<seeds.size(); ++j)
						{
							DoubleReal rt = map_[seeds[j].spectrum].getRT();
							DoubleReal mz = map_[seeds[j].spectrum][seeds[j].peak].getMZ();
							if (bb.encloses(rt,mz) && f.encloses(rt,mz))
							{
								//set intensity to zero => the peak will be skipped!
								seeds[j].intensity = 0.0;
							}
						}
					}
					this->ff_->endProgress();
					std::cout << "Found " << this->features_->size()-feature_count_before << " features candidates for charge " << c << "." << std::endl;
				}
					
				//------------------------------------------------------------------
				//Step 4:
				//Resolve contradicting and overlapping features
				//------------------------------------------------------------------
				this->ff_->startProgress(0, this->features_->size()*this->features_->size(), "Resolving overlapping features");
				log_ << "Resolving intersecting features" << std::endl;
				for (UInt i=0; i<this->features_->size(); ++i)
				{
					Feature& f1(this->features_->at(i));
					for (UInt j=i+1; j<this->features_->size(); ++j)
					{
						this->ff_->setProgress(i*this->features_->size()+j);
						Feature& f2(this->features_->at(j));
						if (f1.getIntensity()==0.0 || f2.getIntensity()==0.0) continue;
						//act depending on the intersection
						DoubleReal intersection = intersection_(f1, f2);
						if (intersection>=max_feature_intersection_)
						{
							log_ << " - Intersection (" << (i+1) << "/" << (j+1) << "): " << intersection << std::endl;
							if (f1.getCharge()==f2.getCharge())
							{
								if (f1.getIntensity()*f1.getOverallQuality()>f2.getIntensity()*f2.getOverallQuality())
								{
									log_ << "   - same charge -> removing duplicate " << (j+1) << std::endl;
									f2.setIntensity(0.0);
								}
								else
								{
									log_ << "   - same charge -> removing duplicate " << (i+1) << std::endl;
									f1.setIntensity(0.0);
								}
							}
							else if (f2.getCharge()%f1.getCharge()==0)
							{
								log_ << "   - different charge (one is the multiple of the other) -> removing lower charge " << (i+1) << std::endl;
								f1.setIntensity(0.0);
							}
							else
							{
								if (f1.getOverallQuality()>f2.getOverallQuality())
								{
									log_ << "   - different charge -> removing lower score " << (j+1) << std::endl;
									f2.setIntensity(0.0);
								}
								else
								{
									log_ << "   - different charge -> removing lower score " << (i+1) << std::endl;
									f1.setIntensity(0.0);
								}
							}
						}
					}
				}
				//finally remove features with intensity 0
				UInt removed = 0;
				for(Int i=this->features_->size()-1; i>=0; --i)
				{
					if (this->features_->operator[](i).getIntensity()==0)
					{
						++removed;
						this->features_->erase(this->features_->begin()+i);
					}
				}
				this->ff_->endProgress();
				std::cout << "Removed " << removed << " overlapping features." << std::endl;
				std::cout << this->features_->size() << " features left." << std::endl;
				
				//Abort reasons 
				std::cout << std::endl;
				std::cout << "Abort reasons during feature construction:" << std::endl;
				for (std::map<String,UInt>::const_iterator it=aborts_.begin(); it!=aborts_.end(); ++it)
				{
					std::cout << "- " << it->first << ": " << it->second << std::endl;
				}
				if (debug)
				{
					//store map of abort reasons for failed seeds
					FeatureMap<> abort_map;
					abort_map.reserve(abort_reasons_.size());
					for (typename std::map<Seed, String>::iterator it2=abort_reasons_.begin(); it2!=abort_reasons_.end(); ++it2)
					{
						Feature f;
						f.setRT(map_[it2->first.spectrum].getRT());
						f.setMZ(map_[it2->first.spectrum][it2->first.peak].getMZ());
						f.setIntensity(map_[it2->first.spectrum][it2->first.peak].getIntensity());
						f.setMetaValue("abort_reason",it2->second);
						abort_map.push_back(f);
					}
					FeatureXMLFile().store("debug/abort_reasons.featureXML", abort_map);
					
					//store input map with calculated scores (without overall score)
					for (UInt s=0; s<map_.size(); ++s)
					{
						map_[s].getMetaDataArrays().erase(map_[s].getMetaDataArrays().begin()+2);
					}					
					MzDataFile().store("debug/input.mzData", map_);
				}
			}
			
			static FeatureFinderAlgorithm<PeakType,FeatureType>* create()
			{
				return new FeatureFinderAlgorithmPicked();
			}

			static const String getProductName()
			{
				return "picked_peak";
			}
	
		protected:
			/// editable copy of the map
			MapType map_;
			/// Output stream for log/debug info
			std::ofstream log_; 
			/// Array of abort reasons
			std::map<String, UInt> aborts_;
			/// Array of abort reasons
			std::map<Seed, String> abort_reasons_;
						
			///@name Members for parameters often needed in methods
			//@{
			DoubleReal pattern_tolerance_; ///< Stores mass_trace:mz_tolerance
			DoubleReal trace_tolerance_; ///< Stores isotopic_pattern:mz_tolerance
			UInt min_spectra_; ///< Number of spectra that have to show the same mass (for finding a mass trace)
			UInt max_missing_trace_peaks_; ///< Stores mass_trace:max_missing
			DoubleReal slope_bound_; ///< Max slope of mass trace intensities
			DoubleReal intensity_percentage_; ///< Isotope pattern intensity contribution of required peaks
			DoubleReal intensity_percentage_optional_; ///< Isotope pattern intensity contribution of optional peaks
			DoubleReal optional_fit_improvement_; ///< Minimal imrovment for leaving out optional isotope
			DoubleReal mass_window_width_; ///< Width of the isotope pattern mass bins
			UInt intensity_bins_; ///< Number of bins (in RT and MZ) for intensity significance estimation
			DoubleReal min_isotope_fit_; ///< Mimimum isotope pattern fit for a feature
			DoubleReal min_trace_score_; ///< Minimum quality of a traces
			DoubleReal min_rt_span_; ///< Mimum RT range that has to be left after the fit
			DoubleReal max_feature_intersection_; ///< Maximum allowed feature intersection (if larger, that one of the feature is removed)
			//@}

			///@name Members for intensity significance estimation
			//@{			
			/// RT bin width
			DoubleReal intensity_rt_step_;
			/// m/z bin width
			DoubleReal intensity_mz_step_;
			/// Precalculated intensity 20-quantiles (binned)
			std::vector< std::vector< std::vector<DoubleReal> > > intensity_thresholds_;
			//@}

			///Vector of precalculated isotope distributions for several mass winows
			std::vector< TheoreticalIsotopePattern > isotope_distributions_;

			//Docu in base class
			virtual void updateMembers_()
			{
				pattern_tolerance_ = param_.getValue("mass_trace:mz_tolerance");
				trace_tolerance_ = param_.getValue("isotopic_pattern:mz_tolerance");
				min_spectra_ = (UInt)std::floor((DoubleReal)param_.getValue("mass_trace:min_spectra")*0.5);
				max_missing_trace_peaks_ = param_.getValue("mass_trace:max_missing");
				slope_bound_ = param_.getValue("mass_trace:slope_bound");
				intensity_percentage_ = (DoubleReal)param_.getValue("isotopic_pattern:intensity_percentage")/100.0;
				intensity_percentage_optional_ = (DoubleReal)param_.getValue("isotopic_pattern:intensity_percentage_optional")/100.0;
				optional_fit_improvement_ = (DoubleReal)param_.getValue("isotopic_pattern:optional_fit_improvement")/100.0;
				mass_window_width_ = param_.getValue("isotopic_pattern:mass_window_width");
				intensity_bins_ =  param_.getValue("intensity:bins");
				min_isotope_fit_ = param_.getValue("feature:min_isotope_fit");
				min_trace_score_ = param_.getValue("feature:min_trace_score");
				min_rt_span_ = param_.getValue("feature:min_rt_span");
				max_feature_intersection_ = param_.getValue("feature:max_intersection");
			}
			
			///Writes the abort reason to the log file and counts occurences for each reason
			void abort_(bool debug, const Seed& seed, const String& reason)
			{
				log_ << "Abort: " << reason << std::endl;
				aborts_[reason]++;
				if (debug) abort_reasons_[seed] = reason;
			}

			///Calculates the intersection between features.
			///The value is normalized by the size of the smaller feature, so it rages from 0 to 1.
			DoubleReal intersection_(const Feature& f1, const Feature& f2)
			{
				//calculate the RT range sum of feature 1
				DoubleReal s1 = 0.0;
				std::vector<ConvexHull2D> hulls1 = f1.getConvexHulls();
				for (UInt i=0; i<hulls1.size(); ++i)
				{
					s1 += hulls1[i].getBoundingBox().width();
				}
				
				//calculate the RT range sum of feature 2
				DoubleReal s2 = 0.0;
				std::vector<ConvexHull2D> hulls2 = f2.getConvexHulls();
				for (UInt j=0; j<hulls2.size(); ++j)
				{
					s2 += hulls2[j].getBoundingBox().width();
				}
				
				//calculate overlap
				DoubleReal overlap = 0.0;
				for (UInt i=0; i<hulls1.size(); ++i)
				{
					DBoundingBox<2> bb1 = hulls1[i].getBoundingBox();
					for (UInt j=0; j<hulls2.size(); ++j)
					{
						DBoundingBox<2> bb2 = hulls2[j].getBoundingBox();
						if (bb1.intersects(bb2))
						{
							if (bb1.min()[0]<=bb2.min()[0] && bb1.max()[0]>=bb2.max()[0]) //bb1 contains bb2
							{
								overlap += bb2.width();
							}
							else if (bb2.min()[0]<=bb1.min()[0] && bb2.max()[0]>=bb1.max()[0]) //bb2 contains bb1
							{
								overlap += bb1.width();
							}
							else if (bb1.min()[0]<=bb2.min()[0] && bb1.max()[0]<=bb2.max()[0]) //the end of bb1 overlaps with bb2
							{
								overlap += bb1.max()[0]-bb2.min()[0];
							}
							else if (bb2.min()[0]<=bb1.min()[0] && bb2.max()[0]<=bb1.max()[0]) //the end of bb2 overlaps with bb1
							{
								overlap += bb2.max()[0]-bb1.min()[0];
							}
						}
					}
				}
				
				return overlap/std::min(s1,s2);
			}
			
			///Returns the isotope distribution for a certain mass window
			const TheoreticalIsotopePattern& getIsotopeDistribution_(DoubleReal mass)
			{
				//calculate index in the vector
				UInt index = (UInt)std::floor(mass/mass_window_width_);
				
				//enlarge vector if necessary
				if (index>=isotope_distributions_.size())
				{
					isotope_distributions_.resize(index+1);
				}
				
				//calculate distribution if necessary
				if (isotope_distributions_[index].intensity.size()==0)
				{
					//log_ << "Calculating iso dist for mass: " << 0.5*mass_window_width_ + index * mass_window_width_ << std::endl;
					IsotopeDistribution d;
					d.setMaxIsotope(20);
					d.estimateFromPeptideWeight(0.5*mass_window_width_ + index * mass_window_width_);
					d.trimLeft(intensity_percentage_optional_);
					d.trimRight(intensity_percentage_optional_);
					for (IsotopeDistribution::Iterator it=d.begin(); it!=d.end(); ++it)
					{
						isotope_distributions_[index].intensity.push_back(it->second);
						//log_ << " - " << it->second << std::endl;
					}
					//determine the number of optional peaks at the beginning/end
					UInt begin = 0;
					UInt end = 0;
					bool is_begin = true;
					bool is_end = false;
					for (UInt i=0; i<isotope_distributions_[index].intensity.size(); ++i)
					{
						if (isotope_distributions_[index].intensity[i]<intensity_percentage_)
						{
							if (!is_end && !is_begin) is_end = true;
							if (is_begin) ++begin;
							else if (is_end) ++end;
						}
						else if (is_begin)
						{
							is_begin = false;
						}
					}
					isotope_distributions_[index].optional_begin = begin;
					isotope_distributions_[index].optional_end = end;
					//scale the distibution to a maximum of 1
					DoubleReal max = 0.0;
					for (UInt i=0; i<isotope_distributions_[index].intensity.size(); ++i)
					{
						if (isotope_distributions_[index].intensity[i]>max) max = isotope_distributions_[index].intensity[i];
					}
					isotope_distributions_[index].max = max;
					for (UInt i=0; i<isotope_distributions_[index].intensity.size(); ++i)
					{
						isotope_distributions_[index].intensity[i] /= max;
					}
					
					//log_ << " - optinal begin/end:" << begin << " / " << end << std::endl;
				}
				//Return distribution
				return isotope_distributions_[index];
			}
						
			/**
				@brief Finds the best fitting position of the isotopic pattern estimate defined by @p center
				
				@param center the maximum peak of the isotope distribution (contains charge as well)
				@param charge The charge of the pattern 
				@param best_pattern Returns the indices of the isotopic peaks. If a isopopic peak is missing -1 is returned.
			*/
			DoubleReal findBestIsotopeFit_(const Seed& center, UInt charge, IsotopePattern& best_pattern)
			{
				log_ << "Testing isotope patterns for charge " << charge << ": " << std::endl;			
				const SpectrumType& spectrum = map_[center.spectrum];
				const TheoreticalIsotopePattern& isotopes = getIsotopeDistribution_(spectrum[center.peak].getMZ()*charge);	
				log_ << " - Seed: " << center.peak << " (mz:" << spectrum[center.peak].getMZ()<< ")" << std::endl;
				
				//Find m/z boundaries of search space (linear search as this is local and we have the center already)
				DoubleReal mass_window = (DoubleReal)(isotopes.size()+1) / (DoubleReal)charge;
				log_ << " - Mass window: " << mass_window << std::endl;
				UInt end = center.peak;
				while(end<spectrum.size() && spectrum[end].getMZ()<spectrum[center.peak].getMZ()+mass_window)
				{
					++end;
				}
				--end;
				//search begin
				Int begin = center.peak;
				while(begin>=0 && spectrum[begin].getMZ()>spectrum[center.peak].getMZ()-mass_window)
				{
					--begin;
				}
				++begin;
				log_ << " - Begin: " << begin << " (mz:" << spectrum[begin].getMZ()<< ")" << std::endl;
				log_ << " - End: " << end << " (mz:" << spectrum[end].getMZ()<< ")" << std::endl;

				//fit isotope distribution to peaks
				DoubleReal max_score = 0.0;
				for (UInt start=begin; start<=end; ++start)
				{
					//find isotope peaks for the current start peak
					UInt peak_index = start;
					IsotopePattern pattern(isotopes.size());
					pattern.intensity[0] = spectrum[start].getIntensity();
					pattern.peak[0] = start;
					pattern.spectrum[0] = center.spectrum;
					pattern.mz_score[0] = 1.0;
					pattern.theoretical_mz[0] = spectrum[start].getMZ();
					log_ << " - Fitting at " << start << " (mz:" << spectrum[start].getMZ() << ")" << std::endl;
					log_ << "   - Isotope 0: " << pattern.intensity[0] << std::endl;
					for (UInt iso=1; iso<isotopes.size(); ++iso)
					{
						DoubleReal pos = spectrum[start].getMZ() + iso/(DoubleReal)charge;
						findIsotope_(pos, center.spectrum, pattern, iso, true, peak_index);
					}
					
					//check if the seed is contained, otherwise abort
					bool seed_contained = false;
					for (UInt iso=0; iso<pattern.peak.size(); ++iso)
					{
						if (pattern.peak[iso]==(Int)center.peak && pattern.spectrum[iso]==center.spectrum)
						{
							seed_contained = true;
							break;
						}
					}
					if(!seed_contained)
					{
						log_ << "   - aborting: seed is not contained!" << std::endl;
						continue;
					}

					DoubleReal score = isotopeScore_(isotopes, pattern, false, true);
	
					//check if the seed is still contained, otherwise abort
					seed_contained = false;
					for (UInt iso=0; iso<pattern.peak.size(); ++iso)
					{
						if (pattern.peak[iso]==(Int)center.peak && pattern.spectrum[iso]==center.spectrum)
						{
							seed_contained = true;
							break;
						}
					}
					if(!seed_contained)
					{
						log_ << "   - aborting: seed was removed during isotope fit!" << std::endl;
						continue;
					}
					
					log_ << "   - final score: " << score << std::endl;
					if (score>max_score)
					{
						max_score = score;
						best_pattern = pattern;
					}
				}
				log_ << " - best score: " << max_score << std::endl;
				best_pattern.theoretical_ints = isotopes;
				return max_score;
			}
			
			///Extends all mass traces of a isotope pattern in one step
			void extendMassTraces_(const IsotopePattern& pattern, MassTraces& traces)
			{
				//find index of the trace with the maximum intensity
				DoubleReal max_int =  0.0;
				UInt max_trace_index = 0;
				for (UInt p=0; p<pattern.peak.size(); ++p)
				{
					if (pattern.peak[p]<0) continue; //skip missing and removed traces
					if (map_[pattern.spectrum[p]][pattern.peak[p]].getIntensity()>max_int)
					{
						max_int = map_[pattern.spectrum[p]][pattern.peak[p]].getIntensity();
						max_trace_index = p;
					}
				}
				
				//extend the maximum intensity trace to determine the boundaries in RT dimension
				UInt start_index = pattern.spectrum[max_trace_index];
				const PeakType* start_peak = &(map_[pattern.spectrum[max_trace_index]][pattern.peak[max_trace_index]]);
				DoubleReal start_mz = start_peak->getMZ();
				DoubleReal start_rt = map_[start_index].getRT();
				log_ << " - Trace " << max_trace_index << " (maximum intensity)" << std::endl;
				log_ << "   - extending from: " << map_[start_index].getRT() << " / " << start_mz << " (int: " << start_peak->getIntensity() << ")" << std::endl;
				//initialize the trace and extend
				MassTrace max_trace;
				max_trace.peaks.push_back(std::make_pair(start_rt,start_peak));
				extendMassTrace_(max_trace, start_index, start_mz, false);
				extendMassTrace_(max_trace, start_index, start_mz, true);

				DoubleReal rt_max = max_trace.peaks.back().first;
				DoubleReal rt_min = max_trace.peaks.begin()->first;
				log_ << "   - rt bounds: " << rt_min << "-" << rt_max << std::endl;
				//Abort if too few peak were found
				if ((Int)max_trace.peaks.size()< (2*(Int)min_spectra_-(Int)max_missing_trace_peaks_))
				{
					log_ << "   - could not extend trace with maximum intensity => abort" << std::endl;
					return;
				}
				for (UInt p=0; p<pattern.peak.size(); ++p)
				{
					log_ << " - Trace " << p << std::endl;
					if (p==max_trace_index)
					{
						log_ << "   - previously extended maximum trace" << std::endl;
						traces.push_back(max_trace);
						traces.back().theoretical_int = pattern.theoretical_ints.intensity[p];
						traces.max_trace = traces.size()-1;
						continue;
					}
					Seed starting_peak;
					starting_peak.spectrum = pattern.spectrum[p];
					starting_peak.peak = pattern.peak[p];
					if (pattern.peak[p]==-2)
					{
						log_ << "   - removed during isotope fit" << std::endl;
						continue;
					}
					else if (pattern.peak[p]==-1)
					{
						log_ << "   - missing" << std::endl;
						continue;
					}
					starting_peak.intensity = map_[starting_peak.spectrum][starting_peak.peak].getIntensity();
					log_ << "   - trace seed: " << map_[starting_peak.spectrum].getRT() << " / " << map_[starting_peak.spectrum][starting_peak.peak].getMZ() << " (int: " << map_[starting_peak.spectrum][starting_peak.peak].getIntensity() << ")" << std::endl;
					
					//search for nearby maximum of the mass trace as the extension assumes that it starts at the maximum
					UInt begin = std::max(0u,starting_peak.spectrum-min_spectra_);
					UInt end = std::min(starting_peak.spectrum+min_spectra_,(UInt)map_.size());
					DoubleReal mz = map_[starting_peak.spectrum][starting_peak.peak].getMZ();
					DoubleReal inte = map_[starting_peak.spectrum][starting_peak.peak].getIntensity();
					for (UInt spectrum_index=begin; spectrum_index<end; ++spectrum_index)
					{
						//find better seeds (no-empty scan/low mz diff/higher intensity)
						Int peak_index = -1;
						try
						{
							peak_index = map_[spectrum_index].findNearest(map_[starting_peak.spectrum][starting_peak.peak].getMZ());
						}
						catch(...) //no peaks in the spectrum
						{
							peak_index=-1;
						}
						if (peak_index<0 ||
								map_[spectrum_index][peak_index].getIntensity()<=inte ||
								std::fabs(mz-map_[spectrum_index][peak_index].getMZ())>=pattern_tolerance_
							 ) continue;
						starting_peak.spectrum = spectrum_index;
						starting_peak.peak = peak_index;
						inte = map_[spectrum_index][peak_index].getIntensity();
					}
					log_ << "   - extending from: " << map_[starting_peak.spectrum].getRT() << " / " << map_[starting_peak.spectrum][starting_peak.peak].getMZ() << " (int: " << map_[starting_peak.spectrum][starting_peak.peak].getIntensity() << ")" << std::endl;
					
					//------------------------------------------------------------------
					//Extend seed to a mass trace
					MassTrace trace;
					const PeakType* seed = &(map_[starting_peak.spectrum][starting_peak.peak]);
					//initialize trace with seed data and extend
					trace.peaks.push_back(std::make_pair(map_[starting_peak.spectrum].getRT(),seed));
					extendMassTrace_(trace, starting_peak.spectrum, seed->getMZ(), false, rt_min, rt_max);
					extendMassTrace_(trace, starting_peak.spectrum, seed->getMZ(), true, rt_min, rt_max);
					
					//check if enough peaks were found
					if (!trace.isValid())
					{
						log_ << "   - could not extend trace " << std::endl;
						//Missing traces in the middle of a pattern are not acceptable => fix this
						if (p<traces.max_trace)
						{
							traces.clear(); //remove earlier traces
							continue;
						}
						else if (p>traces.max_trace)
						{
            	break; //no more traces are possible
						}
					}
					traces.push_back(trace);
					traces.back().theoretical_int = pattern.theoretical_ints.intensity[p];
				}
			}

			/**
				@brief Extends a single mass trace in one RT direction
				
				How to use this method:
				- Add the starting peak to the @p trace
				- Extend in downstream direction
				- extend in upstream direction
				
				@note this method assumes that it extends from a local maximum.
			*/
			void extendMassTrace_(MassTrace& trace, Int spectrum_index, DoubleReal mz, bool inc_rt, DoubleReal min_rt=0.0, DoubleReal max_rt = 0.0)
			{
				//Reverse peaks if we run the method for the second time (to keep them in chronological order)
				if (inc_rt)
				{
					++spectrum_index;
					std::reverse(trace.peaks.begin(), trace.peaks.end());
				}
				else
				{
					--spectrum_index;
				}
				//check if boundaries are set
				bool boundaries = false;
				if (max_rt != min_rt)
				{
					boundaries = true;
				}
				//Relax slope theshold if there is a hard boundary for the extension
				DoubleReal current_slope_bound = (1.0 + (DoubleReal)boundaries) * slope_bound_;
				UInt delta_count = min_spectra_;
				DoubleReal last_int = trace.peaks.back().second->getIntensity();
				std::vector<DoubleReal> deltas(delta_count-1, 0);
				//deltas.reserve(2*delta_count);
				UInt missing_peaks = 0;
				UInt peaks_before = trace.peaks.size();
				String abort_reason = "";
				while((!inc_rt && spectrum_index>=0) || (inc_rt && spectrum_index<(Int)map_.size()))
				{
					if(boundaries && ((!inc_rt && map_[spectrum_index].getRT()<min_rt) || (inc_rt && map_[spectrum_index].getRT()>max_rt)) )
					{
						abort_reason = "Hit upper/lower boundary";
						break;
					}
					Int peak_index = -1;
					try
					{
						peak_index = map_[spectrum_index].findNearest(mz);
					}
					catch(...) //no peaks in the spectrum
					{
						peak_index=-1;
					}
					if (peak_index<0 || map_[spectrum_index].getMetaDataArrays()[2][peak_index]<0.01 || positionScore_( mz, map_[spectrum_index][peak_index].getMZ(), trace_tolerance_)==0.0)
					{
						++missing_peaks;
						if(missing_peaks>max_missing_trace_peaks_)
						{
							abort_reason = "too many peaks missing";
							break;
						}
					}
					else
					{
						missing_peaks = 0;
						//add last peak to trace
						trace.peaks.push_back(std::make_pair(map_[spectrum_index].getRT(),&(map_[spectrum_index][peak_index])));
						//update ints and deltas 
						deltas.push_back( (map_[spectrum_index][peak_index].getIntensity() - last_int) / last_int);
						last_int = map_[spectrum_index][peak_index].getIntensity();

						//Abort if the average delta is too big (as intensity increases then)
						DoubleReal average_delta = std::accumulate(deltas.end()-delta_count,deltas.end(),0.0) / (DoubleReal)delta_count;
						if ( average_delta > current_slope_bound)
						{
							abort_reason = String("Average delta above threshold: ")+average_delta+"/"+current_slope_bound;
							//remove last peaks as we extended too far
							UInt remove = std::min((UInt)(trace.peaks.size()-peaks_before),delta_count-1);
							trace.peaks.erase(trace.peaks.end()-remove,trace.peaks.end());
							break;
						}
					}
					//increase/decrease scan index
					if (inc_rt) ++spectrum_index; else --spectrum_index;
				}
				log_ << "   - Added " << (trace.peaks.size()-peaks_before) << " peaks (abort: " << abort_reason << ")" << std::endl;
			}

			/// Returns the index of the peak nearest to m/z @p pos in spectrum @p spec (linear search starting from index @p start)
			template <typename SpectrumType>
			UInt nearest_(DoubleReal pos, const SpectrumType& spec, UInt start) const
			{
				UInt index = start;
				DoubleReal dist = std::fabs(pos-spec[index].getMZ());
				++index;
				while (index < spec.size())
				{
					DoubleReal new_dist = std::fabs(pos-spec[index].getMZ());
					if (new_dist<dist)
					{
						dist = new_dist;
						++index;	
					}
					else
					{
						break;
					}
				}
				return --index; 
			}
			
			/**
				@brief Searches for an isotopic peak in the current spectrum and the adjacent spectra
				
				@param pos m/z position of the searched for peak
				@param spectrum_index index of the central spectrum
				@param pattern IsotopePattern to store found peaks
				@param pattern_index index of the isotope in the pattern
				@param debug Flag that turn on debug info
				@param peak_index starting index of the search (to avoid multiple binary searches)
			*/
			void findIsotope_(DoubleReal pos, UInt spectrum_index, IsotopePattern& pattern, UInt pattern_index, bool debug, UInt& peak_index)
			{
				//search in the center spectrum
				const SpectrumType& spectrum = map_[spectrum_index];
				peak_index = nearest_(pos, spectrum, peak_index);
				DoubleReal mz_score = positionScore_(pos, spectrum[peak_index].getMZ(), pattern_tolerance_);
				pattern.theoretical_mz[pattern_index] = pos;
				if (mz_score!=0.0)
				{
					if (debug) log_ << "   - Isotope " << pattern_index << ": " << spectrum[peak_index].getIntensity() << std::endl;
					pattern.peak[pattern_index] = peak_index;
					pattern.spectrum[pattern_index] = spectrum_index;
					pattern.mz_score[pattern_index] = mz_score;
					pattern.intensity[pattern_index] = spectrum[peak_index].getIntensity();
					return;
				}
				//try to find the mass in the previous spectrum if it contains peaks)
				if (spectrum_index!=0 && map_[spectrum_index-1].size()>0)
				{
					const SpectrumType& spectrum_before = map_[spectrum_index-1];
					UInt index_before = spectrum_before.findNearest(pos);
					if (positionScore_(pos, spectrum_before[index_before].getMZ(), pattern_tolerance_)!=0.0)
					{
						if (debug) log_ << "   - Isotope " << pattern_index << ": " << spectrum_before[index_before].getIntensity() << " - previous spectrum" << std::endl;
						pattern.peak[pattern_index] = index_before;
						pattern.spectrum[pattern_index] = spectrum_index-1;
						pattern.mz_score[pattern_index] = positionScore_(pos, spectrum_before[index_before].getMZ(), pattern_tolerance_);
						pattern.intensity[pattern_index] = spectrum_before[index_before].getIntensity();
						return;
					}
				}
				//try to find the mass in the next spectrum (if it contains peaks)
				if (spectrum_index!=map_.size()-1 && map_[spectrum_index+1].size()>0)
				{
					const SpectrumType& spectrum_after = map_[spectrum_index+1];
					UInt index_after = spectrum_after.findNearest(pos);
					if (positionScore_(pos, spectrum_after[index_after].getMZ(), pattern_tolerance_)!=0.0)
					{
						if (debug) if (debug) log_ << "   - Isotope " << pattern_index << ": " << spectrum_after[index_after].getIntensity() << " - next spectrum" << std::endl;
						pattern.peak[pattern_index] = index_after;
						pattern.spectrum[pattern_index] = spectrum_index+1;
						pattern.mz_score[pattern_index] = positionScore_(pos, spectrum_after[index_after].getMZ(), pattern_tolerance_);
						pattern.intensity[pattern_index] = spectrum_after[index_after].getIntensity();
						return;
					}
				}
				//no isotope found
				if (debug) log_ << "   - Isotope " << pattern_index << ": missing" << std::endl;
				pattern.peak[pattern_index] = -1;
				pattern.mz_score[pattern_index] = 0.0;
				pattern.intensity[pattern_index] = 0.0;
			}

			/// Calculates a score between 0 and 1 for the m/z deviation of two peaks.
			DoubleReal positionScore_(DoubleReal pos1, DoubleReal pos2, DoubleReal allowed_deviation) const
			{
				DoubleReal diff = fabs(pos1 - pos2);
				if (diff <= 0.5*allowed_deviation)
				{
					return 0.1*(0.5*allowed_deviation-diff)/(0.5*allowed_deviation)+0.9;
				}
				else if (diff <= allowed_deviation)
				{
					return 0.9*(allowed_deviation-diff)/(0.5*allowed_deviation);
				}
				return 0.0;
			}

			/// Calculates a score between 0 and 1 for the correlation between theoretical and found isotope pattern
			DoubleReal isotopeScore_(const TheoreticalIsotopePattern& isotopes, IsotopePattern& pattern, bool consider_mz_distances, bool debug)
			{
				if (debug) log_ << "   - fitting " << pattern.intensity.size() << " peaks" << std::endl;
				//Abort if a core peak is missing
				for (UInt iso=0+isotopes.optional_begin; iso<pattern.peak.size()-isotopes.optional_end; ++iso)
				{
					if (pattern.peak[iso]==-1)
					{
						if (debug) log_ << "   - aborting: core peak is missing" << std::endl;
						return 0.0;
					}
				}
				//Find best isotope fit
				// - try to leave out optional isotope peaks to improve the fit
				// - do not allow gaps inside the pattern
				DoubleReal best_int_score = 0.01; //Not 0 as this would result in problems when checking for the percental improvement
				UInt best_begin = 0;
				for (UInt i=isotopes.optional_begin; i>0; --i)
				{
					if (pattern.peak[i-1]==-1)
					{
						best_begin = i;
						break;
					}
				}
				UInt best_end = 0;
				for (UInt i=isotopes.optional_end; i>0; --i)
				{
					if (pattern.peak[pattern.peak.size()-i]==-1)
					{
						best_end = i;
						break;
					}
				}
				if (debug) log_ << "   - best_begin/end: " << best_begin << "/" << best_end << std::endl;
				for (UInt b=best_begin; b<=isotopes.optional_begin; ++b)
				{
					for (UInt e=best_end; e<=isotopes.optional_end; ++e)
					{
						//Make sure we have more than 2 peaks (unless in the first loop interation, there we allow two points) 
						if (isotopes.size()-b-e>2 || (b==best_begin && e==best_end && isotopes.size()-b-e>1))
						{
							DoubleReal int_score = Math::pearsonCorrelationCoefficient(isotopes.intensity.begin()+b, isotopes.intensity.end()-e, pattern.intensity.begin()+b, pattern.intensity.end()-e);	
							if (isnan(int_score)) int_score = 0.0;
							if (isotopes.size()-b-e==2 && int_score>min_isotope_fit_) int_score = min_isotope_fit_; //special case for the first loop iteration (otherwise the score is 1)
							if (debug) log_ << "   - fit (" << b << "/" << e << "): " << int_score;
							if (int_score/best_int_score>=1.0+optional_fit_improvement_)
							{
								if (debug) log_ << " - new best fit ";
								best_int_score = int_score;
								best_begin = b;
								best_end = e;
							}
							if (debug) log_ << std::endl;
						}
					}
				}
				
				//if the best fit is empty, abort
				if (pattern.mz_score.size()-best_begin-best_end==0)
				{
					return 0.0;
				}
				
				//remove left out peaks from the beginning
				for (UInt i=0; i<best_begin; ++i)
				{
					pattern.peak[i] = -2;
					pattern.intensity[i] = 0.0;
					pattern.mz_score[i] = 0.0;
				}
				//remove left out peaks from the end
				for (UInt i=0; i<best_end; ++i)
				{
					pattern.peak[isotopes.size()-1-i] = -2;
					pattern.intensity[isotopes.size()-1-i] = 0.0;
					pattern.mz_score[isotopes.size()-1-i] = 0.0;
				}
				//calculate m/z score (if required)
				if (consider_mz_distances)
				{
					best_int_score *= std::accumulate(pattern.mz_score.begin()+best_begin, pattern.mz_score.end()-best_end,0.0) / (pattern.mz_score.size()-best_begin-best_end);
				}

				//return final score
				OPENMS_POSTCONDITION(best_int_score>=0.0,  (String("Internal error: Isotope score (") + best_int_score + ") should be >=0.0").c_str())
				OPENMS_POSTCONDITION(best_int_score<=1.0,  (String("Internal error: Isotope score (") + best_int_score + ") should be <=1.0").c_str())
				return best_int_score;
			}
			
			DoubleReal intensityScore_(UInt spectrum, UInt peak)
			{
				//calculate (half) bin numbers
				DoubleReal intensity  = map_[spectrum][peak].getIntensity();
				DoubleReal rt = map_[spectrum].getRT();
				DoubleReal mz = map_[spectrum][peak].getMZ();
				DoubleReal rt_min = map_.getMinRT();
				DoubleReal mz_min = map_.getMinMZ();
				UInt rt_bin = std::min(2*intensity_bins_-1,(UInt)std::floor((rt - rt_min) / intensity_rt_step_ * 2.0));
				UInt mz_bin = std::min(2*intensity_bins_-1,(UInt)std::floor((mz - mz_min) / intensity_mz_step_ * 2.0));
				//determine mz bins
				UInt ml,mh;
				if (mz_bin==0 || mz_bin==2*intensity_bins_-1)
				{
					ml = mz_bin/2;
					mh = mz_bin/2;
				}
				else if (Math::isOdd(mz_bin))
				{
					ml = mz_bin/2;
					mh = mz_bin/2+1;
				}
				else
				{
					ml = mz_bin/2-1; 
					mh = mz_bin/2;
				}
				//determine rt bins
				UInt rl,rh;
				if (rt_bin==0 || rt_bin==2*intensity_bins_-1)
				{
					rl = rt_bin/2;
					rh = rt_bin/2;
				}
				else if (Math::isOdd(rt_bin))
				{
					rl = rt_bin/2;
					rh = rt_bin/2+1;
				}
				else
				{
					rl = rt_bin/2-1; 
					rh = rt_bin/2;
				}
				//calculate distances to surrounding points (normalized to [0,1])
				DoubleReal drl = std::fabs(rt_min+(0.5+rl)*intensity_rt_step_-rt)/intensity_rt_step_;
				DoubleReal drh = std::fabs(rt_min+(0.5+rh)*intensity_rt_step_-rt)/intensity_rt_step_;
				DoubleReal dml = std::fabs(mz_min+(0.5+ml)*intensity_mz_step_-mz)/intensity_mz_step_;
				DoubleReal dmh = std::fabs(mz_min+(0.5+mh)*intensity_mz_step_-mz)/intensity_mz_step_;
				//Calculate weights for the intensity scores (the nearer to better)
				DoubleReal d1 = std::sqrt(std::pow(1.0-drl,2.0)+std::pow(1.0-dml,2.0));
				DoubleReal d2 = std::sqrt(std::pow(1.0-drh,2.0)+std::pow(1.0-dml,2.0));
				DoubleReal d3 = std::sqrt(std::pow(1.0-drl,2.0)+std::pow(1.0-dmh,2.0));
				DoubleReal d4 = std::sqrt(std::pow(1.0-drh,2.0)+std::pow(1.0-dmh,2.0));
				DoubleReal d_sum = d1 + d2 + d3 + d4;				
				//Final score
				DoubleReal final = intensityScore_(rl, ml, intensity)*(d1/d_sum)
													+ intensityScore_(rh, ml, intensity)*(d2/d_sum)
													+ intensityScore_(rl, mh, intensity)*(d3/d_sum)
													+ intensityScore_(rh, mh, intensity)*(d4/d_sum);
				
				OPENMS_POSTCONDITION(final>=0.0, (String("Internal error: Intensity score (") + final + ") should be >=0.0").c_str())
				OPENMS_POSTCONDITION(final<=1.0001, (String("Internal error: Intensity score (") + final + ") should be <=1.0").c_str())
				return final;
			}

			DoubleReal intensityScore_(UInt rt_bin, UInt mz_bin, DoubleReal intensity)
			{
				//interpolate score value according to quantiles(20)
				std::vector<DoubleReal>& quantiles20 = intensity_thresholds_[rt_bin][mz_bin];
				std::vector<DoubleReal>::const_iterator it = std::lower_bound(quantiles20.begin(),quantiles20.end(),intensity);
				//bigger than the biggest value => return 1.0
				if (it==quantiles20.end())
				{
					return 1.0;
				}
				//interpolate inside the bin
				DoubleReal bin_score = 0.0;
				if (it==quantiles20.begin())
				{
					bin_score = 0.05 * intensity / *it;
				}
				else
				{
					bin_score = 0.05 * (intensity-*(it-1)) / (*it-*(it-1));
				}
				
				DoubleReal final = bin_score + 0.05*((it - quantiles20.begin()) -1.0);
				
				//fix numerical problems
				if (final<0.0) final = 0.0;
				if (final>1.0) final = 1.0;				
				
				return final;
			}

			static int gaussF_(const gsl_vector* param, void* data, gsl_vector* f)
			{
				MassTraces* traces = static_cast<MassTraces*>(data);
				double height = gsl_vector_get (param, 0);
				double x0 = gsl_vector_get (param, 1);
				double sig = gsl_vector_get (param, 2);
				
				UInt count = 0;
				for (UInt t=0; t< traces->size(); ++t)
				{
					MassTrace& trace = traces->at(t);			
					for (UInt i=0; i<trace.peaks.size(); ++i)
					{
						gsl_vector_set(f, count, traces->baseline + trace.theoretical_int * height * exp(-0.5 * pow(trace.peaks[i].first - x0, 2)  / pow(sig, 2)) - trace.peaks[i].second->getIntensity() );
						++count;
					}
				}
				return GSL_SUCCESS;
			}
		
			static int gaussDF_(const gsl_vector* param, void* data, gsl_matrix* J)
			{
				MassTraces* traces = static_cast<MassTraces*>(data);
				double height = gsl_vector_get (param, 0);
				double x0 = gsl_vector_get (param, 1);
				double sig = gsl_vector_get (param, 2);
				
				UInt count = 0;
				for (UInt t=0; t<traces->size(); ++t)
				{
					MassTrace& trace = traces->at(t);
					for (UInt i=0; i< trace.peaks.size(); ++i)
					{
						DoubleReal rt = trace.peaks[i].first;
						gsl_matrix_set (J, count, 0, trace.theoretical_int * exp(-0.5 * pow(rt-x0,2) / pow(sig,2)));
						gsl_matrix_set (J, count, 1, trace.theoretical_int * height * exp(-0.5 * pow(rt-x0,2) / pow(sig,2)) * (rt-x0) / pow(sig,2));
						gsl_matrix_set (J, count, 2, 0.125 * trace.theoretical_int * height * exp(-0.5 * pow(rt-x0,2) / pow(sig,2)) * pow(rt-x0,2) / pow(sig,3));				
						++count;
					}
				}
			  return GSL_SUCCESS;
			}
		
			static int gaussFDF_(const gsl_vector* param, void* data, gsl_vector* f, gsl_matrix* J)
			{
			  gaussF_(param, data, f);
			  gaussDF_(param, data, J);
			  return GSL_SUCCESS;
			}

		private:
			
			/// Not implemented
			FeatureFinderAlgorithmPicked& operator=(const FeatureFinderAlgorithmPicked&);
			/// Not implemented
			FeatureFinderAlgorithmPicked(const FeatureFinderAlgorithmPicked&);
	};

} // namespace OpenMS

#endif // OPENMS_TRANSFORMATIONS_FEATUREFINDER_FEATUREFINDERALGORITHMPICKED_H
