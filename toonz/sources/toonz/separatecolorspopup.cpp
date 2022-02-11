#include "separatecolorspopup.h"

#include "separatecolorsswatch.h"

#include "menubarcommandids.h"
#include "tapp.h"
#include "tlevel_io.h"
#include "tiio.h"
#include "toutputproperties.h"
#include "filebrowser.h"
#include "filebrowserpopup.h"

#include "toonzqt/gutil.h"
#include "toonzqt/imageutils.h"
#include "toonzqt/menubarcommand.h"
#include "toonzqt/filefield.h"
#include "toonzqt/intfield.h"
#include "toonzqt/colorfield.h"
#include "toonzqt/checkbox.h"
#include "toonzqt/doublefield.h"
#include "toonz/tscenehandle.h"
#include "toonz/toonzscene.h"
#include "toonz/sceneproperties.h"
#include "tsystem.h"
#include "tenv.h"
#include "tpixelutils.h"

#include <QPushButton>
#include <QComboBox>
#include <QLabel>
#include <QMainWindow>
#include <QCheckBox>
#include <QSplitter>
#include <QGroupBox>
#include <QMessageBox>
#include <QThreadPool>
#include <QVector2D>
#include <QPainter>
#include <QSettings>

TEnv::StringVar SeparateColorsPopup_PaperColor("SeparateColorsPopup_PaperColor",
                                               "#FFFFFF");
TEnv::StringVar SeparateColorsPopup_MainColor("SeparateColorsPopup_MainColor",
                                              "#000000");
TEnv::StringVar SeparateColorsPopup_SubColor1("SeparateColorsPopup_SubColor1",
                                              "#FF0000");
TEnv::StringVar SeparateColorsPopup_SubColor2("SeparateColorsPopup_SubColor2",
                                              "#0000FF");
TEnv::StringVar SeparateColorsPopup_SubColor3("SeparateColorsPopup_SubColor3",
                                              "#00FF00");

TEnv::DoubleVar SeparateColorsPopup_SubAdjust("SeparateColorsPopup_SubAdjust",
                                              3.0);
TEnv::DoubleVar SeparateColorsPopup_BorderSmooth(
    "SeparateColorsPopup_BorderSmooth", 0.1);
TEnv::IntVar SeparateColorsPopup_OutMain("SeparateColorsPopup_OutMain", 1);
TEnv::IntVar SeparateColorsPopup_OutSub1("SeparateColorsPopup_OutSub1", 1);
TEnv::IntVar SeparateColorsPopup_OutSub2("SeparateColorsPopup_OutSub2", 1);
TEnv::IntVar SeparateColorsPopup_OutSub3("SeparateColorsPopup_OutSub3", 0);
TEnv::StringVar SeparateColorsPopup_MainSuffix("SeparateColorsPopup_MainSuffix",
                                               "_m");
TEnv::StringVar SeparateColorsPopup_Sub1Suffix("SeparateColorsPopup_Sub1Suffix",
                                               "_s1");
TEnv::StringVar SeparateColorsPopup_Sub2Suffix("SeparateColorsPopup_Sub2Suffix",
                                               "_s2");
TEnv::StringVar SeparateColorsPopup_Sub3Suffix("SeparateColorsPopup_Sub3Suffix",
                                               "_s3");

TEnv::IntVar SeparateColorsPopup_doMatte("SeparateColorsPopup_DoMatte", 1);
TEnv::DoubleVar SeparateColorsPopup_MatteThreshold(
    "SeparateColorsPopup_MatteThreshold ", 0.0);
TEnv::IntVar SeparateColorsPopup_MatteRadius("SeparateColorsPopup_MatteRadius",
                                             0);

TEnv::StringVar SeparateColorsPopup_FileFormat("SeparateColorsPopup_FileFormat",
                                               "png");

//=============================================================================
// SeparateColorsPopup
//-----------------------------------------------------------------------------

