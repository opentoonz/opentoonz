#pragma once

#ifndef TVER_INCLUDED
#define TVER_INCLUDED

#include <string>

namespace TVER {

class ToonzVersion {
public:
  std::string getAppName(void);
  float getAppVersion(void);
  float getAppRevision(void);
  std::string getAppNote(void);
  bool hasAppNote(void);
  std::string getAppVersionString(void);
  std::string getAppRevisionString(void);
  std::string getAppVersionInfo(std::string msg);

private:
  const char *applicationName     = "OpenToonz";
  const float applicationVersion  = 1.7f;
  const float applicationRevision = 1;
  const char *applicationNote     = "";
};
}  // namespace TVER

#endif  // TVER_INCLUDED
