#pragma once

#ifndef LAYER_HEADER_PANEL_INCLUDED
#define LAYER_HEADER_PANEL_INCLUDED

#include <QWidget>
#include <QToolButton>
#include <boost/optional.hpp>

#include "orientation.h"

using boost::optional;

class XsheetViewer;

// Panel showing column headers for layers in timeline mode
class LayerHeaderPanel final : public QWidget {
  Q_OBJECT

  enum { ToggleAllTransparency = 1, ToggleAllPreviewVisible, ToggleAllLock };

public:
  void toggleColumnVisibility(int visibilityFlag);

  QToolButton *m_previewButton;
  QToolButton *m_camstandButton;
  QToolButton *m_lockButton;

private:
  XsheetViewer *m_viewer;

public:
  LayerHeaderPanel(XsheetViewer *viewer, QWidget *parent = nullptr,
                   Qt::WindowFlags flags = Qt::WindowFlags());
  ~LayerHeaderPanel();

  void showOrHide(const Orientation *o);

public slots:
  void onPreviewClicked();
  void onCamstandClicked();
  void onLockClicked();
};

#endif