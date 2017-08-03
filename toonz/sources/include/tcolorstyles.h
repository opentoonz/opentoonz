#pragma once

#ifndef TCOLORSTYLES_H
#define TCOLORSTYLES_H

// TnzCore includes
#include "tfilepath.h"
#include "traster.h"

// Qt includes
#include <QString>

#undef DVAPI
#undef DVVAR

#ifdef TVRENDER_EXPORTS
#define DVAPI DV_EXPORT_API
#define DVVAR DV_EXPORT_VAR
#else
#define DVAPI DV_IMPORT_API
#define DVVAR DV_IMPORT_VAR
#endif

//=================================================

//    Forward declarations

class TStroke;
class TRegion;
class TStrokeProp;
class TRegionProp;
class TVectorRenderData;
class TFlash;
class TInputStreamInterface;
class TOutputStreamInterface;
class TRasterStyleFx;
class QStringList;

//=================================================

//************************************************************************
//    TRasterStyleFx  definition
//************************************************************************

class DVAPI TRasterStyleFx {
public:
  struct Params {
    TRasterP m_r;
    TPoint m_p;
    TRasterP m_rOrig;
    int m_colorIndex;
    int m_frame;

  public:
    Params(const TRasterP &r, const TPoint &p, const TRasterP &rOrig, int index,
           int frame)
        : m_r(r), m_p(p), m_rOrig(rOrig), m_colorIndex(index), m_frame(frame) {
      assert(m_r);
    }
  };

public:
  virtual ~TRasterStyleFx() {}

  virtual bool isInkStyle() const   = 0;
  virtual bool isPaintStyle() const = 0;

  virtual bool inkFxNeedRGBMRaster() const { return false; }

  virtual bool compute(const Params &params) const = 0;

  virtual void getEnlargement(int &borderIn, int &borderOut) const {
    borderIn = borderOut = 0;
  }
};

//************************************************************************
//    TColorStyle  definition
//************************************************************************

/*!
  \brief Abstract class representing a color style in a Toonz palette.
*/

class DVAPI TColorStyle : public TSmartObject {
public:
  //! Helper class to declare color styles
  class Declaration {
  public:
    Declaration(TColorStyle *style) { declare(style); }
  };

  enum ParamType {
    BOOL,     //!< Boolean parameter type.
    INT,      //!< Integer parameter type.
    ENUM,     //!< Enum parameter type (maps to integer values).
    DOUBLE,   //!< Double parameter type (getParamType() default).
    FILEPATH  //!< TFilePath parameter type.
  };

  struct double_tag {};
  struct bool_tag {};
  struct int_tag {};
  struct TFilePath_tag {};

private:
  std::wstring m_name;          //!< User-define style name.
  std::wstring m_globalName;    //!< User-define style \a global name.
  std::wstring m_originalName;  //!< If the style is copied from studio palette,
                                //! its original name is stored

  int m_versionNumber;  //!< Style's version number.

  unsigned int m_flags;  //!< Style attributes.
  bool m_enabled;        //!< Style's \a enabled status.

  bool m_isEditedFromOriginal;  //<! If the style is copied from studio palette,
                                // This flag will be set when the
  //!  style is edited from the original one.

  TPoint m_pickedPosition;  // picked position from color model by using style
                            // picker tool with "organize palette" option.

protected:
  TRaster32P m_icon;  //!< Icon shown on TPalette viewers.
  bool m_validIcon;   //!< Icon's validity status.

public:
  static int m_currentFrame;  //!< Time instant in the palette's timeline.
                              //!  \deprecated  Should be done better
public:
  TColorStyle();
  TColorStyle(
      const TColorStyle &other);  //!< Copies another style \a except its icon
  virtual ~TColorStyle();

  virtual TColorStyle *clone() const = 0;  //!< Polymorphic clone of the style.
  virtual TColorStyle &copy(
      const TColorStyle &other)  //!< Polymorphic copy of the style.
  {
    assignBlend(other, other, 0.0);
    return *this;
  }

  bool operator==(const TColorStyle &cs) const;
  bool operator!=(const TColorStyle &cs) { return !operator==(cs); }

  // Renderizer-related  objects

  virtual TStrokeProp *makeStrokeProp(
      const TStroke
          *stroke) = 0;  //!< Allocates a \a new derived TStrokeProp instance
  //!  used to draw the style on the specified stroke object.

  virtual TRegionProp *makeRegionProp(
      const TRegion
          *region) = 0;  //!< Allocates a \a new derived TRegionProp instance
  //!  used to draw the style on the specified region object.

