

#include "toonz/tcenterlinevectorizer.h"

// TnzCore includes
#include "tsystem.h"
#include "tstopwatch.h"
#include "tpalette.h"
#include "trastercm.h"
#include "ttoonzimage.h"
#include "tregion.h"
#include "tstroke.h"
#include "trasterimage.h"
#include "tmathutil.h"

// tcg includes
#include "tcg/tcg_numeric_ops.h"
#include "tcg/tcg_function_types.h"

// STD includes
#include <cmath>
#include <functional>

#undef DEBUG

//---------------------------------------------------------

struct ControlPoint {
  TStroke *m_stroke;
  int m_index;
  ControlPoint(TStroke *stroke, int index) : m_stroke(stroke), m_index(index) {}
  TPointD getPoint() const { return m_stroke->getControlPoint(m_index); }
  void setPoint(const TPointD &p) {
    TThickPoint point = m_stroke->getControlPoint(m_index);
    point.x           = p.x;
    point.y           = p.y;
    m_stroke->setControlPoint(m_index, point);
  }
};

//---------------------------------------------------------

class Node;

class DataPixel {
public:
  TPoint m_pos;
  int m_value;
  bool m_ink;
  Node *m_node;
  DataPixel() : m_value(0), m_ink(false), m_node(0) {}
};

//---------------------------------------------------------
#ifdef _WIN32
template class DV_EXPORT_API TSmartPointerT<TRasterT<DataPixel>>;
#endif
typedef TRasterPT<DataPixel> DataRasterP;
//---------------------------------------------------------

class Junction;

class Node {
public:
  Node *m_other;
  DataPixel *m_pixel;
  Node *m_prev, *m_next;
  Junction *m_junction;
#ifdef DEBUG
  bool m_flag;
#endif
  bool m_visited;
  Node()
      : m_pixel(0)
      , m_prev(0)
      , m_next(0)
      , m_junction(0)
      ,
#ifdef DEBUG
      m_flag(false)
      ,
#endif
      m_visited(false) {
  }
};

//---------------------------------------------------------

class ProtoStroke;

class Junction {
public:
  TThickPoint m_center;
  std::deque<Node *> m_nodes;
  int m_junctionOrder;
  std::vector<ProtoStroke *> m_protoStrokes;
  bool m_locked;
  Junction()
      : m_center()
      , m_nodes()
      , m_junctionOrder(0)
      , m_protoStrokes()
      , m_locked(false) {}
  bool isConvex();
};

//---------------------------------------------------------

class ProtoStroke {
public:
  TPointD m_startDirection, m_endDirection;
  Junction *m_startJunction, *m_endJunction;
  std::deque<TThickPoint> m_points;
  ProtoStroke()
      : m_points()
      , m_startDirection()
      , m_endDirection()
      , m_startJunction(0)
      , m_endJunction(0) {}
  ProtoStroke(std::deque<TThickPoint>::iterator it_b,
              std::deque<TThickPoint>::iterator it_e)
      : m_points(it_b, it_e)
      , m_startDirection()
      , m_endDirection()
      , m_startJunction(0)
      , m_endJunction(0) {}
};

//---------------------------------------------------------

static double computeDistance2(Node *na, Node *nb) {
  assert(na->m_pixel);
  assert(nb->m_pixel);
  TPointD d = convert(na->m_pixel->m_pos - nb->m_pixel->m_pos);
  return d * d;
}

//---------------------------------------------------------

static void renormalizeImage(TVectorImage *vi) {
  int i, j;
  int n = vi->getStrokeCount();
  std::vector<ControlPoint> points;
  points.reserve(n * 2);
  for (i = 0; i < n; i++) {
    TStroke *stroke = vi->getStroke(i);
    int m           = stroke->getControlPointCount();
    if (m > 0) {
      if (m == 1)
        points.push_back(ControlPoint(stroke, 0));
      else {
        points.push_back(ControlPoint(stroke, 0));
        points.push_back(ControlPoint(stroke, m - 1));
      }
    }
  }
  int count = points.size();
  for (i = 0; i < count; i++) {
    ControlPoint &pi = points[i];
    TPointD posi     = pi.getPoint();
    TPointD center   = posi;
    std::vector<int> neighbours;
    neighbours.push_back(i);
    for (j = i + 1; j < count; j++) {
      TPointD posj = points[j].getPoint();
      double d     = tdistance(posj, posi);
      if (d < 0.01) {
        neighbours.push_back(j);
        center += posj;
      }
    }
    int m = neighbours.size();
    if (m == 1) continue;
    center = center * (1.0 / m);
    for (j = 0; j < m; j++) points[neighbours[j]].setPoint(center);
  }
}

//---------------------------------------------------------

class OutlineVectorizer {
  TPalette *m_palette;

public:
  TRasterP m_src;

  OutlineConfiguration m_configuration;
  DataRasterP m_dataRaster;
  std::vector<std::pair<int, DataRasterP>> m_dataRasterArray;
  TVectorImageP m_vimage;
  std::vector<Node *> m_nodes;
  std::list<std::vector<TThickPoint>> m_protoOutlines;

  std::vector<Junction *> m_junctions;

  OutlineVectorizer(const OutlineConfiguration &configuration,
                    TPalette *palette)
      : m_configuration(configuration), m_palette(palette) {}

  ~OutlineVectorizer();

  void traceOutline(Node *initialNode);
  void createOutlineStrokes();
  void makeDataRaster(const TRasterP &src);

  Node *findOtherSide(Node *node);

  void clearNodes();
  Node *createNode(DataPixel *pix);

  void clearJunctions();

  void init();

  void link(DataPixel *pix, DataPixel *from, DataPixel *to);

  TPoint computeGradient(DataPixel *pix) {
    assert(m_dataRaster);
    const int wrap = m_dataRaster->getWrap();

    TPoint g(0, 0);
    int n, s, w, e, nw, sw, ne, se;

    w  = pix[-1].m_value;
    nw = pix[-1 + wrap].m_value;
    sw = pix[-1 - wrap].m_value;

    e  = pix[+1].m_value;
    ne = pix[+1 + wrap].m_value;
    se = pix[+1 - wrap].m_value;

    n = pix[+wrap].m_value;
    s = pix[-wrap].m_value;

    g.y = -sw + ne - se + nw + 2 * (n - s);
    g.x = -sw + ne + se - nw + 2 * (e - w);
    return g;
  }

private:
  // not implemented
  OutlineVectorizer(const OutlineVectorizer &);
  OutlineVectorizer &operator=(const OutlineVectorizer &);
};

//---------------------------------------------------------

OutlineVectorizer::~OutlineVectorizer() {
  m_protoOutlines.clear();
  clearNodes();
  clearJunctions();
}

//---------------------------------------------------------

