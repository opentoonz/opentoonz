

#include "viewerdraw.h"
#include "tapp.h"
#include "toonz/txsheethandle.h"
#include "toonz/tscenehandle.h"
#include "toonz/tframehandle.h"
#include "toonz/tcolumnhandle.h"
#include "toonz/tobjecthandle.h"
#include "toonz/cleanupparameters.h"
#include "sceneviewer.h"
#include "ruler.h"

#include "tgl.h"
#include "trop.h"
#include "toonz/txsheet.h"
#include "toonz/toonzscene.h"
#include "toonz/sceneproperties.h"
#include "toonz/tstageobjectid.h"
#include "toonz/tstageobjecttree.h"
#include "toonz/stage2.h"
#include "toonz/tcamera.h"
#include "toonz/tproject.h"
#include "toonz/preferences.h"
#include "toonz/toonzfolders.h"
#include "toonzqt/gutil.h"
#include "tconvert.h"

#include "tenv.h"
#include "tsystem.h"
#include "tfilepath_io.h"
#include "tstream.h"

#include "subcameramanager.h"

#include <QSettings>

TEnv::StringVar EnvSafeAreaName("SafeAreaName", "PR_safe");

/* TODO, move to include */
void getSafeAreaSizeList(QList<QList<double>> &_sizeList);

//=============================================================================
//=============================================================================
// SafeAreaData
//-----------------------------------------------------------------------------

void getSafeAreaSizeList(QList<QList<double>> &_sizeList) {
  static QList<QList<double>> sizeList;

  static TFilePath projectPath;
  static QString safeAreaName;

  TFilePath fp                = TEnv::getConfigDir();
  QString currentSafeAreaName = QString::fromStdString(EnvSafeAreaName);

  if (fp != projectPath || currentSafeAreaName != safeAreaName) {
    sizeList.clear();

    projectPath  = fp;
    safeAreaName = currentSafeAreaName;

    std::string safeAreaFileName = "safearea.ini";

    while (!TFileStatus(fp + safeAreaFileName).doesExist() && !fp.isRoot() &&
           fp.getParentDir() != TFilePath())
      fp = fp.getParentDir();

    fp = fp + safeAreaFileName;

    if (TFileStatus(fp).doesExist()) {
      QSettings settings(toQString(fp), QSettings::IniFormat);

      // find the current safearea name from the list
      QStringList groups = settings.childGroups();
      for (int g = 0; g < groups.size(); g++) {
        settings.beginGroup(groups.at(g));
        // If found, get the safe area setting values
        if (safeAreaName == settings.value("name", "").toString()) {
          // enter area group
          settings.beginGroup("area");

          QStringList keys = settings.childKeys();
          for (int i = 0; i < keys.size(); i++) {
            QList<QVariant> tmp =
                settings.value(keys.at(i), QList<QVariant>()).toList();
            QList<double> val_list;
            for (int j = 0; j < tmp.size(); j++)
              val_list.push_back(tmp.at(j).toDouble());
            sizeList.push_back(val_list);
          }

          // close area group
          settings.endGroup();

          settings.endGroup();
          break;
        }
        settings.endGroup();
      }
      // If not found, then put some temporal values..
      if (sizeList.isEmpty()) {
        QList<double> tmpList0, tmpList1;
        tmpList0 << 80.0 << 80.0;
        tmpList1 << 90.0 << 90.0;
        sizeList << tmpList0 << tmpList1;
      }
    }
  }

  _sizeList = sizeList;
}

