

#include <qstringlist.h>

#include "tsimplecolorstyles.h"

#include "ttessellator.h"
#include "trop.h"
#include "drawutil.h"
#include "tpixelutils.h"
#include "tlevel_io.h"
#include "timage_io.h"
#include "trandom.h"
#include "tvectorimage.h"
#include "toonz/toonzscene.h"
#include "toonz/imagestyles.h"

//*************************************************************************************
//    TTextureStyle  implementation
//*************************************************************************************

namespace {

TRandom Random;

#define QUAD_PRIMITIVE GL_QUAD_STRIP

typedef std::pair<TPointD, TPointD> PointPair;
typedef std::vector<TPointD> PointArray;

PointPair computeTexParameters(const TPointD &p1, const TPointD &p2,
                               const TPointD &tex1, const TPointD &tex2,
                               const PointPair &newP, const TDimension &size) {
  // texture points
  static PointPair tex;

  // set vector of movement
  TPointD v = (newP.first + newP.second) * 0.5 - (p2 + p1) * 0.5;

  // compute length of opposite segment
  double lengthNextP1NextP2 = tdistance(newP.first, newP.second);

  // compute parameter texture offset starting from 0.5
  double texParameterOffset = lengthNextP1NextP2 / size.lx * 0.5;

  // fix value for s
  tex.first.x  = 0.5 - texParameterOffset;
  tex.second.x = 0.5 + texParameterOffset;

  // get length
  double disty = norm(v);
  assert(tex1.y == tex2.y);

  // find parameter for t
  texParameterOffset = disty / size.ly;

  // fix values for t (oldValue + newValue)
  tex.first.y = tex.second.y = tex1.y + texParameterOffset;

  return tex;
}

//-----------------------------------------------------------------------------

void setTexCoords(const TPointD tex, TOutlinePoint &p) {
  p.u = tex.x;
  p.v = tex.y;
}

//-----------------------------------------------------------------------------

TPointD getTexCoords(const TOutlinePoint &p) { return TPointD(p.u, p.v); }

}  // namespace

//-----------------------------------------------------------------------------
TTextureStyle::TTextureStyle(const TRasterP &ras, const TFilePath &texturePath)
    : m_texture(ras)
    , m_texturePath(texturePath)
    , m_tessellator(new TglTessellator)
    , m_params()
    , m_texturePathLoaded() {
  setAverageColor();
}

//-----------------------------------------------------------------------------

TFilePath TImageStyle::m_libraryDir;
ToonzScene *TImageStyle::m_currentScene = 0;

//-----------------------------------------------------------------------------

TTextureStyle::TTextureStyle(const TTextureStyle &other)
    : TOutlineStyle(other)
    , TRasterStyleFx(other)
    , TImageStyle(other)
    , m_texture(other.m_texture)
    , m_texturePath(other.m_texturePath)
    , m_texturePathLoaded(other.m_texturePathLoaded)
    , m_params(other.m_params)
    , m_tessellator(new TglTessellator) {
  setAverageColor();
}

//-----------------------------------------------------------------------------

TTextureStyle::~TTextureStyle() { delete m_tessellator; }

//-----------------------------------------------------------------------------

TColorStyle *TTextureStyle::clone() const { return new TTextureStyle(*this); }

//-----------------------------------------------------------------------------

QString TTextureStyle::getDescription() const { return "TextureStyle"; }

//-----------------------------------------------------------------------------

//-----------------------------------------------------------------------------
int TTextureStyle::getTagId() const { return 4; }
//-----------------------------------------------------------------------------

TRasterP TTextureStyle::getTexture() const { return m_texture; }
//-----------------------------------------------------------------------------

void TTextureStyle::setAverageColor() {
  loadTextureRaster();

  TRaster32P r32 = (TRaster32P)m_texture;
  if (!r32) {
    m_averageColor = TPixel::Black;
    return;
  }
  if (m_params.m_isPattern) {
    m_averageColor = m_params.m_patternColor;
    return;
  }
  TPixelD color(0, 0, 0, 0);
  r32->lock();
  for (int i = 0; i < r32->getLy(); i++) {
    TPixel32 *buf = r32->pixels(i);
    for (int j = 0; j < r32->getLx(); j++, buf++) {
      color.r += buf->r;
      color.g += buf->g;
      color.b += buf->b;
      color.m += buf->m;
    }
  }
  r32->unlock();
  int pixels       = r32->getLx() * r32->getLy();
  m_averageColor.r = (int)(color.r / pixels);
  m_averageColor.g = (int)(color.g / pixels);
  m_averageColor.b = (int)(color.b / pixels);
  m_averageColor.m = (int)(color.m / pixels);
}

