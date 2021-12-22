
#include <cstring>

// TnzCore includes
#include "tsystem.h"
#include "tthread.h"
#include "tstopwatch.h"

#include "tvectorgl.h"
#include "tvectorrenderdata.h"
#include "tvectorimage.h"
#include "trasterimage.h"
#include "tlevel_io.h"
#include "tstrokeprop.h"
#include "ttessellator.h"
#include "tcolorfunctions.h"
#include "tofflinegl.h"
#include "drawutil.h"
#include "trop.h"
#include "tstencilcontrol.h"
#include "tpalette.h"
#include "tpixelutils.h"
#include "tregionprop.h"
#include "tcurves.h"
#include "tcurveutil.h"
#include "tstrokeoutline.h"
#include "tregion.h"

#include "tsimplecolorstyles.h"

#ifndef _WIN32
#define CALLBACK
#endif

//-----------------------------------------------------------------------------

TFilePath TRasterImagePatternStrokeStyle::m_rootDir = TFilePath();
TFilePath TVectorImagePatternStrokeStyle::m_rootDir = TFilePath();

//-----------------------------------------------------------------------------

#ifndef NDEBUG
static void checkGlError() {
  GLenum err = glGetError();
  assert(err != GL_INVALID_ENUM);
  assert(err != GL_INVALID_VALUE);
  assert(err != GL_INVALID_OPERATION);
  assert(err != GL_STACK_OVERFLOW);
  assert(err != GL_STACK_UNDERFLOW);
  assert(err != GL_OUT_OF_MEMORY);
  assert(err == GL_NO_ERROR);
}

#define CHECK_GL_ERROR checkGlError();
#else
#define CHECK_GL_ERROR
#endif

// Use GL_LINE_STRIP to debug the centerline-to-outline algorithm
#define QUAD_PRIMITIVE GL_QUAD_STRIP

//*************************************************************************************
//    Local namespace  stuff
//*************************************************************************************

namespace {

//-----------------------------------------------------------------------------

// se ras ha le dimensioni giuste (potenze di due) ritorna ras. Altrimenti
// crea il piu' piccolo raster con le dimensioni giuste che contenga ras e
// copia il contenuto (scalandolo)
// se il raster di partenza e' vuoto, non e' ras32 o e' piu' piccolo di 2x2
// ritorna un raster vuoto
TRaster32P makeTexture(const TRaster32P &ras) {
  if (!ras || ras->getLx() < 2 || ras->getLy() < 2) return TRaster32P();
  TRaster32P ras32 = ras;
  if (!ras32) return TRaster32P();
  TDimension d(2, 2);
  while (d.lx < 256 && d.lx * 2 <= ras32->getLx()) d.lx *= 2;
  while (d.ly < 256 && d.ly * 2 <= ras32->getLy()) d.ly *= 2;
  if (d == ras32->getSize())
    return ras32;
  else {
    TRaster32P texture(d);
    TScale sc((double)d.lx / ras32->getLx(), (double)d.ly / ras32->getLy());
    TRop::resample(texture, ras32, sc);
    return texture;
  }
}

//-----------------------------------------------------------------------------

class OutlineBuilder {
private:
  double width;
  int stage;
  double stages[3][3];
  double px, py, pz;
  double ax, ay;

  inline void next_stage(const double &x, const double &y, const double &z) {
    stages[stage][0] = x;
    stages[stage][1] = y;
    stages[stage][2] = z;
    ++stage;
  }

public:
  std::vector<double> vertices;

  explicit OutlineBuilder(double width, int count = 0)
      : width(width), stage(0), px(), py(), pz(), ax(), ay() {
    memset(stages, 0, sizeof(stages));
    vertices.reserve(12 * (count + 2));
  }

  void add(double x, double y, double z = 0.0) {
    const double maxl = 4.0;

    if (stage == 0)
      next_stage(x, y, z);
    else {
      double bx = x - px;
      double by = y - py;
      double lb = sqrt(bx * bx + by * by);
      if (lb < 1e-9) return;
      bx = width * bx / lb;
      by = width * by / lb;

      if (stage == 1)
        next_stage(x, y, z);
      else {
        if (stage == 2) next_stage(x, y, z);
        double l = fabs(ax + bx) > 1e-9
                       ? -(ay - by) / (ax + bx)
                       : fabs(ay + by) > 1e-9 ? (ax - bx) / (ay + by) : 0.0;
        if (fabs(l) > maxl || fabs(l) < 1.0 || l > 0.0) {
          vertices.resize(vertices.size() + 12);
          double *p = &vertices.back() - 11;
          p[0]      = px;
          p[1]      = py;
          p[2]      = pz;
          p[3]      = px + ay;
          p[4]      = py - ax;
          p[5]      = pz;
          p[6]      = px;
          p[7]      = py;
          p[8]      = pz;
          p[9]      = px + by;
          p[10]     = py - bx;
          p[11]     = pz;
        } else {
          vertices.resize(vertices.size() + 6);
          double *p = &vertices.back() - 5;
          p[0]      = px;
          p[1]      = py;
          p[2]      = pz;
          p[3]      = px - l * bx + by;
          p[4]      = py - l * by - bx;
          p[5]      = pz;
        }
      }

      ax = bx;
      ay = by;
    }

    px = x;
    py = y;
    pz = z;
  }

  void finish() {
    for (int i = 0; i < stage; ++i)
      add(stages[i][0], stages[i][1], stages[i][2]);
    stage = 0;
  }

  double get_width() const { return width; }

  void restart(double width) {
    this->width = width;
    stage       = 0;
    vertices.clear();
  }

  void invert() { restart(-width); }
};

void vector4_div(double *v) {
  double k = fabs(v[3]) > 1e-9 ? 1.0 / v[3] : 0.0;
  v[0] *= k, v[1] *= k, v[2] *= k;
  v[3] = 1.0;
}

void matrix4_x_vector4(double *dv, const double *m, const double *v) {
  dv[0] = m[0] * v[0] + m[4] * v[1] + m[8] * v[2] + m[12] * v[3];
  dv[1] = m[1] * v[0] + m[5] * v[1] + m[9] * v[2] + m[13] * v[3];
  dv[2] = m[2] * v[0] + m[6] * v[1] + m[10] * v[2] + m[14] * v[3];
  dv[3] = m[3] * v[0] + m[7] * v[1] + m[11] * v[2] + m[15] * v[3];
}

void matrix4_x_matrix4(double *dm, const double *ma, const double *mb) {
  matrix4_x_vector4(dm + 0, ma, mb + 0);
  matrix4_x_vector4(dm + 4, ma, mb + 4);
  matrix4_x_vector4(dm + 8, ma, mb + 8);
  matrix4_x_vector4(dm + 12, ma, mb + 12);
}

class AntialiasingOutlinePainter : public OutlineBuilder {
private:
  double matrix[16];
  double projection_matrix[16];
  double modelview_matrix[16];
  double anti_viewport_matrix[16];

public:
  explicit AntialiasingOutlinePainter(int count = 0) : OutlineBuilder(1.0, 0) {
    memset(matrix, 0, sizeof(matrix));
    memset(projection_matrix, 0, sizeof(projection_matrix));
    memset(modelview_matrix, 0, sizeof(modelview_matrix));
    memset(anti_viewport_matrix, 0, sizeof(anti_viewport_matrix));

    // read transformations
    double viewport[4] = {};
    glGetDoublev(GL_VIEWPORT, viewport);
    glGetDoublev(GL_PROJECTION_MATRIX, projection_matrix);
    glGetDoublev(GL_MODELVIEW_MATRIX, modelview_matrix);

    double viewport_matrix[16] = {};
    viewport_matrix[0]         = 0.5 * viewport[2];
    viewport_matrix[5]         = 0.5 * viewport[3];
    viewport_matrix[10]        = 1.0;
    viewport_matrix[12]        = 0.5 * viewport[2];
    viewport_matrix[13]        = 0.5 * viewport[3];
    viewport_matrix[15]        = 1.0;

    anti_viewport_matrix[0]  = 2.0 / viewport[2];
    anti_viewport_matrix[5]  = 2.0 / viewport[3];
    anti_viewport_matrix[10] = 1.0;
    anti_viewport_matrix[12] = -1.0;
    anti_viewport_matrix[13] = -1.0;
    anti_viewport_matrix[15] = 1.0;

    {
      double tmp[16] = {};
      matrix4_x_matrix4(tmp, projection_matrix, modelview_matrix);
      matrix4_x_matrix4(matrix, viewport_matrix, tmp);
    }
  }

