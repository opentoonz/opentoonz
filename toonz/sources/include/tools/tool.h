#pragma once

#ifndef TOOL_INCLUDED
#define TOOL_INCLUDED

// TnzLib includes
#include "toonz/tstageobjectid.h"
#include "toonz/txsheet.h"
#include "toonz/imagepainter.h"
#include "toonz/tapplication.h"

// TnzCore includes
#include "tcommon.h"
#include "tgeometry.h"
#include "tfilepath.h"

// Qt includes
#include <QString>
#include <QPoint>

#undef DVAPI
#undef DVVAR
#ifdef TNZTOOLS_EXPORTS
#define DVAPI DV_EXPORT_API
#define DVVAR DV_EXPORT_VAR
#else
#define DVAPI DV_IMPORT_API
#define DVVAR DV_IMPORT_VAR
#endif

//===================================================================

//    Forward Declarations

class TToolParam;
class TMouseEvent;
class TStroke;
class TImage;
class TPropertyGroup;
class TColorStyle;
class TFrameId;
class TPalette;
class TSelection;

class TFrameHandle;
class TXshLevelHandle;
class TXsheetHandle;
class TObjectHandle;
class TColumnHandle;
class TSceneHandle;
class TPaletteHandle;
class ToolHandle;
class TSelectionHandle;
class TOnionSkinMaskHandle;
class PaletteController;
class TFxHandle;

class ToolOptionsBox;

class QMenu;

// iwsw commented out
// class Ghibli3DLutUtil;

//===================================================================

//*****************************************************************************************
//    TMouseEvent  definition
//*****************************************************************************************

class DVAPI TMouseEvent {
public:
  enum ModifierBitshift  //! \brief  Bit shifts from 1 associated with modifier
                         //! keys.
  { SHIFT_BITSHIFT,      //!< Bit shift for the Shift key modifier.
    ALT_BITSHIFT,        //!< Bit shift for the Alt key modifier.
    CTRL_BITSHIFT        //!< Bit shift for the Ctrl key modifier.
  };

  enum ModifierMask  //! \brief  Bitmask specifying modifier keys applying on a
  {                  //!         mouse event.
    NO_KEY    = 0x0,
    SHIFT_KEY = (1 << SHIFT_BITSHIFT),  //!< Shift key is being pressed.
    ALT_KEY   = (1 << ALT_BITSHIFT),    //!< Alt key is begin pressed.
    CTRL_KEY  = (1 << CTRL_BITSHIFT)    //!< Ctrl key is being pressed.
  };

public:
  TPoint m_pos;  //!< Mouse position in window coordinates, bottom-left origin.
  int m_pressure;  //!< Pressure of the tablet pen, or 255 for pure mouse
                   //! events.
  ModifierMask m_modifiersMask;  //!< Bitmask specifying key modifiers applying
                                 //! on the event.

  Qt::MouseButtons m_buttons;
  Qt::MouseButton m_button;
  QPoint m_mousePos;  // mouse position obtained with QMouseEvent::pos() or
                      // QTabletEvent::pos()
  bool m_isTablet;

public:
  TMouseEvent()
      : m_pressure(255)
      , m_modifiersMask(NO_KEY)
      , m_buttons(Qt::NoButton)
      , m_button(Qt::NoButton)
      , m_isTablet(false) {}

  bool isShiftPressed() const { return (m_modifiersMask & SHIFT_KEY); }
  bool isAltPressed() const { return (m_modifiersMask & ALT_KEY); }
  bool isCtrlPressed() const { return (m_modifiersMask & CTRL_KEY); }

  bool isLeftButtonPressed() const { return (m_buttons & Qt::LeftButton) != 0; }
  Qt::MouseButtons buttons() const { return m_buttons; }
  Qt::MouseButton button() const { return m_button; }
  QPoint mousePos() const { return m_mousePos; }
  bool isTablet() const { return m_isTablet; }

  void setModifiers(bool shiftPressed, bool altPressed, bool ctrlPressed) {
    m_modifiersMask = ModifierMask((shiftPressed << SHIFT_BITSHIFT) |
                                   (altPressed << ALT_BITSHIFT) |
                                   (ctrlPressed << CTRL_BITSHIFT));
  }

  ModifierMask getModifiersMask() const { return m_modifiersMask; }
};

//*****************************************************************************************
//    TTool  declaration
//*****************************************************************************************

