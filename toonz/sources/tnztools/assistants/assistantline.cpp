

// TnzTools includes
#include <tools/assistant.h>
#include <tools/assistants/guidelineline.h>

// TnzCore includes
#include <tgl.h>

//*****************************************************************************************
//    TAssistantLine implementation
//*****************************************************************************************

class TAssistantLine final : public TAssistant {
  Q_DECLARE_TR_FUNCTIONS(TAssistantVanishingPoint)
public:
  const TStringId m_idRestricktA;
  const TStringId m_idRestricktB;
  const TStringId m_idParallel;
  const TStringId m_idGrid;
  const TStringId m_idPerspective;

protected:
  TAssistantPoint &m_a;
  TAssistantPoint &m_b;
  TAssistantPoint &m_grid0;
  TAssistantPoint &m_grid1;

public:
  TAssistantLine(TMetaObject &object)
      : TAssistant(object)
      , m_idRestricktA("restrictA")
      , m_idRestricktB("restrictB")
      , m_idParallel("parallel")
      , m_idGrid("grid")
      , m_idPerspective("perspective")
      , m_a(addPoint("a", TAssistantPoint::CircleCross))
      , m_b(addPoint("b", TAssistantPoint::Circle, TPointD(100.0, 0.0)))
      , m_grid0(addPoint("grid0", TAssistantPoint::CircleDoubleDots,
                         TPointD(0.0, -50.0)))
      , m_grid1(addPoint("grid1", TAssistantPoint::CircleDots,
                         TPointD(25.0, -75.0))) {
    addProperty(new TBoolProperty(m_idRestricktA.str(), getRestrictA()));
    addProperty(new TBoolProperty(m_idRestricktB.str(), getRestrictB()));
    addProperty(new TBoolProperty(m_idParallel.str(), getParallel()));
    addProperty(new TBoolProperty(m_idGrid.str(), getGrid()));
    addProperty(new TBoolProperty(m_idPerspective.str(), getPerspective()));
  }

  static QString getLocalName() { return tr("Line"); }

  void updateTranslation() const override {
    setTranslation(m_idRestricktA, tr("Restrict A"));
    setTranslation(m_idRestricktB, tr("Restrict B"));
    setTranslation(m_idParallel, tr("Parallel"));
    setTranslation(m_idGrid, tr("Grid"));
    setTranslation(m_idPerspective, tr("Perspective"));
  }

  inline bool getRestrictA() const { return data()[m_idRestricktA].getBool(); }
  inline bool getRestrictB() const { return data()[m_idRestricktB].getBool(); }
  inline bool getParallel() const { return data()[m_idParallel].getBool(); }
  inline bool getGrid() const { return data()[m_idGrid].getBool(); }
  inline bool getPerspective() const {
    return data()[m_idPerspective].getBool();
  }

  void onDataChanged(const TVariant &value) override {
    TAssistant::onDataChanged(value);
    m_grid0.visible =
        getGrid() || (getParallel() && (getRestrictA() || getRestrictB()));
    m_grid1.visible = getGrid();
  }

private:
  void fixGrid1(const TPointD &previousA, const TPointD &previousB) {
    TPointD dx = previousB - previousA;
    double l   = norm2(dx);
    if (l <= TConsts::epsilon * TConsts::epsilon) return;
    dx = dx * (1.0 / sqrt(l));
    TPointD dy(-dx.y, dx.x);

    TPointD g1 = m_grid1.position - m_grid0.position;
    g1         = TPointD(dx * g1, dy * g1);

    dx = m_b.position - m_a.position;
    l  = norm2(dx);
    if (l <= TConsts::epsilon * TConsts::epsilon) return;
    dx = dx * (1.0 / sqrt(l));
    dy = TPointD(-dx.y, dx.x);

    m_grid1.position = m_grid0.position + dx * g1.x + dy * g1.y;
  }

public:
  void onMovePoint(TAssistantPoint &point, const TPointD &position) override {
    TPointD previousA = m_a.position;
    TPointD previousB = m_b.position;
    point.position    = position;
    if (&point != &m_grid1) fixGrid1(previousA, previousB);
  }