  void add(double x, double y, double z = 0.0) {
    double dest[4], src[4] = {x, y, z, 1.0};
    matrix4_x_vector4(dest, matrix, src);
    vector4_div(dest);
    OutlineBuilder::add(dest[0], dest[1], dest[2]);
  }

  void finish() {
    OutlineBuilder::finish();
    if (vertices.empty()) return;
    int count = (int)vertices.size() / 6;

    // prepare colors
    float color[8] = {};
    glGetFloatv(GL_CURRENT_COLOR, color);
    memcpy(color + 4, color, 3 * sizeof(float));
    std::vector<float> colors(8 * count);
    for (float *c = &colors[0], *ce = &colors.back(); c < ce; c += 8)
      memcpy(c, color, sizeof(color));

    // draw
    glMatrixMode(GL_MODELVIEW);
    glLoadMatrixd(anti_viewport_matrix);
    glMatrixMode(GL_PROJECTION);
    glLoadIdentity();
    glEnableClientState(GL_VERTEX_ARRAY);
    glEnableClientState(GL_COLOR_ARRAY);

    glVertexPointer(3, GL_DOUBLE, 0, &vertices.front());
    glColorPointer(4, GL_FLOAT, 0, &colors.front());
    glDrawArrays(GL_TRIANGLE_STRIP, 0, 2 * count);

    glDisableClientState(GL_VERTEX_ARRAY);
    glDisableClientState(GL_COLOR_ARRAY);
    glLoadMatrixd(projection_matrix);
    glMatrixMode(GL_MODELVIEW);
    glLoadMatrixd(modelview_matrix);

    glColor4fv(color);
    restart(get_width());
  }
};

void drawAntialiasedOutline(const std::vector<TOutlinePoint> &_v,
                            const TStroke *stroke) {
  if (_v.empty()) return;

  const TOutlinePoint *begin = &_v.front(), *end = &_v.back();
  assert(_v.size() % 2 == 0);

  AntialiasingOutlinePainter outline(_v.size());
  for (const TOutlinePoint *i = begin; i < end; i += 2) outline.add(i->x, i->y);
  for (const TOutlinePoint *i = end; i > begin; i -= 2) outline.add(i->x, i->y);
  outline.finish();

  outline.invert();
  for (const TOutlinePoint *i = begin; i < end; i += 2) outline.add(i->x, i->y);
  for (const TOutlinePoint *i = end; i > begin; i -= 2) outline.add(i->x, i->y);
  outline.finish();
}

}  // namespace

//*************************************************************************************
//    DisplayListManager  definition
//*************************************************************************************

#ifdef _WIN32

namespace {

class DisplayListManager  // singleton
{
public:
  TThread::Mutex m_mutex;
  typedef HGLRC T_hGlContext;

  //----------------------------------------------

  struct Key {
    std::string m_name;
    TFrameId m_fid;
    T_hGlContext m_glContext;
    Key(std::string name, TFrameId fid, T_hGlContext glContext)
        : m_name(name), m_fid(fid), m_glContext(glContext) {}
  };

  //----------------------------------------------
  struct Info {
    GLuint m_displayListId;
    TColorFunction::Parameters m_parameters;
    HDC m_hdc;
    Info()
        : m_displayListId(0)
        , m_hdc(0)
        , m_parameters(TColorFunction::Parameters()) {}

    Info(GLuint displayListId, HDC hdc, TColorFunction::Parameters parameters)
        : m_displayListId(displayListId)
        , m_hdc(hdc)
        , m_parameters(parameters) {}

    // se ad una stessa chiave corrispondono due displayInfo diversi (second
    // questo metodo),
    // si aggiorna la displayList secondo l'ultima chiamata.
    // Serve per non duplicare troppe liste, per parametri che non cambiano
    // spesso, come le cf.
    bool isDrawnAs(const Info &linfo) {
      return (this->m_parameters.m_mR == linfo.m_parameters.m_mR &&
              this->m_parameters.m_mG == linfo.m_parameters.m_mG &&
              this->m_parameters.m_mB == linfo.m_parameters.m_mB &&
              this->m_parameters.m_mM == linfo.m_parameters.m_mM &&
              this->m_parameters.m_cR == linfo.m_parameters.m_cR &&
              this->m_parameters.m_cG == linfo.m_parameters.m_cG &&
              this->m_parameters.m_cB == linfo.m_parameters.m_cB &&
              this->m_parameters.m_cM == linfo.m_parameters.m_cM);
    }
  };
  //----------------------------------------------

  struct KeyLess final : public std::binary_function<Key, Key, bool> {
    bool operator()(const Key &d1, const Key &d2) const {
      return d1.m_glContext < d2.m_glContext ||
             (d1.m_glContext == d2.m_glContext &&
              (d1.m_fid < d2.m_fid ||
               d1.m_fid == d2.m_fid && d1.m_name < d2.m_name));
    }
  };

  //----------------------------------------------

  typedef std::map<Key, Info, KeyLess> DisplayListMap;

private:
  DisplayListMap m_displayListMap;

  DisplayListManager() {}

  void recordList(GLuint listId, TVectorImage *image,
                  const TVectorRenderData &rd) {
    QMutexLocker sl(&m_mutex);
    TRectD bbox = image->getBBox();
    double ry   = bbox.getLy() * 0.5;
    if (ry < 1e-5) ry = 1e-5;
    TAffine identity;
    TRect rect;
    glPushMatrix();

    glLoadIdentity();
    // forzo pixelSize come se si disegnasse su una stroke con thickness=10
    // tglMultMatrix(TScale(10.0/ry));

    // forzo pixelSize a 20.
    // In questo modo, un'immagine piu' grande (nel pli
    // che definisce il custom style) viene piu' definita di una piu' piccola,
    // anche se appaiono della stessa dimensione (che dipende esclusivamente
    // dalla thickness)
    // tglMultMatrix(TScale(0.50));

    TPalette *vPalette = image->getPalette();
    assert(vPalette);

    TVectorRenderData rd2(identity, rect, vPalette, rd.m_cf, true, true);
    rd2.m_isOfflineRender = rd.m_isOfflineRender;
    rd2.m_isImagePattern  = true;

    CHECK_GL_ERROR
    glNewList(listId, GL_COMPILE);
    CHECK_GL_ERROR
    tglDraw(rd2, image);
    CHECK_GL_ERROR
    glEndList();
    CHECK_GL_ERROR
    glPopMatrix();
    CHECK_GL_ERROR
  }

public:
  static DisplayListManager *instance() {
    static DisplayListManager singleton;
    return &singleton;
  }
  //--------------------------------------------

