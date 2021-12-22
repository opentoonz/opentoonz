

// TnzBase includes
#include "trasterfx.h"

#include "ttzpimagefx.h"

//**********************************************************************************************
//    Global  functions
//**********************************************************************************************

void parseIndexes(std::string indexes, std::vector<std::string> &items) {
#ifdef _MSC_VER
  char seps[] = " ,;";
  char *token;
  if (indexes == "all" || indexes == "All" || indexes == "ALL")
    indexes     = "0-4095";
  char *context = 0;
  token         = strtok_s((char *)(indexes.c_str()), seps, &context);
  while (token != NULL) {
    items.push_back(token);
    token = strtok_s(NULL, seps, &context);
  }
#else
  char seps[] = " ,;";
  char *token;
  if (indexes == "all" || indexes == "All" || indexes == "ALL")
    indexes = "0-4095";
  token     = strtok((char *)(indexes.c_str()), seps);
  while (token != NULL) {
    items.push_back(token);
    token = strtok(NULL, seps);
  }
#endif
}

//-------------------------------------------------------------------

void insertIndexes(std::vector<std::string> items,
                   PaletteFilterFxRenderData *t) {
#ifdef _MSC_VER
  for (int i = 0; i < (int)items.size(); i++) {
    char *starttoken, *endtoken;
    char subseps[]  = "-";
    std::string tmp = items[i];
    char *context   = 0;
    starttoken      = strtok_s((char *)tmp.c_str(), subseps, &context);
    endtoken        = strtok_s(NULL, subseps, &context);
    if (!endtoken && isInt(starttoken)) {
      int index;
      index = std::stoi(starttoken);
      t->m_colors.insert(index);
    } else {
      if (isInt(starttoken) && isInt(endtoken)) {
        int start, end;
        start = std::stoi(starttoken);
        end   = std::stoi(endtoken);
        for (int i = start; i <= end; i++) t->m_colors.insert(i);
      }
    }
  }
#else
  for (int i = 0; i < (int)items.size(); i++) {
    char *starttoken, *endtoken;
    char subseps[]  = "-";
    std::string tmp = items[i];
    starttoken      = strtok((char *)tmp.c_str(), subseps);
    endtoken        = strtok(NULL, subseps);
    if (!endtoken && isInt(starttoken)) {
      int index;
      index = std::stoi(starttoken);
      t->m_colors.insert(index);
    } else {
      if (isInt(starttoken) && isInt(endtoken)) {
        int start, end;
        start = std::stoi(starttoken);
        end   = std::stoi(endtoken);
        for (int i = start; i <= end; i++) t->m_colors.insert(i);
      }
    }
  }
#endif
}

//**********************************************************************************************
//    ExternalPaletteFxRenderData  implementation
//**********************************************************************************************

ExternalPaletteFxRenderData::ExternalPaletteFxRenderData(
    TPaletteP palette, const std::string &name)
    : m_palette(palette), m_name(name) {}

//------------------------------------------------------------------------------

bool ExternalPaletteFxRenderData::operator==(
    const TRasterFxRenderData &data) const {
  return false;  // non sono capace. non ho i pezzi per browsare nell'albero da
                 // qui.
}

//------------------------------------------------------------------------------

std::string ExternalPaletteFxRenderData::toString() const { return m_name; }

//**********************************************************************************************
//    PaletteFilterFxRenderData  implementation
//**********************************************************************************************

PaletteFilterFxRenderData::PaletteFilterFxRenderData() {
  m_keep = false;
  m_type = eApplyToInksAndPaints;
}

//------------------------------------------------------------------------------

bool PaletteFilterFxRenderData::operator==(
    const TRasterFxRenderData &data) const {
  const PaletteFilterFxRenderData *theData =
      dynamic_cast<const PaletteFilterFxRenderData *>(&data);
  if (!theData) return false;

  return (theData->m_colors == m_colors && theData->m_type == m_type &&
          theData->m_keep == m_keep);
}

//------------------------------------------------------------------------------

std::string PaletteFilterFxRenderData::toString() const {
  std::string alias;
  std::set<int>::const_iterator it = m_colors.begin();
  for (; it != m_colors.end(); ++it) alias += std::to_string(*it);
  alias += "keep=" + std::to_string(m_keep);
  alias += "type=" + std::to_string(m_type);
  return alias;
}

//**********************************************************************************************
//    SandorFxRenderData  implementation
//**********************************************************************************************

SandorFxRenderData::SandorFxRenderData(Type type, int argc, const char *argv[],
                                       int border, int shrink,
                                       const TRectD &controllerBBox,
                                       const TRasterP &controller)
    : m_type(type)
    , m_border(border)
    , m_shrink(shrink)
    , m_blendParams()
    , m_callParams()
    , m_contourParams()
    , m_argc(argc)
    , m_controllerBBox(controllerBBox)
    , m_controller(controller)
    , m_controllerAlias() {
  for (int i = 0; i < argc; i++) m_argv[i] = argv[i];
}

//------------------------------------------------------------------------------

