

// TnzCore includes
#include "tgeometry.h"
#include "tstream.h"
#include "texception.h"

#include "toonz/vectorizerparameters.h"

//**************************************************************************
//    Local namespace  strings
//**************************************************************************

namespace {

const char *s_version = "version",

           *s_generalConfiguration = "generalConfiguration",
           *s_outline = "outline", *s_threshold = "threshold",
           *s_leaveUnpainted = "leaveUnpainted",
           *s_visibilityBits = "visibilityBits",

           *s_Centerline = "Centerline", *s_despeckling = "despeckling",
           *s_maxThickness = "maxThickness", *s_penalty = "penalty",
           *s_thicknessRatio = "thicknessRatio", *s_makeFrame = "makeFrame",
           *s_naaSource = "naaSource",

           *s_accuracy                      = "accuracy",
           *s_thicknessRatioFirst           = "thicknessRatioFirst",
           *s_thicknessRatioLast            = "thicknessRatioLast",
           *s_paintFill                     = "paintFill",
           *s_alignBoundaryStrokesDirection = "alignBoundaryStrokesDirection",

           *s_Outline = "Outline", *s_adherenceTol = "adherenceTol",
           *s_angleTol = "angleTol", *s_relativeTol = "relativeTol",
           *s_mergeTol = "mergeTol", *s_maxColors = "maxColors",
           *s_transparentColor = "transparentColor", *s_toneTol = "toneTol",

           *s_adherence = "adherence", *s_angle = "angle",
           *s_relative = "relative", *s_toneThreshold = "toneThreshold";

}  // namespace

//**************************************************************************
//    Local namespace  stuff
//**************************************************************************

