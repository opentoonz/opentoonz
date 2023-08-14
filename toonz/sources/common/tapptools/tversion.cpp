#include "tversion.h"
#include <string>

namespace TVER {

std::string ToonzVersion::getAppName(void) {
  std::string appname = applicationName;
  return appname;
}
float ToonzVersion::getAppVersion(void) {
  float appver = applicationVersion;
  return appver;
}
float ToonzVersion::getAppRevision(void) {
  float apprev = applicationRevision;
  return apprev;
}
std::string ToonzVersion::getAppNote(void) {
  std::string appnote = applicationNote;
  return appnote;
}
bool ToonzVersion::hasAppNote(void) { return *applicationNote != 0; }
std::string ToonzVersion::getAppVersionString(void) {
  char buffer[50];
  snprintf(buffer, sizeof(buffer), "%.1f", applicationVersion);
  std::string appver = std::string(buffer);
  return appver;
}
std::string ToonzVersion::getAppRevisionString(void) {
  char buffer[50];
  snprintf(buffer, sizeof(buffer), "%g", applicationRevision);
  std::string apprev = std::string(buffer);
  return apprev;
}
std::string ToonzVersion::getAppVersionInfo(std::string msg) {
  std::string appinfo = std::string(applicationName);
  appinfo += " " + msg + " v";
  appinfo += getAppVersionString();
  appinfo += "." + getAppRevisionString();
  if (hasAppNote()) appinfo += " " + std::string(applicationNote);
  return appinfo;
}

}