void OutlineVectorizer::init() {
  int y;

  DataRasterP dataRaster = m_dataRaster;
  const int wrap         = dataRaster->getWrap();
  const int delta[]      = {-wrap - 1, -wrap, -wrap + 1, 1,
                       wrap + 1,  wrap,  wrap - 1,  -1};

  for (y = 1; y < dataRaster->getLy() - 1; y++) {
    DataPixel *pix    = dataRaster->pixels(y);
    DataPixel *endPix = pix + dataRaster->getLx() - 1;
    pix++;
    for (pix++; pix < endPix; ++pix) {
      if ((pix->m_ink == false) || (pix[-wrap].m_ink && pix[wrap].m_ink &&
                                    pix[-1].m_ink && pix[1].m_ink))
        continue;
      int i;
      for (i = 0; i < 8; i++)
        if (pix[delta[i]].m_ink && pix[delta[(i + 1) & 0x7]].m_ink == false)
          break;
      int start = i;
      if (i == 8) continue;  // punto isolato
      for (;;) {
        int j = (i + 1) & 0x7;
        assert(i < 8 && pix[delta[i]].m_ink);
        assert(j < 8 && pix[delta[j]].m_ink == false);
        do
          j = (j + 1) & 0x7;
        while (pix[delta[j]].m_ink == false);
        assert(j < 8 && pix[delta[j]].m_ink);
        if (((i + 2) & 0x7) != j || (i & 1) == 0) {
          // il bianco comprende anche un fianco
          link(pix, pix + delta[i], pix + delta[j]);
        }
        i = j;
        assert(i < 8);
        while (pix[delta[(i + 1) & 0x7]].m_ink) i = (i + 1) & 0x7;
        assert(i < 8 && pix[delta[i]].m_ink);
        assert(pix[delta[(i + 1) & 0x7]].m_ink == false);
        if (i == start) break;
      }
    }
  }
}

//---------------------------------------------------------

Node *OutlineVectorizer::createNode(DataPixel *pix) {
  Node *node    = new Node();
  node->m_pixel = pix;
  node->m_other = pix->m_node;
  pix->m_node   = node;
  m_nodes.push_back(node);
  return node;
}

//---------------------------------------------------------

void OutlineVectorizer::clearNodes() {
  int i;
  for (i = 0; i < (int)m_nodes.size(); i++) delete m_nodes[i];
  m_nodes.clear();
}

//---------------------------------------------------------

void OutlineVectorizer::clearJunctions() {
  int i;
  for (i = 0; i < (int)m_junctions.size(); i++) delete m_junctions[i];
  m_junctions.clear();
}

//---------------------------------------------------------

void OutlineVectorizer::link(DataPixel *pix, DataPixel *srcPix,
                             DataPixel *dstPix) {
  Node *srcNode = 0, *dstNode = 0, *node = 0;
  Node *tmp;
  for (tmp = pix->m_node; tmp; tmp = tmp->m_other) {
    if (tmp->m_pixel == 0) continue;
    if (tmp->m_prev && tmp->m_prev->m_pixel == srcPix) {
      assert(srcNode == 0);
      if (node) {
        assert(node->m_next->m_pixel == dstPix);
        assert(node->m_prev == 0);
        node->m_prev        = tmp->m_prev;
        tmp->m_prev->m_next = node;
        tmp->m_next = tmp->m_prev = 0;
        tmp->m_pixel              = 0;
        return;
      }
      assert(tmp->m_next == 0);
      srcNode = tmp->m_prev;
      node    = tmp;
    }
    if (tmp->m_next && tmp->m_next->m_pixel == dstPix) {
      assert(dstNode == 0);
      if (node) {
        assert(node->m_prev->m_pixel == srcPix);
        assert(node->m_next == 0);
        node->m_next        = tmp->m_next;
        tmp->m_next->m_prev = node;
        tmp->m_next = tmp->m_prev = 0;
        tmp->m_pixel              = 0;
        return;
      }
      assert(tmp->m_prev == 0);
      dstNode = tmp->m_next;
      node    = tmp;
    }
  }
  if (!node) node       = createNode(pix);
  if (!srcNode) srcNode = createNode(srcPix);
  if (!dstNode) dstNode = createNode(dstPix);

  if (!node->m_next) {
    node->m_next = dstNode;
    assert(dstNode->m_prev == 0);
    dstNode->m_prev = node;
  }
  if (!node->m_prev) {
    node->m_prev = srcNode;
    assert(srcNode->m_next == 0);
    srcNode->m_next = node;
  }

  assert(node->m_next == dstNode);
  assert(node->m_prev == srcNode);
  assert(dstNode->m_prev == node);
  assert(srcNode->m_next == node);
}

//---------------------------------------------------------

void OutlineVectorizer::traceOutline(Node *initialNode) {
  Node *startNode = initialNode;
  Node *node;
  do {
    if (!startNode) break;
    node = findOtherSide(startNode);
    if (!node) break;

    double startDist2 = computeDistance2(startNode, node);
    if (startDist2 > 0.1) break;

    startNode = startNode->m_next;
  } while (startNode != initialNode);

  if (!startNode) return;
  node = startNode;
  std::vector<TThickPoint> points;
  do {
    node = node->m_next;
    if (!node) break;
    node->m_visited = true;
    points.push_back(TThickPoint(convert(node->m_pixel->m_pos), 0));
  } while (node != startNode);
  m_protoOutlines.push_back(points);
}

//---------------------------------------------------------

Node *OutlineVectorizer::findOtherSide(Node *node) {
  DataPixel *pix = node->m_pixel;

  TPoint dir = -computeGradient(pix);
  if (dir == TPoint(0, 0)) return 0;
  TPoint d1(tsign(dir.x), 0), d2(0, tsign(dir.y));
  int num = abs(dir.y), den = abs(dir.x);
  if (num > den) {
    std::swap(d1, d2);
    std::swap(num, den);
  }
  TPoint pos = pix->m_pos;
  int i;
  for (i = 0;; i++) {
    TPoint q(pos.x + d1.x * i + d2.x * num * i / den,
             pos.y + d1.y * i + d2.y * num * i / den);
    DataPixel *nextPix = m_dataRaster->pixels(q.y) + q.x;
    if (nextPix->m_ink == false) break;
    pix = nextPix;
  }
  assert(pix);
  if (!pix->m_node) {
    const int wrap = m_dataRaster->getWrap();
    if (pix[-1].m_node)
      pix--;
    else if (pix[1].m_node)
      pix++;
    else if (pix[wrap].m_node)
      pix += wrap;
    else if (pix[-wrap].m_node)
      pix -= wrap;
    else {
      assert(0);
    }
  }
  if (!pix->m_node) return 0;
  Node *q                                 = pix->m_node;
  while (q->m_pixel == 0 && q->m_other) q = q->m_other;
  assert(q && q->m_pixel == pix);

  for (i = 0; i < 5; i++) {
    if (!q->m_prev) break;
    q = q->m_prev;
  }

  Node *best       = q;
  double bestDist2 = computeDistance2(q, node);
  for (i = 0; i < 10; i++) {
    q = q->m_next;
    if (!q) break;
    double dist2 = computeDistance2(q, node);
    if (dist2 < bestDist2) {
      bestDist2 = dist2;
      best      = q;
    }
  }

  return best;
}