namespace {

const VersionNumber l_versionNumber(71, 0);

//=======================================================

void saveData(const VectorizerConfiguration &conf, TOStream &os) {
  int leaveUnpainted = conf.m_leaveUnpainted ? 1 : 0;

  os.child(s_threshold) << conf.m_threshold;
  os.child(s_leaveUnpainted) << leaveUnpainted;
}

//-------------------------------------------------------

void loadData(VectorizerConfiguration &conf, TIStream &is) {
  std::string tagName;
  while (is.matchTag(tagName)) {
    if (tagName == s_threshold)
      is >> conf.m_threshold, is.matchEndTag();
    else if (tagName == s_leaveUnpainted) {
      int leaveUnpainted;
      is >> leaveUnpainted,
          conf.m_leaveUnpainted = (leaveUnpainted == 0) ? false : true;

      is.matchEndTag();
    } else
      is.skipCurrentTag();
  }
}

//-------------------------------------------------------

void saveData(const CenterlineConfiguration &conf, TOStream &os) {
  os.openChild(s_generalConfiguration);
  saveData(static_cast<const VectorizerConfiguration &>(conf), os);
  os.closeChild();

  int makeFrame = conf.m_makeFrame ? 1 : 0;

  os.child(s_despeckling) << conf.m_despeckling;
  os.child(s_maxThickness) << conf.m_maxThickness;
  os.child(s_penalty) << conf.m_penalty;
  os.child(s_thicknessRatio) << conf.m_thicknessRatio;
  os.child(s_makeFrame) << makeFrame;
  os.child(s_naaSource) << (conf.m_naaSource ? 1 : 0);
}

//-------------------------------------------------------

void loadData(CenterlineConfiguration &conf, TIStream &is) {
  std::string tagName;
  while (is.matchTag(tagName)) {
    if (tagName == s_generalConfiguration)
      loadData(static_cast<VectorizerConfiguration &>(conf), is),
          is.matchEndTag();
    else if (tagName == s_despeckling)
      is >> conf.m_despeckling, is.matchEndTag();
    else if (tagName == s_maxThickness)
      is >> conf.m_maxThickness, is.matchEndTag();
    else if (tagName == s_penalty)
      is >> conf.m_penalty, is.matchEndTag();
    else if (tagName == s_thicknessRatio)
      is >> conf.m_thicknessRatio, is.matchEndTag();
    else if (tagName == s_makeFrame) {
      int makeFrame;
      is >> makeFrame, conf.m_makeFrame = (makeFrame == 0) ? false : true;
      is.matchEndTag();
    } else if (tagName == s_naaSource) {
      int naaSource;
      is >> naaSource, conf.m_naaSource = (naaSource == 0) ? false : true;
      is.matchEndTag();
    } else
      is.skipCurrentTag();
  }
}

//-------------------------------------------------------

void saveData(const NewOutlineConfiguration &conf, TOStream &os) {
  os.openChild(s_generalConfiguration);
  saveData(static_cast<const VectorizerConfiguration &>(conf), os);
  os.closeChild();

  os.child(s_despeckling) << conf.m_despeckling;
  os.child(s_adherenceTol) << conf.m_adherenceTol;
  os.child(s_angleTol) << conf.m_angleTol;
  os.child(s_relativeTol) << conf.m_relativeTol;
  os.child(s_mergeTol) << conf.m_mergeTol;
  os.child(s_maxColors) << conf.m_maxColors;
  os.child(s_transparentColor) << conf.m_transparentColor;
  os.child(s_toneTol) << conf.m_toneTol;
}

//-------------------------------------------------------

void loadData(NewOutlineConfiguration &conf, TIStream &is) {
  std::string tagName;
  while (is.matchTag(tagName)) {
    if (tagName == s_generalConfiguration)
      loadData(static_cast<VectorizerConfiguration &>(conf), is),
          is.matchEndTag();
    else if (tagName == s_despeckling)
      is >> conf.m_despeckling, is.matchEndTag();
    else if (tagName == s_adherenceTol)
      is >> conf.m_adherenceTol, is.matchEndTag();
    else if (tagName == s_angleTol)
      is >> conf.m_angleTol, is.matchEndTag();
    else if (tagName == s_relativeTol)
      is >> conf.m_relativeTol, is.matchEndTag();
    else if (tagName == s_mergeTol)
      is >> conf.m_mergeTol, is.matchEndTag();
    else if (tagName == s_maxColors)
      is >> conf.m_maxColors, is.matchEndTag();
    else if (tagName == s_transparentColor)
      is >> conf.m_transparentColor, is.matchEndTag();
    else if (tagName == s_toneTol)
      is >> conf.m_toneTol, is.matchEndTag();
    else
      is.skipCurrentTag();
  }
}

//-------------------------------------------------------

void convert(const CenterlineConfiguration &conf,
             VectorizerParameters &params) {
  params.m_cThreshold    = conf.m_threshold / 25.0;
  params.m_cAccuracy     = 10 - conf.m_penalty;
  params.m_cDespeckling  = conf.m_despeckling / 2.0;
  params.m_cMaxThickness = conf.m_maxThickness * 2;
  // params.m_cThicknessRatio        = centConf.m_thicknessRatio;
  params.m_cThicknessRatioFirst = conf.m_thicknessRatio;
  params.m_cThicknessRatioLast  = conf.m_thicknessRatio;
  params.m_cMakeFrame           = conf.m_makeFrame;
  params.m_cPaintFill           = !conf.m_leaveUnpainted;
  params.m_cAlignBoundaryStrokesDirection =
      conf.m_alignBoundaryStrokesDirection;
  params.m_cNaaSource = conf.m_naaSource;
}

//-------------------------------------------------------

void convert(const NewOutlineConfiguration &conf,
             VectorizerParameters &params) {
  params.m_oDespeckling      = conf.m_despeckling;
  params.m_oAccuracy         = tround((5.0 - conf.m_mergeTol) * 2.0);
  params.m_oAdherence        = tround(conf.m_adherenceTol * 100.0);
  params.m_oAngle            = tround(conf.m_angleTol * 180.0);
  params.m_oRelative         = tround(conf.m_relativeTol * 100.0);
  params.m_oMaxColors        = conf.m_maxColors;
  params.m_oToneThreshold    = conf.m_toneTol;
  params.m_oTransparentColor = conf.m_transparentColor;
  params.m_oPaintFill        = !conf.m_leaveUnpainted;
  params.m_oAlignBoundaryStrokesDirection =
      conf.m_alignBoundaryStrokesDirection;
}

}  // namespace