/*!
  \brief    TTool is the abstract base class defining the interface for Toonz
tools - the ones
            accessible from the Toolbar panel that users can activate to edit
the scene contents
            interactively.

  \details  Toonz implements a number of interactive tools, like the <I>Brush
Tool</I>, <I>Fill
            Tool</I>, <I>Zoom Tool</I> and others. They all inherit from this
class, which provides
            the necessary interface and framework to implement a generic
interactive tool in Toonz.

            A Toonz Tool should re-implement the following key functionalities:
 <UL>
              <LI> The abstract getToolType() method, which classifies the tool,
and
                   eventually getTargetType()</LI>
              <LI> The draw() method, used to draw the tool on screen</LI>
              <LI> The mouse-related methods, to grant user interaction</LI>
              <LI> The getProperties() and onPropertyChanged() methods, to
define and track
                   a tool's TProperty members</LI>
              <LI> The addContextMenuItems() method, to insert actions in
right-click menus</LI>
 </UL>
              \par Tool classification
            Toonz enforces a strict classification of its tools that is used to
enable or disable them
            in the appropriate contexts:
<UL>
              <LI> <B>Generic Tools:</B> the tool is always enabled, since it
does not need to access
                    specific scene contents. Hidden tools typically prefer to
select this type since they
                    should handle disablements silently.</LI>
              <LI> <B>Column Tools:</B> the tool is used to alter or define the
placement of a column's
                    content. It is disabled in Filmstrip view mode, since that's
a strictly level-related view.</LI>
              <LI> <B>LevelRead Tools:</B> the tool is used to \a read a level's
images data. It is therefore
                    enabled on all view modes. The tool is disabled in camera
stand or 3D view modes if the
                    level's host column has sustained a placement which makes it
impossible to access image
                    data (as is the case with Plastic-deformed columns).</LI>
              <LI> <B>LevelWrite Tools:</B> the tool is used to \a write a
level's images data. It is
                    disabled in all contexts where a LevelRead Tool would be
disabled. It is also disabled in
                    case the current level is of a type unsupported for write,
\a or the level is read-only
                    on disk.</LI>
</UL>
            Furthermore, tools define a bitwise combination of <I>Target
Types</I>, which are the category
            of level types it can work on (including every image type and the
motion path type).
            The Target Type is used only associated with LevelRead and
LevelWrite tool types.
 \n\n
            There are a number of additional generic rules that define whenever
a tool is disabled:
 <UL>
              <LI> Every tool is disabled when a viewer is in playback.</LI>
              <LI> Every non-Generic tool is disabled on level/columns that do
not host a
                   placeable image type (eg sound or magpie data).</LI>
              <LI> Every non-Generic tool is disabled when working on
columns/levels that have
                   been hidden.</LI>
              <LI> Every non-Generic tool is disabled when working on columns
that have been locked
                   (the lock icon on a column header).</LI>
 </UL>
              \par Drawing
            Tools use OpenGL to draw in their currently associated Viewer
instance, which can be retrieved
            through the getViewer() accessor function. The viewer is assigned to
the tool just before it
            invokes the tool's draw() function - use the onSetViewer() virtual
method to access viewer data
            \a before the viewer starts drawing (observe the tool is typically
drawn as an overlay, on top
            of other stuff).
 \n\n
            Just before draw() is invoked by the viewer, the \p GL_MODELVIEW
matrix is automatically pushed
            by the viewer with the tool-to-window affine returned by getMatrix()
(multiplied by the viewer's
            view affine). Use glGetDoublev() to retrieve the effective
tool-to-window reference change
            affine, and in case reimplement updateMatrix() to specify the affine
returned by getMatrix().
 \n\n
            The default implementation for updateMatrix() sets the tool
reference to current object's
            world one.

              \par Tool Properties
            A tool's properties must be implemented by defining one or more
TPropertyGroup containers,
            and adding them the TProperty specializations corresponding to the
required parameters.
 \n\n
            Every TProperty instance in group 0 is automatically added to the
Tool Options panel
            in Toonz's GUI. Further groups or special toolbar options must be
currently hard-coded
            elsewhere. Tool Options panel construction will probably be
redirected to the tool in
            future Toonz versions.

              \par Context Menu Items
            The addContextMenuItems() is used to insert context menu actions \a
before the standard
            actions provided by the tool viewer. Remember to insert separators
to isolate commands
            of different type (such as view, editing, etc).
*/

class DVAPI TTool {
public:
  class Viewer;

  typedef TApplication Application;

public:
  enum ToolType         //!  Tool editing type.
  { GenericTool   = 1,  //!< Tool will not deal with specific scene content.
    ColumnTool    = 2,  //!< Tool deals with placement of column objects.
    LevelReadTool = 4,  //!< Tool reads a level's image data.
    LevelWriteTool =
        8,  //!< Tool writes a level's image data (implies LevelReadTool).