  GLuint getDisplayListId(TVectorImage *image, std::string name, TFrameId fid,
                          const TVectorRenderData &rd) {
    QMutexLocker sl(&m_mutex);
    TColorFunction::Parameters parameters;
    if (rd.m_cf) rd.m_cf->getParameters(parameters);
    CHECK_GL_ERROR

    HDC hDC               = wglGetCurrentDC();
    T_hGlContext hContext = wglGetCurrentContext();
    Info listInfo(0, hDC, parameters);
    Key key(name, fid, hContext);
    DisplayListMap::iterator it = m_displayListMap.find(key);
    if (it != m_displayListMap.end()) {
      if (listInfo.isDrawnAs(it->second)) {
        // lascia tutto com'e' ed utilizza la displaliList
        listInfo.m_displayListId = it->second.m_displayListId;
      } else {  // aggiorna la displayList con le nuove info (attualmente solo
                // la cf)
        listInfo.m_displayListId = it->second.m_displayListId;
        recordList(listInfo.m_displayListId, image, rd);
        m_displayListMap[key] = listInfo;
      }
    } else  // se non ho mai disegnato l'immagine creo la displayList
    {
      CHECK_GL_ERROR
      listInfo.m_displayListId = glGenLists(1);

      if (listInfo.m_displayListId != 0)
        recordList(listInfo.m_displayListId, image, rd);
      else
        std::cout << "list of " << name << fid << " is 0." << std::endl;

      CHECK_GL_ERROR
      m_displayListMap[key] = listInfo;
    }
    return listInfo.m_displayListId;
  }

  void clearLists() {
    // save current context and others
    HDC hCurrentDC               = wglGetCurrentDC();
    T_hGlContext hCurrentContext = wglGetCurrentContext();
    // free all display lists
    DisplayListMap::iterator it = m_displayListMap.begin();
    for (; it != m_displayListMap.end(); ++it) {
      wglMakeCurrent(it->second.m_hdc, it->first.m_glContext);
      glDeleteLists(it->second.m_displayListId, 1);
    }
    m_displayListMap.clear();
    // restore current context

    wglMakeCurrent(hCurrentDC, hCurrentContext);
  }
};

}  // namespace

#endif  // _WIN32

//*************************************************************************************
//    TSimpleStrokeStyle  implementation
//*************************************************************************************

TStrokeProp *TSimpleStrokeStyle::makeStrokeProp(const TStroke *stroke) {
  return new TSimpleStrokeProp(stroke, this);
}

//*************************************************************************************
//    TOutlineStyle  implementation
//*************************************************************************************

TOutlineStyle::TOutlineStyle() : m_regionOutlineModifier(0) {}

//-----------------------------------------------------------------------------

TOutlineStyle::TOutlineStyle(const TOutlineStyle &other)
    : TColorStyle(other)
    , m_regionOutlineModifier(other.m_regionOutlineModifier
                                  ? other.m_regionOutlineModifier->clone()
                                  : 0) {}

//-----------------------------------------------------------------------------

TOutlineStyle::~TOutlineStyle() { delete m_regionOutlineModifier; }

//-----------------------------------------------------------------------------

void TOutlineStyle::setRegionOutlineModifier(RegionOutlineModifier *modifier) {
  delete m_regionOutlineModifier;
  m_regionOutlineModifier = modifier;
}

//-----------------------------------------------------------------------------

TStrokeProp *TOutlineStyle::makeStrokeProp(const TStroke *stroke) {
  return new OutlineStrokeProp(stroke, this);
}

//-----------------------------------------------------------------------------

TRegionProp *TOutlineStyle::makeRegionProp(const TRegion *region) {
  return new OutlineRegionProp(region, this);
}

//-----------------------------------------------------------------------------

void TOutlineStyle::computeOutline(const TStroke *stroke,
                                   TStrokeOutline &outline,
                                   TOutlineUtil::OutlineParameter param) const {
  TOutlineUtil::makeOutline(*stroke, outline, param);
}

//*************************************************************************************
//    TSolidColorStyle  implementation
//*************************************************************************************

TSolidColorStyle::TSolidColorStyle(const TPixel32 &color)
    : m_color(color), m_tessellator(new TglTessellator) {}

//-----------------------------------------------------------------------------

TSolidColorStyle::TSolidColorStyle(const TSolidColorStyle &other)
    : TOutlineStyle(other)
    , m_color(other.m_color)
    , m_tessellator(new TglTessellator) {}

//-----------------------------------------------------------------------------

TSolidColorStyle::~TSolidColorStyle() { delete m_tessellator; }

//-----------------------------------------------------------------------------

TColorStyle *TSolidColorStyle::clone() const {
  return new TSolidColorStyle(*this);
}

//-----------------------------------------------------------------------------

void TSolidColorStyle::makeIcon(const TDimension &size) {
  /*-- TSolidColorStyle, TColorCleanupStyle, TBlackCleanupStyle --*/
  if (getTagId() != 3 && getTagId() != 2001 &&
      getTagId() !=
          2002)  // e' un'istanza di una classe derivata da TSolidColorStyle
  {
    TColorStyle::makeIcon(size);
  } else {
    if (!m_icon || m_icon->getSize() != size) {
      TRaster32P ras(size);
      m_icon = ras;
    }
    TPixel32 col = m_color;
    if (col.m == 255)
      m_icon->fill(col);
    else {
      TRaster32P fg(size);
      fg->fill(premultiply(col));
      TRop::checkBoard(m_icon, TPixel32::Black, TPixel32::White,
                       TDimensionD(6, 6), TPointD());
      TRop::over(m_icon, fg);
    }
  }
}

//-----------------------------------------------------------------------------

void TSolidColorStyle::drawRegion(const TColorFunction *cf,
                                  const bool antiAliasing,
                                  TRegionOutline &boundary) const {
  m_tessellator->tessellate(cf, antiAliasing, boundary, m_color);
}

//-----------------------------------------------------------------------------

//=============================================================================

