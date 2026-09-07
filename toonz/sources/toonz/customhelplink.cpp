#include "customhelplink.h"

// TnzQt includes
#include "toonzqt/dvdialog.h"

// TnzLib includes
#include "toonz/preferences.h"

// TnzBase includes
#include "tenv.h"

// TnzCore includes
#include "tsystem.h"

// Qt includes
#include <QCoreApplication>
#include <QDesktopServices>
#include <QFileInfo>
#include <QUrl>

namespace {

const QString DefaultCustomHelpLink = QStringLiteral("index.html");

QUrl resolveHelpLink(const QString &link) {
  QUrl url(link);
  // QUrl("C:/docs/help.html") reports "c" as a one-character scheme.
  if (url.isValid() && url.scheme().length() > 1) return url;

  // A PDF page is expressed as a URL fragment (for example #page=12). Keep
  // the fragment out of local filesystem resolution, then restore it on the
  // resulting file URL.
  QString localPath = link;
  QString fragment;
  int fragmentIndex = localPath.indexOf('#');
  if (fragmentIndex >= 0) {
    fragment  = localPath.mid(fragmentIndex + 1);
    localPath = localPath.left(fragmentIndex);
  }

  QFileInfo fileInfo(localPath);
  if (fileInfo.isAbsolute()) {
    url = QUrl::fromLocalFile(fileInfo.absoluteFilePath());
    url.setFragment(fragment);
    return url;
  }

  std::string lang =
      Preferences::instance()->getCurrentLanguage().toStdString();
  TFilePath base = TEnv::getStuffDir() + "doc";
  TFilePath relativePath(localPath.toStdWString());
  TFilePath fp = base + lang + relativePath;
  if (!TFileStatus(fp).doesExist()) fp = base + relativePath;

  url = QUrl::fromLocalFile(QString::fromStdWString(fp.getWideString()));
  url.setFragment(fragment);
  return url;
}

}  // namespace

namespace CustomHelpLink {

QString current() {
  QString link =
      Preferences::instance()->getStringValue(customHelpLink).trimmed();
  return link.isEmpty() ? DefaultCustomHelpLink : link;
}

void set(const QString &link) {
  Preferences::instance()->setValue(customHelpLink, link.trimmed());
}

void open() {
  const QString link = current();
  QUrl url           = resolveHelpLink(link);
  if (!url.isValid()) {
    DVGui::warning(QCoreApplication::translate(
        "MainWindow", "The Quicklink URL is not valid."));
    return;
  }

  if (url.isLocalFile() && !QFileInfo::exists(url.toLocalFile())) {
    DVGui::warning(QCoreApplication::translate(
                       "MainWindow", "Could not find the help file \"%1\".")
                       .arg(link));
    return;
  }

  if (!QDesktopServices::openUrl(url))
    DVGui::warning(QCoreApplication::translate(
        "MainWindow", "Could not open the Quicklink URL."));
}

}  // namespace CustomHelpLink
