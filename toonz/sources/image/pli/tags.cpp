

#ifdef _MSC_VER
#pragma warning(disable : 4661)
#endif

#include "pli_io.h"

typedef TVectorImage::IntersectionBranch IntersectionBranch;

/*=====================================================================*/

PliTag::PliTag() : m_type(NONE) {}

/*=====================================================================*/

PliTag::PliTag(const Type type) : m_type(type) {}

/*=====================================================================*/

// PliTag::PliTag(const PliTag &tag)
//: m_type(tag.m_type)
//{}

/*=====================================================================*/
/*=====================================================================*/

PliObjectTag::PliObjectTag() : PliTag() {}

/*=====================================================================*/

PliObjectTag::PliObjectTag(const Type type) : PliTag(type) {}

/*=====================================================================*/
/*=====================================================================*/

PliGeometricTag::PliGeometricTag() : PliObjectTag() {}

/*=====================================================================*/

PliGeometricTag::PliGeometricTag(const Type type) : PliObjectTag(type) {}

/*=====================================================================*/
/*=====================================================================*/

TextTag::TextTag() : PliObjectTag(PliTag::TEXT), m_text() {}

/*=====================================================================*/

TextTag::TextTag(const TextTag &textTag)
    : PliObjectTag(textTag), m_text(textTag.m_text) {}

/*=====================================================================*/

TextTag::TextTag(const std::string &text)
    : PliObjectTag(PliTag::TEXT), m_text(text) {}

/*=====================================================================*/
/*=====================================================================*/

PaletteTag::PaletteTag()
    : PliTag(PliTag::PALETTE), m_numColors(0), m_color(NULL) {}

/*=====================================================================*/

PaletteTag::PaletteTag(TUINT32 numColors, TPixelRGBM32 *color)
    : PliTag(PliTag::PALETTE) {
  m_numColors = numColors;

  if (m_numColors == 0)
    m_color = NULL;
  else {
    m_color = new TPixelRGBM32[m_numColors];
    for (UINT i = 0; i < m_numColors; i++) m_color[i] = color[i];
  }
}

/*=====================================================================*/

PaletteTag::PaletteTag(const PaletteTag &paletteTag) : PliTag(PliTag::PALETTE) {
  m_numColors = paletteTag.m_numColors;
  if (m_numColors == 0)
    m_color = NULL;
  else {
    m_color = new TPixelRGBM32[m_numColors];
    for (UINT i = 0; i < m_numColors; i++) m_color[i] = paletteTag.m_color[i];
  }
}

/*=====================================================================*/

PaletteTag::~PaletteTag() { delete m_color; }

/*=====================================================================*/
/*=====================================================================*/

PaletteWithAlphaTag::PaletteWithAlphaTag()
    : PliTag(PliTag::PALETTE_WITH_ALPHA), m_numColors(0), m_color(NULL) {}

/*=====================================================================*/

PaletteWithAlphaTag::PaletteWithAlphaTag(TUINT32 numColors, TPixelRGBM32 *color)
    : PliTag(PliTag::PALETTE_WITH_ALPHA) {
  m_numColors = numColors;

  if (m_numColors == 0)
    m_color = NULL;
  else {
    m_color = new TPixelRGBM32[m_numColors];
    for (UINT i = 0; i < m_numColors; i++) m_color[i] = color[i];
  }
}

/*=====================================================================*/

PaletteWithAlphaTag::PaletteWithAlphaTag(PaletteWithAlphaTag &paletteTag)
    : PliTag(PliTag::PALETTE_WITH_ALPHA) {
  m_numColors = paletteTag.m_numColors;
  if (m_numColors == 0)
    m_color = NULL;
  else {
    m_color = new TPixelRGBM32[m_numColors];
    for (UINT i = 0; i < m_numColors; i++) m_color[i] = paletteTag.m_color[i];
  }
}

/*=====================================================================*/

PaletteWithAlphaTag::~PaletteWithAlphaTag() { delete m_color; }

/*=====================================================================*/
/*=====================================================================*/

GroupTag::GroupTag()
    : PliObjectTag(PliTag::GROUP_GOBJ)
    , m_type(GroupTag::NONE)
    , m_numObjects(0) {}

/*=====================================================================*/
GroupTag::GroupTag(UCHAR type, TUINT32 numObjects, PliObjectTag **object)
    : PliObjectTag(PliTag::GROUP_GOBJ), m_type(type), m_numObjects(numObjects) {
  if (m_numObjects > 0) {
    m_object.reset(new PliObjectTag *[m_numObjects]);
    for (UINT i = 0; i < m_numObjects; i++) {
      m_object[i] = object[i];
    }
  }
}

GroupTag::GroupTag(UCHAR type, TUINT32 numObjects,
                   std::unique_ptr<PliObjectTag *[]> object)
    : PliObjectTag(PliTag::GROUP_GOBJ)
    , m_type(type)
    , m_numObjects(numObjects)
    , m_object(std::move(object)) {}

