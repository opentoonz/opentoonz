#pragma once

#ifndef _PLI_IO_H
#define _PLI_IO_H

#ifdef _MSC_VER
#pragma warning(disable : 4661)
#pragma warning(disable : 4018)
#endif

#include <memory>
#include <vector>

#include "tfilepath.h"
#include "tvectorimage.h"
#include "tstroke.h"

#include <QString>

/*=====================================================================

This include file defines classes needed for parsing of the PLI format
designed for use in the paperless consortium.
The classes are designed in reference of the file format as described
in the related paperless document.

====================================================================*/

/*=====================================================================
these headers contains definition for the pixel types,
and for the different curves used (segments, quadratics, cubics)
=====================================================================*/

#include "traster.h"
#include "tcurves.h"

/*=====================================================================
utility macro used to export classes from the dll
(DVAPI: Digital Video Application Progam Interface)
=====================================================================*/

/*=====================================================================
Base class for the generic PLI tag
=====================================================================*/
/*!
  This is the base class for the different tags that form the PLI format.
  For an explanation of each one of them, refer to the documentation
  The different tags can be extracted fronm the class ParsedPli,
  using methods getFirstTag and getNextTag.
  The tag type is stored in m_type member.
 */
class PliTag {
public:
  enum Type {
    NONE      = -1,
    END_CNTRL = 0,
    SET_DATA_8_CNTRL,
    SET_DATA_16_CNTRL,
    SET_DATA_32_CNTRL,
    TEXT,
    PALETTE,
    PALETTE_WITH_ALPHA,
    DUMMY1,
    DUMMY2,
    DUMMY3,
    THICK_QUADRATIC_CHAIN_GOBJ,
    DUMMY4,
    DUMMY5,
    BITMAP_GOBJ,
    GROUP_GOBJ,
    TRANSFORMATION_GOBJ,
    IMAGE_GOBJ,
    COLOR_NGOBJ,
    GEOMETRIC_TRANSFORMATION_GOBJ,
    DOUBLEPAIR_OBJ,
    STYLE_NGOBJ,
    INTERSECTION_DATA_GOBJ,
    IMAGE_BEGIN_GOBJ,
    THICK_QUADRATIC_LOOP_GOBJ,
    OUTLINE_OPTIONS_GOBJ,
    PRECISION_SCALE_GOBJ,
    AUTOCLOSE_TOLERANCE_GOBJ,
    // ...
    HOW_MANY_TAG_TYPES
  };

  Type m_type;

  PliTag();
  PliTag(const Type type);
  virtual ~PliTag(){};
  // PliTag(const PliTag &tag);
};

//=====================================================================

class TStyleParam {
public:
  enum Type {
    SP_NONE = 0,
    SP_BYTE,
    SP_INT,
    SP_DOUBLE,
    SP_USHORT,
    SP_RASTER,
    SP_STRING,
    SP_HOWMANY
  };
  Type m_type;
  double m_numericVal;
  TRaster32P m_r;
  std::string m_string;

  TStyleParam() : m_type(SP_NONE), m_numericVal(0), m_r(), m_string() {}

  TStyleParam(const TStyleParam &styleParam)
      : m_type(styleParam.m_type)
      , m_numericVal(styleParam.m_numericVal)
      , m_r(styleParam.m_r)
      , m_string(styleParam.m_string) {}

  TStyleParam(double x)
      : m_type(SP_DOUBLE), m_numericVal(x), m_r(), m_string() {}
  TStyleParam(int x) : m_type(SP_INT), m_numericVal(x), m_r(), m_string() {}
  TStyleParam(BYTE x) : m_type(SP_BYTE), m_numericVal(x), m_r(), m_string() {}
  TStyleParam(USHORT x)
      : m_type(SP_USHORT), m_numericVal(x), m_r(), m_string() {}

  TStyleParam(const TRaster32P &x)
      : m_type(SP_RASTER), m_numericVal(0), m_r(x), m_string() {}

  TStyleParam(const std::string &x)
      : m_type(SP_STRING), m_numericVal(0), m_r(), m_string(x) {}

  UINT getSize();
};

/*=====================================================================

Subclasses useful to a ierarchical structure of the tags

=====================================================================*/

class PliObjectTag : public PliTag {
protected:
  PliObjectTag();
  PliObjectTag(const Type type);
};

/*=====================================================================*/

class PliGeometricTag : public PliObjectTag {
protected:
  PliGeometricTag();
  PliGeometricTag(const Type type);
};

/*=====================================================================
real tags; their structures resembles the structure in the PLI file and
the description in the PLI specific document
=====================================================================*/

class TextTag final : public PliObjectTag {
public:
  std::string m_text;

  TextTag();
  TextTag(const TextTag &textTag);
  TextTag(const std::string &text);
};

//=====================================================================
/*!
 */