//-----------------------------------------------------------------------------
namespace {
//-----------------------------------------------------------------------------

// da spostare in tgl.h?

inline void glVertex(const T3DPointD &p) { glVertex3d(p.x, p.y, p.z); }

//-----------------------------------------------------------------------------

// da spostare in util.h?

inline int findLowerIndex(double x, double step) {
  int index;
  double istep = 1 / step;
  if (x >= 0)
    index = tfloor(x * istep);
  else
    index = -tceil(-x * istep);
  assert(index * step <= x && x < (index + 1) * step);
  return index;
}

//-----------------------------------------------------------------------------

inline int findUpperIndex(double x, double step) {
  int index = -findLowerIndex(-x, step);
  assert((index - 1) * step < x && x <= index * step);
  return index;
}

//-----------------------------------------------------------------------------

T3DPointD make3dPoint(const TPointD &p, double z) {
  return T3DPointD(p.x, p.y, z);
}

//-----------------------------------------------------------------------------
/*
void getCameraSection(T3DPointD points[4], int row, double z)
{
  TXsheet *xsh = TApp::instance()->getCurrentXsheet()->getXsheet();
  TStageObjectId cameraId = xsh->getStageObjectTree()->getCurrentCameraId();
  double camZ = xsh->getZ(cameraId, row);
  TAffine camAff = xsh->getPlacement(cameraId, row);

  TRectD cameraRect =
    TApp::instance()->getCurrentScene()->getScene()->getCurrentCamera()->getStageRect();

  TPointD p[4];
  p[0] = camAff * cameraRect.getP00();
  p[1] = camAff * cameraRect.getP10();
  p[2] = camAff * cameraRect.getP11();
  p[3] = camAff * cameraRect.getP01();
  TPointD center = 0.5*(p[0]+p[2]);

  double f = 1000;
  double sc = 1+(camZ-z)/f;

  for(int i=0;i<4;i++)
    points[i] = make3dPoint(center+sc*(p[i]-center), z);

}
*/

//-----------------------------------------------------------------------------
}  // namespace
//=============================================================================

//=============================================================================
// ViewerDraw
//-----------------------------------------------------------------------------

//-----------------------------------------------------------------------------
/*! when camera view mode, draw the mask plane outside of the camera box
*/
void ViewerDraw::drawCameraMask(SceneViewer *viewer) {
  TXsheet *xsh = TApp::instance()->getCurrentXsheet()->getXsheet();

  //  TAffine viewMatrix = viewer->getViewMatrix();
  int x1, x2, y1, y2;
  viewer->rect().getCoords(&x1, &y1, &x2, &y2);
  TRect clipRect = TRect(x1, y1, x2 + 1, y2 + 1);

  GLfloat modelView[16];
  glGetFloatv(GL_MODELVIEW_MATRIX, modelView);
  TAffine modelViewAff(modelView[0], modelView[4], modelView[12], modelView[1],
                       modelView[5], modelView[13]);

  TRectD cameraRect = getCameraRect();

  TPointD clipCorner[] = {
      modelViewAff.inv() * TPointD(clipRect.x0, clipRect.y0),
      modelViewAff.inv() * TPointD(clipRect.x1, clipRect.y0),
      modelViewAff.inv() * TPointD(clipRect.x1, clipRect.y1),
      modelViewAff.inv() * TPointD(clipRect.x0, clipRect.y1)};

  // bounds: nel sistema di riferimento "corrente" bounds e' il piu' piccolo
  // rettangolo che copre completamente tutto il viewer
  TRectD bounds;
  bounds.x0 = bounds.x1 = clipCorner[0].x;
  bounds.y0 = bounds.y1 = clipCorner[0].y;
  int i;
  for (i = 1; i < 4; i++) {
    const TPointD &p = clipCorner[i];
    if (p.x < bounds.x0)
      bounds.x0 = p.x;
    else if (p.x > bounds.x1)
      bounds.x1 = p.x;
    if (p.y < bounds.y0)
      bounds.y0 = p.y;
    else if (p.y > bounds.y1)
      bounds.y1 = p.y;
  }

  // set the camera mask color same as the previewBG color
  TPixel32 maskColor = Preferences::instance()->getPreviewBgColor();
  double mask_r, mask_g, mask_b;
  mask_r = (double)maskColor.r / 255.0;
  mask_g = (double)maskColor.g / 255.0;
  mask_b = (double)maskColor.b / 255.0;
  glColor3d(mask_r, mask_g, mask_b);

  if (cameraRect.overlaps(bounds)) {
    double x0 = cameraRect.x0;
    double y0 = cameraRect.y0;
    double x1 = cameraRect.x1;
    double y1 = cameraRect.y1;

    if (bounds.x0 <= x0) tglFillRect(bounds.x0, bounds.y0, x0, bounds.y1);
    if (x1 <= bounds.x1) tglFillRect(x1, bounds.y0, bounds.x1, bounds.y1);

    if (x0 < bounds.x1 && x1 > bounds.x0) {
      double xa = std::max(x0, bounds.x0);
      double xb = std::min(x1, bounds.x1);
      if (bounds.y0 < y0) tglFillRect(xa, bounds.y0, xb, y0);
      if (y1 < bounds.y1) tglFillRect(xa, y1, xb, bounds.y1);
    }
  } else {
    tglFillRect(bounds);
  }
}

