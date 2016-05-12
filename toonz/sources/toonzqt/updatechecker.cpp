#include "./toonzqt/updatechecker.h"
#include <QNetworkReply>

UpdateChecker::UpdateChecker(const QUrl &updateUrl)
{
	// TODO: Determine if *manager will be deleted at the appropriate time
	QNetworkAccessManager *manager = new QNetworkAccessManager(this);
	QNetworkReply *reply = manager->get(QNetworkRequest(updateUrl));

	connect(manager, &QNetworkAccessManager::finished, this, &UpdateChecker::httpRequestFinished);
}

void UpdateChecker::httpRequestFinished(QNetworkReply *reply)
{
	// If there was an error, don't bother doing the check
	if (reply->error() != QNetworkReply::NoError) {
		emit done(true);
		return;
	}

	// Convert the response from a QByteArray into a QString
	QString candidateVersion = QString(reply->readAll()).trimmed();

	// TODO: Verify that the response was valid by ensuring we have a single line in the format x.x[.x]*
	// For now, we can do a lazy version by ensuring we have at least a period
	if (candidateVersion.indexOf(".") < 0) {
		// There was some invalid response, so we'll ignore the check for now
		emit done(true);
		return;
	}

	// Completed with no errors
	m_latestVersion = candidateVersion;
	emit done(false);
}
