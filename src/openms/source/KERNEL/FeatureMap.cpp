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
// $Maintainer: Chris Bielow $
// $Authors: Marc Sturm, Chris Bielow, Clemens Groepl $
// --------------------------------------------------------------------------

#include <OpenMS/KERNEL/FeatureMap.h>

namespace OpenMS
{

  std::ostream & operator<<(std::ostream & os, const AnnotationStatistics& ann)
  {
    os << "Feature annotation with identifications:" << "\n";
    for (Size i=0; i<ann.states.size(); ++i)
    {
      os << "    " << BaseFeature::NamesOfAnnotationState[i] << ": " << ann.states[i] << "\n";
    }
    os << std::endl;
    return os;
  }

  std::ostream & operator<<(std::ostream & os, const FeatureMap & map)
  {
    os << "# -- DFEATUREMAP BEGIN --" << "\n";
    os << "# POS \tINTENS\tOVALLQ\tCHARGE\tUniqueID" << "\n";
    for (FeatureMap::const_iterator iter = map.begin(); iter != map.end(); ++iter)
    {
      os << iter->getPosition() << '\t'
      << iter->getIntensity() << '\t'
      << iter->getOverallQuality() << '\t'
      << iter->getCharge() << '\t'
      << iter->getUniqueId() << "\n";
    }
    os << "# -- DFEATUREMAP END --" << std::endl;
    return os;
  }
  
}