bool SandorFxRenderData::operator==(const TRasterFxRenderData &data) const {
  const SandorFxRenderData *theData =
      dynamic_cast<const SandorFxRenderData *>(&data);
  if (!theData) return false;

  if (m_type == BlendTz)
    return (theData->m_blendParams.m_colorIndex == m_blendParams.m_colorIndex &&
            theData->m_blendParams.m_noBlending == m_blendParams.m_noBlending &&
            theData->m_blendParams.m_amount == m_blendParams.m_amount &&
            theData->m_blendParams.m_smoothness == m_blendParams.m_smoothness);

  if (m_type == Calligraphic || m_type == OutBorder)
    return (theData->m_callParams.m_colorIndex == m_callParams.m_colorIndex &&
            theData->m_callParams.m_thickness == m_callParams.m_thickness &&
            theData->m_callParams.m_doWDiagonal == m_callParams.m_doWDiagonal &&
            theData->m_callParams.m_noise == m_callParams.m_noise &&
            theData->m_callParams.m_horizontal == m_callParams.m_horizontal &&
            theData->m_callParams.m_vertical == m_callParams.m_vertical &&
            theData->m_callParams.m_accuracy == m_callParams.m_accuracy &&
            theData->m_callParams.m_upWDiagonal == m_callParams.m_upWDiagonal);

  if (m_type == ArtAtContour)
    return (
        theData->m_contourParams.m_density == m_contourParams.m_density &&
        theData->m_contourParams.m_colorIndex == m_contourParams.m_colorIndex &&
        theData->m_contourParams.m_keepLine == m_contourParams.m_keepLine &&
        theData->m_contourParams.m_maxOrientation ==
            m_contourParams.m_maxOrientation &&
        theData->m_contourParams.m_maxDistance ==
            m_contourParams.m_maxDistance &&
        theData->m_contourParams.m_maxSize == m_contourParams.m_maxSize &&
        theData->m_contourParams.m_minOrientation ==
            m_contourParams.m_minOrientation &&
        theData->m_contourParams.m_minDistance ==
            m_contourParams.m_minDistance &&
        theData->m_contourParams.m_minSize == m_contourParams.m_minSize &&
        theData->m_contourParams.m_randomness == m_contourParams.m_randomness &&
        theData->m_contourParams.m_keepColor == m_contourParams.m_keepColor &&
        theData->m_contourParams.m_includeAlpha ==
            m_contourParams.m_includeAlpha &&
        theData->m_controllerAlias == m_controllerAlias);

  return false;
}

//------------------------------------------------------------------------------

std::string SandorFxRenderData::toString() const {
  std::string alias;
  if (m_type == BlendTz) {
    alias += ::to_string(m_blendParams.m_colorIndex) + " ";
    alias += std::to_string(m_blendParams.m_smoothness) + " ";
    alias += std::to_string(m_blendParams.m_amount) + " ";
    alias += std::to_string(m_blendParams.m_noBlending);
    return alias;
  }

  if (m_type == Calligraphic || m_type == OutBorder) {
    alias += ::to_string(m_callParams.m_colorIndex) + " ";
    alias += std::to_string(m_callParams.m_noise) + " ";
    alias += std::to_string(m_callParams.m_accuracy) + " ";
    alias += std::to_string(m_callParams.m_upWDiagonal) + " ";
    alias += std::to_string(m_callParams.m_vertical) + " ";
    alias += std::to_string(m_callParams.m_upWDiagonal) + " ";
    alias += std::to_string(m_callParams.m_horizontal) + " ";
    alias += std::to_string(m_callParams.m_thickness);
    return alias;
  }

  if (m_type == ArtAtContour) {
    alias += std::to_string(m_contourParams.m_maxSize) + " ";
    alias += std::to_string(m_contourParams.m_minSize) + " ";
    alias += std::to_string(m_contourParams.m_maxOrientation) + " ";
    alias += std::to_string(m_contourParams.m_minOrientation) + " ";
    alias += std::to_string(m_contourParams.m_randomness) + " ";
    alias += std::to_string(m_contourParams.m_maxDistance) + " ";
    alias += std::to_string(m_contourParams.m_minDistance) + " ";
    alias += std::to_string(m_contourParams.m_density) + " ";
    alias += std::to_string(m_contourParams.m_keepLine) + " ";
    alias += std::to_string(m_contourParams.m_keepColor) + " ";
    alias += std::to_string(m_contourParams.m_includeAlpha) + " ";
    alias += ::to_string(m_contourParams.m_colorIndex) + " ";
    alias += m_controllerAlias;
    return alias;
  }

  return alias;
}

//------------------------------------------------------------------------------

TRectD SandorFxRenderData::getBBoxEnlargement(const TRectD &bbox) {
  switch (m_type) {
  case BlendTz: {
    // Nothing happen, unless we have color 0 among the blended ones. In such
    // case,
    // we have to enlarge the bbox proportionally to the amount param.
    std::vector<std::string> items;
    std::string indexes = std::string(m_argv[0]);
    parseIndexes(indexes, items);
    PaletteFilterFxRenderData paletteFilterData;
    insertIndexes(items, &paletteFilterData);

    if (paletteFilterData.m_colors.size() > 0 &&
        *paletteFilterData.m_colors.begin() == 0)
      return bbox.enlarge(m_blendParams.m_amount);

    return bbox;
  }

  case Calligraphic:
  case OutBorder:
    return bbox.enlarge(m_callParams.m_thickness);

  case ArtAtContour:
    return bbox.enlarge(std::max(tceil(m_controllerBBox.getLx()),
                                 tceil(m_controllerBBox.getLy())) *
                        m_contourParams.m_maxSize);

  default:
    assert(false);
    return bbox;
  }
}
