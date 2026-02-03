#pragma once

#ifndef MAGPIEFILEIMPORTPOPUP_H
#define MAGPIEFILEIMPORTPOPUP_H

#include "toonzqt/dvdialog.h"
#include "tfilepath.h"

class FlipBook;

namespace DVGui {
class IntLineEdit;
class FileField;
class LineEdit;
}

class MagpieInfo {
  QList<QString> m_actorActs;
  QList<QString> m_comments;
  QList<QString> m_actsIdentifier;
  QString m_fileName;

public:
  MagpieInfo(TFilePath path);

  int getFrameCount() const { return m_comments.size(); }

  QList<QString> getComments() const { return m_comments; }
  QList<QString> getActorActs() const { return m_actorActs; }
  QList<QString> getActsIdentifier() const { return m_actsIdentifier; }
  QString getFileName() const { return m_fileName; }
};

//=============================================================================
// MagpieFileImportPopup
//-----------------------------------------------------------------------------

class MagpieFileImportPopup final : public DVGui::Dialog {
  Q_OBJECT

  MagpieInfo *m_info;

  DVGui::FileField *m_levelField;
  DVGui::IntLineEdit *m_fromField;
  DVGui::IntLineEdit *m_toField;

  QList<QPair<QLabel *, DVGui::IntLineEdit *>> m_actFields;

  FlipBook *m_flipbook;
  TFilePath m_levelPath;

public:
  MagpieFileImportPopup();

  void setFilePath(TFilePath path);

protected:
  void showEvent(QShowEvent *) override;
  void hideEvent(QHideEvent *) override;

protected slots:
  void onLevelPathChanged();
  void onOkPressed();

signals:
  void closeButtonPressed();
  void doubleClick();
};

#endif  // MAGPIEFILEIMPORTPOPUP_H
