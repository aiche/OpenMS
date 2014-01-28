// --------------------------------------------------------------------------
//                   OpenMS -- Open-Source Mass Spectrometry
// --------------------------------------------------------------------------
// Copyright The OpenMS Team -- Eberhard Karls University Tuebingen,
// ETH Zurich, and Freie Universitaet Berlin 2002-2013.
//
// This software is released under a three-clause BSD license:
//  * Redistributions of source code must retain the above copyright
//    notice, this list of conditions and the following disclaimer.
//  * Redistributions in binary form must reproduce the above copyright
//    notice, this list of conditions and the following disclaimer in the
//    documentation and/or other materials provided with the distribution.
//  * Neither the name of any author or any participating institution
//    may be used to endorse or promote products derived from this software
//    without specific prior written permission.
// For a full list of authors, refer to the file AUTHORS.
// --------------------------------------------------------------------------
// THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS"
// AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
// IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
// ARE DISCLAIMED. IN NO EVENT SHALL ANY OF THE AUTHORS OR THE CONTRIBUTING
// INSTITUTIONS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL,
// EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO,
// PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS;
// OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY,
// WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR
// OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF
// ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
//
// --------------------------------------------------------------------------
// $Maintainer: Hendrik Weisser $
// $Authors: Hannes Roest, Hendrik Weisser $
// --------------------------------------------------------------------------

#include <numeric> // for "accumulate"

#include <OpenMS/ANALYSIS/OPENSWATH/ConfidenceScoring.h>

#include <OpenMS/KERNEL/Feature.h>
#include <OpenMS/KERNEL/FeatureMap.h>

using namespace std;

namespace OpenMS
{

  /// Randomize the list of decoy indexes
  void ConfidenceScoring::chooseDecoys_()
  {
    if (n_decoys_ == 0) return;   // list is already initialized

    // somewhat inefficient to shuffle the whole list when we only need a random
    // sample, but easy to do...
    random_shuffle(decoy_index_.begin(), decoy_index_.end(), rand_gen_);
  }

  // DoubleReal rmsd_(DoubleList x, DoubleList y)
  // {
  //   DoubleReal sum_of_squares = 0;
  //   for (Size i = 0; i < x.size(); i++)
  //   {
  //     DoubleReal diff = x[i] - y[i];
  //     sum_of_squares += diff * diff;
  //   }
  //   return sqrt(sum_of_squares / x.size());
  // }

  /// Manhattan distance
  DoubleReal ConfidenceScoring::manhattanDist_(DoubleList x, DoubleList y)
  {
    DoubleReal sum = 0;
    for (Size i = 0; i < x.size(); ++i)
    {
      sum += fabs(x[i] - y[i]);
    }
    return sum;
  }

  /// Get the retention time of an assay
  DoubleReal ConfidenceScoring::getAssayRT_(const TargetedExperiment::Peptide& assay,
                                            const String& cv_accession)
  {
    OPENMS_PRECONDITION(assay.rts.size() > 0, "More than zero RTs needed")
    OPENMS_PRECONDITION(assay.rts[0].getCVTerms()[cv_accession].size() >  0, "More than zero cv terms of retention time needed")
    String value = assay.rts[0].getCVTerms()[cv_accession][0].getValue();
    return value.toDouble();
  }

    // double rmsd_(DoubleList x, DoubleList y)
    // {
    //   double sum_of_squares = 0;
    //   for (Size i = 0; i < x.size(); i++)
    //   {
    //     double diff = x[i] - y[i];
    //     sum_of_squares += diff * diff;
    //   }
    //   return sqrt(sum_of_squares / x.size());
    // }

