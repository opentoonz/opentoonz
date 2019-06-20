

#include "tflash.h"
//#include "tstroke.h"
#include "tcurves.h"
#include "tregion.h"
#include "tstrokeprop.h"
#include "tregionprop.h"

#include "tpalette.h"
#include "tvectorimage.h"
#include "tmachine.h"
#include "trasterimage.h"
#include "tsimplecolorstyles.h"
#include "tcolorfunctions.h"
#include "tsop.h"
#include "tropcm.h"
#include "tsweepboundary.h"
#include "tiio_jpg_util.h"
#include "zlib.h"
//#include "trop.h"
#include "ttoonzimage.h"
#include "tconvert.h"
#include "timage_io.h"
#include "tsystem.h"
#include <stack>
#include <fstream>

#if !defined(TNZ_LITTLE_ENDIAN)
TNZ_LITTLE_ENDIAN undefined !!
#endif

    int Tw = 0;

static bool areTwEqual(double x, double y) {
  assert(Tw != 0);

  return (int)(Tw * x) == (int)(Tw * y);
}

static bool areTwEqual(TPointD p0, TPointD p1) {
  assert(Tw != 0);

  return areTwEqual(p0.x, p1.x) && areTwEqual(p0.y, p1.y);
}
//-------------------------------------------------------------------

const std::wstring TFlash::ConstantLines = L"Low: Constant Thickness";
const std::wstring TFlash::MixedLines    = L"Medium: Mixed Thickness";
const std::wstring TFlash::VariableLines = L"High: Variable Thickness";

Tiio::SwfWriterProperties::SwfWriterProperties()
    : m_lineQuality("Curve Quality")
    , m_isCompressed("File Compression", true)
    , m_autoplay("Autoplay", true)
    , m_looping("Looping", true)
    , m_jpgQuality("Jpg Quality", 0, 100, 90)
    , m_url("URL", std::wstring())
    , m_preloader("Insert Preloader", false) {
  m_lineQuality.addValue(TFlash::MixedLines);
  m_lineQuality.addValue(TFlash::ConstantLines);
  m_lineQuality.addValue(TFlash::VariableLines);

  bind(m_lineQuality);
  bind(m_isCompressed);
  bind(m_autoplay);
  bind(m_looping);
  bind(m_jpgQuality);
  bind(m_url);
  bind(m_preloader);

  TEnumProperty::Range range = m_lineQuality.getRange();
}

//-------------------------------------------------------------------

enum PolyType {
  None,
  Centerline,
  Solid,
  Texture,
  LinearGradient,
  RadialGradient
};

class PolyStyle {
public:
  PolyType m_type;
  TPixel32 m_color1;     // only if type!=Texture
  TPixel32 m_color2;     // only if type==LinearGradient || type==RadialGradient
  double m_smooth;       // only if type==RadialGradient
  double m_thickness;    // only if type==Centerline
  TAffine m_matrix;      // only if type==Texture
  TRaster32P m_texture;  // only if type==Texture
  // bool m_isRegion;            //only if type!=Centerline
  // bool m_isHole;              //only if type!=Centerline && m_isRegion==true
  PolyStyle()
      : m_type(None)
      , m_color1()
      , m_color2()
      , m_smooth(0)
      , m_thickness(0)
      , m_matrix()
      , m_texture() /*, m_isRegion(false), m_isHole(false)*/ {}
  bool operator==(const PolyStyle &p) const;
  bool operator<(const PolyStyle &p) const;
};

class FlashPolyline {
public:
  UINT m_depth;
  bool m_skip;
  bool m_toBeDeleted;
  bool m_isPoint;
  std::vector<TQuadratic *> m_quads;
  PolyStyle m_fillStyle1;
  PolyStyle m_fillStyle2;
  PolyStyle m_lineStyle;
  // PolyStyle m_bgStyle;
  FlashPolyline()
      : m_depth(0)
      , m_skip(false)
      , m_toBeDeleted(false)
      , m_isPoint(false)
      , m_fillStyle1()
      , m_fillStyle2()
      , m_lineStyle() {}
  bool operator<(const FlashPolyline &p) const { return m_depth < p.m_depth; }
};

class biPoint {
public:
  TPointD p0, p1;

