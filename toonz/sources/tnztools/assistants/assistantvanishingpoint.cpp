

// TnzTools includes
#include <tools/assistant.h>
#include <tools/assistants/guidelineline.h>

// TnzCore includes
#include <tgl.h>

//*****************************************************************************************
//    TAssistantVanishingPoint implementation
//*****************************************************************************************

class TAssistantVanishingPoint final : public TAssistant {
  Q_DECLARE_TR_FUNCTIONS(TAssistantVanishingPoint)
public:
  const TStringId m_idPassThrough;
  const TStringId m_idGrid;
  const TStringId m_idPerspective;

protected:
  TAssistantPoint &m_center;
  TAssistantPoint &m_a0;
  TAssistantPoint &m_a1;
  TAssistantPoint &m_b0;
  TAssistantPoint &m_b1;
  TAssistantPoint &m_grid0;
  TAssistantPoint &m_grid1;

public:
  TAssistantVanishingPoint(TMetaObject &object)
      : TAssistant(object)
      , m_idPassThrough("passThrough")
      , m_idGrid("grid")
      , m_idPerspective("perspective")
      , m_center(addPoint("center", TAssistantPoint::CircleCross))
      , m_a0(addPoint("a0", TAssistantPoint::Circle, TPointD(-50.0, 0.0)))
      , m_a1(addPoint("a1", TAssistantPoint::Circle, TPointD(-75.0, 0.0)))
      , m_b0(addPoint("b0", TAssistantPoint::Circle, TPointD(50.0, 0.0)))
      , m_b1(addPoint("b1", TAssistantPoint::Circle, TPointD(75.0, 0.0)))
      , m_grid0(addPoint("grid0", TAssistantPoint::CircleDoubleDots,
                         TPointD(0.0, -50.0)))
      , m_grid1(addPoint("grid1", TAssistantPoint::CircleDots,
                         TPointD(25.0, -50.0))) {
    addProperty(new TBoolProperty(m_idPassThrough.str(), getPassThrough()));
    addProperty(new TBoolProperty(m_idGrid.str(), getGrid()));
    addProperty(new TBoolProperty(m_idPerspective.str(), getPerspective()));
  }

  static QString getLocalName() { return tr("Vanishing Point"); }

  void updateTranslation() const override {
    setTranslation(m_idPassThrough, tr("Pass Through"));
    setTranslation(m_idGrid, tr("Grid"));
    setTranslation(m_idPerspective, tr("Perspective"));
  }

  inline bool getPassThrough() const {
    return data()[m_idPassThrough].getBool();
  }
  inline bool getGrid() const { return data()[m_idGrid].getBool(); }
  inline bool getPerspective() const {
    return data()[m_idPerspective].getBool();
  }

  void onDataChanged(const TVariant &value) override {
    TAssistant::onDataChanged(value);
    m_grid0.visible = m_grid1.visible = getGrid();
  }

private:
  void fixCenter() {
    if (!(m_a0.position == m_a1.position) &&
        !(m_b0.position == m_b1.position)) {
      const TPointD &a = m_a0.position;
      const TPointD &b = m_b0.position;
      const TPointD da = m_a1.position - a;
      const TPointD db = m_b1.position - b;
      const TPointD ab = b - a;
      double k         = db.x * da.y - db.y * da.x;
      if (fabs(k) > TConsts::epsilon) {
        double lb           = (da.x * ab.y - da.y * ab.x) / k;
        m_center.position.x = lb * db.x + b.x;
        m_center.position.y = lb * db.y + b.y;
      }
    }
  }

  void fixSidePoint(TAssistantPoint &p0, TAssistantPoint &p1,
                    TPointD previousP0) {
    if (p0.position != m_center.position && p0.position != p1.position) {
      TPointD dp0 = p0.position - m_center.position;
      TPointD dp1 = p1.position - previousP0;
      double l0   = norm(dp0);
      double l1   = norm(dp1);
      if (l0 > TConsts::epsilon && l0 + l1 > TConsts::epsilon)
        p1.position = m_center.position + dp0 * ((l0 + l1) / l0);
    }
  }

  void fixSidePoint(TAssistantPoint &p0, TAssistantPoint &p1) {
    fixSidePoint(p0, p1, p0.position);
  }

  void fixGrid1(const TPointD &previousCenter, const TPointD &previousGrid0) {
    TPointD dx = previousCenter - previousGrid0;
    double l   = norm2(dx);
    if (l <= TConsts::epsilon * TConsts::epsilon) return;
    dx = dx * (1.0 / sqrt(l));
    TPointD dy(-dx.y, dx.x);

    TPointD d = m_grid1.position - previousGrid0;
    double x  = (dx * d);
    double y  = (dy * d);

    dx = m_center.position - m_grid0.position;
    l  = norm2(dx);
    if (l <= TConsts::epsilon * TConsts::epsilon) return;
    dx = dx * (1.0 / sqrt(l));
    dy = TPointD(-dx.y, dx.x);

    m_grid1.position = m_grid0.position + dx * x + dy * y;
  }

public:
  void onFixPoints() override {
    fixSidePoint(m_a0, m_a1);
    fixSidePoint(m_b0, m_b1);
    fixCenter();
  }

