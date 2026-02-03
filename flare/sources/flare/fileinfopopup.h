#pragma once

#ifndef FILEINFOPOPUP_H
#define FILEINFOPOPUP_H

#include "toonzqt/dvdialog.h"
#include "tgeometry.h"

// forward declaration
class QPushButton;
class QLabel;
class TFilePath;

//=============================================================================
// FileInfoPopup
//-----------------------------------------------------------------------------

class FileInfoPopup final : public DVGui::Dialog {
  Q_OBJECT

  typedef std::pair<std::string, std::string> StringPair;

  QPushButton *m_closeBtn;

  QLabel *m_nameLabel;
  QLabel *m_pathLabel;
  QLabel *m_sizeLabel;
  QLabel *m_dataChangedLabel;
  //  StringPair m_name;
  //  StringPair m_path;
  //  StringPair m_size;
  //  StringPair m_modDate;

  //  std::vector<StringPair> m_attrib;
  //  QLabel *m_dpiLabel;
  //  TFilePath *m_xl;

  void drawpair(TPoint p, std::pair<std::string, std::string> &StringPair);

public:
  FileInfoPopup();

  void configureNotify(const TDimension &size);
  void draw();
  void getSizeandDate(TFilePath &fp);

  void raise();
};

#endif  // FILEINFOPOPUP_H