  void getGuidelines(const TPointD &position, const TAffine &toTool,
                     TGuidelineList &outGuidelines) const override {
    bool restrictA   = getRestrictA();
    bool restrictB   = getRestrictB();
    bool parallel    = getParallel();
    bool perspective = getPerspective();

    TPointD a     = toTool * m_a.position;
    TPointD b     = toTool * m_b.position;
    TPointD ab    = b - a;
    double abLen2 = norm2(ab);
    if (abLen2 < TConsts::epsilon * TConsts::epsilon) return;

    if (parallel) {
      TPointD abp = rotate90(ab);
      TPointD ag  = toTool * m_grid0.position - a;
      double k    = abp * ag;
      if (fabs(k) <= TConsts::epsilon) {
        if (restrictA || restrictB) return;
        a = position;
      } else {
        k = (abp * (position - a)) / k;
        a = a + ag * k;
      }
      if (perspective && (restrictA || restrictB)) ab = ab * k;
      b                                               = a + ab;
    }

    if (restrictA && restrictB)
      outGuidelines.push_back(
          TGuidelineP(new TGuidelineLine(getEnabled(), getMagnetism(), a, b)));
    else if (restrictA)
      outGuidelines.push_back(
          TGuidelineP(new TGuidelineRay(getEnabled(), getMagnetism(), a, b)));
    else if (restrictB)
      outGuidelines.push_back(TGuidelineP(
          new TGuidelineRay(getEnabled(), getMagnetism(), b, a)));  // b first
    else
      outGuidelines.push_back(TGuidelineP(
          new TGuidelineInfiniteLine(getEnabled(), getMagnetism(), a, b)));
  }

private:
  void drawRuler(const TPointD &a, const TPointD &b, double pixelSize,
                 bool perspective) const {
    double minStep = 10.0 * pixelSize;
    double alpha   = getDrawingGridAlpha();

    TPointD direction = b - a;
    double l2         = norm2(direction);
    if (l2 <= TConsts::epsilon * TConsts::epsilon) return;
    double dirLen   = sqrt(l2);
    TPointD dirProj = direction * (1.0 / l2);

    double xg0 = dirProj * (m_grid0.position - a);
    double xg1 = dirProj * (m_grid1.position - a);

    if (perspective) {
      // draw perspective
      double xa0 = dirProj * (m_a.position - a);
      double k = 0.0, begin = 0.0, end = 0.0;
      if (!calcPerspectiveStep(minStep / dirLen, 0.0, 1.0, xa0, xg0, xg1, k,
                               begin, end))
        return;
      for (double x = begin; fabs(x) < fabs(end); x *= k)
        drawDot(a + direction * (xa0 + x), alpha);
    } else {
      // draw linear
      double dx = fabs(xg1 - xg0);
      if (dx * dirLen < minStep) return;
      for (double x = xg0 - floor(xg0 / dx) * dx; x < 1.0; x += dx)
        drawDot(a + direction * x, alpha);
    }
  }

  void drawLine(const TAffine &matrix, const TAffine &matrixInv,
                double pixelSize, const TPointD &a, const TPointD &b,
                bool restrictA, bool restrictB, double alpha) const {
    const TRectD oneBox(-1.0, -1.0, 1.0, 1.0);
    TPointD aa = matrix * a;
    TPointD bb = matrix * b;
    if (restrictA && restrictB
            ? TGuidelineLineBase::truncateLine(oneBox, aa, bb)
            : restrictA ? TGuidelineLineBase::truncateRay(oneBox, aa, bb)
                        : restrictB ? TGuidelineLineBase::truncateRay(
                                          oneBox, bb, aa)  // aa first
                                    : TGuidelineLineBase::truncateInfiniteLine(
                                          oneBox, aa, bb))
      drawSegment(matrixInv * aa, matrixInv * bb, pixelSize, alpha);
  }