namespace {
//--------------------------------------------------------------------
void getFrameIds(int from, int to, const TLevelP& level,
                 std::vector<TFrameId>& frames) {
  std::string msg;
  int r0, r1;
  TLevel::Iterator begin = level->begin();
  TLevel::Iterator end   = level->end();
  end--;

  r0 = from;
  r1 = to;
  if (r0 > r1) {
    int app = r0;
    r0      = r1;
    r1      = app;
  }
  TLevel::Iterator it = begin;
  if (r0 != -1 && r0 <= level->getFrameCount()) std::advance(it, r0 - 1);
  while (it != level->end() && r1 >= r0) {
    frames.push_back(it->first);
    if (r0 != -1) ++r0;
    ++it;
  }
}

struct uchar3 {
  unsigned char x, y, z;
};
struct uchar4 {
  unsigned char x, y, z, w;
};

// returns the factors of the equation of a plane which touches the specified 3
// points
// a * x + b * y + c * z = d    ->  returns (a,b,c,d)
QVector4D getPane(QVector3D p0, QVector3D p1, QVector3D p2) {
  // vector p0->p1
  QVector3D vec_p01 = p1 - p0;
  // vector p0->p2
  QVector3D vec_p02 = p2 - p0;

  // cross product
  QVector3D cross_p01_p02 = QVector3D::crossProduct(vec_p01, vec_p02);

  QVector4D pane(cross_p01_p02.x(), cross_p01_p02.y(), cross_p01_p02.z(),
                 cross_p01_p02.x() * p0.x() + cross_p01_p02.y() * p0.y() +
                     cross_p01_p02.z() * p0.z());
  return pane;
}

QVector3D myPix2Rgb(TPixel32 pix) {
  return QVector3D((float)pix.r / (float)TPixel32::maxChannelValue,
                   (float)pix.g / (float)TPixel32::maxChannelValue,
                   (float)pix.b / (float)TPixel32::maxChannelValue);
}

QVector3D myRgb2Hls(QVector3D rgb) {
  double h, l, s;
  rgb2hls(rgb.x(), rgb.y(), rgb.z(), &h, &l, &s);
  return QVector3D((float)h, (float)l, (float)s);
}

QVector3D myHls2Xyz(QVector3D hls) {
  float hueRad = hls.x() * 3.14159265f / 180.0f;
  return QVector3D(
      hls.z() * std::cos(hueRad) * (1.0f - (std::abs(hls.y() - 0.5f) * 2.0f)),
      hls.y() - 0.5f,
      hls.z() * std::sin(hueRad) * (1.0f - (std::abs(hls.y() - 0.5f) * 2.0f)));
}

// modify values
float modify(float val) {
  if (val >= 0.5f) return 1.0f;

  return val * 2.0f;
}

// separate main color and two sub colors
QVector3D separate3_Kernel(QVector3D pix_xyz, QVector3D paper_xyz,
                           QVector3D main_xyz, QVector3D sub1_xyz,
                           QVector3D sub2_xyz, QVector4D pane, float mainAdjust,
                           float borderRatio) {
  // vector paper -> pixel
  QVector3D vec_e = pix_xyz - paper_xyz;

  float de = pane.x() * vec_e.x() + pane.y() * vec_e.y() + pane.z() * vec_e.z();

  // if de == 0, vec_e is in pallarel with the pane
  if (de == 0.0f) return QVector3D(0.0f, 0.0f, 0.0f);

  // compute the factor t which extends vec_e to the pane
  float t = (pane.w() - (pane.x() * paper_xyz.x() + pane.y() * paper_xyz.y() +
                         pane.z() * paper_xyz.z())) /
            de;

  // intersecting point on the pane
  QVector3D Q = paper_xyz + t * vec_e;

  // ratio_ms1
  // vector  main color -> Q
  QVector3D vec_mQ = Q - main_xyz;
  // vector  main color -> sub1 color
  QVector3D vec_ms1 = sub1_xyz - main_xyz;
  float ratio_ms1 =
      QVector3D::dotProduct(vec_mQ, vec_ms1) / vec_ms1.lengthSquared();

  // value_ms1
  float value_ms1;
  if (ratio_ms1 <= 1.0f / (1.0f + mainAdjust) - borderRatio)
    value_ms1 = 1.0f;
  else if (ratio_ms1 < 1.0f / (1.0f + mainAdjust) + borderRatio)
    value_ms1 = 1.0f - (ratio_ms1 - 1.0f / (1.0f + mainAdjust) + borderRatio) /
                           (2.0f * borderRatio);
  else
    value_ms1 = 0.0f;

  // ratio_ms2
  // vector  main color -> sub2 color
  QVector3D vec_ms2 = sub2_xyz - main_xyz;
  float ratio_ms2 =
      QVector3D::dotProduct(vec_mQ, vec_ms2) / vec_ms2.lengthSquared();

  // value_ms2
  float value_ms2;
  if (ratio_ms2 <= 1.0f / (1.0f + mainAdjust) - borderRatio)
    value_ms2 = 1.0f;
  else if (ratio_ms2 < 1.0f / (1.0f + mainAdjust) + borderRatio)
    value_ms2 = 1.0f - (ratio_ms2 - 1.0f / (1.0f + mainAdjust) + borderRatio) /
                           (2.0f * borderRatio);
  else
    value_ms2 = 0.0f;

  // ratio_s1s2
  // vector  sub1 color -> Q
  QVector3D vec_s1Q = Q - sub1_xyz;
  // vector  sub1 color -> sub2 color
  QVector3D vec_s1s2 = sub2_xyz - sub1_xyz;
  float ratio_s1s2 =
      QVector3D::dotProduct(vec_s1Q, vec_s1s2) / vec_s1s2.lengthSquared();

  // value_s1s2
  float value_s1s2;
  if (ratio_s1s2 <= 0.5f - borderRatio)
    value_s1s2 = 1.0f;
  else if (ratio_s1s2 < 0.5f + borderRatio)
    value_s1s2 =
        1.0f - (ratio_s1s2 - 0.5f + borderRatio) / (2.0f * borderRatio);
  else
    value_s1s2 = 0.0f;

  float base_m  = modify(value_ms1) * modify(value_ms2);
  float base_s1 = modify(1.0f - value_ms1) * modify(value_s1s2);
  float base_s2 = modify(1.0f - value_ms2) * modify(1.0f - value_s1s2);

  // vector  paper color -> main color
  QVector3D vec_pm = main_xyz - paper_xyz;
  // vector  paper color -> sub1 color
  QVector3D vec_ps1 = sub1_xyz - paper_xyz;
  // vector  paper color -> sub2 color
  QVector3D vec_ps2 = sub2_xyz - paper_xyz;

  float ratio_m = QVector3D::dotProduct(vec_e, vec_pm) / vec_pm.lengthSquared();
  float ratio_s1 =
      QVector3D::dotProduct(vec_e, vec_ps1) / vec_ps1.lengthSquared();
  float ratio_s2 =
      QVector3D::dotProduct(vec_e, vec_ps2) / vec_ps2.lengthSquared();

  return QVector3D(std::max(0.0f, base_m * ratio_m),
                   std::max(0.0f, base_s1 * ratio_s1),
                   std::max(0.0f, base_s2 * ratio_s2));
}

// compute intensity of each pencil color
// main = out.x(), sub1 = out.y(), sub2 = out.z()
void separate3Colors(int i, QVector3D* src, QVector3D* out, QVector3D paper_xyz,
                     QVector3D main_xyz, QVector3D sub1_xyz, QVector3D sub2_xyz,
                     QVector4D pane, float mainAdjust, float borderRatio) {
  // compute HLS value of the target pixel
  // then convert HLS value to XYZ coordinate
  QVector3D pix_xyz = myHls2Xyz(myRgb2Hls(src[i]));

  // pixels lighter than the paper color is 100% paper
  if (pix_xyz.y() >= paper_xyz.y()) {
    out[i].setX(0.0f);
    out[i].setY(0.0f);
    out[i].setZ(0.0f);
    return;
  }

  out[i] = separate3_Kernel(pix_xyz, paper_xyz, main_xyz, sub1_xyz, sub2_xyz,
                            pane, mainAdjust, borderRatio);
}

QVector2D getFactor(QVector3D vec, QVector3D point1, QVector3D point2) {
  float b = (point1.x() * vec.y() - point1.y() * vec.x()) /
            (point1.x() * point2.y() - point2.x() * point1.y());
  float a = (vec.x() - b * point2.x()) / point1.x();

  return QVector2D(a, b);
}

// compute intensity of each pencil color
// main = out.x(), sub1 = out.y(), sub2 = out.z(), sub3 = out.w()
void separate4Colors(int i, QVector3D* src, QVector4D* out, QVector3D paper_xyz,
                     QVector3D main_xyz, QVector3D sub1_xyz, QVector3D sub2_xyz,
                     QVector3D sub3_xyz, QVector4D pane_m12, QVector4D pane_m13,
                     QVector4D pane_m23, QVector4D pane_123,
                     QVector3D R,  // intersection of paper->main vector and
                                   // plane of 3 sub colors
                     float mainAdjust, float borderRatio) {
  // compute HLS value of the target pixel
  // then convert HLS value to XYZ coordinate
  QVector3D pix_xyz = myHls2Xyz(myRgb2Hls(src[i]));

  // pixels lighter than the paper color is 100% paper
  if (pix_xyz.y() >= paper_xyz.y()) {
    out[i].setX(0.0f);
    out[i].setY(0.0f);
    out[i].setZ(0.0f);
    out[i].setW(0.0f);
    return;
  }

  // vector paper -> pixel
  QVector3D vec_e = pix_xyz - paper_xyz;

  float de = pane_123.x() * vec_e.x() + pane_123.y() * vec_e.y() +
             pane_123.z() * vec_e.z();

  // if de == 0, vec_e is in pallarel with the pane
  if (de == 0.0f) {
    out[i].setX(0.0f);
    out[i].setY(0.0f);
    out[i].setZ(0.0f);
    out[i].setW(0.0f);
    return;
  }

  // compute the factor t which extends vec_e to the pane
  float t = (pane_123.w() -
             (pane_123.x() * paper_xyz.x() + pane_123.y() * paper_xyz.y() +
              pane_123.z() * paper_xyz.z())) /
            de;

  // intersecting point on the pane
  QVector3D Q = paper_xyz + t * vec_e;

  // vector R (pencil on the pane) -> Q (current pixel on the pane)
  QVector3D vec_RQ = Q - R;

  // vector from R to each sub color
  QVector3D vec_RS1 = sub1_xyz - R;
  QVector3D vec_RS2 = sub2_xyz - R;
  QVector3D vec_RS3 = sub3_xyz - R;

  // define which two sub colors are mixed in this pixel.
  // obtain factors
  QVector2D k_S1S2 = getFactor(vec_RQ, vec_RS1, vec_RS2);

  // in case of sub1 and sub2. sub3 is 0
  if (k_S1S2.x() >= 0.0f && k_S1S2.y() >= 0.0f) {
    QVector3D ret =
        separate3_Kernel(pix_xyz, paper_xyz, main_xyz, sub1_xyz, sub2_xyz,
                         pane_m12, mainAdjust, borderRatio);

    out[i].setX(ret.x());
    out[i].setY(ret.y());
    out[i].setZ(ret.z());
    out[i].setW(0.0f);
  } else {
    QVector2D k_S1S3 = getFactor(vec_RQ, vec_RS1, vec_RS3);
    // in case of sub1 and sub3. sub2 is 0
    if (k_S1S3.x() >= 0.0f && k_S1S3.y() >= 0.0f) {
      QVector3D ret =
          separate3_Kernel(pix_xyz, paper_xyz, main_xyz, sub1_xyz, sub3_xyz,
                           pane_m13, mainAdjust, borderRatio);

      out[i].setX(ret.x());
      out[i].setY(ret.y());
      out[i].setZ(0.0f);
      out[i].setW(ret.z());
    }
    // in case of sub2 and sub3. sub1 is 0
    else {
      QVector3D ret =
          separate3_Kernel(pix_xyz, paper_xyz, main_xyz, sub2_xyz, sub3_xyz,
                           pane_m23, mainAdjust, borderRatio);

      out[i].setX(ret.x());
      out[i].setY(0.0f);
      out[i].setZ(ret.y());
      out[i].setW(ret.z());
    }
  }
}

void maskByThres(int i, QVector3D* out, uchar3* mask, float maskThres) {
  if (out[i].x() < maskThres)
    mask[i].x = 0;
  else
    mask[i].x = 1;

  if (out[i].y() < maskThres)
    mask[i].y = 0;
  else
    mask[i].y = 1;

  if (out[i].z() < maskThres)
    mask[i].z = 0;
  else
    mask[i].z = 1;
}

void maskByThres(int i, QVector4D* out, uchar4* mask, float maskThres) {
  if (out[i].x() < maskThres)
    mask[i].x = 0;
  else
    mask[i].x = 1;

  if (out[i].y() < maskThres)
    mask[i].y = 0;
  else
    mask[i].y = 1;

  if (out[i].z() < maskThres)
    mask[i].z = 0;
  else
    mask[i].z = 1;

  if (out[i].w() < maskThres)
    mask[i].w = 0;
  else
    mask[i].w = 1;
}

void expandMask(int x, int y, uchar3* mask_host, int lx, int ly, int gen) {
  int i = y * lx + x;
  // check if the target pixel is already opac
  bool foundMain = (mask_host[i].x >= 1 && mask_host[i].x <= gen + 1);
  bool foundSub1 = (mask_host[i].y >= 1 && mask_host[i].y <= gen + 1);
  bool foundSub2 = (mask_host[i].z >= 1 && mask_host[i].z <= gen + 1);

  if (foundMain && foundSub1 && foundSub2) return;

  // search neighbor pixels' mask_host values
  for (int yy = y - 1; yy <= y + 1; yy++) {
    // boundary condition
    if (yy < 0 || yy >= ly) continue;

    // current searching index
    int curIndex = yy * lx + x - 1;

    for (int xx = x - 1; xx <= x + 1; xx++, curIndex++) {
      // boundary condition
      if (xx < 0 || xx >= lx) continue;

      // update the flags if the searching pixel is opac
      if (!foundMain && mask_host[curIndex].x >= 1 &&
          mask_host[curIndex].x <= gen + 1)
        foundMain = true;
      if (!foundSub1 && mask_host[curIndex].y >= 1 &&
          mask_host[curIndex].y <= gen + 1)
        foundSub1 = true;
      if (!foundSub2 && mask_host[curIndex].z >= 1 &&
          mask_host[curIndex].z <= gen + 1)
        foundSub2 = true;

      // end the loop when all colors are found
      if (foundMain && foundSub1 && foundSub2) break;
    }
    // end the loop when all colors are found
    if (foundMain && foundSub1 && foundSub2) break;
  }

  // update mask_host
  if (foundMain && mask_host[i].x == 0) mask_host[i].x = gen + 2;
  if (foundSub1 && mask_host[i].y == 0) mask_host[i].y = gen + 2;
  if (foundSub2 && mask_host[i].z == 0) mask_host[i].z = gen + 2;
}

void expandMask(int x, int y, uchar4* mask_host, int lx, int ly, int gen) {
  int i = y * lx + x;
  // check if the target pixel is already opac
  bool foundMain = (mask_host[i].x >= 1 && mask_host[i].x <= gen + 1);
  bool foundSub1 = (mask_host[i].y >= 1 && mask_host[i].y <= gen + 1);
  bool foundSub2 = (mask_host[i].z >= 1 && mask_host[i].z <= gen + 1);
  bool foundSub3 = (mask_host[i].w >= 1 && mask_host[i].w <= gen + 1);

  if (foundMain && foundSub1 && foundSub2 && foundSub3) return;

  // search neighbor pixels' mask_host values
  for (int yy = y - 1; yy <= y + 1; yy++) {
    // boundary condition
    if (yy < 0 || yy >= ly) continue;

    // current searching index
    int curIndex = yy * lx + x - 1;

    for (int xx = x - 1; xx <= x + 1; xx++, curIndex++) {
      // boundary condition
      if (xx < 0 || xx >= lx) continue;

      // update the flags if the searching pixel is opac
      if (!foundMain && mask_host[curIndex].x >= 1 &&
          mask_host[curIndex].x <= gen + 1)
        foundMain = true;
      if (!foundSub1 && mask_host[curIndex].y >= 1 &&
          mask_host[curIndex].y <= gen + 1)
        foundSub1 = true;
      if (!foundSub2 && mask_host[curIndex].z >= 1 &&
          mask_host[curIndex].z <= gen + 1)
        foundSub2 = true;
      if (!foundSub3 && mask_host[curIndex].w >= 1 &&
          mask_host[curIndex].w <= gen + 1)
        foundSub3 = true;

      // end the loop when all colors are found
      if (foundMain && foundSub1 && foundSub2 && foundSub3) break;
    }
    // end the loop when all colors are found
    if (foundMain && foundSub1 && foundSub2 && foundSub3) break;
  }

  // update mask_host
  if (foundMain && mask_host[i].x == 0) mask_host[i].x = gen + 2;
  if (foundSub1 && mask_host[i].y == 0) mask_host[i].y = gen + 2;
  if (foundSub2 && mask_host[i].z == 0) mask_host[i].z = gen + 2;
  if (foundSub3 && mask_host[i].w == 0) mask_host[i].w = gen + 2;
}

TPixel32 BlueMaskColor(128, 128, 255);
TPixel32 RedMaskColor(255, 128, 128);
};  // namespace

