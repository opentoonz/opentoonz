#pragma once

#ifndef SCENEVIEWER_H
#define SCENEVIEWER_H

// TnzCore includes
#include "tgeometry.h"
#include "tgl.h"

// TnzLib includes
#include "toonz/imagepainter.h"

// TnzQt includes
#include "toonzqt/menubarcommand.h"
#include "toonzqt/flipconsole.h"
//iwsw commented out temporarily
//#include "toonzqt/ghibli_3dlut_util.h"

// TnzTools includes
#include "tools/tool.h"

// Toonz includes
#include "pane.h"
#include "previewer.h"

// Qt includes
#include <QGLWidget>

//=====================================================================

//  Forward declarations

class Ruler;
class QMenu;
class SceneViewer;

namespace ImageUtils
{
class FullScreenWidget;
}

//=====================================================================

class ToggleCommandHandler : public MenuItemHandler
{
	int m_status;

public:
	ToggleCommandHandler(CommandId id, bool startStatus);

	bool getStatus() const { return m_status; }
	//For reproducing the UI toggle when launch
	void setStatus(bool status) { m_status = status; }
	void execute();
};

//=============================================================================
// SceneViewer
//-----------------------------------------------------------------------------

class SceneViewer : public QGLWidget, public TTool::Viewer, public Previewer::Listener
{
	Q_OBJECT

	qreal m_pressure;
	QPoint m_lastMousePos;
	QPoint m_pos;
	Qt::MouseButton m_mouseButton;

	bool m_foregroundDrawing;
	bool m_tabletEvent;
	//used to handle wrong mouse drag events!
	bool m_buttonClicked;
	bool m_shownOnce = false;
	int m_referenceMode;
	int m_previewMode;
	bool m_isMouseEntered, m_forceGlFlush;

	/*!  FreezedStatus:
*  \li NO_FREEZED freezed is not active;
*  \li NORMAL_FREEZED freezed is active: show grab image;
*  \li UPDATE_FREEZED freezed is active: draw last unfreezed image and grab view;
*/
	enum FreezedStatus {
		NO_FREEZED = 0,
		NORMAL_FREEZED = 1,
		UPDATE_FREEZED = 2,
	} m_freezedStatus;
	TRaster32P m_viewGrabImage;

	int m_FPS;

	ImagePainter::CompareSettings m_compareSettings;
	Ruler *m_hRuler;
	Ruler *m_vRuler;

	TPointD m_pan3D;
	double m_zoomScale3D;
	double m_phi3D;
	double m_theta3D;
	double m_minZ;

	// current pan/zoom matrix (two different matrices are used for editing scenes and leves)
	TAffine m_viewAff[2];
	int m_viewMode;

	TPointD m_dpiScale;

	int m_tableDLId; //To compute table DisplayList only if necessary.

	int m_groupIndexToBeEntered;

	double m_pixelSize;
	bool m_eraserPointerOn;
	QString m_backupTool;
	TRectD m_clipRect;

	bool m_isPicking;

	TRaster32P m_3DSideL;
	TRaster32P m_3DSideR;
	TRaster32P m_3DTop;

	TPointD m_sideRasterPos;
	TPointD m_topRasterPos;
	QString m_toolDisableReason;

	bool m_editPreviewSubCamera;

	enum Device3D {
		NONE,
		SIDE_LEFT_3D,
		SIDE_RIGHT_3D,
		TOP_3D,
	} m_current3DDevice;

	//iwsw commented out temporarily
	//Ghibli3DLutUtil * m_ghibli3DLutUtil;
public:
	//iwsw commented out temporarily
	//Ghibli3DLutUtil* get3DLutUtil(){ return m_ghibli3DLutUtil; }
	enum ReferenceMode {
		NORMAL_REFERENCE = 1,
		CAMERA3D_REFERENCE = 2,
		CAMERA_REFERENCE = 3,
		LEVEL_MODE = 128,
	};

	// Zoom/Pan matrices are selected by ViewMode
	enum ViewMode {
		SCENE_VIEWMODE = 0,
		LEVEL_VIEWMODE = 1
	};

	enum PreviewMode {
		NO_PREVIEW = 0,
		FULL_PREVIEW = 1,
		SUBCAMERA_PREVIEW = 2
	};

	SceneViewer(ImageUtils::FullScreenWidget *parent);
	~SceneViewer();

	double getPixelSize() const { return m_pixelSize; }

	// Previewer::Listener
	TRectD getPreviewRect() const;
	void onRenderStarted(int frame);
	void onRenderCompleted(int frame);
	void onPreviewUpdate();

	void startForegroundDrawing();
	void endForegroundDrawing();
	bool isPreviewEnabled() const { return m_previewMode != NO_PREVIEW; }
	int getPreviewMode() const { return m_previewMode; }

	void setVisual(const ImagePainter::VisualSettings &settings);

	TRect getActualClipRect(const TAffine &aff);

	//! Return the view matrix.
	//! The view matrix is a matrix contained in \b m_viewAff; if the SceneViewer in showing images
	//! in Camera view Mode (m_referenceMode = CAMERA_REFERENCE) the returned affine is composed with camera
	//! transformation.
	TAffine getViewMatrix() const;
	//! Return the view matrix.
	//! The view matrix is a matrix contained in \b m_viewAff
	TAffine getSceneMatrix() const;

	void setViewMatrix(const TAffine &aff, int viewMode);

	int getFPS() { return m_FPS; }

	void setRulers(Ruler *v, Ruler *h)
	{
		m_vRuler = v;
		m_hRuler = h;
	}

	bool is3DView() const;
	TDimension getViewportSize() const { return TDimension(width(), height()); }