//-----------------------------------------------------------------------------

void ViewerDraw::drawGridAndGuides(SceneViewer *viewer, double sc, Ruler *vr,
                                   Ruler *hr, bool gridEnabled) {
  int vGuideCount     = 0;
  int hGuideCount     = 0;
  if (vr) vGuideCount = vr->getGuideCount();
  if (hr) hGuideCount = hr->getGuideCount();

  // int xp1, yp1, xp2, yp2;
  // viewer->geometry().getCoords(&xp1, &yp1, &xp2, &yp2);
  TRect clipRect = TRect(-20, -10, viewer->width() + 10, viewer->height() + 20);
  // TRect clipRect = TRect(xp1- 20, yp2 + 20, xp2 + 10, yp1 - 10);
  // viewer->rect().adjusted(-20, -10, 10, 20);
  clipRect -=
      TPoint((clipRect.x0 + clipRect.x1) / 2, (clipRect.y0 + clipRect.y1) / 2);

  TAffine mat = viewer->getViewMatrix().inv();

  TPointD p00 = mat * convert(clipRect.getP00());
  TPointD p01 = mat * convert(clipRect.getP01());
  TPointD p10 = mat * convert(clipRect.getP10());
  TPointD p11 = mat * convert(clipRect.getP11());

  double xmin = std::min({p00.x, p01.x, p10.x, p11.x});
  double xmax = std::max({p00.x, p01.x, p10.x, p11.x});
  double ymin = std::min({p00.y, p01.y, p10.y, p11.y});
  double ymax = std::max({p00.y, p01.y, p10.y, p11.y});

  double step  = 10;
  double absSc = abs(sc);
  if (absSc * step < 4)
    while (absSc * step < 4) step *= 5;
  else if (absSc * step > 20)
    while (absSc * step > 20) step *= 0.2;

  int i0 = findLowerIndex(xmin, step);
  int i1 = findUpperIndex(xmax, step);

  int j0 = findLowerIndex(ymin, step);
  int j1 = findUpperIndex(ymax, step);

  double x0 = i0 * step;
  double x1 = i1 * step;
  double y0 = j0 * step;
  double y1 = j1 * step;

  if (gridEnabled) {
    glColor3d(0.7, 0.7, 0.7);

    glBegin(GL_LINES);
    glVertex2d(0, y0);
    glVertex2d(0, y1);
    glVertex2d(x0, 0);
    glVertex2d(x1, 0);
    glEnd();

    glEnable(GL_LINE_STIPPLE);

    glLineStipple(1, 0xAAAA);

    glBegin(GL_LINES);
    int i;
    for (i = i0; i <= i1; i++) {
      double x = i * step;
      if (i == 0)
        continue;
      else if ((abs(i) % 10) == 0)
        glColor3d(0.8, 0.8, 0.8);
      else
        glColor3d(0.9, 0.9, 0.9);
      glVertex2d(x, y0);
      glVertex2d(x, y1);
    }
    for (i = j0; i <= j1; i++) {
      double y = i * step;
      if (i == 0)
        continue;
      else if ((abs(i) % 10) == 0)
        glColor3d(0.8, 0.8, 0.8);
      else
        glColor3d(0.9, 0.9, 0.9);
      glVertex2d(x0, y);
      glVertex2d(x1, y);
    }
    glEnd();

    glDisable(GL_LINE_STIPPLE);
  }

  glColor3d(0.7, 0.7, 0.7);
  glLineStipple(1, 0xAAAA);
  glEnable(GL_LINE_STIPPLE);

  int i;
  glBegin(GL_LINES);
  for (i = 0; i < hGuideCount; i++) {
    double x = hr->getGuide(i);
    glVertex2d(x, y0);
    glVertex2d(x, y1);
  }
  for (i = 0; i < vGuideCount; i++) {
    double y = vr->getGuide(i);
    glVertex2d(x0, y);
    glVertex2d(x1, y);
  }
  glEnd();
  glDisable(GL_LINE_STIPPLE);
}