  void onMovePoint(TAssistantPoint &point, const TPointD &position) override {
    TPointD previousCenter = m_center.position;
    TPointD previous       = point.position;
    point.position         = position;
    if (&point == &m_center) {
      fixSidePoint(m_a0, m_a1);
      fixSidePoint(m_b0, m_b1);
    } else if (&point == &m_a0) {
      fixSidePoint(m_a0, m_a1, previous);
      fixSidePoint(m_b0, m_b1);
    } else if (&point == &m_b0) {
      fixSidePoint(m_a0, m_a1);
      fixSidePoint(m_b0, m_b1, previous);
    } else if (&point == &m_a1) {
      fixCenter();
      fixSidePoint(m_a0, m_a1);
      fixSidePoint(m_b0, m_b1);
    } else if (&point == &m_b1) {
      fixCenter();
      fixSidePoint(m_b0, m_b1);
      fixSidePoint(m_a0, m_a1);
    }

    if (&point == &m_grid0) {
      fixGrid1(previousCenter, previous);
    } else if (&point != &m_grid1) {
      fixGrid1(previousCenter, m_grid0.position);
    }
  }

  void getGuidelines(const TPointD &position, const TAffine &toTool,
                     TGuidelineList &outGuidelines) const override {
    if (getPassThrough()) {
      outGuidelines.push_back(TGuidelineP(new TGuidelineInfiniteLine(
          getEnabled(), getMagnetism(), toTool * m_center.position, position)));
    } else {
      outGuidelines.push_back(TGuidelineP(new TGuidelineRay(
          getEnabled(), getMagnetism(), toTool * m_center.position, position)));
    }
  }

  void drawSimpleGrid() const {
    double alpha     = getDrawingGridAlpha();
    const TPointD &p = m_center.position;
    double pixelSize = sqrt(tglGetPixelSize2());
    double minStep   = 5.0 * pixelSize;

    // calculate rays count and step
    TPointD d0 = m_grid0.position - p;
    TPointD d1 = m_grid1.position - p;
    TPointD dp = d0;
    double l   = norm(d0);
    if (l <= TConsts::epsilon) return;
    if (norm2(d1) <= TConsts::epsilon * TConsts::epsilon) return;
    double a0                     = atan(d0);
    double a1                     = atan(d1);
    double da                     = fabs(a1 - a0);
    if (da > M_PI) da             = M_PI - da;
    if (da < TConsts::epsilon) da = TConsts::epsilon;
    double count                  = M_2PI / da;
    if (count > 1e6) return;
    double radiusPart = minStep / (da * l);
    if (radiusPart > 1.0) return;
    int raysCount = (int)round(count);
    double step   = M_2PI / (double)raysCount;

    // common data about viewport
    const TRectD oneBox(-1.0, -1.0, 1.0, 1.0);
    TAffine4 modelview, projection;
    glGetDoublev(GL_MODELVIEW_MATRIX, modelview.a);
    glGetDoublev(GL_PROJECTION_MATRIX, projection.a);
    TAffine matrix    = (projection * modelview).get2d();
    TAffine matrixInv = matrix.inv();

    // calculate range
    if (!(matrixInv * oneBox).contains(p)) {
      TPointD corners[4] = {
          TPointD(oneBox.x0, oneBox.y0), TPointD(oneBox.x0, oneBox.y1),
          TPointD(oneBox.x1, oneBox.y0), TPointD(oneBox.x1, oneBox.y1)};
      double angles[4];
      double a0 = 0.0, a1 = 0.0, da = 0.0;
      for (int i = 0; i < 4; ++i) {
        angles[i] = atan(matrixInv * corners[i] - p) + M_2PI;
        for (int j = 0; j < i; ++j) {
          double d        = fabs(angles[i] - angles[j]);
          if (d > M_PI) d = M_2PI - d;
          if (d > da) da = d, a0 = angles[i], a1 = angles[j];
        }
      }
      if (a1 < a0) std::swap(a1, a0);
      if (a1 - a0 > M_PI) {
        std::swap(a1, a0);
        a1 += M_2PI;
      }
      double a = atan(dp) + M_2PI;
      a0       = ceil((a0 - a) / step) * step + a;
      a1       = floor((a1 - a) / step) * step + a;

      double s  = sin(a0 - a);
      double c  = cos(a0 - a);
      dp        = TPointD(c * dp.x - s * dp.y, s * dp.x + c * dp.y);
      raysCount = (int)round((a1 - a0) / step);
    }

    // draw rays
    double s = sin(step);
    double c = cos(step);
    for (int i = 0; i < raysCount; ++i) {
      TPointD p0 = matrix * (p + dp * radiusPart);
      TPointD p1 = matrix * (p + dp);
      if (TGuidelineLineBase::truncateRay(oneBox, p0, p1))
        drawSegment(matrixInv * p0, matrixInv * p1, pixelSize, alpha);
      dp = TPointD(c * dp.x - s * dp.y, s * dp.x + c * dp.y);
    }
  }

