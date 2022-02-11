#pragma once

#ifndef FXSETTINGS_H
#define FXSETTINGS_H

#ifdef _MSC_VER
#pragma warning(disable : 4251)
#endif

#include <QSplitter>
#include <QToolBar>
#include <QStackedWidget>
#include <QScrollArea>
#include <QMap>
#include <QGroupBox>

#include "tcommon.h"
#include "tfx.h"
#include "tabbar.h"
#include "gutil.h"

#include "toonzqt/framenavigator.h"
#include "toonzqt/paramfield.h"
#include "toonzqt/swatchviewer.h"
#include "toonzqt/fxhistogramrender.h"

#undef DVAPI
#undef DVVAR
#ifdef TOONZQT_EXPORTS
#define DVAPI DV_EXPORT_API
#define DVVAR DV_EXPORT_VAR
#else
#define DVAPI DV_IMPORT_API
#define DVVAR DV_IMPORT_VAR
#endif

// forward declaration
class QToolBar;
class QStackedWidget;
class QVBoxLayout;
class QGridLayout;
class QPushButton;
class FxKeyframeNavigator;
class ParamViewer;
class TFxHandle;
class TFrameHandle;
class TXsheetHandle;
class TSceneHandle;
class TXshLevelHandle;
class TObjectHandle;
class ToonzScene;

//=============================================================================
/*! \brief ParamsPage. View a page with fx params.

                Inherits \b QWidget.
*/
class DVAPI ParamsPage final : public QFrame {
  Q_OBJECT

  QColor m_textColor; /*-- 文字の色 デフォルト黒 --*/
  Q_PROPERTY(QColor TextColor READ getTextColor WRITE setTextColor)

  QGridLayout *m_mainLayout;
  QHBoxLayout *m_horizontalLayout;
  QGridLayout *m_groupLayout;

  friend class ParamViewer;
  QVector<ParamField *> m_fields;
  /*! To menage eventually histogram in page. */
  FxHistogramRender *m_fxHistogramRender;

  ParamViewer *m_paramViewer;

public:
  ParamsPage(QWidget *parent = 0, ParamViewer *paramViewer = 0);
  ~ParamsPage();

  void setPage(TIStream &is, const TFxP &fx, bool isFirstPage) {
    setPageField(is, fx);
    if (isFirstPage) addGlobalControl(fx);
    setPageSpace();
  }

  void setFx(const TFxP &currentFx, const TFxP &actualFx, int frame);

  void update(int frame);
  void setPointValue(int index, const TPointD &p);

  FxHistogramRender *getFxHistogramRender() const {
    return m_fxHistogramRender;
  }

  /*- 現在のページの最適なサイズを返す -*/
  QSize getPreferredSize();

  void setTextColor(const QColor &color) { m_textColor = color; }
  QColor getTextColor() const { return m_textColor; }

protected:
  void setPageField(TIStream &is, const TFxP &fx, bool isVertical = true);
  void addGlobalControl(const TFxP &fx);

public:
  void setPageSpace();
  void beginGroup(const char *name);
  void endGroup();

  void addWidget(QWidget *, bool isVertical = true);

#define TOONZ_DECLARE_NEW_COMPONENT(NAME)                                      \
  QWidget *NAME(TFx *fx, const char *name)

  TOONZ_DECLARE_NEW_COMPONENT(newParamField);
  TOONZ_DECLARE_NEW_COMPONENT(newLineEdit);
  TOONZ_DECLARE_NEW_COMPONENT(newSlider);
  TOONZ_DECLARE_NEW_COMPONENT(newSpinBox);
  TOONZ_DECLARE_NEW_COMPONENT(newCheckBox);
  TOONZ_DECLARE_NEW_COMPONENT(newRadioButton);
  TOONZ_DECLARE_NEW_COMPONENT(newComboBox);

#undef TOONZ_DECLARE_NEW_COMPONENT

  // make ParamsPageSet to re-compute preferred size.
  // currently emitted only from ToneCurveParamField
signals:
  void preferredPageSizeChanged();
};

//=============================================================================
/*! \brief ParamsPageSet. Contains a stack of page \b ParamsPage with relative
   tab.

                Inherits \b QWidget.
*/
class DVAPI ParamsPageSet final : public QWidget {
  Q_OBJECT

  TabBarContainter *m_tabBarContainer;
  DVGui::TabBar *m_tabBar;
  QStackedWidget *m_pagesList;

  ParamViewer *m_parent;

  //! Allows to map page and index, useful to display a macro.
  QMap<ParamsPage *, int> m_pageFxIndexTable;

  QSize m_preferredSize;
  /*-- ヘルプのファイルパス（もしあれば）---*/
  std::string m_helpFilePath;
  /*-- pdfファイルのページ指定など、引数が必要な場合の追加引数 --*/
  std::string m_helpCommand;
  /*-- ヘルプボタンで開くURL --*/
  std::string m_helpUrl;
  QPushButton *m_helpButton;

public:
#if QT_VERSION >= 0x050500
  ParamsPageSet(QWidget *parent = 0, Qt::WindowFlags flags = 0);
#else
  ParamsPageSet(QWidget *parent = 0, Qt::WFlags flags = 0);
#endif
  ~ParamsPageSet();

