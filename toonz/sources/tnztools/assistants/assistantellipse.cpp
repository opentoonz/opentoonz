

// TnzTools includes
#include <tools/assistant.h>
#include <tools/assistants/guidelineline.h>
#include <tools/assistants/guidelineellipse.h>

// TnzCore includes
#include <tgl.h>

// std includes
#include <limits>

//*****************************************************************************************
//    TAssistantEllipse implementation
//*****************************************************************************************

class TAssistantEllipse final : public TAssistant {
  Q_DECLARE_TR_FUNCTIONS(TAssistantEllipse)
public:
  const TStringId m_idRestricktA;
  const TStringId m_idRestricktB;
  const TStringId m_idRepeat;
  const TStringId m_idGrid;
  const TStringId m_idPerspective;

protected:
  TAssistantPoint &m_center;
  TAssistantPoint &m_a;
  TAssistantPoint &m_b;
  TAssistantPoint &m_grid0;
  TAssistantPoint &m_grid1;

public:
  TAssistantEllipse(TMetaObject &object)
      : TAssistant(object)
      , m_idRestricktA("restrictA")
      , m_idRestricktB("restrictB")
      , m_idRepeat("repeat")
      , m_idGrid("grid")
      , m_idPerspective("perspective")
      , m_center(addPoint("center", TAssistantPoint::CircleCross))
      , m_a(addPoint("a", TAssistantPoint::CircleFill, TPointD(100.0, 0.0)))
      , m_b(addPoint("b", TAssistantPoint::Circle, TPointD(0.0, 50.0)))
      , m_grid0(addPoint("grid0", TAssistantPoint::CircleDoubleDots,
                         TPointD(0.0, -50.0)))
      , m_grid1(addPoint("grid1", TAssistantPoint::CircleDots,
                         TPointD(25.0, -75.0))) {
    addProperty(new TBoolProperty(m_idRestricktA.str(), getRestrictA()));
    addProperty(new TBoolProperty(m_idRestricktB.str(), getRestrictB()));
    addProperty(new TBoolProperty(m_idRepeat.str(), getRepeat()));
    addProperty(new TBoolProperty(m_idGrid.str(), getGrid()));
    addProperty(new TBoolProperty(m_idPerspective.str(), getPerspective()));
  }

  static QString getLocalName() { return tr("Ellipse"); }

  void updateTranslation() const override {
    setTranslation(m_idRestricktA, tr("Restrict A"));
    setTranslation(m_idRestricktB, tr("Restrict B"));
    setTranslation(m_idRepeat, tr("Repeat"));
    setTranslation(m_idGrid, tr("Grid"));
    setTranslation(m_idPerspective, tr("Perspective"));
  }

  inline bool getRestrictA() const { return data()[m_idRestricktA].getBool(); }
  inline bool getRestrictB() const { return data()[m_idRestricktB].getBool(); }
  inline bool getRepeat() const { return data()[m_idRepeat].getBool(); }
  inline bool getGrid() const { return data()[m_idGrid].getBool(); }
  inline bool getPerspective() const {
    return data()[m_idPerspective].getBool();
  }

  void onDataChanged(const TVariant &value) override {
    TAssistant::onDataChanged(value);
    m_grid0.visible = m_grid1.visible = getGrid();
  }

private:
  void fixBAndGgid1(const TPointD &previousCenter, const TPointD &previousA) {
    TPointD dx = previousA - previousCenter;
    double l   = norm2(dx);
    if (l <= TConsts::epsilon * TConsts::epsilon) return;
    dx = dx * (1.0 / sqrt(l));
    TPointD dy(-dx.y, dx.x);

    double r2 = dy * (m_b.position - m_center.position);

    TPointD g1 = m_grid1.position - m_grid0.position;
    g1         = TPointD(dx * g1, dy * g1);

    dx = m_a.position - m_center.position;
    l  = norm2(dx);
    if (l <= TConsts::epsilon * TConsts::epsilon) return;
    dx = dx * (1.0 / sqrt(l));
    dy = TPointD(-dx.y, dx.x);

    m_grid1.position = m_grid0.position + dx * g1.x + dy * g1.y;
    m_b.position     = m_center.position + dy * r2;
  }

public:
  void onMovePoint(TAssistantPoint &point, const TPointD &position) override {
    TPointD previousCenter = m_center.position;
    TPointD previousA      = m_a.position;
    point.position         = position;
    if (&point == &m_center) {
      m_a.position += m_center.position - previousCenter;
      m_b.position += m_center.position - previousCenter;
    } else if (&point == &m_a || &point == &m_b)
      fixBAndGgid1(previousCenter, previousA);
  }