//-------------------------------------------------------------------

void Separate3ColorsTask::run() {
  for (int i = m_from; i < m_to; i++) {
    separate3Colors(i, m_src, m_out, m_paper_xyz, m_main_xyz, m_sub1_xyz,
                    m_sub2_xyz, m_pane, m_mainAdjust, m_borderRatio);
  }
}

//-------------------------------------------------------------------

void Separate4ColorsTask::run() {
  for (int i = m_from; i < m_to; i++) {
    separate4Colors(i, m_src, m_out, m_paper_xyz, m_main_xyz, m_sub1_xyz,
                    m_sub2_xyz, m_sub3_xyz, m_pane_m12, m_pane_m13, m_pane_m23,
                    m_pane_123, m_R, m_mainAdjust, m_borderRatio);
  }
}

//-------------------------------------------------------------------

SeparateColorsPopup::SeparateColorsPopup()
    : DVGui::Dialog(TApp::instance()->getMainWindow(), false, false,
                    "SeparateColors")
    , m_isConverting(false) {
  setModal(false);
  setMinimumSize(900, 670);

  QSplitter* mainSplitter = new QSplitter(this);

  m_autoBtn         = new QPushButton(tr("Auto"), this);
  m_previewBtn      = new QPushButton(tr("Preview"), this);
  m_okBtn           = new QPushButton(tr("Separate"), this);
  m_cancelBtn       = new QPushButton(tr("Close"), this);
  m_fromFld         = new IntLineEdit(this);
  m_toFld           = new IntLineEdit(this);
  m_saveInFileFld   = new FileField(0, QString(""), false, false, false);
  m_fileFormatCombo = new QComboBox(this);

  m_paperColorField =
      new DVGui::ColorField(this, false, TPixel32(249, 246, 243), 35, true, 55);
  m_mainColorField =
      new DVGui::ColorField(this, false, TPixel32(176, 171, 169), 35, true, 55);
  m_subColor1Field =
      new DVGui::ColorField(this, false, TPixel32(249, 164, 164), 35, true, 55);
  m_subColor2Field =
      new DVGui::ColorField(this, false, TPixel32(165, 227, 187), 35, true, 55);

  QLabel* color3Label = new QLabel(tr("Sub Color 3:"), this);
  m_subColor3Field =
      new DVGui::ColorField(this, false, TPixel32::Green, 35, true, 55);

  m_subColorAdjustFld   = new DoubleField(this);
  m_borderSmoothnessFld = new DoubleField(this);

  m_matteGroupBox  = new QGroupBox(tr("Alpha Matting"), this);
  m_matteThreshold = new DoubleField(this);
  m_matteRadius    = new IntField(this);

  m_notifier = new ImageUtils::FrameTaskNotifier();

  m_outMainCB = new QCheckBox(tr("Main"), this);
  m_outSub1CB = new QCheckBox(tr("Sub1"), this);
  m_outSub2CB = new QCheckBox(tr("Sub2"), this);
  m_outSub3CB = new QCheckBox(tr("Sub3"), this);

  m_mainSuffixEdit = new LineEdit(QString("_m"));
  m_sub1SuffixEdit = new LineEdit(QString("_s1"));
  m_sub2SuffixEdit = new LineEdit(QString("_s2"));
  m_sub3SuffixEdit = new LineEdit(QString("_s3"));

  m_previewFrameField = new IntField(this);
  m_previewFrameLabel = new QLabel(this);

  m_pickBtn = new QPushButton(
      CommandManager::instance()->getAction("T_RGBPicker")->icon(),
      tr("Pick Color"), this);

  QPixmap iconPm(13, 13);
  iconPm.fill(
      QColor((int)RedMaskColor.r, (int)RedMaskColor.g, (int)RedMaskColor.b));
  m_showMatteBtn = new QPushButton(QIcon(iconPm), tr("Show Mask"), this);

  {
    iconPm.fill(Qt::black);
    QPainter icon_p(&iconPm);
    icon_p.setPen(Qt::NoPen);
    icon_p.setBrush(Qt::white);
    icon_p.drawEllipse(3, 3, 7, 7);
    m_showAlphaBtn = new QPushButton(QIcon(iconPm), tr("Show Alpha"), this);
  }

  m_separateSwatch = new SeparateSwatch(this, 200, 150);

  QPushButton* saveSettingsBtn = new QPushButton(tr("Save Settings"), this);
  QPushButton* loadSettingsBtn = new QPushButton(tr("Load Settings"), this);

  //----

  m_autoBtn->setStyleSheet("font-size: 17px;");
  m_autoBtn->setFixedHeight(60);
  m_autoBtn->setCheckable(true);
  m_autoBtn->setChecked(true);

  m_previewBtn->setStyleSheet("font-size: 17px;");
  m_previewBtn->setFixedHeight(60);
  m_previewBtn->setDisabled(true);

  m_okBtn->setStyleSheet("font-size: 17px;");
  m_okBtn->setFixedHeight(60);

  m_fileFormatCombo->addItem("png");
  m_fileFormatCombo->addItem("tif");

  m_subColorAdjustFld->setRange(0.5, 10.0);
  m_borderSmoothnessFld->setRange(0.0, 0.5);
  m_subColorAdjustFld->setValue(3.0);
  m_borderSmoothnessFld->setValue(0.1);

  m_matteGroupBox->setCheckable(true);
  m_matteGroupBox->setChecked(true);
  m_matteThreshold->setRange(0.0, 1.0);
  m_matteRadius->setRange(0, 30);
  m_matteThreshold->setValue(0.0);
  m_matteRadius->setValue(0);

  m_outMainCB->setChecked(true);
  m_outSub1CB->setChecked(true);
  m_outSub2CB->setChecked(true);

  m_outSub3CB->setChecked(false);
  m_sub3SuffixEdit->setEnabled(false);
  color3Label->setVisible(false);
  m_subColor3Field->setVisible(false);

  m_pickBtn->setMaximumWidth(130);

  m_showMatteBtn->setCheckable(true);
  m_showMatteBtn->setChecked(true);

  m_showAlphaBtn->setCheckable(true);
  m_showAlphaBtn->setChecked(false);

  //----
  {
    QVBoxLayout* leftLay = new QVBoxLayout();
    leftLay->setMargin(5);
    leftLay->setSpacing(5);
    {
      QHBoxLayout* previewLay = new QHBoxLayout();
      previewLay->setMargin(0);
      previewLay->setSpacing(5);
      {
        previewLay->addWidget(new QLabel(tr("Preview Frame:"), this), 0,
                              Qt::AlignRight);
        previewLay->addWidget(m_previewFrameField, 1);
        previewLay->addWidget(m_previewFrameLabel, 0);

        previewLay->addSpacing(10);

        previewLay->addWidget(m_showMatteBtn, 0);
        previewLay->addWidget(m_showAlphaBtn, 0);

        previewLay->addSpacing(20);

        previewLay->addWidget(m_pickBtn);
      }
      leftLay->addLayout(previewLay, 0);

      leftLay->addWidget(m_separateSwatch, 1);
    }
    QWidget* leftWidget = new QWidget(this);
    leftWidget->setLayout(leftLay);
    mainSplitter->addWidget(leftWidget);

    QVBoxLayout* rightLay = new QVBoxLayout();
    rightLay->setMargin(10);
    rightLay->setSpacing(10);
    {
      QGridLayout* upperLay = new QGridLayout();
      upperLay->setMargin(0);
      upperLay->setHorizontalSpacing(5);
      upperLay->setVerticalSpacing(10);
      {
        upperLay->addWidget(new QLabel(tr("Paper Color:"), this), 0, 0,
                            Qt::AlignRight);
        upperLay->addWidget(m_paperColorField, 0, 1);
        upperLay->addWidget(new QLabel(tr("Main Color:"), this), 1, 0,
                            Qt::AlignRight);
        upperLay->addWidget(m_mainColorField, 1, 1);
        upperLay->addWidget(new QLabel(tr("Sub Color 1:"), this), 2, 0,
                            Qt::AlignRight);
        upperLay->addWidget(m_subColor1Field, 2, 1);
        upperLay->addWidget(new QLabel(tr("Sub Color 2:"), this), 3, 0,
                            Qt::AlignRight);
        upperLay->addWidget(m_subColor2Field, 3, 1);

        upperLay->addWidget(color3Label, 4, 0, Qt::AlignRight);
        upperLay->addWidget(m_subColor3Field, 4, 1);

        upperLay->addWidget(new QLabel(tr("Sub Adjust:"), this), 5, 0,
                            Qt::AlignRight);
        upperLay->addWidget(m_subColorAdjustFld, 5, 1);
        upperLay->addWidget(new QLabel(tr("Border Smooth:"), this), 6, 0,
                            Qt::AlignRight);
        upperLay->addWidget(m_borderSmoothnessFld, 6, 1);

        QGridLayout* maskLay = new QGridLayout();
        maskLay->setMargin(10);
        maskLay->setHorizontalSpacing(5);
        maskLay->setVerticalSpacing(10);
        {
          maskLay->addWidget(new QLabel(tr("Mask Threshold:"), this), 0, 0,
                             Qt::AlignRight);
          maskLay->addWidget(m_matteThreshold, 0, 1);
          maskLay->addWidget(new QLabel(tr("Mask Radius:"), this), 1, 0,
                             Qt::AlignRight);
          maskLay->addWidget(m_matteRadius, 1, 1);
        }
        maskLay->setColumnStretch(0, 0);
        maskLay->setColumnStretch(1, 1);
        m_matteGroupBox->setLayout(maskLay);
        upperLay->addWidget(m_matteGroupBox, 7, 0, 1, 2);
      }
      upperLay->setColumnStretch(0, 0);
      upperLay->setColumnStretch(1, 1);

      rightLay->addLayout(upperLay, 0);

      QGridLayout* middleLay = new QGridLayout();
      middleLay->setMargin(0);
      middleLay->setHorizontalSpacing(5);
      middleLay->setVerticalSpacing(10);
      {
        middleLay->addWidget(new QLabel(tr("Start:"), this), 0, 0,
                             Qt::AlignRight);
        QHBoxLayout* rangeLay = new QHBoxLayout();
        rangeLay->setMargin(0);
        rangeLay->setSpacing(5);
        {
          rangeLay->addWidget(m_fromFld, 0);
          rangeLay->addSpacing(10);
          rangeLay->addWidget(new QLabel(tr("End:"), this), 0);
          rangeLay->addWidget(m_toFld, 0);
          rangeLay->addStretch(1);
          rangeLay->addWidget(new QLabel(tr("Format:"), this), 0);
          rangeLay->addWidget(m_fileFormatCombo, 0);
        }
        middleLay->addLayout(rangeLay, 0, 1, 1, 6);

        middleLay->addWidget(new QLabel(tr("Save in:"), this), 1, 0,
                             Qt::AlignRight);
        middleLay->addWidget(m_saveInFileFld, 1, 1, 1, 6);

        middleLay->addWidget(new QLabel(tr("File Suffix:"), this), 2, 0,
                             Qt::AlignRight);
        middleLay->addWidget(m_outMainCB, 2, 1,
                             Qt::AlignRight | Qt::AlignVCenter);
        middleLay->addWidget(m_mainSuffixEdit, 2, 2);
        middleLay->addWidget(m_outSub1CB, 2, 4,
                             Qt::AlignRight | Qt::AlignVCenter);
        middleLay->addWidget(m_sub1SuffixEdit, 2, 5);

        middleLay->addWidget(m_outSub2CB, 3, 1,
                             Qt::AlignRight | Qt::AlignVCenter);
        middleLay->addWidget(m_sub2SuffixEdit, 3, 2);
        middleLay->addWidget(m_outSub3CB, 3, 4,
                             Qt::AlignRight | Qt::AlignVCenter);
        middleLay->addWidget(m_sub3SuffixEdit, 3, 5);
      }
      middleLay->setColumnStretch(0, 0);
      middleLay->setColumnStretch(1, 0);
      middleLay->setColumnStretch(2, 0);
      middleLay->setColumnStretch(3, 0);
      middleLay->setColumnStretch(4, 0);
      middleLay->setColumnStretch(5, 0);
      middleLay->setColumnStretch(6, 1);
      rightLay->addLayout(middleLay, 0);

      rightLay->addSpacing(10);

      QHBoxLayout* saveLoadLay = new QHBoxLayout();
      saveLoadLay->setMargin(0);
      saveLoadLay->setSpacing(0);
      {
        saveLoadLay->addWidget(saveSettingsBtn, 1);
        saveLoadLay->addSpacing(10);
        saveLoadLay->addWidget(loadSettingsBtn, 1);
      }
      rightLay->addLayout(saveLoadLay, 0);
      rightLay->addSpacing(10);

      QHBoxLayout* buttonLay = new QHBoxLayout();
      buttonLay->setMargin(0);
      buttonLay->setSpacing(0);
      {
        buttonLay->addWidget(m_autoBtn, 0);
        buttonLay->addWidget(m_previewBtn, 1);
        buttonLay->addSpacing(30);
        buttonLay->addWidget(m_okBtn, 1);
      }
      rightLay->addLayout(buttonLay, 0);
      rightLay->addSpacing(10);
      rightLay->addWidget(m_cancelBtn, 0);

      rightLay->addStretch(1);
    }
    QWidget* widget = new QWidget(this);
    widget->setLayout(rightLay);

    mainSplitter->addWidget(widget);
  }
  mainSplitter->setStretchFactor(0, 1);
  mainSplitter->setStretchFactor(1, 0);
  m_topLayout->addWidget(mainSplitter, 1);

  //----

  bool ret = true;

  ret = ret && connect(m_autoBtn, SIGNAL(toggled(bool)), m_previewBtn,
                       SLOT(setDisabled(bool)));
  ret = ret && connect(m_previewBtn, SIGNAL(clicked()), this,
                       SLOT(onPreviewBtnPressed()));
  ret = ret && connect(m_okBtn, SIGNAL(clicked()), this, SLOT(separate()));
  ret = ret && connect(m_cancelBtn, SIGNAL(clicked()), this, SLOT(reject()));

  m_progressDialog = new DVGui::ProgressDialog("", tr("Cancel"), 0, 0);
  m_progressDialog->setWindowTitle(tr("Separate by colors ... "));
  m_progressDialog->setWindowFlags(
      Qt::Dialog | Qt::WindowTitleHint);  // Don't show ? and X buttons
  // m_progressDialog->setWindowModality(Qt::WindowModal);
  m_progressDialog->setMinimum(0);
  m_progressDialog->setMaximum(100);
  ret = ret && connect(m_notifier, SIGNAL(frameCompleted(int)),
                       m_progressDialog, SLOT(setValue(int)));
  ret = ret && connect(m_progressDialog, SIGNAL(canceled()), m_notifier,
                       SLOT(onCancelTask()));

  ret = ret && connect(m_outMainCB, SIGNAL(toggled(bool)), m_mainSuffixEdit,
                       SLOT(setEnabled(bool)));
  ret = ret && connect(m_outSub1CB, SIGNAL(toggled(bool)), m_sub1SuffixEdit,
                       SLOT(setEnabled(bool)));
  ret = ret && connect(m_outSub2CB, SIGNAL(toggled(bool)), m_sub2SuffixEdit,
                       SLOT(setEnabled(bool)));

  ret = ret && connect(m_outSub3CB, SIGNAL(toggled(bool)), color3Label,
                       SLOT(setVisible(bool)));
  ret = ret && connect(m_outSub3CB, SIGNAL(toggled(bool)), m_subColor3Field,
                       SLOT(setVisible(bool)));
  ret = ret && connect(m_outSub3CB, SIGNAL(toggled(bool)), m_sub3SuffixEdit,
                       SLOT(setEnabled(bool)));
  ret = ret && connect(m_outSub3CB, SIGNAL(toggled(bool)), m_separateSwatch,
                       SLOT(showSub3Swatch(bool)));

  ret = ret && connect(m_previewFrameField, SIGNAL(valueChanged(bool)), this,
                       SLOT(onPreviewFrameFieldValueChanged(bool)));

  QAction* pickScreenAction =
      CommandManager::instance()->getAction("A_ToolOption_PickScreen");
  ret = ret && connect(m_pickBtn, SIGNAL(clicked()), pickScreenAction,
                       SLOT(trigger()));

  ret = ret && connect(m_matteGroupBox, SIGNAL(toggled(bool)), m_showMatteBtn,
                       SLOT(setEnabled(bool)));

  // auto preview update
  ret =
      ret && connect(m_autoBtn, SIGNAL(toggled(bool)), this, SLOT(onToggle()));
  ret = ret && connect(m_subColorAdjustFld, SIGNAL(valueChanged(bool)), this,
                       SLOT(onChange(bool)));
  ret = ret && connect(m_borderSmoothnessFld, SIGNAL(valueChanged(bool)), this,
                       SLOT(onChange(bool)));
  ret = ret &&
        connect(m_matteGroupBox, SIGNAL(toggled(bool)), this, SLOT(onToggle()));
  ret = ret && connect(m_matteThreshold, SIGNAL(valueChanged(bool)), this,
                       SLOT(onChange(bool)));
  ret = ret && connect(m_matteRadius, SIGNAL(valueChanged(bool)), this,
                       SLOT(onChange(bool)));
  ret = ret &&
        connect(m_outSub3CB, SIGNAL(toggled(bool)), this, SLOT(onToggle()));
  ret = ret &&
        connect(m_paperColorField, SIGNAL(colorChanged(const TPixel32&, bool)),
                this, SLOT(onColorChange(const TPixel32&, bool)));
  ret = ret &&
        connect(m_mainColorField, SIGNAL(colorChanged(const TPixel32&, bool)),
                this, SLOT(onColorChange(const TPixel32&, bool)));
  ret = ret &&
        connect(m_subColor1Field, SIGNAL(colorChanged(const TPixel32&, bool)),
                this, SLOT(onColorChange(const TPixel32&, bool)));
  ret = ret &&
        connect(m_subColor2Field, SIGNAL(colorChanged(const TPixel32&, bool)),
                this, SLOT(onColorChange(const TPixel32&, bool)));
  ret = ret &&
        connect(m_subColor3Field, SIGNAL(colorChanged(const TPixel32&, bool)),
                this, SLOT(onColorChange(const TPixel32&, bool)));
  ret = ret &&
        connect(m_showMatteBtn, SIGNAL(toggled(bool)), this, SLOT(onToggle()));
  ret = ret &&
        connect(m_showAlphaBtn, SIGNAL(toggled(bool)), this, SLOT(onToggle()));

  ret = ret && connect(saveSettingsBtn, SIGNAL(clicked()), this,
                       SLOT(onSaveSettings()));
  ret = ret && connect(loadSettingsBtn, SIGNAL(clicked()), this,
                       SLOT(onLoadSettings()));
  ret = ret && connect(m_saveInFileFld, SIGNAL(pathChanged()), this,
                       SLOT(onSaveInPathChanged()));
  assert(ret);

  //----- reproduce UI

  QColor paperCol, mainCol, sub1Col, sub2Col, sub3Col;
  paperCol.setNamedColor(
      QString::fromStdString(SeparateColorsPopup_PaperColor.getValue()));
  mainCol.setNamedColor(
      QString::fromStdString(SeparateColorsPopup_MainColor.getValue()));
  sub1Col.setNamedColor(
      QString::fromStdString(SeparateColorsPopup_SubColor1.getValue()));
  sub2Col.setNamedColor(
      QString::fromStdString(SeparateColorsPopup_SubColor2.getValue()));
  sub3Col.setNamedColor(
      QString::fromStdString(SeparateColorsPopup_SubColor3.getValue()));

  m_paperColorField->setColor(
      TPixel32(paperCol.red(), paperCol.green(), paperCol.blue()));
  m_mainColorField->setColor(
      TPixel32(mainCol.red(), mainCol.green(), mainCol.blue()));
  m_subColor1Field->setColor(
      TPixel32(sub1Col.red(), sub1Col.green(), sub1Col.blue()));
  m_subColor2Field->setColor(
      TPixel32(sub2Col.red(), sub2Col.green(), sub2Col.blue()));
  m_subColor3Field->setColor(
      TPixel32(sub3Col.red(), sub3Col.green(), sub3Col.blue()));

  m_subColorAdjustFld->setValue(SeparateColorsPopup_SubAdjust);
  m_borderSmoothnessFld->setValue(SeparateColorsPopup_BorderSmooth);

  m_outMainCB->setChecked(SeparateColorsPopup_OutMain ? 1 : 0);
  m_outSub1CB->setChecked(SeparateColorsPopup_OutSub1 ? 1 : 0);
  m_outSub2CB->setChecked(SeparateColorsPopup_OutSub2 ? 1 : 0);
  m_outSub3CB->setChecked(SeparateColorsPopup_OutSub3 ? 1 : 0);

  m_mainSuffixEdit->setText(
      QString::fromStdString(SeparateColorsPopup_MainSuffix.getValue()));
  m_sub1SuffixEdit->setText(
      QString::fromStdString(SeparateColorsPopup_Sub1Suffix.getValue()));
  m_sub2SuffixEdit->setText(
      QString::fromStdString(SeparateColorsPopup_Sub2Suffix.getValue()));
  m_sub3SuffixEdit->setText(
      QString::fromStdString(SeparateColorsPopup_Sub3Suffix.getValue()));

  m_matteGroupBox->setChecked(SeparateColorsPopup_doMatte == 1 ? true : false);
  m_matteThreshold->setValue(SeparateColorsPopup_MatteThreshold);
  m_matteRadius->setValue(SeparateColorsPopup_MatteRadius);

  m_fileFormatCombo->setCurrentText(
      QString::fromStdString(SeparateColorsPopup_FileFormat));
}

