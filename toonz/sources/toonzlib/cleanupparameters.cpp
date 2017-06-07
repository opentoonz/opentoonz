

#include "toonz/cleanupparameters.h"
#include "tstream.h"
#include "texception.h"
#include "toonz/toonzscene.h"
#include "toonz/sceneproperties.h"
#include "toonz/txshleveltypes.h"
#include "tsystem.h"
#include "tenv.h"
#include "tconvert.h"
#include "cleanuppalette.h"
#include "tpalette.h"

#ifdef _MSC_VER
#pragma warning(disable : 4996)
#endif

using namespace CleanupTypes;

#define FDG_VERSION_FILE "VERSION_1.0"

CleanupParameters CleanupParameters::GlobalParameters;
CleanupParameters CleanupParameters::LastSavedParameters;
//=========================================================

namespace {

//=========================================================

class FdgManager {  // singleton

  std::map<std::string, FDG_INFO> m_infos;

  void loadFieldGuideInfo();
  FdgManager() { loadFieldGuideInfo(); }

public:
  static FdgManager *instance() {
    static FdgManager _instance;
    return &_instance;
  }

  const FDG_INFO *getFdg(std::string name) const {
    std::map<std::string, FDG_INFO>::const_iterator it;
    it = m_infos.find(name);
    if (it == m_infos.end())
      return 0;
    else
      return &it->second;
  }

  void getFdgNames(std::vector<std::string> &names) const {
    std::map<std::string, FDG_INFO>::const_iterator it;
    for (it = m_infos.begin(); it != m_infos.end(); ++it)
      names.push_back(it->first);
  }
};

//---------------------------------------------------------

#define REMOVE_CR(s)                                                           \
  {                                                                            \
    while (*(s) && *(s) != '\n' && *(s) != '\r') (s)++;                        \
    *(s) = '\0';                                                               \
  }

#define SKIP_BLANKS(s)                                                         \
  {                                                                            \
    while (*(s) && (*(s) == ' ' || *(s) == '\t')) (s)++;                       \
  }

#define STR_EQ(s1, s2) (!strcmp((s1), (s2)))

//---------------------------------------------------------

void FdgManager::loadFieldGuideInfo() {
  FILE *fp;
  char *s, buffer[256], label[100], arg[100];
  int i, version = 0, cnt;

  try {
    TFilePathSet fps =
        TSystem::readDirectory(TEnv::getConfigDir() + "fdg", false);
    for (TFilePathSet::iterator it = fps.begin(); it != fps.end(); ++it) {
      TFilePath fname = *it;
      if (fname.getType() != "fdg") continue;
      fp = fopen(::to_string(fname.getWideString()).c_str(), "r");
      if (!fp) continue;
      FDG_INFO fdg_info;

      // memset(&fdg_info,0, sizeof(FDG_INFO));

      fdg_info.dots.resize(3);

      cnt = 0;
      while (fgets(buffer, 256, fp)) {
        s = buffer;
        if (*s == '#' || *s == '*') continue;
        REMOVE_CR(s)
        s = buffer;
        SKIP_BLANKS(s)
        if (!*s) continue;
        for (i = 0; *s && *s != ' ' && *s != '\t'; i++, s++) label[i] = *s;
        label[i] = '\0';
        SKIP_BLANKS(s)
        for (i = 0; *s && *s != ' ' && *s != '\t'; i++, s++) arg[i] = *s;
        arg[i] = '\0';

        /* suppongo ci siano sempre 3 dots */

        if (STR_EQ(label, FDG_VERSION_FILE))
          version = 1;
        else if (STR_EQ(label, "CTRX"))
          sscanf(arg, "%lf", &fdg_info.ctr_x), fdg_info.ctr_type = 1;
        else if (STR_EQ(label, "CTRY"))
          sscanf(arg, "%lf", &fdg_info.ctr_y), fdg_info.ctr_type = 1;
        else if (STR_EQ(label, "ANGLE"))
          sscanf(arg, "%lf", &fdg_info.ctr_angle), fdg_info.ctr_type = 1;
        else if (STR_EQ(label, "SKEW"))
          sscanf(arg, "%lf", &fdg_info.ctr_skew), fdg_info.ctr_type = 1;
        else if (STR_EQ(label, "HOLE")) {
          sscanf(arg, "%d", &cnt);
          if (cnt >= (int)fdg_info.dots.size()) {
            fdg_info.dots.push_back(CleanupTypes::DOT());
          }
        } else if (STR_EQ(label, "X0"))
          sscanf(arg, "%d", &fdg_info.dots[cnt].x1);
        else if (STR_EQ(label, "Y0"))
          sscanf(arg, "%d", &fdg_info.dots[cnt].y1);
        else if (STR_EQ(label, "X1"))
          sscanf(arg, "%d", &fdg_info.dots[cnt].x2);
        else if (STR_EQ(label, "Y1"))
          sscanf(arg, "%d", &fdg_info.dots[cnt].y2);
        else if (STR_EQ(label, "X"))
          sscanf(arg, "%f", &fdg_info.dots[cnt].x);
        else if (STR_EQ(label, "Y"))
          sscanf(arg, "%f", &fdg_info.dots[cnt].y);
        else if (STR_EQ(label, "XSIZE"))
          sscanf(arg, "%d", &fdg_info.dots[cnt].lx);
        else if (STR_EQ(label, "YSIZE"))
          sscanf(arg, "%d", &fdg_info.dots[cnt].ly);
        else if (STR_EQ(label, "AREA"))
          sscanf(arg, "%d", &fdg_info.dots[cnt].area);
        else if (STR_EQ(label, "END")) {
        } else if (STR_EQ(label, "DIST_CTR_TO_CTR_HOLE"))
          sscanf(arg, "%lf", &fdg_info.dist_ctr_to_ctr_hole);
        else if (STR_EQ(label, "DIST_CTR_HOLE_TO_EDGE"))
          sscanf(arg, "%lf", &fdg_info.dist_ctr_hole_to_edge);
      }

      fclose(fp);
      if (!fdg_info.ctr_type && version != 1) {
        // tmsg_error("load field guide info: bad file or old version");
        // return FALSE;
      } else {
        fdg_info.m_name          = fname.getName();
        m_infos[fdg_info.m_name] = fdg_info;
      }
    }
  } catch (...) {
  }
  // return TRUE;
}

}  // namespace