void TSolidColorStyle::drawStroke(const TColorFunction *cf,
                                  TStrokeOutline *outline,
                                  const TStroke *stroke) const {
  struct locals {
    static inline void fillOutlinedStroke(const std::vector<TOutlinePoint> &v) {
      static const int stride = sizeof(TOutlinePoint);
      glEnableClientState(GL_VERTEX_ARRAY);
      glVertexPointer(2, GL_DOUBLE, stride, &v[0]);
      glDrawArrays(QUAD_PRIMITIVE, 0, v.size());
      glDisableClientState(GL_VERTEX_ARRAY);
    }
  };  // locals

  TPixel32 color = m_color;
  if (cf) color = (*cf)(color);

  if (color.m == 0) return;

  tglColor(color);

  const std::vector<TOutlinePoint> &v = outline->getArray();

  if (v.empty()) return;

  if (true)  // color.m != 254) // !dummyStyle)
  {
    if (color.m < 255) {
      TStencilControl *stencil = TStencilControl::instance();

      /*
// invertendo l'ordine tra disegno interno e contorno, l'antialiasing
// peggiora un po'. Piu che altro, il mangiucchiamento che faceva su alcune
macchine,
// ma solo in drawOnScreen, ora lo fa anche quando si renderizza offline
stencil->beginMask(TStencilControl::DRAW_ON_SCREEN_ONLY_ONCE);
// center line
fillOutlinedStroke(v);
// outline with antialiasing
drawAntialiasedOutline(v);
stencil->endMask();
*/

      stencil->beginMask(TStencilControl::DRAW_ON_SCREEN_ONLY_ONCE);
      locals::fillOutlinedStroke(v);
      stencil->endMask();
      stencil->enableMask(TStencilControl::SHOW_OUTSIDE);
      drawAntialiasedOutline(v, stroke);
      stencil->disableMask();

    } else {
      // outline with antialiasing
      drawAntialiasedOutline(v, stroke);

      // center line
      locals::fillOutlinedStroke(v);
    }

  } else {
    int i = 0;
    for (i = 0; i < stroke->getControlPointCount(); i++) {
      TThickPoint p = stroke->getControlPoint(i);
      if (i % 2) {
        tglColor(TPixel32(255, 0, 0));
        tglDrawCircle(p, p.thick);
      } else {
        tglColor(TPixel32(255, 200, 200));
        tglDrawDisk(p, p.thick);
        tglColor(TPixel32(0, 255, 0));
        tglDrawCircle(p, p.thick);
      }
      tglColor(TPixel32(127, 127, 127));
      tglDrawDisk(p, 0.3);
    }
    tglColor(TPixel32(127, 127, 127));
    glBegin(GL_LINE_STRIP);
    for (i = 0; i < stroke->getControlPointCount(); i++)
      tglVertex(stroke->getControlPoint(i));
    glEnd();

    tglColor(TPixel32(0, 0, 0));
    drawAntialiasedOutline(v, stroke);
  }
}

//-----------------------------------------------------------------------------

QString TSolidColorStyle::getDescription() const { return "SolidColorStyle"; }

//-----------------------------------------------------------------------------

int TSolidColorStyle::getTagId() const { return 3; }

//-----------------------------------------------------------------------------

void TSolidColorStyle::loadData(TInputStreamInterface &is) {
  TPixel32 color;
  is >> color;
  m_color = color;
}

//-----------------------------------------------------------------------------

void TSolidColorStyle::saveData(TOutputStreamInterface &os) const {
  os << m_color;
}

//*************************************************************************************
//    TCenterLineStrokeStyle  implementation
//*************************************************************************************

TCenterLineStrokeStyle::TCenterLineStrokeStyle(const TPixel32 &color,
                                               USHORT stipple, double width)
    : m_color(color), m_stipple(stipple), m_width(width) {}

//-----------------------------------------------------------------------------

TColorStyle *TCenterLineStrokeStyle::clone() const {
  return new TCenterLineStrokeStyle(*this);
}

//------------------------------------------------------------------------------------

void TCenterLineStrokeStyle::drawStroke(const TColorFunction *cf,
                                        const TStroke *stroke) const {
  if (!stroke) return;

  // center line

  int i, n = stroke->getChunkCount();
  if (n == 0) return;
  TPixel32 color = m_color;
  if (cf) color = (*cf)(m_color);
  if (color.m == 0) return;
  tglColor(color);
  double pixelSize = sqrt(tglGetPixelSize2());

  if (m_stipple != 0) {
    glEnable(GL_LINE_STIPPLE);
    glLineStipple(1, m_stipple);
  }

  const TThickQuadratic *q = 0;
  q                        = stroke->getChunk(0);
  GLfloat rangeValue[2];
  glGetFloatv(GL_LINE_WIDTH_RANGE, &rangeValue[0]);

  float width = float(m_width);

  if (width < rangeValue[0])
    width = rangeValue[0];
  else if (width > rangeValue[1])
    width = rangeValue[1];

  width = 2 * width;

  if (width <= 1.0) {
    if (m_width != 0.0) width /= pixelSize;

    if (width ==
        0.F)  // http://www.opengl.org/sdk/docs/man/xhtml/glLineWidth.xml
      width = 0.00001;

    glLineWidth(width);

    glBegin(GL_LINE_STRIP);
    for (i = 0; i < n; i++) {
      q           = stroke->getChunk(i);
      double step = computeStep(*q, pixelSize);
      for (double t = 0; t < 1; t += step) tglVertex(q->getPoint(t));
    }
    assert(q);
    tglVertex(q->getP2());
    glEnd();
  } else {
    double s      = 0;
    double length = stroke->getLength();
    TPointD pos1, pos4;
    std::vector<TPointD> pv;
    width = width / 2.;
    while (s <= length) {
      double step = 1.0;
      double w    = stroke->getParameterAtLength(s);
      if (w <= 0) {
        s += 0.01;
        continue;
      }  // per tamponare il baco della getParameterAtLength()
      TPointD u = stroke->getSpeed(w);
      if (norm2(u) == 0) {
        s += 0.1;
        continue;
      }  // non dovrebbe succedere mai, ma per prudenza....
      TThickPoint pos = stroke->getThickPoint(w);
      u               = normalize(u);
      TPointD v       = rotate90(u) * (width);
      pos1            = pos + v;
      pos4            = pos - v;
      pv.push_back(pos1);
      pv.push_back(pos4);
      s += step;
    }
    double w = stroke->getParameterAtLength(length);
    if (w > 0) {
      TPointD u = stroke->getSpeed(w);
      if (norm2(u) != 0) {
        TThickPoint pos = stroke->getThickPoint(w);
        u               = normalize(u);
        TPointD v       = rotate90(u) * (width);
        pos1            = pos + v;
        pos4            = pos - v;
        pv.push_back(pos1);
        pv.push_back(pos4);
      }
    }

    if (pv.size() > 2) {
      glColor4ub(color.r, color.g, color.b, color.m);
      glBegin(QUAD_PRIMITIVE);
      std::vector<TPointD>::iterator it = pv.begin();
      for (; it != pv.end(); ++it) {
        tglVertex(*it);
      }
      glEnd();

      glBegin(GL_LINE_STRIP);

      std::vector<TPointD>::iterator it1 = pv.begin();
      for (; it1 != pv.end(); ++it1) {
        ++it1;
        tglVertex(*it1);
      }
      glEnd();
      glBegin(GL_LINE_STRIP);
      std::vector<TPointD>::iterator it2 = pv.begin();
      if (it2 != pv.end()) ++it2;
      for (; it2 != pv.end() - 1; ++it2) {
        ++it2;
        tglVertex(*it2);
      }
      glEnd();
    }
  }

  if (m_stipple != 0) glDisable(GL_LINE_STIPPLE);

  glLineWidth(1.0);
}