class PaletteTag final : public PliTag {
public:
  TUINT32 m_numColors;
  TPixelRGBM32 *m_color;

  PaletteTag();
  PaletteTag(TUINT32 numColors, TPixelRGBM32 *m_color);
  PaletteTag(const PaletteTag &paletteTag);
  ~PaletteTag();

  bool setColor(const TUINT32 index, const TPixelRGBM32 &color);
};

//=====================================================================
/*!
 */
class PaletteWithAlphaTag final : public PliTag {
public:
  TUINT32 m_numColors;
  TPixelRGBM32 *m_color;

  PaletteWithAlphaTag();
  PaletteWithAlphaTag(TUINT32 numColors);
  PaletteWithAlphaTag(TUINT32 numColors, TPixelRGBM32 *m_color);
  PaletteWithAlphaTag(PaletteWithAlphaTag &paletteTag);
  ~PaletteWithAlphaTag();

  bool setColor(const TUINT32 index, const TPixelRGBM32 &color);
};

//=====================================================================
/*!
All the geometric tags that contains curve informations are
instantiations of this template class
 */

class ThickQuadraticChainTag final : public PliGeometricTag {
public:
  TUINT32 m_numCurves;
  std::unique_ptr<TThickQuadratic[]> m_curve;
  bool m_isLoop;
  double m_maxThickness;
  TStroke::OutlineOptions m_outlineOptions;

  ThickQuadraticChainTag()
      : PliGeometricTag(THICK_QUADRATIC_CHAIN_GOBJ)
      , m_numCurves(0)
      , m_maxThickness(1) {}

  ThickQuadraticChainTag(TUINT32 numCurves, const TThickQuadratic *curve,
                         double maxThickness)
      : PliGeometricTag(THICK_QUADRATIC_CHAIN_GOBJ)
      , m_numCurves(numCurves)
      , m_maxThickness(maxThickness <= 0 ? 1 : maxThickness) {
    if (m_numCurves > 0) {
      m_curve.reset(new TThickQuadratic[m_numCurves]);
      for (UINT i = 0; i < m_numCurves; i++) {
        m_curve[i] = curve[i];
      }
    }
  }

  ThickQuadraticChainTag(const ThickQuadraticChainTag &chainTag)
      : PliGeometricTag(THICK_QUADRATIC_CHAIN_GOBJ)
      , m_numCurves(chainTag.m_numCurves)
      , m_maxThickness(chainTag.m_maxThickness) {
    if (m_numCurves > 0) {
      m_curve.reset(new TThickQuadratic[m_numCurves]);
      for (UINT i = 0; i < m_numCurves; i++) {
        m_curve[i] = chainTag.m_curve[i];
      }
    }
  }

private:
  // not implemented
  const ThickQuadraticChainTag &operator      =(
      const ThickQuadraticChainTag &chainTag) = delete;
};

//=====================================================================

//=====================================================================
/*!
Not yet implemented
*/
class BitmapTag final : public PliGeometricTag {
public:
  enum compressionType { NONE = 0, RLE, HOW_MANY_COMPRESSION };

  TRaster32P m_r;

  BitmapTag();
  BitmapTag(const TRaster32P &r);
  BitmapTag(const BitmapTag &bitmap);
  ~BitmapTag();
};

//=====================================================================
/*!
Not yet implemented
*/

class ColorTag final : public PliObjectTag {
public:
  enum styleType {
    STYLE_NONE = 0,
    SOLID,
    LINEAR_GRADIENT,
    RADIAL_GRADIENT,
    STYLE_HOW_MANY
  };
  enum attributeType {
    ATTRIBUTE_NONE = 0,
    EVENODD_LOOP_FILL,
    DIRECTION_LOOP_FILL,
    STROKE_COLOR,
    LEFT_STROKE_COLOR,
    RIGHT_STROKE_COLOR,
    ATTRIBUTE_HOW_MANY
  };

  styleType m_style;
  attributeType m_attribute;

  TUINT32 m_numColors;
  std::unique_ptr<TUINT32[]> m_color;

  ColorTag();
  ColorTag(styleType style, attributeType attribute, TUINT32 numColors,
           std::unique_ptr<TUINT32[]> color);
  ColorTag(const ColorTag &colorTag);
  ~ColorTag();
};

//=====================================================================

class StyleTag final : public PliObjectTag {
public:
  USHORT m_id;
  USHORT m_pageIndex;
  int m_numParams;
  std::unique_ptr<TStyleParam[]> m_param;

  StyleTag();
  StyleTag(int id, USHORT pagePaletteindex, int m_numParams,
           TStyleParam *m_params);
  StyleTag(const StyleTag &trasformationTag);
  ~StyleTag();
};

//=====================================================================

class GeometricTransformationTag final : public PliGeometricTag {
public:
  TAffine m_affine;
  PliGeometricTag *m_object;

