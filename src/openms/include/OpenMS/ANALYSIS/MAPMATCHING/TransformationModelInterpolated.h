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
// $Maintainer: Stephan Aiche $
// $Authors: Stephan Aiche $
// --------------------------------------------------------------------------

#ifndef OPENMS_ANALYSIS_MAPMATCHING_TRANSFORMATIONMODELINTERPOLATED_H
#define OPENMS_ANALYSIS_MAPMATCHING_TRANSFORMATIONMODELINTERPOLATED_H

#include <OpenMS/config.h>

#include <OpenMS/ANALYSIS/MAPMATCHING/TransformationModel.h>
#include <OpenMS/ANALYSIS/MAPMATCHING/TransformationModelLinear.h>

#include <OpenMS/MATH/MISC/Spline2d.h>

namespace OpenMS
{
  /**
    @brief Interpolation model for transformations

    Between the data points, the interpolation uses the neighboring points. Outside the range spanned by the points, we extrapolate using a line through the first and the last point.

    Interpolation is done by a cubic spline. Note that at least 4 data point are required.

    @ingroup MapAlignment
  */
  class OPENMS_DLLAPI TransformationModelInterpolated :
    public TransformationModel
  {
public:
    /**
         @brief Constructor

         @exception IllegalArgument is thrown if there are not enough data points or if an unknown interpolation type is given.
    */
    TransformationModelInterpolated(const DataPoints & data,
                                    const Param & params);

    /// Destructor
    ~TransformationModelInterpolated();

    /// Evaluates the model at the given value
    double evaluate(const double value) const;

    /// Gets the default parameters
    static void getDefaultParameters(Param & params);

protected:
    /// Data coordinates
    std::vector<double> x_, y_;
    /// Interpolation function
    Spline2d<double> * interp_;
    /// Linear model for extrapolation
    TransformationModelLinear * lm_;
  };

} // namespace

#endif // OPENMS_ANALYSIS_MAPMATCHING_TRANSFORMATIONMODELINTERPOLATED_H