    // Convenience testing flags - getToolType() should not return these

    LevelTool = LevelReadTool | LevelWriteTool };

  enum ToolTargetType  //!  Object types the tool can operate on.
  { NoTarget    = 0x0,
    VectorImage = 0x1,   //!< Will work on vector images
    ToonzImage  = 0x2,   //!< Will work on colormap (tlv) images
    RasterImage = 0x4,   //!< Will work on fullcolor images
    MeshImage   = 0x8,   //!< Will work on mesh images
    Splines     = 0x10,  //!< Will work on motion paths

    LevelColumns = 0x20,  //!< Will work on level columns
    MeshColumns  = 0x40,  //!< Will work on mesh columns

    EmptyTarget = 0x80,  //!< Will work on empty cells/columns

    CommonImages = VectorImage | ToonzImage | RasterImage,
    AllImages    = CommonImages | MeshImage,
    Vectors      = VectorImage | Splines,

    CommonLevels = CommonImages | LevelColumns,
    MeshLevels   = MeshImage | MeshColumns,

    AllTargets = 0xffffffff,
  };

public:
  static TTool *getTool(std::string toolName, ToolTargetType targetType);

  static TApplication *getApplication();
  static void setApplication(TApplication *application) {
    m_application = application;
  }

  /*! \warning  In case there is no level currently selected, <I>or the
          object to be edited is a spline path</I>, the xsheet cell
          returned by getImageCell() is empty. */

  static TXshCell
  getImageCell();  //!< Returns the level-frame pair to be edited by the tool.

  /*! \details  The image returned by getImage() is either the one
          associated to getImageCell(), or the vector image
          corresponding to currently edited spline path. */

  static TImage *getImage(
      bool toBeModified,
      int subsampling = 0);  //!< Returns the image to be edited by the tool.

  static TImage *touchImage();  //!< Returns a pointer to the actual image - the
                                //!  one of the frame that has been selected.

  /*! \details      This function is necessary since tools are created before
the main
              application (TAB or Toonz) starts, and hence tr() calls have no
effect
              (the translation is not yet installed - to install one you need at
least
              an instance of QApplication / QCoreApplication).

\deprecated   This so much stinks of a bug turned into design choice... */

  static void updateToolsPropertiesTranslation();  //!< Updates translation of
                                                   //! the bound properties of
  //!  every tool (invoking updateTranslation() for each).

public:
  TTool(std::string toolName);
  virtual ~TTool() {}

  virtual ToolType getToolType() const = 0;
  ToolTargetType getTargetType() const { return (ToolTargetType)m_targetType; }

  std::string getName() const { return m_name; }

  /*! \details  The default returns a generic box containing the options
          for property group 0).
\sa       See tooloptions.h for more details. */

  virtual ToolOptionsBox *
  createOptionsBox();  //!< Factory function returning a newly created
                       //!  GUI options box to be displayed for the tool

  void setViewer(Viewer *viewer) {
    m_viewer = viewer;
    onSetViewer();
  }
  Viewer *getViewer() const { return m_viewer; }

  double getPixelSize() const;

  //! Causes the refreshing of the \b rect portion of the viewer.
  //! If rect is empty all viewer is refreshed. \b rect must be in image
  //! coordinate.
  void invalidate(const TRectD &rect = TRectD());

  /*!
          Picks a region of the scene, using an OpenGL projection matrix to
          restrict drawing to a small regionaround \p p of the viewport.
          Retuns -1 if no object's view has been changed.
  */
  int pick(const TPoint &p);
  bool isPicking() const { return m_picking; }

  virtual void updateTranslation(){};

  /*!
This method is called before leftButtonDown() and can be used e.g. to create the
image if needed.
return true if the method execution can have changed the current tool
*/
  virtual bool preLeftButtonDown() { return false; }

  virtual void mouseMove(const TPointD &, const TMouseEvent &) {}
  virtual void leftButtonDown(const TPointD &, const TMouseEvent &) {}
  virtual void leftButtonDrag(const TPointD &, const TMouseEvent &) {}
  virtual void leftButtonUp(const TPointD &, const TMouseEvent &) {}
  virtual void leftButtonDoubleClick(const TPointD &, const TMouseEvent &) {}
  virtual void rightButtonDown(const TPointD &, const TMouseEvent &) {}

  //! For keycodes list, \see keycodes.h.
  virtual bool keyDown(int, TUINT32, const TPoint &) { return false; }
  virtual bool keyDown(int, std::wstring, TUINT32, const TPoint &) {
    return false;
  }

  virtual void onInputText(std::wstring, std::wstring, int, int){};

  virtual void onSetViewer() {}

