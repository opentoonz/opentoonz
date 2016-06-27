#pragma once

#ifndef EXPORT_LEVEL_COMMAND_H
#define EXPORT_LEVEL_COMMAND_H

// TnzCore includes
#include "tfilepath.h"
#include "tpixel.h"
#include "timage.h"

// TnzLib includes
#include "toonz/txshleveltypes.h"
#include "toonz/tcamera.h"

//=============================================

//  Forward Declarations
class TXshLevel;
class TXshSimpleLevel;
class TCamera;
class TPropertyGroup;

class QString;

//=============================================

namespace IoCmd {

//************************************************************************************
//    Export Level  callbacks
//************************************************************************************

// Event Callbacks - accessories to some IoCmds that can be implemented
// externally

struct OverwriteCallbacks {
  virtual ~OverwriteCallbacks() {}

  //! Returns whether an overwrite must be performed
  virtual bool overwriteRequest(const TFilePath &fp) = 0;
};

//---------------------------------------------------------------------

struct ProgressCallbacks {
  virtual ~ProgressCallbacks() {}

  virtual void setProcessedName(const QString &name) = 0;
  virtual void setRange(int min, int max) = 0;
  virtual void setValue(int val) = 0;
  virtual bool canceled() const  = 0;
};

//************************************************************************************
//    Export Level  options
//************************************************************************************

/*!
  \brief    Collection of options accepted by the export level command.
*/

struct ExportLevelOptions {
  const TPropertyGroup *m_props;  //!< File Format Properties for the export.

  TPixel32 m_bgColor;  //!< Background color to be applied under the image.
  TCamera m_camera;  //!< Stores field and resolution settings for PLI exports.

  //! \remark   In single frame exports, the transform applied to the frame is
  //! <I>the first</I>.
  double m_thicknessTransform[2][2];  //!< Vector images' thickness
                                      //! transformation at the start and
  //!  end of the level export. Each transform is represented as
  //!  first order polynomial through its coefficients.

  //! \warning  This settings currently enforces only Retas's paths convention.
  bool m_forRetas;  //!< Whether the exported level follows Retas's standards:
                    //!  \li 24 bit targa files, with compression
                    //!  \li File paths like "levelNameXXXX.tga"
                    //!  \li Transparent pixels mapped to white
  bool m_noAntialias;  //!< Whether antialias must be removed from images.

public:
  ExportLevelOptions()
      : m_props(0)
      , m_bgColor(TPixel32::Transparent)
      , m_forRetas(false)
      , m_noAntialias(false) {
    m_thicknessTransform[0][0] = 0.0, m_thicknessTransform[0][1] = 1.0;
    m_thicknessTransform[1][0] = 0.0, m_thicknessTransform[1][1] = 1.0;
  }
};

//************************************************************************************
//    Export Level  commands
//************************************************************************************

/*!
  \brief    Intended for preview purposes, computes the image that will be
  exported
            by the exportLevel() command.

  \remark   Supplied image \a may be the one retrievable by sl, depending on
            whether the specified options require it to be modified.

  \return   The image that will be exported by the exportLevel() command.
*/

TImageP exportedImage(
    const std::string
        &ext,  //!< File extension - like \a tga, \a tlv, \a png, etc...
    const TXshSimpleLevel &sl,  //!< Level host of the image to be exported.
    const TFrameId &fid,        //!< Frame of the image in sl.
    const ExportLevelOptions &opts = ExportLevelOptions()  //!< Export options.
    );

//---------------------------------------------------------------------

/*!
  \brief    Exports passed level to the specified path.
  \return   Wheter the level was successfully exported <I>in its entirety</I>.
            User cancels are reported as failures, even if a part of the level
            has been successfully exported.

  \details  This function performs level export depending on the specified
  path's extension,
            and optional parameters. If no input level is specified for the
  export, this function
            attempts the export of currently active level (if any).

            This function allows external callbacks to deal with overwrite and
  progress notifications.
            If no callback is supplied to the function, default ones will be
  used.
*/

bool exportLevel(
    const TFilePath &path,  //!< File path to export the level to.
    TXshSimpleLevel *sl =
        0,  //!< Level to export; if \p 0, current level will be used.
    ExportLevelOptions opts = ExportLevelOptions(),  //!< Export options.
    OverwriteCallbacks *overwriteCB =
        0,  //!< External callbacks to overwrite requests.
    ProgressCallbacks *progressCB =
        0  //!< External callbacks to progress notifications.
    );

}  // namespace IoCmd

#endif  // EXPORT_LEVEL_COMMAND_H
