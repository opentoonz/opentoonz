#define _XOPEN_SOURCE 700 /* isascii() */
#undef _GNU_SOURCE /* int strerror_r() on glibc */
#include <cerrno>
#include <cstring> /* memset */
#include <vector>
#include <stdexcept>  // std::domain_error(-)
#include <locale>
#include "igs_resource_msg_from_err.h"


#if defined UNICODE
/*------ ワイド文字文字列 --> マルチバイト文字列 ------*/
static void wcs_to_mbs(const std::wstring &wcs, std::string &mbs) {
  size_t length = 0;
  {
    const wchar_t *src_ptr = wcs.c_str();
    mbstate_t ss;
    ::memset(&ss, 0, sizeof(ss));
    length = ::wcsrtombs(NULL, &src_ptr, 0, &ss);
    if (length <= 0) {
      return;
    } /* 文字がないなら何もしない */
    ++length;
  }
  // std::vector<char> dst(length);
  mbs.resize(length);
  {
    const wchar_t *src_ptr = wcs.c_str();
    mbstate_t ss;
    ::memset(&ss, 0, sizeof(ss));
    // length = ::wcsrtombs(&dst.at(0) ,&src_ptr ,length ,&ss);
    length =
        ::wcsrtombs(const_cast<char *>(mbs.c_str()), &src_ptr, length, &ss);
    if (length <= 0) {
      throw std::domain_error("wcstombs(-) got bad wide character");
    }
  }
  // mbs = std::string(dst.begin() ,dst.end()-1);/* 終端以外を */
  mbs.erase(mbs.end() - 1); /* 終端文字を消す */
}
#endif
/*------ UNICODE宣言ならワイド文字文字列をマルチバイト文字列に変換 ------*/
const static std::string mbs_from_ts(
    const std::basic_string<TCHAR> &ts) {
#if defined UNICODE
  std::string mbs;
  wcs_to_mbs(ts, mbs);
  return mbs;
#else
  /* MBCSの場合のsize()は文字数ではなくchar(byte)数,2bytes文字は2 */
  return ts;
#endif
}

/*------ エラーメッセージ表示の元関数、直接呼び出すことはしない ------*/
#include <cerrno>   // errno
#include <cstring>  // strerror_r()
#include <sstream>  // std::istringstream
#include "igs_resource_msg_from_err.h"

const std::string igs::resource::msg_from_err_(
    const std::basic_string<TCHAR> &tit, const int erno,
    const std::string &file, const std::string &line,
    const std::string &pretty_function, const std::string &comp_type,
    const std::string &gnuc, const std::string &gnuc_minor,
    const std::string &gnuc_patchlevel, const std::string &gnuc_rh_release,
    const std::string &date, const std::string &time) {
  std::string errmsg;
  errmsg += '\"';

  /* フルパスで入ってきた場合ファイル名だけにする */
  std::string::size_type index = file.find_last_of("/\\");
  if (std::basic_string<TCHAR>::npos != index) {
    errmsg += file.substr(index + 1);
  } else {
    errmsg += file;
  }

  errmsg += ':';
  errmsg += line;
  errmsg += ':';
  errmsg += comp_type;
  errmsg += ':';
  errmsg += gnuc;
  errmsg += '.';
  errmsg += gnuc_minor;
  errmsg += '.';
  errmsg += gnuc_patchlevel;
  errmsg += '-';
  errmsg += gnuc_rh_release;
  {
    std::istringstream ist(date);
    std::string month, day, year;
    ist >> month;
    ist >> day;
    ist >> year;
    errmsg += ':';
    errmsg += year;
    errmsg += ':';
    errmsg += month;
    errmsg += ':';
    errmsg += day;
  }
  errmsg += ':';
  errmsg += time;
  errmsg += '\"';
  errmsg += ' ';
  errmsg += '\"';
  errmsg += pretty_function;
  errmsg += '\"';
  errmsg += ' ';
  errmsg += '\"';
  if (0 < tit.size()) {
    errmsg += mbs_from_ts(tit);
  }
  if (0 != erno) {
    errmsg += ':';

#if defined __HP_aCC
    /*
HP-UX(v11.23)では、strerror_r()をサポートしない。
注意::strerror()はThread SafeではなくMulti Threadでは正常動作しない
*/
    errmsg += ::strerror(erno);
#elif !defined(__APPLE__)
    /*
http://japanese-linux-man-pages.coding-school.com/man/X_strerror_r-3
より、POSIX.1.2002で規定されたXSI準拠のバージョンのstrerror_r()
*/
    char buff[4096];
    const int ret = ::strerror_r(erno, buff, sizeof(buff));
    if (0 == ret) {
      errmsg += buff;
    } else if (-1 == ret) {
      switch(errno) {
      case EINVAL:
        errmsg +=
            "strerror_r() gets Error : The value of errnum is not a "
            "valid error number.";
        /* errnum の値が有効なエラー番号ではない */
        break;
      case ERANGE:
        errmsg +=
            "strerror_r() gets Error : Insufficient storage was "
            "supplied via strerrbuf and buflen  to contain the "
            "generated message string.";
        /* エラーコードを説明する文字列のために、
充分な領域が確保できな かった */
        break;
      default:
        errmsg += "strerror_r() gets Error and Returns bad errno";
        break;
      }
    } else {
      errmsg += "strerror_r() returns bad value";
    }
#else
    char buff[4096];
    int ret = ::strerror_r(erno, buff, sizeof(buff));
    if (!ret) {
      errmsg += buff;
    }
#endif
  }
  errmsg += '\"';
  return errmsg;
}