//-----------------------------------------------------------------------------

QString TCenterLineStrokeStyle::getDescription() const {
  return QCoreApplication::translate("TCenterLineStrokeStyle", "Constant");
}

//-----------------------------------------------------------------------------

int TCenterLineStrokeStyle::getTagId() const { return 2; }

//-----------------------------------------------------------------------------

void TCenterLineStrokeStyle::loadData(TInputStreamInterface &is) {
  is >> m_color >> m_stipple >> m_width;
}

//-----------------------------------------------------------------------------

void TCenterLineStrokeStyle::saveData(TOutputStreamInterface &os) const {
  os << m_color << m_stipple << m_width;
}

//-----------------------------------------------------------------------------

int TCenterLineStrokeStyle::getParamCount() const { return 1; }

//-----------------------------------------------------------------------------

TColorStyle::ParamType TCenterLineStrokeStyle::getParamType(int index) const {
  assert(0 <= index && index < getParamCount());
  return TColorStyle::DOUBLE;
}

//-----------------------------------------------------------------------------

QString TCenterLineStrokeStyle::getParamNames(int index) const {
  return QCoreApplication::translate("TCenterLineStrokeStyle", "Thickness");
}

//-----------------------------------------------------------------------------

void TCenterLineStrokeStyle::getParamRange(int index, double &min,
                                           double &max) const {
  assert(0 <= index && index < getParamCount());
  min = 0.0, max = 10.0;
}

//-----------------------------------------------------------------------------

double TCenterLineStrokeStyle::getParamValue(TColorStyle::double_tag,
                                             int index) const {
  assert(0 <= index && index < getParamCount());
  return m_width;
}

//-----------------------------------------------------------------------------

void TCenterLineStrokeStyle::setParamValue(int index, double value) {
  assert(0 <= index && index < getParamCount());
  m_width = value;
}

/*
TPixel32 TTextureStyle::getMainColor() const
{return m_averageColor; }*/

//*************************************************************************************
//    TRasterImagePatternStrokeStyle  implementation
//*************************************************************************************

TRasterImagePatternStrokeStyle::TRasterImagePatternStrokeStyle()
    : m_level(), m_name(""), m_space(0), m_rotation(0) {}

//-----------------------------------------------------------------------------

TRasterImagePatternStrokeStyle::TRasterImagePatternStrokeStyle(
    const std::string &patternName)
    : m_level(), m_name(patternName), m_space(20), m_rotation(0) {
  if (m_name != "") loadLevel(m_name);
}

//-----------------------------------------------------------------------------

TColorStyle *TRasterImagePatternStrokeStyle::clone() const {
  return new TRasterImagePatternStrokeStyle(*this);
}

//-----------------------------------------------------------------------------

void TRasterImagePatternStrokeStyle::makeIcon(const TDimension &size) {
  if (!m_level) loadLevel(m_name);
  m_icon                   = TRaster32P();
  TLevel::Iterator frameIt = m_level->begin();

  if (frameIt != m_level->end()) {
    TRasterImageP img = frameIt->second;
    TRaster32P src;
    if (img) src = img->getRaster();
    if (src && src->getLx() > 1 && src->getLy() > 1) {
      TRaster32P icon(size);
      icon->fill(TPixel32::White);
      double sx = (double)icon->getLx() / (double)src->getLx();
      double sy = (double)icon->getLy() / (double)src->getLy();
      double sc = 0.8 * std::min(sx, sy);
      TRop::resample(icon, src,
                     TScale(sc).place(src->getCenterD(), icon->getCenterD()));
      TRop::addBackground(icon, TPixel32::White);
      m_icon = icon;
    }
  }

  if (!m_icon) {
    TRaster32P icon(size);
    icon->fill(TPixel32::Black);
    int lx = icon->getLx();
    int ly = icon->getLy();
    for (int y = 0; y < ly; y++) {
      int x = ((lx - 1 - 10) * y / ly);
      icon->extractT(x, y, x + 5, y)->fill(TPixel32::Red);
    }
    m_icon = icon;
    return;
  }
}

//-----------------------------------------------------------------------------

int TRasterImagePatternStrokeStyle::getParamCount() const { return 2; }

//-----------------------------------------------------------------------------

TColorStyle::ParamType TRasterImagePatternStrokeStyle::getParamType(
    int index) const {
  assert(0 <= index && index < getParamCount());
  return TColorStyle::DOUBLE;
}

//-----------------------------------------------------------------------------

QString TRasterImagePatternStrokeStyle::getParamNames(int index) const {
  assert(0 <= index && index < getParamCount());
  return (index == 0) ? QCoreApplication::translate(
                            "TRasterImagePatternStrokeStyle", "Distance")
                      : QCoreApplication::translate(
                            "TRasterImagePatternStrokeStyle", "Rotation");
}

//-----------------------------------------------------------------------------

void TRasterImagePatternStrokeStyle::getParamRange(int index, double &min,
                                                   double &max) const {
  assert(0 <= index && index < getParamCount());

  if (index == 0) {
    min = -50;
    max = 50;
  } else {
    min = -180;
    max = 180;
  }
}

//-----------------------------------------------------------------------------

double TRasterImagePatternStrokeStyle::getParamValue(TColorStyle::double_tag,
                                                     int index) const {
  assert(0 <= index && index < getParamCount());
  return (index == 0) ? m_space : m_rotation;
}

//-----------------------------------------------------------------------------

void TRasterImagePatternStrokeStyle::setParamValue(int index, double value) {
  assert(0 <= index && index < getParamCount());

  if (index == 0) {
    if (m_space != value) {
      updateVersionNumber();  // aggiorna il vettore di affini
    }
    m_space = value;
  } else {
    if (m_rotation != value) {
      updateVersionNumber();  // aggiorna il vettore di affini
    }
    m_rotation = value;
  }
}

//-----------------------------------------------------------------------------