//---------------------------------------------------------

void OutlineVectorizer::createOutlineStrokes() {
  m_vimage->enableRegionComputing(true, false);
  int j;

  for (j = 0; j < (int)m_nodes.size(); j++) {
    Node *node = m_nodes[j];
    if (node->m_pixel == 0 || node->m_visited) continue;
    traceOutline(node);
  }

#ifdef DEBUG
  for (j = 0; j < (int)m_nodes.size(); j++) {
    Node *node = m_nodes[j];
    if (node->m_pixel == 0 || node->m_flag) continue;
    outputNodes(node);
  }
#endif

  std::list<std::vector<TThickPoint>>::iterator it_outlines =
      m_protoOutlines.begin();
  for (it_outlines; it_outlines != m_protoOutlines.end(); it_outlines++) {
    if (it_outlines->size() > 3) {
      std::vector<TThickPoint> points;
      std::vector<TThickPoint>::iterator it;

      if (it_outlines->size() > 10) {
        it = it_outlines->begin() + 1;
        for (;;) {
          // Baco: Ricontrolla l'if seguente - in alcuni casi va fuori bounds...
          if ((int)it_outlines->size() <= m_configuration.m_smoothness + 1)
            break;
          if (it >= it_outlines->end() - (m_configuration.m_smoothness + 1))
            break;
          for (j = 0; j < m_configuration.m_smoothness; j++)
            it = it_outlines->erase(it);
          ++it;
        }
      }

      points.push_back(it_outlines->front());
      it              = it_outlines->begin();
      TThickPoint old = *it;
      ++it;
      for (; it != it_outlines->end(); ++it) {
        TThickPoint point((1 / 2.0) * (*it + old));
        points.push_back(point);
        old = *it;
      }

      points.push_back(it_outlines->back());
      points.push_back(it_outlines->front());

      TStroke *stroke =
          TStroke::interpolate(points, m_configuration.m_interpolationError);
      stroke->setStyle(m_configuration.m_strokeStyleId);
      stroke->setSelfLoop();
      m_vimage->addStroke(stroke);
    }
  }
}

//---------------------------------------------------------

inline int colorDistance2(const TPixel32 &c0, const TPixel32 &c1) {
  return ((c0.r - c1.r) * (c0.r - c1.r) + (c0.g - c1.g) * (c0.g - c1.g) +
          (c0.b - c1.b) * (c0.b - c1.b));
}

//---------------------------------------------------------
#define MAX_TOLERANCE 20

#include "tcolorstyles.h"

