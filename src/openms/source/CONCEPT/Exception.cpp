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
// $Maintainer: Stephan Aiche$
// $Authors: Marc Sturm $
// --------------------------------------------------------------------------

#include <OpenMS/CONCEPT/Exception.h>
#include <OpenMS/CONCEPT/GlobalExceptionHandler.h>

#include <iostream>
#include <typeinfo>
#include <exception>
#include <iostream>
#include <cstdio>
#include <sstream>

#define DEF_EXCEPTION(a, b) \
  a::a(const char* file, int line, const char* function) throw() :                                                           \
    BaseException(file, line, function, # a, b) \
  { \
  } \

/**
  @brief Defines assignment operator, copy constructor, and destructor for
  default exceptions. Should be used in combinations with DECLARE_EXCEPTION_OP.

  @note This macro can only be used if the exception has <b>no</b> own data
        members.
*/
#define DEFINE_EXCEPTION_OP(T)      \
  T& T::operator=(T const& rhs)     \
  {                                 \
    BaseException::operator=(rhs);  \
    return *this;                   \
  }                                 \
  T::T(T const& other) :            \
    BaseException(other)            \
  {}                                \
  T::~T() throw()                   \
  {}                                \

using namespace std;

namespace OpenMS
{

  namespace Exception
  {

    BaseException::BaseException() throw() :
      file_("?"),
      line_(-1),
      function_("?"),
      name_("Exception"),
      what_("unspecified error")
    {
      GlobalExceptionHandler::getInstance().set(file_, line_, function_, std::string(name_), std::string(what_));
    }

    BaseException::BaseException(const char* file, int line, const char* function, const std::string& name, const std::string& message) throw() :
      file_(file),
      line_(line),
      function_(function),
      name_(name),
      what_(message)
    {
      GlobalExceptionHandler::getInstance().set(file_, line_, function_, name_, what_);
    }

    BaseException::BaseException(const char* file, int line, const char* function) throw() :
      file_(file),
      line_(line),
      function_(function),
      name_("Exception"),
      what_("unknown error")
    {
      GlobalExceptionHandler::getInstance().set(file_, line_, function_, name_, what_);
    }

    BaseException::BaseException(const BaseException& exception) throw() :
      std::exception(exception),
      file_(exception.file_),
      line_(exception.line_),
      function_(exception.function_),
      name_(exception.name_),
      what_(exception.what_)
    {
    }

    BaseException::~BaseException() throw()
    {
    }

    BaseException& BaseException::operator=(const BaseException& rhs) throw()
    {
      std::exception::operator=(rhs);
      file_ = rhs.file_;
      line_ = rhs.line_;
      function_ = rhs.function_;
      name_ = rhs.name_;
      what_ = rhs.what_;

      return *this;
    }

    const char* BaseException::getName() const throw()
    {
      return name_.c_str();
    }

    const char* BaseException::what() const throw()
    {
      return what_.c_str();
    }

    const char* BaseException::getFile() const throw()
    {
      return file_;
    }

    const char* BaseException::getFunction() const throw()
    {
      return function_;
    }

    const char* BaseException::getMessage() const throw()
    {
      return what_.c_str();
    }

    int BaseException::getLine() const throw()
    {
      return line_;
    }

    void BaseException::setMessage(const std::string& message) throw()
    {
      what_ = message;
    }

    DEFINE_EXCEPTION_OP(Precondition)

    Precondition::Precondition(const char* file, int line, const char* function, const string& condition) throw() :
      BaseException(file, line, function, "Precondition failed", "")
    {
      what_ += std::string(condition);
      GlobalExceptionHandler::getInstance().setMessage(what_);
    }

    DEFINE_EXCEPTION_OP(Postcondition)

    Postcondition::Postcondition(const char* file, int line, const char* function, const string& condition) throw() :
      BaseException(file, line, function, "Postcondition failed", "")
    {
      what_ += std::string(condition);
      GlobalExceptionHandler::getInstance().setMessage(what_);
    }

    DEFINE_EXCEPTION_OP(IndexUnderflow)

    IndexUnderflow::IndexUnderflow(const char* file, int line, const char* function, SignedSize index, Size size) throw() :
      BaseException(file, line, function, "IndexUnderflow", "")
    {
      what_ = "the given index was too small: ";
      char buf[40];

      sprintf(buf, "%ld", (long)index);
      what_ += buf;
      what_ += " (size = ";

      sprintf(buf, "%ld", (long)size);
      what_ += buf;
      what_ += ")";

      GlobalExceptionHandler::getInstance().setMessage(what_);
    }

    DEFINE_EXCEPTION_OP(IndexOverflow)

    IndexOverflow::IndexOverflow(const char* file, int line, const char* function, SignedSize index, Size size) throw() :
      BaseException(file, line, function, "IndexOverflow", "an index was too large")
    {
      what_ = "the given index was too large: ";
      char buf[40];

      sprintf(buf, "%ld", (long)index);
      what_ += buf;
      what_ += " (size = ";

      sprintf(buf, "%ld", (long)size);
      what_ += buf;
      what_ += ")";

      GlobalExceptionHandler::getInstance().setMessage(what_);
    }

    DEFINE_EXCEPTION_OP(FailedAPICall)

    FailedAPICall::FailedAPICall(const char* file, int line, const char* function, const std::string& message) throw() :
      BaseException(file, line, function, "FailedAPICall", message)
    {
    }

    DEFINE_EXCEPTION_OP(OutOfMemory)

    OutOfMemory::OutOfMemory(const char* file, int line, const char* function, Size size) throw() :
      BaseException(file, line, function, "OutOfMemory", "a memory allocation failed")
    {
      what_ = "unable to allocate enough memory (size = ";
      char buf[40];

      sprintf(buf, "%ld", (long)size);
      what_ += buf;
      what_ += " bytes) ";

      GlobalExceptionHandler::getInstance().setMessage(what_);
    }

    DEFINE_EXCEPTION_OP(SizeUnderflow)

    SizeUnderflow::SizeUnderflow(const char* file, int line, const char* function, Size size) throw() :
      BaseException(file, line, function, "SizeUnderflow", "")
    {
      what_ = "the given size was too small: ";
      char buf[40];
      sprintf(buf, "%ld", (long)size);

      what_ += buf;
      GlobalExceptionHandler::getInstance().setMessage(what_);
    }

    DEFINE_EXCEPTION_OP(InvalidSize)

    InvalidSize::InvalidSize(const char* file, int line, const char* function, Size size) throw() :
      BaseException(file, line, function, "InvalidSize", "")
    {
      what_ = "the given size was not expected: ";
      char buf[40];
      sprintf(buf, "%ld", (long)size);

      what_ += buf;
      GlobalExceptionHandler::getInstance().setMessage(what_);
    }

    DEFINE_EXCEPTION_OP(IllegalPosition)

    IllegalPosition::IllegalPosition(const char* file, int line, const char* function, float x, float y, float z) throw() :
      BaseException(file, line, function, "IllegalPosition:", "")
    {
      char buf1[40];
      sprintf(buf1, "%f", x);
      char buf2[40];
      sprintf(buf2, "%f", y);
      char buf3[40];
      sprintf(buf3, "%f", z);

      what_ += "(";
      what_ += buf1;
      what_ += ",";
      what_ += buf2;
      what_ += ",";
      what_ += buf3;
      what_ += ")";
      GlobalExceptionHandler::getInstance().setMessage(what_);
    }

    DEFINE_EXCEPTION_OP(ParseError)

    ParseError::ParseError(const char* file, int line, const char* function, const std::string& expression, const std::string& message) throw() :
      BaseException(file, line, function, "Parse Error", "")
    {
      what_ += message;
      what_ += " in: ";
      what_ += expression;
      GlobalExceptionHandler::getInstance().setMessage(what_);
    }

    DEFINE_EXCEPTION_OP(FileNotFound)

    FileNotFound::FileNotFound(const char* file, int line, const char* function, const std::string& filename) throw() :
      BaseException(file, line, function, "FileNotFound", "")
    {
      what_ = "the file '" + filename + "' could not be found";
      GlobalExceptionHandler::getInstance().setMessage(what_);
    }

    DEFINE_EXCEPTION_OP(FileNotReadable)

    FileNotReadable::FileNotReadable(const char* file, int line, const char* function, const std::string& filename) throw() :
      BaseException(file, line, function, "FileNotReadable", "")
    {
      what_ = "the file '" + filename + "' is not readable for the current user";
      GlobalExceptionHandler::getInstance().setMessage(what_);
    }

    DEFINE_EXCEPTION_OP(FileNotWritable)

    FileNotWritable::FileNotWritable(const char* file, int line, const char* function, const std::string& filename) throw() :
      BaseException(file, line, function, "FileNotWritable", "")
    {
      what_ = "the file '" + filename + "' is not writable for the current user";
      GlobalExceptionHandler::getInstance().setMessage(what_);
    }

    DEFINE_EXCEPTION_OP(IOException)

    IOException::IOException(const char* file, int line, const char* function, const std::string& filename) throw() :
      BaseException(file, line, function, "IOException", "")
    {
      what_ = "IO error for file '" + filename + "'";
      GlobalExceptionHandler::getInstance().setMessage(what_);
    }

    DEFINE_EXCEPTION_OP(FileEmpty)

    FileEmpty::FileEmpty(const char* file, int line, const char* function, const std::string& filename) throw() :
      BaseException(file, line, function, "FileEmpty", "")
    {
      what_ = "the file '" + filename + "' is empty";
      GlobalExceptionHandler::getInstance().setMessage(what_);
    }

    DEFINE_EXCEPTION_OP(ConversionError)

    ConversionError::ConversionError(const char* file, int line, const char* function, const std::string& error) throw() :
      BaseException(file, line, function, "ConversionError", "")
    {
      what_ = error;
      GlobalExceptionHandler::getInstance().setMessage(what_);
    }

    DEFINE_EXCEPTION_OP(InvalidValue)

    InvalidValue::InvalidValue(const char* file, int line, const char* function, const std::string& message, const std::string& value) throw() :
      BaseException(file, line, function, "InvalidValue", "")
    {
      stringstream ss;
      ss << "The value '" << value << "' was used but is not valid! " << message;
      what_ = ss.str();
      GlobalExceptionHandler::getInstance().setMessage(what_);
    }

    DEFINE_EXCEPTION_OP(InvalidParameter)

    InvalidParameter::InvalidParameter(const char* file, int line, const char* function, const std::string& message) throw() :
      BaseException(file, line, function, "InvalidParameter", message)
    {
    }

    DEFINE_EXCEPTION_OP(UnableToCreateFile)

    UnableToCreateFile::UnableToCreateFile(const char* file, int line, const char* function, const std::string& filename) throw() :
      BaseException(file, line, function, "UnableToCreateFile", "")
    {
      what_ = "the file '" + filename + "' could not be created";
      GlobalExceptionHandler::getInstance().setMessage(what_);
    }

    DEFINE_EXCEPTION_OP(IllegalArgument)

    IllegalArgument::IllegalArgument(const char* file, int line, const char* function, const string& error_message) throw() :
      BaseException(file, line, function)
    {
      what_ = error_message;
    }

    DEFINE_EXCEPTION_OP(MissingInformation)

    MissingInformation::MissingInformation(const char* file, int line, const char* function, const string& error_message) throw() :
      BaseException(file, line, function)
    {
      what_ = error_message;
    }

    DEFINE_EXCEPTION_OP(ElementNotFound)

    ElementNotFound::ElementNotFound(const char* file, int line, const char* function, const string& element)   throw() :
      BaseException(file, line, function, "ElementNotFound", "")
    {
      what_ = "the element '" + element + "' could not be found";
      GlobalExceptionHandler::getInstance().setMessage(what_);
    }

    DEFINE_EXCEPTION_OP(UnableToFit)

    UnableToFit::UnableToFit(const char* file, int line, const char* function, const string& name, const string& message) throw() :
      BaseException(file, line, function, name, message)
    {
    }

    DEFINE_EXCEPTION_OP(UnableToCalibrate)

    UnableToCalibrate::UnableToCalibrate(const char* file, int line, const char* function, const string& name, const string& message) throw() :
      BaseException(file, line, function, name, message)
    {
    }

    DEFINE_EXCEPTION_OP(DepletedIDPool)

    DepletedIDPool::DepletedIDPool(const char* file, int line, const char* function, const string& name, const string& message) throw() :
      BaseException(file, line, function, name, message)
    {
    }

    DEFINE_EXCEPTION_OP(DivisionByZero)
    DEF_EXCEPTION(DivisionByZero, "a division by zero was requested")

    DEFINE_EXCEPTION_OP(InvalidRange)
    DEF_EXCEPTION(InvalidRange, "the range of the operation was invalid")

    DEFINE_EXCEPTION_OP(OutOfRange)
    DEF_EXCEPTION(OutOfRange, "the argument was not in range")

    DEFINE_EXCEPTION_OP(NullPointer)
    DEF_EXCEPTION(NullPointer, "a null pointer was specified")

    DEFINE_EXCEPTION_OP(InvalidIterator)
    DEF_EXCEPTION(InvalidIterator, "the iterator is invalid - probably it is not bound to a container")

    DEFINE_EXCEPTION_OP(IncompatibleIterators)
    DEF_EXCEPTION(IncompatibleIterators, "the iterator could not be assigned because it is bound to a different container")

    DEFINE_EXCEPTION_OP(NotImplemented)
    DEF_EXCEPTION(NotImplemented, "this method has not been implemented yet. Feel free to complain about it!")

    DEFINE_EXCEPTION_OP(IllegalSelfOperation)
    DEF_EXCEPTION(IllegalSelfOperation, "cannot perform operation on the same object")

    DEFINE_EXCEPTION_OP(IllegalTreeOperation)
    DEF_EXCEPTION(IllegalTreeOperation, "an illegal tree operation was requested")

    DEFINE_EXCEPTION_OP(BufferOverflow)
    DEF_EXCEPTION(BufferOverflow, "the maximum buffersize has been reached")

    DEFINE_EXCEPTION_OP(OutOfGrid)
    DEF_EXCEPTION(OutOfGrid, "a point was outside a grid")

  } // namespace Exception

  std::ostream& operator<<(std::ostream& os, const Exception::BaseException& e)
  {
    os << e.getName() << " @ " << e.getFile() << ":" << e.getFunction() << " (Line " << e.getLine() << "): " << e.what();

    return os;
  }

} // namespace OPENMS