  TAffine calcEllipseMatrix() const {
    TPointD da = m_a.position - m_center.position;
    TPointD db = m_b.position - m_center.position;
    double r1  = norm(da);
    if (r1 <= TConsts::epsilon) return TAffine::zero();
    double r2 = fabs((rotate90(da) * db) * (1.0 / r1));
    if (r2 <= TConsts::epsilon) return TAffine::zero();
    return TAffine::translation(m_center.position) *
           TAffine::rotation(atan(da)) * TAffine::scale(r1, r2);
  }

  void getGuidelines(const TPointD &position, const TAffine &toTool,
                     TGuidelineList &outGuidelines) const override {
    bool restrictA = getRestrictA();
    bool restrictB = getRestrictB();
    bool repeat    = getRepeat();

    TAffine matrix = calcEllipseMatrix();
    if (matrix.isZero()) return;
    if (!restrictA && restrictB) {
      std::swap(matrix.rowX(), matrix.rowY());
      std::swap(restrictA, restrictB);
    }

    matrix            = toTool * matrix;
    TAffine matrixInv = matrix.inv();

    if (restrictA && restrictB) {
      // ellipse
      outGuidelines.push_back(TGuidelineP(new TGuidelineEllipse(
          getEnabled(), getMagnetism(), matrix, matrixInv)));
    } else if (!restrictA && !restrictB) {
      // scaled ellipse
      TPointD p = matrixInv * position;
      double l  = norm(p);
      outGuidelines.push_back(TGuidelineP(new TGuidelineEllipse(
          getEnabled(), getMagnetism(), matrix * TAffine::scale(l))));
    } else {  // restrictA
      TPointD p = matrixInv * position;
      if (repeat) {
        double ox = round(0.5 * p.x) * 2.0;
        p.x -= ox;
        matrix *= TAffine::translation(ox, 0.0);
      }

      // scale by Y
      if (p.x <= TConsts::epsilon - 1.0) {
        // line x = -1
        outGuidelines.push_back(TGuidelineP(new TGuidelineInfiniteLine(
            getEnabled(), getMagnetism(), matrix * TPointD(-1.0, 0.0),
            matrix * TPointD(-1.0, 1.0))));
      } else if (p.x >= 1.0 - TConsts::epsilon) {
        // line x = 1
        outGuidelines.push_back(TGuidelineP(new TGuidelineInfiniteLine(
            getEnabled(), getMagnetism(), matrix * TPointD(1.0, 0.0),
            matrix * TPointD(1.0, 1.0))));
      } else {
        // ellipse scaled by Y
        double k = fabs(p.y / sqrt(1.0 - p.x * p.x));
        outGuidelines.push_back(TGuidelineP(new TGuidelineEllipse(
            getEnabled(), getMagnetism(), matrix * TAffine::scale(1.0, k))));
      }
    }
  }

private:
  void drawEllipseRanges(const TAngleRangeSet &ranges,
                         const TAffine &ellipseMatrix,
                         const TAffine &screenMatrixInv, double pixelSize,
                         double alpha) const {
    assert(ranges.check());
    TAngleRangeSet actualRanges(ranges);
    const TRectD oneBox(-1.0, -1.0, 1.0, 1.0);
    if (!TGuidelineEllipse::truncateEllipse(
            actualRanges, ellipseMatrix.inv() * screenMatrixInv, oneBox))
      return;
    assert(actualRanges.check());

    int segments =
        TGuidelineEllipse::calcSegmentsCount(ellipseMatrix, pixelSize);
    double da = M_2PI / segments;
    double s  = sin(da);
    double c  = cos(da);

    for (TAngleRangeSet::Iterator i(actualRanges); i; ++i) {
      double a0 = i.d0();
      double a1 = i.d1greater();
      int cnt   = (int)floor((a1 - a0) / da);
      TPointD r(cos(a0), sin(a0));
      TPointD p0 = ellipseMatrix * r;
      for (int j = 0; j < cnt; ++j) {
        r          = TPointD(r.x * c - r.y * s, r.y * c + r.x * s);
        TPointD p1 = ellipseMatrix * r;
        drawSegment(p0, p1, pixelSize, alpha);
        p0 = p1;
      }
      drawSegment(p0, ellipseMatrix * TPointD(cos(a1), sin(a1)), pixelSize,
                  alpha);
    }
  }

