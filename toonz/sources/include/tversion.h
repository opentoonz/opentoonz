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
		std::string appname = ToonzVersion.applicationName;
		return appname;
	}
	float ToonzVersion::getAppVersion( void ) {
		float appver = ToonzVersion.applicationVersion;
		return appver;
	}
	float ToonzVersion::getAppRevision( void ) {
		float apprev = ToonzVersion.applicationRevision;
		return apprev;
	}
	std::string ToonzVersion::getAppVersionString( void ){
		std::string appver = std::to_string(ToonzVersion.applicationVersion);
		return appver;
	}
	std::string ToonzVersion::getAppRevisionString( void ){
		std::string apprev = std::to_string(ToonzVersion.applicationRevision);
		return apprev;
	}
	std::string ToonzVersion::getAppVersionInfo( std::string msg ){
		std::string appinfo = std::string(ToonzVersion.applicationName);
		appinfo += " " + msg + " v";
		appinfo += std:to_string(ToonzVersion.applicationVersion);
		appinfo += "." + std:to_string(ToonzVersion.applicationRevision;
		return appinfo;
	}


}  // namespace TVER

#endif  // TVER_INCLUDED