//-----------------------------------------------------------------------------

void ViewerDraw::drawColorcard(UCHAR channel) {
  ToonzScene *scene = TApp::instance()->getCurrentScene()->getScene();
  TRectD rect       = getCameraRect();

  TPixel color = (ToonzCheck::instance()->getChecks() & ToonzCheck::eBlackBg)
                     ? TPixel::Black
                     : scene->getProperties()->getBgColor();
  if (channel == 0)
    color.m = 255;  // fondamentale: senno' non si vedono i fill con le texture
                    // in camera stand!
  else {
    if (channel == TRop::MChan) {
      switch (channel) {
      case TRop::RChan:
        color.r = color.g = color.b = color.m = color.r;
        break;
      case TRop::GChan:
        color.r = color.g = color.b = color.m = color.g;
        break;
      case TRop::BChan:
        color.r = color.g = color.b = color.m = color.b;
        break;
      case TRop::MChan:
        color.r = color.g = color.b = color.m = color.m;
        break;
      default:
        assert(false);
      }
    } else {
      color.r = channel & TRop::RChan ? color.r : 0;
      color.b = channel & TRop::BChan ? color.b : 0;
      color.g = channel & TRop::GChan ? color.g : 0;
    }
  }
  tglColor(color);
  tglFillRect(rect);
}

//-----------------------------------------------------------------------------

void ViewerDraw::draw3DCamera(unsigned long flags, double zmin, double phi) {
  bool cameraRef = 0 != (flags & ViewerDraw::CAMERA_REFERENCE);
  bool safeArea  = 0 != (flags & ViewerDraw::SAFE_AREA);

  TApp *app               = TApp::instance();
  int frame               = app->getCurrentFrame()->getFrame();
  ToonzScene *scene       = app->getCurrentScene()->getScene();
  TXsheet *xsh            = scene->getXsheet();
  TStageObjectId cameraId = xsh->getStageObjectTree()->getCurrentCameraId();
  TAffine camAff          = xsh->getPlacement(cameraId, frame);
  double zcam             = xsh->getZ(cameraId, frame);
  TRectD rect             = getCameraRect();

  double znear = 1000 + zcam - 100;

  TPointD cameraCorners[4] = {camAff * rect.getP00(), camAff * rect.getP10(),
                              camAff * rect.getP11(), camAff * rect.getP01()};
  TPointD cameraCenter = 0.5 * (cameraCorners[0] + cameraCorners[2]);

  T3DPointD cage[4][4];
  std::vector<double> cageZ;
  cageZ.push_back(znear);
  double ztable = 0;
  if (ztable < znear) cageZ.push_back(ztable);
  if (zmin < znear) cageZ.push_back(zmin);

  int columnIndex = app->getCurrentColumn()->getColumnIndex();
  if (columnIndex >= 0) {
    TStageObjectId objId = TStageObjectId::ColumnId(columnIndex);
    double zcur          = xsh->getZ(objId, frame);
    if (zcur < znear) cageZ.push_back(zcur);
  }
  std::sort(cageZ.begin(), cageZ.end());
  int m = (int)cageZ.size();

  for (int i = 0; i < m; i++) {
    double z  = cageZ[i];
    double sc = (1000 + zcam - z) * 0.001;
    for (int j = 0; j < 4; j++)
      cage[i][j] =
          make3dPoint(cameraCenter + sc * (cameraCorners[j] - cameraCenter), z);
  }

  if (cameraRef) {
    glColor3d(1.0, 0.0, 1.0);
    glLineStipple(1, 0xFFFC);
  } else {
    glColor3d(1.0, 0.0, 0.0);
    glLineStipple(1, 0xCCCC);
  }
  glEnable(GL_LINE_STIPPLE);

  double yBigBox       = -Stage::bigBoxSize[1];
  double xBigBox       = Stage::bigBoxSize[0];
  if (phi < 0) xBigBox = -xBigBox;

  for (int i = 0; i < m; i++) {
    // ombre
    glColor3d(1.0, 0.8, 0.8);
    glBegin(GL_LINES);
    glVertex3d(cage[i][0].x, yBigBox, cageZ[i]);
    glVertex3d(cage[i][1].x, yBigBox, cageZ[i]);
    glVertex3d(xBigBox, cage[i][1].y, cageZ[i]);
    glVertex3d(xBigBox, cage[i][2].y, cageZ[i]);
    glEnd();

    glColor3d(1.0, 0.0, 0.0);
    glBegin(GL_LINE_STRIP);
    for (int j = 0; j < 4; j++) glVertex(cage[i][j]);
    glVertex(cage[i][0]);
    glEnd();
  }
  if (m >= 2) {
    // ombre
    glColor3d(1.0, 0.8, 0.8);
    glVertex3d(cage[0][0].x, yBigBox, cageZ[0]);
    glVertex3d(cage[m - 1][0].x, yBigBox, cageZ[m - 1]);
    glVertex3d(cage[0][1].x, yBigBox, cageZ[0]);
    glVertex3d(cage[m - 1][1].x, yBigBox, cageZ[m - 1]);
    glVertex3d(xBigBox, cage[0][1].y, cageZ[0]);
    glVertex3d(xBigBox, cage[m - 1][1].y, cageZ[m - 1]);
    glVertex3d(xBigBox, cage[0][2].y, cageZ[0]);
    glVertex3d(xBigBox, cage[m - 1][2].y, cageZ[m - 1]);

    glColor3d(1.0, 0.0, 0.0);
    glBegin(GL_LINES);
    for (int j = 0; j < 4; j++) {
      glVertex(cage[0][j]);
      glVertex(cage[m - 1][j]);
    }

    glEnd();
  }
  /*

if(objId != cameraId)
{
glColor3d(1.0,0.0,1.0);
glBegin(GL_LINE_STRIP);
for(j=0;j<4;j++) glVertex(currentRect[j]);
glVertex(currentRect[0]);
glEnd();
}
*/

  glDisable(GL_LINE_STIPPLE);
  /*
glPushMatrix();
glTranslated(0,0,zcam);
drawCamera(flags);
glPopMatrix();
*/
}