void OutlineVectorizer::makeDataRaster(const TRasterP &src) {
  m_vimage = new TVectorImage();
  if (!src) return;
  m_src = src;

  clearNodes();
  clearJunctions();

  int x, y, ii = 0;
  TRaster32P srcRGBM = (TRaster32P)m_src;
  TRasterCM32P srcCM = (TRasterCM32P)m_src;
  TRasterGR8P srcGR  = (TRasterGR8P)m_src;

  // Inizializzo DataRasterP per i casi in cui si ha un TRaster32P, un
  // TRasterGR8P o un TRasterCM32P molto grande
  DataRasterP dataRaster(m_src->getSize().lx + 2, m_src->getSize().ly + 2);
  if (srcRGBM || srcGR ||
      (srcCM && srcCM->getLx() * srcCM->getLy() > 5000000)) {
    int ly              = dataRaster->getLy();
    int lx              = dataRaster->getLx();
    int wrap            = dataRaster->getWrap();
    DataPixel *dataPix0 = dataRaster->pixels(0);
    DataPixel *dataPix1 = dataRaster->pixels(0) + m_src->getLx() + 1;
    for (y = 0; y < ly; y++, dataPix0 += wrap, dataPix1 += wrap) {
      dataPix0->m_pos.x = 0;
      dataPix1->m_pos.x = lx - 1;
      dataPix0->m_pos.y = dataPix1->m_pos.y = y;
      dataPix0->m_value = dataPix1->m_value = 0;
      dataPix0->m_ink = dataPix1->m_ink = false;
      dataPix0->m_node = dataPix1->m_node = 0;
    }
    dataPix0 = dataRaster->pixels(0);
    dataPix1 = dataRaster->pixels(ly - 1);
    for (x = 0; x < lx; x++, dataPix0++, dataPix1++) {
      dataPix0->m_pos.x = dataPix1->m_pos.x = x;
      dataPix0->m_pos.y                     = 0;
      dataPix1->m_pos.y                     = ly - 1;
      dataPix0->m_value = dataPix1->m_value = 0;
      dataPix0->m_ink = dataPix1->m_ink = false;
      dataPix0->m_node = dataPix1->m_node = 0;
    }
  }

  if (srcRGBM) {
    assert(m_palette);
    int inkId = m_palette->getClosestStyle(m_configuration.m_inkColor);
    if (!inkId ||
        m_configuration.m_inkColor !=
            m_palette->getStyle(inkId)->getMainColor()) {
      inkId = m_palette->getStyleCount();
      m_palette->getStylePage(1)->insertStyle(1, m_configuration.m_inkColor);
      m_palette->setStyle(inkId, m_configuration.m_inkColor);
    }
    assert(inkId);

    m_dataRasterArray.push_back(std::pair<int, DataRasterP>(inkId, dataRaster));
    int maxDistance2 =
        m_configuration.m_threshold * m_configuration.m_threshold;

    for (y = 0; y < m_src->getLy(); y++) {
      TPixel32 *inPix    = srcRGBM->pixels(y);
      TPixel32 *inEndPix = inPix + srcRGBM->getLx();
      DataPixel *dataPix = dataRaster->pixels(y + 1) + 1;
      x                  = 0;
      while (inPix < inEndPix) {
        *dataPix      = DataPixel();
        int distance2 = colorDistance2(m_configuration.m_inkColor, *inPix);

        if (y == 0 || y == m_src->getLy() - 1 || x == 0 ||
            x == m_src->getLx() - 1 || inPix->m == 0) {
          dataPix->m_value = 255;
          dataPix->m_ink   = false;
        } else {
          dataPix->m_value = (inPix->r + 2 * inPix->g + inPix->b) >> 2;
          dataPix->m_ink   = (distance2 < maxDistance2);
        }
        dataPix->m_pos.x = x++;
        dataPix->m_pos.y = y;
        dataPix->m_node  = 0;
        inPix++;
        dataPix++;
      }
    }
  } else if (srcGR) {
    assert(m_palette);
    int inkId = m_palette->getClosestStyle(m_configuration.m_inkColor);
    if (!inkId ||
        m_configuration.m_inkColor !=
            m_palette->getStyle(inkId)->getMainColor()) {
      inkId = m_palette->getStyleCount();
      m_palette->getStylePage(1)->insertStyle(1, m_configuration.m_inkColor);
      m_palette->setStyle(inkId, m_configuration.m_inkColor);
    }
    assert(inkId);

    m_dataRasterArray.push_back(std::pair<int, DataRasterP>(inkId, dataRaster));
    int threshold = m_configuration.m_threshold;

    for (y = 0; y < m_src->getLy(); y++) {
      TPixelGR8 *inPix    = srcGR->pixels(y);
      TPixelGR8 *inEndPix = inPix + srcGR->getLx();
      DataPixel *dataPix  = dataRaster->pixels(y + 1) + 1;
      x                   = 0;
      while (inPix < inEndPix) {
        *dataPix = DataPixel();
        if (y == 0 || y == m_src->getLy() - 1 || x == 0 ||
            x == m_src->getLx() - 1) {
          dataPix->m_value = 255;
          dataPix->m_ink   = false;
        } else {
          dataPix->m_value = inPix->value;
          dataPix->m_ink   = (inPix->value < threshold);
        }
        dataPix->m_pos.x = x++;
        dataPix->m_pos.y = y;
        dataPix->m_node  = 0;
        inPix++;
        dataPix++;
      }
    }
  }

  else if (srcCM) {
    int currInk, nextInk = 0;

    if (srcCM->getLx() * srcCM->getLy() > 5000000) {
      int threshold = m_configuration.m_threshold;
      int inkId     = m_palette->getClosestStyle(TPixel::Black);

      if (TPixel::Black != m_palette->getStyle(inkId)->getMainColor()) {
        inkId = m_palette->getStyleCount();
        m_palette->getStylePage(1)->insertStyle(1, m_configuration.m_inkColor);
        m_palette->setStyle(inkId, m_configuration.m_inkColor);
      }
      assert(inkId);

      m_dataRasterArray.push_back(
          std::pair<int, DataRasterP>(inkId, dataRaster));

      // inizializza la parte centrale
      for (y = 0; y < m_src->getLy(); y++) {
        TPixelCM32 *inPix    = srcCM->pixels(y);
        TPixelCM32 *inEndPix = inPix + m_src->getLx();
        DataPixel *dataPix   = dataRaster->pixels(y + 1) + 1;
        x                    = 0;
        while (inPix < inEndPix) {
          *dataPix                                     = DataPixel();
          int value                                    = inPix->getTone();
          if (m_configuration.m_ignoreInkColors) inkId = 1;
          if (y == 0 || y == m_src->getLy() - 1 || x == 0 ||
              x == m_src->getLx() - 1) {
            dataPix->m_value = 255;
            dataPix->m_ink   = false;
          } else {
            dataPix->m_value = value;
            dataPix->m_ink   = (value < threshold);
          }
          dataPix->m_pos.x = x++;
          dataPix->m_pos.y = y;
          dataPix->m_node  = 0;
          inPix++;
          dataPix++;
        }
      }
    } else {
      do {
        // Inizializzo DataRasterP
        DataRasterP dataRaster(m_src->getSize().lx + 2,
                               m_src->getSize().ly + 2);
        int ly              = dataRaster->getLy();
        int lx              = dataRaster->getLx();
        int wrap            = dataRaster->getWrap();
        DataPixel *dataPix0 = dataRaster->pixels(0);
        DataPixel *dataPix1 = dataRaster->pixels(0) + m_src->getLx() + 1;
        for (y = 0; y < ly; y++, dataPix0 += wrap, dataPix1 += wrap) {
          dataPix0->m_pos.x = 0;
          dataPix1->m_pos.x = lx - 1;
          dataPix0->m_pos.y = dataPix1->m_pos.y = y;
          dataPix0->m_value = dataPix1->m_value = 0;
          dataPix0->m_ink = dataPix1->m_ink = false;
          dataPix0->m_node = dataPix1->m_node = 0;
        }
        dataPix0 = dataRaster->pixels(0);
        dataPix1 = dataRaster->pixels(ly - 1);
        for (x = 0; x < lx; x++, dataPix0++, dataPix1++) {
          dataPix0->m_pos.x = dataPix1->m_pos.x = x;
          dataPix0->m_pos.y                     = 0;
          dataPix1->m_pos.y                     = ly - 1;
          dataPix0->m_value = dataPix1->m_value = 0;
          dataPix0->m_ink = dataPix1->m_ink = false;
          dataPix0->m_node = dataPix1->m_node = 0;
        }

        int threshold =
            m_configuration.m_threshold;  // tolerance: 1->MAX thresh: 1-255
        currInk = nextInk;
        nextInk = 0;
        m_dataRasterArray.push_back(
            std::pair<int, DataRasterP>(currInk, dataRaster));

        // inizializza la parte centrale
        for (y = 0; y < m_src->getLy(); y++) {
          TPixelCM32 *inPix    = srcCM->pixels(y);
          TPixelCM32 *inEndPix = inPix + m_src->getLx();
          DataPixel *dataPix   = dataRaster->pixels(y + 1) + 1;
          x                    = 0;
          while (inPix < inEndPix) {
            *dataPix  = DataPixel();
            int value = inPix->getTone();
            if (value < 255 && !m_configuration.m_ignoreInkColors) {
              int ink = inPix->getInk();
              if (currInk == 0) {
                currInk                        = ink;
                m_dataRasterArray.back().first = ink;
              } else if (ink != currInk) {
                value = 255;
                if (nextInk == 0) {
                  for (ii = 0; ii < (int)m_dataRasterArray.size() - 1; ii++)
                    if (m_dataRasterArray[ii].first == ink) break;
                  if (ii == (int)m_dataRasterArray.size() - 1) nextInk = ink;
                }
              }
            }
            dataPix->m_pos.x = x++;
            dataPix->m_pos.y = y;
            dataPix->m_value = value;
            dataPix->m_ink   = (value < threshold);
            dataPix->m_node  = 0;
            inPix++;
            dataPix++;
          }
        }
      } while (nextInk != 0);
    }
    if (m_configuration.m_ignoreInkColors) {
      assert(m_dataRasterArray.size() == 1);
      m_dataRasterArray.back().first = 1;
    }
  } else
    assert(false);
}

//---------------------------------------------------------

