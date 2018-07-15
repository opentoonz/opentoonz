#pragma once

#ifndef SCHEMATICVIEWER_H
#define SCHEMATICVIEWER_H

// TnzLib includes
#include "toonz/tstageobjectid.h"

// TnzBase includes
#include "tfx.h"

// Qt includes
#include <QGraphicsScene>
#include <QGraphicsView>

#include <QIcon>

// STD includes
#include <set>

#undef DVAPI
#undef DVVAR
#ifdef TOONZQT_EXPORTS
#define DVAPI DV_EXPORT_API
#define DVVAR DV_EXPORT_VAR
#else
#define DVAPI DV_IMPORT_API
#define DVVAR DV_IMPORT_VAR
#endif

//====================================================

//    Forward declarations

class SchematicNode;
class SchematicPort;
class SchematicLink;
class ToonzScene;
class StageSchematicScene;
class FxSchematicScene;
class TXsheetHandle;
class TObjectHandle;
class TColumnHandle;
class TFxHandle;
class TSceneHandle;
class TFrameHandle;
class TFx;
class TLevel;
class TSelection;
class TApplication;
class QToolBar;
class QToolButton;
class QAction;

//====================================================

//==================================================================
//
// SchematicScene
//
//==================================================================

class DVAPI SchematicScene : public QGraphicsScene {
  Q_OBJECT

public:
  SchematicScene(QWidget *parent);
  ~SchematicScene();

  void clearAllItems();

  virtual QGraphicsItem *getCurrentNode() { return 0; }
  virtual void reorderScene() = 0;
  virtual void updateScene()  = 0;

protected:
  QList<SchematicLink *> m_highlightedLinks;
  enum GridDimension { eLarge, eSmall };

protected:
  //! Returns \b true if no nodes intersects \b rect.
  bool isAnEmptyZone(const QRectF &rect);
  //! Returns a vector containing all nodes which had their bounding rects
  //! conatined in \b node bounding
  //! rect enlarged of 10.
  QVector<SchematicNode *> getPlacedNode(SchematicNode *node);

  void showEvent(QShowEvent *se);
  void hideEvent(QHideEvent *se);

protected slots:

  virtual void onSelectionSwitched(TSelection *, TSelection *) {}
};

//==================================================================
//
// SchematicSceneViewer
//
//==================================================================

class DVAPI SchematicSceneViewer final : public QGraphicsView {
  Q_OBJECT

public:
  SchematicSceneViewer(QWidget *parent);
  ~SchematicSceneViewer();

  void zoomQt(bool zoomin, bool resetZoom);

  QPointF getOldScenePos() { return m_oldScenePos; }

protected:
  void mousePressEvent(QMouseEvent *me) override;
  void mouseMoveEvent(QMouseEvent *me) override;
  void mouseReleaseEvent(QMouseEvent *me) override;
  void keyPressEvent(QKeyEvent *ke) override;
  void wheelEvent(QWheelEvent *me) override;
  void showEvent(QShowEvent *se) override;

protected slots:

  void fitScene();
  void centerOnCurrent();
  void reorderScene();
  void normalizeScene();

private:
  Qt::MouseButton m_buttonState;
  QPoint m_oldWinPos;
  QPointF m_oldScenePos;
  bool m_firstShowing;

private:
  void changeScale(const QPoint &winPos, qreal scaleFactor);
};

//==================================================================
//
// SchematicViewer
//
//==================================================================

class DVAPI SchematicViewer final : public QWidget {
  Q_OBJECT

  QColor m_textColor;  // text color (black)
  Q_PROPERTY(QColor TextColor READ getTextColor WRITE setTextColor)

  QColor m_verticalLineColor;
  Q_PROPERTY(QColor VerticalLineColor READ getVerticalLineColor WRITE
                 setVerticalLineColor)

  // TZP column
  QColor m_levelColumnColor;  //(127,219,127)
  Q_PROPERTY(QColor LevelColumnColor READ getLevelColumnColor WRITE
                 setLevelColumnColor)

  // PLI column
  QColor m_vectorColumnColor;  //(212,212,133)
  Q_PROPERTY(QColor VectorColumnColor READ getVectorColumnColor WRITE
                 setVectorColumnColor)

  // subXsheet column
  QColor m_childColumnColor;  //(214,154,219)
  Q_PROPERTY(QColor ChildColumnColor READ getChildColumnColor WRITE
                 setChildColumnColor)