//**************************************************************************
//    VectorizerConfiguration  implementation
//**************************************************************************

PERSIST_IDENTIFIER(VectorizerParameters, "vectorizerParameters")

//--------------------------------------------------------------------------

VectorizerParameters::VectorizerParameters()
    : m_visibilityBits(-1)  // All visible by default
    , m_isOutline(false) {
  CenterlineConfiguration cConf;
  convert(cConf, *this);

  NewOutlineConfiguration oConf;
  convert(oConf, *this);
}

//--------------------------------------------------------------------------

CenterlineConfiguration VectorizerParameters::getCenterlineConfiguration(
    double frame) const {
  CenterlineConfiguration conf;

  conf.m_outline      = false;
  conf.m_threshold    = m_cThreshold * 25;
  conf.m_penalty      = 10 - m_cAccuracy;  // m_cAccuracy in [1,10]
  conf.m_despeckling  = m_cDespeckling * 2;
  conf.m_maxThickness = m_cMaxThickness / 2.0;
  // conf.m_thicknessRatio   = m_cThicknessRatio;
  conf.m_thicknessRatio =
      (1 - frame) * m_cThicknessRatioFirst + frame * m_cThicknessRatioLast;
  conf.m_leaveUnpainted                = !m_cPaintFill;
  conf.m_alignBoundaryStrokesDirection = m_cAlignBoundaryStrokesDirection;
  conf.m_makeFrame                     = m_cMakeFrame;
  conf.m_naaSource                     = m_cNaaSource;

  return conf;
}

//--------------------------------------------------------------------------

NewOutlineConfiguration VectorizerParameters::getOutlineConfiguration(
    double frame) const {
  NewOutlineConfiguration conf;

  conf.m_outline                       = true;
  conf.m_despeckling                   = m_oDespeckling;
  conf.m_adherenceTol                  = m_oAdherence * 0.01;
  conf.m_angleTol                      = m_oAngle / 180.0;
  conf.m_relativeTol                   = m_oRelative * 0.01;
  conf.m_mergeTol                      = 5.0 - m_oAccuracy * 0.5;
  conf.m_leaveUnpainted                = !m_oPaintFill;
  conf.m_alignBoundaryStrokesDirection = m_oAlignBoundaryStrokesDirection;
  conf.m_maxColors                     = m_oMaxColors;
  conf.m_transparentColor              = m_oTransparentColor;
  conf.m_toneTol                       = m_oToneThreshold;

  return conf;
}

//--------------------------------------------------------------------------

void VectorizerParameters::saveData(TOStream &os) {
  os.child(s_version) << l_versionNumber.first << l_versionNumber.second;
  os.child(s_outline) << (m_isOutline ? 1 : 0);
  os.child(s_visibilityBits) << int(m_visibilityBits);

  os.openChild(s_Centerline);
  {
    os.child(s_threshold) << m_cThreshold;
    os.child(s_accuracy) << m_cAccuracy;
    os.child(s_despeckling) << m_cDespeckling;
    os.child(s_maxThickness) << m_cMaxThickness;
    os.child(s_thicknessRatioFirst) << m_cThicknessRatioFirst;
    os.child(s_thicknessRatioLast) << m_cThicknessRatioLast;
    os.child(s_makeFrame) << (m_cMakeFrame ? 1 : 0);
    os.child(s_paintFill) << (m_cPaintFill ? 1 : 0);
    os.child(s_alignBoundaryStrokesDirection)
        << (m_cAlignBoundaryStrokesDirection ? 1 : 0);
    os.child(s_naaSource) << (m_cNaaSource ? 1 : 0);
  }
  os.closeChild();

  os.openChild(s_Outline);
  {
    os.child(s_despeckling) << m_oDespeckling;
    os.child(s_accuracy) << m_oAccuracy;
    os.child(s_adherence) << m_oAdherence;
    os.child(s_angle) << m_oAngle;
    os.child(s_relative) << m_oRelative;
    os.child(s_maxColors) << m_oMaxColors;
    os.child(s_toneThreshold) << m_oToneThreshold;
    os.child(s_transparentColor) << m_oTransparentColor;
    os.child(s_paintFill) << (m_oPaintFill ? 1 : 0);
    os.child(s_alignBoundaryStrokesDirection)
        << (m_oAlignBoundaryStrokesDirection ? 1 : 0);
  }
  os.closeChild();
}