//-------------------------------------------------------------------

void SeparateColorsPopup::onSeparateFinished() {
  m_isConverting = false;
  TFilePath dstFilePath(m_saveInFileFld->getPath().toStdWString());
  if (dstFilePath == m_srcFilePaths[0].getParentDir())
    FileBrowser::refreshFolder(dstFilePath);

  m_progressDialog->close();
}

//-------------------------------------------------------------------

SeparateColorsPopup::~SeparateColorsPopup() {
  delete m_notifier;
  delete m_progressDialog;
}

//-------------------------------------------------------------------

bool SeparateColorsPopup::isConverting() { return m_isConverting; }

//-------------------------------------------------------------------

void SeparateColorsPopup::setFiles(const std::vector<TFilePath>& fps) {
  if (!m_srcFrames.isEmpty()) m_srcFrames.clear();

  m_srcFilePaths = fps;
  if (m_srcFilePaths.size() == 1) {
    setWindowTitle(tr("Separate 1 Level"));
    m_fromFld->setEnabled(true);
    m_toFld->setEnabled(true);

    TLevelP levelTmp;
    TLevelReaderP lrTmp = TLevelReaderP(fps[0]);
    if (lrTmp) levelTmp = lrTmp->loadInfo();
    m_fromFld->setText("1");
    m_toFld->setText(QString::number(levelTmp->getFrameCount()));

    std::vector<TFrameId> frames;
    getFrameIds(1, levelTmp->getFrameCount(), levelTmp, frames);
    for (int f = 0; f < frames.size(); f++)
      m_srcFrames.append(qMakePair(fps[0], frames[f]));
  } else {
    setWindowTitle(tr("Separate %1 Levels").arg(m_srcFilePaths.size()));
    m_fromFld->setText("");
    m_toFld->setText("");
    m_fromFld->setEnabled(false);
    m_toFld->setEnabled(false);
    for (int s = 0; s < fps.size(); s++) {
      TLevelReaderP lrTmp = TLevelReaderP(fps[s]);
      if (lrTmp) {
        TLevelP levelTmp = lrTmp->loadInfo();
        std::vector<TFrameId> frames;
        getFrameIds(1, levelTmp->getFrameCount(), levelTmp, frames);
        for (int f = 0; f < frames.size(); f++)
          m_srcFrames.append(qMakePair(fps[s], frames[f]));
      }
    }
  }
  m_saveInFileFld->setPath(toQString(fps[0].getParentDir()));
  m_lastAcceptedSaveInPath = m_saveInFileFld->getPath();

  // update preview frames
  m_previewFrameField->setRange(1, m_srcFrames.size());
  m_previewFrameField->setValue(1);

  // load parameter
  TFilePath settingsFp = fps[0].withNoFrame().withType("sep");
  if (TSystem::doesExistFileOrLevel(settingsFp))
    doLoadSettings(settingsFp, true);

  onPreviewFrameFieldValueChanged(true);
}

