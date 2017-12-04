#pragma once

#ifndef FUNCTIONVIEWER_H
#define FUNCTIONVIEWER_H

// TnzCore includes
#include "tcommon.h"
#include "saveloadqsettings.h"

// TnzLib includes
#include "toonz/tframehandle.h"
#include "toonz/preferences.h"

// TnzQt includes
#include "toonzqt/treemodel.h"

// Qt includes
#include <QSplitter>
#include <QSettings>

#undef DVAPI
#undef DVVAR
#ifdef TOONZQT_EXPORTS
#define DVAPI DV_EXPORT_API
#define DVVAR DV_EXPORT_VAR
#else
#define DVAPI DV_IMPORT_API
#define DVVAR DV_IMPORT_VAR
#endif

//==================================================

//    Forward declarations

class TParam;
class TDoubleParam;
class TXsheetHandle;
class TObjectHandle;
class TFxHandle;
class TColumnHandle;
class TStageObject;
class TSceneHandle;

class FrameNavigator;
class FunctionTreeView;
class FunctionPanel;
class FunctionSheet;
class FunctionKeyframeNavigator;
class FunctionSegmentViewer;
class FunctionToolbar;
class FunctionSelection;

class QStackedWidget;
class QAction;
class QScrollArea;
class QSplitter;
class QVBoxLayout;

namespace DVGui {
class ValueField;
}

//==================================================

//************************************************************************
//    FunctionViewer  declaration
//************************************************************************

// Function editor widget
class DVAPI FunctionViewer final : public QSplitter, public SaveLoadQSettings {
  Q_OBJECT

public:
  enum IoType { eSaveCurve, eLoadCurve, eExportCurve };

public:
#if QT_VERSION >= 0x050500
  FunctionViewer(QWidget *parent = 0, Qt::WindowFlags flags = 0);
#else
  FunctionViewer(QWidget *parent = 0, Qt::WFlags flags = 0);
#endif
  ~FunctionViewer();

  void setXsheetHandle(TXsheetHandle *xshHandle);  //!< Associates an xsheet to
                                                   //! the function editor.
  void setFrameHandle(TFrameHandle *frameHandle);  //!< Synchronizes an external
  //! timeline with the function
  //! editor.
  void setObjectHandle(TObjectHandle *objectHandle);  //!< Associates a stage
  //! objects selector to the
  //! function editor.
  void setFxHandle(TFxHandle *fxHandle);  //!< Associates an fx selector to the
                                          //! function editor.
  void setColumnHandle(TColumnHandle *columnHandle);  //!< Associates a column
                                                      //! selector to the
  //! function editor.

  FunctionSelection *getSelection() const { return m_selection; }

  void openContextMenu(TreeModel::Item *item, const QPoint &globalPos);

  void addParameter(TParam *parameter,
                    const TFilePath &folder = TFilePath());  //!< Adds the
                                                             //! specified
  //! parameter to the
  //! function editor.
  //!\param parameter
  //! Parameter to be
  //! added. Ownership
  //! remains \a
  //! external. \param
  //! folder Host
  //! folder (created
  //! if necessary)
  //! the parameter
  //! will be added
  //! to.

  void setFocusColumnsOrGraph();
  void clearFocusColumnsAndGraph();
  bool columnsOrGraphHasFocus();
  void setSceneHandle(TSceneHandle *sceneHandle);
  // SaveLoadQSettings
  virtual void save(QSettings &settings) const override;
  virtual void load(QSettings &settings) override;

signals:

  void curveChanged();
  void curveIo(int type, TDoubleParam *curve, const std::string &name);
  void editObject();

public slots:

  void refreshModel();
  void rebuildModel();
  void onFrameSwitched();
  void toggleMode();
  void onValueFieldChanged();
  void onXsheetChanged();
  void onStageObjectSwitched();
  void onStageObjectChanged(bool isDragging = false);
  void onFxSwitched();

  void onCurveChanged(bool isDragging);
  void onCurveSelected(TDoubleParam *);
  void onSelectionChanged();

  void doSwitchCurrentObject(TStageObject *obj);
  void doSwitchCurrentFx(TFx *fx);

  // in order to avoid FunctionViewer to get focus while editing the expression
  bool isExpressionPageActive();

private:
  // Handles

  TXsheetHandle *m_xshHandle;
  TFrameHandle *m_frameHandle;
  TObjectHandle *m_objectHandle;
  TFxHandle *m_fxHandle;
  TColumnHandle *m_columnHandle;
  TSceneHandle *m_sceneHandle;
  QVBoxLayout *m_leftLayout;
  int m_spacing;
  int m_toggleStatus = 0;
  Preferences::FunctionEditorToggle m_toggleStart;
  TFrameHandle
      m_localFrame;  //!< Internal timeline - which is attached to m_frameHandle
                     //!  <I>in case</I> it is not zero.
  // Widgets

  FunctionTreeView *m_treeView;  //!< Tree view on the left side of the viewer.
  FunctionToolbar *m_toolbar;    //!< Central area's toolbar
  FunctionPanel *m_functionGraph;     //!< The function graph view widget.
  FunctionSheet *m_numericalColumns;  //!< The numerical columns view widget.
  FunctionSegmentViewer *m_segmentViewer;  //!< Lower segment editor.

  // Current objects

  TDoubleParam *m_curve;
  FunctionSelection *m_selection;

private:
  void showEvent(QShowEvent *) override;
  void hideEvent(QHideEvent *) override;

public:  //  :(
  void emitCurveChanged() {
    emit curveChanged();
  }  //!< \deprecated  Should not be public.
  void emitIoCurve(int type, TDoubleParam *curve, const std::string &name) {
    emit curveIo(type, curve, name);
  }  //!< \deprecated  Should not be public.

private slots:

  void propagateExternalSetFrame();  //!< Forwards m_frameHandle's setFrame()
                                     //! invocations to m_localFrame.
};

#endif  // FUNCTIONEDITORVIEWER_H