  void setFx(const TFxP &currentFx, const TFxP &actualFx, int frame);
  void setScene(ToonzScene *scene);
  void setIsCameraViewMode(bool isCameraViewMode);

  void updatePage(int frame, bool onlyParam);
  /*! Create a page reading xml file relating to \b fx. */
  void createControls(const TFxP &fx, int index = -1);

  ParamsPage *getCurrentParamsPage() const;
  ParamsPage *getParamsPage(int index) const;
  int getParamsPageCount() const { return (int)m_pagesList->count(); };

  ParamsPage *createParamsPage();
  void addParamsPage(ParamsPage *page, const char *name);

  QSize getPreferredSize() { return m_preferredSize; }

protected:
  void createPage(TIStream &is, const TFxP &fx, int index);

protected slots:
  void setPage(int);
  void openHelpFile();
  void openHelpUrl();
  void recomputePreferredSize();
};

//=============================================================================
/*! \brief ParamViewer. Contains a stack of \b ParamsPageSet.

                Inherits \b QWidget.
*/
class DVAPI ParamViewer final : public QFrame {
  Q_OBJECT

  TFxP m_fx;
  TFxP m_actualFx;

  QStackedWidget *m_tablePageSet;
  QMap<std::string, int> m_tableFxIndex;

public:
#if QT_VERSION >= 0x050500
  ParamViewer(QWidget *parent = 0, Qt::WindowFlags flags = 0);
#else
  ParamViewer(QWidget *parent = 0, Qt::WFlags flags = 0);
#endif
  ~ParamViewer();

  void setFx(const TFxP &currentFx, const TFxP &actualFx, int frame,
             ToonzScene *scene);
  void setScene(ToonzScene *scene);
  void setIsCameraViewMode(bool isCameraViewMode);
  /*! If onlyParam is true don't invalidate raster of associated histogram. */
  void update(int frame, bool onlyParam);

  void setPointValue(int index, const TPointD &p);

  void notifyPreferredSizeChanged(QSize size) {
    emit preferredSizeChanged(size);
  }

protected:
  ParamsPageSet *getCurrentPageSet() const;

signals:
  void currentFxParamChanged();
  void actualFxParamChanged();
  void paramKeyChanged();

  void preferredSizeChanged(QSize);
  void showSwatchButtonToggled(bool);
};

//=============================================================================
/*! \brief FxSettings.

                Inherits \b QWidget.
*/

class QActionGroup;
class DVAPI FxSettings final : public QSplitter {
  Q_OBJECT

  QToolBar *m_toolBar;
  QAction *m_checkboardBg;
  ParamViewer *m_paramViewer;
  SwatchViewer *m_viewer;

  TFxHandle *m_fxHandle;
  TXsheetHandle *m_xsheetHandle;
  TSceneHandle *m_sceneHandle;
  TXshLevelHandle *m_levelHandle;
  TFrameHandle *m_frameHandle;
  TObjectHandle *m_objectHandle;

  FxKeyframeNavigator *m_keyframeNavigator;
  FrameNavigator *m_frameNavigator;

  TPixel32 m_checkCol1, m_checkCol2;

  bool m_isCameraModeView;

  int m_container_height;
  int m_container_width;

public:
  FxSettings(QWidget *parent, const TPixel32 &checkCol1,
             const TPixel32 &checkCol2);
  ~FxSettings();

  // Devono essere settati!
  void setFxHandle(TFxHandle *fxHandle);
  TFxHandle *getFxHandle() const { return m_fxHandle; }
  void setFrameHandle(TFrameHandle *frameHandle);
  TFrameHandle *getFrameHandle() const { return m_frameHandle; }
  void setXsheetHandle(TXsheetHandle *XsheetHandle);
  TXsheetHandle *getXsheetHandle() const { return m_xsheetHandle; }
  void setSceneHandle(TSceneHandle *sceneHandle);
  TSceneHandle *getSceneHandle() const { return m_sceneHandle; }
  void setLevelHandle(TXshLevelHandle *levelHandle);
  TXshLevelHandle *getLevelHandle() const { return m_levelHandle; }
  void setObjectHandle(TObjectHandle *objectHandle);
  TObjectHandle *getObjectHandle() const { return m_objectHandle; }

public slots:
  void setCurrentFrame();
  void setCurrentFx();
  void setCurrentScene();
  void notifySceneChanged();

protected:
  /*! \b currentFx is fx with parent, \b actualFx is simple fx. */
  void setFx(const TFxP &currentFx, const TFxP &actualFx);

  void createToolBar();
  void showEvent(QShowEvent *) override;
  void hideEvent(QHideEvent *) override;
  void setCheckboardColors(const TPixel32 &col1, const TPixel32 &col2);

  void changeTitleBar(TFx *fx);

protected slots:
  void updateViewer();
  void updateParamViewer();
  void onPointChanged(int index, const TPointD &p);
  void onViewModeChanged(QAction *);

  void setWhiteBg();
  void setBlackBg();
  void setCheckboardBg();

  void onPreferredSizeChanged(QSize);
  void onShowSwatchButtonToggled(bool);
};

#endif  // FXSETTINGS_H