    /// Manhattan distance
    double ConfidenceScoring::manhattanDist_(DoubleList x, DoubleList y)
    {
      double sum = 0;
      for (Size i = 0; i < x.size(); ++i)
      {
        intensity_map.right.erase(intensity_map.right.begin());
      }
    }
    // fill output list ordered by m/z:
    intensities.clear();
    for (BimapType::left_map::iterator int_it = intensity_map.left.begin();
         int_it != intensity_map.left.end(); ++int_it)
    {
      // missing values might be "-1"
      intensities.push_back(max(0.0, int_it->second));
    }
  }

    /// Get the retention time of an assay
    double ConfidenceScoring::getAssayRT_(const TargetedExperiment::Peptide& assay,
                           const String& cv_accession)
    {
      const ReactionMonitoringTransition& transition =
        library_.getTransitions()[*trans_it];
      // for the "true" assay, we need to choose the same transitions as for the
      // feature:
      if (!transition_ids.empty() &&
          (transition_ids.count(transition.getNativeID()) == 0)) continue;
      // seems like Boost's Bimap doesn't support "operator[]"...
      intensity_map.left.insert(make_pair(transition.getProductMZ(),
                                          transition.getLibraryIntensity()));
    }
    DoubleList assay_intensities;
    extractIntensities_(intensity_map, feature_intensities.size(),
                        assay_intensities);

    if (feature_intensities.empty())
    {
      throw Exception::IllegalArgument(__FILE__, __LINE__, __PRETTY_FUNCTION__,
                                       "Feature intensities were empty - please provide feature subordinate with intensities");
    }
    if (feature_intensities.size() != assay_intensities.size())
    {
      throw Exception::IllegalArgument(__FILE__, __LINE__, __PRETTY_FUNCTION__,
                                       "Did not find a feature for each assay provided - each feature needs "
                                       "to have n subordinates with the meta-value 'native_id' set to the corresponding transition.");
    }

    // compute intensity distance:
    OpenSwath::Scoring::normalize_sum(&feature_intensities[0],
                                      boost::numeric_cast<int>(feature_intensities.size()));
    OpenSwath::Scoring::normalize_sum(&assay_intensities[0],
                                      boost::numeric_cast<int>(assay_intensities.size()));
    DoubleReal dist_int = manhattanDist_(feature_intensities,
                                         assay_intensities);

    DoubleReal score = glm_(diff_rt, dist_int);

    LOG_DEBUG << "\ndelta_RT:  " << fabs(diff_rt)
              << "\ndist_int:  " << dist_int
              << "\nGLM_score: " << score << endl;

    return score;
  }

  /// Score a feature
  void ConfidenceScoring::scoreFeature_(Feature& feature)
  {
    // extract predictors from feature:
    DoubleReal feature_rt = rt_norm_(rt_trafo_.apply(feature.getRT()));
    BimapType intensity_map;
    // for the "true" assay, we need to make sure we compare based on the same
    // transitions, so keep track of them:
    Map<DoubleReal, String> trans_id_map;   // Q3 m/z -> transition ID
    for (vector<Feature>::iterator sub_it = feature.getSubordinates().begin();
         sub_it != feature.getSubordinates().end(); ++sub_it)
    {
      // seems like Boost's Bimap doesn't support "operator[]"...
      intensity_map.left.insert(make_pair(sub_it->getMZ(),
                                          sub_it->getIntensity()));
      trans_id_map[sub_it->getMZ()] = sub_it->getMetaValue("native_id");
    }
    DoubleList feature_intensities;
    extractIntensities_(intensity_map, n_transitions_, feature_intensities);
    if ((n_transitions_ > 0) && (feature_intensities.size() < n_transitions_))
    {
      LOG_WARN << "Warning: Feature '" << feature.getUniqueId()
               << "' contains only " << feature_intensities.size()
               << " transitions." << endl;
    }
    // "intensity_map" now only contains the transitions we need later:
    std::set<String> transition_ids;
    for (BimapType::left_map::iterator int_it = intensity_map.left.begin();
         int_it != intensity_map.left.end(); ++int_it)
    {
      transition_ids.insert(trans_id_map[int_it->first]);
    }

    DoubleList scores;   // "true" score is in "scores[0]", decoy scores follow

    /// Score the assay @p assay against feature data (@p feature_rt,
    /// @p feature_intensities), optionally using only the specified transitions
    /// (@p transition_ids)
    double ConfidenceScoring::scoreAssay_(const TargetedExperiment::Peptide& assay,
                           double feature_rt, DoubleList& feature_intensities,
                           const std::set<String>& transition_ids)
    {
      // compute RT difference:
      double assay_rt = rt_norm_(getAssayRT_(assay));
      double diff_rt = assay_rt - feature_rt;

      // collect transition intensities:
      BimapType intensity_map;
      for (IntList::iterator trans_it = transition_map_[assay.id].begin();
           trans_it != transition_map_[assay.id].end(); ++trans_it)
      {
        const ReactionMonitoringTransition& transition =
          library_.getTransitions()[*trans_it];
        // for the "true" assay, we need to choose the same transitions as for the
        // feature:
        if (!transition_ids.empty() &&
            (transition_ids.count(transition.getNativeID()) == 0)) continue;
        // seems like Boost's Bimap doesn't support "operator[]"...
        intensity_map.left.insert(make_pair(transition.getProductMZ(),
                                            transition.getLibraryIntensity()));
      }
      DoubleList assay_intensities;
      extractIntensities_(intensity_map, feature_intensities.size(),
                          assay_intensities);

    // compare to decoy assays:
    chooseDecoys_();
    Size counter = 0;
    for (IntList::iterator decoy_it = decoy_index_.begin();
         decoy_it != decoy_index_.end(); ++decoy_it)
    {
      const TargetedExperiment::Peptide& decoy_assay =
        library_.getPeptides()[*decoy_it];

      // skip the "true" assay and assays with too few transitions:
      // TODO: maybe add an option to include assays with too few transitions?
      if ((decoy_assay.id == true_id) ||
          (transition_map_[decoy_assay.id].size() < feature_intensities.size()))
      {
        continue;
      }
      LOG_DEBUG << "Decoy assay " << scores.size() << " (ID '" << decoy_assay.id
                << "')" << endl;

      // compute intensity distance:
      OpenSwath::Scoring::normalize_sum(&feature_intensities[0],
                                        boost::numeric_cast<int>(feature_intensities.size()));
      OpenSwath::Scoring::normalize_sum(&assay_intensities[0],
                                        boost::numeric_cast<int>(assay_intensities.size()));
      double dist_int = manhattanDist_(feature_intensities,
                                           assay_intensities);

      double score = glm_(diff_rt, dist_int);

    Size n_scores = scores.size();
    if (n_scores - 1 < n_decoys_)
    {
      LOG_WARN << "Warning: Feature '" << feature.getUniqueId()
               << "': Couldn't find enough decoy assays with at least "
               << feature_intensities.size() << " transitions. "
               << "Scoring based on " << n_scores - 1 << " decoys." << endl;
    }
    // TODO: this warning may trigger for every feature and get annoying
    if ((n_decoys_ == 0) && (n_scores < library_.getPeptides().size()))
    {
      LOG_WARN << "Warning: Feature '" << feature.getUniqueId()
               << "': Skipped some decoy assays with fewer than "
               << feature_intensities.size() << " transitions. "
               << "Scoring based on " << n_scores - 1 << " decoys." << endl;
    }

    // count decoy scores that are greater than the "true" score:
    counter = 0;
    for (DoubleList::iterator it = ++scores.begin(); it != scores.end(); ++it)
    {
      if (*it > scores[0]) counter++;
    }

    // annotate feature:
    feature.setMetaValue("GLM_score", scores[0]);
    DoubleReal local_fdr = counter / (n_scores - 1.0);
    feature.setMetaValue("local_FDR", local_fdr);
    feature.setOverallQuality(1.0 - local_fdr);
  }

  void ConfidenceScoring::initialize(TargetedExperiment library, Size n_decoys, Size n_transitions, TransformationDescription rt_trafo)
  {
    library_ = TargetedExperiment(library);
    n_decoys_ = n_decoys;
    n_transitions_ = n_transitions;
    rt_trafo_ = rt_trafo;
  }

  void ConfidenceScoring::initializeGlm(double intercept, double rt_coef, double int_coef)
  {
    glm_.intercept = intercept;
    glm_.rt_coef = rt_coef;
    glm_.int_coef = int_coef;
  }

  void ConfidenceScoring::scoreMap(FeatureMap& features)
  {
    // are there enough assays in the library?
    Size n_assays = library_.getPeptides().size();
    if (n_assays < 2)
    {
      // extract predictors from feature:
      double feature_rt = rt_norm_(rt_trafo_.apply(feature.getRT()));
      BimapType intensity_map;
      // for the "true" assay, we need to make sure we compare based on the same
      // transitions, so keep track of them:
      Map<double, String> trans_id_map; // Q3 m/z -> transition ID
      for (vector<Feature>::iterator sub_it = feature.getSubordinates().begin();
           sub_it != feature.getSubordinates().end(); ++sub_it)
      {
        // seems like Boost's Bimap doesn't support "operator[]"...
        intensity_map.left.insert(make_pair(sub_it->getMZ(),
                                            sub_it->getIntensity()));
        trans_id_map[sub_it->getMZ()] = sub_it->getMetaValue("native_id");
      }
      DoubleList feature_intensities;
      extractIntensities_(intensity_map, n_transitions_, feature_intensities);
      if ((n_transitions_ > 0) && (feature_intensities.size() < n_transitions_))
      {
        LOG_WARN << "Warning: Feature '" << feature.getUniqueId()
                 << "' contains only " << feature_intensities.size()
                 << " transitions." << endl;
      }
      // "intensity_map" now only contains the transitions we need later:
      std::set<String> transition_ids;
      for (BimapType::left_map::iterator int_it = intensity_map.left.begin();
           int_it != intensity_map.left.end(); ++int_it)
      {
        transition_ids.insert(trans_id_map[int_it->first]);
      }

    decoy_index_.resize(n_assays);
    for (Size i = 0; i < n_assays; ++i)
      decoy_index_[i] = boost::numeric_cast<Int>(i);

    // build mapping between assays and transitions:
    LOG_DEBUG << "Building transition map..." << std::endl;
    for (Size i = 0; i < library_.getTransitions().size(); ++i)
    {
      const String& ref = library_.getTransitions()[i].getPeptideRef();
      transition_map_[ref].push_back(boost::numeric_cast<Int>(i));
    }
    // find min./max. RT in the library:
    LOG_DEBUG << "Determining retention time range..." << std::endl;
    rt_norm_.min_rt = std::numeric_limits<double>::infinity();
    rt_norm_.max_rt = -std::numeric_limits<double>::infinity();
    for (std::vector<TargetedExperiment::Peptide>::const_iterator it =
           library_.getPeptides().begin(); it != library_.getPeptides().end();
         ++it)
    {
      DoubleReal current_rt = getAssayRT_(*it);
      if (current_rt == -1.0) continue; // indicates a missing value
      rt_norm_.min_rt = std::min(rt_norm_.min_rt, current_rt);
      rt_norm_.max_rt = std::max(rt_norm_.max_rt, current_rt);
    }

    // log scoring progress:
    LOG_DEBUG << "Scoring features..." << std::endl;
    startProgress(0, features.size(), "scoring features");

      // annotate feature:
      feature.setMetaValue("GLM_score", scores[0]);
      double local_fdr = counter / (n_scores - 1.0);
      feature.setMetaValue("local_FDR", local_fdr);
      feature.setOverallQuality(1.0 - local_fdr);
    }
    endProgress();

  }

}