  // Raster image column
  QColor m_fullcolorColumnColor;  //(154,214,219)
  Q_PROPERTY(QColor FullcolorColumnColor READ getFullcolorColumnColor WRITE
                 setFullcolorColumnColor)

  // Fx column
  QColor m_fxColumnColor;  //(130,129,93)
  Q_PROPERTY(QColor FxColumnColor READ getFxColumnColor WRITE setFxColumnColor)

  // Palette column
  QColor m_paletteColumnColor;  //(42,171,154)
  Q_PROPERTY(QColor PaletteColumnColor READ getPaletteColumnColor WRITE
                 setPaletteColumnColor)

  // Mesh column
  QColor m_meshColumnColor;
  Q_PROPERTY(
      QColor MeshColumnColor READ getMeshColumnColor WRITE setMeshColumnColor)

  // Table node
  QColor m_tableColor;
  Q_PROPERTY(QColor TableColor READ getTableColor WRITE setTableColor)

  // Camera nodes
  QColor m_activeCameraColor, m_otherCameraColor;
  Q_PROPERTY(QColor ActiveCameraColor READ getActiveCameraColor WRITE
                 setActiveCameraColor)
  Q_PROPERTY(QColor OtherCameraColor READ getOtherCameraColor WRITE
                 setOtherCameraColor)

  // Group node
  QColor m_groupColor;
  Q_PROPERTY(QColor GroupColor READ getGroupColor WRITE setGroupColor)

  // Peg node
  QColor m_pegColor;
  Q_PROPERTY(QColor PegColor READ getPegColor WRITE setPegColor)

  // Path node
  QColor m_splineColor;
  Q_PROPERTY(QColor SplineColor READ getSplineColor WRITE setSplineColor)

  // Output nodes
  QColor m_activeOutputColor, m_otherOutputColor;
  Q_PROPERTY(QColor ActiveOutputColor READ getActiveOutputColor WRITE
                 setActiveOutputColor)
  Q_PROPERTY(QColor OtherOutputColor READ getOtherOutputColor WRITE
                 setOtherOutputColor)

  // Xsheet node
  QColor m_xsheetColor;
  Q_PROPERTY(QColor XsheetColor READ getXsheetColor WRITE setXsheetColor)

  // Fx nodes
  QColor m_normalFx;
  Q_PROPERTY(QColor NormalFxColor READ getNormalFxColor WRITE setNormalFxColor)

  QColor m_macroFx;
  Q_PROPERTY(QColor MacroFxColor READ getMacroFxColor WRITE setMacroFxColor)

  QColor m_imageAdjustFx;
  Q_PROPERTY(QColor ImageAdjustFxColor READ getImageAdjustFxColor WRITE
                 setImageAdjustFxColor)

  QColor m_layerBlendingFx;
  Q_PROPERTY(QColor LayerBlendingFxColor READ getLayerBlendingFxColor WRITE
                 setLayerBlendingFxColor)

  QColor m_matteFx;
  Q_PROPERTY(QColor MatteFxColor READ getMatteFxColor WRITE setMatteFxColor)

  // Schematic Preview Button
  QColor m_schematicPreviewButtonBgOnColor;
  QIcon m_schematicPreviewButtonOnImage;
  QColor m_schematicPreviewButtonBgOffColor;
  QIcon m_schematicPreviewButtonOffImage;
  Q_PROPERTY(QColor SchematicPreviewButtonBgOnColor READ
                 getSchematicPreviewButtonBgOnColor WRITE
                     setSchematicPreviewButtonBgOnColor)
  Q_PROPERTY(
      QIcon SchematicPreviewButtonOnImage READ getSchematicPreviewButtonOnImage
          WRITE setSchematicPreviewButtonOnImage)
  Q_PROPERTY(QColor SchematicPreviewButtonBgOffColor READ
                 getSchematicPreviewButtonBgOffColor WRITE
                     setSchematicPreviewButtonBgOffColor)
  Q_PROPERTY(QIcon SchematicPreviewButtonOffImage READ
                 getSchematicPreviewButtonOffImage WRITE
                     setSchematicPreviewButtonOffImage)

