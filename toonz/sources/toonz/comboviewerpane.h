#pragma once

#ifndef COMBOVIEWER_PANE_INCLUDED
#define COMBOVIEWER_PANE_INCLUDED

#include "viewerpane.h"

class Toolbar;
class ToolOptions;

//=============================================================================
// ComboViewerPanel
//-----------------------------------------------------------------------------

class ComboViewerPanel final : public BaseViewerPanel {
  Q_OBJECT

  Toolbar *m_toolbar;
  ToolOptions *m_toolOptions;
  Ruler *m_vRuler;
  Ruler *m_hRuler;

public:
  ComboViewerPanel(QWidget *parent = 0, Qt::WindowFlags flags = 0);
  ~ComboViewerPanel() {}

  ToolOptions *getToolOptions() { return m_toolOptions; }

  void updateShowHide() override;
  void addShowHideContextMenu(QMenu *) override;

protected:
  void checkOldVersionVisblePartsFlags(QSettings &settings) override;
};

#endif