void TTextureStyle::setTexture(const TRasterP &ras) {
  m_texture = ras;
  delete m_tessellator;
  m_tessellator = new TglTessellator;

  setAverageColor();
}
//-----------------------------------------------------------------------------
TPixel32 TTextureStyle::getAverageColor() const { return m_averageColor; }
//-----------------------------------------------------------------------------

void TTextureStyle::computeOutline(const TStroke *stroke,
                                   TStrokeOutline &outline,
                                   TOutlineUtil::OutlineParameter param) const {
  TOutlineStyle::computeOutline(stroke, outline, param);
  std::vector<TOutlinePoint> &v = outline.getArray();
  PointPair newPnt;
  TDimension size = m_texture->getSize();
  UINT i          = 0;
  for (i = 2; i < v.size(); i += 2) {
    newPnt.first  = convert(v[i]);
    newPnt.second = convert(v[i + 1]);
    newPnt        = computeTexParameters(convert(v[i - 2]), convert(v[i - 1]),
                                  getTexCoords(v[i - 2]),
                                  getTexCoords(v[i - 1]), newPnt, size);
    setTexCoords(newPnt.first, v[i]);
    setTexCoords(newPnt.second, v[i + 1]);
  }
  for (i = 0; i < v.size(); i++) {
    v[i].u = (i & 1) == 0 ? 0 : 1;
  }
}