//=========================================================

CleanupParameters::CleanupParameters()
    : m_autocenterType(AUTOCENTER_NONE)
    , m_pegSide(PEGS_BOTTOM)
    , m_fdgInfo()
    , m_rotate(0)
    , m_flipx(false)
    , m_flipy(false)
    , m_offx(0)
    , m_offy(0)
    //, m_scale(1)
    , m_lineProcessingMode(lpGrey)
    , m_noAntialias(false)
    , m_postAntialias(false)
    , m_despeckling(2)
    , m_aaValue(70)
    , m_closestField(999.)
    , m_sharpness(90.)
    , m_autoAdjustMode(AUTO_ADJ_NONE)
    , m_transparencyCheckEnabled(false)
    , m_colors()
    , m_path()
    , m_cleanupPalette(createStandardCleanupPalette())
    , m_camera()
    , m_dirtyFlag(false)
    //, m_resName("")
    , m_offx_lock(false)
    , m_offy_lock(false) {}

//---------------------------------------------------------

bool CleanupParameters::setFdgByName(std::string name) {
  const FDG_INFO *info = FdgManager::instance()->getFdg(name);
  if (info) {
    m_fdgInfo = *info;
    return true;
  } else {
    m_fdgInfo = FDG_INFO();
    return false;
  }
}

//---------------------------------------------------------

void CleanupParameters::getFdgNames(std::vector<std::string> &names) {
  FdgManager::instance()->getFdgNames(names);
}

//---------------------------------------------------------

TFilePath CleanupParameters::getPath(ToonzScene *scene) const {
  if (m_path == TFilePath()) {
    int levelType =
        (m_lineProcessingMode != lpNone) ? TZP_XSHLEVEL : OVL_XSHLEVEL;
    TFilePath fp = scene->getDefaultLevelPath(levelType);
    return fp.getParentDir();
  } else
    return scene->decodeSavePath(m_path);
}

//---------------------------------------------------------

void CleanupParameters::setPath(ToonzScene *scene, TFilePath fp) {
  if (fp == scene->getDefaultLevelPath(TZP_XSHLEVEL).getParentDir())
    m_path = TFilePath();
  else
    m_path = scene->codeSavePath(fp);
}

//------------------------------------------------------------------------------

