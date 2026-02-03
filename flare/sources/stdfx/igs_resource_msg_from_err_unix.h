#pragma once

#ifndef igs_resource_msg_from_err_h
#define igs_resource_msg_from_err_h

#include <string>
#include "igs_os_type.h" /* TCHAR,TEXT(),... */

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
                                const int erno, const std::string &file,
                                const std::string &line,
                                const std::string &pretty_function,
                                const std::string &comp_type,
                                const std::string &gnuc,
                                const std::string &gnuc_minor,
                                const std::string &gnuc_patchlevel,
                                const std::string &gnuc_rh_release,
                                const std::string &date,
                                const std::string &time);
}
}
/*--- errno値からエラーメッセージを得る ---*/
#define igs_resource_msg_from_err(tit, erno)                                   \
  igs::resource::msg_from_err_(                                                \
      tit, erno, __FILE__, igs_tostr(__LINE__), __PRETTY_FUNCTION__,           \
      igs_tostr_(__GNUC__), igs_tostr(__GNUC__), igs_tostr(__GNUC_MINOR__),    \
      igs_tostr(__GNUC_PATCHLEVEL__), igs_tostr(__GNUC_RH_RELEASE__),          \
      __DATE__, __TIME__)
/*--- エラーメッセージを得る ---*/
#define igs_resource_msg_from_er(tit)                                          \
  igs::resource::msg_from_err_(                                                \
      tit, 0, __FILE__, igs_tostr(__LINE__), __PRETTY_FUNCTION__,              \
      igs_tostr_(__GNUC__), igs_tostr(__GNUC__), igs_tostr(__GNUC_MINOR__),    \
      igs_tostr(__GNUC_PATCHLEVEL__), igs_tostr(__GNUC_RH_RELEASE__),          \
      __DATE__, __TIME__)
#endif /* !igs_resource_msg_from_err_h */