/*=====================================================================*/

GroupTag::GroupTag(const GroupTag &groupTag)
    : PliObjectTag(PliTag::GROUP_GOBJ)
    , m_type(groupTag.m_type)
    , m_numObjects(groupTag.m_numObjects) {
  if (m_numObjects > 0) {
    m_object.reset(new PliObjectTag *[m_numObjects]);
    for (UINT i = 0; i < m_numObjects; i++) {
      m_object[i] = groupTag.m_object[i];
    }
  }
}

/*=====================================================================*/

GroupTag::~GroupTag() {}

/*=====================================================================*/
/*=====================================================================*/

StyleTag::StyleTag()
    : PliObjectTag(PliTag::STYLE_NGOBJ)
    , m_id(0)
    , m_numParams(0)
    , m_pageIndex(0) {}

/*=====================================================================*/

StyleTag::StyleTag(int id, USHORT pagePaletteIndex, int numParams,
                   TStyleParam *param)
    : PliObjectTag(PliTag::STYLE_NGOBJ)
    , m_id(id)
    , m_pageIndex(pagePaletteIndex)
    , m_numParams(numParams) {
  if (numParams > 0) {
    m_param.reset(new TStyleParam[m_numParams]);
    for (UINT i = 0; i < (UINT)m_numParams; i++) {
      m_param[i] = param[i];
    }
  }
}

/*=====================================================================*/

StyleTag::StyleTag(const StyleTag &styleTag)
    : PliObjectTag(PliTag::STYLE_NGOBJ)
    , m_id(styleTag.m_id)
    , m_pageIndex(styleTag.m_pageIndex)
    , m_numParams(styleTag.m_numParams) {
  if (styleTag.m_numParams > 0) {
    m_param.reset(new TStyleParam[styleTag.m_numParams]);
    for (UINT i = 0; i < (UINT)styleTag.m_numParams; i++) {
      m_param[i] = styleTag.m_param[i];
    }
  }
}

/*=====================================================================*/

StyleTag::~StyleTag() {}

//=====================================================================

ColorTag::ColorTag()
    : PliObjectTag(PliTag::COLOR_NGOBJ)
    , m_style(STYLE_NONE)
    , m_attribute(ATTRIBUTE_NONE)
    , m_numColors(0) {}

/*=====================================================================*/

ColorTag::ColorTag(ColorTag::styleType style, ColorTag::attributeType attribute,
                   TUINT32 numColors, std::unique_ptr<TUINT32[]> color)
    : PliObjectTag(PliTag::COLOR_NGOBJ)
    , m_style(style)
    , m_attribute(attribute)
    , m_numColors(numColors)
    , m_color(std::move(color)) {}
/*=====================================================================*/

ColorTag::ColorTag(const ColorTag &tag)
    : PliObjectTag(PliTag::COLOR_NGOBJ)
    , m_style(tag.m_style)
    , m_attribute(tag.m_attribute)
    , m_numColors(tag.m_numColors) {
  if (tag.m_numColors > 0) {
    m_color.reset(new TUINT32[m_numColors]);
    for (UINT i = 0; i < m_numColors; i++) {
      m_color[i] = tag.m_color[i];
    }
  }
}

/*=====================================================================*/

ColorTag::~ColorTag() {}

/*=====================================================================*/
/*=====================================================================*/
/*=====================================================================*/

BitmapTag::BitmapTag() : m_r() {}

/*=====================================================================*/

BitmapTag::BitmapTag(const TRaster32P &r) : m_r(r) {}

/*=====================================================================*/

BitmapTag::BitmapTag(const BitmapTag &bitmap) : m_r(bitmap.m_r) {}

/*=====================================================================*/

BitmapTag::~BitmapTag() {}

/*=====================================================================*/
/*=====================================================================*/

IntersectionDataTag::IntersectionDataTag()
    : PliObjectTag(PliTag::INTERSECTION_DATA_GOBJ), m_branchCount(0) {}

/*=====================================================================*/

IntersectionDataTag::IntersectionDataTag(
    UINT branchCount, std::unique_ptr<IntersectionBranch[]> branchArray)
    : PliObjectTag(PliTag::INTERSECTION_DATA_GOBJ)
    , m_branchCount(branchCount)
    , m_branchArray(std::move(branchArray)) {}

/*=====================================================================*/

IntersectionDataTag::IntersectionDataTag(const IntersectionDataTag &tag)
    : PliObjectTag(PliTag::INTERSECTION_DATA_GOBJ)
    , m_branchCount(tag.m_branchCount) {
  if (m_branchCount == 0) {
    m_branchArray.reset(new IntersectionBranch[m_branchCount]);
    for (UINT i = 0; i < m_branchCount; i++) {
      m_branchArray[i] = tag.m_branchArray[i];
    }
  }
}

/*=====================================================================*/

