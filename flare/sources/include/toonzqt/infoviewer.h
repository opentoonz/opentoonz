#pragma once

#ifndef INFOVIEWER_H
#define INFOVIEWER_H

#include <memory>

#include "toonzqt/dvdialog.h"

#undef DVAPI
#undef DVVAR
#ifdef TOONZQT_EXPORTS
#define DVAPI DV_EXPORT_API
#define DVVAR DV_EXPORT_VAR
#else
#define DVAPI DV_IMPORT_API
#define DVVAR DV_IMPORT_VAR
#endif

class TLevelP;
class TFilePath;
class TPalette;
class InfoViewerImp;

class DVAPI InfoViewer final : public DVGui::Dialog {
  Q_OBJECT
  std::unique_ptr<InfoViewerImp> m_imp;

public:
  InfoViewer(QWidget *parent = 0);
  ~InfoViewer();

protected:
  void hideEvent(QHideEvent *) override;
protected slots:
  void onSliderChanged(bool);
public slots:
  void setItem(const TLevelP &level, TPalette *palette, const TFilePath &path);
};

#endif