  // Schematic Camstand Button
  QColor m_schematicCamstandButtonBgOnColor;
  QIcon m_schematicCamstandButtonOnImage;
  QIcon m_schematicCamstandButtonTranspImage;
  QColor m_schematicCamstandButtonBgOffColor;
  QIcon m_schematicCamstandButtonOffImage;
  Q_PROPERTY(QColor SchematicCamstandButtonBgOnColor READ
                 getSchematicCamstandButtonBgOnColor WRITE
                     setSchematicCamstandButtonBgOnColor)
  Q_PROPERTY(QIcon SchematicCamstandButtonOnImage READ
                 getSchematicCamstandButtonOnImage WRITE
                     setSchematicCamstandButtonOnImage)
  Q_PROPERTY(QIcon SchematicCamstandButtonTranspImage READ
                 getSchematicCamstandButtonTranspImage WRITE
                     setSchematicCamstandButtonTranspImage)
  Q_PROPERTY(QColor SchematicCamstandButtonBgOffColor READ
                 getSchematicCamstandButtonBgOffColor WRITE
                     setSchematicCamstandButtonBgOffColor)
  Q_PROPERTY(QIcon SchematicCamstandButtonOffImage READ
                 getSchematicCamstandButtonOffImage WRITE
                     setSchematicCamstandButtonOffImage)

public:
  SchematicViewer(QWidget *parent);
  ~SchematicViewer();

  void setSchematicScene(SchematicScene *scene);
  void setApplication(TApplication *app);
  bool isStageSchematicViewed();
  void setStageSchematicViewed(bool isStageSchematic);

  void setTextColor(const QColor &color) { m_textColor = color; }
  QColor getTextColor() const { return m_textColor; }

  void setVerticalLineColor(const QColor &color) {
    m_verticalLineColor = color;
  }
  QColor getVerticalLineColor() const { return m_verticalLineColor; }

  // TZP column
  void setLevelColumnColor(const QColor &color) { m_levelColumnColor = color; }
  QColor getLevelColumnColor() const { return m_levelColumnColor; }

  // PLI column
  void setVectorColumnColor(const QColor &color) {
    m_vectorColumnColor = color;
  }
  QColor getVectorColumnColor() const { return m_vectorColumnColor; }

  // subXsheet column
  void setChildColumnColor(const QColor &color) { m_childColumnColor = color; }
  QColor getChildColumnColor() const { return m_childColumnColor; }

  // Raster image column
  void setFullcolorColumnColor(const QColor &color) {
    m_fullcolorColumnColor = color;
  }
  QColor getFullcolorColumnColor() const { return m_fullcolorColumnColor; }

  // Fx column
  void setFxColumnColor(const QColor &color) { m_fxColumnColor = color; }
  QColor getFxColumnColor() const { return m_fxColumnColor; }

  // Palette column
  void setPaletteColumnColor(const QColor &color) {
    m_paletteColumnColor = color;
  }
  QColor getPaletteColumnColor() const { return m_paletteColumnColor; }

  // Mesh column
  void setMeshColumnColor(const QColor &color) { m_meshColumnColor = color; }
  QColor getMeshColumnColor() const { return m_meshColumnColor; }

  // Table node
  void setTableColor(const QColor &color) { m_tableColor = color; }
  QColor getTableColor() const { return m_tableColor; }

  // Camera nodes
  void setActiveCameraColor(const QColor &color) {
    m_activeCameraColor = color;
  }
  void setOtherCameraColor(const QColor &color) { m_otherCameraColor = color; }
  QColor getActiveCameraColor() const { return m_activeCameraColor; }
  QColor getOtherCameraColor() const { return m_otherCameraColor; }

  // Group node
  void setGroupColor(const QColor &color) { m_groupColor = color; }
  QColor getGroupColor() const { return m_groupColor; }

  // Peg node
  void setPegColor(const QColor &color) { m_pegColor = color; }
  QColor getPegColor() const { return m_pegColor; }

  // Path node
  void setSplineColor(const QColor &color) { m_splineColor = color; }
  QColor getSplineColor() const { return m_splineColor; }

  // Output nodes
  void setActiveOutputColor(const QColor &color) {
    m_activeOutputColor = color;
  }
  void setOtherOutputColor(const QColor &color) { m_otherOutputColor = color; }
  QColor getActiveOutputColor() const { return m_activeOutputColor; }
  QColor getOtherOutputColor() const { return m_otherOutputColor; }

  // Xsheet node
  void setXsheetColor(const QColor &color) { m_xsheetColor = color; }
  QColor getXsheetColor() const { return m_xsheetColor; }