TVectorImageP VectorizerCore::outlineVectorize(
    const TImageP &image, const OutlineConfiguration &configuration,
    TPalette *palette) {
  TVectorImageP out;

  OutlineVectorizer vectorizer(configuration, palette);

  TRasterImageP ri = image;
  TToonzImageP vi  = image;
  if (ri)
    vectorizer.makeDataRaster(ri->getRaster());
  else
    vectorizer.makeDataRaster(vi->getRaster());
  int layersCount = vectorizer.m_dataRasterArray.size();
  if (layersCount > 1) {
    out = new TVectorImage();
    out->setPalette(palette);
  }
  int i;
  for (i = 0; i < (int)layersCount; i++) {
    vectorizer.m_dataRaster = vectorizer.m_dataRasterArray[i].second;
    vectorizer.m_configuration.m_strokeStyleId =
        vectorizer.m_dataRasterArray[i].first;
    vectorizer.m_protoOutlines.clear();
    vectorizer.init();
    vectorizer.createOutlineStrokes();
    renormalizeImage(vectorizer.m_vimage.getPointer());
    vectorizer.m_vimage->setPalette(palette);
    if (layersCount > 1) out->mergeImage(vectorizer.m_vimage, TAffine());

    if (i != (int)layersCount - 1) vectorizer.m_vimage = new TVectorImage();
  }

  return (layersCount == 1) ? vectorizer.m_vimage : out;
}

//=========================================================

static bool isPointInRegion(TPointD p, TRegion *r) {
  int i;
  for (i = 0; i < 5; i++) {
    double stepX = i * 0.2;
    int j;
    for (j = 0; j < 5; j++) {
      double stepY = j * 0.2;
      if (r->contains(TPointD(p.x + stepX, p.y + stepY))) return true;
    }
  }
  return false;
}

//------------------------------------------------------

// Se findInk == true :
//    trova il punto piu' vicino a p con ink puro e restituisce true se e'
//    contenuto nella regione
// Se findInk == false :
//    Trova il punto piu' vicino a p con paint puro e restituisce true se e'
//    contenuto nella regione

//(Daniele) Aggiunti controlli per evitare uscite dai bounds

static bool isNearestInkOrPaintInRegion(bool findInk, const TRasterCM32P &ras,
                                        TRegion *r, const TAffine &aff,
                                        const TPoint &p) {
  bool isTheLastSquare = false;
  int mx, my, Mx, My;
  int i;

  for (i = 1; i <= 100; i++) {
    int j, t, s, e;
    if (p.x - i >= 0) {
      my = std::max(p.y - i, 0);
      My = std::min(p.y + i, ras->getLy() - 1);
      for (j = my; j <= My; j++) {
        TPixelCM32 col = ras->pixels(j)[p.x - i];
        int tone       = col.getTone();
        if ((findInk && tone == 0) || (!findInk && tone == 255)) {
          if (isPointInRegion(aff * TPointD(double(p.x - i), double(j)), r))
            return true;
          else
            isTheLastSquare = true;
        }
      }
    }
    if (p.y + i < ras->getLy()) {
      mx = std::max(p.x - i + 1, 0);
      Mx = std::min(p.x + i, ras->getLx() - 1);
      for (t = mx; t <= Mx; t++) {
        TPixelCM32 col = ras->pixels(p.y + i)[t];
        int tone       = col.getTone();
        if ((findInk && tone == 0) || (!findInk && tone == 255)) {
          if (isPointInRegion(aff * TPointD(double(t), double(p.y + i)), r))
            return true;
          else
            isTheLastSquare = true;
        }
      }
    }
    if (p.x + i < ras->getLx()) {
      my = std::max(p.y - i, 0);
      My = std::min(p.y + i - 1, ras->getLy() - 1);
      for (s = my; s <= My; s++) {
        TPixelCM32 col = ras->pixels(s)[p.x + i];
        int tone       = col.getTone();
        if ((findInk && tone == 0) || (!findInk && tone == 255)) {
          if (isPointInRegion(aff * TPointD(double(p.x + i), double(s)), r))
            return true;
          else
            isTheLastSquare = true;
        }
      }
    }
    if (p.y - i >= 0) {
      mx = std::max(p.x - i + 1, 0);
      Mx = std::min(p.x + i - 1, ras->getLx() - 1);
      for (e = mx; e <= Mx; e++) {
        TPixelCM32 col = ras->pixels(p.y - i)[e];
        int tone       = col.getTone();
        if ((findInk && tone == 0) || (!findInk && tone == 255)) {
          if (isPointInRegion(aff * TPointD(double(e), double(p.y - i)), r))
            return true;
          else
            isTheLastSquare = true;
        }
      }
    }

    if (isTheLastSquare) return false;
  }

  return false;
}

//======================================================

inline bool isBright(const TPixelCM32 &pix, int threshold) {
  return pix.getTone() >= threshold;
}

inline bool isBright(const TPixelGR8 &pix, int threshold) {
  return pix.value >= threshold;
}

inline bool isBright(const TPixel32 &pix, int threshold) {
  // Using Value in HSV color model
  return std::max(pix.r, std::max(pix.g, pix.b)) >= threshold * (pix.m / 255.0);

  // Using Lightness in HSL color model
  // return (max(pix.r,max(pix.g,pix.b)) + min(pix.r,min(pix.g,pix.b))) / 2.0
  //  >= threshold * (pix.m / 255.0);

  // Using (relative) Luminance
  // return 0.2126 * pix.r + 0.7152 * pix.g + 0.0722 * pix.b >= threshold *
  // (pix.m / 255.0);
}

//------------------------------------------------------

inline bool isDark(const TPixelCM32 &pix, int threshold) {
  return !isBright(pix, threshold);
}

inline bool isDark(const TPixelGR8 &pix, int threshold) {
  return !isBright(pix, threshold);
}

inline bool isDark(const TPixelRGBM32 &pix, int threshold) {
  return !isBright(pix, threshold);
}

//------------------------------------------------------

