

#include "morphtool.h"
#include "tgl.h"
#include "tvectorgl.h"
#include "tvectorrenderdata.h"
#include "tvectorimage.h"
#include "tstroke.h"
#include <math.h>

#include "tools/toolhandle.h"
#include "toonz/txshlevelhandle.h"
#include "toonz/tframehandle.h"
#include "toonz/txshsimplelevel.h"

#include <QKeyEvent>

class Deformation {
public:
  std::vector<TPointD> m_controlPoints;
  int m_selected;
  TAffine m_aff;
  std::vector<TPointD> m_delta;

  int getClosest(const TPointD &p) const;

  Deformation();

  void update() {
    TPointD p0 = m_controlPoints[0];
    TPointD p1 = m_controlPoints[2];
    TPointD p2 = m_controlPoints[4];
    double a00 = p0.x - p2.x, a01 = p1.x - p2.x, a10 = p0.y - p2.y,
           a11 = p1.y - p2.y;
    TAffine aff(a00, a01, 0, a10, a11, 0);
    aff       = aff.inv();
    TPointD d = -(aff * p2);
    aff.a13   = d.x;
    aff.a23   = d.y;
    m_aff     = aff;
    m_delta.resize(3);
    m_delta[0] = m_controlPoints[1] - p0;
    m_delta[1] = m_controlPoints[3] - p1;
    m_delta[2] = m_controlPoints[5] - p2;
  }

  TPointD apply(const TPointD &p, double t = 1.0) {
    TPointD d = m_aff * p;
    double c0 = d.x, c1 = d.y, c2 = 1 - c0 - c1;
    TPointD delta = c0 * m_delta[0] + c1 * m_delta[1] + c2 * m_delta[2];
    return p + delta * t;
  }

  void deform(TStroke *dstStroke, const TStroke *srcStroke, double t = 1.0) {
    int n = srcStroke->getControlPointCount();
    if (dstStroke->getControlPointCount() < n)
      n = dstStroke->getControlPointCount();

    for (int i = 0; i < n; i++) {
      TThickPoint srcPoint = srcStroke->getControlPoint(i);
      dstStroke->setControlPoint(i, apply(srcPoint, t));
    }
  }

  void deform(TVectorImage *dstImage, const TVectorImage *srcImage,
              double t = 1.0) {
    update();
    int n                                      = srcImage->getStrokeCount();
    if ((int)dstImage->getStrokeCount() < n) n = dstImage->getStrokeCount();
    std::vector<int> ii(n);
    std::vector<TStroke *> oldStrokes(n);
    for (int i = 0; i < n; i++) {
      ii[i]         = i;
      oldStrokes[i] = srcImage->getStroke(i);
      deform(dstImage->getStroke(i), oldStrokes[i], t);
    }
    dstImage->notifyChangedStrokes(ii, oldStrokes);
  }

  void updateLevel() {
    TTool::Application *app = TTool::getApplication();
    if (!app->getCurrentLevel()->getLevel()) return;
    TXshSimpleLevelP xl = app->getCurrentLevel()->getLevel()->getSimpleLevel();
    if (app->getCurrentFrame()->getFrameType() != TFrameHandle::LevelFrame)
      return;

    TFrameId fid      = app->getCurrentFrame()->getFid();
    TVectorImageP src = xl->getFrame(fid, true);
    int count         = src->getStrokeCount();

    for (int i = 1; i < 10; i++) {
      ++fid;
      if (!xl->isFid(fid)) {
        TVectorImageP vi = new TVectorImage();
        xl->setFrame(fid, vi);
      }
      TVectorImageP vi  = xl->getFrame(fid, true);
      TVectorImageP dst = src->clone();
      deform(dst.getPointer(), src.getPointer(), (double)i / (double)9);
      count = dst->getStrokeCount();
      vi->mergeImage(dst, TAffine());
      app->getCurrentTool()->getTool()->notifyImageChanged(fid);
    }
  }
};