void CleanupParameters::getOutputImageInfo(TDimension &outDim, double &outDpiX,
                                           double &outDpiY) const {
  double lq_nozoom, lp_nozoom;
  double zoom_factor;

  // Retrieve camera infos
  const TDimensionD &cameraSize = m_camera.getSize();
  const TDimension &cameraRes   = m_camera.getRes();

  // Camera resolution
  lp_nozoom = cameraRes.lx;
  lq_nozoom = cameraRes.ly;

  // Zoom factor due to the 'closest field' parameter
  if (m_closestField < cameraSize.lx)
    zoom_factor = cameraSize.lx / m_closestField;
  else
    zoom_factor = 1.0;

  // Output image resolution (considering the closest field)
  // apart from any rotation (eg m_parameters.m_rotate == 90 or 270)
  outDim.lx = troundp(zoom_factor * lp_nozoom);
  outDim.ly = troundp(zoom_factor * lq_nozoom);

  // output image dpi
  outDpiX = zoom_factor * cameraRes.lx / cameraSize.lx;
  outDpiY = zoom_factor * cameraRes.ly / cameraSize.ly;
}

//------------------------------------------------------------------------------------

void CleanupParameters::assign(const CleanupParameters *param,
                               bool clonePalette) {
  // m_resName        = param->m_resName;
  m_camera         = param->m_camera;
  m_autocenterType = param->m_autocenterType;
  m_pegSide        = param->m_pegSide;
  m_fdgInfo        = param->m_fdgInfo;
  m_rotate         = param->m_rotate;
  m_flipx          = param->m_flipx;
  m_flipy          = param->m_flipy;
  // m_scale          = param->m_scale;
  m_offx                     = param->m_offx;
  m_offy                     = param->m_offy;
  m_closestField             = param->m_closestField;
  m_autoAdjustMode           = param->m_autoAdjustMode;
  m_sharpness                = param->m_sharpness;
  m_path                     = param->m_path;
  m_colors                   = param->m_colors;
  m_dirtyFlag                = param->m_dirtyFlag;
  m_transparencyCheckEnabled = param->m_transparencyCheckEnabled;
  m_lineProcessingMode       = param->m_lineProcessingMode;
  m_noAntialias              = param->m_noAntialias;
  m_postAntialias            = param->m_postAntialias;
  m_despeckling              = param->m_despeckling;
  m_aaValue                  = param->m_aaValue;

  // In modern Toonz scenes, there always is a cleanup palette.
  // In older Toonz scenes, it may be missing. In this case, leave the current
  // one.
  if (clonePalette && param->m_cleanupPalette)
    m_cleanupPalette = param->m_cleanupPalette->clone();

  m_offx_lock = param->m_offx_lock;
  m_offy_lock = param->m_offy_lock;
}

//---------------------------------------------------------

void CleanupParameters::saveData(TOStream &os) const {
  CleanupParameters::LastSavedParameters.assign(this);

  os.openChild("cleanupCamera");
  m_camera.saveData(os);
  os.closeChild();

  os.openChild("cleanupPalette");
  m_cleanupPalette->saveData(os);
  os.closeChild();

  std::map<std::string, std::string> attr;
  if (m_autocenterType != AUTOCENTER_NONE) {
    attr.clear();
    attr["type"]     = std::to_string(m_autocenterType);
    attr["pegHoles"] = std::to_string(m_pegSide);
    os.openCloseChild("autoCenter", attr);
  }

  if (m_flipx || m_flipy || m_rotate != 0  //|| m_scale!=1
      || m_offx != 0 || m_offy != 0) {
    attr.clear();
    std::string flip =
        std::string(m_flipx ? "x" : "") + std::string(m_flipy ? "y" : "");
    if (flip != "") attr["flip"]      = flip;
    if (m_rotate != 0) attr["rotate"] = std::to_string(m_rotate);
    if (m_offx != 0.0) attr["xoff"]   = std::to_string(m_offx);
    if (m_offy != 0.0) attr["yoff"]   = std::to_string(m_offy);
    os.openCloseChild("transform", attr);
  }

  if (m_lineProcessingMode != lpNone) {
    attr.clear();
    attr["sharpness"]  = std::to_string(m_sharpness);
    attr["autoAdjust"] = std::to_string(m_autoAdjustMode);
    attr["mode"]       = (m_lineProcessingMode == lpGrey ? "grey" : "color");
    os.openCloseChild("lineProcessing", attr);
  }
  if (m_noAntialias) {
    attr.clear();
    os.openCloseChild("noAntialias", attr);
  }
  if (m_postAntialias) {
    attr.clear();
    os.openCloseChild("MLAA", attr);
  }
  attr.clear();
  attr["value"] = std::to_string(m_despeckling);
  os.openCloseChild("despeckling", attr);
  attr.clear();
  attr["value"] = std::to_string(m_aaValue);
  os.openCloseChild("aaValue", attr);
  attr.clear();
  attr["value"] = std::to_string(m_closestField);
  os.openCloseChild("closestField", attr);
  attr.clear();
  attr["name"] = m_fdgInfo.m_name;
  os.openCloseChild("fdg", attr);
  attr.clear();
  if (m_path != TFilePath()) os.child("path") << m_path;
}