//-----------------------------------------------------------------------------
void TTextureStyle::drawStroke(const TColorFunction *cf,
                               TStrokeOutline *outline,
                               const TStroke *stroke) const {
  /*struct locals {
static float adaptToGLTextureFunction( USHORT style )
{
switch( style )
{
case TTextureStyle::DECAL:
  return GL_DECAL;
  break;
case TTextureStyle::MODULATE:
  return GL_MODULATE;
  break;
case TTextureStyle::BLEND:
  return GL_BLEND;
  break;
}
return GL_DECAL;
}
};    // locals;*/

  UINT i;

  std::vector<TOutlinePoint> &v = outline->getArray();
  if (v.empty()) return;

  const TRasterP texture = m_texture;

  assert(!v.empty() || !texture);

  if (v.empty() || !texture) return;

  static const int stride = sizeof(TOutlinePoint);

  glColor4d(1.0, 1.0, 1.0, 1.0);

  // information about vertex
  glEnableClientState(GL_VERTEX_ARRAY);
  glVertexPointer(2, GL_DOUBLE, stride, &v[0]);

  glEnable(GL_TEXTURE_2D);

#ifdef _DEBUG
  GLboolean blendEnabled;
  glGetBooleanv(GL_BLEND, &blendEnabled);
  assert(blendEnabled);
#endif

  glEnableClientState(GL_TEXTURE_COORD_ARRAY);
  glTexCoordPointer(2, GL_DOUBLE, stride, &(v[0].u));

  TextureInfoForGL texInfo;

  m_texture->lock();
  TRasterP texImage = prepareTexture(m_texture, texInfo);

  // Generate a texture id and bind it.
  GLuint texId;
  glGenTextures(1, &texId);

  glBindTexture(GL_TEXTURE_2D, texId);

  // Specify texture parameters
  glTexParameterf(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_REPEAT);
  glTexParameterf(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_REPEAT);
  glTexParameterf(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
  glTexParameterf(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);

  glTexEnvf(GL_TEXTURE_ENV, GL_TEXTURE_ENV_MODE,
            GL_MODULATE);  // change texture blending function

  glPixelStorei(GL_UNPACK_ROW_LENGTH, texImage->getWrap() != texImage->getLx()
                                          ? texImage->getWrap()
                                          : 0);

  if (texImage != m_texture) texImage->lock();

  // Load texture
  glTexImage2D(GL_TEXTURE_2D,
               0,                       // one level only
               texInfo.internalformat,  // pixel channels count
               texInfo.width,           // width
               texInfo.height,          // height
               0,                       // border size
               texInfo.type,    // pixel format           // crappy names
               texInfo.format,  // pixel data type        // oh, SO much
               texImage->getRawData());

  m_texture->unlock();
  if (texImage != m_texture) texImage->lock();

  // Step 1: draw outline
  glBegin(GL_LINE_STRIP);
  for (i = 0; i < v.size(); i += 2) glArrayElement(i);
  glEnd();
  glBegin(GL_LINE_STRIP);
  for (i = 1; i < v.size(); i += 2) glArrayElement(i);
  glEnd();

  // Step 2: draw texturized stroke
  /*
#ifdef MACOSX                   //NON SI CAPISCE PERCHE DRAWARRAY E DISPLAY LIST
PROVOCHINO UN CRASH SU MAC
#warning "INDAGARE ANCORA!!!!"  //PER ADESSO CHIEDO SE STA REGISTANDO UNA
DISPLAYLIST E IN QUEL CASO NON USO GLI ARRAY
GLuint listId;
glGetIntegerv(GL_LIST_INDEX,(GLint*)&listId);
if(listId==0)
{
#endif
*/
  glDrawArrays(QUAD_PRIMITIVE, 0, v.size());
  /*
#ifdef MACOSX
}
else
{
glBegin( GL_QUAD_STRIP );
for(UINT i=0; i< v.size(); i++)
glVertex2dv( &v[i].x );
glEnd();
}
#endif
*/

  // Delete texture
  glDeleteTextures(1, &texId);

  glDisableClientState(GL_VERTEX_ARRAY);
  glDisableClientState(GL_TEXTURE_COORD_ARRAY);
  glPixelStorei(GL_UNPACK_ROW_LENGTH, 0);
  glDisable(GL_TEXTURE_2D);
}
//-----------------------------------------------------------------------------
// drawRegion( const TVectorRenderData &rd,  TRegionOutline &boundary ) const
void TTextureStyle::drawRegion(const TColorFunction *cf,
                               const bool antiAliasing,
                               TRegionOutline &boundary) const {
  if (m_tessellator)
    m_tessellator->tessellate(cf, antiAliasing, boundary, m_texture);
}

//-----------------------------------------------------------------------------

//----------------------------------------
namespace {

void tileRaster(const TRaster32P &tile, const TRaster32P &rout) {
  int x0, y0;

  if (rout->getLy() > tile->getLy())  /// tile must be centered in rout
    y0 =
        tile->getLy() - (((rout->getLy() - tile->getLy()) / 2) % tile->getLy());
  else
    y0 = (tile->getLy() - rout->getLy()) / 2;
  if (rout->getLx() > tile->getLx())  /// tile must be centered in rout
    x0 =
        tile->getLx() - (((rout->getLx() - tile->getLx()) / 2) % tile->getLx());
  else
    x0 = (tile->getLx() - rout->getLx()) / 2;

  // x0-=tround(offs.x);
  // y0-=tround(offs.y);
  while (x0 < 0) x0 += tile->getLx();
  while (y0 < 0) y0 += tile->getLy();
  x0 = x0 % tile->getLx();
  y0 = y0 % tile->getLy();

  int y = y0;
  for (int i = 0; i < rout->getLy(); i++, y++) {
    if (y == tile->getLy()) y = 0;
    int x                     = x0;
    TPixel32 *pixout          = rout->pixels(i);
    TPixel32 *pixin           = tile->pixels(y) + x;

    for (int j = 0; j < rout->getLx(); j++, pixin++, pixout++, x++) {
      if (x == tile->getLx()) {
        x = 0, pixin = tile->pixels(y);
      }
      *pixout = *pixin;
    }
  }
}

//---------------------------------------------------------------------

void doContrast(double contrast, const TRaster32P &rin) {
  TPixel32 *row, *pix;
  int i, j, lx, ly, wrap;
  double line_avg_r, line_avg_g, line_avg_b;
  double avg_r = 0, avg_g = 0, avg_b = 0;

  lx   = rin->getLx();
  ly   = rin->getLy();
  wrap = rin->getWrap();

  for (j = 0, row = (TPixel32 *)rin->getRawData(); j < ly; j++, row += wrap) {
    line_avg_r = 0;
    line_avg_g = 0;
    line_avg_b = 0;
    for (i = 0, pix = row; i < lx; i++, pix++) {
      line_avg_r += pix->r;
      line_avg_g += pix->g;
      line_avg_b += pix->b;
    }
    avg_r += line_avg_r / lx;
    avg_g += line_avg_g / lx;
    avg_b += line_avg_b / lx;
  }

  avg_r /= ly;
  avg_g /= ly;
  avg_b /= ly;

  int ires;
  double dres;
  TPixel32 lut[256];

  for (int i = 0; i < 256; i++) {
    dres     = contrast * (i - avg_r) + avg_r;
    ires     = tround(dres);
    lut[i].r = tcrop(ires, 0, 255);
    dres     = contrast * (i - avg_g) + avg_g;
    ires     = tround(dres);
    lut[i].g = tcrop(ires, 0, 255);
    dres     = contrast * (i - avg_b) + avg_b;
    ires     = tround(dres);
    lut[i].b = tcrop(ires, 0, 255);
  }

  for (j = 0, row = (TPixel32 *)rin->getRawData(); j < ly; j++, row += wrap) {
    for (i = 0, pix = row; i < lx; i++, pix++) {
      pix->r = lut[pix->r].r;
      pix->g = lut[pix->g].g;
      pix->b = lut[pix->b].b;

      if (pix->r > pix->m) pix->r = pix->m;
      if (pix->g > pix->m) pix->g = pix->m;
      if (pix->b > pix->m) pix->b = pix->m;
    }
  }
}

//------------------------------------------------------------------------------------

#define BYTE_ROUND(A) ((UINT)((A) + (1U << 23)) >> 24)

#define MAGICFAC (257U * 256U + 1U)

static void doPattern(const TRaster32P &r, TPixel32 color) {
  int i, j, fac = 0;

  if (TPixelGR8::from(color).value < 128)
    fac = 255 - color.m * (255.0 - color.m) / 255.0;

  TPixel32 *buf;
  UINT b;

  for (i = 0; i < r->getLy(); i++) {
    buf = (TPixel32 *)r->pixels(i);
    for (j = 0; j < r->getLx(); j++, buf++) {
      if (buf->m > 0) {
        b = TPixelGR8::from(*buf).value * MAGICFAC;

        buf->r = BYTE_ROUND(b * color.r + fac);
        buf->g = BYTE_ROUND(b * color.g + fac);
        buf->b = BYTE_ROUND(b * color.b + fac);
        if (buf->m < 255 || color.m < 255) {
          buf->m = BYTE_ROUND(buf->m * color.m / 255.0);
          premult(*buf);
        } else
          buf->m = 255;
      }
    }
  }
}

//--------------------------------------------------------

void applyTexture(const TRaster32P &rTex, const TRaster32P &r, TPoint p) {
  while (p.x < 0) p.x += rTex->getLx();
  while (p.y < 0) p.y += rTex->getLy();
  r->lock();
  rTex->lock();
  for (int y = 0, yTex = p.y; y < r->getLy(); y++, yTex++) {
    TPixel32 *pixIn = r->pixels(y);
    if (yTex >= rTex->getLy()) yTex -= rTex->getLy();

    TPixel32 *pixTex = (TPixel32 *)(rTex->pixels(yTex)) + p.x;

    for (int x = 0, xTex = p.x; x < r->getLx();
         x++, xTex++, pixIn++, pixTex++) {
      if (xTex >= rTex->getLx()) xTex -= rTex->getLx(), pixTex -= rTex->getLx();

      if (pixIn->m == 0) continue;

      if (pixIn->m == 255)
        *pixIn = *pixTex;
      else {
        pixIn->r = pixIn->m * pixTex->r / 255;
        pixIn->g = pixIn->m * pixTex->g / 255;
        pixIn->b = pixIn->m * pixTex->b / 255;
        pixIn->m = pixIn->m * pixTex->m / 255;
      }
    }
  }
  r->unlock();
  rTex->unlock();
}

}  // namespace
//---------------------------------------------------------------------------------------------------------------

TPoint computeCentroid(const TRaster32P &r);

//-----------------------------------------------------------------------------

int TTextureStyle::getParamCount() const { return 8; }

//-------------------------------------------------------------------

QString TTextureStyle::getParamNames(int index) const {
  switch (index) {
  case 0:
    return "Use As Pattern";
  case 1:
    return "Position";
  case 2:
    return "Scale";
  case 3:
    return "Rotation(degrees)";
  case 4:
    return "X displ";
  case 5:
    return "Y displ";
  case 6:
    return "Contrast";
  case 7:
    return "Load From File";
  default:
    assert(false);
  }

  return QString("");
}

//-------------------------------------------------------------------

void TTextureStyle::getParamRange(int index, double &min, double &max) const {
  assert(index > 1);
  switch (index) {
  case 2:
    min = 0.15;
    max = 10;
    break;
  case 3:
    min = -180;
    max = 180;
    break;
  case 4:
    min = -500;
    max = 500;
    break;
  case 5:
    min = -500;
    max = 500;
    break;
  case 6:
    min = 0.01;
    max = 10;
    break;
  default:
    assert(false);
  }
}

//-------------------------------------------------------------------

TColorStyle::ParamType TTextureStyle::getParamType(int index) const {
  assert(0 <= index && index < getParamCount());
  if (index == 0)
    return TColorStyle::BOOL;
  else if (index == 1)
    return TColorStyle::ENUM;
  else if (index == 7)
    return TColorStyle::FILEPATH;
  return TColorStyle::DOUBLE;
}

//-----------------------------------------------------------------------------

void TTextureStyle::getParamRange(int index, QStringList &enumItems) const {
  assert(index == 1 || index == 7);
  if (index == 1)
    enumItems << "FIXED"
              << "AUTOMATIC"
              << "RANDOM";
  else if (index == 7)
    enumItems << "bmp"
              << "jpg"
              << "png"
              << "tif"
              << "tiff"
              << "gif";
}

//-------------------------------------------------------------------------

bool TTextureStyle::getParamValue(TColorStyle::bool_tag, int index) const {
  assert(index == 0);
  return m_params.m_isPattern;
}

//---------------------------------------------------

double TTextureStyle::getParamValue(TColorStyle::double_tag, int index) const {
  assert(index > 1);
  switch (index) {
  case 2:
    return m_params.m_scale;
  case 3:
    return m_params.m_rotation;
  case 4:
    return m_params.m_displacement.x;
  case 5:
    return m_params.m_displacement.y;
  case 6:
    return m_params.m_contrast;
  default:
    assert(false);
  }
  return 0;
}

TFilePath TTextureStyle::getParamValue(TColorStyle::TFilePath_tag,
                                       int index) const {
  assert(index == 7);
  return m_texturePath;
}

//-------------------------------------------------------------------

int TTextureStyle::getParamValue(TColorStyle::int_tag, int index) const {
  assert(index == 1);

  return m_params.m_type;
}

//-------------------------------------------------------------------

void TTextureStyle::setParamValue(int index, const TFilePath &value) {
  assert(index == 7);
  m_texturePath = value;
  setAverageColor();
}

//-----------------------------------------------------------------

void TTextureStyle::setParamValue(int index, int value) {
  assert(index == 1);

  m_params.m_type = (TTextureParams::TYPE)value;
}

//-------------------------------------------------------------------

void TTextureStyle::setParamValue(int index, bool value) {
  assert(index == 0);
  m_params.m_isPattern = value;
}

//--------------------------------------------------------------------------

void TTextureStyle::setParamValue(int index, double value) {
  assert(index > 1);
  switch (index) {
  case 0:
    m_params.m_isPattern = (((int)value == 0) ? false : true);
    break;
  case 1:
    m_params.m_type =
        (((int)value == 0) ? TTextureParams::FIXED
                           : ((int)value == 1) ? TTextureParams::AUTOMATIC
                                               : TTextureParams::RANDOM);
    break;
  case 2:
    m_params.m_scale = value;
    break;
  case 3:
    m_params.m_rotation = value;
    break;
  case 4:
    m_params.m_displacement.x = value;
    break;
  case 5:
    m_params.m_displacement.y = value;
    break;
  case 6:
    m_params.m_contrast = value;
    break;
  default:
    assert(false);
  }
}

//------------------------------------------------------------
bool TTextureStyle::loadTextureRaster() {
  if (m_texturePathLoaded != TFilePath() &&
      m_texturePath == m_texturePathLoaded)
    return true;

  m_texturePathLoaded = m_texturePath;

  TFilePath path;
  if (m_texturePath.getParentDir() != TFilePath())  // It's a custom texture
  {
    assert(m_currentScene);
    path = m_currentScene->decodeFilePath(m_texturePath);
    if (path.isLevelName()) {
      TLevelReader lr(path);
      path = path.withFrame(lr.loadInfo()->begin()->first);
    }
  } else  // is a library texture
  {
    path = m_texturePath.withParentDir(m_libraryDir + "textures");
  }

  TRasterP aux;

  if (!TImageReader::load(path, aux)) {
    m_texture = TRaster32P(128, 128);
    m_texture->clear();
    m_texturePathLoaded = TFilePath();
    return false;
  }

  m_texture = aux;
  return true;
}

//----------------------------------------------------------------------------
TRaster32P TTextureStyle::loadTextureRasterWithFrame(int frame) const {
  if (m_texturePathLoaded != TFilePath() &&
      m_texturePath == m_texturePathLoaded &&
      (!m_texturePath.isLevelName() || frame == 0))
    return m_texture->clone();

  TFilePath path;
  if (m_texturePath.getParentDir() != TFilePath())  // It's a custom texture
  {
    assert(m_currentScene);
    path = m_currentScene->decodeFilePath(m_texturePath);
    if (path.isLevelName()) {
      TLevelReader lr(path);
      TLevelP info        = lr.loadInfo();
      TLevel::Iterator it = info->begin();
      // frame = frame % (lr.loadInfo()->getFrameCount());
      std::advance(it, frame % (info->getFrameCount()));

      //
      path = path.withFrame(it->first);
    }
  } else  // is a library texture
  {
    path = m_texturePath.withParentDir(m_libraryDir + "textures");
  }

  TRasterP aux;

  if (!TImageReader::load(path, aux)) {
    aux = TRaster32P(128, 128);
    aux->clear();
  }
  return aux;
}

//---------------------------------------------

void TTextureStyle::loadData(TInputStreamInterface &is) {
  if (is.versionNumber().first < 71) {
    is >> m_texture;
    setTexture(m_texture);
    return;
  }

  std::string path;
  is >> path;
  m_texturePath = TFilePath(path);

  // TOutlineStyle::loadData(is);
  // is >> m_texture;
  loadTextureRaster();

  is >> m_params.m_patternColor;
  m_averageColor = m_params.m_patternColor;

  double value;

  is >> value;
  m_params.m_isPattern = value == 1.0 ? true : false;

  is >> value;
  m_params.m_type =
      (((int)value == 0) ? TTextureParams::FIXED
                         : ((int)value == 1) ? TTextureParams::AUTOMATIC
                                             : TTextureParams::RANDOM);

  is >> m_params.m_scale;
  is >> m_params.m_rotation;
  is >> m_params.m_displacement.x;
  is >> m_params.m_displacement.y;
  is >> m_params.m_contrast;

  delete m_tessellator;
  m_tessellator = new TglTessellator;

  setAverageColor();
}

//------------------------------------------------------------

void TTextureStyle::saveData(TOutputStreamInterface &os) const {
  // TOutlineStyle::saveData(os);
  // os << m_texture;
  std::wstring wstr = m_texturePath.getWideString();
  std::string str;
  str.assign(wstr.begin(), wstr.end());
  os << str;

  os << m_params.m_patternColor;

  os << ((double)m_params.m_isPattern);

  double value = (m_params.m_type == TTextureParams::FIXED)
                     ? 0
                     : ((m_params.m_type == TTextureParams::AUTOMATIC) ? 1 : 2);
  os << value;

  os << m_params.m_scale;
  os << m_params.m_rotation;
  os << m_params.m_displacement.x;
  os << m_params.m_displacement.y;
  os << m_params.m_contrast;
}

//------------------------------------------------------------

int TTextureStyle::getColorParamCount() const { return 1; }

TPixel32 TTextureStyle::getColorParamValue(int index) const {
  return m_params.m_patternColor;
}

void TTextureStyle::setColorParamValue(int index, const TPixel32 &color) {
  m_params.m_patternColor = color;
}
//----------------------------------------------------------------------------
inline void drawdot(TPixel32 *pix, int wrap) {
  for (int i = -wrap; i <= wrap; i += wrap)
    for (int j = -1; j <= 1; j++) *(pix + i + j) = TPixel::Red;
}

void TTextureStyle::fillCustomTextureIcon(const TRaster32P &r) {
  r->fill(TPixel::White);
  int x1, x2, x3;
  x2 = r->getLx() / 2;
  x1 = x2 + ((r->getLx() <= 64) ? 6 : 9);
  x3 = x2 - ((r->getLx() <= 64) ? 6 : 9);

  TPixel32 *pix = r->pixels(r->getLy() / 4);
  drawdot(pix + x1, r->getWrap());
  drawdot(pix + x2, r->getWrap());
  drawdot(pix + x3, r->getWrap());
  // TImageWriter::save(TFilePath("C:\\temp\\boh.png"), r);
  /**(pix+x1) = TPixel::Black;
*(pix+x2) = TPixel::Black;
*(pix+x3) = TPixel::Black;*/
}

void TTextureStyle::makeIcon(const TDimension &outputRect) {
  if (!m_icon || m_icon->getSize() != outputRect) {
    TRaster32P ras(outputRect);
    ras->fill(TPixel::Red);
    m_icon = ras;
  }

  if (!loadTextureRaster()) {
    fillCustomTextureIcon(m_icon);
    // m_icon->fill(TPixel::Green);
    return;
  }
  TRaster32P rTex;

  if (m_params.m_contrast != 1.0 || m_params.m_isPattern) {
    rTex = m_texture->clone();

    if (m_params.m_contrast != 1.0) doContrast(m_params.m_contrast, rTex);

    if (m_params.m_isPattern) doPattern(rTex, m_params.m_patternColor);
  } else
    rTex = m_texture;

  double fitScale = std::min((double)(outputRect.lx) / m_texture->getLx(),
                             (double)(outputRect.ly) / m_texture->getLy());
  TAffine affine =
      TScale(m_params.m_scale * (fitScale)) * TRotation(-m_params.m_rotation);

  if (affine != TAffine()) {
    int border = 2;
    TRaster32P raux(m_icon->getLx() + 2 * border, m_icon->getLy() + 2 * border);
    TRaster32P rin(convert(affine.inv() *
                           TRectD(0, 0, raux->getLx() - 1, raux->getLy() - 1))
                       .getSize());
    tileRaster(rTex, rin);

    TRop::resample(raux, rin,
                   affine.place(rin->getCenterD(), raux->getCenterD()));

    TRop::copy(m_icon,
               raux->extract(border, border, m_icon->getLx() + border - 1,
                             m_icon->getLy() + border - 1));
  } else
    applyTexture(rTex, m_icon, TPoint());
}

//-----------------------------------------------------------------------------

//-----------------------------------------------------------------------------

bool TTextureStyle::doCompute(const Params &params) const {
  TRaster32P rTex = loadTextureRasterWithFrame(params.m_frame);
  TRaster32P r    = params.m_r;
  // TRaster32P rTex = m_texture->clone();

  assert(r);

  if (m_params.m_contrast != 1.0) doContrast(m_params.m_contrast, rTex);

  if (m_params.m_isPattern) doPattern(rTex, m_params.m_patternColor);

  TPoint p = -convert(m_params.m_displacement);

  if (m_params.m_type == TTextureParams::FIXED)
    p += params.m_p;
  else if (m_params.m_type == TTextureParams::RANDOM)
    p += TPoint(Random.getUInt(m_texture->getLx()),
                Random.getUInt(m_texture->getLy()));

  if (m_params.m_scale != 1.0 || m_params.m_rotation != 0.0) {
    TAffine affine = TScale(m_params.m_scale) * TRotation(-m_params.m_rotation);

    if (m_params.m_type == TTextureParams::AUTOMATIC) {
      TRect bBox;
      TRop::computeBBox(r, bBox);

      r = r->extract(bBox);
      p += r->getCenter() - computeCentroid(r);
    }

    p.x = (p.x % m_texture->getLx());
    p.y = (p.y % m_texture->getLy());

    TRect rectIn =
        convert(affine.inv() * TRectD(0, 0, r->getLx() - 1, r->getLy() - 1));
    TRaster32P rin(rectIn.getLx() + abs(p.x), rectIn.getLy() + abs(p.y));
    TRaster32P rout(r->getSize());

    tileRaster(rTex, rin);
    TRop::resample(rout, rin, affine.place(rin->getCenterD(),
                                           rout->getCenterD() - convert(p)));
    rTex = rout;
    p    = TPoint();
  } else {
    if (m_params.m_type == TTextureParams::AUTOMATIC)
      p += rTex->getCenter() - computeCentroid(r);
    p.x = (p.x % m_texture->getLx());
    p.y = (p.y % m_texture->getLy());
  }

  applyTexture(rTex, r, p);

  return true;
}

//------------------------------------------------------------------

namespace {

TRaster32P makeSimpleRaster() {
  TRaster32P ras(2, 2);
  ras->fill(TPixel32::White);
  return ras;
}

TColorStyle::Declaration s2(new TTextureStyle(makeSimpleRaster(), TFilePath()));
}
