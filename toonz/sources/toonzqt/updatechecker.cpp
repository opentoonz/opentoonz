#include "./toonzqt/updatechecker.h"
#include <QNetworkReply>

UpdateChecker::UpdateChecker(QUrl const& updateUrl)
    : manager_(new QNetworkAccessManager(this),
               &QNetworkAccessManager::deleteLater) {
  connect(manager_.data(), SIGNAL(finished(QNetworkReply*)), this,
          SLOT(httpRequestFinished(QNetworkReply*)));

  manager_->get(QNetworkRequest(updateUrl));
}

void UpdateChecker::httpRequestFinished(QNetworkReply* pReply) {
  QSharedPointer<QNetworkReply> reply(pReply, &QNetworkReply::deleteLater);

  // If there was an error, don't bother doing the check
  if (reply->error() != QNetworkReply::NoError) {
    emit done(true);
    return;
  }

  // Convert the response from a QByteArray into a QString
  QString candidateVersion = QString(reply->readAll()).trimmed();

  // TODO: Verify that the response was valid by ensuring we have a single line
  // in the format x.x[.x]*
  if (candidateVersion.indexOf(".") < 0) {
    // There was some invalid response, so we'll ignore the check for now
    emit done(true);
    return;
  }

  // Completed with no errors
  m_latestVersion = candidateVersion;
  emit done(false);
}
