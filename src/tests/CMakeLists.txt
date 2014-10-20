# --------------------------------------------------------------------------
#                   OpenMS -- Open-Source Mass Spectrometry
# --------------------------------------------------------------------------
# Copyright The OpenMS Team -- Eberhard Karls University Tuebingen,
# ETH Zurich, and Freie Universitaet Berlin 2002-2012.
#
# This software is released under a three-clause BSD license:
#  * Redistributions of source code must retain the above copyright
#    notice, this list of conditions and the following disclaimer.
#  * Redistributions in binary form must reproduce the above copyright
#    notice, this list of conditions and the following disclaimer in the
#    documentation and/or other materials provided with the distribution.
#  * Neither the name of any author or any participating institution
#    may be used to endorse or promote products derived from this software
#    without specific prior written permission.
# For a full list of authors, refer to the file AUTHORS.
# --------------------------------------------------------------------------
# THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS"
# AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
# IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
# ARE DISCLAIMED. IN NO EVENT SHALL ANY OF THE AUTHORS OR THE CONTRIBUTING
# INSTITUTIONS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL,
# EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO,
# PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS;
# OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY,
# WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR
# OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF
# ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
#
# --------------------------------------------------------------------------
# $Maintainer: Stephan Aiche $
# $Authors: Stephan Aiche $
# --------------------------------------------------------------------------

cmake_minimum_required(VERSION 2.8.3 FATAL_ERROR)
project("OpenMS_tests")

option(ENABLE_STYLE_TESTING "Enables checking of code convention violations (cpplint) and static code analysis (cppchecker). Note that this will disable the regular test system." OFF)
option(ENABLE_PIPELINE_TESTING "Enables the additional testing of various TOPPAS pipelines when 'make test' is called." OFF)

#------------------------------------------------------------------------------
# divide available tests .. note that this mostly for internal use to reduce
# test/build time on travis-ci
set(ACTIVE_TESTS "ALL" CACHE INTERNAL "List of activated tests that will be added and executed when calling the test target.")

# check which tests where activated by the user
if(ACTIVE_TESTS STREQUAL "TOPP")
  set(_with_topp_tests ON)
elseif(ACTIVE_TESTS STREQUAL "CLASS_TESTS")
  set(_with_class_tests ON)
elseif(ACTIVE_TESTS STREQUAL "ALL")
  set(_with_class_tests ON)
  set(_with_topp_tests ON)
else()
  message(FATAL_ERROR "Invalid test selection. Please choose either TOPP, CLASS_TESTS, or ALL.")
endif()

#------------------------------------------------------------------------------
# we only test if we have no package target
if("${PACKAGE_TYPE}" STREQUAL "none")
  if(ENABLE_STYLE_TESTING)
    add_subdirectory(coding)
  else()
    ## configure the regular class ..
    if(_with_class_tests)
      add_subdirectory(class_tests)
    endif()

    # TOPP Test project ..
    if(_with_topp_tests)
      add_subdirectory(topp)
    endif()

    # and if requested also toppas pipeline tests
    if(ENABLE_PIPELINE_TESTING)
      add_subdirectory(toppas)
    endif()
  endif(ENABLE_STYLE_TESTING)
endif("${PACKAGE_TYPE}" STREQUAL "none")