  void drawEllipse(const TAffine &ellipseMatrix, const TAffine &screenMatrixInv,
                   double pixelSize, double alpha) const {
    drawEllipseRanges(TAngleRangeSet(true), ellipseMatrix, screenMatrixInv,
                      pixelSize, alpha);
  }

  void drawRuler(const TAffine &ellipseMatrix, double pixelSize) const {
    double minStep = 10.0 * pixelSize;
    double alpha   = getDrawingGridAlpha();

    double r =
        sqrt(0.5 * (norm2(ellipseMatrix.rowX()) + norm2(ellipseMatrix.rowX())));
    double actualMinStep     = minStep / r;
    TAffine ellipseMatrixInv = ellipseMatrix.inv();
    TPointD g0               = ellipseMatrixInv * m_grid0.position;
    TPointD g1               = ellipseMatrixInv * m_grid1.position;
    if (norm2(g0) <= TConsts::epsilon * TConsts::epsilon) return;
    if (norm2(g1) <= TConsts::epsilon * TConsts::epsilon) return;
    double ga0 = atan(g0);
    double ga1 = atan(g1);

    if (getPerspective()) {
      // draw perspective
      if (ga0 < 0.0) {
        if (ga1 > 0.0) ga1 -= M_2PI;
      } else {
        if (ga1 < 0.0) ga1 += M_2PI;
      }
      double k = 0.0, begin = 0.0, end = 0.0;
      if (!calcPerspectiveStep(actualMinStep, 0.0, M_2PI, 0.0, fabs(ga0), ga1,
                               k, begin, end))
        return;
      for (double x = begin; fabs(x) < fabs(end); x *= k)
        drawDot(ellipseMatrix *
                TPointD(cos(x), (ga0 < 0.0 ? -1.0 : 1.0) * sin(x)));
    } else {
      // draw linear
      double da = ga1 - ga0;
      if (da < 0.0) {
        da = -da;
        std::swap(ga0, ga1);
      }
      if (ga1 - ga0 > M_PI) {
        da = M_2PI - da;
        std::swap(ga0, ga1);
      }
      if (da < actualMinStep) return;
      for (double a = ga0 - floor(M_PI / da) * da; a < ga0 + M_PI; a += da)
        drawDot(ellipseMatrix * TPointD(cos(a), sin(a)));
    }
  }