  // Fx nodes
  QColor getNormalFxColor() const { return m_normalFx; }
  void setNormalFxColor(const QColor &color) { m_normalFx = color; }

  QColor getMacroFxColor() const { return m_macroFx; }
  void setMacroFxColor(const QColor &color) { m_macroFx = color; }

  QColor getImageAdjustFxColor() const { return m_imageAdjustFx; }
  void setImageAdjustFxColor(const QColor &color) { m_imageAdjustFx = color; }

  QColor getLayerBlendingFxColor() const { return m_layerBlendingFx; }
  void setLayerBlendingFxColor(const QColor &color) {
    m_layerBlendingFx = color;
  }

  QColor getMatteFxColor() const { return m_matteFx; }
  void setMatteFxColor(const QColor &color) { m_matteFx = color; }

  // Schematic Preview Button
  void setSchematicPreviewButtonBgOnColor(const QColor &color) {
    m_schematicPreviewButtonBgOnColor = color;
  }
  void setSchematicPreviewButtonOnImage(const QIcon &image) {
    m_schematicPreviewButtonOnImage = image;
  }
  void setSchematicPreviewButtonBgOffColor(const QColor &color) {
    m_schematicPreviewButtonBgOffColor = color;
  }
  void setSchematicPreviewButtonOffImage(const QIcon &image) {
    m_schematicPreviewButtonOffImage = image;
  }
  QColor getSchematicPreviewButtonBgOnColor() const {
    return m_schematicPreviewButtonBgOnColor;
  }
  QIcon getSchematicPreviewButtonOnImage() const {
    return m_schematicPreviewButtonOnImage;
  }
  QColor getSchematicPreviewButtonBgOffColor() const {
    return m_schematicPreviewButtonBgOffColor;
  }
  QIcon getSchematicPreviewButtonOffImage() const {
    return m_schematicPreviewButtonOffImage;
  }

  // Schematic Camstand Button
  void setSchematicCamstandButtonBgOnColor(const QColor &color) {
    m_schematicCamstandButtonBgOnColor = color;
  }
  void setSchematicCamstandButtonOnImage(const QIcon &image) {
    m_schematicCamstandButtonOnImage = image;
  }
  void setSchematicCamstandButtonTranspImage(const QIcon &image) {
    m_schematicCamstandButtonTranspImage = image;
  }
  void setSchematicCamstandButtonBgOffColor(const QColor &color) {
    m_schematicCamstandButtonBgOffColor = color;
  }
  void setSchematicCamstandButtonOffImage(const QIcon &image) {
    m_schematicCamstandButtonOffImage = image;
  }
  QColor getSchematicCamstandButtonBgOnColor() const {
    return m_schematicCamstandButtonBgOnColor;
  }
  QIcon getSchematicCamstandButtonOnImage() const {
    return m_schematicCamstandButtonOnImage;
  }
  QIcon getSchematicCamstandButtonTranspImage() const {
    return m_schematicCamstandButtonTranspImage;
  }
  QColor getSchematicCamstandButtonBgOffColor() const {
    return m_schematicCamstandButtonBgOffColor;
  }
  QIcon getSchematicCamstandButtonOffImage() const {
    return m_schematicCamstandButtonOffImage;
  }

  void getNodeColor(int ltype, QColor &nodeColor);

  QColor getSelectedNodeTextColor();

public slots:

  void updateSchematic();

signals:

  void showPreview(TFxP);
  void doCollapse(const QList<TFxP> &);
  void doCollapse(QList<TStageObjectId>);
  void doExplodeChild(const QList<TFxP> &);
  void doExplodeChild(QList<TStageObjectId>);
  void editObject();

protected slots:

  void onSceneChanged();
  void onSceneSwitched();
  void updateScenes();
  void changeNodeSize();

private:
  SchematicSceneViewer *m_viewer;
  StageSchematicScene *m_stageScene;
  FxSchematicScene *m_fxScene;

  TSceneHandle *m_sceneHandle;

  QToolBar *m_stageToolbar, *m_commonToolbar, *m_fxToolbar, *m_swapToolbar;

  QAction *m_fitSchematic, *m_centerOn, *m_reorder, *m_normalize, *m_nodeSize,
      *m_changeScene;

  bool m_fullSchematic, m_maximizedNode;

private:
  void createToolbars();
  void createActions();

  void setStageSchematic();
  void setFxSchematic();
};

#endif  // SCHEMATICVIEWER_H