  biPoint(TPointD _p0, TPointD _p1) : p0(_p0), p1(_p1) {}
  biPoint() {}
  bool operator<(const biPoint &b) const {
    biPoint aux;
    aux.p0.x = areTwEqual(p0.x, b.p0.x) ? p0.x : b.p0.x;
    aux.p0.y = areTwEqual(p0.y, b.p0.y) ? p0.y : b.p0.y;
    aux.p1.x = areTwEqual(p1.x, b.p1.x) ? p1.x : b.p1.x;
    aux.p1.y = areTwEqual(p1.y, b.p1.y) ? p1.y : b.p1.y;

    return (p0.x == aux.p0.x)
               ? ((p0.y == aux.p0.y) ? ((p1.x == aux.p1.x) ? (p1.y < aux.p1.y)
                                                           : (p1.x < aux.p1.x))
                                     : (p0.y < aux.p0.y))
               : p0.x < aux.p0.x;
  }
  void revert() { std::swap(p0, p1); }
};

class wChunk {
public:
  double w0, w1;
  wChunk(double _w0, double _w1) : w0(_w0), w1(_w1) {}
  bool operator<(const wChunk &b) const { return (w1 < b.w0); }
};

//-------------------------------------------------------------------

const int c_soundRate        = 5512;  //  5512; //11025
const int c_soundBps         = 16;
const bool c_soundIsSigned   = false;
const int c_soundChannelNum  = 1;
const int c_soundCompression = 3;  // per compatibilita' con MAC!!!

//-------------------------------------------------------------------

class FlashImageData {
public:
  FlashImageData(TAffine aff, TImageP img, const TColorFunction *cf,
                 bool isMask, bool isMasked)
      : m_aff(aff)
      , m_img(img)
      , m_cf(cf)
      , m_isMask(isMask)
      , m_isMasked(isMasked) {
    assert(!isMask || !isMasked);
  }
  TAffine m_aff;
  const TColorFunction *m_cf;
  bool m_isMask, m_isMasked;
  TImageP m_img;
};

static double computeAverageThickness(const TStroke *s) {
  int count       = s->getControlPointCount();
  double resThick = 0;
  int i;

  for (i = 0; i < s->getControlPointCount(); i++) {
    double thick = s->getControlPoint(i).thick;
    if (i >= 2 && i < s->getControlPointCount() - 2) resThick += thick;
  }
  if (count < 6) return s->getControlPoint(count / 2 + 1).thick;

  return resThick / (s->getControlPointCount() - 4);
}

static void putquads(const TStroke *s, double w0, double w1,
                     std::vector<TQuadratic *> &quads) {
  int chunkIndex0, chunkIndex1, i;
  double dummy;
  bool ret;

  ret = s->getChunkAndT(w0, chunkIndex0, dummy);
  assert(!ret);
  ret = s->getChunkAndT(w1, chunkIndex1, dummy);
  assert(!ret);
  assert(chunkIndex0 <= chunkIndex1);

  for (i = chunkIndex0; i <= chunkIndex1; i++)
    quads.push_back((TQuadratic *)s->getChunk(i));
}

//-------------------------------------------------------------------

static void computeOutlineBoundary(std::vector<TStroke *> &outlines,
                                   std::list<FlashPolyline> &polylinesArray,
                                   const TPixel &color) {
  UINT size = polylinesArray.size();

  std::vector<std::vector<TQuadratic *>> quads;
  computeSweepBoundary(outlines, quads);

  outlines.clear();
  std::list<FlashPolyline>::iterator it = polylinesArray.begin();
  std::advance(it, size);
  for (int i = 0; i < (int)quads.size(); i++) {
    std::vector<TQuadratic *> &q = quads[i];

    polylinesArray.push_back(FlashPolyline());
    polylinesArray.back().m_quads               = quads[i];
    polylinesArray.back().m_toBeDeleted         = true;
    polylinesArray.back().m_fillStyle1.m_type   = Solid;
    polylinesArray.back().m_fillStyle1.m_color1 = color;
  }
}

//-------------------------------------------------------------------

//	TFlash::drawSegments and TFlash::drawquads cannot be inline defined
//	since size of TSegment and TQuadratic are unkown in the header

void TFlash::drawSegments(const std::vector<TSegment> segmentArray,
                          bool isGradientColor) {}

void TFlash::drawquads(const std::vector<TQuadratic> quadsArray) {}

//-------------------------------------------------------------------

bool PolyStyle::operator==(const PolyStyle &p) const {
  if (m_type != p.m_type) return false;

  switch (m_type) {
  case Centerline:
    return m_thickness == p.m_thickness && m_color1 == p.m_color1;
  case Solid:
    return m_color1 == p.m_color1;
  case Texture:
    return m_matrix == p.m_matrix &&
           m_texture.getPointer() == p.m_texture.getPointer();
  case LinearGradient:
  case RadialGradient:
    return m_color1 == p.m_color1 && m_color2 == p.m_color2 &&
           m_matrix == p.m_matrix && m_smooth == p.m_smooth;
  default:
    assert(false);
    return false;
  }
}