//--------------------------------------------------------------------------

void VectorizerParameters::loadData(TIStream &is) {
  VersionNumber version;

  std::string tagName;
  int val;

  while (is.matchTag(tagName)) {
    if (tagName == s_version)
      is >> version.first >> version.second, is.matchEndTag();
    else if (tagName == s_outline)
      is >> val, m_isOutline = (val != 0), is.matchEndTag();
    else if (tagName == s_visibilityBits) {
      is >> val, is.matchEndTag();

      if (version ==
          l_versionNumber)  // Restore visibility bits only if the parameters
        m_visibilityBits =
            val;  // version is current - defaulting them is no big deal.
    } else if (tagName == s_Centerline) {
      while (is.matchTag(tagName)) {
        if (tagName == s_threshold)
          is >> m_cThreshold, is.matchEndTag();
        else if (tagName == s_accuracy)
          is >> m_cAccuracy, is.matchEndTag();
        else if (tagName == s_despeckling)
          is >> m_cDespeckling, is.matchEndTag();
        else if (tagName == s_maxThickness)
          is >> m_cMaxThickness, is.matchEndTag();
        else if (tagName == s_thicknessRatioFirst)
          is >> m_cThicknessRatioFirst, is.matchEndTag();
        else if (tagName == s_thicknessRatioLast)
          is >> m_cThicknessRatioLast, is.matchEndTag();
        else if (tagName == s_makeFrame)
          is >> val, m_cMakeFrame = (val != 0), is.matchEndTag();
        else if (tagName == s_paintFill)
          is >> val, m_cPaintFill = (val != 0), is.matchEndTag();
        else if (tagName == s_alignBoundaryStrokesDirection)
          is >> val, m_cAlignBoundaryStrokesDirection = (val != 0),
                     is.matchEndTag();
        else if (tagName == s_naaSource)
          is >> val, m_cNaaSource = (val != 0), is.matchEndTag();
        else
          is.skipCurrentTag();
      }

      is.matchEndTag();
    } else if (tagName == s_Outline) {
      while (is.matchTag(tagName)) {
        if (tagName == s_despeckling)
          is >> m_oDespeckling, is.matchEndTag();
        else if (tagName == s_accuracy)
          is >> m_oAccuracy, is.matchEndTag();
        else if (tagName == s_adherence)
          is >> m_oAdherence, is.matchEndTag();
        else if (tagName == s_angle)
          is >> m_oAngle, is.matchEndTag();
        else if (tagName == s_relative)
          is >> m_oRelative, is.matchEndTag();
        else if (tagName == s_maxColors)
          is >> m_oMaxColors, is.matchEndTag();
        else if (tagName == s_toneThreshold)
          is >> m_oToneThreshold, is.matchEndTag();
        else if (tagName == s_transparentColor)
          is >> m_oTransparentColor, is.matchEndTag();
        else if (tagName == s_paintFill)
          is >> val, m_oPaintFill = (val != 0), is.matchEndTag();
        else if (tagName == s_alignBoundaryStrokesDirection)
          is >> val, m_oAlignBoundaryStrokesDirection = (val != 0),
                     is.matchEndTag();
        else
          is.skipCurrentTag();
      }

      is.matchEndTag();
    } else if (tagName ==
               "CenterlineConfiguration")  // Old tags, not saved anymore
    {
      CenterlineConfiguration conf;
      ::loadData(conf, is), is.matchEndTag();

      convert(conf, *this);
    } else if (tagName == "NewOutlineConfiguration")  //
    {
      NewOutlineConfiguration conf;
      ::loadData(conf, is), is.matchEndTag();

      convert(conf, *this);
    } else
      is.skipCurrentTag();
  }
}
