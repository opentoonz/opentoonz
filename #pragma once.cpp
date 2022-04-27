#pragma once
#ifndef ORIENTATION_INCLUDED
#define ORIENTATION_INCLUDED
#undef DVAPI
#undef DVVAR
#ifdef TOONZLIB_EXPORTS
#define DVAPI DV_EXPORT_API
#define DVVAR DV_EXPORT_VAR
#else
#define DVAPI DV_IMPORT_API
#define DVVAR DV_IMPORT_VAR
#endif
#include "tcommon.h"
#include "cellposition.h"
#include "toonz/cellpositionratio.h"
#include <QPoint>
#include <QLine>
#include <QRect>
#include <QPainterPath>
#include <map>
#include <vector>
// Defines timeline direction: top to bottom;  left to right.
// old (vertical timeline) = new (universal)    = old (kept)
//                x        =   layer axis       =   column
//                y        =   frame axis       =    row
// A pair of numbers
class DVAPI NumberRange {
  int _from, _to;  // _from <= _to
  NumberRange() : _from(0), _to(0) {}
public:
  NumberRange(int from, int to)
      : _from(std::min(from, to)), _to(std::max(from, to)) {}
  int from() const { return _from; }
  int to() const { return _to; }
  int length() const { return _to - _from; }
  int middle() const { return (_to + _from) / 2; }
  int weight(double toWeight) const;
  double ratio(int at) const;
  NumberRange adjusted(int addFrom, int addTo) const;
};
class ColumnFan;
class QPixmap;
class QPainterPath;
//! lists predefined rectangle sizes and positions (relative to top left corner
//! of a cell)
enum class PredefinedRect {
  CELL,                     //! size of a cell
  CAMERA_CELL,              //! size of a cell of the camera column
  DRAG_HANDLE_CORNER,       //! area for dragging a cell
  KEY_ICON,                 //! position of key icon
  CAMERA_KEY_ICON,          //! position of key icon in the camera column
  CELL_NAME,                //! cell name box
  CELL_NAME_WITH_KEYFRAME,  //! cell name box when keyframe is displayed
  END_EXTENDER,             //! bottom / right extender
  BEGIN_EXTENDER,           //! top / left extender
  KEYFRAME_AREA,            //! part of cell dedicated to key frames
  DRAG_AREA,                //! draggable side bar
  CELL_MARK_AREA,           //! cell mark
  SOUND_TRACK,              //! area dedicated to waveform display
  PREVIEW_TRACK,            //! sound preview area
  BEGIN_SOUND_EDIT,         //! top sound resize
  END_SOUND_EDIT,           //! bottom sound resize
  NOTE_AREA,                //! size of top left note controls
  NOTE_ICON,                //! size of note icons that appear in cells area
  FRAME_LABEL,              //! area for writing frame number
  FRAME_HEADER,
  LAYER_HEADER,
  FOLDED_LAYER_HEADER,  //! size of layer header when it is folded
  CAMERA_LAYER_HEADER,
  PLAY_RANGE,       //! area for play range marker within frame header
  ONION,            //! onion handle placement
  ONION_DOT,        //! moveable dot placement
  ONION_DOT_FIXED,  //! fixed dot placement
  ONION_AREA,       //! area where mouse events will alter onion
  ONION_FIXED_DOT_AREA,
  ONION_DOT_AREA,
  PINNED_CENTER_KEY,   //! displays a small blue number
  RENAME_COLUMN,       //! where column rename control appears after clicking
  EYE_AREA,            //! clickable area larger than the eye, containing it
  EYE,                 //! the eye itself
  PREVIEW_LAYER_AREA,  //! clickable area larger than preview icon, containing
                       //! it
  PREVIEW_LAYER,
  LOCK_AREA,          //! clickable area larger than lock icon, containing it
  LOCK,               //! the lock icon itself
  CAMERA_LOCK_AREA,   //! lock area for the camera column
  CAMERA_LOCK,        //! the lock icon for camera column
  DRAG_LAYER,         //! draggable area in layer header
  LAYER_NAME,         //! where to display column name. clicking will rename
  CAMERA_LAYER_NAME,  //! where to display the camera column name
  LAYER_NUMBER,       //! where to display column number.
  SOUND_ICON,
  VOLUME_TRACK,        //! area where track is displayed
  VOLUME_AREA,         //! active area for volume control
  LOOP_ICON,           //! area for repeat animation icon
  CAMERA_LOOP_ICON,    //! area for repeat icon in the camera column
  LAYER_HEADER_PANEL,  //! panel displaying headers for the layer rows in
                       //! timeline mode
  THUMBNAIL_AREA,      //! area for header thumbnails and other icons
  THUMBNAIL,           //! the actual thumbnail, if there is one
  CAMERA_ICON_AREA,    //! area for the camera column icon
  CAMERA_ICON,         //! the actual camera column icon
  PEGBAR_NAME,         //! where to display pegbar name
  PARENT_HANDLE_NAME,  //! where to display parent handle number
  FILTER_COLOR,        //! where to show layer's filter color
  CONFIG_AREA,  //! clickable area larger than the config icon, containing it
  CONFIG,       //! the config icon itself
  CAMERA_CONFIG_AREA,        //! config area for the camera column
  CAMERA_CONFIG,             //! the config icon for camera column
  FRAME_MARKER_AREA,         //! Cell's frame indicator
  CAMERA_FRAME_MARKER_AREA,  //! Cell's frame indicator for camera column
  FRAME_INDICATOR,           //! Row # indicator
  ZOOM_SLIDER_AREA,
  ZOOM_SLIDER,
  ZOOM_IN_AREA,
  ZOOM_IN,
  ZOOM_OUT_AREA,
  ZOOM_OUT,
  LAYER_FOOTER_PANEL,
  PREVIEW_FRAME_AREA,
  SHIFTTRACE_DOT,
  SHIFTTRACE_DOT_AREA,
  PANEL_EYE,
  PANEL_PREVIEW_LAYER,
  PANEL_LOCK,
  PANEL_LAYER_NAME,
  // ADD_LEVEL_AREA,
  // ADD_LEVEL,
  FOOTER_NOTE_OBJ_AREA,
  FOOTER_NOTE_AREA
  FOOTER_NOTE_AREA,
  NAVIGATION_TAG_AREA
};
enum class PredefinedLine {
  LOCKED,              //! dotted vertical line when cell is locked
@@ -169,7 +170,8 @@ enum class PredefinedPath {
  VOLUME_SLIDER_TRACK,  //! slider track
  VOLUME_SLIDER_HEAD,   //! slider head
  TIME_INDICATOR_HEAD,  //! current time indicator head
  FRAME_MARKER_DIAMOND
  FRAME_MARKER_DIAMOND,
  NAVIGATION_TAG
};
enum class PredefinedPoint {
  KEY_HIDDEN,  //! move extender handle that much if key icons are disabled
 71  
toonz/sources/include/toonz/navigationtags.h
Viewed
@@ -0,0 +1,71 @@
#pragma once

#ifndef NAVIGATION_TAGS_INCLUDED
#define NAVIGATION_TAGS_INCLUDED

#include "tcommon.h"

#undef DVAPI
#undef DVVAR
#ifdef TOONZLIB_EXPORTS
#define DVAPI DV_EXPORT_API
#define DVVAR DV_EXPORT_VAR
#else
#define DVAPI DV_IMPORT_API
#define DVVAR DV_IMPORT_VAR
#endif

#include <QString>
#include <QColor>

class TOStream;
class TIStream;

class DVAPI NavigationTags {
public:
  struct Tag {
    int m_frame;
    QString m_label;
    QColor m_color;
    Tag() : m_frame(-1), m_label(), m_color(Qt::magenta) {}
    Tag(int frame) : m_frame(frame), m_label(), m_color(Qt::magenta) {}
    Tag(int frame, QString label)
        : m_frame(frame), m_label(label), m_color(Qt::magenta) {}
    Tag(int frame, QString label, QColor color)
        : m_frame(frame), m_label(label), m_color(color) {}
    ~Tag() {}