IntersectionDataTag::~IntersectionDataTag() {}

/*=====================================================================*/

StrokeOutlineOptionsTag::StrokeOutlineOptionsTag()
    : PliObjectTag(OUTLINE_OPTIONS_GOBJ) {}

/*=====================================================================*/

StrokeOutlineOptionsTag::StrokeOutlineOptionsTag(
    const TStroke::OutlineOptions &options)
    : PliObjectTag(OUTLINE_OPTIONS_GOBJ), m_options(options) {}

/*=====================================================================*/

PrecisionScaleTag::PrecisionScaleTag() : PliObjectTag(PRECISION_SCALE_GOBJ) {}

/*=====================================================================*/

PrecisionScaleTag::PrecisionScaleTag(int precisionScale)
    : PliObjectTag(PRECISION_SCALE_GOBJ), m_precisionScale(precisionScale) {}

/*=====================================================================*/

AutoCloseToleranceTag::AutoCloseToleranceTag()
    : PliObjectTag(AUTOCLOSE_TOLERANCE_GOBJ) {}

/*=====================================================================*/

AutoCloseToleranceTag::AutoCloseToleranceTag(int tolerance)
    : PliObjectTag(AUTOCLOSE_TOLERANCE_GOBJ), m_autoCloseTolerance(tolerance) {}

/*=====================================================================*/
/*=====================================================================*/

/*=====================================================================*/
/*=====================================================================*/

/*ImageTag::ImageTag()
: PliObjectTag(PliTag::IMAGE_GOBJ)
, m_numFrame(0)
, m_numObjects(0)
, m_object(NULL)
{
}*/

/*=====================================================================*/
ImageTag::ImageTag(const TFrameId &numFrame, TUINT32 numObjects,
                   PliObjectTag **object)
    : PliObjectTag(PliTag::IMAGE_GOBJ)
    , m_numFrame(numFrame)
    , m_numObjects(numObjects) {
  if (m_numObjects > 0) {
    m_object.reset(new PliObjectTag *[m_numObjects]);
    for (UINT i = 0; i < m_numObjects; i++) {
      m_object[i] = object[i];
    }
  }
}

ImageTag::ImageTag(const TFrameId &numFrame, TUINT32 numObjects,
                   std::unique_ptr<PliObjectTag *[]> object)
    : PliObjectTag(PliTag::IMAGE_GOBJ)
    , m_numFrame(numFrame)
    , m_numObjects(numObjects)
    , m_object(std::move(object)) {}

/*=====================================================================*/

ImageTag::ImageTag(const ImageTag &imageTag)
    : PliObjectTag(PliTag::IMAGE_GOBJ)
    , m_numFrame(imageTag.m_numFrame)
    , m_numObjects(imageTag.m_numObjects) {
  if (m_numObjects > 0) {
    m_object.reset(new PliObjectTag *[m_numObjects]);
    for (UINT i = 0; i < m_numObjects; i++) {
      m_object[i] = imageTag.m_object[i];
    }
  }
}

/*=====================================================================*/

ImageTag::~ImageTag() {}

/*=====================================================================*/
/*=====================================================================*/

GeometricTransformationTag::GeometricTransformationTag()
    : PliGeometricTag(PliTag::GEOMETRIC_TRANSFORMATION_GOBJ)
    , m_affine()
    , m_object(NULL) {}

/*=====================================================================*/

GeometricTransformationTag::GeometricTransformationTag(
    const GeometricTransformationTag &trasformationTag)
    : PliGeometricTag(PliTag::GEOMETRIC_TRANSFORMATION_GOBJ)
    , m_affine(trasformationTag.m_affine)
    , m_object(trasformationTag.m_object) {}

/*=====================================================================*/

GeometricTransformationTag::GeometricTransformationTag(const TAffine &affine,
                                                       PliGeometricTag *object)
    : PliGeometricTag(PliTag::GEOMETRIC_TRANSFORMATION_GOBJ)
    , m_affine(affine)
    , m_object(object) {}

/*=====================================================================*/
GeometricTransformationTag::~GeometricTransformationTag() {
  // if (m_object) delete m_object;
}

/*=====================================================================*/

DoublePairTag::DoublePairTag()
    : PliObjectTag(PliTag::DOUBLEPAIR_OBJ), m_first(0), m_second(0) {}

/*=====================================================================*/

DoublePairTag::DoublePairTag(const DoublePairTag &doublePairTag)
    : PliObjectTag(PliTag::DOUBLEPAIR_OBJ)
    , m_first(doublePairTag.m_first)
    , m_second(doublePairTag.m_second) {}

/*=====================================================================*/

DoublePairTag::DoublePairTag(double first, double second)
    : PliObjectTag(PliTag::DOUBLEPAIR_OBJ), m_first(first), m_second(second) {}

/*=====================================================================*/
DoublePairTag::~DoublePairTag() {
  // if (m_object) delete m_object;
}