  GeometricTransformationTag();
  GeometricTransformationTag(const TAffine &affine, PliGeometricTag *m_object);
  GeometricTransformationTag(
      const GeometricTransformationTag &trasformationTag);
  ~GeometricTransformationTag();
};

//=====================================================================

class GroupTag final : public PliObjectTag {
public:
  enum {
    NONE = 0,
    STROKE,
    SKETCH_STROKE,
    LOOP,
    FILL_SEED,  //(1 ColorTag + 1 pointTag)
    PALETTE,
    TYPE_HOW_MANY
  };

  UCHAR m_type;
  TUINT32 m_numObjects;
  std::unique_ptr<PliObjectTag *[]> m_object;

  GroupTag();
  GroupTag(UCHAR type, TUINT32 numObjects, PliObjectTag **object);
  GroupTag(UCHAR type, TUINT32 numObjects,
           std::unique_ptr<PliObjectTag *[]> object);
  GroupTag(const GroupTag &groupTag);
  ~GroupTag();
};

//=====================================================================

class ImageTag final : public PliObjectTag {
public:
  TFrameId m_numFrame;

  TUINT32 m_numObjects;
  std::unique_ptr<PliObjectTag *[]> m_object;

  // ImageTag();
  ImageTag(const TFrameId &numFrame, TUINT32 numObjects, PliObjectTag **object);
  ImageTag(const TFrameId &frameId, TUINT32 numObjects,
           std::unique_ptr<PliObjectTag *[]> object);
  ImageTag(const ImageTag &imageTag);
  ~ImageTag();
};

//=====================================================================

class DoublePairTag final : public PliObjectTag {
public:
  double m_first, m_second;

  DoublePairTag();
  DoublePairTag(double x, double y);
  DoublePairTag(const DoublePairTag &pointTag);
  ~DoublePairTag();
};

//=====================================================================

class IntersectionDataTag final : public PliObjectTag {
public:
  UINT m_branchCount;
  std::unique_ptr<TVectorImage::IntersectionBranch[]> m_branchArray;

  IntersectionDataTag();
  IntersectionDataTag(
      UINT branchCount,
      std::unique_ptr<TVectorImage::IntersectionBranch[]> branchArray);
  IntersectionDataTag(const IntersectionDataTag &tag);

  ~IntersectionDataTag();
};

//=====================================================================

class StrokeOutlineOptionsTag final : public PliObjectTag {
public:
  TStroke::OutlineOptions m_options;

  StrokeOutlineOptionsTag();
  StrokeOutlineOptionsTag(const TStroke::OutlineOptions &options);
};

//=====================================================================

class PrecisionScaleTag final : public PliObjectTag {
public:
  int m_precisionScale;

  PrecisionScaleTag();
  PrecisionScaleTag(int precisionScale);
};

//=====================================================================

class AutoCloseToleranceTag final : public PliObjectTag {
public:
  int m_autoCloseTolerance;

  AutoCloseToleranceTag();
  AutoCloseToleranceTag(int tolerance);
};

//=====================================================================

/*!
The class which will store the parsed info in reading.
(reading is realized by means of the constructor)
and that must be filled up in writing(using writePli).
Notice that implementation is opaque at this level (by means class
ParsedPliImp).
*/

class ParsedPliImp;
class TFilePath;
class TContentHistory;

class ParsedPli {
protected:
  ParsedPliImp *imp;

public:
  void setFrameCount(int);

  ParsedPli();
  ParsedPli(const TFilePath &filename, bool readInfo = false);
  ParsedPli(USHORT framesNumber, UCHAR precision, UCHAR maxThickness,
            double autocloseTolerance);
  ~ParsedPli();

  QString getCreator() const;
  void setCreator(const QString &creator);

  void getVersion(UINT &majorVersionNumber, UINT &minorVersionNumber) const;
  void setVersion(UINT majorVersionNumber, UINT minorVersionNumber);
  bool addTag(PliTag *tag, bool addFront = false);

  void loadInfo(bool readPalette, TPalette *&palette,
                TContentHistory *&history);
  ImageTag *loadFrame(const TFrameId &frameId);
  const TFrameId &getFrameNumber(int index);
  int getFrameCount() const;

  double getThickRatio() const;
  double getMaxThickness() const;
  void setMaxThickness(double maxThickness);
  double getAutocloseTolerance() const;
  void setAutocloseTolerance(int tolerance);
  int &precisionScale();
  // aggiuti questi 2 membri per salvare la paletta globale

  // vector<bool> m_idWrittenColorsArray;
  std::vector<PliObjectTag *> m_palette_tags;
  // these two functions are used to browse the tag list,
  // code example:   for (PliTag *tag = getFirstTag(); tag; tag = getNextTag())
  // {}

  PliTag *getFirstTag();
  PliTag *getNextTag();

  bool writePli(const TFilePath &filename);
  void setCreator(std::string creator);
};

#endif