Deformation::Deformation() : m_selected(-1) {
  m_controlPoints.resize(6);
  m_controlPoints[0] = TPointD(-250, 100);
  m_controlPoints[2] = TPointD(0, -300);
  m_controlPoints[4] = TPointD(250, 100);
  for (int i = 0; i < 6; i += 2) m_controlPoints[i + 1] = m_controlPoints[i];
}

int Deformation::getClosest(const TPointD &p) const {
  int k            = -1;
  double closestD2 = 0;
  for (int i = 0; i < (int)m_controlPoints.size(); i++) {
    TPointD cp = m_controlPoints[i];
    double d2  = norm2(p - cp);
    if (k < 0 || d2 <= closestD2) {
      closestD2 = d2;
      k         = i;
    }
  }
  return closestD2 < 100 ? k : -1;
}

Deformation deformation;

/*
TThickPoint deform(const TThickPoint &p)
{
  double r2 = p.x*p.x+p.y*p.y;
  double f = exp(-r2*0.001);
  return p + delta * f;
}
*/

MorphTool::MorphTool() : m_pixelSize(1) {}

MorphTool::~MorphTool() {}

void MorphTool::setImage(const TVectorImageP &vi) { m_vi = vi; }

void MorphTool::leftButtonDown(const TPointD &pos, const TMouseEvent &e) {
  m_lastPos = m_firstPos = pos;
  int index              = deformation.getClosest(pos);
  if (index >= 0)
    deformation.m_selected = index;
  else
    deformation.m_selected = -1;

  if (m_vi && index >= 0) {
    m_vi2 = m_vi->clone();
    deformation.deform(m_vi2.getPointer(), m_vi.getPointer());
  } else {
    m_vi2 = TVectorImageP();
  }
}

void MorphTool::leftButtonDrag(const TPointD &pos, const TMouseEvent &e) {
  if (deformation.m_selected < 0) return;
  TPointD delta = pos - m_lastPos;
  m_lastPos     = pos;
  deformation.m_controlPoints[deformation.m_selected] += delta;
  if ((deformation.m_selected & 1) == 0)
    deformation.m_controlPoints[deformation.m_selected + 1] += delta;
  if (m_vi2 && m_vi) deformation.deform(m_vi2.getPointer(), m_vi.getPointer());
}

void MorphTool::leftButtonUp(const TPointD &pos, const TMouseEvent &e) {
  m_vi2 = TVectorImageP();
}

void MorphTool::draw() {
  m_pixelSize = sqrt(tglGetPixelSize2());
  if (m_vi2) {
    TVectorRenderData rd(TTranslation(10, 10), TRect(), 0, 0);
    tglDraw(rd, m_vi2.getPointer());
  }

  double u = m_pixelSize * 5;
  for (int i = 0; i < (int)deformation.m_controlPoints.size(); i++) {
    TPointD p     = deformation.m_controlPoints[i];
    bool selected = deformation.m_selected == i;
    bool base     = (i & 1) == 0;
    if (base)
      if (selected)
        glColor3d(0.8, 0.8, 0.1);
      else
        glColor3d(0.5, 0.5, 0.1);
    else if (selected)
      glColor3d(0.8, 0.3, 0.1);
    else
      glColor3d(0.5, 0.1, 0.1);

    double r = base ? u * 2 : u * 1;
    tglDrawDisk(p, r);
    glColor3d(0, 0, 0);
    tglDrawCircle(p, r);
  }
  glColor3f(0, 1, 0);
  for (int i = 0; i + 1 < (int)deformation.m_controlPoints.size(); i += 2) {
    TPointD a = deformation.m_controlPoints[i];
    TPointD b = deformation.m_controlPoints[i + 1];
    tglDrawSegment(a, b);
  }
  /*
deformation.update();
glBegin(GL_LINES);
for(double x = -200; x<=200; x+=20)
for(double y = -200; y<=200; y+=20)
{
TPointD p0(x,y);
TPointD p1 = deformation.apply(p0);
glColor3d(0,1,0);
tglVertex(p0);
glColor3d(1,0,0);
tglVertex(p1);
}
glEnd();
*/
}

bool MorphTool::keyDown(QKeyEvent *event) {
  if (event->key() == Qt::Key_A)
    deformation.updateLevel();
  else
    return false;
  return true;
}