  void drawPerspectiveGrid() const {
    // initial calculations
    double alpha          = getDrawingGridAlpha();
    const TPointD &center = m_center.position;
    double pixelSize      = sqrt(tglGetPixelSize2());
    double minStep        = 5.0 * pixelSize;

    TPointD step    = m_grid1.position - m_grid0.position;
    double stepLen2 = norm2(step);
    double stepLen  = sqrt(stepLen2);
    if (stepLen <= minStep) return;
    TPointD stepProj = step * (1.0 / stepLen2);

    TPointD dp        = center - m_grid0.position;
    double startX     = dp * stepProj;
    TPointD zeroPoint = m_grid0.position + step * startX;
    TPointD cz        = zeroPoint - center;
    double czLen2     = norm2(cz);
    double czLen      = sqrt(czLen2);
    if (czLen <= TConsts::epsilon) return;
    TPointD zeroProj = cz * (1.0 / czLen2);

    double smallK         = minStep / stepLen;
    TPointD smallGrid0    = center - dp * smallK;
    TPointD smallStep     = step * smallK;
    TPointD smallStepProj = stepProj * (1 / smallK);

    // common data about viewport
    const TRectD oneBox(-1.0, -1.0, 1.0, 1.0);
    TAffine4 modelview, projection;
    glGetDoublev(GL_MODELVIEW_MATRIX, modelview.a);
    glGetDoublev(GL_PROJECTION_MATRIX, projection.a);
    TAffine matrix    = (projection * modelview).get2d();
    TAffine matrixInv = matrix.inv();

    // calculate bounds
    bool found  = false;
    double minx = 0.0, maxx = 0.0;
    TPointD p0 = matrix * (smallGrid0);
    TPointD p1 = matrix * (smallGrid0 + smallStep);
    if (TGuidelineLineBase::truncateInfiniteLine(oneBox, p0, p1)) {
      p0   = matrixInv * p0;
      p1   = matrixInv * p1;
      minx = (p0 - smallGrid0) * smallStepProj;
      maxx = (p1 - smallGrid0) * smallStepProj;
      if (maxx < minx) std::swap(maxx, minx);
      found = true;
    }
    if (!oneBox.contains(matrix * center)) {
      TPointD corners[4] = {
          TPointD(oneBox.x0, oneBox.y0), TPointD(oneBox.x0, oneBox.y1),
          TPointD(oneBox.x1, oneBox.y0), TPointD(oneBox.x1, oneBox.y1)};
      for (int i = 0; i < 4; ++i) {
        TPointD p = matrixInv * corners[i] - center;
        double k  = p * zeroProj;
        if (k < TConsts::epsilon) continue;
        double x                     = startX + (p * stepProj) / k;
        if (!found || x < minx) minx = x;
        if (!found || x > maxx) maxx = x;
        found                        = true;
      }
      if (maxx <= minx) return;
    }

    // draw grid
    if (maxx - minx > 1e6) return;
    for (double x = ceil(minx); x < maxx; ++x) {
      TPointD p  = smallGrid0 + smallStep * x - center;
      TPointD p0 = matrix * (center + p);
      TPointD p1 = matrix * (center + p * 2.0);
      if (TGuidelineLineBase::truncateRay(oneBox, p0, p1))
        drawSegment(matrixInv * p0, matrixInv * p1, pixelSize, alpha);
    }

    // draw horizon
    p0 = matrix * (center);
    p1 = matrix * (center + step);
    if (TGuidelineLineBase::truncateInfiniteLine(oneBox, p0, p1))
      drawSegment(matrixInv * p0, matrixInv * p1, pixelSize, alpha);
  }

  void draw(TToolViewer *viewer, bool enabled) const override {
    double pixelSize = sqrt(tglGetPixelSize2());
    const TPointD &p = m_center.position;
    TPointD dx(20.0 * pixelSize, 0.0);
    TPointD dy(0.0, 10.0 * pixelSize);
    double alpha = getDrawingAlpha(enabled);
    drawSegment(p - dx - dy, p + dx + dy, pixelSize, alpha);
    drawSegment(p - dx + dy, p + dx - dy, pixelSize, alpha);
    if (getGrid()) {
      if (getPerspective())
        drawPerspectiveGrid();
      else
        drawSimpleGrid();
    }
  }

  void drawEdit(TToolViewer *viewer) const override {
    double pixelSize = sqrt(tglGetPixelSize2());
    drawSegment(m_center.position, m_a1.position, pixelSize);
    drawSegment(m_center.position, m_b1.position, pixelSize);
    TAssistant::drawEdit(viewer);
  }
};

//*****************************************************************************************
//    Registration
//*****************************************************************************************

static TAssistantTypeT<TAssistantVanishingPoint> assistantVanishingPoint(
    "assistantVanishingPoint");