//
// carico il pattern 'patternName' dalla directory dei custom styles
//
void TRasterImagePatternStrokeStyle::loadLevel(const std::string &patternName) {
  struct locals {
    static TAffine getAffine(const TDimension &srcSize,
                             const TDimension &dstSize) {
      double scx = 1 * dstSize.lx / (double)srcSize.lx;
      double scy = 1 * dstSize.ly / (double)srcSize.ly;
      double sc  = std::min(scx, scy);
      double dx  = (dstSize.lx - srcSize.lx * sc) * 0.5;
      double dy  = (dstSize.ly - srcSize.ly * sc) * 0.5;
      return TScale(sc) * TTranslation(0.5 * TPointD(srcSize.lx, srcSize.ly) +
                                       TPointD(dx, dy));
    }
  };  // locals

  // button l'eventuale livello
  m_level = TLevelP();

  // aggiorno il nome
  m_name = patternName;

  // getRootDir() e' nulla se non si e' chiamata la setRoot(..)
  assert(!getRootDir().isEmpty());

  // leggo tutti i livelli contenuti
  TFilePathSet fps;
  TSystem::readDirectory(fps, getRootDir());

  // prendo il primo livello il cui nome sia patternName
  // (puo' essere un pli, ma anche un png, ecc.)
  TFilePath fp;
  TFilePathSet::iterator fpIt;
  for (fpIt = fps.begin(); fpIt != fps.end(); ++fpIt) {
    if (fpIt->getName() == patternName) {
      fp = *fpIt;
      break;
    }
  }

  // se non ho trovato nulla esco
  if (fp == TFilePath() || !TSystem::doesExistFileOrLevel(fp)) return;

  // Leggo i frames del livello e ne ricavo delle textures
  // che memorizzo in m_level come TRasterImage
  TLevelReaderP lr(fp);
  TLevelP level = lr->loadInfo();
  TLevel::Iterator frameIt;
  TOfflineGL *glContext = 0;

  for (frameIt = level->begin(); frameIt != level->end(); ++frameIt) {
    TImageP img = lr->getFrameReader(frameIt->first)->load();
    if (TRasterImageP ri = img) {
      // se il frame e' raster...
      TRaster32P ras = ri->getRaster();
      if (!ras) continue;
      // aggiusta le dimensioni
      ras = makeTexture(ras);
      if (!ras) continue;
      m_level->setFrame(frameIt->first, new TRasterImage(ras));
    } else if (TVectorImageP vi = img) {
      // se il frame e' vettoriale
      // lo rasterizzo creando una texture 256x256 (N.B. le dimensioni
      // delle textures openGL devono essere potenze di due)
      TRaster32P ras(256, 256);
      // se serve creo il contesto OpenGL offline (l'idea e' di crearne
      // uno solo per tutto il livello
      if (!glContext) glContext = new TOfflineGL(ras->getSize());
      // camera di default 640x480. probabilmente non e' la scelta
      // migliore.
      // TDimension cameraSize(768, 576);
      TDimension cameraSize(1920, 1080);

      // definisco i renderdata
      const TVectorRenderData rd(locals::getAffine(cameraSize, ras->getSize()),
                                 TRect(), level->getPalette(), 0, true, true);
      // rasterizzo
      glContext->draw(vi, rd);
      ras->copy(glContext->getRaster());
      m_level->setFrame(frameIt->first, new TRasterImage(ras));
    }
  }
  // cancello il contesto offline (se e' stato creato)
  delete glContext;
}

//--------------------------------------------------------------------------------------------------

void TRasterImagePatternStrokeStyle::computeTransformations(
    std::vector<TAffine> &transformations, const TStroke *stroke) const {
  const int frameCount = m_level->getFrameCount();
  if (frameCount == 0) return;
  transformations.clear();
  const double length = stroke->getLength();

  std::vector<TDimensionD> images;
  assert(m_level->begin() != m_level->end());
  TLevel::Iterator lit;
  for (lit = m_level->begin(); lit != m_level->end(); ++lit) {
    TRasterImageP ri = lit->second;
    if (!ri) continue;
    TDimension d = ri->getRaster()->getSize();
    images.push_back(TDimensionD(d.lx, d.ly));
  }
  assert(!images.empty());
  if (images.empty()) return;

  double s  = 0;
  int index = 0;
  int m     = images.size();
  while (s < length) {
    double t      = stroke->getParameterAtLength(s);
    TThickPoint p = stroke->getThickPoint(t);
    TPointD v     = stroke->getSpeed(t);
    double ang    = rad2degree(atan(v)) + m_rotation;

    int ly    = std::max(1.0, images[index].ly);
    double sc = p.thick / ly;
    transformations.push_back(TTranslation(p) * TRotation(ang) * TScale(sc));
    double ds = std::max(2.0, sc * images[index].lx * 2 + m_space);
    s += ds;
  }
}

//-----------------------------------------------------------------------------

void TRasterImagePatternStrokeStyle::drawStroke(
    const TVectorRenderData &rd, const std::vector<TAffine> &transformations,
    const TStroke *stroke) const {
  TStopWatch sw;
  sw.start();
  CHECK_GL_ERROR

  const int frameCount = m_level->getFrameCount();
  if (frameCount == 0) return;

  // lo stroke viene disegnato ripetendo size volte le frameCount immagini
  // contenute in level, posizionando ognuna secondo transformations[i]
  UINT size = transformations.size();

  glEnable(GL_TEXTURE_2D);
  glEnable(GL_BLEND);

  glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);

  GLuint texId;
  glGenTextures(1, &texId);

  glBindTexture(GL_TEXTURE_2D, texId);

  glTexParameterf(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_REPEAT);
  glTexParameterf(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_REPEAT);
  glTexParameterf(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
  glTexParameterf(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);

  // l'utilizzo di modulate, con un colore di 1,1,1,1 permette di
  // tenere in conto il canale alfa delle texture
  glTexEnvf(GL_TEXTURE_ENV, GL_TEXTURE_ENV_MODE, GL_MODULATE);

  glPixelStorei(GL_UNPACK_ROW_LENGTH, 0);

  // visto che cambiare texture costa tempo il ciclo esterno e' sulle textures
  // piuttosto che sulle trasformazioni
  TLevel::Iterator frameIt = m_level->begin();
  for (int i = 0; i < (int)size && frameIt != m_level->end(); ++i, ++frameIt) {
    TRasterImageP ri = frameIt->second;
    TRasterP ras;
    if (ri) ras = ri->getRaster();
    if (!ras) continue;
    TextureInfoForGL texInfo;
    TRasterP texImage = prepareTexture(ras, texInfo);

    glTexImage2D(GL_TEXTURE_2D,
                 0,                       // one level only
                 texInfo.internalformat,  // pixel channels count
                 texInfo.width,           // width
                 texInfo.height,          // height
                 0,                       // border size
                 texInfo.type,    // pixel format           // crappy names
                 texInfo.format,  // pixel data type        // oh, SO much
                 texImage->getRawData());

    for (int j = i; j < (int)size; j += frameCount) {
      TAffine aff = rd.m_aff * transformations[j];
      glPushMatrix();
      tglMultMatrix(aff);

      double rx = ras->getLx();
      double ry = ras->getLy();

      glColor4d(1, 1, 1, 1);

      glBegin(QUAD_PRIMITIVE);
      glTexCoord2d(0, 0);
      glVertex2d(-rx, -ry);
      glTexCoord2d(1, 0);
      glVertex2d(rx, -ry);
      glTexCoord2d(0, 1);
      glVertex2d(-rx, ry);
      glTexCoord2d(1, 1);
      glVertex2d(rx, ry);
      glEnd();

      glPopMatrix();
    }
  }

  glDeleteTextures(1, &texId);

  glDisable(GL_TEXTURE_2D);
  glDisable(GL_BLEND);
}

//-----------------------------------------------------------------------------

void TRasterImagePatternStrokeStyle::loadData(TInputStreamInterface &is) {
  m_level = TLevelP();
  m_name  = "";
  std::string name;
  is >> name >> m_space >> m_rotation;
  if (name != "") {
    try {
      loadLevel(name);
    } catch (...) {
    }
  }
}

//-----------------------------------------------------------------------------

void TRasterImagePatternStrokeStyle::loadData(int ids,
                                              TInputStreamInterface &is) {
  if (ids != 100)
    throw TException("image pattern stroke style: unknown obsolete format");

  m_level = TLevelP();
  m_name  = "";
  std::string name;
  is >> name;
  if (name != "") {
    try {
      loadLevel(name);
    } catch (...) {
    }
  }
}

//-----------------------------------------------------------------------------

