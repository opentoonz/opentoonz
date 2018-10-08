#pragma once

#ifndef IMAGEUTILS_H
#define IMAGEUTILS_H

// TnzBase includes
#include "trasterfx.h"

// TnzCore includes
#include "tregion.h"
#include "tvectorimage.h"

// Qt includes
#include <QWidget>
#include <QKeyEvent>

#undef DVAPI
#undef DVVAR
#ifdef TOONZQT_EXPORTS
#define DVAPI DV_EXPORT_API
#define DVVAR DV_EXPORT_VAR
#else
#define DVAPI DV_IMPORT_API
#define DVVAR DV_IMPORT_VAR
#endif

//=============================================================================

//    Forward declarations

class TFilePath;
class TPropertyGroup;
// enum TRenderSettings::ResampleQuality;

class QHBoxLayout;

//=============================================================================

namespace ImageUtils {

/*!
  \brief    Notify that a task on a single frame is completed.
*/
class DVAPI FrameTaskNotifier final : public QObject {
  Q_OBJECT

  int m_errorCount, m_warningCount;
  bool m_abort;

public:
  FrameTaskNotifier() : m_errorCount(0), m_warningCount(0), m_abort(false) {}
  ~FrameTaskNotifier() {}

  void notifyFrameCompleted(int frame) { emit frameCompleted(frame); }
  void notifyError() {
    m_errorCount++;
    emit error();
  }
  void notifyLevelCompleted(const TFilePath &fullPath) {
    emit levelCompleted(fullPath);
  }

  bool abortTask() { return m_abort; }
  void reset() { m_abort = false; }

  int getErrorCount() const { return m_errorCount; }

signals:

  void frameCompleted(int);
  void levelCompleted(const TFilePath &fullPath);
  void error();

protected slots:

  void onCancelTask() { m_abort = true; }
};

//----------------------------------------------------

TFilePath DVAPI duplicate(const TFilePath &levelPath);

void DVAPI premultiply(const TFilePath &levelPath);

void DVAPI convert(
    const TFilePath &source,  //!< Level path to convert from.
    const TFilePath &dest,    //!< Level path to convert to.
    const TFrameId &from,     //!< First source frame to convert. Supports
                              //! TFrameId::EMPTY_FRAME
    //!  to specify conversion from the beginning of level.
    const TFrameId
        &to,  //!< Last source frame to convert. Supports TFrameId::EMPTY_FRAME
              //!  to specify conversion to the end of level.
    double framerate,      //!< Frame rate for destination movie formats.
    TPropertyGroup *prop,  //!< Format properties for the destination level.
    FrameTaskNotifier
        *frameNotifier,  //!< Observer class for frame success notifications.
    const TPixel &bgColor =
        TPixel::Transparent,  //!< Destination Background color.
    bool removeDotBeforeFrameNumber =
        false /*-- ConvertPopup
                 での指定に合わせて、[レベル名].[フレーム番号].[拡張子]のうち、
                                                                                          [レベル名]と[フレーム番号]の間のドットを消す。 --*/
    );        //!< Converts a saved level to fullcolor, and saves the result.

void DVAPI convertNaa2Tlv(
    const TFilePath &source,  //!< Level path to convert from.
    const TFilePath &dest,    //!< Level path to convert to.
    const TFrameId &from,     //!< First source frame to convert.
    const TFrameId &to,       //!< Last source frame to convert.
    FrameTaskNotifier
        *frameNotifier,  //!< Observer class for frame success notifications.
    TPalette *palette =
        0,  //!< Special conversion function from an antialiased level to tlv.
            //!  \sa  Function ImageUtils::convert().
    bool removeUnusedStyles = false,
    double dpi = 0.0);  //! Remove unused styles from input palette.

// convert old levels (tzp / tzu) to tlv
void DVAPI convertOldLevel2Tlv(
    const TFilePath &source,  //!< Level path to convert from.
    const TFilePath &dest,    //!< Level path to convert to.
    const TFrameId &from,     //!< First source frame to convert.
    const TFrameId &to,       //!< Last source frame to convert.
    FrameTaskNotifier
        *frameNotifier  //!< Observer class for frame success notifications.
    );

double DVAPI getQuantizedZoomFactor(double zf, bool forward);

void DVAPI getFillingInformationOverlappingArea(
    const TVectorImageP &vi, std::vector<TFilledRegionInf> &regs,
    const TRectD &area1, const TRectD &area2 = TRectD());

void DVAPI getFillingInformationInArea(const TVectorImageP &vi,
                                       std::vector<TFilledRegionInf> &regs,
                                       const TRectD &area);

void DVAPI assignFillingInformation(TVectorImage &vi,
                                    const std::vector<TFilledRegionInf> &regs);

void DVAPI getStrokeStyleInformationInArea(
    const TVectorImageP &vi, std::vector<std::pair<int, int>>
                                 &strokesInfo,  // pair:strokeIndex, styleIndex
    const TRectD &area);

//*********************************************************************************************
//    FullScreenWidget  declaration
//*********************************************************************************************

/*!
  \brief    Temporary class used to deal with QTBUG #7556 - QGLWidgets going
  fullscreen \a need
            a containing widget that leaves a small margin to prevent the widget
  from covering other
            widgets (specifically, context menus).
*/

class DVAPI FullScreenWidget final : public QWidget {
  Q_OBJECT

  QWidget *m_widget;  //!< (Owned) The content widget.

public:
  FullScreenWidget(QWidget *parent = 0);

  void setWidget(
      QWidget *widget);  //!< Sets the content, surrendering ownership.
  QWidget *widget() const { return m_widget; }

public slots:

  bool toggleFullScreen(bool quit = false);
};

//*********************************************************************************************
//    ShortcutZoomer  declaration
//*********************************************************************************************

/*!
  \brief    The ShortcutZoomer abstract base class is used by viewer widget to
  access
            shortcut-related commands.

  \details  This class is a wrapper for shortcuts established by the
  CommandManager
            interface.

            Subclass it defining the required view commands, then implement a
            \p keyPressEvent() event handler in the viewer widget you want:

            \code
            void MyViewer::keyPressEvent(QKeyEvent* ke)
            {
              if(ViewerZoomer(this).exec(event))
                return;

              return MyViewerBase::keyPressEvent(ke);
            }
            \endcode

  \warning  Use the FullScreenWidget class to wrap a viewer class that
            needs to go fullscreen.
*/

class DVAPI ShortcutZoomer {
  QWidget *m_widget;  //!< Viewer widget being processed.

public:
  ShortcutZoomer(
      QWidget *viewerWidget);  //!< Constructs on the specified viewer widget.

  QWidget *getWidget() {
    return m_widget;
  }  //!< Returns the processed viewer widget.

  bool exec(
      QKeyEvent *event);  //!< Processes a key event for shortcuts related to
                          //!  viewer commands.
                          //!  \return  Whether a shortcut was recognized.
protected:
  virtual bool zoom(
      bool zoomin,
      bool resetZoom) = 0;  //!< Handler for zoom commands. Required.
  virtual bool fit() {
    return false;
  }  //!< Handler for 'fit to image' commands.
  virtual bool setActualPixelSize() {
    return false;
  }  //!< Handler for 'use actual pixel size' commands.
  virtual bool setFlipX() {
    return false;
  }  //!< Handler for 'flip viewer vertically' commands.
  virtual bool setFlipY() {
    return false;
  }  //!< Handler for 'flip viewer horizontally' commands.
  virtual bool toggleFullScreen(
      bool quit = false)  //!  Handler for 'toggle fullscreen' commands.
  {
    return false;
  }
};

}  // namespace ImageUtils

#endif
