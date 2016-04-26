

#include "toonzqt/licensechecker.h"
//#include "licensecontroller.h"
#include <assert.h>

#include "tsystem.h"
#include "tfilepath_io.h"
#include "tenv.h"

#include <QUrl>
#include <QHostInfo>
#include <QList>
#if QT_VERSION >= 0x050000
#include <QNetworkReply>
#include <QNetworkInterface>
#include <QCoreApplication>
#include <QEventLoop>
#endif

namespace
{
QString getMacAddressFromFile()
{
	TFilePath fp(TEnv::getStuffDir() + "config" + "computercode.txt");
	if (TFileStatus(fp).doesExist() == false)
		return QString();
	Tifstream is(fp);
	char buffer[1024];
	is.getline(buffer, sizeof buffer);
	std::string macAddr = buffer;
	return QString::fromStdString(macAddr);
}

} // namespace

//=============================================================================
// LicenseChecker
//-----------------------------------------------------------------------------

LicenseChecker::LicenseChecker(const QString &requestUrl, LicenseMode licenseMode,
							   std::string license, std::string applicationName,
							   const QString &version)
#if QT_VERSION < 0x050000
	: QHttp(),
#else
	:
#endif
	  m_licenseMode(licenseMode), m_isValid(true)
{
	QString requestToServer = buildRequest(requestUrl, license, applicationName, version);

	QUrl url(requestToServer);

#if QT_VERSION >= 0x050000

	QString urlTemp = requestToServer;

	QStringList urlList = urlTemp.split('?');
	if (urlList.count() <= 1) {
		abort();
		return;
	}

	QStringList paramList = urlList.at(1).split('&');

	m_httpRequestAborted = false;

	QString param;
	param = QString("?") + paramList.at(0) + QString("&") + paramList.at(1) + QString("&") +
			paramList.at(2) + QString("&") + paramList.at(3) + QString("&") + paramList.at(4);

	QNetworkAccessManager manager;
	QNetworkReply *reply = manager.get(QNetworkRequest(url.path() + param));

	QEventLoop loop;

	connect(&manager, SIGNAL(requestFinished(int, bool)), this, SLOT(httpRequestFinished(reply)));
	connect(reply, SIGNAL(requestStarted(int)), &loop, SLOT(httpRequestStarted(int)));
	connect(reply, SIGNAL(authenticationRequired(const QString &, quint16, QAuthenticator *)),
			&loop, SLOT(slotAuthenticationRequired(const QString &, quint16, QAuthenticator *)));
	connect(reply, SIGNAL(stateChanged(int)), &loop, SLOT(httpStateChanged(int)));

	loop.exec();

#else
	connect(this, SIGNAL(requestFinished(int, bool)), this, SLOT(httpRequestFinished(int, bool)));
	connect(this, SIGNAL(requestStarted(int)), this, SLOT(httpRequestStarted(int)));
	connect(this, SIGNAL(responseHeaderReceived(const QHttpResponseHeader &)), this,
			SLOT(readResponseHeader(const QHttpResponseHeader &)));
	connect(this, SIGNAL(authenticationRequired(const QString &, quint16, QAuthenticator *)), this,
			SLOT(slotAuthenticationRequired(const QString &, quint16, QAuthenticator *)));
	connect(this, SIGNAL(stateChanged(int)), this, SLOT(httpStateChanged(int)));
	QHttp::ConnectionMode mode =
		url.scheme().toLower() == "https" ? QHttp::ConnectionModeHttps : QHttp::ConnectionModeHttp;

	setHost(url.host(), mode, url.port() == -1 ? 0 : url.port());
	QString urlTemp = requestToServer;

	QStringList urlList = urlTemp.split('?');
	if (urlList.count() <= 1) {
		abort();
		return;
	}

	QStringList paramList = urlList.at(1).split('&');
	if (!url.userName().isEmpty())
		setUser(url.userName(), url.password());

	m_httpRequestAborted = false;

	QString param;
	param = QString("?") + paramList.at(0) + QString("&") + paramList.at(1) + QString("&") +
			paramList.at(2) + QString("&") + paramList.at(3) + QString("&") + paramList.at(4);
	m_httpGetId = get(url.path() + param); //, file);
#endif
}

//-----------------------------------------------------------------------------

#if QT_VERSION < 0x050000
void LicenseChecker::readResponseHeader(const QHttpResponseHeader &responseHeader)
{
	int err = responseHeader.statusCode();
	if (err != 200 && err != 502) {
		m_httpRequestAborted = true;
		abort();
	}
}
#endif

//-----------------------------------------------------------------------------

void LicenseChecker::httpStateChanged(int status)
{
	std::string stateStr;
	switch (status) {
	case 1:
		stateStr = "A host name lookup is in progress...";
		break;
	case 2:
		stateStr = "Connecting...";
		break;
	case 3:
		stateStr = "Sending informations...";
		break;
	case 4:
		stateStr = "Reading informations...";
		break;
	case 5:
		stateStr = "Connected.";
		break;
	case 6:
		stateStr = "The connection is closing down, but is not yet closed. (The state will be "
				   "Unconnected when the connection is closed.)";
	default:
		stateStr = "There is no connection to the host.";
	}
#if QT_VERSION < 0x050000
	status = state();
	qDebug("Status: %d : %s", status, stateStr.c_str());
#endif
}

//-----------------------------------------------------------------------------

#if QT_VERSION < 0x050000
void LicenseChecker::httpRequestFinished(int requestId, bool error)
#else
void LicenseChecker::httpRequestFinished(QNetworkReply *reply)
#endif
{
	QByteArray arr;
	std::string responseString;
	QString qstr;
#if QT_VERSION < 0x050000
	if (requestId != m_httpGetId) {
		return;
	}
	if (error || m_httpRequestAborted) {
		abort();
		return;
	}

	arr = readAll();
#else
	if (reply->error() != QNetworkReply::NoError)
		return;
	arr = reply->readAll();
#endif
	qstr = QString(arr);

	int startIndex = qstr.indexOf("startblock");
	int endIndex = qstr.indexOf("endblock");

	if ((endIndex - startIndex - 9) <= 0) {
		abort();
		return;
	}

	responseString = qstr.toStdString();
	responseString = responseString.substr(startIndex + 12, endIndex - startIndex - 13);

	if (responseString == "YES")
		m_isValid = false;
}

//-----------------------------------------------------------------------------

void LicenseChecker::slotAuthenticationRequired(const QString &hostName, quint16,
												QAuthenticator *authenticator)
{
	assert(false);
}

//-----------------------------------------------------------------------------

QString LicenseChecker::buildRequest(const QString &requestUrl, std::string license,
									 std::string applicationName, const QString &version)
{
	QString licenseCode = QString::fromStdString(license);

	QString MacAddress = getMacAddressFromFile();

	QHostInfo info = QHostInfo::fromName(QHostInfo::localHostName());

	QString ipAddress = info.addresses().first().toString();

	QString retString = requestUrl;
	retString = retString + QString("?Application_Name=") + QString::fromStdString(applicationName);
	retString = retString + QString("&MAC_Address=") + MacAddress;
	retString = retString + QString("&IP_Address=") + ipAddress;
	retString = retString + QString("&License_Code=") + licenseCode;
	retString = retString + QString("&Version=") + version;
	retString.remove(" ");

	return retString;
}

//-----------------------------------------------------------------------------
