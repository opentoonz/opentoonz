#pragma once

#ifndef TPALETTE_H
#define TPALETTE_H

// TnzCore includes
#include "tpersist.h"
#include "timage.h"
#include "tfilepath.h"
#include "tpixel.h"
#include "tcolorstyles.h"

// Qt includes
#include <QMutex>

#undef DVAPI
#undef DVVAR
#ifdef TVRENDER_EXPORTS
#define DVAPI DV_EXPORT_API
#define DVVAR DV_EXPORT_VAR
#else
#define DVAPI DV_IMPORT_API
#define DVVAR DV_IMPORT_VAR
#endif

#ifdef _MSC_VER
#pragma warning(push)
#pragma warning(disable : 4251)
#endif

//=========================================================

//    Forward declarations

class TPixelRGBM32;

typedef TSmartPointerT<TPalette> TPaletteP;

//=========================================================

//*****************************************************************************************
//    TPalette  declaration
//*****************************************************************************************

/*!
  \brief    The class representing a Toonz palette object.

  \par Rationale
    A palette is a collection of <I>color styles</I> that adds a level of
indirection
    on color selections when drawing certain graphical objects.
\n
    Palette-based objects store <I>style identifiers</I> into an associated
palette object,
    rather than the colors directly. When a color needs to be accessed, the
palette
    objects is queried for a <I>style id</I>, and the associated \a style is
returned.
\n
    A TColorStyle instance generalizes the notion of \a color, providing the
ability to
    reduce a style instance to a color, \a and specialized drawing functions for
    <I>vector graphics</I> objects.

  \par Reserved colors
    A palette object has two fixed \a reserved styles - which <I>cannot be
removed</I> - at
    indexes \p 0 and \p1. Index \p 0 is reserved for the \b transparent color.
Index \p 1
    is reserved for the \b ink color, initialized by default to black.

  \par Syntax
    A palette is supposedly created <B>on the heap</B>, to the point that, <I>in
current
    implementation</I>, copy and assignment are \a forbidden. This behavior is
somewhat
    inconsitent since a palette \b is currently \a clonable and \a assignable
with the
    appropriate functions. Normalization to common C++ syntax should be enforced
ASAP.

\sa The TColorStyle class.
*/

class DVAPI TPalette final : public TPersist, public TSmartObject {
  DECLARE_CLASS_CODE
  PERSIST_DECLARATION(TPalette);

public:
  /*!
          \brief A palette page is a restricted view of a palette instance.
  */

  class DVAPI Page {
    friend class TPalette;

  private:
    std::wstring m_name;  //!< Name of the page to be displayed.
    int m_index;  //!< Index of the page in the palette's pages collection.
    TPalette *m_palette;  //!< (\p not \p owned)  Palette the page refers to.
    std::vector<int> m_styleIds;  //!< Palette style ids contained in the page.

  public:
    Page(std::wstring name);

    std::wstring getName() const {
      return m_name;
    }  //!< Returns the name of the page.
    void setName(std::wstring name) {
      m_name = name;
    }  //!< Sets the name of the page.

    TPalette *getPalette() const {
      return m_palette;
    }  //!< Returns a pointer to the palette that contains this page.

    int getIndex() const {
      return m_index;
    }  //!< Returns the page index in the palette.

    int getStyleCount() const {
      return (int)m_styleIds.size();
    }  //!< Returns the number of the styles contained in the page.

    int getStyleId(int indexInPage) const;  //!< Returns the \a index-th style
                                            //! id in the page, or \p -1 if not
    //! found.
    TColorStyle *getStyle(int indexInPage) const;  //!< Returns the \a index-th
    //! style in the page, or \p 0
    //! if not found.

