// -*- Mode: C++; tab-width: 2; -*-
// vi: set ts=2:
//
// --------------------------------------------------------------------------
//                   OpenMS Mass Spectrometry Framework
// --------------------------------------------------------------------------
//  Copyright (C) 2003-2010 -- Oliver Kohlbacher, Knut Reinert
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
// $Maintainer: Lars Nilse $
// $Authors: Steffen Sass, Holger Plattfaut $
// --------------------------------------------------------------------------

#ifndef OPENMS_FILTERING_DATAREDUCTION_SILACFILTERING_H
#define OPENMS_FILTERING_DATAREDUCTION_SILACFILTERING_H

#include <OpenMS/KERNEL/StandardTypes.h>
#include <OpenMS/KERNEL/MSExperiment.h>
#include <OpenMS/FILTERING/DATAREDUCTION/SILACFilter.h>
#include <OpenMS/DATASTRUCTURES/DataPoint.h>
#include <OpenMS/CONCEPT/ProgressLogger.h>
#include <gsl/gsl_interp.h>
#include <gsl/gsl_spline.h>
#include<list>
#include<map>
#include<vector>

namespace OpenMS
{
class SILACFilter;

/**
 * @brief Filtering for SILAC data.
 * This filtering can be used to extract SILAC features from an MS experiment. Several SILACFilters can be added to the filtering to search for specific SILAC patterns.
 * @see SILACFilter
 */
class OPENMS_DLLAPI SILACFiltering : public ProgressLogger{
	friend class SILACFilter;
private:
		
	/**
	 * @brief holds all filters used in the filtering
	 */
	std::list<SILACFilter*> filters;
	
	/**
	 * @brief average m/z distance between scanned data points
	 */
	static DoubleReal mz_stepwidth;
	
	/**
	 * @brief minimal intensity of SILAC features
	 */
	static DoubleReal intensity_cutoff;
	
	/**
	 * @brief minimal intensity correlation between regions of different peaks
	 */
	static DoubleReal intensity_correlation;
	
	/**
	 * @brief flag for missing peaks
	 */
	static bool allow_missing_peaks;
	
	static gsl_interp_accel* current_lin;
	static gsl_interp_accel* current_spl;
	static gsl_spline* spline_lin;
	static gsl_spline* spline_spl;
	
	/**
	 * @brief current feature id
	 */
	static Int feature_id;

	static DoubleReal mz_min;
	
	/**
	 * @brief raw data
	 */
	MSExperiment<Peak1D>& exp;
	

public:
	/**
	 * @brief detailed constructor
	 * @param exp raw data
	 * @param mz_stepwidth_ average m/z distance between scanned data points
	 * @param intensity_cutoff_ minimal intensity of SILAC features
	 */
	SILACFiltering(MSExperiment<Peak1D>& exp_, DoubleReal mz_stepwidth_, DoubleReal intensity_cutoff_, DoubleReal intensity_correlation_, bool allow_missing_peaks);
	
	/**
	 * destructor
	 */
	virtual ~SILACFiltering();
	
	/**
	 * @brief adds a new filter to the filtering
	 * @param filter filter to add
	 */
	void addFilter(SILACFilter& filter);
	
	/**
	 * @brief starts the filtering based on the added filters
	 */
	void filterDataPoints();
	
	/**
	 * @brief puts m/z values on the filter's blacklist
	 * @param peak_positions m/z positions to be added
	 * @param source filter, from which the value originate
	 */
	void blockPositions(const std::vector<DoubleReal>& peak_positions,SILACFilter* source);
	
	/**
	 * @brief structure for blacklist
	 * @param mzBlack_min starting point of blacklisted m/z range
	 * @param mzBlack_max end point of blacklisted m/z range
   * @param rtBlack_center rt that generated the blacklist entry
   * @param rtBlack_min starting point of blacklisted rt range
   * @param rtBlack_max end point of blacklisted rt range
	 * @param generatingFilter filter that generated blacklist entry
	 */
	struct BlacklistEntry
	{
		DoubleReal mzBlack_min;
		DoubleReal mzBlack_max;
    DoubleReal rtBlack_center;
    DoubleReal rtBlack_min;
    DoubleReal rtBlack_max;
		SILACFilter* generatingFilter;
	};
	
	/**
	 * @brief holds the m/z range and rt age that are ignored and the filter that generated the blacklist entry 
	 */
 	std::list<BlacklistEntry> blacklist;

};

}

#endif /* SILACFILTERING_H_ */