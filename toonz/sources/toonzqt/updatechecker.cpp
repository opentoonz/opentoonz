

#include "./toonzqt/updatechecker.h"
#include <assert.h>
#include <QNetworkReply>
#include <QEventLoop>
//=============================================================================
// UpdateChecker
//-----------------------------------------------------------------------------

UpdateChecker::UpdateChecker(const QString &requestToServer)
#if QT_VERSION >= 0x050000
	:
#else
	: QHttp(),
#endif
	  m_webPageUrl()
{
	int i = 0;
	QUrl url(requestToServer);
#if QT_VERSION >= 0x050000
	QString urlTemp = requestToServer;

	QStringList urlList = urlTemp.split('?');
	if (urlList.count() <= 1) {
		abort();
		return;
	}

	QStringList paramList = urlList.at(1).split('&');
	// if (!url.userName().isEmpty())
	//	setUser(url.userName(), url.password());

	m_httpRequestAborted = false;

	QString param;
	param = QString("?");
	for (i = 0; i < paramList.size(); i++) {
		param += paramList.at(i);
		if (i < paramList.size() - 1)
			param += QString("&");
	}

	QNetworkAccessManager manager;
	QNetworkReply *reply = manager.get(QNetworkRequest(url.path() + param));

	QEventLoop loop;

	connect(reply, SIGNAL(requestStarted(int)), &loop, SLOT(httpRequestStarted(int)));
	connect(reply, SIGNAL(authenticationRequired(const QString &, quint16, QAuthenticator *)),
			&loop, SLOT(slotAuthenticationRequired(const QString &, quint16, QAuthenticator *)));
	connect(reply, SIGNAL(stateChanged(int)), &loop, SLOT(httpStateChanged(int)));

	connect(&manager, SIGNAL(finished(QNetworkReply *)), this,
			SLOT(httpRequestFinished(QNetworkReply *)));

	loop.exec();

#else
	// connect(this, SIGNAL(readyRead(const QHttpResponseHeader &)), this,
	//	SLOT(readyReadexec(const QHttpResponseHeader &)));
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
	param = QString("?");
	for (i = 0; i < paramList.size(); i++) {
		param += paramList.at(i);
		if (i < paramList.size() - 1)
			param += QString("&");
	}
	/*param = QString("?")+paramList.at(0)+QString("&")
		+paramList.at(1) + QString("&") + paramList.at(2);*/
	m_httpGetId = get(url.path() + param); //, file);
#endif
}

//-----------------------------------------------------------------------------

#if QT_VERSION < 0x050000
void UpdateChecker::readResponseHeader(const QHttpResponseHeader &responseHeader)
{
	int err = responseHeader.statusCode();
	if (err != 200 && err != 502) {
		m_httpRequestAborted = true;
		abort();
	}
}
#endif

//-----------------------------------------------------------------------------

void UpdateChecker::httpStateChanged(int status)
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
		break;
	default:
		stateStr = "There is no connection to the host.";
	}
#if QT_VERSION < 0x050000
	status = state();
	qDebug("Status: %d : %s", status, stateStr.c_str());
#endif
}

//-----------------------------------------------------------------------------

#if QT_VERSION >= 0x050000
void UpdateChecker::httpRequestFinished(QNetworkReply *reply)
#else
void UpdateChecker::httpRequestFinished(int requestId, bool error)
#endif
{
	QByteArray arr;
	std::string webPageString;
	std::string dateString;
	QString qstr;
#if QT_VERSION >= 0x050000
	if (reply->error() != QNetworkReply::NoError)
		return;
	arr = reply->readAll();
#else
	if (requestId != m_httpGetId) {
		return;
	}
	if (error || m_httpRequestAborted) {
		abort();
		return;
	}
	arr = readAll();
#endif
	qstr = QString(arr);

	int startIndex = qstr.indexOf("Startdate");
	int endIndex = qstr.indexOf("Enddate");

	if ((endIndex - startIndex - 9) <= 0) {
		abort();
		return;
	}

	dateString = qstr.toStdString();
	dateString = dateString.substr(startIndex + 9, endIndex - startIndex - 9);

	QString qDateString = QString::fromStdString(dateString);
	qDateString.remove("\"");

	// Make sure that the format is exactly MM/dd/yyyy
	if (qDateString.size() != 10) {
		QStringList fields = qDateString.split("/");
		if (fields.size() == 3) {
			QString month = fields.at(0);
			QString day = fields.at(1);
			QString year = fields.at(2);

			if (day.size() == 1)
				day.prepend("0");
			if (month.size() == 1)
				month.prepend("0");

			qDateString = month + "/" + day + "/" + year;
		}
	}

	m_updateDate = QDate::fromString(qDateString, "MM/dd/yyyy");

	startIndex = qstr.indexOf("Starturl");
	endIndex = qstr.indexOf("Endurl");

	webPageString = qstr.toStdString();
	webPageString = webPageString.substr(startIndex + 9, endIndex - startIndex - 9);

	QString qWebPageString = QString::fromStdString(webPageString);
	qWebPageString.remove("\"");

	QUrl webPageUrl(qWebPageString);

	if (webPageUrl.isValid())
		m_webPageUrl = webPageUrl;
}

//-----------------------------------------------------------------------------

void UpdateChecker::slotAuthenticationRequired(const QString &hostName, quint16,
											   QAuthenticator *authenticator)
{
	assert(false);
}