//---------------------------------------------------------

void CleanupParameters::loadData(TIStream &is, bool globalParams) {
  if (globalParams) {
    CleanupParameters cp;
    assign(&cp);
  }

  std::string tagName;
  m_lineProcessingMode = lpNone;
  m_noAntialias        = false;
  m_postAntialias      = false;

  while (is.matchTag(tagName)) {
    if (tagName == "cleanupPalette") {
      m_cleanupPalette->loadData(is);
      m_cleanupPalette->setIsCleanupPalette(true);
      is.closeChild();
    } else if (tagName == "cleanupCamera") {
      m_camera.loadData(is);
      is.closeChild();
    } else if (tagName == "autoCenter") {
      m_autocenterType                          = AUTOCENTER_FDG;
      std::string s                             = is.getTagAttribute("type");
      if (s != "" && isInt(s)) m_autocenterType = (AUTOCENTER_TYPE)std::stoi(s);
      s                                  = is.getTagAttribute("pegHoles");
      if (s != "" && isInt(s)) m_pegSide = (PEGS_SIDE)std::stoi(s);
    } else if (tagName == "transform") {
      std::string s                      = is.getTagAttribute("flip");
      m_flipx                            = (s.find("x") != std::string::npos);
      m_flipy                            = (s.find("y") != std::string::npos);
      s                                  = is.getTagAttribute("rotate");
      if (s != "" && isInt(s)) m_rotate  = std::stoi(s);
      s                                  = is.getTagAttribute("xoff");
      if (s != "" && isDouble(s)) m_offx = std::stod(s);
      s                                  = is.getTagAttribute("yoff");
      if (s != "" && isDouble(s)) m_offy = std::stod(s);
    } else if (tagName == "lineProcessing") {
      m_lineProcessingMode                    = lpGrey;
      std::string s                           = is.getTagAttribute("sharpness");
      if (s != "" && isDouble(s)) m_sharpness = std::stod(s);
      s = is.getTagAttribute("autoAdjust");
      if (s != "" && isDouble(s))
        m_autoAdjustMode = (CleanupTypes::AUTO_ADJ_MODE)std::stoi(s);
      s                  = is.getTagAttribute("mode");
      if (s != "" && s == "color") m_lineProcessingMode = lpColor;
    } else if (tagName == "despeckling") {
      std::string s                          = is.getTagAttribute("value");
      if (s != "" && isInt(s)) m_despeckling = std::stoi(s);
    } else if (tagName == "aaValue") {
      std::string s                      = is.getTagAttribute("value");
      if (s != "" && isInt(s)) m_aaValue = std::stoi(s);
    } else if (tagName == "noAntialias")
      m_noAntialias = true;
    else if (tagName == "MLAA")
      m_postAntialias = true;
    else if (tagName == "closestField") {
      std::string s                              = is.getTagAttribute("value");
      if (s != "" && isDouble(s)) m_closestField = std::stod(s);
    } else if (tagName == "fdg") {
      std::string s = is.getTagAttribute("name");
      if (s != "") setFdgByName(s);
    } else if (tagName == "path") {
      is >> m_path;
      is.closeChild();
    } else
      is.skipCurrentTag();
  }

  CleanupParameters::LastSavedParameters.assign(this);
  if (globalParams) CleanupParameters::GlobalParameters.assign(this);
}

//---------------------------------------------------------

const CleanupTypes::FDG_INFO &CleanupParameters::getFdgInfo() {
  if (m_fdgInfo.m_name == "") {
    std::vector<std::string> names;
    FdgManager::instance()->getFdgNames(names);
    if (names.size() > 1) {
      const CleanupTypes::FDG_INFO *info =
          FdgManager::instance()->getFdg(names[0]);
      if (info) m_fdgInfo = *info;
    }
  }
  return m_fdgInfo;
}