//-------------------------------------------------------------------

void SeparateColorsPopup::doSeparate(const TFilePath& source, int from, int to,
                                     int framerate,
                                     FrameTaskNotifier* frameNotifier,
                                     bool do4Colors) {
  QString msg;

  TLevelReaderP lr(source);
  TLevelP level = lr->loadInfo();

  TDimension res(0, 0);
  std::string srcExt = source.getType();

  std::vector<TFrameId> frames;
  getFrameIds(from, to, level, frames);

  TImageInfo* ii       = (TImageInfo*)lr->getImageInfo(frames[0]);
  TPropertyGroup* prop = ii->m_properties;

  // destination folder
  TFilePath dstFolder(m_saveInFileFld->getPath().toStdWString());
  dstFolder = TApp::instance()->getCurrentScene()->getScene()->decodeFilePath(
      dstFolder);

  // file type
  std::string ext = m_fileFormatCombo->currentText().toStdString();

  TFilePath dest_m =
      source.withParentDir(dstFolder)
          .withName(source.getName() + m_mainSuffixEdit->text().toStdString())
          .withType(ext);
  TFilePath dest_c1 =
      source.withParentDir(dstFolder)
          .withName(source.getName() + m_sub1SuffixEdit->text().toStdString())
          .withType(ext);
  TFilePath dest_c2 =
      source.withParentDir(dstFolder)
          .withName(source.getName() + m_sub2SuffixEdit->text().toStdString())
          .withType(ext);
  TFilePath dest_c3 =
      source.withParentDir(dstFolder)
          .withName(source.getName() + m_sub3SuffixEdit->text().toStdString())
          .withType(ext);

  if (!TSystem::touchParentDir(dest_m)) {
    QMessageBox::critical(this, tr("Critical"),
                          tr("Failed to access the destination folder!"));
    return;
  }

  TPropertyGroup* propForWrite = (m_matteGroupBox->isChecked()) ? 0 : prop;

  TLevelWriterP lw_m(dest_m, propForWrite);
  TLevelWriterP lw_c1(dest_c1, propForWrite);
  TLevelWriterP lw_c2(dest_c2, propForWrite);
  TLevelWriterP lw_c3(dest_c3, propForWrite);

  lw_m->setFrameRate(framerate);
  lw_c1->setFrameRate(framerate);
  lw_c2->setFrameRate(framerate);
  lw_c3->setFrameRate(framerate);

  bool outMain = m_outMainCB->isChecked();
  bool outSub1 = m_outSub1CB->isChecked();
  bool outSub2 = m_outSub2CB->isChecked();
  bool outSub3 = do4Colors;

  // for each frame
  for (int i = 0; i < (int)frames.size(); i++) {
    if (frameNotifier->abortTask()) break;

    TImageReaderP ir  = lr->getFrameReader(frames[i]);
    TRasterImageP img = ir->load();
    double xDpi, yDpi;
    img->getDpi(xDpi, yDpi);

    TDimensionI dim = convert(img->getBBox()).getSize();

    TRaster32P raster(dim);
    raster = img->getRaster();
    raster->lock();

    TRaster32P ras_m(dim);
    ras_m->lock();
    TRaster32P ras_c1(dim);
    ras_c1->lock();
    TRaster32P ras_c2(dim);
    ras_c2->lock();

    if (!do4Colors) {
      doCompute(raster, dim, ras_m, ras_c1, ras_c2);
    } else {
      TRaster32P ras_c3(dim);
      ras_c3->lock();
      doCompute(raster, dim, ras_m, ras_c1, ras_c2, ras_c3);
      TRasterImageP newImg_c3(ras_c3);
      newImg_c3->setDpi(xDpi, yDpi);
      TImageWriterP iw_c3 = lw_c3->getFrameWriter(frames[i]);
      iw_c3->save(newImg_c3);
      ras_c3->unlock();
    }

    TRasterImageP newImg_m(ras_m);
    TRasterImageP newImg_c1(ras_c1);
    TRasterImageP newImg_c2(ras_c2);

    newImg_m->setDpi(xDpi, yDpi);
    newImg_c1->setDpi(xDpi, yDpi);
    newImg_c2->setDpi(xDpi, yDpi);

    if (outMain) {
      TImageWriterP iw_m = lw_m->getFrameWriter(frames[i]);
      iw_m->save(newImg_m);
    }
    if (outSub1) {
      TImageWriterP iw_c1 = lw_c1->getFrameWriter(frames[i]);
      iw_c1->save(newImg_c1);
    }
    if (outSub2) {
      TImageWriterP iw_c2 = lw_c2->getFrameWriter(frames[i]);
      iw_c2->save(newImg_c2);
    }

    raster->unlock();
    ras_m->unlock();
    ras_c1->unlock();
    ras_c2->unlock();

    // this notification is for updating the progress bar.
    // thus, not frameId but the amount of finished frames is thrown.
    frameNotifier->notifyFrameCompleted(100 * (i + 1) / frames.size());
  }
}

//-------------------------------------------------------------------

void SeparateColorsPopup::doPreview(TRaster32P& orgRas, TRaster32P& mainRas,
                                    TRaster32P& sub1Ras, TRaster32P& sub2Ras,
                                    TRaster32P& sub3Ras) {
  if (m_srcFrames.isEmpty()) return;

  TPixel paperColor = m_paperColorField->getColor();
  TPixel mainColor  = m_mainColorField->getColor();
  TPixel subColor1  = m_subColor1Field->getColor();
  TPixel subColor2  = m_subColor2Field->getColor();
  TPixel subColor3  = m_subColor3Field->getColor();

  bool do4Colors = m_outSub3CB->isChecked();

  // specify the transparent colors
  bool showAlpha = m_showAlphaBtn->isChecked();
  m_separateSwatch->setTranspColors(mainColor, subColor1, subColor2, subColor3,
                                    showAlpha);

  int frame    = m_previewFrameField->getValue() - 1;
  TFilePath fp = m_srcFrames[frame].first;
  TFrameId fId = m_srcFrames[frame].second;

  TLevelReaderP lr(fp);
  TImageReaderP ir = lr->getFrameReader(fId);

  TRasterImageP img = ir->load();
  TDimensionI dim   = convert(img->getBBox()).getSize();

  orgRas  = img->getRaster();
  mainRas = TRaster32P(dim);
  sub1Ras = TRaster32P(dim);
  sub2Ras = TRaster32P(dim);

  if (!do4Colors) {
    doCompute(orgRas, dim, mainRas, sub1Ras, sub2Ras, true);
  } else {
    sub3Ras = TRaster32P(dim);
    doCompute(orgRas, dim, mainRas, sub1Ras, sub2Ras, sub3Ras, true);
  }
}