  virtual void onActivate() {
  }  //!< Callback invoked whenever the tool activates.
  virtual void onDeactivate() {
  }  //!< Callback for tool deactivation, includes tool switch.

  virtual void onImageChanged() {}  //!< Notifies changes in the image in use.

  virtual void onEnter() {
  }  //!< Callback for the mouse entering the viewer area.
  virtual void onLeave() {
  }  //!< Callback for the mouse leaving the viewer area.

  /*-- rasterSelectionTool
   * のフローティング選択が残った状態でフレームが移動したときの挙動を決める --*/
  virtual void onFrameSwitched() {}

  virtual void reset() {}

  virtual void draw() {}  //!< Draws the tool on the viewer.

  bool isActive() const {
    return m_active;
  }  //!< Used to know if a tool is active, (used in TextTool only).
  void setActive(bool active) { m_active = active; }

  virtual TPropertyGroup *getProperties(int) { return 0; }

  /*!
          Does the tasks associated to changes in \p propertyName and returns \p
     true;
  */
  virtual bool onPropertyChanged(std::string propertyName) {
    return false;
  }  //!< Does the tasks associated to changes in \p propertyName and
     //!  returns \p true.
  virtual TSelection *getSelection() {
    return 0;
  }  //!< Returns a pointer to the tool selection.

  //! \sa    For a list of cursor ids cursor.h
  virtual int getCursorId() const {
    return 0;
  }  //!< Returns the type of cursor used by the tool.

  // returns true if the pressed key is recognized and processed.
  // used in SceneViewer::event(), reimplemented in SelectionTool
  // and ControlPointEditorTool
  virtual bool isEventAcceptable(QEvent *e) { return false; }

  TXsheet *getXsheet() const;  //!< Returns a pointer to the actual Xsheet.

  int getFrame();        //!< Returns the actual frame in use.
  int getColumnIndex();  //!< Returns the actual column index.

  TStageObjectId
  getObjectId();  //!< Returns a pointer to the actual stage object.

  void notifyImageChanged();  //!< Notifies changes on the actual image; used to
                              //! update
  //!  images on the level view.
  void notifyImageChanged(const TFrameId &fid);  //!< Notifies changes on the
  //! frame \p fid; used to update
  //!  images on the level view.

  /*! \details   It can depend on the actual frame and the actual cell or
            on the current fid (editing level). In editing scene if
            the current cell is empty the method returns TFrameId::NO_FRAME. */

  TFrameId getCurrentFid()
      const;  //!< Returns the number of the actual editing frame.

  const TAffine &getMatrix() const { return m_matrix; }
  void setMatrix(const TAffine &matrix) { m_matrix = matrix; }

  TAffine getCurrentColumnMatrix()
      const;  //!< Returns the current column matrix transformation.
              //!  \sa  TXsheet::getPlacement.

  TAffine getCurrentColumnParentMatrix()
      const;  //!< Returns the current matrix transformation of the
              //!  current stage object parent.
  TAffine getCurrentObjectParentMatrix() const;
  TAffine getCurrentObjectParentMatrix2() const;

  /*!
          Returns the matrix transformation of the stage object with column
  index equal to \p index
          and frame as the current frame.
  \sa TXsheet::getPlacement.
  */
  TAffine getColumnMatrix(int index) const;

  /*!
   Updates the current matrix transformation with the actual column matrix
transformation.
\sa getCurrentColumnMatrix().
*/
  virtual void updateMatrix();

  /*!
   Add a context menu to the actual tool, as for example pressing right mouse
   button
   with the stroke selection tool.
*/
  virtual void addContextMenuItems(QMenu *menu) {}

  void enable(bool on) { m_enabled = on; }
  bool isEnabled() const { return m_enabled; }

  QString updateEnabled();  //!< Sets the tool's \a enability and returns a
                            //!  reason in case the tool was disabled.
  bool isColumnLocked(int columnIndex) const;

  void resetInputMethod();  //!< Resets Input Context (IME)

  // return true if the pencil mode is active in the Brush / PaintBrush / Eraser
  // Tools.
  virtual bool isPencilModeActive() { return false; }

  void setSelectedFrames(const std::set<TFrameId> &selectedFrames);
  static const std::set<TFrameId> &getSelectedFrames() {
    return m_selectedFrames;
  }

public:
  static std::vector<int> m_cellsData;  //!< \deprecated  brutto brutto. fix
                                        //! quick & dirty del baco #6213 (undo
  //! con animation sheet) spiegazioni in
  //! tool.cpp
  static bool m_isLevelCreated;  //!< \deprecated  Shouldn't expose global
                                 //! static variables.
  static bool m_isFrameCreated;  //!< \deprecated  Shouldn't expose global
                                 //! static variables.

protected:
  std::string m_name;  //!< The tool's name.