template <typename Pix, typename Selector>
bool getInternalPoint(const TRasterPT<Pix> &ras, const Selector &sel,
                      const TAffine &inverse, const VectorizerConfiguration &c,
                      const TRegion *region, TPointD &p) {
  struct Locals {
    const TRasterPT<Pix> &m_ras;
    const Selector &m_sel;
    const TAffine &m_inverse;
    double m_pixelSize;
    const TRegion &m_region;

    static bool contains(const TRegion &region, const TPointD &p) {
      return region.getBBox().contains(p) &&
             (region.leftScanlineIntersections(p.x, p.y) % 2);
    }

    bool contains(const TPointD &p) {
      if (!contains(m_region, p)) return false;

      UINT sr, srCount = m_region.getSubregionCount();
      for (sr = 0; sr != srCount; ++sr) {
        if (contains(*m_region.getSubregion(sr), p)) return false;
      }

      return true;
    }

    // Subdivide the output scanline in even intervals, and sample each's
    // midpoint
    bool sampleMidpoints(TPointD &p, double x0, double x1, double y,
                         int intervalsCount) {
      const double iCountD = intervalsCount;

      for (int i = 0; i != intervalsCount; ++i) {
        double i_x0 = tcg::numeric_ops::lerp(x0, x1, i / iCountD),
               i_x1 = tcg::numeric_ops::lerp(x0, x1, (i + 1) / iCountD);

        if (sample(p = TPointD(0.5 * (i_x0 + i_x1), y))) return true;
      }

      return false;
    }

    // Sample the output scanline's midpoint
    bool sample(TPointD &point) {
      return (contains(point) &&
              adjustPoint(point)  // Ensures that point is inRaster()
              && selected(point));
    }

    TPoint toRaster(const TPointD &p) {
      const TPointD &pRasD = m_inverse * p;
      return TPoint(pRasD.x, pRasD.y);
    }

    bool inRaster(const TPointD &point) {
      const TPoint &pRas = toRaster(point);
      return (pRas.x >= 0 && pRas.x < m_ras->getLx() && pRas.y >= 0 &&
              pRas.y < m_ras->getLy());
    }

    bool selected(const TPointD &point) {
      assert(inRaster(point));

      const TPoint &pRas = toRaster(point);
      return m_sel(m_ras->pixels(pRas.y)[pRas.x]);
    }

    bool adjustPoint(TPointD &p) {
      const TRectD &bbox = m_region.getBBox();
      const double tol   = std::max(1e-1 * m_pixelSize, 1e-4);

      TPointD newP = p;
      {
        // Adjust along x axis
        int iCount = scanlineIntersectionsBefore(newP.x, newP.y, true);

        double in0 = newP.x, out0 = bbox.x0, in1 = newP.x, out1 = bbox.x1;

        isolateBorderX(in0, out0, newP.y, iCount, tol);
        isolateBorderX(in1, out1, newP.y, iCount, tol);

        newP = TPointD(0.5 * (in0 + in1), newP.y);
        assert(scanlineIntersectionsBefore(newP.x, newP.y, true) == iCount);
      }
      {
        // Adjust along y axis
        int iCount = scanlineIntersectionsBefore(newP.x, newP.y, false);

        double in0 = newP.y, out0 = bbox.y0, in1 = newP.y, out1 = bbox.y1;

        isolateBorderY(newP.x, in0, out0, iCount, tol);
        isolateBorderY(newP.x, in1, out1, iCount, tol);

        newP = TPointD(newP.x, 0.5 * (in0 + in1));
        assert(scanlineIntersectionsBefore(newP.x, newP.y, false) == iCount);
      }

      return inRaster(newP) ? (p = newP, true) : false;
    }

    void isolateBorderX(double &xIn, double &xOut, double y, int iCount,
                        const double tol) {
      assert(scanlineIntersectionsBefore(xIn, y, true) == iCount);

      while (true) {
        // Subdivide current interval
        double mid = 0.5 * (xIn + xOut);

        if (scanlineIntersectionsBefore(mid, y, true) == iCount)
          xIn = mid;
        else
          xOut = mid;

        if (std::abs(xOut - xIn) < tol) break;
      }
    }

    void isolateBorderY(double x, double &yIn, double &yOut, int iCount,
                        const double tol) {
      assert(scanlineIntersectionsBefore(x, yIn, false) == iCount);

      while (true) {
        // Subdivide current interval
        double mid = 0.5 * (yIn + yOut);

        if (scanlineIntersectionsBefore(x, mid, false) == iCount)
          yIn = mid;
        else
          yOut = mid;

        if (std::abs(yOut - yIn) < tol) break;
      }
    }

    int scanlineIntersectionsBefore(double x, double y, bool hor) {
      int result = m_region.scanlineIntersectionsBefore(x, y, hor);

      UINT sr, srCount = m_region.getSubregionCount();
      for (sr = 0; sr != srCount; ++sr)
        result +=
            m_region.getSubregion(sr)->scanlineIntersectionsBefore(x, y, hor);

      return result;
    }

  } locals = {ras, sel, inverse, c.m_thickScale, *region};

  assert(region);

  const TRectD &regionBBox = region->getBBox();
  double regionMidY        = 0.5 * (regionBBox.y0 + regionBBox.y1);

  int ic, icEnd = tceil((regionBBox.x1 - regionBBox.x0) / c.m_thickScale) +
                  1;  // Say you have 4 pixels, in [0, 4]. We want to
                      // have at least 4 intervals where midpoints are
                      // taken - so end intervals count is 5.
  for (ic = 1; ic < icEnd; ic *= 2) {
    if (locals.sampleMidpoints(p, regionBBox.x0, regionBBox.x1, regionMidY, ic))
      return true;
  }

  return false;
}

//=========================================================

//(Daniele)

// Taking lone, unchecked points is dangerous - they could lie inside
// region r and still have a wrong color (for example, if they lie
//*on* a boundary stroke).
// Plus, over-threshold regions should always be considered black.

// In order to improve this, we search a 4way-local-brightest
// neighbour of p. Observe that, however, it may still lie outside r;
// would that happen, p was not significative in the first place.
//---------------------------------------------------------------

inline TPixel32 takeLocalBrightest(const TRaster32P rr, TRegion *r,
                                   const VectorizerConfiguration &c,
                                   TPoint &p) {
  TPoint pMax;

  while (r->contains(c.m_affine * convert(p))) {
    pMax = p;
    if (p.x > 0 && rr->pixels(p.y)[p.x - 1] > rr->pixels(pMax.y)[pMax.x])
      pMax = TPoint(p.x - 1, p.y);
    if (p.x < rr->getLx() - 1 &&
        rr->pixels(p.y)[p.x + 1] > rr->pixels(pMax.y)[pMax.x])
      pMax = TPoint(p.x + 1, p.y);
    if (p.y > 0 && rr->pixels(p.y - 1)[p.x] > rr->pixels(pMax.y)[pMax.x])
      pMax = TPoint(p.x, p.y - 1);
    if (p.y < rr->getLy() - 1 &&
        rr->pixels(p.y + 1)[p.x] > rr->pixels(pMax.y)[pMax.x])
      pMax = TPoint(p.x, p.y + 1);

    if (p == pMax) break;

    p = pMax;
  }

  if (!isBright(rr->pixels(p.y)[p.x], c.m_threshold))
    return TPixel32::Black;
  else
    return rr->pixels(p.y)[p.x];
}

//------------------------------------------------------