void TRasterImagePatternStrokeStyle::saveData(
    TOutputStreamInterface &os) const {
  os << m_name << m_space << m_rotation;
}

//-----------------------------------------------------------------------------

TFilePath TRasterImagePatternStrokeStyle::getRootDir() { return m_rootDir; }

//-----------------------------------------------------------------------------

TStrokeProp *TRasterImagePatternStrokeStyle::makeStrokeProp(
    const TStroke *stroke) {
  return new TRasterImagePatternStrokeProp(stroke, this);
}

//-----------------------------------------------------------------------------

void TRasterImagePatternStrokeStyle::getObsoleteTagIds(
    std::vector<int> &ids) const {
  ids.push_back(100);
}

//*************************************************************************************
//    TVectorImagePatternStrokeStyle  implementation
//*************************************************************************************

TVectorImagePatternStrokeStyle::TVectorImagePatternStrokeStyle()
    : m_level(), m_name(""), m_space(0), m_rotation(0) {}

//-----------------------------------------------------------------------------

TVectorImagePatternStrokeStyle::TVectorImagePatternStrokeStyle(
    const std::string &patternName)
    : m_level(), m_name(patternName), m_space(20), m_rotation(0) {
  loadLevel(patternName);
}

//-----------------------------------------------------------------------------

TColorStyle *TVectorImagePatternStrokeStyle::clone() const {
  return new TVectorImagePatternStrokeStyle(*this);
}

//-----------------------------------------------------------------------------

void TVectorImagePatternStrokeStyle::makeIcon(const TDimension &size) {
  TLevel::Iterator frameIt = m_level->begin();
  if (frameIt == m_level->end()) {
    TRaster32P icon(size);
    icon->fill(TPixel32::Black);
    int lx = icon->getLx();
    int ly = icon->getLy();
    for (int y = 0; y < ly; y++) {
      int x = ((lx - 1 - 10) * y / ly);
      icon->extractT(x, y, x + 5, y)->fill(TPixel32::Red);
    }
    m_icon = icon;
    return;
  }
  TFrameId fid      = frameIt->first;
  TVectorImageP img = m_level->frame(fid);
  if (!img) {
    TRaster32P icon(size);
    icon->fill(TPixel32::Black);
    int lx = icon->getLx();
    int ly = icon->getLy();
    for (int y = 0; y < ly; y++) {
      int x = ((lx - 1 - 10) * y / ly);
      icon->extractT(x, y, x + 5, y)->fill(TPixel32::Red);
    }
    m_icon = icon;
    return;
  }
  // img->setPalette(m_level->getPalette());
  TPalette *vPalette = m_level->getPalette();
  assert(vPalette);
  img->setPalette(vPalette);
  TOfflineGL *glContext = TOfflineGL::getStock(size);
  glContext->clear(TPixel32::White);
  TRectD bbox = img->getBBox();
  double scx  = 0.8 * size.lx / bbox.getLx();
  double scy  = 0.8 * size.ly / bbox.getLy();
  double sc   = std::min(scx, scy);
  double dx   = (size.lx - bbox.getLx() * sc) * 0.5;
  double dy   = (size.ly - bbox.getLy() * sc) * 0.5;
  TAffine aff = TScale(sc) * TTranslation(-bbox.getP00() + TPointD(dx, dy));
  TVectorRenderData rd(aff, size, vPalette, 0, true);
  glContext->draw(img, rd);
  if (!m_icon || m_icon->getSize() != size)
    m_icon = glContext->getRaster()->clone();
  else
    m_icon->copy(glContext->getRaster());
}

//-----------------------------------------------------------------------------

int TVectorImagePatternStrokeStyle::getParamCount() const { return 2; }

//-----------------------------------------------------------------------------

TColorStyle::ParamType TVectorImagePatternStrokeStyle::getParamType(
    int index) const {
  assert(0 <= index && index < getParamCount());
  return TColorStyle::DOUBLE;
}

//-----------------------------------------------------------------------------

QString TVectorImagePatternStrokeStyle::getParamNames(int index) const {
  assert(0 <= index && index < getParamCount());
  return (index == 0) ? QCoreApplication::translate(
                            "TVectorImagePatternStrokeStyle", "Distance")
                      : QCoreApplication::translate(
                            "TVectorImagePatternStrokeStyle", "Rotation");
}

//-----------------------------------------------------------------------------

void TVectorImagePatternStrokeStyle::getParamRange(int index, double &min,
                                                   double &max) const {
  assert(0 <= index && index < getParamCount());

  if (index == 0) {
    min = -50;
    max = 50;
  } else {
    min = -180;
    max = 180;
  }
}

//-----------------------------------------------------------------------------

double TVectorImagePatternStrokeStyle::getParamValue(TColorStyle::double_tag,
                                                     int index) const {
  assert(0 <= index && index < getParamCount());
  return index == 0 ? m_space : m_rotation;
}

//-----------------------------------------------------------------------------

void TVectorImagePatternStrokeStyle::setParamValue(int index, double value) {
  assert(0 <= index && index < getParamCount());

  if (index == 0) {
    if (m_space != value) {
      updateVersionNumber();  // aggiorna il vettore di affini
    }
    m_space = value;
  } else {
    if (m_rotation != value) {
      updateVersionNumber();  // aggiorna il vettore di affini
    }
    m_rotation = value;
  }
}

//-----------------------------------------------------------------------------

void TVectorImagePatternStrokeStyle::loadLevel(const std::string &patternName) {
  m_level = TLevelP();
  m_name  = patternName;
  assert(!getRootDir()
              .isEmpty());  // se e' vuota, non si e' chiamata la setRoot(..)
  TFilePath fp = getRootDir() + (patternName + ".pli");
  TLevelReaderP lr(fp);
  m_level = lr->loadInfo();
  TLevel::Iterator frameIt;
  std::map<TPixel32, int> colors;
  for (frameIt = m_level->begin(); frameIt != m_level->end(); ++frameIt) {
    TVectorImageP img = lr->getFrameReader(frameIt->first)->load();
    if (img) m_level->setFrame(frameIt->first, img);
  }
}

//--------------------------------------------------------------------------------------------------

void TVectorImagePatternStrokeStyle::computeTransformations(
    std::vector<TAffine> &transformations, const TStroke *stroke) const {
  const int frameCount = m_level->getFrameCount();
  if (frameCount == 0) return;
  transformations.clear();
  const double length = stroke->getLength();
  assert(m_level->begin() != m_level->end());
  TLevel::Iterator lit = m_level->begin();
  double s             = 0;

  while (s < length) {
    TFrameId fid      = lit->first;
    TVectorImageP img = m_level->frame(fid);
    if (++lit == m_level->end()) lit = m_level->begin();
    assert(img);
    if (img->getType() != TImage::VECTOR) return;
    double t       = stroke->getParameterAtLength(s);
    TThickPoint p  = stroke->getThickPoint(t);
    TPointD v      = stroke->getSpeed(t);
    double ang     = rad2degree(atan(v)) + m_rotation;
    TRectD bbox    = img->getBBox();
    TPointD center = 0.5 * (bbox.getP00() + bbox.getP11());
    double ry      = bbox.getLy() * 0.5;
    if (ry * ry < 1e-5) ry = p.thick;
    double sc = p.thick / ry;
    if (sc < 0.0001) sc = 0.0001;
    TAffine aff =
        TTranslation(p) * TRotation(ang) * TScale(sc) * TTranslation(-center);
    transformations.push_back(aff);
    double ds = std::max(2.0, sc * bbox.getLx() + m_space);
    s += ds;
  }
}

