

#ifndef LICENSE_CHECKER_H
#define LICENSE_CHECKER_H

#include <QtGlobal>

//#if QT_VERSION >= 0x050000
#include <QNetworkAccessManager>
//#else
//#include <QHttp>
//#endif
#include <QUrl>

#include "tcommon.h"

#undef DVAPI
#undef DVVAR
#ifdef TOONZQT_EXPORTS
#define DVAPI DV_EXPORT_API
#define DVVAR DV_EXPORT_VAR
#else
#define DVAPI DV_IMPORT_API
#define DVVAR DV_IMPORT_VAR
#endif

/* moc tool が preprocessor directive を無視するので QT_VERSION で分岐するのは諦める */
class DVAPI LicenseChecker
	//#if QT_VERSION < 0x050000
	//: public QHttp
	//#else
	: public QObject
//#endif
{
	Q_OBJECT

  public:
	enum LicenseMode { TAB, TOONZ };

  private:
	bool m_httpRequestAborted;
	int m_httpGetId;

	LicenseMode m_licenseMode;

	bool m_isValid;

  public:
	LicenseChecker(const QString &requestUrl, LicenseMode licenseMode, std::string license,
				   std::string applicationName, const QString &version);

	bool isLicenseValid() const { return m_isValid; }

  private:
	QString buildRequest(const QString &requestUrl, std::string license,
						 std::string applicationName, const QString &version);

  protected slots:
	//#if QT_VERSION >= 0x050000
	void httpRequestFinished(QNetworkReply *);
	//#else
	//  void httpRequestFinished(int requestId, bool error);
	//  void readyReadExec(const QHttpResponseHeader &head){}
	//  void readResponseHeader(const QHttpResponseHeader &responseHeader);
	//#endif
	void httpRequestStarted(int requestId) {}
	void slotAuthenticationRequired(const QString &hostName, quint16,
									QAuthenticator *authenticator);
	void httpStateChanged(int state);
};

#endif // LICENSE_CHECKER_H
