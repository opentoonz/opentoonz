

#include "toonz/tcenterlinevectorizer.h"
#include "tsystem.h"
#include "tstopwatch.h"
#include "tpalette.h"
//#include "timage_io.h"
//#include "tstrokeutil.h"
//#include "tspecialstyleid.h"
#include "trastercm.h"
#include "ttoonzimage.h"
//#include "dpiscale.h"
#include "tregion.h"
#include "tstroke.h"

#include "trasterimage.h"

#define SEARCH_WINDOW 3
#define JUNCTION_THICKNESS_RATIO 0.7
#define JOIN_LIMIT 0.8
#define THICKNESS_LIMIT 0.2
#define THICKNESS_LIMIT_2 0.04

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

void renormalizeImage(TVectorImage *vi) {
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
        /*if(d>0)
{
int yy = 123;
}*/
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

class Node;

class DataPixel {
public:
  TPoint m_pos;
  int m_value;
  bool m_ink;
  // int m_index;
  Node *m_node;
  DataPixel() : m_value(0), m_ink(false), /*m_index(0), */ m_node(0) {}
};

//---------------------------------------------------------
#ifdef WIN32
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

class ProtoRegion {
public:
  TPointD m_center;
  bool m_isConvex;
  std::vector<TThickPoint> m_points;
  ProtoRegion(bool isConvex) : m_points(), m_isConvex(isConvex), m_center() {}
};

class JunctionMerge {
public:
  Junction *m_junction;
  Node *m_startNode, *m_endNode;
  bool m_isNext;
  JunctionMerge(Junction *junction)
      : m_junction(junction), m_startNode(0), m_endNode(0), m_isNext(true) {}
  JunctionMerge(Junction *junction, Node *startNode, Node *endNode, bool isNext)
      : m_junction(junction)
      , m_startNode(startNode)
      , m_endNode(endNode)
      , m_isNext(isNext) {}
  bool operator==(const JunctionMerge &op2) {
    return (m_junction == op2.m_junction);
  }
};

class JunctionLink {
public:
  Junction *m_first;
  Junction *m_second;
  int m_order;
  JunctionLink(Junction *first, Junction *second)
      : m_first(first), m_second(second), m_order(0) {}
  bool operator==(const JunctionLink &op2) const {
    return (m_first == op2.m_first && m_second == op2.m_second) ||
           (m_first == op2.m_second && m_second == op2.m_first);
  }
};

class CenterLineVectorizer {
  TPalette *m_palette;

public:
  //	int m_delta[8];
  //	int m_radius;
  TRasterP m_src;

  VectorizerConfiguration m_configuration;

#ifdef DEBUG
  TRaster32P m_output;
#endif
  DataRasterP m_dataRaster;
  vector<pair<int, DataRasterP>> m_dataRasterArray;
  TVectorImageP m_vimage;
#ifdef DEBUG
  TVectorImageP m_rimage;
#endif

#ifdef DEBUG
  vector<vector<TThickPoint>> m_outlines;
  vector<TPointD> m_unvisitedNodes;
  vector<vector<Node *>> m_junctionPolygons;
#endif

  vector<Node *> m_nodes;
  vector<Junction *> m_junctions;
  vector<JunctionLink> m_links;
  list<ProtoStroke> m_protoStrokes;
  list<ProtoRegion> m_protoRegions;
  // list< std::vector<TThickPoint> > m_protoHoles;

  Node *findOtherSide(Node *node);
  bool testOtherSide(Node *na1, Node *nb1, double &startDist2);

  TPointD computeCenter(Node *na, Node *nb, double &r);

  bool isHole(Node *startNode);
  void traceLine(DataPixel *pix);  // , vector<TThickPoint> &points);

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

  CenterLineVectorizer(const VectorizerConfiguration &configuration,
                       TPalette *palette)
      : m_configuration(configuration), m_palette(palette) {}

  ~CenterLineVectorizer();

  void clearNodes();
  void createNodes();

  void clearJunctions();

  void makeOutputRaster();
  void makeDataRaster(const TRasterP &src);

  DataPixel *findUnvisitedInkPixel();

  bool linkNextProtoStroke(ProtoStroke *const dstProtoStroke,
                           Junction *const currJunction, const int k);
  void joinProtoStrokes(ProtoStroke *const dstProtoStroke);

  void resolveUnvisitedNode(Node *node);
  Junction *mergeJunctions(const std::list<JunctionMerge> &junctions);
  void joinJunctions();
  void createJunctionPolygon(Junction *junction);
  void handleLinks();
  void handleJunctions();

  void createStrokes();
  void createRegions();

  void vectorize();
  void init();
  //	void click(const TPoint &p);

  Node *createNode(DataPixel *pix);

  void link(DataPixel *pix, DataPixel *from, DataPixel *to);

  /*#ifdef DEBUG
  inline TPixel32 &getOutPix(DataPixel*pix) {return
m_output->pixels()[pix->m_index]; }
  void outputNodes(Node *node);
  void outputInks();
#endif*/

private:
  // not implemented
  CenterLineVectorizer(const CenterLineVectorizer &);
  CenterLineVectorizer &operator=(const CenterLineVectorizer &);
};

//=========================================================

class CompareJunctionNodes {
  TPointD m_center;

public:
  CompareJunctionNodes(TPointD center) : m_center(center) {}

  bool operator()(const Node *n1, const Node *n2) {
    TPointD p1 = convert(n1->m_pixel->m_pos) - m_center;
    TPointD p2 = convert(n2->m_pixel->m_pos) - m_center;

    double alpha1, alpha2;

    if (p1.x > 0)
      alpha1 = -p1.y / sqrt(norm2(p1));
    else if (p1.x < 0)
      alpha1 = 2 + p1.y / sqrt(norm2(p1));
    else  //(p1.x = 0)
    {
      if (p1.y > 0)
        alpha1 = -1;
      else if (p1.y < 0)
        alpha1 = 1;
      else
        assert(true);
    }

    if (p2.x > 0)
      alpha2 = -p2.y / sqrt(norm2(p2));
    else if (p2.x < 0)
      alpha2 = 2 + p2.y / sqrt(norm2(p2));
    else  //(p2.x = 0)
    {
      if (p2.y > 0)
        alpha2 = -1;
      else if (p2.y < 0)
        alpha2 = 1;
      else
        assert(true);
    }

    if (alpha2 - alpha1 > 0)
      return true;
    else if (alpha2 - alpha1 < 0)
      return false;
    else
      return false;  // n1->m_pixel->m_pos == n2->m_pixel->m_pos!!
  }
};

//---------------------------------------------------------
//---------------------------------------------------------

inline bool collinear(const TPointD &a, const TPointD &b, const TPointD &c) {
  double area = (b.x - a.x) * (c.y - a.y) - (c.x - a.x) * (b.y - a.y);
  return area == 0;
}

//---------------------------------------------------------

inline bool right(const TPointD &a, const TPointD &b, const TPointD &c) {
  double area = (b.x - a.x) * (c.y - a.y) - (c.x - a.x) * (b.y - a.y);
  return area < 0;
}

//---------------------------------------------------------

enum RectIntersectionResult { parallel, coincident, intersected };

RectIntersectionResult rectsIntersect(const TPointD &a1, const TPointD &a2,
                                      const TPointD &b1, const TPointD &b2,
                                      TPointD &intersection) {
  double ma, mb, ca, cb;

  if (a2.x == a1.x && b2.x == b1.x) {
    if (collinear(a1, a2, b1) && collinear(a1, a2, b2))
      return coincident;
    else
      return parallel;
  } else if (a1.x == a2.x) {
    mb             = (b2.y - b1.y) / (b2.x - b1.x);
    cb             = b1.y - mb * b1.x;
    intersection.x = a1.x;
    intersection.y = mb * intersection.x + cb;
    return intersected;
  } else if (b1.x == b2.x) {
    ma             = (a2.y - a1.y) / (a2.x - a1.x);
    ca             = a1.y - ma * a1.x;
    intersection.x = b1.x;
    intersection.y = ma * intersection.x + ca;
    return intersected;
  } else {
    ma = (a2.y - a1.y) / (a2.x - a1.x);
    mb = (b2.y - b1.y) / (b2.x - b1.x);

    if (ma == mb) {
      if (collinear(a1, a2, b1) && collinear(a1, a2, b2))
        return coincident;
      else
        return parallel;
    }

    ca = a1.y - ma * a1.x;
    cb = b1.y - mb * b1.x;

    intersection.x = (ca - cb) / (mb - ma);
    intersection.y = ma * intersection.x + ca;
    return intersected;
  }
}

//---------------------------------------------------------
//---------------------------------------------------------

bool Junction::isConvex() {
  int nonConvexAngles = 0;
  assert(m_nodes.size() > 2);
  std::deque<Node *>::iterator it   = m_nodes.begin();
  std::deque<Node *>::iterator it_e = m_nodes.end() - 2;
  for (; it != it_e; ++it) {
    if (right(convert((*(it))->m_pixel->m_pos),
              convert((*(it + 1))->m_pixel->m_pos),
              convert((*(it + 2))->m_pixel->m_pos)))
      nonConvexAngles++;
    if (nonConvexAngles > 1) return false;
  }
  if (right(convert((*(it))->m_pixel->m_pos),
            convert((*(it + 1))->m_pixel->m_pos),
            convert((*(m_nodes.begin()))->m_pixel->m_pos)))
    nonConvexAngles++;
  if (nonConvexAngles > 1) return false;
  return true;
}

//---------------------------------------------------------
//---------------------------------------------------------

CenterLineVectorizer::~CenterLineVectorizer() {
  clearNodes();
  clearJunctions();

#ifdef DEBUG
  m_outlines.clear();
  m_unvisitedNodes.clear();
  m_junctionPolygons.clear();
#endif

  m_links.clear();
  m_protoStrokes.clear();
  m_protoRegions.clear();
  // m_protoHoles.clear();
}

//---------------------------------------------------------

void CenterLineVectorizer::clearNodes() {
  for (int i = 0; i < (int)m_nodes.size(); i++) delete m_nodes[i];
  m_nodes.clear();
}

//---------------------------------------------------------

void CenterLineVectorizer::clearJunctions() {
  for (int i = 0; i < (int)m_junctions.size(); i++) delete m_junctions[i];
  m_junctions.clear();
}

//---------------------------------------------------------

void CenterLineVectorizer::link(DataPixel *pix, DataPixel *srcPix,
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

inline int colorDistance2(const TPixel32 &c0, const TPixel32 &c1) {
  return ((c0.r - c1.r) * (c0.r - c1.r) + (c0.g - c1.g) * (c0.g - c1.g) +
          (c0.b - c1.b) * (c0.b - c1.b));
}

//---------------------------------------------------------
#define MAX_TOLERANCE 20

#include "tcolorstyles.h"

void CenterLineVectorizer::makeDataRaster(const TRasterP &src) {
  m_vimage = new TVectorImage();
  if (!src) return;
  m_src = src;

  clearNodes();
  clearJunctions();

  int ii, x, y = 0;
  TRaster32P srcRGBM = (TRaster32P)m_src;
  TRasterCM32P srcCM = (TRasterCM32P)m_src;
  TRasterGR8P srcGR  = (TRasterGR8P)m_src;

  DataRasterP dataRaster(m_src->getSize().lx + 2, m_src->getSize().ly + 2);
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

  if (srcRGBM) {
    assert(m_palette);
    int inkId = m_palette->addStyle(m_configuration.m_inkColor);

    m_dataRasterArray.push_back(pair<int, DataRasterP>(inkId, dataRaster));
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

        // int value = (inPix->r + 2*inPix->g + inPix->b)>>2;
        if (y == 0 || y == m_src->getLy() - 1 || x == 0 ||
            x == m_src->getLx() - 1) {
          dataPix->m_value = 255;
          dataPix->m_ink   = false;
        } else {
          dataPix->m_value = (inPix->r + 2 * inPix->g + inPix->b) >> 2;
          dataPix->m_ink   = (distance2 < maxDistance2);
        }
        dataPix->m_pos.x = x++;
        dataPix->m_pos.y = y;
        dataPix->m_node  = 0;
        // dataPix->m_index = index++;
        inPix++;
        dataPix++;
      }
    }
  } else if (srcGR) {
    assert(m_palette);
    int inkId = m_palette->addStyle(m_configuration.m_inkColor);

    m_dataRasterArray.push_back(pair<int, DataRasterP>(inkId, dataRaster));
    int threshold = m_configuration.m_threshold;

    for (y = 0; y < m_src->getLy(); y++) {
      TPixelGR8 *inPix    = srcGR->pixels(y);
      TPixelGR8 *inEndPix = inPix + srcGR->getLx();
      DataPixel *dataPix  = dataRaster->pixels(y + 1) + 1;
      x                   = 0;
      while (inPix < inEndPix) {
        *dataPix = DataPixel();
        // int distance2 = colorDistance2(m_configuration.m_inkColor, *inPix);

        // int value = (inPix->r + 2*inPix->g + inPix->b)>>2;
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
        // dataPix->m_index = index++;
        inPix++;
        dataPix++;
      }
    }
  }

  else if (srcCM) {
    int currInk, nextInk = 0;
    do {
      int threshold =
          m_configuration.m_threshold;  // tolerance: 1->MAX thresh: 1-255
      currInk = nextInk;
      nextInk = 0;
      m_dataRasterArray.push_back(pair<int, DataRasterP>(currInk, dataRaster));

      // inizializza la parte centrale
      for (y = 0; y < srcCM->getLy(); y++) {
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
          // TPixel col = m_palette->getStyle(inPix->getInk())->getMainColor();
          dataPix->m_pos.x = x++;
          dataPix->m_pos.y = y;
          dataPix->m_value = value;
          dataPix->m_ink   = (value < threshold);
          dataPix->m_node  = 0;
          // dataPix->m_index = index++;
          inPix++;
          dataPix++;
        }
      }
    } while (nextInk != 0);
    if (m_configuration.m_ignoreInkColors) {
      assert(m_dataRasterArray.size() == 1);
      m_dataRasterArray.back().first = 1;
    }
  } else
    assert(false);
}

