#include "igs_resource_msg_from_err.h"
#include <locale>
#include <stdexcept>


#if defined UNICODE
/*------ ワイド文字文字列 --> マルチバイト文字列 ------*/
static void wcs_to_mbs(const std::wstring &wcs, std::string &mbs,
                               const UINT code_page) {
  /* 第４引数で -1 指定により終端文字を含む大きさを返す */
  int length = ::WideCharToMultiByte(code_page, 0, wcs.c_str(), -1, 0, 0, 0, 0);
  if (length <= 1) {
    return;
  } /* 終端以外の文字がないなら何もしない */

  /***	std::vector<char> buf(length);
  length = ::WideCharToMultiByte(
          code_page ,0 ,wcs.c_str() ,-1
          ,&buf.at(0) ,static_cast<int>(buf.size()) ,0 ,NULL
  );***/
  mbs.resize(length);
  length = ::WideCharToMultiByte(code_page, 0, wcs.c_str(), -1,
                                 const_cast<LPSTR>(mbs.data()),
                                 static_cast<int>(mbs.size()), 0, NULL);
  if (0 == length) {
    switch (::GetLastError()) {
    case ERROR_INSUFFICIENT_BUFFER:
      throw std::domain_error("WideCharToMultiByte():insufficient buffer");
    case ERROR_INVALID_FLAGS:
      throw std::domain_error("WideCharToMultiByte():invalid flags");
    case ERROR_INVALID_PARAMETER:
      throw std::domain_error("WideCharToMultiByte():invalid parameter");
    }
  }
  // mbs = std::string(buf.begin() ,buf.end()-1); /* 終端以外を */
  mbs.erase(mbs.end() - 1); /* 終端文字を消す。end()は終端より先位置 */
}
#endif
/*------ UNICODE宣言ならワイド文字文字列をマルチバイト文字列に変換 ------*/
static const std::string mbs_from_ts(const std::basic_string<TCHAR> &ts) {
#if defined UNICODE
  std::string mbs;
  wcs_to_mbs(ts, mbs, CP_THREAD_ACP);
  return mbs;
#else
  /* MBCSの場合のsize()は文字数ではなくchar(byte)数,2bytes文字は2 */
  return ts;
#endif
}

/*------ エラーメッセージ表示の元関数、直接呼び出すことはしない ------*/
#include <sstream>
const std::string igs::resource::msg_from_err_(
    const std::basic_string<TCHAR> &tit, const DWORD error_message_id,
    const std::basic_string<TCHAR> &file, const std::basic_string<TCHAR> &line,
    const std::basic_string<TCHAR> &funcsig,
    const std::basic_string<TCHAR> &comp_type,
    const std::basic_string<TCHAR> &msc_full_ver,
    const std::basic_string<TCHAR> &date,
    const std::basic_string<TCHAR> &time) {
  /*
汎用データ型  ワイド文字(UNICODE)(※1)	マルチバイト文字(_MBCS)(※2)
TCHAR	      wchar_t			char
LPTSTR	      wchar_t *			char *
LPCTSTR	      const wchar_t *		const char *
※1  1文字を16ビットのワイド文字として表すUnicode を使う方法
  すべての文字が 16 ビットに固定されます。
  マルチバイト文字に比べ、メモリ効率は低下しますが処理速度は向上します
※2  1文字を複数のバイトで表すマルチバイト文字
  MBCS(Multibyte Character Set) と呼ばれる文字集合を使う方法
  可変長だが、事実上、サポートされているのは 2 バイト文字までなので、
  マルチバイト文字の 1 文字は 1 バイトまたは 2 バイトとなります。
  Windows 2000 以降、Windows は内部で Unicode を使用しているため、
  マルチバイト文字を使用すると内部で文字列の変換が発生するため
  オーバーヘッドが発生します。
  UNICODEも_MBCSも未定義のときはこちらになる。
*/
  std::basic_string<TCHAR> errmsg;
  errmsg += TEXT('\"');

  /* makefile-vc2008mdAMD64等でコンパイルすると
  フルパスで入ってくるのでファイル名だけにする
  */
  std::basic_string<TCHAR>::size_type index = file.find_last_of(TEXT("/\\"));
  if (std::basic_string<TCHAR>::npos != index) {
    errmsg += file.substr(index + 1);
  } else {
    errmsg += file;
  }

  errmsg += TEXT(':');
  errmsg += line;
  errmsg += TEXT(':');
  errmsg += comp_type;
  errmsg += TEXT(":");
  errmsg += msc_full_ver;
  {
    std::basic_istringstream<TCHAR> ist(date);
    std::basic_string<TCHAR> month, day, year;
    ist >> month;
    ist >> day;
    ist >> year;
    errmsg += TEXT(':');
    errmsg += year;
    errmsg += TEXT(':');
    errmsg += month;
    errmsg += TEXT(':');
    errmsg += day;
  }
  errmsg += TEXT(':');
  errmsg += time;
  errmsg += TEXT('\"');
  errmsg += TEXT(' ');
  errmsg += TEXT('\"');
  errmsg += funcsig;
  errmsg += TEXT('\"');
  errmsg += TEXT(' ');
  errmsg += TEXT('\"');
  if (0 < tit.size()) {
    errmsg += tit;
  }
  if (NO_ERROR != error_message_id) {
    errmsg += TEXT(':');
    LPTSTR lpMsgBuf = 0;
    if (0 < ::FormatMessage(
                FORMAT_MESSAGE_ALLOCATE_BUFFER | FORMAT_MESSAGE_FROM_SYSTEM |
                    FORMAT_MESSAGE_IGNORE_INSERTS,
                NULL, error_message_id,
                MAKELANGID(LANG_NEUTRAL, SUBLANG_DEFAULT) /* 既定言語 */
                ,
                reinterpret_cast<LPTSTR>(&lpMsgBuf), 0,
                NULL)) { /* --- 成功 --- */
      errmsg += lpMsgBuf;
      ::LocalFree(lpMsgBuf);
      std::string::size_type index = errmsg.find_first_of(TEXT("\r\n"));
      if (std::string::npos != index) {
        errmsg.erase(index);
      }
    } else { /* エラー */
      errmsg += TEXT("FormatMessage() can not get (error)message");
    }
  }
  errmsg += TEXT('\"');

  /* MBCSで返す */
  return mbs_from_ts(errmsg);
}