//-------------------------------------------------------------------

void SeparateColorsPopup::doCompute(TRaster32P raster, TDimensionI& dim,
                                    TRaster32P ras_m, TRaster32P ras_c1,
                                    TRaster32P ras_c2, bool isPreview) {
  TPixel paperColor = m_paperColorField->getColor();
  TPixel mainColor  = m_mainColorField->getColor();
  TPixel subColor1  = m_subColor1Field->getColor();
  TPixel subColor2  = m_subColor2Field->getColor();

  // alpha matting
  float matteThres = (float)(m_matteThreshold->getValue());
  int matteRadius  = m_matteRadius->getValue();
  bool doMatte     = m_matteGroupBox->isChecked() && matteThres > 0.0f;

  // intensity of each color is to be stored
  QVector3D* out_host;
  TRasterGR8P out_host_ras(sizeof(QVector3D) * dim.lx * dim.ly, 1);
  out_host_ras->lock();
  out_host = (QVector3D*)out_host_ras->getRawData();

  QVector3D* src_host;
  TRasterGR8P src_host_ras(sizeof(QVector3D) * dim.lx * dim.ly, 1);
  src_host_ras->lock();
  src_host = (QVector3D*)src_host_ras->getRawData();

  // matte information
  uchar3* matte_host = nullptr;
  TRasterGR8P matte_host_ras;
  if (doMatte) {
    matte_host_ras = TRasterGR8P(sizeof(uchar3) * dim.lx * dim.ly, 1);
    matte_host_ras->lock();
    matte_host = (uchar3*)matte_host_ras->getRawData();
  }

  // input the source image
  QVector3D* chann_p = src_host;
  for (int j = 0; j < dim.ly; j++) {
    TPixel32* pix = raster->pixels(j);
    for (int i = 0; i < dim.lx; i++, chann_p++, pix++) {
      (*chann_p).setX((float)pix->r / (float)TPixel32::maxChannelValue);
      (*chann_p).setY((float)pix->g / (float)TPixel32::maxChannelValue);
      (*chann_p).setZ((float)pix->b / (float)TPixel32::maxChannelValue);
    }
  }

  QVector3D paper_xyz = myHls2Xyz(myRgb2Hls(myPix2Rgb(paperColor)));
  QVector3D main_xyz  = myHls2Xyz(myRgb2Hls(myPix2Rgb(mainColor)));
  QVector3D sub1_xyz  = myHls2Xyz(myRgb2Hls(myPix2Rgb(subColor1)));
  QVector3D sub2_xyz  = myHls2Xyz(myRgb2Hls(myPix2Rgb(subColor2)));

  // obtain the factors of plane equation : ax + by + cz = d
  QVector4D pane = getPane(main_xyz, sub1_xyz, sub2_xyz);

  int maxThreadCount =
      std::max(1, QThreadPool::globalInstance()->maxThreadCount() / 2);
  int start = 0;
  for (int t = 0; t < maxThreadCount; t++) {
    int end = (int)std::round((float)(dim.lx * dim.ly * (t + 1)) /
                              (float)maxThreadCount);

    Separate3ColorsTask* task = new Separate3ColorsTask(
        start, end, src_host, out_host, paper_xyz, main_xyz, sub1_xyz, sub2_xyz,
        pane, (float)m_subColorAdjustFld->getValue(),
        (float)m_borderSmoothnessFld->getValue());

    QThreadPool::globalInstance()->start(task);

    start = end;
  }

  QThreadPool::globalInstance()->waitForDone();

  src_host_ras->unlock();

  if (doMatte) {
    // mask by threshold, then expand it
    for (int i = 0; i < dim.lx * dim.ly; i++) {
      maskByThres(i, out_host, matte_host, matteThres);
    }

    for (int r = 0; r < matteRadius; r++) {
      for (int y = 0; y < dim.ly; y++) {
        for (int x = 0; x < dim.lx; x++) {
          expandMask(x, y, matte_host, dim.lx, dim.ly, r);
        }
      }
    }
  }

  auto getChannelValue = [](unsigned char pencil_chanVal,
                            float ratio) -> unsigned char {
    return (unsigned char)(std::round((float)pencil_chanVal * ratio));
  };

  bool showMask      = m_showMatteBtn->isChecked();
  auto getMatteColor = [&](TPixel32 lineColor) -> TPixel32 {
    if (!isPreview || !showMask) return TPixel32::Transparent;
    TPixelD lineColorD = toPixelD(lineColor);
    double h, l, s;
    rgb2hls(lineColorD.r, lineColorD.g, lineColorD.b, &h, &l, &s);
    // if the specified color is reddish, use blue color for transparent area
    if ((h <= 30.0 || 330.0 <= h) && s >= 0.2)
      return BlueMaskColor;
    else
      return RedMaskColor;
  };

  TPixel32 matteColor_m  = getMatteColor(mainColor);
  TPixel32 matteColor_s1 = getMatteColor(subColor1);
  TPixel32 matteColor_s2 = getMatteColor(subColor2);

  // show alpha channel
  if (isPreview && m_showAlphaBtn->isChecked()) {
    mainColor = TPixel::White;
    subColor1 = TPixel::White;
    subColor2 = TPixel::White;
  }

  // compute result
  QVector3D* ratio_p = out_host;
  uchar3* matte_p    = matte_host;
  for (int j = 0; j < dim.ly; j++) {
    TPixel32* pix_m  = ras_m->pixels(j);
    TPixel32* pix_c1 = ras_c1->pixels(j);
    TPixel32* pix_c2 = ras_c2->pixels(j);
    for (int i = 0; i < dim.lx; i++, ratio_p++, pix_m++, pix_c1++, pix_c2++) {
      // main
      if (!doMatte || (*matte_p).x > 0) {
        float ratio_m = (*ratio_p).x();
        if (doMatte)
          ratio_m *=
              (float)(matteRadius + 1 - (int)(*matte_p).x) / (float)matteRadius;
        ratio_m = std::min(1.0f, ratio_m);

        pix_m->r = getChannelValue(mainColor.r, ratio_m);
        pix_m->g = getChannelValue(mainColor.g, ratio_m);
        pix_m->b = getChannelValue(mainColor.b, ratio_m);
        pix_m->m = getChannelValue(TPixel32::maxChannelValue, ratio_m);
      } else {
        *pix_m = matteColor_m;
      }

      // sub1
      if (!doMatte || (*matte_p).y > 0) {
        float ratio_s1 = (*ratio_p).y();
        if (doMatte)
          ratio_s1 *=
              (float)(matteRadius + 1 - (int)(*matte_p).y) / (float)matteRadius;
        ratio_s1 = std::min(1.0f, ratio_s1);

        pix_c1->r = getChannelValue(subColor1.r, ratio_s1);
        pix_c1->g = getChannelValue(subColor1.g, ratio_s1);
        pix_c1->b = getChannelValue(subColor1.b, ratio_s1);
        pix_c1->m = getChannelValue(TPixel32::maxChannelValue, ratio_s1);
      } else {
        *pix_c1 = matteColor_s1;
      }

      // sub2
      if (!doMatte || (*matte_p).z > 0) {
        float ratio_s2 = (*ratio_p).z();
        if (doMatte)
          ratio_s2 *=
              (float)(matteRadius + 1 - (int)(*matte_p).z) / (float)matteRadius;
        ratio_s2 = std::min(1.0f, ratio_s2);

        pix_c2->r = getChannelValue(subColor2.r, ratio_s2);
        pix_c2->g = getChannelValue(subColor2.g, ratio_s2);
        pix_c2->b = getChannelValue(subColor2.b, ratio_s2);
        pix_c2->m = getChannelValue(TPixel32::maxChannelValue, ratio_s2);
      } else {
        *pix_c2 = matteColor_s2;
      }

      if (doMatte) matte_p++;
    }
  }
  out_host_ras->unlock();

  if (doMatte) matte_host_ras->unlock();
}

//-------------------------------------------------------------------
// 4colors version