    //! \return The insertion index in the page, or \p -1 on failure
    int addStyle(int styleId);  //!< Adds the specified style Id to the page (at
                                //! the \a back
                                //!  of the page).
                                /*!
                            \warning  The supplied style must have been allocated with \a new.
                            \warning  Style ownership is surrendered to the palette.
                            \return   The insertion index in the page, or \p -1 on failure.
                                In case of failure, the supplied style is \a deleted.
                            */
    int addStyle(TColorStyle *style);  //!< Adds the specified style to the
                                       //! palette, and assigns it
    //!  to this page.
    //! \return The insertion index in the page, or \p -1 on failure
    int addStyle(TPixel32 color);  //!< Add a solid color style to the palette,
                                   //! and assigns it
    //!  to this page.

    void insertStyle(int indexInPage, int styleId);  //!< Inserts the supplied
                                                     //! style id at the
    //! specified position
    //!  in the page.
    //! \sa The specifics of addStyle(TColorStyle*) apply here.
    void insertStyle(int indexInPage, TColorStyle *style);  //!< Inserts the
    //! supplied style in
    //! the palette, and
    //! assigns its
    //!  id at the specified position in the page.
    void insertStyle(int indexInPage, TPixel32 color);  //!< Inserts a solid
                                                        //! color style in the
    //! palette, and assigns
    //! its
    //!  id at the specified position in the page.

    void removeStyle(int indexInPage);  //!< Removes the style at the specified
                                        //! position from this page.
    int search(int styleId)
        const;  //!< Returns the page position of the specified style id,
                //!  or \p -1 if it cannot be found on the page.
    int search(TColorStyle *style)
        const;  //!< Returns the page position of the specified style,
                //!  or \p -1 if it cannot be found on the page.
  };

private:
  typedef std::map<int, TColorStyleP>
      StyleAnimation;  //!< Style keyframes list.
  typedef std::map<int, StyleAnimation>
      StyleAnimationTable;  //!< Style keyframes list per style id.

  friend class Page;

private:
  std::wstring m_globalName;   //!< Palette \a global name.
  std::wstring m_paletteName;  //!< Palette name.

  int m_version;                //!< Palette version number.
  std::vector<Page *> m_pages;  //!< Pages list.
  std::vector<std::pair<Page *, TColorStyleP>> m_styles;  //!< Styles container.
  std::map<int, int> m_shortcuts;
  StyleAnimationTable
      m_styleAnimationTable;  //!< Table of style animations (per style).
  int m_currentFrame;         //!< Palette's current frame in style animations.
  bool m_isCleanupPalette;    //!< Whether the palette is used for cleanup
                              //! purposes.

  TImageP m_refImg;
  TFilePath m_refImgPath;
  std::vector<TFrameId> m_refLevelFids;

  bool m_dirtyFlag;  //!< Whether the palette changed and needs to be refreshed.
  QMutex m_mutex;    //!< Synchronization mutex for multithreading purposes.

  bool m_isLocked;          //!< Whether the palette is locked.
  bool m_askOverwriteFlag;  //!< This variable is quite unique. This flag is to
                            //! acheive following beghavior:
  //! When saving the level with the palette being edited, ask whether the
  //! palette
  //! should be overwitten ONLY ONCE AT THE FIRST TIME.
  //! The confirmation dialog will not be opened unless the palette is edited
  //! again,
  //! even if the palette's dirtyflag is true.

  int m_shortcutScopeIndex;

  int m_currentStyleId;

  bool m_areRefLevelFidsSpecified = false;

public:
  TPalette();
  ~TPalette();

  TPalette *clone()
      const;  //!< Allocates a \a new copy of this palette instance.

  static void setRootDir(const TFilePath &fp);  //!< It must be specified to
                                                //! save textures in \e
  //! fp/textures.
  static TFilePath getRootDir();

  std::wstring getGlobalName() const {
    return m_globalName;
  }  //!< Returns the name of the palette object.
  void setGlobalName(std::wstring name) {
    m_globalName = name;
  }  //!< Assigns the name of the palette.