  virtual bool isRasterStyle() const {
    return false;
  }  //!< Returns whether the style is of the raster kind.
  virtual TRasterStyleFx *getRasterStyleFx() {
    return 0;
  }  //!< If the style contains raster effects it must return it,
     //!  else returns 0.

  virtual bool isRegionStyle()
      const = 0;  //!< Returns whether the style applies on regions.
  virtual bool isStrokeStyle()
      const = 0;  //!< Returns whether the style applies on strokes.

  // General functions

  void setName(std::wstring name) {
    m_name = name;
  }  //!< Sets the style's user-defined name.
  std::wstring getName() const {
    return m_name;
  }  //!< Returns the style's user-defined name.

  /*! \detail
The \a global name contains information about palette id.
*/
  void setGlobalName(std::wstring name) {
    m_globalName = name;
  }  //!< Sets the global name of the style.
  std::wstring getGlobalName() const {
    return m_globalName;
  }  //!< Returns the global name of the style.

  void setOriginalName(std::wstring name) {
    m_originalName = name;
  }  //!< If the style is originally copied from studio palette, set this.
  std::wstring getOriginalName() const {
    return m_originalName;
  }  //!< Returns the original name of the style.

  void setIsEditedFlag(bool edited) {
    m_isEditedFromOriginal = edited;
  }  //<! If the style is copied from studio palette, This flag will be set when
     // the
  //!  style is edited from the original one.
  bool getIsEditedFlag() const {
    return m_isEditedFromOriginal;
  }  //!< Returns whether the style is edited from the original from studio
     //! palette

  void assignNames(
      const TColorStyle *src);  //!< Copies the style names from src.

  //! \sa tcolorflags.h
  unsigned int getFlags() const { return m_flags; }  //!< Sets color attributes.
  void setFlags(unsigned int flags) {
    m_flags = flags;
  }  //!< Returns color attributes.

  void setPickedPosition(const TPoint &pos) { m_pickedPosition = pos; }
  TPoint getPickedPosition() const { return m_pickedPosition; }

  // Color-related functions

  /*! \detail
Raster or vector pattern image styles, for example, return false.
*/
  virtual bool hasMainColor() const {
    return false;
  }  //!< Returns whether the style has a reference \a main color.
  virtual TPixel32 getMainColor() const {
    return TPixel32::Black;
  }  //!< Returns the style's main color, or black if none.
  virtual void setMainColor(const TPixel32 &color) {
  }  //!< Sets the main color, if it has one.

  /*! \detail
For example a bubble styles has two colors.
By default it returns \p 1 if the style has only a main color else
returns \p 0 if the style has no color.
*/
  virtual int getColorParamCount() const {
    return hasMainColor() ? 1 : 0;
  }  //!< Returns the number of colors (if any) of the style.

  /*! \detail
Returns the main color by default.
*/
  virtual TPixel32 getColorParamValue(int index) const {
    return getMainColor();
  }  //!< Returns the value of the color of index \e index.
  virtual void setColorParamValue(int index, const TPixel32 &color) {
    setMainColor(color);
  }  //!< Sets the \a index-th color value.

  virtual TPixel32 getAverageColor() const {
    return getMainColor();
  }  //!< Returns a color representing the average of all the style's colors

  // Parameters-related functions

  /*! \detail
   For example: a the general cleanup style has two parameters, brightness and
   contrast and
   a black cleanup style has in addition a black and white threshold, therefore
   4 parameters.
*/
  virtual int getParamCount() const {
    return 0;
  }  //!< Returns the number of parameters necessary
     //!  to describe the style.

  virtual ParamType getParamType(int paramIdx) const {
    assert(0 <= paramIdx && paramIdx < getParamCount());
    return DOUBLE;
  }  //!< Returns the type of the specified parameter.

  virtual bool hasParamDefault(int index) const {
    return false;
  }  //!< Value of parameter can be reset to default.

  virtual void setParamDefault(int index) {
    assert(false);
  }  //!< Reset value of parameter to default.

  virtual bool isParamDefault(int index) const {
    return false;
  }  //!< Check if current value of parameter equals to default

  virtual void setParamValue(int index, bool value) {
    assert(false);
  }  //!< Assigns a value to the specified \p bool parameter.
  virtual bool getParamValue(bool_tag, int index) const {
    assert(false);
    return bool();
  }  //!< Retrieves the specified \p bool parameter value.

