#pragma once

#ifndef LEVELPROPERTIES_H
#define LEVELPROPERTIES_H

// TnzCore includes
#include "tgeometry.h"

#undef DVAPI
#undef DVVAR
#ifdef TOONZLIB_EXPORTS
#define DVAPI DV_EXPORT_API
#define DVVAR DV_EXPORT_VAR
#else
#define DVAPI DV_IMPORT_API
#define DVVAR DV_IMPORT_VAR
#endif

//**********************************************************************************
//    LevelOptions  definition
//**********************************************************************************

/*!
  \brief    User-editable level properties.

  \details  Historically, LevelProperties stores properties both accessible
            to Toonz end-users, and internal to the Toonz application.

            The user-accessible options have been moved to this nested class
            to clearly separate them from internal properties of a level.
*/

class DVAPI LevelOptions {
public:
  enum DpiPolicy       //!  Describes the dpi policy used for a level.
  { DP_ImageDpi  = 0,  //!< Level uses the natural dpi embedded in its images.
    DP_CustomDpi = 2   //!< Level uses a custom dpi set by the user.
  };

public:
  double m_dpi;       //!< <TT>[default: Stage::inch]</TT> Dpi used with the
                      //!  LevelProperties::DP_CustomDpi policy.
  int m_subsampling,  //!< <TT>[default: 1]</TT> Image subsampling value (see
                      //!  LevelProperties::setSubsampling()).
      m_antialias;    //!< <TT>[default: 0]</TT> Antialias amount (\p 0 meaning
                      //!  no antialias).
  DpiPolicy m_dpiPolicy;  //!< <TT>[default: DP_ImageDpi]</TT> Dpi policy to be
                          //! adpoted.

  bool m_whiteTransp,  //!< <TT>[default: false]</TT> Whether white should be
                       //!  visualized as transparent.
      m_premultiply,  //!< <TT>[default: false]</TT> Whether level images should
                      //! be
      //!  premultiplied by Toonz for alpha compositing (because they
      //!  are not).
      m_isStopMotionLevel;

public:
  LevelOptions();  //!< Constructs with default values.

  bool operator==(const LevelOptions &opts) const;
  bool operator!=(const LevelOptions &other) const {
    return !operator==(other);
  }
};

//**********************************************************************************
//    LevelProperties  definition
//**********************************************************************************

/*!
  \brief    Stores the possible properties of a Toonz level.
*/

class DVAPI LevelProperties {
public:
  /*!
\brief        Alias for LevelOptions::DpiPolicy provided for backward
            compatibility.

\deprecated   Use LevelOptions::DpiPolicy instead.
*/

  enum DpiPolicy {
    DP_ImageDpi  = LevelOptions::DP_ImageDpi,
    DP_CustomDpi = LevelOptions::DP_CustomDpi
  };

public:
  LevelProperties();

  const LevelOptions &options() const  //!  Returns user-accessible options.
  {
    return m_options;
  }
  LevelOptions &options()  //!  Returns user-accessible options.
  {
    return m_options;
  }

  /*! \details  The level subsampling is the integer value (strictly
          greater than 1) denoting the fraction of pixels to be
          displayed for its images <I>while in camera stand mode</I>.
          For example, a value of 2 will \a halve the original image
          resolution.

          Note that only the crudest algorithm will be applied for
          resampling, as this feature is explicitly intended to
          improve visualization speed as much as possible.                    */

  void setSubsampling(int s);  //!< Sets the level subsampling.
  int getSubsampling() const;  //!< Returns the level subsampling.

  /*! \details  The dirty flag is the boolean value specifying whether
          a level has been altered in any way. Saving operations on
          levels whose dirty flag is \p false are no-op.                      */

  void setDirtyFlag(bool on);  //!< Sets the level's <I>dirty flag</I>.
  bool getDirtyFlag() const;   //!< Returns the level's <I>dirty flag</I>

  void setDpiPolicy(DpiPolicy dpiPolicy);  //!< Sets the level's DpiPolicy.
  DpiPolicy getDpiPolicy() const;          //!< Returns the level's DpiPolicy.