//-----------------------------------------------------------------------------

void TVectorImagePatternStrokeStyle::clearGlDisplayLists() {
#ifdef _WIN32
  DisplayListManager *pmgr = DisplayListManager::instance();
  assert(pmgr);
  pmgr->clearLists();
#endif
}

//--------------------------------------------------------------------------------------------------

void TVectorImagePatternStrokeStyle::drawStroke(
    const TVectorRenderData &rd, const std::vector<TAffine> &transformations,
    const TStroke *stroke) const {
  const int frameCount = m_level->getFrameCount();
  if (frameCount == 0) return;
  //------------------------------------------
  // if the average thickness of the stroke, with the current pixelSize,
  // is less than 2, do not drow anything
  UINT cpCount    = stroke->getControlPointCount();
  UINT sampleStep = (cpCount < 10) ? 1 : (UINT)((double)cpCount / 10.0);
  double thickSum = 0;
  UINT count      = 0;
  for (UINT cp = 0; cp < cpCount; cp += sampleStep) {
    thickSum += stroke->getControlPoint(cp).thick;
    count++;
  }
  double averageThick = thickSum / (double)count;
  glPushMatrix();
  tglMultMatrix(rd.m_aff);
  double pixelSize2 = tglGetPixelSize2();
  glPopMatrix();
  if (averageThick * averageThick < 4 * pixelSize2) {
    CHECK_GL_ERROR

    glPushMatrix();

    CHECK_GL_ERROR

    tglMultMatrix(rd.m_aff);

    TLevel::Iterator lit = m_level->begin();

    TFrameId fid      = lit->first;
    TVectorImageP img = m_level->frame(fid);

    std::set<int> styles;
    img->getUsedStyles(styles);

    std::set<int>::iterator si = styles.begin();

    // Calcolo media dei colori utilizzati

    int numStyles  = 0, styleID;
    int averageRed = 0, averageBlue = 0, averageGreen = 0;
    TPixel32 color, averageColor;
    TColorStyle *colorStyle;

    for (; si != styles.end(); si++) {
      styleID = (int)*si;

      if (styleID != 0) {
        colorStyle = m_level->getPalette()->getStyle(styleID);
        color      = colorStyle->getAverageColor();
        if (color.m != 0) {
          numStyles++;
          averageRed += color.r;
          averageGreen += color.g;
          averageBlue += color.b;
        }
      }
    }

    if (numStyles != 0) {
      averageColor.r = averageRed / numStyles;
      averageColor.g = averageGreen / numStyles;
      averageColor.b = averageBlue / numStyles;
    } else
      averageColor = TPixel32::Black;

    tglColor(averageColor);

    glLineStipple(2, 0xAAAA);
    glEnable(GL_LINE_STIPPLE);

    UINT n = stroke->getLength() *
             0.05;  // TODO: funzione di stroke length e pixelsize
    glBegin(GL_LINE_STRIP);
    for (UINT i = 0; i < n; i++) {
      if (n != 1) {
        TThickPoint p = stroke->getPoint(i / (double)(n - 1));
        glVertex2d(p.x, p.y);
      }
    }
    glEnd();

    glDisable(GL_LINE_STIPPLE);

    CHECK_GL_ERROR

    glPopMatrix();

  } else {
    //--------------------------------------------
    assert(m_level->begin() != m_level->end());
    TLevel::Iterator lit = m_level->begin();
    UINT i, size = transformations.size();

    for (i = 0; i < size; i++) {
      TFrameId fid      = lit->first;
      TVectorImageP img = m_level->frame(fid);
      if (++lit == m_level->end()) lit = m_level->begin();
      assert(img);
      if (!img) continue;
      if (img->getType() != TImage::VECTOR) return;
      TVectorImage *imgPointer    = img.getPointer();
      TAffine totalTransformation = rd.m_aff * transformations[i];
      if (rd.m_clippingRect != TRect() && !rd.m_is3dView &&
          !(convert(totalTransformation * imgPointer->getBBox())
                .overlaps(rd.m_clippingRect)))
        continue;
      // drawing
      CHECK_GL_ERROR

      glPushMatrix();

      CHECK_GL_ERROR

      tglMultMatrix(totalTransformation);

      CHECK_GL_ERROR
#ifdef _WIN32
      // currently drawing with the display list doesn't work properly

      // GLuint listId = DisplayListManager::instance()->getDisplayListId(
      //    imgPointer, m_name, fid, rd);
      // if (listId != 0) {
      //  CHECK_GL_ERROR
      //  glCallList(listId);
      //  CHECK_GL_ERROR
      //} else
#endif

      {
        TAffine identity;
        TRect rect;
        TPalette *vPalette = imgPointer->getPalette();
        assert(vPalette);
        TVectorRenderData rd2(identity, rect, vPalette, rd.m_cf, true, true);
        CHECK_GL_ERROR

        tglDraw(rd2, imgPointer);

        CHECK_GL_ERROR
      }
      glPopMatrix();
    }
  }
}

//-----------------------------------------------------------------------------

void TVectorImagePatternStrokeStyle::loadData(TInputStreamInterface &is) {
  m_level = TLevelP();
  m_name  = "";
  std::string name;
  is >> name >> m_space >> m_rotation;
  if (name != "") {
    try {
      loadLevel(name);
    } catch (...) {
    }
  }
}

//-----------------------------------------------------------------------------

void TVectorImagePatternStrokeStyle::loadData(int ids,
                                              TInputStreamInterface &is) {
  if (ids != 100)
    throw TException("image pattern stroke style: unknown obsolete format");

  m_level = TLevelP();
  m_name  = "";
  std::string name;
  is >> name;
  if (name != "") {
    try {
      loadLevel(name);
    } catch (...) {
    }
  }
}

//-----------------------------------------------------------------------------

void TVectorImagePatternStrokeStyle::saveData(
    TOutputStreamInterface &os) const {
  os << m_name << m_space << m_rotation;
}

//-----------------------------------------------------------------------------

TFilePath TVectorImagePatternStrokeStyle::getRootDir() { return m_rootDir; }

//-----------------------------------------------------------------------------

TStrokeProp *TVectorImagePatternStrokeStyle::makeStrokeProp(
    const TStroke *stroke) {
  return new TVectorImagePatternStrokeProp(stroke, this);
}

//-----------------------------------------------------------------------------

void TVectorImagePatternStrokeStyle::getObsoleteTagIds(
    std::vector<int> &ids) const {
  // ids.push_back(100);
}

//*************************************************************************************
//    Style declarations  instances
//*************************************************************************************

namespace {

TColorStyle::Declaration s0(new TCenterLineStrokeStyle());
TColorStyle::Declaration s1(new TSolidColorStyle());
TColorStyle::Declaration s3(new TRasterImagePatternStrokeStyle());
TColorStyle::Declaration s4(new TVectorImagePatternStrokeStyle());
}  // namespace