  void drawConcentricGrid(const TAffine &ellipseMatrix,
                          const TAffine &screenMatrixInv,
                          double pixelSize) const {
    double minStep           = 20.0 * pixelSize;
    double alpha             = getDrawingGridAlpha();
    TAffine ellipseMatrixInv = ellipseMatrix.inv();

    // calculate bounds
    TAffine matrixInv = ellipseMatrixInv * screenMatrixInv;
    TPointD o         = matrixInv * TPointD(-1.0, -1.0);
    TPointD dx        = matrixInv.transformDirection(TPointD(2.0, 0.0));
    TPointD dy        = matrixInv.transformDirection(TPointD(0.0, 2.0));
    double max        = 0.0;
    double min        = std::numeric_limits<double>::infinity();

    // distance to points
    TPointD corners[] = {o, o + dx, o + dx + dy, o + dy};
    for (int i = 0; i < 4; ++i) {
      double k         = norm(corners[i]);
      if (k < min) min = k;
      if (k > max) max = k;
    }

    // distance to sides
    TPointD lines[] = {dx, dy, -1.0 * dx, -1.0 * dy};
    int positive = 0, negative = 0;
    for (int i = 0; i < 4; ++i) {
      double len2 = norm2(lines[i]);
      if (len2 <= TConsts::epsilon * TConsts::epsilon) continue;
      double k = (corners[i] * rotate90(lines[i])) / sqrt(len2);
      if (k > TConsts::epsilon) ++positive;
      if (k < TConsts::epsilon) ++negative;
      double l = -(corners[i] * lines[i]);
      if (l <= TConsts::epsilon || l >= len2 - TConsts::epsilon) continue;
      k                = fabs(k);
      if (k < min) min = k;
      if (k > max) max = k;
    }

    // if center is inside bounds
    if (min < 0.0 || positive == 0 || negative == 0) min = 0.0;
    if (max <= min) return;

    // draw
    double r =
        sqrt(0.5 * (norm2(ellipseMatrix.rowX()) + norm2(ellipseMatrix.rowX())));
    double actualMinStep = minStep / r;
    double gs0           = norm(ellipseMatrixInv * m_grid0.position);
    double gs1           = norm(ellipseMatrixInv * m_grid1.position);
    if (gs0 <= TConsts::epsilon * TConsts::epsilon) return;
    if (gs1 <= TConsts::epsilon * TConsts::epsilon) return;

    if (getPerspective()) {
      // draw perspective
      double k = 0.0, begin = 0.0, end = 0.0;
      if (!calcPerspectiveStep(actualMinStep, min, max, 0.0, gs0, gs1, k, begin,
                               end))
        return;
      for (double x = begin; fabs(x) < fabs(end); x *= k)
        drawEllipse(ellipseMatrix * TAffine::scale(x), screenMatrixInv,
                    pixelSize, alpha);
    } else {
      // draw linear
      double dx = fabs(gs1 - gs0);
      if (dx * r < minStep) return;
      for (double x = gs0 + ceil((min - gs0) / dx) * dx; x < max; x += dx)
        drawEllipse(ellipseMatrix * TAffine::scale(x), screenMatrixInv,
                    pixelSize, alpha);
    }
  }

  void drawParallelGrid(const TAffine &ellipseMatrix,
                        const TAffine &screenMatrixInv,
                        double pixelSize) const {
    double minStep           = 10.0 * pixelSize;
    double alpha             = getDrawingGridAlpha();
    TAffine ellipseMatrixInv = ellipseMatrix.inv();

    double r =
        sqrt(0.5 * (norm2(ellipseMatrix.rowX()) + norm2(ellipseMatrix.rowX())));
    double actualMinStep = minStep / r;
    TPointD g0           = ellipseMatrixInv * m_grid0.position;
    TPointD g1           = ellipseMatrixInv * m_grid1.position;
    if (getRepeat()) {
      g0.x -= round(0.5 * g0.x) * 2.0;
      g1.x -= round(0.5 * g1.x) * 2.0;
    }
    if (fabs(g0.x) >= 1.0 - TConsts::epsilon) return;
    if (fabs(g1.x) >= 1.0 - TConsts::epsilon) return;
    double gs0 = g0.y / sqrt(1.0 - g0.x * g0.x);
    double gs1 = g1.y / sqrt(1.0 - g1.x * g1.x);
    if (fabs(gs0) >= 1.0 - TConsts::epsilon) return;
    if (fabs(gs1) >= 1.0 - TConsts::epsilon) return;

    TAngleRangeSet ranges;
    ranges.add(TAngleRangeSet::fromDouble(0.0),
               TAngleRangeSet::fromDouble(M_PI));

    if (getPerspective()) {
      // draw perspective (actually angular)
      double k = 0.0, begin = 0.0, end = 0.0;
      double a0 = asin(gs0);
      double a1 = asin(gs1);
      double da = fabs(a1 - a0);
      if (fabs(sin(da)) < 2.0 * actualMinStep) return;
      for (double a = a0 + ceil((-M_PI_2 - a0) / da) * da; a < M_PI_2; a += da)
        drawEllipseRanges(
            ranges,
            ellipseMatrix * TAffine::scale(a < 0.0 ? -1.0 : 1.0, sin(a)),
            screenMatrixInv, pixelSize, alpha);
    } else {
      // draw linear
      double dx = fabs(gs1 - gs0);
      if (dx < actualMinStep) return;
      for (double x = gs0 + ceil((-1.0 - gs0) / dx) * dx; x < 1.0; x += dx)
        drawEllipseRanges(
            ranges, ellipseMatrix * TAffine::scale(x < 0.0 ? -1.0 : 1.0, x),
            screenMatrixInv, pixelSize, alpha);
    }
  }