	void invalidateAll();
	void GLInvalidateAll();
	void GLInvalidateRect(const TRectD &rect);
	void invalidateToolStatus();

	TPointD getPan3D() const { return m_pan3D; }
	double getZoomScale3D() const { return m_zoomScale3D; }

	double projectToZ(const TPoint &delta);

	TPointD getDpiScale() const { return m_dpiScale; }
	void zoomQt(bool forward, bool reset);
	TAffine getNormalZoomScale();

	bool canSwapCompared() const;

	bool isEditPreviewSubcamera() const { return m_editPreviewSubCamera; }
	void setEditPreviewSubcamera(bool enabled) { m_editPreviewSubCamera = enabled; }

	// panning by dragging the navigator in the levelstrip
	void navigatorPan(const QPoint &delta);
	// a factor for getting pixel-based zoom ratio
	double getDpiFactor();
	// when showing the viewer with full-screen mode,
	// add a zoom factor which can show image fitting with the screen size
	double getZoomScaleFittingWithScreen();
	// return the viewer geometry in order to avoid picking the style outside of the viewer
	// when using the stylepicker and the finger tools
	virtual TRectD getGeometry() const;

	void setFocus(Qt::FocusReason reason) { QWidget::setFocus(reason); };

public:
	//SceneViewer's gadget public functions
	TPointD winToWorld(const QPoint &pos) const;
	TPointD winToWorld(const TPoint &winPos) const;

	TPoint worldToPos(const TPointD &worldPos) const;

protected:
	//Paint vars
	TAffine m_drawCameraAff;
	TAffine m_drawTableAff;
	bool m_draw3DMode;
	bool m_drawCameraTest;
	bool m_drawIsTableVisible;
	bool m_drawEditingLevel;
	TRect m_actualClipRect;

	//Paint methods
	void drawBuildVars();
	void drawEnableScissor();
	void drawDisableScissor();
	void drawBackground();
	void drawCameraStand();
	void drawPreview();
	void drawOverlay();

	void drawScene();
	void drawToolGadgets();

protected:
	void mult3DMatrix();

	void initializeGL();
	void resizeGL(int width, int height);

	void paintGL();

	void showEvent(QShowEvent *);
	void hideEvent(QHideEvent *);

	void tabletEvent(QTabletEvent *);
	void leaveEvent(QEvent *);
	void enterEvent(QEvent *);
	void mouseMoveEvent(QMouseEvent *event);
	void mousePressEvent(QMouseEvent *event);
	void mouseReleaseEvent(QMouseEvent *event);
	void mouseDoubleClickEvent(QMouseEvent *event);
	void wheelEvent(QWheelEvent *);
	void keyPressEvent(QKeyEvent *event);
	void keyReleaseEvent(QKeyEvent *event);
	void contextMenuEvent(QContextMenuEvent *event);
	void inputMethodEvent(QInputMethodEvent *);
	void drawCompared();
	bool event(QEvent *event);

	// delta.x: right panning, pixel; delta.y: down panning, pixel
	void panQt(const QPoint &delta);

	// center: window coordinate, pixels, topleft origin
	void zoomQt(const QPoint &center, double scaleFactor);

	// overriden from TTool::Viewer
	void pan(const TPoint &delta)
	{
		panQt(QPoint(delta.x, delta.y));
	}

	// overriden from TTool::Viewer
	void zoom(const TPointD &center, double factor);

	void rotate(const TPointD &center, double angle);
	void rotate3D(double dPhi, double dTheta);

	TAffine getToolMatrix() const;

	//! return the column index of the drawing intersecting point \b p
	//! (window coordinate, pixels, bottom-left origin)
	int pick(const TPoint &point);

	//! return the column indexes of the drawings intersecting point \b p
	//! (window coordinate, pixels, bottom-left origin)
	int posToColumnIndex(const TPoint &p, double distance, bool includeInvisible = true) const;
	void posToColumnIndexes(const TPoint &p, std::vector<int> &indexes, double distance, bool includeInvisible = true) const;

	//! return the row of the drawings intersecting point \b p (used with onion skins)
	//! (window coordinate, pixels, bottom-left origin)
	int posToRow(const TPoint &p, double distance, bool includeInvisible = true) const;

	void dragEnterEvent(QDragEnterEvent *event);
	void dropEvent(QDropEvent *event);

	void resetInputMethod();

	void set3DLeftSideView();
	void set3DRightSideView();
	void set3DTopView();

	void setFocus() { QWidget::setFocus(); };

public slots:

	void resetSceneViewer();
	void setActualPixelSize();
	void onXsheetChanged();
	void onObjectSwitched();
	// when tool options are changed, update tooltip immediately
	void onToolChanged();
	void onToolSwitched();
	void onSceneChanged();
	void onLevelChanged();
	// when level is switched, update m_dpiScale in order to show white background for Ink&Paint work properly
	void onLevelSwitched();
	void onFrameSwitched();

	void setReferenceMode(int referenceMode);
	void enablePreview(int previewMode);
	void freeze(bool on);

	void onButtonPressed(FlipConsole::EGadget button);
	void fitToCamera();
	void swapCompared();
	void regeneratePreviewFrame();
	void regeneratePreview();

	// delete preview-subcamera executed from context menu
	void doDeleteSubCamera();

signals:

	void onZoomChanged();
	void freezeStateChanged(bool);
	void previewStatusChanged();
	// when pan/zoom on the viewer, notify to level strip in order to update the navigator
	void refreshNavi();
	// for updating the titlebar
	void previewToggled();
};

#endif // SCENEVIEWER_H