void SeparateColorsPopup::doCompute(TRaster32P raster, TDimensionI& dim,
                                    TRaster32P ras_m, TRaster32P ras_c1,
                                    TRaster32P ras_c2, TRaster32P ras_c3,
                                    bool isPreview) {
  TPixel paperColor = m_paperColorField->getColor();
  TPixel mainColor  = m_mainColorField->getColor();
  TPixel subColor1  = m_subColor1Field->getColor();
  TPixel subColor2  = m_subColor2Field->getColor();
  TPixel subColor3  = m_subColor3Field->getColor();

  // alpha matting
  float matteThres = (float)(m_matteThreshold->getValue());
  int matteRadius  = m_matteRadius->getValue();
  bool doMatte     = m_matteGroupBox->isChecked() && matteThres > 0.0f;

  // intensity of each color is to be stored
  QVector4D* out_host;
  TRasterGR8P out_host_ras(sizeof(QVector4D) * dim.lx * dim.ly, 1);
  out_host_ras->lock();
  out_host = (QVector4D*)out_host_ras->getRawData();

  QVector3D* src_host;
  TRasterGR8P src_host_ras(sizeof(QVector3D) * dim.lx * dim.ly, 1);
  src_host_ras->lock();
  src_host = (QVector3D*)src_host_ras->getRawData();

  // matte information
  uchar4* matte_host = nullptr;
  TRasterGR8P matte_host_ras;
  if (doMatte) {
    matte_host_ras = TRasterGR8P(sizeof(uchar4) * dim.lx * dim.ly, 1);
    matte_host_ras->lock();
    matte_host = (uchar4*)matte_host_ras->getRawData();
  }

  // input the source image
  QVector3D* chann_p = src_host;
  for (int j = 0; j < dim.ly; j++) {
    TPixel32* pix = raster->pixels(j);
    for (int i = 0; i < dim.lx; i++, chann_p++, pix++) {
      (*chann_p).setX((float)pix->r / (float)TPixel32::maxChannelValue);
      (*chann_p).setY((float)pix->g / (float)TPixel32::maxChannelValue);
      (*chann_p).setZ((float)pix->b / (float)TPixel32::maxChannelValue);
    }
  }

  QVector3D paper_xyz = myHls2Xyz(myRgb2Hls(myPix2Rgb(paperColor)));
  QVector3D main_xyz  = myHls2Xyz(myRgb2Hls(myPix2Rgb(mainColor)));
  QVector3D sub1_xyz  = myHls2Xyz(myRgb2Hls(myPix2Rgb(subColor1)));
  QVector3D sub2_xyz  = myHls2Xyz(myRgb2Hls(myPix2Rgb(subColor2)));
  QVector3D sub3_xyz  = myHls2Xyz(myRgb2Hls(myPix2Rgb(subColor3)));

  // obtain the plane equations intersecting three colors
  // plane of main, sub1, and sub2
  QVector4D pane_m12 = getPane(main_xyz, sub1_xyz, sub2_xyz);
  // plane of main, sub1, and sub3
  QVector4D pane_m13 = getPane(main_xyz, sub1_xyz, sub3_xyz);
  // plane of main, sub2, and sub3
  QVector4D pane_m23 = getPane(main_xyz, sub2_xyz, sub3_xyz);
  // plane of sub1, sub2, and sub3
  QVector4D pane_123 = getPane(sub1_xyz, sub2_xyz, sub3_xyz);

  // vector  paper -> main color
  QVector3D vec_pm = main_xyz - paper_xyz;

  float de = pane_123.x() * vec_pm.x() + pane_123.y() * vec_pm.y() +
             pane_123.z() * vec_pm.z();
  float t = (pane_123.w() -
             (pane_123.x() * paper_xyz.x() + pane_123.y() * paper_xyz.y() +
              pane_123.z() * paper_xyz.z())) /
            de;

  // intersection of paper-main vector and the plane 123
  QVector3D R = paper_xyz + t * vec_pm;

  int maxThreadCount =
      std::max(1, QThreadPool::globalInstance()->maxThreadCount() / 2);
  int start = 0;
  for (int t = 0; t < maxThreadCount; t++) {
    int end = (int)std::round((float)(dim.lx * dim.ly * (t + 1)) /
                              (float)maxThreadCount);

    Separate4ColorsTask* task = new Separate4ColorsTask(
        start, end, src_host, out_host, paper_xyz, main_xyz, sub1_xyz, sub2_xyz,
        sub3_xyz, pane_m12, pane_m13, pane_m23, pane_123, R,
        (float)m_subColorAdjustFld->getValue(),
        (float)m_borderSmoothnessFld->getValue());

    QThreadPool::globalInstance()->start(task);

    start = end;
  }

  QThreadPool::globalInstance()->waitForDone();

  src_host_ras->unlock();

  if (doMatte) {
    // mask by threshold, then expand it
    for (int i = 0; i < dim.lx * dim.ly; i++) {
      maskByThres(i, out_host, matte_host, matteThres);
    }

    for (int r = 0; r < matteRadius; r++) {
      for (int y = 0; y < dim.ly; y++) {
        for (int x = 0; x < dim.lx; x++) {
          expandMask(x, y, matte_host, dim.lx, dim.ly, r);
        }
      }
    }
  }

  auto getChannelValue = [](unsigned char pencil_chanVal,
                            float ratio) -> unsigned char {
    return (unsigned char)(std::round((float)pencil_chanVal * ratio));
  };

  bool showMask      = m_showMatteBtn->isChecked();
  auto getMatteColor = [&](TPixel32 lineColor) -> TPixel32 {
    if (!isPreview || !showMask) return TPixel32::Transparent;
    TPixelD lineColorD = toPixelD(lineColor);
    double h, l, s;
    rgb2hls(lineColorD.r, lineColorD.g, lineColorD.b, &h, &l, &s);
    // if the specified color is reddish, use blue color for transparent area
    if ((h <= 30.0 || 330.0 <= h) && s >= 0.2)
      return BlueMaskColor;
    else
      return RedMaskColor;
  };

  TPixel32 matteColor_m  = getMatteColor(mainColor);
  TPixel32 matteColor_s1 = getMatteColor(subColor1);
  TPixel32 matteColor_s2 = getMatteColor(subColor2);
  TPixel32 matteColor_s3 = getMatteColor(subColor3);

  // show alpha channel
  if (isPreview && m_showAlphaBtn->isChecked()) {
    mainColor = TPixel::White;
    subColor1 = TPixel::White;
    subColor2 = TPixel::White;
    subColor3 = TPixel::White;
  }

  // compute result
  QVector4D* ratio_p = out_host;
  uchar4* matte_p    = matte_host;
  for (int j = 0; j < dim.ly; j++) {
    TPixel32* pix_m  = ras_m->pixels(j);
    TPixel32* pix_c1 = ras_c1->pixels(j);
    TPixel32* pix_c2 = ras_c2->pixels(j);
    TPixel32* pix_c3 = ras_c3->pixels(j);
    for (int i = 0; i < dim.lx;
         i++, ratio_p++, pix_m++, pix_c1++, pix_c2++, pix_c3++) {
      // main
      if (!doMatte || (*matte_p).x > 0) {
        float ratio_m = (*ratio_p).x();
        if (doMatte)
          ratio_m *=
              (float)(matteRadius + 1 - (int)(*matte_p).x) / (float)matteRadius;
        ratio_m = std::min(1.0f, ratio_m);

        pix_m->r = getChannelValue(mainColor.r, ratio_m);
        pix_m->g = getChannelValue(mainColor.g, ratio_m);
        pix_m->b = getChannelValue(mainColor.b, ratio_m);
        pix_m->m = getChannelValue(TPixel32::maxChannelValue, ratio_m);
      } else {
        *pix_m = matteColor_m;
      }

      // sub1
      if (!doMatte || (*matte_p).y > 0) {
        float ratio_s1 = (*ratio_p).y();
        if (doMatte)
          ratio_s1 *=
              (float)(matteRadius + 1 - (int)(*matte_p).y) / (float)matteRadius;
        ratio_s1 = std::min(1.0f, ratio_s1);

        pix_c1->r = getChannelValue(subColor1.r, ratio_s1);
        pix_c1->g = getChannelValue(subColor1.g, ratio_s1);
        pix_c1->b = getChannelValue(subColor1.b, ratio_s1);
        pix_c1->m = getChannelValue(TPixel32::maxChannelValue, ratio_s1);
      } else {
        *pix_c1 = matteColor_s1;
      }

      // sub2
      if (!doMatte || (*matte_p).z > 0) {
        float ratio_s2 = (*ratio_p).z();
        if (doMatte)
          ratio_s2 *=
              (float)(matteRadius + 1 - (int)(*matte_p).z) / (float)matteRadius;
        ratio_s2 = std::min(1.0f, ratio_s2);

        pix_c2->r = getChannelValue(subColor2.r, ratio_s2);
        pix_c2->g = getChannelValue(subColor2.g, ratio_s2);
        pix_c2->b = getChannelValue(subColor2.b, ratio_s2);
        pix_c2->m = getChannelValue(TPixel32::maxChannelValue, ratio_s2);
      } else {
        *pix_c2 = matteColor_s2;
      }

      // sub3
      if (!doMatte || (*matte_p).w > 0) {
        float ratio_s3 = (*ratio_p).w();
        if (doMatte)
          ratio_s3 *=
              (float)(matteRadius + 1 - (int)(*matte_p).w) / (float)matteRadius;
        ratio_s3 = std::min(1.0f, ratio_s3);

        pix_c3->r = getChannelValue(subColor3.r, ratio_s3);
        pix_c3->g = getChannelValue(subColor3.g, ratio_s3);
        pix_c3->b = getChannelValue(subColor3.b, ratio_s3);
        pix_c3->m = getChannelValue(TPixel32::maxChannelValue, ratio_s3);
      } else {
        *pix_c3 = matteColor_s3;
      }

      if (doMatte) matte_p++;
    }
  }
  out_host_ras->unlock();

  if (doMatte) matte_host_ras->unlock();
}

void SeparateColorsPopup::showEvent(QShowEvent* e) { onPreviewBtnPressed(); }

//-------------------------------------------------------------------
// store the current value to user env file

void SeparateColorsPopup::hideEvent(QHideEvent* e) {
  TPixel32 col = m_paperColorField->getColor();
  SeparateColorsPopup_PaperColor =
      QColor(col.r, col.g, col.b).name().toStdString();
  col = m_mainColorField->getColor();
  SeparateColorsPopup_MainColor =
      QColor(col.r, col.g, col.b).name().toStdString();
  col = m_subColor1Field->getColor();
  SeparateColorsPopup_SubColor1 =
      QColor(col.r, col.g, col.b).name().toStdString();
  col = m_subColor2Field->getColor();
  SeparateColorsPopup_SubColor2 =
      QColor(col.r, col.g, col.b).name().toStdString();
  col = m_subColor3Field->getColor();
  SeparateColorsPopup_SubColor3 =
      QColor(col.r, col.g, col.b).name().toStdString();

  SeparateColorsPopup_SubAdjust    = m_subColorAdjustFld->getValue();
  SeparateColorsPopup_BorderSmooth = m_borderSmoothnessFld->getValue();
  SeparateColorsPopup_OutMain      = (int)m_outMainCB->isChecked();
  SeparateColorsPopup_OutSub1      = (int)m_outSub1CB->isChecked();
  SeparateColorsPopup_OutSub2      = (int)m_outSub2CB->isChecked();
  SeparateColorsPopup_OutSub3      = (int)m_outSub3CB->isChecked();

  SeparateColorsPopup_MainSuffix = m_mainSuffixEdit->text().toStdString();
  SeparateColorsPopup_Sub1Suffix = m_sub1SuffixEdit->text().toStdString();
  SeparateColorsPopup_Sub2Suffix = m_sub2SuffixEdit->text().toStdString();
  SeparateColorsPopup_Sub3Suffix = m_sub3SuffixEdit->text().toStdString();

  SeparateColorsPopup_doMatte        = (int)m_matteGroupBox->isChecked();
  SeparateColorsPopup_MatteThreshold = m_matteThreshold->getValue();
  SeparateColorsPopup_MatteRadius    = m_matteRadius->getValue();

  SeparateColorsPopup_FileFormat =
      m_fileFormatCombo->currentText().toStdString();
}

//-------------------------------------------------------------------