  void setDirtyFlag(bool dirtyFlag)  //!< Declares the palette \a changed with
                                     //! respect to the last <I>saved state</I>.
  {
    m_dirtyFlag = dirtyFlag;
    // synchronize with the dirty flag
    m_askOverwriteFlag = dirtyFlag;
  }
  bool getDirtyFlag() {
    return m_dirtyFlag;
  }  //!< Returns whether the palette changed with respect to the last <I>saved
     //! state</I>.

  TColorStyle *getStyle(int styleId)
      const;  //!< Returns a pointer to the color style with the specified id,
              //!  or \p 0 if said id is not stored in the palette.
  int getStyleCount() const {
    return (int)m_styles.size();
  }  //!< Returns the number of the color styles in the palette.
  int getStyleInPagesCount()
      const;  //!< Returns the number of styles contained in palette pages.

  int getFirstUnpagedStyle() const;  //!< Returns the styleId of the first style
                                     //! not in a page (\p -1 if none).

  /*!
          \remark   Style ownserhip is \a surrendered to the palette
          \return   The styleId associated to the inserted style, or \a -1 on
     failure.
  */
  int addStyle(TColorStyle *style);         //!< Adds the specified style to the
                                            //! palette (but in no page).
  int addStyle(const TPixelRGBM32 &color);  //!< Adds a solid color style to the
                                            //! palette (but in no page).

  /*!
\remark     Style ownserhip is \a surrendered to the palette.
\remark     Any existing style's animation will be discarded.
*/
  void setStyle(
      int styleId,
      TColorStyle *style);  //!< Replaces the style with the specified style id.
  void setStyle(int styleId, const TPixelRGBM32 &color);  //!< Replaces the
                                                          //! style with the
  //! specified style id.

  int getPageCount() const;  //!< Returns the pages count.

  Page *getPage(int pageIndex);  //!< Returns the \a index-th palette page, or
                                 //!\p 0 if no such page was found.
  const Page *getPage(int pageIndex) const;  //!< Const version of getPage(int).

  /*!
\return  A pointer to the newly created page.
*/
  Page *addPage(
      std::wstring name);     //!< Creates a new page with the specified name.
  void erasePage(int index);  //!< Deletes the \a index-th page.

  void movePage(Page *page, int dstPageIndex);  //!< Moves the specified page to
                                                //! a different page index.

  Page *getStylePage(int styleId)
      const;  //!< Returns the page containing the specified \a styleId.

  /*!
\note   The distance between colors is calculated with the usual sphrical norm
between RGBA color components.
\return The style id of the nearest style, or \p -1 if none was found.
  */
  int getClosestStyle(const TPixelRGBM32 &color)
      const;  //!< Returns the index of the style whose main color
              //!  is nearest to the requested one.

  void loadData(TIStream &is) override;  //!< I/O palette save function.
  void saveData(TOStream &os) override;  //!< I/O palette load function.

  int getVersion() const {
    return m_version;
  }  //!< Returns the palette's version number
  void setVersion(int v) {
    m_version = v;
  }  //!< Sets the palette's version number

  void setRefLevelFids(const std::vector<TFrameId> fids,
                       bool specified);  //!< Associates the
                                         //! list of frames \e
  //! fids to this palette.
  //! When specified == true fids were specified by user on loading.
  std::vector<TFrameId> getRefLevelFids();  //!< Returns the list of frames
                                            //! associated to this palette.

  //! \deprecated  Should be substituted by operator=(const TPalette&).
  void assign(const TPalette *src,
              bool isFromStudioPalette = false);  //!< Copies src's page, color
                                                  //! styles an animation table
  //! to the
  //!  correspondig features of the palette.
  //!  if the palette is copied from studio palette, this function will modify
  //!  the original names.
  void merge(const TPalette *src,
             bool isFromStudioPalette =
                 false);  //!< Adds src's styles and pages to the palette.
  //!  if the palette is merged from studio palette, this function will modify
  //!  the original names.

