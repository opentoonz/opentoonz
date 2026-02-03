#pragma once

#ifndef igs_resource_msg_from_err_h
#define igs_resource_msg_from_err_h

#include <string>
#include "igs_os_type.h" /* windows.h... */

#define igs_tostr(n) igs_tostr_(n) /* __LINE__ -->  56   */
#define igs_tostr_(n) #n           /* 56       --> "56" */

#ifndef IGS_RESOURCE_LOG_EXPORT
#define IGS_RESOURCE_LOG_EXPORT
#endif

/*------ エラーメッセージ表示 --------------------------------------*/
namespace igs {
namespace resource {
const std::string msg_from_err_(/* 直によんではいけない */
                                const std::basic_string<TCHAR> &tit,
                                const DWORD error_message_id,
                                const std::basic_string<TCHAR> &file,
                                const std::basic_string<TCHAR> &line,
                                const std::basic_string<TCHAR> &funcsig,
                                const std::basic_string<TCHAR> &comp_type,
                                const std::basic_string<TCHAR> &msc_full_ver,
                                const std::basic_string<TCHAR> &date,
                                const std::basic_string<TCHAR> &time);
}
}
/*--- ::GetLastError()値からエラーメッセージを得る ---*/
#ifdef _MSC_VER
#define igs_resource_msg_from_err(tit, error_message_id)                       \
  igs::resource::msg_from_err_(                                                \
      tit, error_message_id, TEXT(__FILE__), TEXT(igs_tostr(__LINE__)),        \
      TEXT(__FUNCSIG__), TEXT(igs_tostr_(_MSC_VER)),                           \
      TEXT(igs_tostr(_MSC_FULL_VER)), TEXT(__DATE__), TEXT(__TIME__))
/*--- エラーメッセージを得る ---*/
#define igs_resource_msg_from_er(tit)                                          \
  igs::resource::msg_from_err_(                                                \
      tit, NO_ERROR, TEXT(__FILE__), TEXT(igs_tostr(__LINE__)),                \
      TEXT(__FUNCSIG__), TEXT(igs_tostr_(_MSC_VER)),                           \
      TEXT(igs_tostr(_MSC_FULL_VER)), TEXT(__DATE__), TEXT(__TIME__))
#else
#define igs_resource_msg_from_err(tit, error_message_id)                       \
  igs::resource::msg_from_err_(                                                \
      tit, error_message_id, TEXT(__FILE__), TEXT(igs_tostr(__LINE__)),        \
      TEXT(__PRETTY_FUNCTION__), TEXT(__VERSION__), TEXT(__VERSION__),         \
      TEXT(__DATE__), TEXT(__TIME__))
/*--- エラーメッセージを得る ---*/
#define igs_resource_msg_from_er(tit)                                          \
  igs::resource::msg_from_err_(                                                \
      tit, NO_ERROR, TEXT(__FILE__), TEXT(igs_tostr(__LINE__)),                \
      TEXT(__PRETTY_FUNCTION__), TEXT(__VERSION__), TEXT(__VERSION__),         \
      TEXT(__DATE__), TEXT(__TIME__))
#endif

#endif /* !igs_resource_msg_from_err_h */