inline TPixel32 takeLocalBrightest(const TRasterGR8P rgr, TRegion *r,
                                   const VectorizerConfiguration &c,
                                   TPoint &p) {
  TPoint pMax;

  while (r->contains(c.m_affine * convert(p))) {
    pMax = p;
    if (p.x > 0 && rgr->pixels(pMax.y)[pMax.x] < rgr->pixels(p.y)[p.x - 1])
      pMax = TPoint(p.x - 1, p.y);
    if (p.x < rgr->getLx() - 1 &&
        rgr->pixels(pMax.y)[pMax.x] < rgr->pixels(p.y)[p.x + 1])
      pMax = TPoint(p.x + 1, p.y);
    if (p.y > 0 && rgr->pixels(pMax.y)[pMax.x] < rgr->pixels(p.y - 1)[p.x])
      pMax = TPoint(p.x, p.y - 1);
    if (p.y < rgr->getLy() - 1 &&
        rgr->pixels(pMax.y)[pMax.x] < rgr->pixels(p.y + 1)[p.x])
      pMax = TPoint(p.x, p.y + 1);

    if (p == pMax) break;

    p = pMax;
  }

  if (!isBright(rgr->pixels(p.y)[p.x], c.m_threshold))
    return TPixel32::Black;
  else {
    int val = rgr->pixels(p.y)[p.x].value;
    return TPixel32(val, val, val, 255);
  }
}

//---------------------------------------------------------------

inline TPixel32 takeLocalDarkest(const TRaster32P rr, TRegion *r,
                                 const VectorizerConfiguration &c, TPoint &p) {
  TPoint pMax;

  while (r->contains(c.m_affine * convert(p)))  // 1
  {
    pMax = p;
    if (p.x > 0 && rr->pixels(p.y)[p.x - 1] < rr->pixels(pMax.y)[pMax.x])
      pMax = TPoint(p.x - 1, p.y);
    if (p.x < rr->getLx() - 1 &&
        rr->pixels(p.y)[p.x + 1] < rr->pixels(pMax.y)[pMax.x])
      pMax = TPoint(p.x + 1, p.y);
    if (p.y > 0 && rr->pixels(p.y - 1)[p.x] < rr->pixels(pMax.y)[pMax.x])
      pMax = TPoint(p.x, p.y - 1);
    if (p.y < rr->getLy() - 1 &&
        rr->pixels(p.y + 1)[p.x] < rr->pixels(pMax.y)[pMax.x])
      pMax = TPoint(p.x, p.y + 1);

    if (p == pMax) break;

    p = pMax;
  }

  return rr->pixels(p.y)[p.x];
}

//------------------------------------------------------

inline TPixel32 takeLocalDarkest(const TRasterGR8P rgr, TRegion *r,
                                 const VectorizerConfiguration &c, TPoint &p) {
  TPoint pMax;

  while (r->contains(c.m_affine * convert(p))) {
    pMax = p;
    if (p.x > 0 && rgr->pixels(p.y)[p.x - 1] < rgr->pixels(pMax.y)[pMax.x])
      pMax = TPoint(p.x - 1, p.y);
    if (p.x < rgr->getLx() - 1 &&
        rgr->pixels(p.y)[p.x + 1] < rgr->pixels(pMax.y)[pMax.x])
      pMax = TPoint(p.x + 1, p.y);
    if (p.y > 0 && rgr->pixels(p.y - 1)[p.x] < rgr->pixels(pMax.y)[pMax.x])
      pMax = TPoint(p.x, p.y - 1);
    if (p.y < rgr->getLy() - 1 &&
        rgr->pixels(p.y + 1)[p.x] < rgr->pixels(pMax.y)[pMax.x])
      pMax = TPoint(p.x, p.y + 1);

    if (p == pMax) break;

    p = pMax;
  }

  int val = rgr->pixels(p.y)[p.x].value;
  return TPixel32(val, val, val, 255);
}

//=================================================================
//  Vectorizer Core
//-----------------------------------------------------------------

void VectorizerCore::applyFillColors(TRegion *r, const TRasterP &ras,
                                     TPalette *palette,
                                     const CenterlineConfiguration &c,
                                     int regionCount) {
  struct locals {
    static inline bool alwaysTrue(const TPixelCM32 &) { return true; }
  };

  TRasterCM32P rt = ras;
  TRaster32P rr   = ras;
  TRasterGR8P rgr = ras;

  assert(rt || rr || rgr);

  bool isBrightRegion = true;
  {
    unsigned int e, edgesCount = r->getEdgeCount();
    for (e = 0; e < edgesCount; ++e) {
      if (isInkRegionEdge(r->getEdge(e)->m_s)) {
        if (r->getEdge(e)->m_w0 > r->getEdge(e)->m_w1) isBrightRegion = false;
        break;
      }
      if (isInkRegionEdgeReversed(r->getEdge(e)->m_s)) {
        if (r->getEdge(e)->m_w0 < r->getEdge(e)->m_w1) isBrightRegion = false;
        break;
      }
    }
  }

  TAffine inverse = c.m_affine.inv();
  TPointD pd;

  typedef bool (*cm_func)(const TPixelCM32 &, int);
  typedef bool (*rgbm_func)(const TPixelRGBM32 &, int);
  typedef bool (*gr_func)(const TPixelGR8 &, int);

  bool tookPoint =
      isBrightRegion
          ? rt
                ? getInternalPoint(
                      rt, tcg::bind2nd(cm_func(isBright), c.m_threshold),
                      inverse, c, r,
                      pd) ||  // If no bright pixel could be found,
                      getInternalPoint(rt, locals::alwaysTrue, inverse, c, r,
                                       pd)
                :  // then any pixel inside the region
                rr ? getInternalPoint(
                         rr, tcg::bind2nd(rgbm_func(isBright), c.m_threshold),
                         inverse, c, r, pd)
                   :  // must suffice.
                    getInternalPoint(
                        rgr, tcg::bind2nd(gr_func(isBright), c.m_threshold),
                        inverse, c, r, pd)
          : rt ? getInternalPoint(rt,
                                  tcg::bind2nd(cm_func(isDark), c.m_threshold),
                                  inverse, c, r, pd)
               : rr ? getInternalPoint(
                          rr, tcg::bind2nd(rgbm_func(isDark), c.m_threshold),
                          inverse, c, r, pd)
                    : getInternalPoint(
                          rgr, tcg::bind2nd(gr_func(isDark), c.m_threshold),
                          inverse, c, r, pd);

  if (tookPoint) {
    pd = inverse * pd;
    TPoint p(pd.x, pd.y);  // The same thing that happened inside
                           // getInternalPoint()
    if (ras->getBounds().contains(p)) {
      int styleId = 0;

      if (rt) {
        TPixelCM32 col = rt->pixels(p.y)[p.x];
        styleId        = isBrightRegion
                      ? col.getPaint()
                      : col.getInk();  // Only paint colors with centerline
      }                                // vectorization
      else {
        TPixel32 color;

        // Update color found to local brightness-extremals
        if (rr) {
          color = isBrightRegion ? takeLocalBrightest(rr, r, c, p)
                                 : takeLocalDarkest(rr, r, c, p);
        } else {
          color = isBrightRegion ? takeLocalBrightest(rgr, r, c, p)
                                 : takeLocalDarkest(rgr, r, c, p);
        }

        if (color.m != 0) {
          styleId           = palette->getClosestStyle(color);
          TPixel32 oldColor = palette->getStyle(styleId)->getMainColor();
          if (!(isAlmostZero(double(oldColor.r - color.r), 15.0) &&
                isAlmostZero(double(oldColor.g - color.g), 15.0) &&
                isAlmostZero(double(oldColor.b - color.b), 15.0))) {
            styleId = palette->getStyleCount();
            palette->getStylePage(1)->insertStyle(1, color);
            palette->setStyle(styleId, color);
          }
        }
      }

      ++regionCount;
      r->setStyle(styleId);
    }
  }

  for (int i = 0; i < (int)r->getSubregionCount(); ++i)
    applyFillColors(r->getSubregion(i), ras, palette, c, regionCount);
}