//-----------------------------------------------------------------------------

TRectD ViewerDraw::getCameraRect() {
  if (CleanupPreviewCheck::instance()->isEnabled() ||
      CameraTestCheck::instance()->isEnabled())
    return TApp::instance()
        ->getCurrentScene()
        ->getScene()
        ->getProperties()
        ->getCleanupParameters()
        ->m_camera.getStageRect();
  else
    return TApp::instance()
        ->getCurrentScene()
        ->getScene()
        ->getCurrentCamera()
        ->getStageRect();
}

//-----------------------------------------------------------------------------

void ViewerDraw::drawSafeArea() {
  TRectD rect = getCameraRect();
  glColor3d(1.0, 0.0, 0.0);
  glLineStipple(1, 0xCCCC);
  glEnable(GL_LINE_STIPPLE);

  QList<QList<double>> sizeList;
  getSafeAreaSizeList(sizeList);

  double ux = 0.5 * rect.getLx();
  double uy = 0.5 * rect.getLy();

  for (int i = 0; i < sizeList.size(); i++) {
    QList<double> curSize = sizeList.at(i);
    if (curSize.size() == 5)
      tglColor(
          TPixel((int)curSize.at(2), (int)curSize.at(3), (int)curSize.at(4)));
    else
      tglColor(TPixel32::Red);

    double fx = 0.01 * curSize.at(0);
    double fy = 0.01 * curSize.at(1);

    tglDrawRect(-ux * fx, -uy * fy, ux * fx, uy * fy);
  }

  glDisable(GL_LINE_STIPPLE);
}

//-----------------------------------------------------------------------------