    bool operator<(const Tag &otherTag) const {
      return (m_frame < otherTag.m_frame);
    }
  };
  std::vector<Tag> m_tags;

  NavigationTags() {}
  ~NavigationTags() {}

  std::vector<Tag> getTags() { return m_tags; }

  int getCount() const;

  Tag getTag(int frame);
  void addTag(int frame, QString label = "");
  void removeTag(int frame);
  void clearTags();
  bool isTagged(int frame);
  int getPrevTag(int currentFrame);
  int getNextTag(int currentFrame);
  void moveTag(int fromFrame, int toFrame);
  void shiftTags(int startFrame, int shift);

  QString getTagLabel(int frame);
  void setTagLabel(int frame, QString label);

  QColor getTagColor(int frame);
  void setTagColor(int frame, QColor color);

  void saveData(TOStream &os);
  void loadData(TIStream &is);
};

#endif
  6  
toonz/sources/include/toonz/txsheet.h
Viewed
@@ -53,6 +53,7 @@ class TFrameId;
class Orientation;
class TXsheetColumnChangeObserver;
class ExpressionReferenceMonitor;
class NavigationTags;

//=============================================================================

@@ -158,6 +159,7 @@ class DVAPI TXsheet final : public TSmartObject, public TPersist {
  std::unique_ptr<TXsheetImp> m_imp;
  TXshNoteSet *m_notes;
  SoundProperties *m_soundProperties;
  NavigationTags *m_navigationTags;

  int m_cameraColumnIndex;
  TXshColumn *m_cameraColumn;
@@ -593,6 +595,10 @@ in TXsheetImp.
  void convertToImplicitHolds();
  void convertToExplicitHolds();

  NavigationTags *getNavigationTags() const { return m_navigationTags; }
  bool isFrameTagged(int frame) const;
  void toggleTaggedFrame(int frame);

protected:
  bool checkCircularReferences(TXsheet *childCandidate);

  2  
toonz/sources/toonz/CMakeLists.txt
Viewed
@@ -133,6 +133,7 @@ set(MOC_HEADERS
    ../stopmotion/stopmotionserial.h
    ../stopmotion/stopmotionlight.h
    cameracapturelevelcontrol.h
    navtageditorpopup.h
)

set(HEADERS
@@ -369,6 +370,7 @@ set(SOURCES
    ../stopmotion/stopmotionserial.cpp
    ../stopmotion/stopmotionlight.cpp
	cameracapturelevelcontrol.cpp
    navtageditorpopup.cpp
)

if(WITH_TRANSLATION)
 77  
toonz/sources/toonz/icons/dark/actions/16/next_nav_tag.svg
Viewed

 83  
toonz/sources/toonz/icons/dark/actions/16/prev_nav_tag.svg
Viewed

 66  
toonz/sources/toonz/icons/dark/actions/16/toggle_nav_tag.svg
Viewed

  13  
toonz/sources/toonz/mainwindow.cpp
Viewed
@@ -2040,6 +2040,19 @@ void MainWindow::defineActions() {
  createMenuXsheetAction(MI_SetAutoMarkers, QT_TR_NOOP("Set Auto Markers"), "");
  createMenuXsheetAction(MI_PreviewThis,
                         QT_TR_NOOP("Set Markers to Current Frame"), "");
  createMenuXsheetAction(MI_ToggleTaggedFrame,
                         QT_TR_NOOP("Toggle Navigation Tag"), "",
                         "toggle_nav_tag");
  createMenuXsheetAction(MI_NextTaggedFrame, QT_TR_NOOP("Next Tag"), "",
                         "next_nav_tag");
  createMenuXsheetAction(MI_PrevTaggedFrame, QT_TR_NOOP("Previous Tag"), "",
                         "prev_nav_tag");
  createMenuXsheetAction(MI_EditTaggedFrame, QT_TR_NOOP("Edit Tag"), "", "");
  createMenuXsheetAction(MI_ClearTags, QT_TR_NOOP("Remove Tags"), "", "");
  CommandManager::instance()->enable(MI_NextTaggedFrame, false);
  CommandManager::instance()->enable(MI_PrevTaggedFrame, false);
  CommandManager::instance()->enable(MI_EditTaggedFrame, false);
  CommandManager::instance()->enable(MI_ClearTags, false);

  // Menu - Cells

  7  
toonz/sources/toonz/menubarcommandids.h
Viewed
@@ -485,4 +485,11 @@

#define MI_ToggleImplicitHold "MI_ToggleImplicitHold"

// Navigation tags
#define MI_ToggleTaggedFrame "MI_ToggleTaggedFrame"
#define MI_EditTaggedFrame "MI_EditTaggedFrame"
#define MI_NextTaggedFrame "MI_NextTaggedFrame"
#define MI_PrevTaggedFrame "MI_PrevTaggedFrame"
#define MI_ClearTags "MI_ClearTags"

#endif
 104  
toonz/sources/toonz/navtageditorpopup.cpp
Viewed
@@ -0,0 +1,104 @@
#include "navtageditorpopup.h"

#include "../toonz/tapp.h"

#include <QPushButton>
#include <QMainWindow>
#include <QColor>

namespace {
const QIcon getColorChipIcon(QColor color) {
  QPixmap pixmap(12, 12);
  pixmap.fill(color);
  return QIcon(pixmap);
}
}

void NavTagEditorPopup::accept() {
  m_label = QString(m_labelFld->text());

  Dialog::accept();
}

NavTagEditorPopup::NavTagEditorPopup(int frame, QString label, QColor color)
    : Dialog(TApp::instance()->getMainWindow(), true, true, "Edit Tag")
    , m_label(label)
    , m_color(color) {
  bool ret = true;

  setWindowTitle(tr("Edit Tag"));

  m_labelFld = new DVGui::LineEdit(m_label);
  m_labelFld->setMaximumHeight(DVGui::WidgetHeight);
  addWidget(tr("Frame %1 Label:").arg(frame + 1), m_labelFld);

  m_colorCB = new QComboBox(this);
  m_colorCB->addItem(getColorChipIcon(Qt::magenta), tr("Magenta"),
                     TagColors::Magenta);
  m_colorCB->addItem(getColorChipIcon(Qt::red), tr("Red"), TagColors::Red);
  m_colorCB->addItem(getColorChipIcon(Qt::green), tr("Green"),
                     TagColors::Green);
  m_colorCB->addItem(getColorChipIcon(Qt::blue), tr("Blue"), TagColors::Blue);
  m_colorCB->addItem(getColorChipIcon(Qt::yellow), tr("Yellow"),
                     TagColors::Yellow);
  m_colorCB->addItem(getColorChipIcon(Qt::cyan), tr("Cyan"), TagColors::Cyan);
  m_colorCB->addItem(getColorChipIcon(Qt::white), tr("White"),
                     TagColors::White);
  addWidget(tr("Color:"), m_colorCB);

  if (color == Qt::magenta)
    m_colorCB->setCurrentIndex(0);
  else if (color == Qt::red)
    m_colorCB->setCurrentIndex(1);
  else if (color == Qt::green)
    m_colorCB->setCurrentIndex(2);
  else if (color == Qt::blue)
    m_colorCB->setCurrentIndex(3);
  else if (color == Qt::yellow)
    m_colorCB->setCurrentIndex(4);
  else if (color == Qt::cyan)
    m_colorCB->setCurrentIndex(5);
  else if (color == Qt::white)
    m_colorCB->setCurrentIndex(6);

  ret = ret &&
        connect(m_labelFld, SIGNAL(editingFinished()), SLOT(onLabelChanged()));
  ret = ret &&
        connect(m_colorCB, SIGNAL(activated(int)), SLOT(onColorChanged(int)));

  QPushButton *okBtn = new QPushButton(tr("Ok"), this);
  okBtn->setDefault(true);
  QPushButton *cancelBtn = new QPushButton(tr("Cancel"), this);
  connect(okBtn, SIGNAL(clicked()), this, SLOT(accept()));
  connect(cancelBtn, SIGNAL(clicked()), this, SLOT(reject()));

  addButtonBarWidget(okBtn, cancelBtn);
}

void NavTagEditorPopup::onColorChanged(int index) {
  QColor color;

  switch (index) {
  case TagColors::Red:
    color = Qt::red;
    break;
  case TagColors::Green:
    color = Qt::green;
    break;
  case TagColors::Blue:
    color = Qt::blue;
    break;
  case TagColors::Yellow:
    color = Qt::yellow;
    break;
  case TagColors::Cyan:
    color = Qt::cyan;
    break;
  case TagColors::Magenta:
  default:
    color = Qt::magenta;
    break;
  }

  m_color = color;
}
 41  
toonz/sources/toonz/navtageditorpopup.h
Viewed
@@ -0,0 +1,41 @@
#pragma once

#ifndef TAGEDITORPOPUP_INCLUDED
#define TAGEDITORPOPUP_INCLUDED

#include "tcommon.h"
#include "toonzqt/dvdialog.h"
#include "toonzqt/lineedit.h"

#include <QString>
#include <QColor>
#include <QComboBox>

class NavTagEditorPopup final : public DVGui::Dialog {
  Q_OBJECT

  DVGui::LineEdit *m_labelFld;
  QComboBox *m_colorCB;

  QString m_label;
  QColor m_color;

public:
  enum TagColors { Magenta = 0, Red, Green, Blue, Yellow, Cyan, White };

public:
  NavTagEditorPopup(int frame, QString label, QColor color);

  void accept() override;

  QString getLabel() { return m_label; }

  QColor getColor() { return m_color; }

protected slots:

  void onLabelChanged() {}
  void onColorChanged(int);
};

#endif
  4  
toonz/sources/toonz/toonz.qrc
Viewed
@@ -384,6 +384,10 @@
		<file>icons/dark/actions/30/sound_header_on.svg</file>
		<file>icons/dark/actions/74/notelevel.svg</file>

		<file>icons/dark/actions/16/toggle_nav_tag.svg</file>
		<file>icons/dark/actions/16/next_nav_tag.svg</file>
		<file>icons/dark/actions/16/prev_nav_tag.svg</file>

		<!-- Modes, Types, Options -->
		<file>icons/dark/actions/16/ink_check.svg</file>
		<file>icons/dark/actions/16/inks_only.svg</file>
  118  
toonz/sources/toonz/xsheetcmd.cpp
Viewed
@@ -43,6 +43,7 @@
#include "toonz/scenefx.h"
#include "toonz/preferences.h"
#include "toonz/txshlevelcolumn.h"
#include "toonz/navigationtags.h"

// TnzQt includes
#include "toonzqt/tselectionhandle.h"
@@ -62,6 +63,7 @@
#include "menubarcommandids.h"
#include "columncommand.h"
#include "xshcellviewer.h"  // SetCellMarkUndo
#include "navtageditorpopup.h"

// Qt includes
#include <QClipboard>
@@ -181,6 +183,8 @@ void InsertSceneFrameUndo::doInsertSceneFrame(int frame) {
    if (TStageObject *obj = xsh->getStageObject(objectId))
      insertFrame(obj, frame);
  }

  xsh->getNavigationTags()->shiftTags(frame, 1);
}

//-----------------------------------------------------------------------------
@@ -204,6 +208,9 @@ void InsertSceneFrameUndo::doRemoveSceneFrame(int frame) {
    if (TStageObject *pegbar = xsh->getStageObject(objectId))
      removeFrame(pegbar, frame);
  }

  if (xsh->isFrameTagged(frame)) xsh->getNavigationTags()->removeTag(frame);
  xsh->getNavigationTags()->shiftTags(frame, -1);
}

//-----------------------------------------------------------------------------
@@ -317,6 +324,7 @@ class ToggleImplicitHoldCommand final : public MenuItemHandler {
class RemoveSceneFrameUndo final : public InsertSceneFrameUndo {
  std::vector<TXshCell> m_cells;
  std::vector<TStageObject::Keyframe> m_keyframes;
  NavigationTags::Tag m_tag;

public:
  RemoveSceneFrameUndo(int frame) : InsertSceneFrameUndo(frame) {
@@ -327,6 +335,7 @@ class RemoveSceneFrameUndo final : public InsertSceneFrameUndo {

    m_cells.resize(colsCount);
    m_keyframes.resize(colsCount + 1);
    m_tag = xsh->getNavigationTags()->getTag(frame);

    // Inserting the eventual camera keyframe at the end
    TStageObject *cameraObj = xsh->getStageObject(
@@ -372,6 +381,10 @@ class RemoveSceneFrameUndo final : public InsertSceneFrameUndo {
      }
    }

    // Restore tag if there was one
    if (m_tag.m_frame != -1)
      xsh->getNavigationTags()->addTag(m_tag.m_frame, m_tag.m_label);

    TApp::instance()->getCurrentScene()->setDirtyFlag(true);
    TApp::instance()->getCurrentXsheet()->notifyXsheetChanged();
  }
@@ -2711,6 +2724,107 @@ class PreviewThis final : public MenuItemHandler {
        XsheetGUI::getPlayRange(r0, r1, step);
        XsheetGUI::setPlayRange(row, row, step);
        TApp::instance()->getCurrentXsheetViewer()->update();

    }
} PreviewThis; 
} PreviewThis;

//============================================================

class ToggleTaggedFrame final : public MenuItemHandler {
public:
  ToggleTaggedFrame() : MenuItemHandler(MI_ToggleTaggedFrame) {}
  void execute() override {
    TApp *app = TApp::instance();
    int frame = app->getCurrentFrame()->getFrame();
    assert(frame >= 0);
    TXsheet *xsh = app->getCurrentXsheet()->getXsheet();

    xsh->toggleTaggedFrame(frame);

    NavigationTags *navTags = xsh->getNavigationTags();
    CommandManager::instance()->enable(MI_EditTaggedFrame,
                                       navTags->isTagged(frame));
    CommandManager::instance()->enable(MI_ClearTags, (navTags->getCount() > 0));

    TApp::instance()->getCurrentXsheetViewer()->update();
  }
} ToggleTaggedFrame;

//============================================================

class EditTaggedFrame final : public MenuItemHandler {
public:
  EditTaggedFrame() : MenuItemHandler(MI_EditTaggedFrame) {}
  void execute() override {
    TApp *app = TApp::instance();
    int frame = app->getCurrentFrame()->getFrame();
    assert(frame >= 0);
    TXsheet *xsh = app->getCurrentXsheet()->getXsheet();

    NavigationTags *tags = xsh->getNavigationTags();
    QString label        = tags->getTagLabel(frame);
    QColor color         = tags->getTagColor(frame);
    NavTagEditorPopup navTagEditor(frame, label, color);
    if (navTagEditor.exec() != QDialog::Accepted) return;
    tags->setTagLabel(frame, navTagEditor.getLabel());
    tags->setTagColor(frame, navTagEditor.getColor());
  }
} EditTaggedFrame;

//============================================================

class NextTaggedFrame final : public MenuItemHandler {
public:
  NextTaggedFrame() : MenuItemHandler(MI_NextTaggedFrame) {}
  void execute() override {
    TApp *app = TApp::instance();
    int frame = app->getCurrentFrame()->getFrame();
    assert(frame >= 0);
    TXsheet *xsh = app->getCurrentXsheet()->getXsheet();

    NavigationTags *navTags = xsh->getNavigationTags();
    int nextFrame           = navTags->getNextTag(frame);
    if (nextFrame != -1)
      app->getCurrentXsheetViewer()->setCurrentRow(nextFrame);
  }
} NextTaggedFrame;

//============================================================

class PrevTaggedFrame final : public MenuItemHandler {
public:
  PrevTaggedFrame() : MenuItemHandler(MI_PrevTaggedFrame) {}
  void execute() override {
    TApp *app = TApp::instance();
    int frame = app->getCurrentFrame()->getFrame();
    assert(frame >= 0);
    TXsheet *xsh = app->getCurrentXsheet()->getXsheet();

    NavigationTags *navTags = xsh->getNavigationTags();
    int prevFrame           = navTags->getPrevTag(frame);
    if (prevFrame != -1)
      app->getCurrentXsheetViewer()->setCurrentRow(prevFrame);
  }
} PrevTaggedFrame;

//============================================================

class ClearTags final : public MenuItemHandler {
public:
  ClearTags() : MenuItemHandler(MI_ClearTags) {}
  void execute() override {
    TApp *app = TApp::instance();
    int frame = app->getCurrentFrame()->getFrame();
    assert(frame >= 0);
    TXsheet *xsh = app->getCurrentXsheet()->getXsheet();

    NavigationTags *navTags = xsh->getNavigationTags();
    navTags->clearTags();

    CommandManager::instance()->enable(MI_NextTaggedFrame, false);
    CommandManager::instance()->enable(MI_PrevTaggedFrame, false);
    CommandManager::instance()->enable(MI_EditTaggedFrame, false);
    CommandManager::instance()->enable(MI_ClearTags, false);

    TApp::instance()->getCurrentXsheetViewer()->update();
  }
} ClearTags;
  47  
toonz/sources/toonz/xsheetdragtool.cpp
Viewed
@@ -50,6 +50,7 @@
#include "toutputproperties.h"
#include "toonz/preferences.h"
#include "toonz/columnfan.h"
#include "toonz/navigationtags.h"

// TnzBase includes
#include "tfx.h"
@@ -2207,3 +2208,49 @@ XsheetGUI::DragTool *XsheetGUI::DragTool::makeDragAndDropDataTool(
    XsheetViewer *viewer) {
  return new DataDragTool(viewer);
}

//=============================================================================
//  NavigationTagDragTool
//-----------------------------------------------------------------------------

namespace {

class NavigationTagDragTool final : public XsheetGUI::DragTool {
  int m_taggedRow;

public:
  NavigationTagDragTool(XsheetViewer *viewer) : DragTool(viewer) {}

  void onClick(const CellPosition &pos) override {
    int row = pos.frame();
    m_taggedRow = row;
    refreshRowsArea();
  }

  void onDrag(const CellPosition &pos) override {
    int row          = pos.frame();
    if (row < 0) row = 0;
    onRowChange(row);
    refreshRowsArea();
  }

  void onRowChange(int row) {
    if (row < 0) return;

    TXsheet *xsh            = TApp::instance()->getCurrentXsheet()->getXsheet();
    NavigationTags *navTags = xsh->getNavigationTags();

    if (m_taggedRow == row || navTags->isTagged(row)) return;

    navTags->moveTag(m_taggedRow, row);
    m_taggedRow = row;
  }
};
//-----------------------------------------------------------------------------
}  // namespace
//-----------------------------------------------------------------------------

XsheetGUI::DragTool *XsheetGUI::DragTool::makeNavigationTagDragTool(
    XsheetViewer *viewer) {
  return new NavigationTagDragTool(viewer);
}
  2  
toonz/sources/toonz/xsheetdragtool.h
Viewed
@@ -69,6 +69,8 @@ class DragTool {
  static DragTool *makeColumnLinkTool(XsheetViewer *viewer);
  static DragTool *makeColumnMoveTool(XsheetViewer *viewer);
  static DragTool *makeVolumeDragTool(XsheetViewer *viewer);

  static DragTool *makeNavigationTagDragTool(XsheetViewer *viewer);
};

void setPlayRange(int r0, int r1, int step, bool withUndo = true);
  23  
toonz/sources/toonz/xsheetviewer.cpp
Viewed
@@ -35,6 +35,7 @@
#include "toonz/txshlevelhandle.h"
#include "toonz/tproject.h"
#include "tconvert.h"
#include "toonz/navigationtags.h"

#include "tenv.h"

@@ -1408,6 +1409,18 @@ void XsheetViewer::onSceneSwitched() {
void XsheetViewer::onXsheetChanged() {
  refreshContentSize(0, 0);
  updateAllAree();

  int row                 = TApp::instance()->getCurrentFrame()->getFrame();
  TXsheet *xsh            = getXsheet();
  NavigationTags *navTags = xsh->getNavigationTags();
  int lastTag             = navTags->getPrevTag(INT_MAX);
  int firstTag            = navTags->getNextTag(-1);
  CommandManager::instance()->enable(MI_NextTaggedFrame, (row < lastTag));
  CommandManager::instance()->enable(MI_PrevTaggedFrame,
                                     firstTag != -1 && row > firstTag);
  CommandManager::instance()->enable(MI_EditTaggedFrame,
                                     navTags->isTagged(row));
  CommandManager::instance()->enable(MI_ClearTags, (navTags->getCount() > 0));
}

//-----------------------------------------------------------------------------
@@ -1432,6 +1445,16 @@ void XsheetViewer::onCurrentFrameSwitched() {
  }
  m_isCurrentFrameSwitched = false;
  scrollToRow(row);

  TXsheet *xsh = getXsheet();
  NavigationTags *navTags = xsh->getNavigationTags();
  int lastTag             = navTags->getPrevTag(INT_MAX);
  int firstTag            = navTags->getNextTag(-1);
  CommandManager::instance()->enable(MI_NextTaggedFrame, (row < lastTag));
  CommandManager::instance()->enable(MI_PrevTaggedFrame,
                                     firstTag != -1 && row > firstTag);
  CommandManager::instance()->enable(MI_EditTaggedFrame,
                                     navTags->isTagged(row));
}

//-----------------------------------------------------------------------------
  99  
toonz/sources/toonz/xshrowviewer.cpp
Viewed
@@ -26,6 +26,7 @@
#include "tools/toolcommandids.h"
#include "toonz/tstageobject.h"
#include "toonz/tpinnedrangeset.h"
#include "toonz/navigationtags.h"

#include <QPainter>
#include <QMouseEvent>
@@ -387,6 +388,50 @@ void RowArea::drawStopMotionCameraIndicator(QPainter &p) {

//-----------------------------------------------------------------------------

void RowArea::drawNavigationTags(QPainter &p, int r0, int r1) {
  TApp *app    = TApp::instance();
  TXsheet *xsh = app->getCurrentScene()->getScene()->getXsheet();
  assert(xsh);

  QPoint frameAdj = m_viewer->getFrameZoomAdjustment();

  NavigationTags *tags = xsh->getNavigationTags();

  for (int r = r0; r <= r1; r++) {
    if (!xsh->isFrameTagged(r)) continue;

    QPoint topLeft = m_viewer->positionToXY(CellPosition(r, -1));
    if (!m_viewer->orientation()->isVerticalTimeline())
      topLeft.setY(0);
    else
      topLeft.setX(0);

    QRect tagRect = m_viewer->orientation()
                        ->rect(PredefinedRect::NAVIGATION_TAG_AREA)
                        .translated(topLeft)
                        .translated(-frameAdj / 2);

    int frameMid, frameTop;
    if (m_viewer->orientation()->isVerticalTimeline()) {
      frameMid = tagRect.left() - 3;
      frameTop = tagRect.top() + (tagRect.height() / 2);
    } else {
      frameMid = tagRect.left() + (tagRect.height() / 2) - 3;
      frameTop = tagRect.top() + 1;
    }

    QPainterPath tag = m_viewer->orientation()
                           ->path(PredefinedPath::NAVIGATION_TAG)
                           .translated(QPoint(frameMid, frameTop));
    p.setPen(Qt::black);
    p.setBrush(tags->getTagColor(r));
    p.drawPath(tag);
    p.setBrush(Qt::NoBrush);
  }
}

//-----------------------------------------------------------------------------

void RowArea::drawOnionSkinBackground(QPainter &p, int r0, int r1) {
  const Orientation *o = m_viewer->orientation();

@@ -886,6 +931,8 @@ void RowArea::paintEvent(QPaintEvent *event) {
      !m_viewer->orientation()->isVerticalTimeline())
    drawCurrentTimeLine(p);

  drawNavigationTags(p, r0, r1);

  drawRows(p, r0, r1);

  if (TApp::instance()->getCurrentFrame()->isEditingScene()) {
@@ -993,7 +1040,13 @@ void RowArea::mousePressEvent(QMouseEvent *event) {
        playR0       = 0;
      }

      if (playR1 == -1) {  // getFrameCount = 0 i.e. xsheet is empty
      if (xsh->getNavigationTags()->isTagged(row) &&
          o->rect(PredefinedRect::NAVIGATION_TAG_AREA)
              .adjusted(0, 0, -frameAdj.x(), -frameAdj.y())
              .contains(mouseInCell)) {
        setDragTool(XsheetGUI::DragTool::makeNavigationTagDragTool(m_viewer));
        frameAreaIsClicked = true;
      } else if (playR1 == -1) {  // getFrameCount = 0 i.e. xsheet is empty
        setDragTool(
            XsheetGUI::DragTool::makeCurrentFrameModifierTool(m_viewer));
        frameAreaIsClicked = true;
@@ -1180,7 +1233,14 @@ void RowArea::mouseMoveEvent(QMouseEvent *event) {
    m_tooltip = tr("Pinned Center : Col%1%2")
                    .arg(pinnedCenterColumnId + 1)
                    .arg((isRootBonePinned) ? " (Root)" : "");
  else if (row == currentRow) {
  else if (o->rect(PredefinedRect::NAVIGATION_TAG_AREA)
               .adjusted(0, 0, -frameAdj.x(), -frameAdj.y())
               .contains(mouseInCell)) {
    TXsheet *xsh = m_viewer->getXsheet();
    QString label = xsh->getNavigationTags()->getTagLabel(m_row);
    if (label.isEmpty()) label = "-";
    if (xsh->isFrameTagged(m_row)) m_tooltip = tr("Tag: %1").arg(label);
  } else if (row == currentRow) {
    if (Preferences::instance()->isOnionSkinEnabled() &&
        o->rect(PredefinedRect::ONION)
            .translated(-frameAdj / 2)
@@ -1265,6 +1325,34 @@ void RowArea::contextMenuEvent(QContextMenuEvent *event) {
  menu->addAction(cmdManager->getAction(MI_NoShift));
  menu->addAction(cmdManager->getAction(MI_ResetShift));

  // Tags
  menu->addSeparator();
  menu->addAction(cmdManager->getAction(MI_ToggleTaggedFrame));
  menu->addAction(cmdManager->getAction(MI_EditTaggedFrame));

  QMenu *tagMenu          = menu->addMenu(tr("Tags"));
  NavigationTags *navTags = m_viewer->getXsheet()->getNavigationTags();
  QAction *tagAction;
  if (!navTags->getCount()) {
    tagAction = tagMenu->addAction("Empty");
    tagAction->setEnabled(false);
  } else {
    std::vector<NavigationTags::Tag> tags = navTags->getTags();
    for (int i = 0; i < tags.size(); i++) {
      int frame     = tags[i].m_frame;
      QString label = tr("Frame %1").arg(frame + 1);
      if (!tags[i].m_label.isEmpty()) label += ": " + tags[i].m_label;
      tagAction = tagMenu->addAction(label);
      tagAction->setData(frame);
      connect(tagAction, SIGNAL(triggered()), this, SLOT(onJumpToTag()));
    }
    tagMenu->addSeparator();
    tagMenu->addAction(cmdManager->getAction(MI_ClearTags));
  }

  menu->addAction(cmdManager->getAction(MI_NextTaggedFrame));
  menu->addAction(cmdManager->getAction(MI_PrevTaggedFrame));

  menu->exec(event->globalPos());
}

@@ -1320,4 +1408,11 @@ bool RowArea::event(QEvent *event) {

//-----------------------------------------------------------------------------

void RowArea::onJumpToTag() {
  QAction *senderAction = qobject_cast<QAction *>(sender());
  assert(senderAction);
  int frame = senderAction->data().toInt();
  m_viewer->setCurrentRow(frame);
}

}  // namespace XsheetGUI
  3  
toonz/sources/toonz/xshrowviewer.h
Viewed
@@ -55,6 +55,7 @@ class RowArea final : public QWidget {
  void drawCurrentTimeLine(QPainter &p);
  void drawShiftTraceMarker(QPainter &p);
  void drawStopMotionCameraIndicator(QPainter &p);
  void drawNavigationTags(QPainter &p, int r0, int r1);

  DragTool *getDragTool() const;
  void setDragTool(DragTool *dragTool);
@@ -80,6 +81,8 @@ class RowArea final : public QWidget {
  void mouseDoubleClickEvent(QMouseEvent *event) override;
  bool event(QEvent *event) override;

protected slots:
  void onJumpToTag();
};

}  // namespace XsheetGUI;
  2  
toonz/sources/toonzlib/CMakeLists.txt
Viewed
@@ -163,6 +163,7 @@ set(HEADERS
    ../include/toonz/txsheetcolumnchange.h
    ../include/toonz/expressionreferencemonitor.h
    ../include/toonz/filepathproperties.h
    ../include/toonz/navigationtags.h
)

set(SOURCES
@@ -322,6 +323,7 @@ set(SOURCES
    textureutils.cpp
    boardsettings.cpp
    filepathproperties.cpp
    navigationtags.cpp
)

if(BUILD_TARGET_WIN)
 215  
toonz/sources/toonzlib/navigationtags.cpp
Viewed
@@ -0,0 +1,215 @@
#include "toonz/navigationtags.h"

#include "tstream.h"
#include "texception.h"

#ifndef _WIN32
#include <limits.h>
#endif

//-----------------------------------------------------------------------------

int NavigationTags::getCount() const {
  if (m_tags.empty()) return 0;
  return m_tags.size();
}

//-----------------------------------------------------------------------------

NavigationTags::Tag NavigationTags::getTag(int frame) {
  for (int i = 0; i < m_tags.size(); i++)
    if (m_tags[i].m_frame == frame) return m_tags[i];

  return Tag();
}

//-----------------------------------------------------------------------------

void NavigationTags::addTag(int frame, QString label) {
  if (frame < 0 || isTagged(frame)) return;

  m_tags.push_back(Tag(frame, label));

  std::sort(m_tags.begin(), m_tags.end());
}

//-----------------------------------------------------------------------------

void NavigationTags::removeTag(int frame) {
  if (frame < 0) return;

  Tag tag = getTag(frame);
  if (tag.m_frame == -1) return;

  std::vector<Tag>::iterator it;
  for (it = m_tags.begin(); it != m_tags.end(); it++)
    if (it->m_frame == frame) {
      m_tags.erase(it);
      break;
    }
}

//-----------------------------------------------------------------------------

void NavigationTags::clearTags() { m_tags.clear(); }

//-----------------------------------------------------------------------------

bool NavigationTags::isTagged(int frame) {
  if (frame < 0) return false;

  for (int i = 0; i < m_tags.size(); i++)
    if (m_tags[i].m_frame == frame) return true;

  return false;
}

//-----------------------------------------------------------------------------

void NavigationTags::moveTag(int fromFrame, int toFrame) {
  if (fromFrame < 0 || toFrame < 0 || isTagged(toFrame)) return;

  for (int i = 0; i < m_tags.size(); i++)
    if (m_tags[i].m_frame == fromFrame) {
      m_tags[i].m_frame = toFrame;
      std::sort(m_tags.begin(), m_tags.end());
      break;
    }
}

//-----------------------------------------------------------------------------

// WARNING: When shifting left, shiftTag callers MUST take care of shifting tags
// to frame < 0 or handle possible frame collisions.  This will not do it
// for you!
void NavigationTags::shiftTags(int startFrame, int shift) {
  if (!m_tags.size()) return;

  for (int i = 0; i < m_tags.size(); i++) {
    if (m_tags[i].m_frame < startFrame) continue;
    m_tags[i].m_frame += shift;
  }
}

//-----------------------------------------------------------------------------

int NavigationTags::getPrevTag(int currentFrame) {
  if (currentFrame < 0) return -1;

  int index        = -1;
  int closestFrame = -1;
  for (int i = 0; i < m_tags.size(); i++)
    if (m_tags[i].m_frame < currentFrame && m_tags[i].m_frame > closestFrame) {
      index        = i;
      closestFrame = m_tags[i].m_frame;
    }

  return index >= 0 ? m_tags[index].m_frame : -1;
}

//-----------------------------------------------------------------------------

int NavigationTags::getNextTag(int currentFrame) {
  int index        = -1;
  int closestFrame = INT_MAX;
  for (int i = 0; i < m_tags.size(); i++)
    if (m_tags[i].m_frame > currentFrame && m_tags[i].m_frame < closestFrame) {
      index        = i;
      closestFrame = m_tags[i].m_frame;
    }

  return index >= 0 ? m_tags[index].m_frame : -1;
}

//-----------------------------------------------------------------------------

QString NavigationTags::getTagLabel(int frame) {
  Tag tag = getTag(frame);

  return tag.m_label;
}

//-----------------------------------------------------------------------------

void NavigationTags::setTagLabel(int frame, QString label) {
  if (frame < 0) return;

  for (int i = 0; i < m_tags.size(); i++)
    if (m_tags[i].m_frame == frame) {
      m_tags[i].m_label = label;
      break;
    }
}

//-----------------------------------------------------------------------------

QColor NavigationTags::getTagColor(int frame) {
  Tag tag = getTag(frame);

  return tag.m_color;
}

//-----------------------------------------------------------------------------

void NavigationTags::setTagColor(int frame, QColor color) {
  if (frame < 0) return;

  for (int i = 0; i < m_tags.size(); i++)
    if (m_tags[i].m_frame == frame) {
      m_tags[i].m_color = color;
      break;
    }
}

//-----------------------------------------------------------------------------

void NavigationTags::saveData(TOStream &os) {
  int i;
  os.openChild("Tags");
  for (i = 0; i < getCount(); i++) {
    os.openChild("tag");
    Tag tag = m_tags.at(i);
    os << tag.m_frame;
    os << tag.m_label;
    os << tag.m_color.red();
    os << tag.m_color.green();
    os << tag.m_color.blue();
    os.closeChild();
  }
  os.closeChild();
}

//-----------------------------------------------------------------------------

void NavigationTags::loadData(TIStream &is) {
  while (!is.eos()) {
    std::string tagName;
    if (is.matchTag(tagName)) {
      if (tagName == "Tags") {
        while (!is.eos()) {
          std::string tagName;
          if (is.matchTag(tagName)) {
            if (tagName == "tag") {
              Tag tag;
              is >> tag.m_frame;
              std::wstring text;
              is >> text;
              tag.m_label = QString::fromStdWString(text);
              int r, g, b;
              is >> r;
              is >> g;
              is >> b;
              tag.m_color = QColor(r, g, b);
              m_tags.push_back(tag);
            }
          } else
            throw TException("expected <tag>");
          is.closeChild();
        }
      } else
        throw TException("expected <Tags>");
      is.closeChild();
    } else
      throw TException("expected tag");
  }
}
  29  
toonz/sources/toonzlib/orientation.cpp
Viewed
@@ -12,7 +12,9 @@ const int KEY_ICON_WIDTH      = 11;
const int KEY_ICON_HEIGHT     = 11;
const int EASE_TRIANGLE_SIZE  = 4;
const int PLAY_MARKER_SIZE    = 10;
const int PINNED_SIZE         = 10;
const int PINNED_SIZE         = 11;
const int NAV_TAG_WIDTH       = 7;
const int NAV_TAG_HEIGHT      = 13;
const int FRAME_MARKER_SIZE   = 4;
const int FOLDED_CELL_SIZE    = 9;
const int SHIFTTRACE_DOT_SIZE = 12;
@@ -415,6 +417,10 @@ TopToBottomOrientation::TopToBottomOrientation() {
                SHIFTTRACE_DOT_SIZE, SHIFTTRACE_DOT_SIZE));
  addRect(PredefinedRect::SHIFTTRACE_DOT_AREA,
          QRect(SHIFTTRACE_DOT_OFFSET, 0, SHIFTTRACE_DOT_SIZE, CELL_HEIGHT));
  addRect(
      PredefinedRect::NAVIGATION_TAG_AREA,
      QRect((FRAME_HEADER_WIDTH - NAV_TAG_HEIGHT) / 2,
            (CELL_HEIGHT - NAV_TAG_WIDTH) / 2, NAV_TAG_HEIGHT, NAV_TAG_WIDTH));

  // Column viewer
  addRect(PredefinedRect::LAYER_HEADER,
@@ -1004,6 +1010,14 @@ TopToBottomOrientation::TopToBottomOrientation() {
  }
  addPath(PredefinedPath::VOLUME_SLIDER_HEAD, head);

  QPainterPath tag(QPointF(0, -3));
  tag.lineTo(QPointF(9, -3));
  tag.lineTo(QPointF(13, 0));
  tag.lineTo(QPointF(9, 3));
  tag.lineTo(QPointF(0, 3));
  tag.lineTo(QPointF(0, -3));
  addPath(PredefinedPath::NAVIGATION_TAG, tag);

  //
  // Points
  //
@@ -1202,6 +1216,11 @@ LeftToRightOrientation::LeftToRightOrientation() {
  addRect(PredefinedRect::SHIFTTRACE_DOT_AREA,
          QRect(0, SHIFTTRACE_DOT_OFFSET, CELL_WIDTH, SHIFTTRACE_DOT_SIZE));

  addRect(PredefinedRect::NAVIGATION_TAG_AREA,
          QRect((CELL_WIDTH - NAV_TAG_WIDTH) / 2,
                (FRAME_HEADER_HEIGHT - NAV_TAG_HEIGHT) / 2, NAV_TAG_WIDTH,
                NAV_TAG_HEIGHT));

  // Column viewer
  addRect(PredefinedRect::LAYER_HEADER,
          QRect(1, 0, LAYER_HEADER_WIDTH - 2, CELL_HEIGHT));
@@ -1422,6 +1441,14 @@ LeftToRightOrientation::LeftToRightOrientation() {
  timeIndicator.lineTo(QPointF(0, 0));
  addPath(PredefinedPath::TIME_INDICATOR_HEAD, timeIndicator);

  QPainterPath tag(QPointF(-3, 0));
  tag.lineTo(QPointF(3, 0));
  tag.lineTo(QPointF(3, 9));
  tag.lineTo(QPointF(0, 13));
  tag.lineTo(QPointF(-3, 9));
  tag.lineTo(QPointF(-3, 0));
  addPath(PredefinedPath::NAVIGATION_TAG, tag);

  //
  // Points
  //
  33  
toonz/sources/toonzlib/txsheet.cpp
Viewed
@@ -33,6 +33,7 @@
#include "xshhandlemanager.h"
#include "orientation.h"
#include "toonz/expressionreferencemonitor.h"
#include "toonz/navigationtags.h"

#include "toonz/txsheet.h"
#include "toonz/preferences.h"
@@ -197,7 +198,8 @@ TXsheet::TXsheet()
    , m_imp(new TXsheet::TXsheetImp)
    , m_notes(new TXshNoteSet())
    , m_cameraColumnIndex(0)
    , m_observer(nullptr) {
    , m_observer(nullptr)
    , m_navigationTags(new NavigationTags()) {
  // extern TSyntax::Grammar *createXsheetGrammar(TXsheet*);
  m_soundProperties      = new TXsheet::SoundProperties();
  m_imp->m_handleManager = new XshHandleManager(this);
@@ -218,6 +220,7 @@ TXsheet::~TXsheet() {
  assert(m_imp);
  if (m_notes) delete m_notes;
  if (m_soundProperties) delete m_soundProperties;
  if (m_navigationTags) delete m_navigationTags;
}

//-----------------------------------------------------------------------------
@@ -1337,6 +1340,8 @@ void TXsheet::loadData(TIStream &is) {
      m_imp->copyFoldedState();
    } else if (tagName == "noteSet") {
      m_notes->loadData(is);
    } else if (tagName == "navigationTags") {
      m_navigationTags->loadData(is);
    } else {
      throw TException("xsheet, unknown tag: " + tagName);
    }
@@ -1386,6 +1391,13 @@ void TXsheet::saveData(TOStream &os) {
    notes->saveData(os);
    os.closeChild();
  }

  NavigationTags *navigationTags = getNavigationTags();
  if (navigationTags->getCount() > 0) {
    os.openChild("navigationTags");
    navigationTags->saveData(os);
    os.closeChild();
  }
}

//-----------------------------------------------------------------------------
@@ -2049,3 +2061,22 @@ void TXsheet::convertToExplicitHolds() {
    }
  }
}

//---------------------------------------------------------

bool TXsheet::isFrameTagged(int frame) const {
  if (frame < 0) return false;

  return m_navigationTags->isTagged(frame);
}

//---------------------------------------------------------

void TXsheet::toggleTaggedFrame(int frame) {
  if (frame < 0) return;

  if (isFrameTagged(frame))
    m_navigationTags->removeTag(frame);
  else
    m_navigationTags->addTag(frame);
}
© 2022 GitHub, Inc.
Terms
Privacy
Security
Status
Docs
Contact GitHub
Pricing
API
Training
Blog
About