  void setImageDpi(const TPointD &dpi);  //!< Sets the level's \a image dpi.
  TPointD getImageDpi() const;           //!< Returns the level's \a image dpi.

  void setDpi(const TPointD &dpi);  //!< Sets the level's \a custom dpi.
  void setDpi(double dpi);          //!< Sets a uniform \a custom level dpi.
  TPointD getDpi() const;           //!< Returns the level's \a custom dpi.

  void setImageRes(
      const TDimension &d);  //!< Sets the level's known image resolution.
  TDimension getImageRes()
      const;  //!< Returns the level's known image resolution.

  void setBpp(int bpp);  //!< Sets the level's known bits-per-pixel.
  int getBpp() const;    //!< Returns the level's known bits-per-pixel.

  void setHasAlpha(
      bool hasAlpha)  //!  Sets whether the level has an alpha channel.
  {
    m_hasAlpha = hasAlpha;
  }
  bool hasAlpha() const  //!  Returns whether the level has an alpha channel.
  {
    return m_hasAlpha;
  }

  void setCreator(
      const std::string &creator)  //!  Sets the level's creator string.
  {
    m_creator = creator;
  }
  std::string getCreator() const  //!  Returns the level's creator string.
  {
    return m_creator;
  }

  /*! \details  A level is forbidden if it has been created with a
          uncompatible application version (e.g. it was created with
          Student and loaded with Toonz).                                     */

  void setIsForbidden(
      bool forbidden)  //!  Sets whether it is forbidden to edit a level.
  {
    m_forbidden = forbidden;
  }
  bool isForbidden() const {
    return m_forbidden;
  }  //!< Returns whether the level is forbidden for editing.

  /*! \param    doPremultiply  Whether Toonz must perform premultiplication - ie
          image pixels are \a not already premultiplied.

\details  Images with an alpha channel may or may not store pixels in
          \a premultiplied form. See the Wikipedia entry on alhpa
          compositing for further information:
          http://en.wikipedia.org/wiki/Alpha_compositing

          Already premultiplied images do not require Toonz to perform
          additional premultiplication on its own to perform alpha
          compositing.                                                        */

  void setDoPremultiply(
      bool doPremultiply)  //!  Sets whether premultiplication must be applied.
  {
    m_options.m_premultiply = doPremultiply;
  }
  bool doPremultiply()
      const  //!  Returns whether premultiplication must be applied.
  {
    return m_options.m_premultiply;
  }

  /*! \details  See
     http://visual-computing.intel-research.net/publications/papers/2009/mlaa/mlaa.pdf
          for an in-depth explanation of the morphological antialiasing
     technique.                */

  void setDoAntialias(int softness)  //!  Sets the amount of morphological
                                     //!  antialiasing to be applied.
  {
    m_options.m_antialias = softness;
  }
  int antialiasSoftness() const  //!  Returns the amount of morphological
                                 //!  antialiasing to be applied.
  {
    return m_options.m_antialias;
  }

  /*! \details  White substitution is currently an \a exact feature in Toonz -
ie
          only \a fully white pixels will be replaced with transparent.
          This implicityl restrains its applicability to images <I>without
          antialias</I>. This is therefore coupled with antialiasing settins.
\sa       The setDoAntialias() method.                                        */

  void setWhiteTransp(bool whiteTransp)  //!  Whether full white pixels should
                                         //!  be intended as transparent.
  {
    m_options.m_whiteTransp = whiteTransp;
  }
  bool whiteTransp() const { return m_options.m_whiteTransp; }

  void setIsStopMotion(bool isStopMotion)  // Is this level used for Stop Motion

  {
    m_options.m_isStopMotionLevel = isStopMotion;
  }
  bool isStopMotionLevel() const { return m_options.m_isStopMotionLevel; }

private:
  TPointD m_imageDpi;

  std::string m_creator;

  TDimension m_imageRes;

  int m_bpp;

  bool m_loadAtOnce, m_dirtyFlag, m_forbidden, m_hasAlpha;

  LevelOptions m_options;
};

#endif  // LEVELPROPERTIES_H