  virtual void setParamValue(int index, int value) {
    assert(false);
  }  //!< Assigns a value to the specified \p int parameter.
  virtual int getParamValue(int_tag, int index) const {
    assert(false);
    return int();
  }  //!< Retrieves the specified \p int parameter value.
  virtual void getParamRange(int index, int &min, int &max) const {
    assert(false);
  }  //!< Retrieves the specified \p int parameter range.

  virtual void getParamRange(int pIndex, QStringList &stringItems) const {
    assert(false);
  }  //!< Returns a description of each supported enum or file
  //!  extension value in text form.
  virtual void setParamValue(int index, double value) {
    assert(false);
  }  //!< Assigns a value to the specified \p double parameter.
  virtual double getParamValue(double_tag, int index) const {
    assert(false);
    return double();
  }  //!< Retrieves the specified \p double parameter value.
  virtual void getParamRange(int index, double &min, double &max) const {
    assert(false);
  }  //!< Retrieves the specified \p double parameter range.

  virtual void setParamValue(int index, const TFilePath &fp) {
    assert(false);
  }  //!< Assigns a value to the specified \p TFilePath parameter.
  virtual TFilePath getParamValue(TFilePath_tag, int index) const {
    assert(false);
    return TFilePath();
  }  //!< Retrieves the specified \p TFilePath parameter value.

  /*! \detail
This function is provided to animate a palette style. Given two <I>keyframe
styles</I> \p a and \p b, it calculates their linear interpolation at parameter
\p t and assigns the result to this style instance.

\param a    Style value at parameter <TT>t = 0</TT>.
\param b    Style value at parameter <TT>t = 1</TT>.
\param t    Interpolation parameter.
*/
  void assignBlend(const TColorStyle &a, const TColorStyle &b,
                   double t);  //!< Assigns the linear interpolation between the
                               //!  supplied styles.

  // Description-related functions

  /*! \detail
For example for a stroke style we have "Constant", "Chain", "Rope", "Tulle",
etc...
*/
  virtual QString getDescription()
      const = 0;  //!< Return a brief description of the style.
  virtual QString getParamNames(int index)
      const;  //!< Return the string that identifies the \a index-th parameter.

  // Drawing-related  functions

  virtual void drawStroke(TFlash &flash, const TStroke *stroke)
      const;  //!< Draws the supplied stroke in flash.

  // I/O-related  functions

  virtual int getTagId()
      const = 0;  //!< Returns an unique number representing the style.
  virtual void getObsoleteTagIds(std::vector<int> &) const {};  //!< \deprecated

  void save(TOutputStreamInterface &) const;  //!< Calls the local
                                              //! implementation of saveData()
  //! passing it also the name and
  //! the tagId of the style.
  static TColorStyle *load(TInputStreamInterface &);  //!< Loads the style from
                                                      //! disk. Calls the local
  //! implementation of
  //! loadData().

  static TColorStyle *create(
      int tagId);  //!< Creates a new style with identifier equal to \a tagId.

  static void declare(
      TColorStyle *style);  //!< Puts the style in the table of actual styles.
  static void getAllTags(std::vector<int> &tags);

  // Misc functions

  /*! \detail
It is used when updates must be done after changes or creation of new styles.
*/
  void invalidateIcon() {
    m_validIcon = false;
  }  //!< Sets a flag that defines the style's icon validity.
  virtual const TRaster32P &getIcon(
      const TDimension &d);  //!< Returns an image representing the style.

  bool isEnabled() const {
    return m_enabled;
  }  //!< Returns whether the style can be used.
  void enable(bool on) {
    m_enabled = on;
  }  //!< Sets the style's \a enabled status

  int getVersionNumber() const {
    return m_versionNumber;
  }  //!< Returns the version number of the style.

protected:
  virtual void makeIcon(const TDimension &d);

  virtual void loadData(TInputStreamInterface &)        = 0;
  virtual void saveData(TOutputStreamInterface &) const = 0;

  virtual void loadData(int, TInputStreamInterface &) {
  }  //!< \deprecated  Backward compatibility

  void updateVersionNumber();

protected:
  TColorStyle &operator=(
      const TColorStyle &other);  //!< Copies another style \a except its icon
};

//--------------------------------------------------------------------------

#ifdef _WIN32
template class DVAPI TSmartPointerT<TColorStyle>;
#endif
typedef TSmartPointerT<TColorStyle> TColorStyleP;

#endif  // TCOLORSTYLES_H
