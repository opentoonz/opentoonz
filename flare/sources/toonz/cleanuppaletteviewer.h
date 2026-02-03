#pragma once

#ifndef CLEANUP_PALETTE_VIEWER_H
#define CLEANUP_PALETTE_VIEWER_H

#include <vector>
#include <QWidget>

//------------------------------------------------------------------

//  Forward declarations

class QScrollArea;
class QPushButton;
class QFrame;

class TPaletteHandle;
class TCleanupStyle;

namespace DVGui {
class CleanupColorField;
}

//------------------------------------------------------------------

//********************************************************************************
//    CleanupPaletteViewer declaration
//********************************************************************************

class CleanupPaletteViewer final : public QWidget {
  Q_OBJECT

  TPaletteHandle *m_ph;

  QFrame *m_scrollWidget;
  QScrollArea *m_scrollArea;
  std::vector<DVGui::CleanupColorField *> m_colorFields;
  QPushButton *m_remove, *m_add;

  bool m_greyMode;
  bool m_contrastEnabled;

public:
  CleanupPaletteViewer(QWidget *parent);

  void setMode(bool greyMode);
  void updateColors();
  void setContrastEnabled(bool enable);

protected slots:

  void buildGUI();
  void onColorStyleChanged();
  void onColorStyleSelected(TCleanupStyle *);

  void onAddClicked(bool);
  void onRemoveClicked(bool);
};

#endif  // CLEANUP_PALETTE_VIEWER_H
