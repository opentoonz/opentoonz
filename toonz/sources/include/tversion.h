#pragma once

#ifndef TVER_INCLUDED
#define TVER_INCLUDED

namespace TVER {

	class ToonzVersion {
		public:
			std::string getAppName( void );
			float getAppVersion( void );
			float getAppRevision( void );
			std::string getAppVersionString( void );
			std::string getAppRevisionString( void );
			std::string getAppVersionInfo( std::string msg );

		private:
			const char *applicationName     = "OpenToonz";
			const float applicationVersion  = 1.1;
			const float applicationRevision = 2;
	};

	std::string ToonzVersion::getAppName( void ) {
	   return std::string(ToonzVersion.applicationName);
	}
	float ToonzVersion::getAppVersion( void ) {
	   return ToonzVersion.applicationVersion;
	}
	float ToonzVersion::getAppRevision( void ) {
	   return ToonzVersion.applicationRevision;
	}
	std::string ToonzVersion::getAppVersionString( void ){
	   return std:to_string(ToonzVersion.applicationVersion);
	}
	std::string ToonzVersion::getAppRevisionString( void ){
	   return std:to_string(ToonzVersion.applicationRevision);
	}
	std::string ToonzVersion::getAppVersionInfo( std::string msg ){
	   return std::string(ToonzVersion.applicationName) + " " + msg + " v" + std:to_string(ToonzVersion.applicationVersion) + "." + std:to_string(ToonzVersion.applicationRevision);
	}


}  // namespace TVER

#endif  // TVER_INCLUDED