  void drawGrid(const TAffine &matrix, const TAffine &matrixInv,
                double pixelSize, bool restrictA, bool restrictB,
                bool perspective) const {
    double minStep = 10.0 * pixelSize;

    double alpha  = getDrawingGridAlpha();
    TPointD a     = m_a.position;
    TPointD b     = m_b.position;
    TPointD ab    = b - a;
    double abLen2 = norm2(ab);
    if (abLen2 < TConsts::epsilon * TConsts::epsilon) return;
    double abLen = sqrt(abLen2);

    TPointD g0 = m_grid0.position;
    TPointD g1 = m_grid1.position;

    TPointD abp = rotate90(ab);
    TPointD ag  = g0 - a;
    if (fabs(abp * ag) <= TConsts::epsilon) {
      if (restrictA || restrictB) return;
      ag = abp;
    }
    double agLen2 = norm2(ag);
    if (agLen2 < TConsts::epsilon * TConsts::epsilon) return;
    double agLen      = sqrt(agLen2);
    double abpAgK     = 1.0 / (abp * ag);
    TPointD abpAgProj = abp * abpAgK;

    // draw restriction lines
    if (perspective) {
      if (restrictA)
        drawLine(matrix, matrixInv, pixelSize, a, a + ag, false, false, alpha);
      if (restrictB)
        drawLine(matrix, matrixInv, pixelSize, a, a + ag + ab, false, false,
                 alpha);
    } else {
      if (restrictA)
        drawLine(matrix, matrixInv, pixelSize, a, a + ag, false, false, alpha);
      if (restrictB)
        drawLine(matrix, matrixInv, pixelSize, b, b + ag, false, false, alpha);
    }

    double minStepX = fabs(minStep * abLen * abpAgK);
    if (minStepX <= TConsts::epsilon) return;

    // calculate bounds
    const TRectD oneBox(-1.0, -1.0, 1.0, 1.0);
    TPointD corners[4] = {
        TPointD(oneBox.x0, oneBox.y0), TPointD(oneBox.x0, oneBox.y1),
        TPointD(oneBox.x1, oneBox.y0), TPointD(oneBox.x1, oneBox.y1)};
    double minX = 0.0, maxX = 0.0;
    for (int i = 0; i < 4; ++i) {
      double x                     = abpAgProj * (matrixInv * corners[i] - a);
      if (i == 0 || x < minX) minX = x;
      if (i == 0 || x > maxX) maxX = x;
    }
    if (maxX <= minX) return;

    double x0 = abpAgProj * (g0 - a);
    double x1 = abpAgProj * (g1 - a);

    if (perspective) {
      double k = 0.0, begin = 0.0, end = 0.0;
      if (!calcPerspectiveStep(minStepX, minX, maxX, 0.0, x0, x1, k, begin,
                               end))
        return;
      double abk = 1.0 / fabs(x0);
      for (double x = begin; fabs(x) < fabs(end); x *= k) {
        TPointD ca = a + ag * x;
        TPointD cb = ca + ab * (abk * x);
        drawLine(matrix, matrixInv, pixelSize, ca, cb, restrictA, restrictB,
                 alpha);
      }
    } else {
      double dx = fabs(x1 - x0);
      if (dx < minStepX) return;
      for (double x = x0 + ceil((minX - x0) / dx) * dx; x < maxX; x += dx) {
        TPointD ca = a + ag * x;
        drawLine(matrix, matrixInv, pixelSize, ca, ca + ab, restrictA,
                 restrictB, alpha);
      }
    }
  }

public:
  void draw(TToolViewer *viewer, bool enabled) const override {
    double alpha     = getDrawingAlpha(enabled);
    bool restrictA   = getRestrictA();
    bool restrictB   = getRestrictB();
    bool parallel    = getParallel();
    bool grid        = getGrid();
    bool perspective = getPerspective();

    // common data about viewport
    const TRectD oneBox(-1.0, -1.0, 1.0, 1.0);
    TAffine4 modelview, projection;
    glGetDoublev(GL_MODELVIEW_MATRIX, modelview.a);
    glGetDoublev(GL_PROJECTION_MATRIX, projection.a);
    TAffine matrix    = (projection * modelview).get2d();
    TAffine matrixInv = matrix.inv();
    double pixelSize  = sqrt(tglGetPixelSize2());

    // calculate range
    TPointD aa   = matrix * m_a.position;
    TPointD bb   = matrix * m_b.position;
    bool success = false;
    if (restrictA && restrictB)
      success = TGuidelineLineBase::truncateLine(oneBox, aa, bb);
    else if (restrictA)
      success = TGuidelineLineBase::truncateRay(oneBox, aa, bb);
    else if (restrictB)
      success = TGuidelineLineBase::truncateRay(oneBox, bb, aa);
    else
      success = TGuidelineLineBase::truncateInfiniteLine(oneBox, aa, bb);
    if (!success) return;
    TPointD a = matrixInv * aa;
    TPointD b = matrixInv * bb;

    // draw line
    drawSegment(a, b, pixelSize, alpha);

    // draw restriction marks
    if (restrictA || (!parallel && grid && perspective)) drawDot(m_a.position);
    if (restrictB) drawDot(m_b.position);

    if (grid) {
      if (getParallel()) {
        drawGrid(matrix, matrixInv, pixelSize, restrictA, restrictB,
                 perspective);
      } else {
        drawRuler(a, b, pixelSize, perspective);
      }
    }
  }
};

//*****************************************************************************************
//    Registration
//*****************************************************************************************

static TAssistantTypeT<TAssistantLine> assistantLine("assistantLine");