  void draw(const TAffine &ellipseMatrix, const TAffine &screenMatrixInv,
            double ox, double pixelSize, bool enabled) const {
    const double crossSize = 0.1;

    double alpha     = getDrawingAlpha(enabled);
    bool grid        = getGrid();
    bool ruler       = getRestrictA() && getRestrictB();
    bool concentric  = !getRestrictA() && !getRestrictB();
    bool perspective = getPerspective();

    drawSegment(ellipseMatrix * TPointD(-crossSize, 0.0),
                ellipseMatrix * TPointD(crossSize, 0.0), pixelSize, alpha);
    drawSegment(ellipseMatrix * TPointD(0.0, -crossSize),
                ellipseMatrix * TPointD(0.0, crossSize), pixelSize, alpha);
    drawEllipse(ellipseMatrix, screenMatrixInv, pixelSize, alpha);
    if (ox > 1.0)
      drawSegment(ellipseMatrix * TPointD(-1.0, -1.0),
                  ellipseMatrix * TPointD(-1.0, 1.0), pixelSize, alpha);
    else if (ox < -1.0)
      drawSegment(ellipseMatrix * TPointD(1.0, -1.0),
                  ellipseMatrix * TPointD(1.0, 1.0), pixelSize, alpha);

    if (!grid) return;

    if (ruler) {
      drawRuler(ellipseMatrix, pixelSize);
    } else if (concentric) {
      drawConcentricGrid(ellipseMatrix, screenMatrixInv, pixelSize);
    } else {
      drawParallelGrid(ellipseMatrix, screenMatrixInv, pixelSize);
    }
  }

public:
  void draw(TToolViewer *viewer, bool enabled) const override {
    bool restrictA = getRestrictA();
    bool restrictB = getRestrictB();
    bool repeat    = getRepeat();
    double minStep = 30.0;

    TAffine ellipseMatrix = calcEllipseMatrix();
    if (ellipseMatrix.isZero()) return;
    if (!restrictA && restrictB)
      std::swap(ellipseMatrix.rowX(), ellipseMatrix.rowY());

    // common data about viewport
    const TRectD oneBox(-1.0, -1.0, 1.0, 1.0);
    TAffine4 modelview, projection;
    glGetDoublev(GL_MODELVIEW_MATRIX, modelview.a);
    glGetDoublev(GL_PROJECTION_MATRIX, projection.a);
    TAffine matrix    = (projection * modelview).get2d();
    TAffine matrixInv = matrix.inv();
    double pixelSize  = sqrt(tglGetPixelSize2());

    if (!repeat || restrictA == restrictB ||
        norm(ellipseMatrix.rowX()) < minStep * pixelSize) {
      draw(ellipseMatrix, matrixInv, 0.0, pixelSize, enabled);
    } else {
      // calculate bounds
      const TPointD &o   = ellipseMatrix.rowW();
      TPointD proj       = ellipseMatrix.rowX();
      proj               = proj * (1.0 / norm2(proj));
      TPointD corners[4] = {
          TPointD(oneBox.x0, oneBox.y0), TPointD(oneBox.x0, oneBox.y1),
          TPointD(oneBox.x1, oneBox.y0), TPointD(oneBox.x1, oneBox.y1)};
      double minX = 0.0, maxX = 0.0;
      for (int i = 0; i < 4; ++i) {
        double x                     = proj * (matrixInv * corners[i] - o);
        if (i == 0 || x < minX) minX = x;
        if (i == 0 || x > maxX) maxX = x;
      }
      if (maxX <= minX) return;

      // draw
      for (double ox = round(0.5 * minX) * 2.0; ox - 1.0 < maxX; ox += 2.0)
        draw(ellipseMatrix * TAffine::translation(ox, 0.0), matrixInv, ox,
             pixelSize, enabled);
    }
  }
};

//*****************************************************************************************
//    Registration
//*****************************************************************************************

static TAssistantTypeT<TAssistantEllipse> assistantEllipse("assistantEllipse");