void ViewerDraw::drawCamera(unsigned long flags, double pixelSize) {
  bool cameraRef = 0 != (flags & ViewerDraw::CAMERA_REFERENCE);
  bool camera3d  = 0 != (flags & ViewerDraw::CAMERA_3D);
  bool solidLine = 0 != (flags & ViewerDraw::SOLID_LINE);
  bool subcamera = 0 != (flags & ViewerDraw::SUBCAMERA);

  TApp *app               = TApp::instance();
  ToonzScene *scene       = app->getCurrentScene()->getScene();
  TXsheet *xsh            = scene->getXsheet();
  TStageObjectId cameraId = xsh->getStageObjectTree()->getCurrentCameraId();

  TRectD rect = getCameraRect();

  if (cameraRef) {
    glColor3d(1.0, 0.0, 1.0);
    glLineStipple(1, 0xFFFC);
  } else {
    glColor3d(1.0, 0.0, 0.0);
    glLineStipple(1, solidLine ? 0xFFFF : 0xCCCC);
  }
  glEnable(GL_LINE_STIPPLE);

  // bordo
  glBegin(GL_LINE_STRIP);
  glVertex2d(rect.x0, rect.y0);
  glVertex2d(rect.x0, rect.y1 - pixelSize);
  glVertex2d(rect.x1 - pixelSize, rect.y1 - pixelSize);
  glVertex2d(rect.x1 - pixelSize, rect.y0);
  glVertex2d(rect.x0, rect.y0);
  glEnd();

  // croce al centro
  double dx = 0.05 * rect.getP00().x;
  double dy = 0.05 * rect.getP00().y;
  tglDrawSegment(TPointD(-dx, -dy), TPointD(dx, dy));
  tglDrawSegment(TPointD(-dx, dy), TPointD(dx, -dy));

  glDisable(GL_LINE_STIPPLE);

  // nome della camera
  if (!camera3d) {
    TPointD pos = rect.getP01() + TPointD(0, 4);
    std::string name;
    if (CleanupPreviewCheck::instance()->isEnabled())
      name = "Cleanup Camera";
    else
      name = xsh->getStageObject(cameraId)->getName();
    glPushMatrix();
    glTranslated(pos.x, pos.y, 0);
    glScaled(2, 2, 2);
    tglDrawText(TPointD(), name);
    glPopMatrix();
  }

  // draw preview sub-camera
  if (!CleanupPreviewCheck::instance()->isEnabled() && subcamera) {
    PreviewSubCameraManager *inst = PreviewSubCameraManager::instance();
    TRect previewSubRect(inst->getEditingCameraInterestRect());
    if (previewSubRect.getLx() > 0 && previewSubRect.getLy() > 0) {
      TRectD stagePreviewSubRect(inst->getEditingCameraInterestStageRect());

      glLineStipple(1, 0xCCCC);
      glEnable(GL_LINE_STIPPLE);

      glColor3d(1.0, 0.0, 1.0);
      glBegin(GL_LINE_STRIP);
      glVertex2d(stagePreviewSubRect.x0, stagePreviewSubRect.y0);
      glVertex2d(stagePreviewSubRect.x0, stagePreviewSubRect.y1 - pixelSize);
      glVertex2d(stagePreviewSubRect.x1 - pixelSize,
                 stagePreviewSubRect.y1 - pixelSize);
      glVertex2d(stagePreviewSubRect.x1 - pixelSize, stagePreviewSubRect.y0);
      glVertex2d(stagePreviewSubRect.x0, stagePreviewSubRect.y0);
      glEnd();

      glDisable(GL_LINE_STIPPLE);
    }
  }
}

//-----------------------------------------------------------------------------

void ViewerDraw::draw3DFrame(double minZ, double phi) {
  double a = Stage::bigBoxSize[0];
  double b = Stage::bigBoxSize[1];
  double c = Stage::bigBoxSize[2];

  double d = phi < 0 ? -a : a;

  double z0 = minZ;
  double z1 = 1000;

  glColor3d(0.9, 0.9, 0.86);
  glBegin(GL_LINES);
  glVertex3d(-a, -b, z0);
  glVertex3d(-a, -b, z1);
  glVertex3d(a, -b, z0);
  glVertex3d(a, -b, z1);
  glVertex3d(d, b, z0);
  glVertex3d(d, b, z1);

  glVertex3d(-a, -b, z0);
  glVertex3d(a, -b, z0);
  glVertex3d(d, -b, z0);
  glVertex3d(d, b, z0);

  glVertex3d(-a, -b, z1);
  glVertex3d(a, -b, z1);
  glVertex3d(d, -b, z1);
  glVertex3d(d, b, z1);
  glEnd();

  glColor3d(0.7, 0.7, 0.7);
  glBegin(GL_LINES);
  glVertex3d(0, -b, z0);
  glVertex3d(0, -b, z1);
  glVertex3d(d, 0, z0);
  glVertex3d(d, 0, z1);
  glEnd();
}