void CenterLineVectorizer::init() {
  int y;

  /*m_links.clear();
  m_protoStrokes.clear();
  m_protoRegions.clear();
  clearNodes();
  clearJunctions();*/

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
/*
#ifdef DEBUG

    int i;
    for(i = 0;i<(int)m_nodes.size();i++)
        {
                Node *node = m_nodes[i];
                if(node->m_pixel==0) continue;
                assert(node->m_prev);
                assert(node->m_next);
                assert(node->m_prev->m_next == node);
                assert(node->m_next->m_prev == node);
        }

    for(i = 0;i<m_dataRaster->getLx()*m_dataRaster->getLy(); i++)
        {
                DataPixel *pix = m_dataRaster->pixels()+i;
                for(Node *node = pix->m_node; node; node=node->m_other)
                {
                        if(node->m_pixel==0) continue;
                        assert(node->m_pixel == pix);
                        assert(node->m_prev);
                        assert(node->m_next);
                        assert(node->m_prev->m_next == node);
                        assert(node->m_next->m_prev == node);
                }
        }
#endif
*/

//---------------------------------------------------------

TPointD CenterLineVectorizer::computeCenter(Node *na, Node *nb, double &r) {
  TPointD pa = convert(na->m_pixel->m_pos);
  TPointD pb = convert(nb->m_pixel->m_pos);
  r          = THICKNESS_LIMIT;
  if (pa == pb) return pa;

  TPointD center((pa + pb) * 0.5);
  r = norm(pa - pb) * 0.5;
  return center;

  /*	double vsum = 0.0;
  TPointD d = pa-pb;
  if(fabs(d.x)>fabs(d.y))
{
          if(pa.x>pb.x) std::swap(pa,pb);
          for(int x = pa.x; x<=pb.x; x++)
          {
                  int y = pa.y + (pb.y-pa.y)*(x-pa.x)/(pb.x-pa.x);
                  int v = 255-m_dataRaster->pixels(y)[x].m_value;
                  center += v * TPointD(x,y);
                  vsum += v;
  }
}
  else
{
          if(pa.y>pb.y) std::swap(pa,pb);
          for(int y = pa.y; y<=pb.y; y++)
          {
                  int x = pa.x + (pb.x-pa.x)*(y-pa.y)/(pb.y-pa.y);
                  int v = 255-m_dataRaster->pixels(y)[x].m_value;
                  center += v * TPointD(x,y);
                  vsum += v;
          }
}
  assert(vsum>0);
  r = 0.5*vsum/255.0;
  return center * (1/vsum);
*/
}

//---------------------------------------------------------

double computeDistance2(Node *na, Node *nb) {
  assert(na->m_pixel);
  assert(nb->m_pixel);
  TPointD d = convert(na->m_pixel->m_pos - nb->m_pixel->m_pos);
  return d * d;
}

//---------------------------------------------------------

Node *CenterLineVectorizer::findOtherSide(Node *node) {
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
    // if (!m_dataRaster->getBounds().contains(q)) break;
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

TPointD findDirection(Node *na, Node *nb, int skip) {
  for (int i = 0; i < skip; i++) {
    na = na->m_next;
    nb = nb->m_prev;
  }
  TPointD p0 =
      0.5 * (convert(na->m_pixel->m_pos) + convert(nb->m_pixel->m_pos));
  na = na->m_next;
  nb = nb->m_prev;
  TPointD p1 =
      0.5 * (convert(na->m_pixel->m_pos) + convert(nb->m_pixel->m_pos));
  na = na->m_next;
  nb = nb->m_prev;
  TPointD p2 =
      0.5 * (convert(na->m_pixel->m_pos) + convert(nb->m_pixel->m_pos));
  TPointD dir = 0.25 * (3 * (p1 - p0) + (p2 - p1));
  double dirn = norm(dir);
  if (dirn > 0)
    return dir * (1 / dirn);
  else
    return dir;
}

//---------------------------------------------------------

void setEndpointsDirections(ProtoStroke &protoStroke) {
  assert(!protoStroke.m_points.empty());
  int size              = protoStroke.m_points.size();
  double startThickness = 0, endThickness = 0;
  if (size == 1) return;
  if (size < 5) {
    protoStroke.m_startDirection =
        protoStroke.m_points[0] - protoStroke.m_points[1];
    protoStroke.m_endDirection =
        protoStroke.m_points[size - 1] - protoStroke.m_points[size - 2];
  } else if (size < 10) {
    assert(protoStroke.m_points.front() != protoStroke.m_points.back());
    protoStroke.m_startDirection =
        protoStroke.m_points[0] - protoStroke.m_points[2];
    protoStroke.m_endDirection =
        protoStroke.m_points[size - 1] - protoStroke.m_points[size - 3];
    startThickness =
        0.5 * (protoStroke.m_points[1].thick + protoStroke.m_points[2].thick);
    endThickness = 0.5 * (protoStroke.m_points[size - 2].thick +
                          protoStroke.m_points[size - 3].thick);
  } else {
    protoStroke.m_startDirection = 4 * protoStroke.m_points[0];
    protoStroke.m_endDirection   = 4 * protoStroke.m_points[size - 1];

    for (int i = 0; i < 4; ++i) {
      protoStroke.m_startDirection -= protoStroke.m_points[3 + i];
      protoStroke.m_endDirection -= protoStroke.m_points[size - 4 - i];
      startThickness += protoStroke.m_points[i].thick;
      endThickness += protoStroke.m_points[size - i - 1].thick;
    }
    protoStroke.m_startDirection = protoStroke.m_startDirection * (1 / 4.0);
    protoStroke.m_endDirection   = protoStroke.m_endDirection * (1 / 4.0);
    startThickness *= (1 / 4.0);
    endThickness *= (1 / 4.0);
  }

  double startNorm = norm(protoStroke.m_startDirection);
  if (startNorm)
    protoStroke.m_startDirection =
        protoStroke.m_startDirection * (1 / startNorm);

  double endNorm = norm(protoStroke.m_endDirection);
  if (endNorm)
    protoStroke.m_endDirection = protoStroke.m_endDirection * (1 / endNorm);

  if (startThickness != 0)
    protoStroke.m_points[0].thick     = protoStroke.m_points[1].thick =
        protoStroke.m_points[2].thick = startThickness;
  if (endThickness != 0)
    protoStroke.m_points[size - 1].thick =
        protoStroke.m_points[size - 2].thick =
            protoStroke.m_points[size - 3].thick = endThickness;
}

//---------------------------------------------------------

bool CenterLineVectorizer::testOtherSide(Node *na1, Node *nb1,
                                         double &startDist2) {
  if (na1->m_pixel->m_pos == nb1->m_pixel->m_pos || na1->m_next == nb1 ||
      na1->m_prev == nb1 || na1->m_next->m_next == nb1 ||
      na1->m_prev->m_prev == nb1)
    return false;

  Node *na2 = na1->m_next;
  for (int i = 1; i < 3; ++i) {
    na2       = na2->m_next;
    Node *nb2 = findOtherSide(na2);
    if (nb2 == 0 && i == 3) return true;
    if (nb2 == 0) continue;

    double newStartDist2                       = computeDistance2(na2, nb2);
    if (newStartDist2 > startDist2) startDist2 = newStartDist2;
    for (int j = 0; j < 3 * i + 1; ++j) {
      if (nb2 == nb1) return true;
      nb2 = nb2->m_next;
    }
  }
  return false;
}

//---------------------------------------------------------
/*
bool CenterLineVectorizer::isHole(Node *startNode)
{
        Node *node = startNode;
        for(int i=0; i<20; ++i)
        {
                node = node->m_next;
                if(node == startNode)
                {
                        std::vector<TThickPoint> points;
                        do
                        {
                                node = node->m_next;
                                node->m_visited = true;
                                node->m_flag = true;
                                points.push_back(convert(node->m_pixel->m_pos));
                        }
                        while(node != startNode);
                        m_protoHoles.push_back(points);
                        return true;
                }
        }
        return false;
}
*/

//---------------------------------------------------------

bool isJunction(const Node *na, const Node *nb) {
  TPointD pa = convert(na->m_pixel->m_pos);
  TPointD pb = convert(nb->m_pixel->m_pos);

  Node *na0 = na->m_prev;
  Node *na1 = na->m_next;
  Node *nb0 = nb->m_next;
  Node *nb1 = nb->m_prev;

  TPointD da = convert(na1->m_pixel->m_pos) - pa;
  TPointD db = convert(nb1->m_pixel->m_pos) - pb;

  TPointD da0 = pa - convert(na0->m_pixel->m_pos);
  TPointD db0 = pb - convert(nb0->m_pixel->m_pos);

  if ((da0.x * da.x + da0.y * da.y) / (norm(da0) * norm(da)) <= 0) {
    return true;
  }

  if ((db0.x * db.x + db0.y * db.y) / (norm(db0) * norm(db)) <= 0) {
    return true;
  }

  if (right(TPointD(0, 0), da, db)) {
    if ((da.x * db.x + da.y * db.y) / (norm(da) * norm(db)) <= 0) {
      return true;
    }
  } else {
    if ((da.x * db.x + da.y * db.y) / (norm(da) * norm(db)) <= -0.5) {
      return true;
    }
  }
  return false;
}

//---------------------------------------------------------

void CenterLineVectorizer::traceLine(DataPixel *pix) {
  Junction *startJunction = 0;
  Junction *endJunction   = 0;
  assert(m_dataRaster);
  const int wrap = m_dataRaster->getWrap();
  //  TRect bounds = m_dataRaster->getBounds();
  if (!pix->m_ink) return;
  while (pix->m_node == 0) pix -= wrap;

  Node *node                                               = pix->m_node;
  while (node && node->m_pixel == 0 && node->m_other) node = node->m_other;
  assert(node && node->m_pixel == pix);

  Node *naa = node;

  bool isSamePixel;
  if (node->m_other && node->m_other->m_pixel &&
      (node->m_pixel->m_pos == node->m_other->m_pixel->m_pos)) {
    node        = node->m_other;
    isSamePixel = true;
  } else {
    node        = findOtherSide(node);
    isSamePixel = false;
  }
  if (node == 0) return;
  Node *nbb = node;

  if (naa->m_visited || nbb->m_visited) return;

  double startDist2 = computeDistance2(naa, nbb);
  if (startDist2 > 2000) return;
  if (startDist2 < THICKNESS_LIMIT_2) startDist2 = THICKNESS_LIMIT_2;

  // try to detect errors in findOtherSide
  if (!isSamePixel && !testOtherSide(naa, nbb, startDist2)) return;

  double minDist2 = 0;
  double maxDist2 = startDist2 + 3.0;
  if (startDist2 > 36) {
    minDist2 = startDist2 / 2;
    maxDist2 = startDist2 + 0.25 * startDist2;
  }
  if (naa->m_next == nbb || naa->m_prev == nbb) return;

  naa->m_visited = nbb->m_visited = true;
  std::deque<TThickPoint> dpoints;

  for (int k = 0; k < 2; k++) {
    Node *na = naa;
    Node *nb = nbb;

    for (;;) {
      TPoint &pa = na->m_pixel->m_pos;
      TPoint &pb = nb->m_pixel->m_pos;
      TPoint ba  = pb - pa;

      Node *na1 = na->m_next;
      Node *nb1 = nb->m_prev;
      // if (!na1 || !nb1) break;

      TPoint da = na1->m_pixel->m_pos - pb;
      TPoint db = nb1->m_pixel->m_pos - pa;

      if (na1->m_junction || nb1->m_junction) {
        na1->m_visited = true;
        nb1->m_visited = true;
        TPointD p1;
        if (na1->m_junction && nb1->m_junction) {
          //					assert(na1->m_junction ==
          // nb1->m_junction);
        } else {
          if (na1->m_junction) {
            p1              = convert(nb1->m_pixel->m_pos);
            nb1->m_junction = na1->m_junction;
            na1->m_junction->m_nodes.push_front(nb1);
          } else {
            p1              = convert(na1->m_pixel->m_pos);
            na1->m_junction = nb1->m_junction;
            nb1->m_junction->m_nodes.push_back(na1);
          }
        }
        double r;
        TPointD center;
        TThickPoint point;

        center                     = computeCenter(na, nb, r);
        if (r < THICKNESS_LIMIT) r = THICKNESS_LIMIT;
        point                      = TThickPoint(center, r);
        if (k == 0)
          dpoints.push_front(point);
        else
          dpoints.push_back(point);

        center                     = computeCenter(na1, nb1, r);
        if (r < THICKNESS_LIMIT) r = THICKNESS_LIMIT;
        point                      = TThickPoint(center, r);
        if (k == 0)
          dpoints.push_front(point);
        else
          dpoints.push_back(point);

        if (k == 0)
          startJunction = na1->m_junction;
        else
          endJunction = na1->m_junction;

        break;
      }

      //			if(da*ba>-db*ba)
      if (norm2(db) >= norm2(da)) {
        if (na1->m_visited) break;

        na            = na1;
        na->m_visited = true;
#ifdef DEBUG
        getOutPix(na->m_pixel) = TPixel32::Red;
#endif
      } else {
        if (nb1->m_visited) break;

        nb            = nb1;
        nb->m_visited = true;
#ifdef DEBUG
        getOutPix(nb->m_pixel) = TPixel32::Green;
#endif
      }
      double distance2                = computeDistance2(na, nb);
      if (distance2 < 0.25) distance2 = 0.25;

      double r;
      TPointD center             = computeCenter(na, nb, r);
      if (r < THICKNESS_LIMIT) r = THICKNESS_LIMIT;
      TThickPoint point(center, r);
      TPointI pos = convert(center);
      // TPixel32  *inPix = m_src->pixels(pos.y);
      // inPix += pos.x;
      // int value = (inPix->r + 2*inPix->g + inPix->b)>>2;

      if (distance2 > maxDist2 || distance2 < minDist2 || na->m_next == nb ||
          na->m_prev == nb
          //|| isJunction(na, nb)
          //|| !(value<200)
          ) {
        if (na->m_next == nb || na->m_prev == nb || na->m_next->m_next == nb ||
            na->m_prev->m_prev == nb) {
          if (k == 0) {
            if (!dpoints.empty()) dpoints.pop_front();
            if (!dpoints.empty()) dpoints.pop_front();
          } else {
            if (!dpoints.empty()) dpoints.pop_back();
            if (!dpoints.empty()) dpoints.pop_back();
          }
        }
        //				if(k==0) dpoints.push_front(point);
        //				else dpoints.push_back(point);
        na->m_junction = nb->m_junction = new Junction();
        na->m_junction->m_nodes.push_back(nb);
        nb->m_junction->m_nodes.push_back(na);
        m_junctions.push_back(na->m_junction);

        if (k == 0)
          startJunction = na->m_junction;
        else
          endJunction = na->m_junction;
        break;
      }
      if (k == 0)
        dpoints.push_front(point);
      else
        dpoints.push_back(point);
    }

    std::swap(naa, nbb);
  }
  if (dpoints.size() == 0)
    naa->m_visited = nbb->m_visited = false;
  else {
    m_protoStrokes.push_back(ProtoStroke(dpoints.begin(), dpoints.end()));
    setEndpointsDirections(m_protoStrokes.back());
    if (startJunction) {
      m_protoStrokes.back().m_startJunction = startJunction;
      startJunction->m_protoStrokes.push_back(&m_protoStrokes.back());
    }
    if (endJunction) {
      m_protoStrokes.back().m_endJunction = endJunction;
      endJunction->m_protoStrokes.push_back(&m_protoStrokes.back());
    }
  }
}

//---------------------------------------------------------

Node *CenterLineVectorizer::createNode(DataPixel *pix) {
  Node *node    = new Node();
  node->m_pixel = pix;
  node->m_other = pix->m_node;
  pix->m_node   = node;
  m_nodes.push_back(node);
  return node;
}

//---------------------------------------------------------

void CenterLineVectorizer::createNodes() {}

//---------------------------------------------------------

#ifdef DEBUG
void CenterLineVectorizer::outputNodes(Node *node) {
  std::vector<TThickPoint> points;
  Node *start = node, *curr = node;
  int i = 0;
  do {
    TThickPoint point(convert(curr->m_pixel->m_pos), 0);
    points.push_back(point);

    curr->m_flag = true;
    assert(curr->m_next);
    curr = curr->m_next;
    i++;
  } while (curr != start);
  TThickPoint point(convert(curr->m_pixel->m_pos), 0);
  points.push_back(point);
  m_outlines.push_back(points);
}
#endif

//---------------------------------------------------------

bool CenterLineVectorizer::linkNextProtoStroke(
    ProtoStroke *const dstProtoStroke, Junction *const currJunction, int k) {
  assert(dstProtoStroke);
  assert(currJunction);
  //	if(dstProtoStroke->m_startJunction == dstProtoStroke->m_endJunction)
  return false;
  if (k == 0 && dstProtoStroke->m_startJunction->m_locked ||
      k == 1 && dstProtoStroke->m_endJunction->m_locked)
    return false;

  ProtoStroke *srcProtoStroke = 0;
  Junction *nextJunction      = 0;

  std::vector<ProtoStroke *>::iterator it;
  std::vector<ProtoStroke *>::iterator it_e =
      currJunction->m_protoStrokes.end();
  std::vector<ProtoStroke *>::iterator it_candidate = it_e;
  double candidateProbability                       = JOIN_LIMIT;
  int candidate_k;
  for (it = currJunction->m_protoStrokes.begin(); it != it_e; it++) {
    // erase dstProtoStroke in currJunction
    if (*it == dstProtoStroke) {
      it   = currJunction->m_protoStrokes.erase(it);
      it_e = currJunction->m_protoStrokes.end();
      if (candidateProbability == JOIN_LIMIT) it_candidate = it_e;
      if (it == it_e) break;
    }
    double probability;
    int next_k;
    if (((*it))->m_startJunction == currJunction)
      next_k = 0;
    else if ((*it)->m_endJunction == currJunction)
      next_k = 1;
    else
      assert(false);

    if (k == 0) {
      if (next_k == 0)
        probability = 1 - norm2(dstProtoStroke->m_startDirection +
                                (*it)->m_startDirection);
      else
        probability =
            1 - norm2(dstProtoStroke->m_startDirection + (*it)->m_endDirection);
    } else {
      if (next_k == 0)
        probability =
            1 - norm2(dstProtoStroke->m_endDirection + (*it)->m_startDirection);
      else
        probability =
            1 - norm2(dstProtoStroke->m_endDirection + (*it)->m_endDirection);
    }

    if (probability > candidateProbability ||
        //			currJunction->m_junctionOrder == 2 ||
        (*it)->m_points.size() < 3) {
      it_candidate         = it;
      candidateProbability = probability;
      candidate_k          = next_k;
    }
  }

  if (it_candidate == it_e) return false;
  srcProtoStroke = *it_candidate;
  assert(srcProtoStroke);

  if ((*it_candidate)->m_startJunction == (*it_candidate)->m_endJunction)
    return false;
  /*	if(k==0)
  {
          if(	candidate_k == 0 && dstProtoStroke->m_startJunction ==
(*it_candidate)->m_endJunction ||
                  candidate_k == 1 && dstProtoStroke->m_startJunction ==
(*it_candidate)->m_startJunction	)
                  return false;
  }
  else if(k==1)
  {
          if(	candidate_k == 0 && dstProtoStroke->m_endJunction ==
(*it_candidate)->m_endJunction ||
                  candidate_k == 1 && dstProtoStroke->m_endJunction ==
(*it_candidate)->m_startJunction	)
                  return false;
  }
*/
  if (candidate_k == 0)
    nextJunction = (*it_candidate)->m_endJunction;
  else
    nextJunction = (*it_candidate)->m_startJunction;

  if (k == 0) {
    if (candidate_k == 0) {
      dstProtoStroke->m_points.insert(dstProtoStroke->m_points.begin(),
                                      srcProtoStroke->m_points.rbegin(),
                                      srcProtoStroke->m_points.rend());
    } else {
      dstProtoStroke->m_points.insert(dstProtoStroke->m_points.begin(),
                                      srcProtoStroke->m_points.begin(),
                                      srcProtoStroke->m_points.end());
    }
  } else {
    if (candidate_k == 0) {
      dstProtoStroke->m_points.insert(dstProtoStroke->m_points.end(),
                                      srcProtoStroke->m_points.begin(),
                                      srcProtoStroke->m_points.end());
    } else {
      dstProtoStroke->m_points.insert(dstProtoStroke->m_points.end(),
                                      srcProtoStroke->m_points.rbegin(),
                                      srcProtoStroke->m_points.rend());
    }
  }

  srcProtoStroke->m_points.clear();
  srcProtoStroke->m_startJunction = 0;
  srcProtoStroke->m_endJunction   = 0;

  // refresh currJunction
  currJunction->m_protoStrokes.erase(it_candidate);

  // refresh nextJunction
  if (nextJunction) {
    it = std::find(nextJunction->m_protoStrokes.begin(),
                   nextJunction->m_protoStrokes.end(), srcProtoStroke);
    assert(it != nextJunction->m_protoStrokes.end());
    nextJunction->m_protoStrokes.erase(it);
    nextJunction->m_protoStrokes.push_back(dstProtoStroke);
  }

  if (k == 0) {
    dstProtoStroke->m_startJunction = nextJunction;

    if (candidate_k == 0)
      dstProtoStroke->m_startDirection = srcProtoStroke->m_endDirection;
    else
      dstProtoStroke->m_startDirection = srcProtoStroke->m_startDirection;
  } else {
    dstProtoStroke->m_endJunction = nextJunction;

    if (candidate_k == 0)
      dstProtoStroke->m_endDirection = srcProtoStroke->m_endDirection;
    else
      dstProtoStroke->m_endDirection = srcProtoStroke->m_startDirection;
  }
  if (!nextJunction) return false;
  return true;
}

//---------------------------------------------------------

void CenterLineVectorizer::joinProtoStrokes(ProtoStroke *const dstProtoStroke) {
  // std::deque<TThickPoint> &dstPoints = dstProtoStroke->m_points;
  for (int k = 0; k < 2; k++) {
    for (;;) {
      Junction *currJunction;
      if (k == 0 && !dstProtoStroke->m_startJunction)
        break;
      else if (k == 0)
        currJunction = dstProtoStroke->m_startJunction;
      else if (!dstProtoStroke->m_endJunction)
        break;
      else
        currJunction = dstProtoStroke->m_endJunction;

      if (!linkNextProtoStroke(dstProtoStroke, currJunction, k)) break;
    }
  }
  std::vector<ProtoStroke *>::iterator it;
  if (dstProtoStroke->m_startJunction) {
    it = std::find(dstProtoStroke->m_startJunction->m_protoStrokes.begin(),
                   dstProtoStroke->m_startJunction->m_protoStrokes.end(),
                   dstProtoStroke);
    if (it != dstProtoStroke->m_startJunction->m_protoStrokes.end())
      dstProtoStroke->m_startJunction->m_protoStrokes.erase(it);
    dstProtoStroke->m_startJunction = 0;
  }
  if (dstProtoStroke->m_endJunction) {
    it = std::find(dstProtoStroke->m_endJunction->m_protoStrokes.begin(),
                   dstProtoStroke->m_endJunction->m_protoStrokes.end(),
                   dstProtoStroke);
    if (it != dstProtoStroke->m_endJunction->m_protoStrokes.end())
      dstProtoStroke->m_endJunction->m_protoStrokes.erase(it);
    dstProtoStroke->m_endJunction = 0;
  }
}

//---------------------------------------------------------

bool y_intersect(const double y0, const TPointD p1, const TPointD p2,
                 double &x0) {
  if ((y0 < p1.y && y0 < p2.y) || (y0 > p1.y && y0 > p2.y)) return false;

  x0 = (p1.x * (p2.y - y0) + p2.x * (y0 - p1.y)) / (p2.y - p1.y);
  return true;
}

bool isContained(TThickPoint point, std::deque<Node *> &polygon) {
  std::deque<Node *>::iterator it_b = polygon.begin();
  std::deque<Node *>::iterator it_e = polygon.end();
  std::deque<Node *>::iterator it;

  int intersectionsNumber = 0;

  for (it = it_b; it < it_e; ++it) {
    double x;
    TPointD p1, p2;

    p1 = convert((*it)->m_pixel->m_pos);
    if ((it + 1) == it_e)
      p2 = convert((*it_b)->m_pixel->m_pos);
    else
      p2 = convert((*(it + 1))->m_pixel->m_pos);

    if (point.y == p1.y || point.y == p2.y || p1.y == p2.y) continue;
    if (y_intersect(point.y, p1, p2, x) && x >= point.x) ++intersectionsNumber;
  }

  return (intersectionsNumber % 2 == 0) ? false : true;
}

//---------------------------------------------------------

Junction *CenterLineVectorizer::mergeJunctions(
    const std::list<JunctionMerge> &junctions) {
  Junction *newJunction = new Junction();

  std::list<JunctionMerge>::const_iterator it_junction;
  std::deque<Node *>::iterator it_startNode, it_endNode;

  for (it_junction = junctions.begin(); it_junction != junctions.end();
       ++it_junction) {
    newJunction->m_center += it_junction->m_junction->m_center;

    std::deque<Node *>::iterator it_node;
    if (it_junction->m_startNode == 0 && it_junction->m_endNode == 0) {
      for (it_node = it_junction->m_junction->m_nodes.begin();
           it_node != it_junction->m_junction->m_nodes.end(); ++it_node) {
        (*it_node)->m_junction = newJunction;
        newJunction->m_nodes.push_back(*it_node);
      }
    } else {
      //			if(
      // isContained(it_junction->m_junction->m_center, newJunction->m_nodes) )
      // continue;
      /*			if(
      it_junction->m_junction->m_nodes.size() < 3)
      {
              std::vector<ProtoStroke*>::iterator it_protoStroke;
              bool ignoreJunction = false;
              for(it_protoStroke =
      it_junction->m_junction->m_protoStrokes.begin();
                      it_protoStroke!=it_junction->m_junction->m_protoStrokes.end();
                      ++it_protoStroke)
              {
                      if(	(*it_protoStroke)->m_startJunction ==
      newJunction
      ||
                              (*it_protoStroke)->m_endJunction == newJunction )
                      {
                              ignoreJunction = true;
                              break;
                      }
              }
              if(ignoreJunction) continue;
      }
*/

      it_startNode =
          std::find(newJunction->m_nodes.begin(), newJunction->m_nodes.end(),
                    it_junction->m_startNode);
      assert(it_startNode != newJunction->m_nodes.end());
      it_endNode = std::find(it_junction->m_junction->m_nodes.begin(),
                             it_junction->m_junction->m_nodes.end(),
                             it_junction->m_endNode);
      assert(it_endNode != it_junction->m_junction->m_nodes.end());

      for (it_node = it_junction->m_junction->m_nodes.begin();
           it_node != it_junction->m_junction->m_nodes.end(); ++it_node) {
        (*it_node)->m_junction = newJunction;
      }

      if (it_junction->m_isNext) {
        ++it_startNode;
        newJunction->m_nodes.insert(
            it_startNode, it_junction->m_junction->m_nodes.begin(), it_endNode);
        it_startNode =
            std::find(newJunction->m_nodes.begin(), newJunction->m_nodes.end(),
                      it_junction->m_startNode);
        assert(it_startNode != newJunction->m_nodes.end());
        ++it_startNode;
        newJunction->m_nodes.insert(it_startNode, it_endNode,
                                    it_junction->m_junction->m_nodes.end());
      } else {
        ++it_endNode;
        newJunction->m_nodes.insert(
            it_startNode, it_junction->m_junction->m_nodes.begin(), it_endNode);
        it_startNode =
            std::find(newJunction->m_nodes.begin(), newJunction->m_nodes.end(),
                      it_junction->m_startNode);
        assert(it_startNode != newJunction->m_nodes.end());
        newJunction->m_nodes.insert(it_startNode, it_endNode,
                                    it_junction->m_junction->m_nodes.end());
      }
    }

    std::vector<ProtoStroke *>::iterator it_protoStroke;
    for (it_protoStroke = it_junction->m_junction->m_protoStrokes.begin();
         it_protoStroke != it_junction->m_junction->m_protoStrokes.end();
         ++it_protoStroke) {
      if ((*it_protoStroke)->m_startJunction == it_junction->m_junction) {
        (*it_protoStroke)->m_startJunction = newJunction;
        newJunction->m_protoStrokes.push_back(*it_protoStroke);
      }
      if ((*it_protoStroke)->m_endJunction == it_junction->m_junction) {
        (*it_protoStroke)->m_endJunction = newJunction;
        newJunction->m_protoStrokes.push_back(*it_protoStroke);
      }
    }

    std::vector<JunctionLink>::iterator it_links;
    for (it_links = m_links.begin(); it_links != m_links.end(); ++it_links) {
      if (it_links->m_first == it_junction->m_junction)
        it_links->m_first = newJunction;
      if (it_links->m_second == it_junction->m_junction)
        it_links->m_second = newJunction;
    }

    // erase old junction
    std::vector<Junction *>::iterator it;
    it = std::find(m_junctions.begin(), m_junctions.end(),
                   it_junction->m_junction);
    assert(it != m_junctions.end());
    delete it_junction->m_junction;
    m_junctions.erase(it);
  }
  double size           = junctions.size();
  newJunction->m_center = newJunction->m_center * (1 / size);

  std::vector<ProtoStroke *>::iterator it_protoStroke;
  for (it_protoStroke = newJunction->m_protoStrokes.begin();
       it_protoStroke != newJunction->m_protoStrokes.end(); ++it_protoStroke) {
    if ((*it_protoStroke)->m_points.size() < 6 &&
        (*it_protoStroke)->m_startJunction == newJunction &&
        (*it_protoStroke)->m_endJunction == newJunction) {
      (*it_protoStroke)->m_points.clear();
      (*it_protoStroke)->m_startJunction = (*it_protoStroke)->m_endJunction = 0;

      ProtoStroke *protoStroke = *it_protoStroke;
      for (;;) {
        std::vector<ProtoStroke *>::iterator it;
        it = std::find(newJunction->m_protoStrokes.begin(),
                       newJunction->m_protoStrokes.end(), protoStroke);
        if (it == newJunction->m_protoStrokes.end()) break;
        newJunction->m_protoStrokes.erase(it);
      }
      if (newJunction->m_protoStrokes.empty()) break;
      it_protoStroke = newJunction->m_protoStrokes.begin();
    }
  }

  return newJunction;
}

//---------------------------------------------------------

void CenterLineVectorizer::joinJunctions() {
  for (int i = 0; i < (int)m_junctions.size(); i++) {
    Junction *currJunction = m_junctions[i];

    //		if(currJunction->m_nodes.size() <= 2) continue;

    std::list<JunctionMerge> joinJunctions;
    joinJunctions.push_back(JunctionMerge(currJunction));

    //		std::sort(currJunction->m_nodes.begin(),
    // currJunction->m_nodes.end());
    std::deque<Node *>::iterator it;
    for (it = currJunction->m_nodes.begin(); it != currJunction->m_nodes.end();
         it++) {
      Node *currNode = *it;
      Node *prevNode = *it;
      Node *nextNode = *it;
      std::vector<JunctionLink>::iterator it_link;
      int j;
      for (j = 0; true; ++j) {
        prevNode            = prevNode->m_prev;
        prevNode->m_visited = true;
        if (prevNode->m_junction != 0 && prevNode->m_junction != currJunction) {
          // double junctionMaxDist = 3.5*currJunction->m_center.thick;
          it_link = std::find(m_links.begin(), m_links.end(),
                              JunctionLink(currJunction, prevNode->m_junction));
          /*					if(	j >= 1*SEARCH_WINDOW &&
						norm(currJunction->m_center - prevNode->m_junction->m_center) >= 
junctionMaxDist)
					{
						assert(false);
						if(it_link == m_links.end())
							m_links.push_back(JunctionLink(currJunction, prevNode->m_junction));
						else
							++(it_link->m_order);
					}
					else*/ if (it_link == m_links.end() &&
                                                   std::find(
                                                       joinJunctions.begin(),
                                                       joinJunctions.end(),
                                                       JunctionMerge(
                                                           prevNode
                                                               ->m_junction)) ==
                                                       joinJunctions.end()) {
            joinJunctions.push_back(
                JunctionMerge(prevNode->m_junction, currNode, prevNode, false));
          }
        }
        if (prevNode->m_visited) break;
      }
      for (j = 0; true; ++j) {
        nextNode            = nextNode->m_next;
        prevNode->m_visited = true;
        if (nextNode->m_junction != 0 && nextNode->m_junction != currJunction) {
          // double junctionMaxDist = 3.5*currJunction->m_center.thick;
          it_link = std::find(m_links.begin(), m_links.end(),
                              JunctionLink(currJunction, nextNode->m_junction));
          /*					if(	j >= 1*SEARCH_WINDOW &&
						norm(currJunction->m_center - nextNode->m_junction->m_center) >= 
junctionMaxDist )
					{
						assert(false);
						if(it_link == m_links.end())
							m_links.push_back(JunctionLink(currJunction, nextNode->m_junction));
						else
							++(it_link->m_order);
					}
					else*/ if (it_link == m_links.end() &&
                                                   std::find(
                                                       joinJunctions.begin(),
                                                       joinJunctions.end(),
                                                       JunctionMerge(
                                                           nextNode
                                                               ->m_junction)) ==
                                                       joinJunctions.end()) {
            joinJunctions.push_back(
                JunctionMerge(nextNode->m_junction, currNode, nextNode, true));
          }
        }
        if (nextNode->m_visited) break;
      }
    }
    if (joinJunctions.size() > 1) {
      // erase duplicate junctions
      //			joinJunctions.sort();
      //			std::sort(joinJunctions.begin(),
      // joinJunctions.end());
      //			std::list<JunctionMerge>::iterator it_newEnd;
      //			it_newEnd = std::unique(joinJunctions.begin(),
      // joinJunctions.end());
      //			assert(it_newEnd == joinJunctions.end());
      //			joinJunctions.erase(it_newEnd,
      // joinJunctions.end());

      Junction *newJunction = mergeJunctions(joinJunctions);
      assert(newJunction);
      m_junctions.push_back(newJunction);
    }
    joinJunctions.clear();
  }
}

//---------------------------------------------------------
/*
void CenterLineVectorizer::resolveUnvisitedNode(Node *node)
{
        assert(node->m_visited == false);
        assert(node->m_junction == 0);

        JunctionLink link(0,0);

        Node* currNode = node;
        for(int i=0; i<4*SEARCH_WINDOW; i++)
        {
                currNode = currNode->m_next;
                assert(currNode);
                if(currNode->m_junction)
                {
                        link.m_first = currNode->m_junction;
                        break;
                }
                if(currNode->m_visited) break;
        }

        if(!link.m_first) return;

        int steps = 0;
        for(i=0; i<4*SEARCH_WINDOW; i++)
        {
                currNode = currNode->m_prev;
                assert(currNode);
                if(currNode->m_junction)
                {
                        link.m_second = currNode->m_junction;
                        break;
                }
                if(currNode->m_visited) break;
                currNode->m_visited = true;
                ++steps;
        }

        if(	(link.m_first) && (link.m_second) && (link.m_first !=
link.m_second)
                && steps >= 2*SEARCH_WINDOW && norm(link.m_first->m_center -
link.m_second->m_center) > 2.5 &&
                std::find(m_links.begin(), m_links.end(), link) == m_links.end()
&&
                std::find(m_links.begin(), m_links.end(),
JunctionLink(link.m_second,
link.m_first) ) == m_links.end() )
        {
                m_links.push_back(link);
        }
        else
        {
                for(int i=0; i<4*SEARCH_WINDOW; i++)
                {
                        currNode = currNode->m_next;
                        assert(currNode);
                        if(currNode->m_junction) break;
                        currNode->m_visited = false;
                }
        }
}
*/
//---------------------------------------------------------

void CenterLineVectorizer::createJunctionPolygon(Junction *junction) {
  // int size = junction->m_nodes.size();
  //	std::sort(junction->m_nodes.begin(), junction->m_nodes.end(),
  // CompareJunctionNodes(junction->m_center));
  //	std::reverse(junction->m_nodes.begin(), junction->m_nodes.end());
  // std::deque<Node*>::iterator it, it_temp;
  std::deque<Node *>::iterator it, it_temp;
  for (it = junction->m_nodes.begin(); it != junction->m_nodes.end(); it++) {
    int step = 0;

    Node *currNode;
    Node *otherNode;

    if (it + 1 == junction->m_nodes.end())
      otherNode = *junction->m_nodes.begin();
    else
      otherNode = *(it + 1);

    /*		currNode = (*it)->m_prev;
    if(!currNode->m_visited)
    {
            step=0;
            while(!currNode->m_visited && currNode->m_junction != junction)
            {
                    it = junction->m_nodes.insert(it+1, currNode);
                    currNode->m_visited = true;
                    currNode = currNode->m_prev;
                    ++step;
            }
            if(currNode->m_junction != junction)
                    for(int i=0; i<step; ++i)
                    {
                            currNode = currNode->m_next;
                            currNode->m_visited = false;
                            it = junction->m_nodes.erase(it);
                            --it;
                    }
            else
            {
                    if(currNode != otherNode)
                    {
                            it_temp = std::find(junction->m_nodes.begin(),
junction->m_nodes.end(),
currNode);
                            if(it+1 == junction->m_nodes.end())
                                    std::swap(*junction->m_nodes.begin(),
*it_temp);
                            else
                                    std::swap(*(it+1), *it_temp);
                    }
                    continue;
            }
    }
*/
    currNode = (*it)->m_next;
    if (!currNode->m_visited) {
      step = 0;
      while (!currNode->m_visited && currNode->m_junction != junction) {
        it                  = junction->m_nodes.insert(it + 1, currNode);
        currNode->m_visited = true;
        currNode            = currNode->m_next;
        ++step;
      }
      if (currNode->m_junction != junction)  //|| currNode != otherNode)
        for (int i = 0; i < step; ++i) {
          currNode            = currNode->m_prev;
          currNode->m_visited = false;
          it                  = junction->m_nodes.erase(it);
          --it;
        }
      else {
        //				assert(*(it+1) == otherNode || (it+1 ==
        // junction->m_nodes.end() && *junction->m_nodes.begin() == otherNode)
        // );
        if (currNode != otherNode) {
          Node tempNode = *otherNode;
          *otherNode    = *currNode;
          *currNode     = tempNode;
          //					it_temp =
          // std::find(junction->m_nodes.begin(),
          // junction->m_nodes.end(), currNode);
          //					assert(it_temp !=
          // junction->m_nodes.end());
          //					if(it+1 ==
          // junction->m_nodes.end())
          //						std::swap(*junction->m_nodes.begin(),
          //*it_temp);
          //					else
          //						std::swap(*(it+1),
          //*it_temp);
        }
        continue;
      }
    }
  }
}

//---------------------------------------------------------

void CenterLineVectorizer::handleLinks() {
  std::vector<JunctionLink>::iterator it_links;
  for (it_links = m_links.begin(); it_links != m_links.end(); ++it_links) {
    ProtoStroke linkingProtoStroke;

    // initialize points
    TThickPoint p1 = it_links->m_first->m_center;
    TThickPoint p2 = it_links->m_second->m_center - p1;
    linkingProtoStroke.m_points.push_back(
        TThickPoint(p1, JUNCTION_THICKNESS_RATIO * p1.thick));
    linkingProtoStroke.m_points.push_back(TThickPoint(
        p1 + 0.5 * p2, JUNCTION_THICKNESS_RATIO * (p1.thick + 0.5 * p2.thick)));
    linkingProtoStroke.m_points.push_back(
        TThickPoint(p1 + p2, JUNCTION_THICKNESS_RATIO * (p1.thick + p2.thick)));

    // initialize directions
    setEndpointsDirections(linkingProtoStroke);

    // initialize junctions
    /*		linkingProtoStroke.m_startJunction = it_links->first;
    linkingProtoStroke.m_endJunction = it_links->second;*/
    m_protoStrokes.push_back(linkingProtoStroke);
    /*		it_links->first->m_protoStrokes.push_back(&m_protoStrokes.back());
    it_links->second->m_protoStrokes.push_back(&m_protoStrokes.back());*/
  }
}

//---------------------------------------------------------

bool isFalseBranch(int k, ProtoStroke *protoStroke, int recursionOrder,
                   TThickPoint &edge) {
  if (recursionOrder > 3) return false;
  ++recursionOrder;
  if (k == 0) {
    if (protoStroke->m_points.front().thick > 3.0) return false;
    if (protoStroke->m_points.size() <= 20) {
      if (protoStroke->m_endJunction) {
        if (protoStroke->m_endJunction->m_protoStrokes.size() == 1) {
          edge = protoStroke->m_points.back();
          protoStroke->m_endJunction->m_protoStrokes.clear();
          protoStroke->m_points.clear();
          protoStroke->m_startJunction = 0;
          protoStroke->m_endJunction   = 0;
          return true;
        } else if (protoStroke->m_endJunction->m_protoStrokes.size() == 2) {
          ProtoStroke *nextProtoStroke;
          int next_k;

          if (protoStroke->m_endJunction->m_protoStrokes[0] == protoStroke)
            nextProtoStroke = protoStroke->m_endJunction->m_protoStrokes[1];
          else if (protoStroke->m_endJunction->m_protoStrokes[1] == protoStroke)
            nextProtoStroke = protoStroke->m_endJunction->m_protoStrokes[0];
          if (nextProtoStroke->m_startJunction == protoStroke->m_endJunction)
            next_k = 0;
          else if (nextProtoStroke->m_endJunction == protoStroke->m_endJunction)
            next_k = 1;

          if (isFalseBranch(next_k, nextProtoStroke, recursionOrder, edge)) {
            protoStroke->m_endJunction->m_protoStrokes.clear();
            protoStroke->m_points.clear();
            protoStroke->m_startJunction = 0;
            protoStroke->m_endJunction   = 0;
            return true;
          } else
            return false;
        } else
          return false;
      }
      edge = protoStroke->m_points.back();
      protoStroke->m_points.clear();
      protoStroke->m_startJunction = 0;
      protoStroke->m_endJunction   = 0;
      return true;
    }
  } else if (k == 1) {
    if (protoStroke->m_points.back().thick > 3.0) return false;
    if (protoStroke->m_points.size() <= 20) {
      if (protoStroke->m_startJunction) {
        if (protoStroke->m_startJunction->m_protoStrokes.size() == 1) {
          edge = protoStroke->m_points.front();
          protoStroke->m_startJunction->m_protoStrokes.clear();
          protoStroke->m_points.clear();
          protoStroke->m_startJunction = 0;
          protoStroke->m_endJunction   = 0;
          return true;
        } else if (protoStroke->m_startJunction->m_protoStrokes.size() == 2) {
          ProtoStroke *nextProtoStroke;
          int next_k;

          if (protoStroke->m_startJunction->m_protoStrokes[0] == protoStroke)
            nextProtoStroke = protoStroke->m_startJunction->m_protoStrokes[1];
          else if (protoStroke->m_startJunction->m_protoStrokes[1] ==
                   protoStroke)
            nextProtoStroke = protoStroke->m_startJunction->m_protoStrokes[0];
          if (nextProtoStroke->m_startJunction == protoStroke->m_startJunction)
            next_k = 0;
          else if (nextProtoStroke->m_endJunction ==
                   protoStroke->m_startJunction)
            next_k = 1;

          if (isFalseBranch(next_k, nextProtoStroke, recursionOrder, edge)) {
            protoStroke->m_startJunction->m_protoStrokes.clear();
            protoStroke->m_points.clear();
            protoStroke->m_startJunction = 0;
            protoStroke->m_endJunction   = 0;
            return true;
          } else
            return false;
        } else
          return false;
      }
      edge = protoStroke->m_points.front();
      protoStroke->m_points.clear();
      protoStroke->m_startJunction = 0;
      protoStroke->m_endJunction   = 0;
      return true;
    }
  }
  return false;
}

//---------------------------------------------------------

void CenterLineVectorizer::handleJunctions() {
  std::vector<Junction *>::iterator it_junction = m_junctions.begin();
  for (it_junction = m_junctions.begin(); it_junction < m_junctions.end();
       it_junction++) {
    Junction *currJunction = *it_junction;

    if (currJunction->m_protoStrokes.empty()) {
      std::deque<Node *>::iterator it_node;
      for (it_node = currJunction->m_nodes.begin();
           it_node != currJunction->m_nodes.end(); ++it_node) {
        (*it_node)->m_junction = 0;
        (*it_node)->m_visited  = false;
      }
      delete currJunction;
      it_junction = m_junctions.erase(it_junction);
      --it_junction;
      continue;
    }

    std::vector<ProtoStroke *>::iterator it;

    // set polygon center
    /*		double size = currJunction->m_nodes.size();
    std::deque<Node*>::iterator it_node;
    for(it_node = currJunction->m_nodes.begin();
it_node!=currJunction->m_nodes.end(); it_node++)
    {
            currJunction->m_center += convert( (*it_node)->m_pixel->m_pos );
    }
    currJunction->m_center = currJunction->m_center * (1/size);

    size = currJunction->m_protoStrokes.size();
    if(size == 0)
    {
            currJunction->m_center.thick = THICKNESS_LIMIT;
            continue;
    }
*/
    double size = currJunction->m_protoStrokes.size();
    for (it = currJunction->m_protoStrokes.begin();
         it != currJunction->m_protoStrokes.end(); it++) {
      if (((*it))->m_startJunction == currJunction &&
          (*it)->m_endJunction == currJunction) {
        currJunction->m_center += 0.5 * (*it)->m_points.front();
        currJunction->m_center += 0.5 * (*it)->m_points.back();
      } else if (((*it))->m_startJunction == currJunction) {
        currJunction->m_center += (*it)->m_points.front();
      } else if ((*it)->m_endJunction == currJunction) {
        currJunction->m_center += (*it)->m_points.back();
      }
    }
    currJunction->m_center = currJunction->m_center * (1 / size);
  }

#ifdef DEBUG
  int j = 0;
  for (; j < (int)m_nodes.size(); j++) {
    Node *node = m_nodes[j];
    if (!node->m_visited && node->m_pixel) {
      m_unvisitedNodes.push_back(convert(node->m_pixel->m_pos));
      //			resolveUnvisitedNode(node);
    }
  }

  for (j = 0; j < (int)m_nodes.size(); j++) {
    Node *node = m_nodes[j];
    //		assert(node->m_visited || !node->m_pixel);
    if (node->m_pixel == 0 || node->m_flag) continue;
    outputNodes(node);
  }
#endif

  joinJunctions();
  //	return;
  for (it_junction = m_junctions.begin(); it_junction < m_junctions.end();
       it_junction++) {
    Junction *currJunction = *it_junction;
    if (currJunction->m_center.thick < 2 * THICKNESS_LIMIT)
      currJunction->m_center.thick = 2 * THICKNESS_LIMIT;

    std::vector<ProtoStroke *>::iterator it, it_next;

    createJunctionPolygon(currJunction);

    currJunction->m_junctionOrder = currJunction->m_protoStrokes.size();

    if (currJunction->m_protoStrokes.size() == 3) {
      std::vector<ProtoStroke *>::iterator it_e =
          currJunction->m_protoStrokes.end();
      std::vector<ProtoStroke *>::iterator it_candidate1 = it_e,
                                           it_candidate2 = it_e;
      int candidate1_k, candidate2_k;
      double candidateProbability = JOIN_LIMIT;
      int k, next_k;
      for (it = currJunction->m_protoStrokes.begin(); it != it_e; it++) {
        if (((*it))->m_startJunction == currJunction)
          k = 0;
        else if ((*it)->m_endJunction == currJunction)
          k = 1;
        else
          assert(false);

        if (isFalseBranch(k, *it, 0, currJunction->m_center)) {
          currJunction->m_protoStrokes.erase(it);
          currJunction->m_locked = true;
          break;
        }

        it_next = it;
        it_next++;
        for (; it_next != it_e; it_next++) {
          double probability;
          if (((*it_next))->m_startJunction == currJunction)
            next_k = 0;
          else if ((*it_next)->m_endJunction == currJunction)
            next_k = 1;
          else
            assert(false);

          if (k == 0) {
            if (next_k == 0)
              probability = 1 - norm2((*it)->m_startDirection +
                                      (*it_next)->m_startDirection);
            else
              probability = 1 - norm2((*it)->m_startDirection +
                                      (*it_next)->m_endDirection);
          } else {
            if (next_k == 0)
              probability = 1 - norm2((*it)->m_endDirection +
                                      (*it_next)->m_startDirection);
            else
              probability =
                  1 - norm2((*it)->m_endDirection + (*it_next)->m_endDirection);
          }
          if (probability > candidateProbability ||
              (*it)->m_points.size() < 3) {
            candidateProbability = probability;
            it_candidate1        = it;
            it_candidate2        = it_next;
            candidate1_k         = k;
            candidate2_k         = next_k;
          }
        }
      }
      if (currJunction->m_protoStrokes.size() == 3 && it_candidate1 != it_e &&
          it_candidate2 != it_e) {
        TThickPoint p1 = (candidate1_k == 0)
                             ? (*it_candidate1)->m_points.front()
                             : (*it_candidate1)->m_points.back();
        TThickPoint p2 = (candidate2_k == 0)
                             ? (*it_candidate2)->m_points.front()
                             : (*it_candidate2)->m_points.back();
        currJunction->m_center = 0.5 * (p1 + p2);
      }
    }
    if (false || currJunction->m_protoStrokes.size() == 2) {
      TPointD a1, a2, b1, b2;

      if (currJunction->m_protoStrokes[0]->m_startJunction == currJunction) {
        a1 = currJunction->m_protoStrokes[0]->m_points.front();
        a2 = currJunction->m_protoStrokes[0]->m_startDirection - a1;
      } else {
        assert(currJunction->m_protoStrokes[0]->m_endJunction == currJunction);
        a1 = currJunction->m_protoStrokes[0]->m_points.back();
        a2 = currJunction->m_protoStrokes[0]->m_endDirection - a1;
      }

      if (currJunction->m_protoStrokes[1]->m_startJunction == currJunction) {
        b1 = currJunction->m_protoStrokes[1]->m_points.front();
        b2 = currJunction->m_protoStrokes[1]->m_startDirection - b1;
      } else {
        assert(currJunction->m_protoStrokes[1]->m_endJunction == currJunction);
        b1 = currJunction->m_protoStrokes[1]->m_points.back();
        b2 = currJunction->m_protoStrokes[1]->m_endDirection - b1;
      }

/*			TPointD intersection;
			RectIntersectionResult res;
			res = rectsIntersect(a1, a2, b1, b2, intersection);
//			assert(res == intersected);
			if( norm(intersection - currJunction->m_center) < 5 && res == 
intersected)
			{
				currJunction->m_center.x = intersection.x;
				currJunction->m_center.y = intersection.y;
			}
*/		}

if (true || currJunction->m_nodes.size() <= 12 || currJunction->isConvex()) {
  TThickPoint junctionPoint(currJunction->m_center);
  for (it = currJunction->m_protoStrokes.begin();
       it != currJunction->m_protoStrokes.end(); it++) {
    assert((*it)->m_startJunction == currJunction ||
           (*it)->m_endJunction == currJunction);
    if ((*it)->m_startJunction == currJunction) {
      TPointD deltaPoint = (*it)->m_points.front() - junctionPoint;
      double deltaThick  = (*it)->m_points.front().thick - junctionPoint.thick;
      if (norm2(deltaPoint) > 0.2) {
        if (norm2(deltaPoint) > 2) {
          (*it)->m_points.push_front(
              TThickPoint(junctionPoint + 0.7 * deltaPoint,
                          junctionPoint.thick + 0.7 * deltaThick));
          (*it)->m_points.push_front(
              TThickPoint(junctionPoint + 0.4 * deltaPoint,
                          junctionPoint.thick + 0.4 * deltaThick));
        }
        (*it)->m_points.push_front(
            TThickPoint(junctionPoint + 0.0 * deltaPoint,
                        junctionPoint.thick + 0.0 * deltaThick));
      }
    }
    if ((*it)->m_endJunction == currJunction) {
      TPointD deltaPoint = (*it)->m_points.back() - junctionPoint;
      double deltaThick  = (*it)->m_points.back().thick - junctionPoint.thick;
      if (norm2(deltaPoint) > 0.2) {
        if (norm2(deltaPoint) > 2) {
          (*it)->m_points.push_back(
              TThickPoint(junctionPoint + 0.7 * deltaPoint,
                          junctionPoint.thick + 0.7 * deltaThick));
          (*it)->m_points.push_back(
              TThickPoint(junctionPoint + 0.4 * deltaPoint,
                          junctionPoint.thick + 0.4 * deltaThick));
        }
        (*it)->m_points.push_back(
            TThickPoint(junctionPoint + 0.0 * deltaPoint,
                        junctionPoint.thick + 0.0 * deltaThick));
      }
    }
  }
} else {
  ProtoRegion region(currJunction->isConvex());
  region.m_center = currJunction->m_center;

  std::deque<Node *>::iterator it_nodes;
  for (it_nodes = currJunction->m_nodes.begin();
       it_nodes != currJunction->m_nodes.end(); it_nodes++)
    region.m_points.push_back(
        TThickPoint(convert((*it_nodes)->m_pixel->m_pos), 0.0));

  region.m_points.push_back(TThickPoint(
      convert((*currJunction->m_nodes.begin())->m_pixel->m_pos), 0.0));

  m_protoRegions.push_back(region);

  for (it = currJunction->m_protoStrokes.begin();
       it != currJunction->m_protoStrokes.end(); it++) {
    if ((*it)->m_startJunction == currJunction &&
        (*it)->m_endJunction == currJunction) {
      if ((*it)->m_points.size() < 10) (*it)->m_points.clear();
    }

    if ((*it)->m_startJunction == currJunction) {
      (*it)->m_startJunction = 0;
    }
    if ((*it)->m_endJunction == currJunction) {
      (*it)->m_endJunction = 0;
    }
  }
  currJunction->m_protoStrokes.clear();
}
  }

  // handleLinks();
}

//---------------------------------------------------------

void CenterLineVectorizer::createStrokes() {
  std::list<ProtoStroke>::iterator it_centerlines = m_protoStrokes.begin();
  for (; it_centerlines != m_protoStrokes.end(); it_centerlines++) {
    if (!it_centerlines->m_points.empty()) {
      joinProtoStrokes(&(*it_centerlines));
      if (it_centerlines->m_points.size() <= 1) continue;

      std::vector<TThickPoint> points;
      std::deque<TThickPoint>::iterator it;

      if (m_configuration.m_smoothness > 0)  // gmt: prima era true
      {
        // gmt, 30-3-2006:
        /*
        it = it_centerlines->m_points.begin()+1;
        for(;;)
        {
                if( it >=
        it_centerlines->m_points.end()-(m_configuration.m_smoothness+1)) break;
                for(int j=0; j<m_configuration.m_smoothness;j++)
                        it = it_centerlines->m_points.erase(it);
                ++it;
        }
*/
        // gmt. credo che l'idea sia di lasciare un punto e
        // toglierne m_configuration.m_smoothness e cosi' via fino alla fine
        // n.b. ho modificato il comportamento sugli eventuali ultimi punti
        // (<m_configuration.m_smoothness) che prima venivano lasciati e adesso
        // vengono tolti
        std::deque<TThickPoint> &points = it_centerlines->m_points;
        it                              = points.begin();
        for (;;) {
          if (it == points.end()) break;
          ++it;
          int maxD = std::distance(it, points.end());
          int d    = tmin(maxD, m_configuration.m_smoothness);
          if (d == 0) break;
          it = points.erase(it, it + d);
        }
      }
      it = it_centerlines->m_points.begin();
      points.push_back(*it);
      assert(it->thick >= THICKNESS_LIMIT);
      TThickPoint old = *it;
      for (it++; it != it_centerlines->m_points.end(); ++it) {
        assert(it->thick >= THICKNESS_LIMIT);
        TThickPoint point(0.5 * (*it + old), 0.5 * (it->thick + old.thick));
        points.push_back(point);
        old = *it;
      }
      points.push_back(it_centerlines->m_points.back());

      int n = points.size();
      if (n > 5) {
        double *rr = new double[n];
        int i;
        for (i = 0; i < n; i++) rr[i] = points[i].thick;
        for (i            = 2; i < n - 2; i++)
          points[i].thick = 0.8 * (1 / 5.0) * (/*rr[i-4]+rr[i-3]*/ +rr[i - 2] +
                                               rr[i - 1] + rr[i] + rr[i + 1] +
                                               rr[i + 2] /*+rr[i+3]+rr[i+4]*/);
        points[0].thick = points[1].thick =
            points[2].thick;  //=points[3].thick;//=points[4].thick;
        points[n - 1].thick = points[n - 2].thick =
            points[n - 3].thick;  //=points[n-4].thick;//=points[n-5].thick;
        delete[] rr;
      }
      TStroke *stroke =
          TStroke::interpolate(points, m_configuration.m_interpolationError);
      stroke->setStyle(m_configuration.m_strokeStyleId);  // FirstUserStyle+2);
      m_vimage->addStroke(stroke);
    }
  }
}

//---------------------------------------------------------

void CenterLineVectorizer::createRegions() {
  std::list<ProtoRegion>::iterator it_regions;
  for (it_regions = m_protoRegions.begin(); it_regions != m_protoRegions.end();
       it_regions++) {
    assert(it_regions->m_points.size() > 4);
    TStroke *stroke = TStroke::interpolate(it_regions->m_points, 0.1);
    stroke->setSelfLoop();
    stroke->setStyle(m_configuration.m_strokeStyleId);
    m_vimage->addStroke(stroke);

    /*
    if(it_regions->m_isConvex)
            m_vimage->fill(it_regions->m_center, TPalette::DefaultStrokeStyle);
    else
    {
            TPointD internalPoint;
            internalPoint = rotate90(it_regions->m_points[1] -
it_regions->m_points[0]);
            assert(norm2(internalPoint) != 0);
            normalize(internalPoint);
            internalPoint = internalPoint*0.001 + 0.5*(it_regions->m_points[1] +
it_regions->m_points[0]);
            m_vimage->fill(internalPoint, TPalette::DefaultStrokeStyle);
    }*/
  }
}

//---------------------------------------------------------

void CenterLineVectorizer::vectorize() {
  int j = 0;

  for (j = 0; j < (int)m_nodes.size(); j++) {
    Node *node = m_nodes[j];
    if (node->m_pixel == 0 || node->m_visited) continue;
    traceLine(node->m_pixel);
  }

  handleJunctions();
  createStrokes();
  createRegions();
}

//---------------------------------------------------------

#ifdef DEBUG
void printTime(TStopWatch &sw, string name) {
  stringstream ss;
  ss << name << " : ";
  sw.print(ss);
  ss << '\n' << '\0';
  string s = ss.str();
  TSystem::outputDebug(s);
}
#endif

//---------------------------------------------------------

#ifdef DEBUG
void CenterLineVectorizer::outputInks() {
  for (int i = 0; i < m_dataRaster->getLx() * m_dataRaster->getLy(); i++) {
    DataPixel *pix = m_dataRaster->pixels() + i;
    if (pix->m_ink)
      getOutPix(pix) = TPixel32::Black;
    else
      getOutPix(pix) = TPixel32::White;
  }
}
#endif

//---------------------------------------------------------

TVectorImageP centerlineVectorize(const TImageP &image,
                                  const VectorizerConfiguration &c,
                                  TPalette *palette) {
  CenterLineVectorizer vectorizer(c, palette);

  TRasterImageP ri = image;
  TToonzImageP vi  = image;
  if (ri)
    vectorizer.makeDataRaster(ri->getRaster());
  else
    vectorizer.makeDataRaster(vi->getRaster());
  for (int i = 0; i < (int)vectorizer.m_dataRasterArray.size(); i++) {
    vectorizer.m_dataRaster = vectorizer.m_dataRasterArray[i].second;
    vectorizer.m_configuration.m_strokeStyleId =
        vectorizer.m_dataRasterArray[i].first;
    vectorizer.init();
    vectorizer.vectorize();
  }

  renormalizeImage(vectorizer.m_vimage.getPointer());

  return vectorizer.m_vimage;
}

//=============================================

class OutlineVectorizer : public CenterLineVectorizer {
public:
  list<vector<TThickPoint>> m_protoOutlines;

  void traceOutline(Node *initialNode);

  OutlineVectorizer(const VectorizerConfiguration &configuration,
                    TPalette *palette)
      : CenterLineVectorizer(configuration, palette) {}
  ~OutlineVectorizer();

  void createOutlineStrokes();

private:
  // not implemented
  OutlineVectorizer(const OutlineVectorizer &);
  OutlineVectorizer &operator=(const OutlineVectorizer &);
};

//---------------------------------------------------------

OutlineVectorizer::~OutlineVectorizer() { m_protoOutlines.clear(); }

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
    //		assert(startNode != initialNode);
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
    //		assert(node->m_visited || !node->m_pixel);
    if (node->m_pixel == 0 || node->m_flag) continue;
    outputNodes(node);
  }
#endif

  std::list<std::vector<TThickPoint>>::iterator it_outlines =
      m_protoOutlines.begin();
  for (; it_outlines != m_protoOutlines.end(); it_outlines++) {
    if (it_outlines->size() > 3) {
      std::vector<TThickPoint> points;
      std::vector<TThickPoint>::iterator it;

      if (it_outlines->size() > 10) {
        it = it_outlines->begin() + 1;
        for (;;) {
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

      /*TPointD internalPoint;
      internalPoint = rotate90(points[1] - points[0]);
      assert(norm2(internalPoint) != 0);
      normalize(internalPoint);
      internalPoint = internalPoint*0.001 + points[0];

      TPointD externalPoint;
      externalPoint = rotate270(points[1] - points[0]);
      assert(norm2(externalPoint) != 0);
      normalize(externalPoint);
      externalPoint = externalPoint*0.001 + points[0];*/
      TStroke *stroke =
          TStroke::interpolate(points, m_configuration.m_interpolationError);
      stroke->setStyle(m_configuration.m_strokeStyleId);
      stroke->setSelfLoop();
      m_vimage->addStroke(stroke);
      //			TStroke *stroke = TStroke::interpolate(points,
      // m_configuration.interpolationError);
      //     		stroke->setStyle(strokeStyleId);
      //			m_vimage->addStroke(stroke);//,
      // TPalette::DefaultStrokeStyle, TPalette::DefaultFillStyle);
      // internalPoints.push_back(internalPoint);
      // m_vimage->fill(externalPoint, TPalette::DefaultFillStyle);
    }
  }

  // m_vimage->autoFill(m_configuration.m_strokeStyleId);
  // for (UINT i=0; i<internalPoints.size(); i++)
  //  m_vimage->fill(internalPoints[i],fillStyleId);
}

//---------------------------------------------------------

TVectorImageP outlineVectorize(const TImageP &image,
                               const VectorizerConfiguration &configuration,
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
  for (int i = 0; i < (int)layersCount; i++) {
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

//---------------------------------------------------------
void applyFillColors(TRegion *r, const TRasterP &ras, TPalette *palette,
                     const TPoint &offset, bool leaveUnpainted) {
  TPointD pd;
  TRasterCM32P rt = (TRasterCM32P)ras;
  TRaster32P rr   = (TRaster32P)ras;
  TRasterGR8P rgr = (TRasterGR8P)ras;
  assert(rt || rr || rgr);

  if (/*r->getStyle()==0 && */ r->getInternalPoint(pd)) {
    TPoint p = convert(pd) + offset;
    if (ras->getBounds().contains(p)) {
      int styleId = 0;
      if (rt) {
        TPixelCM32 col = rt->pixels(p.y)[p.x];
        int tone       = col.getTone();
        if (tone < 100) {
          styleId                                      = col.getInk();
          if (styleId == 0 && !leaveUnpainted) styleId = col.getPaint();
        } else if (!leaveUnpainted || col.getPaint() == 0) {
          styleId                   = col.getPaint();
          if (styleId == 0) styleId = col.getInk();
        }
      } else {
        TPixel32 color;
        if (rr)
          color = rr->pixels(p.y)[p.x];
        else {
          int val = rgr->pixels(p.y)[p.x].value;
          color   = (val < 80) ? TPixel32::Black : TPixel32::White;
        }
        styleId = palette->getClosestStyle(color);
        if (palette->getStyle(styleId)->getMainColor() != color)
          styleId = palette->addStyle(color);
      }
      r->fill(pd, styleId);
    }
  }

  for (int i = 0; i < (int)r->getSubregionCount(); i++)
    applyFillColors(r->getSubregion(i), ras, palette, offset, leaveUnpainted);
}

//=========================================================

void applyFillColors(TVectorImageP vi, const TImageP &img, TPalette *palette,
                     bool leaveUnpainted) {
  TToonzImageP ti  = (TToonzImageP)img;
  TRasterImageP ri = (TRasterImageP)img;
  TRectD vb        = vi->getBBox();
  TRectD tb        = img->getBBox();

  TRasterP ras;

  if (ti)
    ras = ti->getRaster();
  else if (ri)
    ras = ri->getRaster();
  else
    assert(false);

  vi->findRegions();
  for (int i = 0; i < (int)vi->getRegionCount(); i++)
    applyFillColors(vi->getRegion(i), ras, palette,
                    convert(tb.getP00() - vb.getP00()), leaveUnpainted);
}

//=========================================================

TVectorImageP vectorize(const TImageP &img, const VectorizerConfiguration &c,
                        TPalette *plt) {
  TVectorImageP vi = c.m_outline ? outlineVectorize(img, c, plt)
                                 : centerlineVectorize(img, c, plt);

  applyFillColors(vi, img, plt, c.m_leaveUnpainted);

  // else if (c.m_outline)
  //  vi->autoFill(c.m_strokeStyleId);

  TPointD center;
  if (TToonzImageP ti = img)
    center = ti->getRaster()->getCenterD();
  else if (TRasterImageP ri = img)
    center = ri->getRaster()->getCenterD();

  TAffine aff = TTranslation(-center);
  vi->transform(aff);

  return vi;
}

//---------------------------------------------------------