//-----------------------------------------------------------------

void VectorizerCore::applyFillColors(TRegion *r, const TRasterP &ras,
                                     TPalette *palette,
                                     const OutlineConfiguration &c,
                                     int regionCount) {
  TRasterCM32P rt = ras;
  TRaster32P rr   = ras;
  TRasterGR8P rgr = ras;

  assert(rt || rr || rgr);

  TAffine inverse = c.m_affine.inv();
  bool doInks = !c.m_ignoreInkColors, doPaints = !c.m_leaveUnpainted;

  // Retrieve a point inside the specified region
  TPointD pd;
  if (r->getInternalPoint(pd)) {
    pd = inverse * pd;     // Convert point to raster coordinates
    TPoint p(pd.x, pd.y);  //

    // Retrieve the corresponding pixel in the raster image

    if (ras->getBounds().contains(p)) {
      int styleId = 0;

      if (rt) {
        // Toonz colormap case
        TPixelCM32 col =
            rt->pixels(p.y)[p.x];  // In the outline vectorization case, color
        int tone = col.getTone();  // can be either ink or paint

        if (tone == 0)  // Full ink case
          styleId = doInks ? col.getInk() : 1;
        else if (tone == 255 && doPaints)  // Full paint case
          styleId = col.getPaint();
        else if (tone != 255) {
          if (regionCount % 2 == 1) {
            // Whenever regionCount is odd, ink is checked first

            if (isNearestInkOrPaintInRegion(true, rt, r, c.m_affine, p))
              styleId = doInks ? col.getInk() : 1;
            else if (doPaints &&
                     isNearestInkOrPaintInRegion(false, rt, r, c.m_affine, p))
              styleId = col.getPaint();
          } else {
            // Whenever regionCount is even, paint is checked first

            if (doPaints &&
                isNearestInkOrPaintInRegion(false, rt, r, c.m_affine, p))
              styleId = col.getPaint();
            else if (isNearestInkOrPaintInRegion(true, rt, r, c.m_affine, p))
              styleId = doInks ? col.getInk() : 1;
          }
        }
      } else {
        TPixel32 color;
        if (rr)
          color = rr->pixels(p.y)[p.x];
        else {
          int val = rgr->pixels(p.y)[p.x].value;
          color   = (val < 80) ? TPixel32::Black : TPixel32::White;
        }

        if ((color.m != 0) && ((!c.m_leaveUnpainted) ||
                               (c.m_leaveUnpainted && color == c.m_inkColor))) {
          styleId           = palette->getClosestStyle(color);
          TPixel32 oldColor = palette->getStyle(styleId)->getMainColor();

          if (!(isAlmostZero(double(oldColor.r - color.r), 15.0) &&
                isAlmostZero(double(oldColor.g - color.g), 15.0) &&
                isAlmostZero(double(oldColor.b - color.b), 15.0))) {
            styleId = palette->getStyleCount();
            palette->getStylePage(1)->insertStyle(1, color);
            palette->setStyle(styleId, color);
          }
        }
      }

      ++regionCount;
      r->setStyle(styleId);
    }
  }

  for (int i = 0; i < (int)r->getSubregionCount(); ++i)
    applyFillColors(r->getSubregion(i), ras, palette, c, regionCount);
}

//-----------------------------------------------------------------

void VectorizerCore::applyFillColors(TVectorImageP vi, const TImageP &img,
                                     TPalette *palette,
                                     const VectorizerConfiguration &c) {
  const CenterlineConfiguration &centConf =
      static_cast<const CenterlineConfiguration &>(c);
  const OutlineConfiguration &outConf =
      static_cast<const OutlineConfiguration &>(c);

  // If configuration is not set for color fill at all, quit.
  if (c.m_leaveUnpainted && (!c.m_outline || outConf.m_ignoreInkColors)) return;

  TToonzImageP ti  = img;
  TRasterImageP ri = img;

  assert(ti || ri);
  TRasterP ras = ti ? TRasterP(ti->getRaster()) : TRasterP(ri->getRaster());

  vi->findRegions();

  int r, regionsCount = vi->getRegionCount();
  if (c.m_outline) {
    for (r = 0; r < regionsCount; ++r)
      applyFillColors(vi->getRegion(r), ras, palette, outConf, 1);
  } else {
    for (r = 0; r < regionsCount; ++r)
      applyFillColors(vi->getRegion(r), ras, palette, centConf,
                      1);  // 1 - c.m_makeFrame;

    clearInkRegionFlags(vi);
  }
}

//=================================================================

TVectorImageP VectorizerCore::vectorize(const TImageP &img,
                                        const VectorizerConfiguration &c,
                                        TPalette *plt) {
  TVectorImageP vi;

  if (c.m_outline)
    vi = newOutlineVectorize(
        img, static_cast<const NewOutlineConfiguration &>(c), plt);
  else {
    TImageP img2(img);
    vi = centerlineVectorize(
        img2, static_cast<const CenterlineConfiguration &>(c), plt);

    if (vi) {
      for (int i = 0; i < (int)vi->getStrokeCount(); ++i) {
        TStroke *stroke = vi->getStroke(i);

        for (int j = 0; j < stroke->getControlPointCount(); ++j) {
          TThickPoint p = stroke->getControlPoint(j);
          p             = TThickPoint(c.m_affine * p, c.m_thickScale * p.thick);

          stroke->setControlPoint(j, p);
        }
      }

      applyFillColors(vi, img2, plt, c);
    }
  }

  return vi;
}

//-----------------------------------------------------------------

void VectorizerCore::emitPartialDone(void) {
  emit partialDone(m_currPartial++, m_totalPartials);
}

//-----------------------------------------------------------------
/*
void VectorizerCore::emitPartialDone(int current)
{
  m_currPartial= current;
  emit partialDone(current, m_totalPartials);
}
*/
