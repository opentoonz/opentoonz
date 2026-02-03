

#include "tstreamexception.h"
#include "tfilepath.h"
#include "tconvert.h"
#include "tstream.h"

namespace {

std::wstring getLocation(TIStream &is) {
  return L"File: " + is.getFilePath().getWideString() + L":" +
         std::to_wstring(is.getLine());
}

std::wstring message(TIStream &is, std::wstring msg) {
  return getLocation(is) + L"\n" + msg;
}

std::wstring message(TIStream &is, std::string msg) {
  return message(is, ::to_wstring(msg));
}

}  // namespace

TIStreamException::TIStreamException(TIStream &is)
    : TException(message(is, L"unknown exception")) {}

TIStreamException::TIStreamException(TIStream &is, const TException &e)
    : TException(message(is, e.getMessage())) {}

TIStreamException::TIStreamException(TIStream &is, std::wstring msg)
    : TException(message(is, msg)) {}

TIStreamException::TIStreamException(TIStream &is, std::string msg)
    : TException(message(is, msg)) {}

TIStreamException::~TIStreamException() {}
