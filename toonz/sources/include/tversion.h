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
	}

	char *TVER::getAppName( void ) {
	   return std::string(applicationName);
	}
	float TVER::getAppVersion( void ) {
	   return applicationVersion;
	}
	float TVER::getAppRevision( void ) {
	   return applicationRevision;
	}
	std::string TVER::getAppVersionString( void ){
	   return std:to_string(applicationVersion);
	}
	std::string TVER::getAppRevisionString( void ){
	   return std:to_string(applicationRevision);
	}
	std::string TVER::getAppVersionInfo( std::string msg ){
	   return std::string(applicationName) + " " + msg + " v" + std:to_string(applicationVersion) + "." + std:to_string(applicationRevision);
	}


}  // namespace TVER

#endif  // TVER_INCLUDED
