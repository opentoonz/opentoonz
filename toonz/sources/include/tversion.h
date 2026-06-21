#pragma once

#ifndef TVER_INCLUDED
#define TVER_INCLUDED

#include "tbuildinfo.h"

#include <cstdlib>
#include <string>

namespace TVER {

class ToonzVersion {
public:
  std::string getAppName(void);
  float getAppVersion(void);
  float getAppRevision(void);
  std::string getAppNote(void);
  std::string getSystemVarPrefix(void);
  bool hasAppNote(void);
  bool hasAppRevision(void);
  std::string getAppVersionString(void);
  std::string getAppRevisionString(void);
  std::string getAppFullVersionString(void);
  std::string getAppVersionInfo(std::string msg);

private:
  const char *applicationName     = "OpenToonz";
  const char *applicationVersion  = OPENTOONZ_APP_VERSION;
  const float applicationRevision = 0;
  const char *applicationNote     = "";
  const char *systemVarPrefix     = "TOONZ";
};

std::string ToonzVersion::getAppName(void) {
  std::string appname = applicationName;
  return appname;
}
float ToonzVersion::getAppVersion(void) {
  return static_cast<float>(std::atof(getAppVersionString().c_str()));
}
float ToonzVersion::getAppRevision(void) {
  return static_cast<float>(std::atof(getAppRevisionString().c_str()));
}
std::string ToonzVersion::getAppNote(void) {
  std::string appnote = applicationNote;
  return appnote;
}
std::string ToonzVersion::getSystemVarPrefix(void) {
  std::string prefix = systemVarPrefix;
  return prefix;
}
bool ToonzVersion::hasAppNote(void) { return *applicationNote != 0; }
bool ToonzVersion::hasAppRevision(void) {
  return !getAppRevisionString().empty();
}
std::string ToonzVersion::getAppVersionString(void) {
  std::string fullVersion = applicationVersion;
  std::string::size_type firstDot = fullVersion.find('.');
  if (firstDot == std::string::npos) return fullVersion;

  std::string::size_type secondDot = fullVersion.find('.', firstDot + 1);
  if (secondDot == std::string::npos) return fullVersion;

  return fullVersion.substr(0, secondDot);
}
std::string ToonzVersion::getAppRevisionString(void) {
  std::string fullVersion = applicationVersion;
  std::string::size_type firstDot = fullVersion.find('.');
  if (firstDot == std::string::npos) return "";

  std::string::size_type secondDot = fullVersion.find('.', firstDot + 1);
  if (secondDot == std::string::npos || secondDot + 1 == fullVersion.size())
    return "";

  return fullVersion.substr(secondDot + 1);
}
std::string ToonzVersion::getAppFullVersionString(void) {
  return std::string(applicationVersion);
}
std::string ToonzVersion::getAppVersionInfo(std::string msg) {
  std::string appinfo = std::string(applicationName);
  appinfo += " " + msg + " v";
  appinfo += getAppFullVersionString();
  if (hasAppNote()) appinfo += " " + std::string(applicationNote);
  return appinfo;
}

}  // namespace TVER

#endif  // TVER_INCLUDED