//-----------------------------------------------------------------------------

void ViewerDraw::drawFieldGuide() {
  double f = 1;  // controlla (indirettamente) la grandezza delle scritte

  TSceneProperties *sprop =
      TApp::instance()->getCurrentScene()->getScene()->getProperties();

  int n        = sprop->getFieldGuideSize();
  if (n < 4) n = 4;
  double ar    = sprop->getFieldGuideAspectRatio();
  double lx    = 0.5 * n / f * Stage::inch;  // 320;
  double ly    = lx / ar;
  glPushMatrix();
  glScaled(f, f, 1);
  double ux = lx / n;
  double uy = ly / n;
  glColor3d(.4, .4, .4);
  glBegin(GL_LINES);
  int i;
  for (i = -n; i <= n; i++) {
    glVertex2d(i * ux, -n * uy);
    glVertex2d(i * ux, n * uy);
    glVertex2d(-n * ux, i * uy);
    glVertex2d(n * ux, i * uy);
  }
  glVertex2d(-n * ux, -n * uy);
  glVertex2d(n * ux, n * uy);
  glVertex2d(-n * ux, n * uy);
  glVertex2d(n * ux, -n * uy);
  glEnd();
  for (i = 1; i <= n; i++) {
    TPointD delta = 0.03 * TPointD(ux, uy);
    std::string s = std::to_string(i);
    tglDrawText(TPointD(0, i * uy) + delta, s);
    tglDrawText(TPointD(0, -i * uy) + delta, s);
    tglDrawText(TPointD(-i * ux, 0) + delta, s);
    tglDrawText(TPointD(i * ux, 0) + delta, s);
  }
  glPopMatrix();
}

//-----------------------------------------------------------------------------

void ViewerDraw::drawDisk(int &tableDLId) {
  static TPixel32 currentBgColor;

  TPixel32 bgColor;

  if ((ToonzCheck::instance()->getChecks() & ToonzCheck::eBlackBg))
    bgColor = TPixel::Black;
  else
    bgColor = Preferences::instance()->getViewerBgColor();

  if (tableDLId == -1 || currentBgColor != bgColor) {
    currentBgColor = bgColor;
    tableDLId      = createDiskDisplayList();
  }
  glCallList(tableDLId);
}

//-----------------------------------------------------------------------------

unsigned int ViewerDraw::createDiskDisplayList() {
  GLuint id = glGenLists(1);
  static std::vector<TPointD> sinCosTable;
  if (sinCosTable.empty()) {
    int n = 120;
    sinCosTable.resize(n);
    int i;
    for (i = 0; i < n; i++) {
      double ang       = 2 * 3.1415293 * i / n;
      sinCosTable[i].x = cos(ang);
      sinCosTable[i].y = sin(ang);
    }
  }
  double r = 10 * Stage::inch;
  glNewList(id, GL_COMPILE);
  glColor3d(.6, .65, .7);
  glBegin(GL_POLYGON);
  int i;
  int n = 120;
  for (i = 0; i < n; i++) tglVertex(r * sinCosTable[i]);
  glEnd();

  glColor3d(0, 0, 0);
  glBegin(GL_LINE_STRIP);
  for (i = 0; i < n; i++) tglVertex(r * sinCosTable[i]);
  tglVertex(r * sinCosTable[0]);
  glEnd();

  TPixel32 bgColor;

  if (ToonzCheck::instance()->getChecks() & ToonzCheck::eBlackBg)
    bgColor = TPixel::Black;
  else
    bgColor = Preferences::instance()->getViewerBgColor();

  tglColor(bgColor);

  r *= 0.9;
  int m = 13;
  glBegin(GL_POLYGON);
  for (i = n - m; i < n; i++) tglVertex(r * sinCosTable[i]);
  for (i = 0; i <= m; i++) tglVertex(r * sinCosTable[i]);
  for (i = n / 2 - m; i <= n / 2 + m; i++) tglVertex(r * sinCosTable[i]);
  glEnd();

  // per non lasciare il colore corrente con il matte a zero
  glColor4d(0, 0, 0, 1);

  glEndList();
  return (id);
}