//-------------------------------------------------------------------

static bool affineMinorThen(const TAffine &m0, const TAffine &m1) {
  if (m0.a11 == m1.a11) {
    if (m0.a12 == m1.a12) {
      if (m0.a13 == m1.a13) {
        if (m0.a21 == m1.a21) {
          if (m0.a22 == m1.a22)
            return m0.a23 < m1.a23;
          else
            return m0.a22 < m1.a22;
        } else
          return m0.a21 < m1.a21;
      } else
        return m0.a13 < m1.a13;
    } else
      return m0.a12 < m1.a12;
  } else
    return m0.a11 < m1.a11;
}

//-------------------------------------------------------------------

bool PolyStyle::operator<(const PolyStyle &p) const {
  if (m_type == p.m_type) switch (m_type) {
    case Centerline:
      return (m_thickness == p.m_thickness) ? m_color1 < p.m_color1
                                            : m_thickness < p.m_thickness;
    case Solid:
      return m_color1 < p.m_color1;
    case Texture:
      return m_texture.getPointer() <
             p.m_texture.getPointer();  // ignoro la matrice!!!!
    case LinearGradient:
    case RadialGradient:
      return (m_smooth == p.m_smooth)
                 ? ((m_color1 == p.m_color1)
                        ? ((m_color2 == p.m_color2)
                               ? affineMinorThen(m_matrix, p.m_matrix)
                               : m_color2 < p.m_color2)
                        : m_color1 < p.m_color1)
                 : m_smooth < p.m_smooth;
    default:
      assert(false);
      return false;
    }
  else
    return m_type < p.m_type;
}

//-------------------------------------------------------------------

static void computeQuadChain(const TEdge &e,
                             std::vector<TQuadratic *> &quadArray,
                             std::vector<TQuadratic *> &toBeDeleted) {
  int chunk_b, chunk_e, chunk = -1;
  double t_b, t_e, w0, w1;
  TThickQuadratic *q_b = 0, *q_e = 0;
  TThickQuadratic dummy;
  bool reversed = false;

  if (e.m_w0 > e.m_w1) {
    reversed = true;
    w0       = e.m_w1;
    w1       = e.m_w0;
  } else {
    w0 = e.m_w0;
    w1 = e.m_w1;
  }

  if (w0 == 0.0)
    chunk_b = 0;
  else {
    if (e.m_s->getChunkAndT(w0, chunk, t_b)) assert(false);
    q_b = new TThickQuadratic();
    toBeDeleted.push_back(q_b);
    e.m_s->getChunk(chunk)->split(t_b, dummy, *q_b);
    chunk_b = chunk + 1;
  }

  if (w1 == 1.0)
    chunk_e = e.m_s->getChunkCount() - 1;
  else {
    if (e.m_s->getChunkAndT(w1, chunk_e, t_e)) assert(false);
    q_e = new TThickQuadratic();
    toBeDeleted.push_back(q_e);
    if (chunk_e == chunk) {
      if (q_b) {
        t_e = q_b->getT(e.m_s->getChunk(chunk)->getPoint(t_e));
        q_b->split(t_e, *q_e, dummy);
      } else
        e.m_s->getChunk(0)->split(t_e, *q_e, dummy);

      if (!reversed)
        quadArray.push_back(q_e);
      else {
        quadArray.push_back(
            new TQuadratic(q_e->getP2(), q_e->getP1(), q_e->getP0()));
        toBeDeleted.push_back(quadArray.back());
      }
      return;
    }
    e.m_s->getChunk(chunk_e)->split(t_e, *q_e, dummy);
    chunk_e--;
  }

  int i;
  assert(chunk_e >= chunk_b - 1);

  if (reversed) {
    if (q_e) {
      q_e->reverse();
      quadArray.push_back(q_e);
    }
    for (i = chunk_e; i >= chunk_b; i--) {
      const TThickQuadratic *qAux = e.m_s->getChunk(i);
      quadArray.push_back(
          new TQuadratic(qAux->getP2(), qAux->getP1(), qAux->getP0()));
      toBeDeleted.push_back(quadArray.back());
    }

    if (q_b) {
      q_b->reverse();
      quadArray.push_back(q_b);
    }
  } else {
    if (q_b) quadArray.push_back(q_b);
    for (i = chunk_b; i <= chunk_e; i++)
      quadArray.push_back((TQuadratic *)e.m_s->getChunk(i));
    if (q_e) quadArray.push_back(q_e);
  }
}

//-------------------------------------------------------------------