  void setAskOverwriteFlag(
      bool askOverwriteFlag) {  //!< Sets the ask-overwrite flag.
    m_askOverwriteFlag = askOverwriteFlag;
  }
  bool getAskOverwriteFlag() {
    return m_askOverwriteFlag;
  }  //!< Returns the ask-overwrite flag.

  void setIsCleanupPalette(
      bool on);  //!< Sets the palette's identity as a cleanup palette.
  bool isCleanupPalette() const {
    return m_isCleanupPalette;
  }  //!< Returns whether this is a cleanup palette.

  const TImageP &getRefImg() const {
    return m_refImg;
  }  //!< Returns an image that represents the frame image associated to this
     //! palette.
  void setRefImg(const TImageP &img);  //!< Associates an image to this palette,
                                       //! that is an image in the frame
  //!  builded or modified using this palette.

  const TFilePath getRefImgPath() const {
    return m_refImgPath;
  }  //!< Returns the file path of the reference image.
  void setRefImgPath(
      const TFilePath &fp);  //!< Sets the path filename of the reference image.

  bool isAnimated() const;  //!< Returns whether this palette is animated
                            //!(styles change through time).

  int getFrame() const;      //!< Returns the index of the current frame.
  void setFrame(int frame);  //!< Sets the index of the current frame.

  bool isKeyframe(int styleId, int frame) const;  //!< Returns whether the
                                                  //! specified frame is a \a
  //! keyframe in styleId's
  //! animation
  int getKeyframeCount(int styleId)
      const;  //!< Returns the keyframes count in styleId's animation.

  int getKeyframe(int styleId, int index)
      const;  //!< Returns the \a index-th frame in styleId's animation,
              //!  or \p -1 if it couldn't be found.
  void setKeyframe(int styleId, int frame);  //!< Sets a keyframe at the
                                             //! specified frame of styleId's
  //! animation.
  void clearKeyframe(int styleId, int frame);  //!< Deletes the keyframe at the
                                               //! specified frame of styleId's
  //! animation.

  /*!
\note   Key is an integer between 0 and 9 included.
*/
  int getShortcutValue(
      int key) const;  //!< Returns the style id \a shortcut associated to key,
                       //!  or \p -1 if it couldn't be found.
  /*!

  */
  int getStyleShortcut(
      int styleId) const;  //!< Returns the shortcut associated to styleId, or
                           //!  or \p -1 if it couldn't be found.
  void setShortcutValue(
      int key, int styleId);  //!< Associates the specified key to a styleId.

  void setPaletteName(std::wstring name) {
    m_paletteName = name;
  }  //!< Sets the name of the palette.
  std::wstring getPaletteName() const {
    return m_paletteName;
  }  //!< Returns the name of the palette.

  QMutex *mutex() { return &m_mutex; }  //!< Returns the palette's mutex

  bool isLocked() const {
    return m_isLocked;
  }                            //!< Returns whether the palette is locked.
  void setIsLocked(bool lock)  //!< Sets lock/unlock to the palette.
  {
    m_isLocked = lock;
  }

  bool hasPickedPosStyle();  // Returns true if there is at least one style with
                             // picked pos value

  void nextShortcutScope(bool invert);

  void setCurrentStyleId(int id) { m_currentStyleId = id; }
  int getCurrentStyleId() const { return m_currentStyleId; }

public:
  // Deprecated functions

  //! \deprecated  Used only once throughout Toonz, should be verified.
  bool getFxRects(const TRect &rect, TRect &rectIn, TRect &rectOut);

private:
  // Not copyable
  TPalette(const TPalette &);             //!< Not implemented
  TPalette &operator=(const TPalette &);  //!< Not implemented
};

//-------------------------------------------------------------------

#ifdef _WIN32
template class DVAPI TSmartPointerT<TPalette>;
#endif

//-------------------------------------------------------------------

#ifdef _MSC_VER
#pragma warning(pop)
#endif

#endif  // TPALETTE_H