  Viewer *m_viewer;  //!< Tool's current viewer.
  TAffine m_matrix;  //!< World-to-window reference change affine.

  int m_targetType;  //!< The tool's image type target.

  bool m_enabled;  //!< Whether the tool allows user interaction.
  bool m_active;
  bool m_picking;

  static TApplication *m_application;

  static std::set<TFrameId> m_selectedFrames;

protected:
  void bind(int targetType);

  virtual void onSelectedFramesChanged() {}

  virtual QString disableString() {
    return QString();
  }  //!< Returns a custom reason to disable the tool
};

//*****************************************************************************************
//    TTool::Viewer  declaration
//*****************************************************************************************

/*!
  \brief    The TTool::Viewer class is the abstract base class that provides an
  interface for
            TTool viewer widgets (it is required that such widgets support
  OpenGL).
*/

class TTool::Viewer {
protected:
  ImagePainter::VisualSettings
      m_visualSettings;  //!< Settings used by the Viewer to draw scene contents

public:
  Viewer() {}
  virtual ~Viewer() {}

  const ImagePainter::VisualSettings &visualSettings() const {
    return m_visualSettings;
  }
  ImagePainter::VisualSettings &visualSettings() { return m_visualSettings; }

  virtual double getPixelSize()
      const = 0;  //!< Returns the length of a pixel in current OpenGL
                  //!< coordinates

  virtual void invalidateAll() = 0;    //!< Redraws the entire viewer, passing
                                       //! through Qt's event system
  virtual void GLInvalidateAll() = 0;  //!< Redraws the entire viewer, bypassing
                                       //! Qt's event system
  virtual void GLInvalidateRect(const TRectD &rect) = 0;  //!< Same as
                                                          //! GLInvalidateAll(),
  //! for a specific
  //! clipping rect
  virtual void invalidateToolStatus() = 0;  //!< Forces the viewer to update the
                                            //! perceived status of tools
  virtual TAffine getViewMatrix() const {
    return TAffine();
  }  //!< Gets the viewer's current view affine (ie the transform from
     //!<  starting to current <I> world view <\I>)

  //! return the column index of the drawing intersecting point \b p
  //! (window coordinate, pixels, bottom-left origin)
  virtual int posToColumnIndex(const TPoint &p, double distance,
                               bool includeInvisible = true) const = 0;
  virtual void posToColumnIndexes(const TPoint &p, std::vector<int> &indexes,
                                  double distance,
                                  bool includeInvisible = true) const = 0;

  //! return the row of the drawing intersecting point \b p (used with
  //! onionskins)
  //! (window coordinate, pixels, bottom-left origin)
  virtual int posToRow(const TPoint &p, double distance,
                       bool includeInvisible = true) const = 0;

  //! return pos in pixel, bottom-left origin
  virtual TPoint worldToPos(const TPointD &worldPos) const = 0;

  //! return the OpenGL nameId of the object intersecting point \b p
  //! (window coordinate, pixels, bottom-left origin)
  virtual int pick(const TPoint &point) = 0;

  // note: winPos in pixel, top-left origin;
  // when no camera movements have been defined then worldPos = 0 at camera
  // center
  virtual TPointD winToWorld(const TPoint &winPos) const = 0;

  // delta.x: right panning, pixels; delta.y: down panning, pixels
  virtual void pan(const TPoint &delta) = 0;

  // center: window coordinates, pixels, bottomleft origin
  virtual void zoom(const TPointD &center, double scaleFactor) = 0;

  virtual void rotate(const TPointD &center, double angle) = 0;
  virtual void rotate3D(double dPhi, double dTheta)        = 0;
  virtual bool is3DView() const      = 0;
  virtual bool getIsFlippedX() const = 0;
  virtual bool getIsFlippedY() const = 0;

  virtual double projectToZ(const TPoint &delta) = 0;

  virtual TPointD getDpiScale() const = 0;
  virtual int getVGuideCount()        = 0;
  virtual int getHGuideCount()        = 0;
  virtual double getHGuide(int index) = 0;
  virtual double getVGuide(int index) = 0;

  virtual void
  resetInputMethod() = 0;  // Intended to call QWidget->resetInputContext()

  virtual void setFocus() = 0;

  /*-- Toolで画面の内外を判断するため --*/
  virtual TRectD getGeometry() const = 0;

  // iwsw commented out
  // virtual Ghibli3DLutUtil* get3DLutUtil(){ return 0; }
};

#endif