void SeparateColorsPopup::separate() {
  if (!m_outMainCB->isChecked() && !m_outSub1CB->isChecked() &&
      !m_outSub2CB->isChecked() && !m_outSub3CB->isChecked())
    return;

  m_isConverting = true;
  m_progressDialog->show();

  m_notifier->reset();
  int from = -1;
  int to   = -1;

  for (int i = 0; !m_notifier->abortTask() && i < m_srcFilePaths.size(); i++) {
    QString label;
    TFilePath dstFilePath;
    if (m_srcFilePaths.size() == 1) {
      from  = m_fromFld->text().toInt();
      to    = m_toFld->text().toInt();
      label = QString(
          tr("Separating %1")
              .arg(QString::fromStdString(m_srcFilePaths[i].getLevelName())));
    } else {
      TLevelP levelTmp;
      TLevelReaderP lrTmp = TLevelReaderP(m_srcFilePaths[i]);
      if (lrTmp) levelTmp = lrTmp->loadInfo();
      from  = 1;
      to    = levelTmp->getFrameCount();
      label = QString(
          tr("Converting level %1 of %2: %3")
              .arg(i + 1)
              .arg(m_srcFilePaths.size())
              .arg(QString::fromStdString(m_srcFilePaths[i].getLevelName())));
    }

    m_progressDialog->setLabelText(label);

    m_notifier->notifyFrameCompleted(0);

    TOutputProperties* oprop = TApp::instance()
                                   ->getCurrentScene()
                                   ->getScene()
                                   ->getProperties()
                                   ->getOutputProperties();
    int framerate = oprop->getFrameRate();

    doSeparate(m_srcFilePaths[i], from, to, framerate, m_notifier,
               m_outSub3CB->isChecked());

    // save settings for reproducing parameters afterwards
    TFilePath settingsFp = m_srcFilePaths[i].withNoFrame().withType("sep");
    doSaveSettings(settingsFp);
  }

  onSeparateFinished();
}

//-------------------------------------------------------------------

void SeparateColorsPopup::onPreviewBtnPressed() {
  TRaster32P orgRas, mainRas, sub1Ras, sub2Ras, sub3Ras;
  doPreview(orgRas, mainRas, sub1Ras, sub2Ras, sub3Ras);
  if (m_outSub3CB->isChecked())
    m_separateSwatch->setRaster(orgRas, mainRas, sub1Ras, sub2Ras, sub3Ras);
  else
    m_separateSwatch->setRaster(orgRas, mainRas, sub1Ras, sub2Ras);
}

//-------------------------------------------------------------------

void SeparateColorsPopup::onPreviewFrameFieldValueChanged(bool isDragging) {
  int frame = m_previewFrameField->getValue();
  if (frame > m_srcFrames.size()) return;

  TFilePath fp = m_srcFrames.at(frame - 1).first;
  TFrameId fId = m_srcFrames.at(frame - 1).second;

  m_previewFrameLabel->setText(QString::fromStdWString(
      fp.withFrame(fId).withoutParentDir().getWideString()));

  onChange(isDragging);
}

//-------------------------------------------------------------------

void SeparateColorsPopup::onChange(bool isDragging) {
  // if autopreview is activated
  if (m_autoBtn->isChecked() && !isDragging && !m_srcFrames.isEmpty())
    onPreviewBtnPressed();
}

//-------------------------------------------------------------------

void SeparateColorsPopup::onSaveSettings() {
  // specify saving path
  static GenericSaveFilePopup* savePopup = 0;
  if (!savePopup) {
    savePopup = new GenericSaveFilePopup(tr("Save Settings"));
    savePopup->addFilterType("sep");
  }

  savePopup->setFolder(m_srcFilePaths[0].getParentDir());
  savePopup->setFilename(
      m_srcFilePaths[0].withoutParentDir().withNoFrame().withType("sep"));

  TFilePath fp = savePopup->getPath();
  if (fp.isEmpty()) return;

  doSaveSettings(fp);
}

//-------------------------------------------------------------------

void SeparateColorsPopup::doSaveSettings(const TFilePath& fp) {
  QSettings settings(fp.getQString(), QSettings::IniFormat);
  settings.beginGroup("SeparateColorsParameters");

  TPixel32 col = m_paperColorField->getColor();
  settings.setValue("PaperColor", QColor(col.r, col.g, col.b).name());
  col = m_mainColorField->getColor();
  settings.setValue("MainColor", QColor(col.r, col.g, col.b).name());
  col = m_subColor1Field->getColor();
  settings.setValue("SubColor1", QColor(col.r, col.g, col.b).name());
  col = m_subColor2Field->getColor();
  settings.setValue("SubColor2", QColor(col.r, col.g, col.b).name());
  col = m_subColor3Field->getColor();
  settings.setValue("SubColor3", QColor(col.r, col.g, col.b).name());

  settings.setValue("SubAdjust", m_subColorAdjustFld->getValue());
  settings.setValue("BorderSmooth", m_borderSmoothnessFld->getValue());
  settings.setValue("OutMain", (int)m_outMainCB->isChecked());
  settings.setValue("OutSub1", (int)m_outSub1CB->isChecked());
  settings.setValue("OutSub2", (int)m_outSub2CB->isChecked());
  settings.setValue("OutSub3", (int)m_outSub3CB->isChecked());

  settings.setValue("MainSuffix", m_mainSuffixEdit->text());
  settings.setValue("Sub1Suffix", m_sub1SuffixEdit->text());
  settings.setValue("Sub2Suffix", m_sub2SuffixEdit->text());
  settings.setValue("Sub3Suffix", m_sub3SuffixEdit->text());

  settings.setValue("doMatte", (int)m_matteGroupBox->isChecked());
  settings.setValue("MatteThreshold", m_matteThreshold->getValue());
  settings.setValue("MatteRadius", m_matteRadius->getValue());

  settings.setValue("FileFormat", m_fileFormatCombo->currentText());
  // following values will be loaded only if "loadAll" option is ON
  settings.setValue("SaveIn", m_saveInFileFld->getPath());
  settings.setValue("FrameFrom", m_fromFld->text());
  settings.setValue("FrameTo", m_toFld->text());
  settings.endGroup();
}

//-------------------------------------------------------------------

void SeparateColorsPopup::onLoadSettings() {
  // specify saving path
  static GenericLoadFilePopup* loadPopup = 0;
  if (!loadPopup) {
    loadPopup = new GenericLoadFilePopup(tr("Load Settings"));
    loadPopup->addFilterType("sep");
  }
  loadPopup->setFolder(m_srcFilePaths[0].getParentDir());

  TFilePath fp = loadPopup->getPath();
  if (fp.isEmpty()) return;

  doLoadSettings(fp, false);

  onChange(false);
}

//-------------------------------------------------------------------

void SeparateColorsPopup::doLoadSettings(const TFilePath& fp, bool loadAll) {
  QSettings settings(fp.getQString(), QSettings::IniFormat);
  if (!settings.childGroups().contains("SeparateColorsParameters")) {
    DVGui::error(
        tr("Failed to load %1.").arg(fp.withoutParentDir().getQString()));
    return;
  }

  settings.beginGroup("SeparateColorsParameters");
  QStringList keys = settings.childKeys();

  auto loadColor = [&](const QString key, DVGui::ColorField* field) {
    if (!keys.contains(key)) return;
    QColor color;
    color.setNamedColor(settings.value(key).toString());
    field->setColor(TPixel32(color.red(), color.green(), color.blue()));
  };
  auto loadDouble = [&](const QString key, DVGui::DoubleField* field) {
    if (!keys.contains(key)) return;
    field->setValue(settings.value(key).toDouble());
  };
  auto loadBool = [&](const QString key, QCheckBox* cb) {
    if (!keys.contains(key)) return;
    cb->setChecked(settings.value(key).toInt() != 0);
  };
  auto loadString = [&](const QString key, DVGui::LineEdit* field) {
    if (!keys.contains(key)) return;
    field->setText(settings.value(key).toString());
  };
  auto clamp = [&](int val, int min, int max) {
    if (val < min) return min;
    if (val > max) return max;
    return val;
  };

  loadColor("PaperColor", m_paperColorField);
  loadColor("MainColor", m_mainColorField);
  loadColor("SubColor1", m_subColor1Field);
  loadColor("SubColor2", m_subColor2Field);
  loadColor("SubColor3", m_subColor3Field);

  loadDouble("SubAdjust", m_subColorAdjustFld);
  loadDouble("BorderSmooth", m_borderSmoothnessFld);

  loadBool("OutMain", m_outMainCB);
  loadBool("OutSub1", m_outSub1CB);
  loadBool("OutSub2", m_outSub2CB);
  loadBool("OutSub3", m_outSub3CB);

  loadString("MainSuffix", m_mainSuffixEdit);
  loadString("Sub1Suffix", m_sub1SuffixEdit);
  loadString("Sub2Suffix", m_sub2SuffixEdit);
  loadString("Sub3Suffix", m_sub3SuffixEdit);

  if (keys.contains("doMatte"))
    m_matteGroupBox->setChecked(settings.value("doMatte").toInt() != 0);
  loadDouble("MatteThreshold", m_matteThreshold);
  if (keys.contains("MatteRadius"))
    m_matteRadius->setValue(settings.value("MatteRadius").toInt());
  if (keys.contains("FileFormat"))
    m_fileFormatCombo->setCurrentText(settings.value("FileFormat").toString());

  if (loadAll) {
    if (keys.contains("SaveIn"))
      m_saveInFileFld->setPath(settings.value("SaveIn").toString());
    if (keys.contains("FrameFrom") && m_fromFld->isEnabled())
      m_fromFld->setValue(
          clamp(settings.value("FrameFrom").toInt(), 1, m_srcFrames.size()));
    if (keys.contains("FrameTo") && m_toFld->isEnabled())
      m_toFld->setValue(
          clamp(settings.value("FrameTo").toInt(), 1, m_srcFrames.size()));
  }
  settings.endGroup();
}

//-------------------------------------------------------------------

void SeparateColorsPopup::onSaveInPathChanged() {
  QString newPath = m_saveInFileFld->getPath();
  if (!TSystem::doesExistFileOrLevel(TFilePath(newPath))) {
    QString question;
    question = QObject::tr(
        "The chosen folder path does not exist."
        "\nDo you want to create it?");
    int ret = DVGui::MsgBox(question, QObject::tr("Create"),
                            QObject::tr("Cancel"), 0);
    if (ret == 0 || ret == 2) {
      m_saveInFileFld->setPath(m_lastAcceptedSaveInPath);
      m_saveInFileFld->setFocus();
      return;
    }

    if (!TSystem::touchParentDir(TFilePath(newPath) + "dummy")) {
      DVGui::warning(tr("Failed to create the folder."));
      m_saveInFileFld->setPath(m_lastAcceptedSaveInPath);
      m_saveInFileFld->setFocus();
      return;
    }
  }
  m_lastAcceptedSaveInPath = newPath;
  m_saveInFileFld->setPath(newPath);
}

//=============================================================================
// SeparateColorsPopupCommand
//-----------------------------------------------------------------------------

OpenPopupCommandHandler<SeparateColorsPopup> openSeparateColorsPopup(
    MI_SeparateColors);
