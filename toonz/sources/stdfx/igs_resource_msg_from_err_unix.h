#pragma once

#ifndef igs_resource_msg_from_err_h
#define igs_resource_msg_from_err_h

#include <string>
#include "igs_os_type.h" /* TCHAR,TEXT(),... */

#define igs_tostr(n) igs_tostr_(n) /* __LINE__ -->  56   */
#define igs_tostr_(n) #n		   /* 56       --> "56" */

#ifndef IGS_RESOURCE_LOG_EXPORT
#define IGS_RESOURCE_LOG_EXPORT
#endif

/*------ 文字コード変換 --------------------------------------------*/
namespace igs
{
namespace resource
{
/*--- localeを日本に設定し日本語を扱うことを指示 ---*/
IGS_RESOURCE_LOG_EXPORT void locale_to_jp(void);
/*--- マルチバイト文字列 --> ワイド文字文字列 ---*/
IGS_RESOURCE_LOG_EXPORT void mbs_to_wcs(
	const std::string &mbs, std::wstring &wcs);
/*--- ワイド文字文字列 --> マルチバイト文字列 ---*/
IGS_RESOURCE_LOG_EXPORT void wcs_to_mbs(
	const std::wstring &wcs, std::string &mbs);
/*
	2012-08-22 呼び側の名前も変更
	from_mbcs(-) --> ts_from_mbs(-)
	mbcs_from(-) --> mbs_from_ts(-)
*/
/*--- UNICODE宣言ならマルチバイト文字列をワイド文字文字列に変換 ---*/
const std::basic_string<TCHAR> ts_from_mbs(const std::string &mbs);
/*--- UNICODE宣言ならワイド文字文字列をマルチバイト文字列に変換 ---*/
const std::string mbs_from_ts(const std::basic_string<TCHAR> &ts);
/*--- cp932を含む文字列をutf-8に変換(mbcsのみ) ---*/
const std::string utf8_from_cp932_mb(const std::string &text);
/*--- utf-8を含む文字列をcp932に変換(mbcsのみ) ---*/
const std::string cp932_from_utf8_mb(const std::string &text);
}
}

/*------ エラーメッセージ表示 --------------------------------------*/
namespace igs
{
namespace resource
{
const std::string msg_from_err_(/* 直によんではいけない */
								const std::basic_string<TCHAR> &tit, const int erno, const std::string &file, const std::string &line, const std::string &pretty_function, const std::string &comp_type, const std::string &gnuc, const std::string &gnuc_minor, const std::string &gnuc_patchlevel, const std::string &gnuc_rh_release, const std::string &date, const std::string &time);
}
}
/*--- errno値からエラーメッセージを得る ---*/
#define igs_resource_msg_from_err(tit, erno) igs::resource::msg_from_err_(tit, erno, __FILE__, igs_tostr(__LINE__), __PRETTY_FUNCTION__, igs_tostr_(__GNUC__), igs_tostr(__GNUC__), igs_tostr(__GNUC_MINOR__), igs_tostr(__GNUC_PATCHLEVEL__), igs_tostr(__GNUC_RH_RELEASE__), __DATE__, __TIME__)
/*--- エラーメッセージを得る ---*/
#define igs_resource_msg_from_er(tit) igs::resource::msg_from_err_(tit, 0, __FILE__, igs_tostr(__LINE__), __PRETTY_FUNCTION__, igs_tostr_(__GNUC__), igs_tostr(__GNUC__), igs_tostr(__GNUC_MINOR__), igs_tostr(__GNUC_PATCHLEVEL__), igs_tostr(__GNUC_RH_RELEASE__), __DATE__, __TIME__)
#endif /* !igs_resource_msg_from_err_h */
