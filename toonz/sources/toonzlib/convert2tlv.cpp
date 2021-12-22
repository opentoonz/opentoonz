

#pragma warning(disable : 4533)

#include "tiio_std.h"

#include "tlevel.h"
#include "trasterimage.h"
#include "ttoonzimage.h"
#include "trastercm.h"
#include "tnzimage.h"
#include "tsystem.h"
#include "trop.h"
#include "toonz/fill.h"
#include "toonz/autoclose.h"
#include "tenv.h"
#include "convert2tlv.h"
#include "tstream.h"

#include <map>

#include "toonz/toonzfolders.h"

// gmt, 14/11/2013 removed a commented out blocks of code (void buildInks1(),
// void buildPalette() )

extern TEnv::DoubleVar AutocloseDistance;
extern TEnv::DoubleVar AutocloseAngle;
extern TEnv::IntVar AutocloseOpacity;

namespace {
//----------------------------------------------

inline TPixel unmultiply(const TPixel &in) {
  if (in.r == 255) return in;
  TPixel out;
  int val = in.r * 255 / in.m;
  out.r   = tcrop(val, 0, 255);
  val     = in.g * 255 / in.m;
  out.g   = tcrop(val, 0, 255);
  val     = in.b * 255 / in.m;
  out.b   = tcrop(val, 0, 255);
  out.m   = 255;
  return out;
}

//----------------------------------------------

inline int distance(const TPixel &c1, const TPixel &c2) {
  return (c1.r - c2.r) * (c1.r - c2.r) + (c1.g - c2.g) * (c1.g - c2.g) +
         (c1.b - c2.b) * (c1.b - c2.b);
}

//----------------------------------------------

int findClosest(const std::map<TPixel, int> &colorMap, TPixel &curPixColor) {
  std::map<TPixel, int>::const_iterator it = colorMap.begin();
  int minDistance                          = 1000000000;
  int index                                = -1;
  for (; it != colorMap.end(); ++it) {
    int dist = distance(it->first, curPixColor);
    if (dist < minDistance) {
      minDistance = dist;
      index       = it->second;
    }
  }
  assert(index != -1);
  return index;
}

//----------------------------------------------

#define CHECKCOLOR(r, x, y, tone)                                              \
  {                                                                            \
    TPixelCM32 color = *((TPixelCM32 *)r->pixels(y) + (x));                    \
    if (color.getTone() == tone /*&& color.getPaint()!=0*/)                    \
      return TPoint(x, y);                                                     \
  }

// cerca in quadrati concentrici di raggio rad intorno al pixel in  (x, y)
// il primo pixel di paint puro e ritorna il suo indice di paint

TPoint getClosestToneValue(const TRasterCM32P &r, int y, int x, int tone) {
  int maxRad = std::min({x, r->getLx() - x - 1, y, r->getLy() - y - 1});

  for (int rad = 1; rad < maxRad; rad++) {
    CHECKCOLOR(r, x, y - rad, tone)
    CHECKCOLOR(r, x, y + rad, tone)
    CHECKCOLOR(r, x - rad, y, tone)
    CHECKCOLOR(r, x + rad, y, tone)
    for (int j = 1; j <= rad; j++) {
      CHECKCOLOR(r, x - j, y - rad, tone)
      CHECKCOLOR(r, x + j, y - rad, tone)
      CHECKCOLOR(r, x - j, y + rad, tone)
      CHECKCOLOR(r, x + j, y + rad, tone)

      CHECKCOLOR(r, x - rad, y - j, tone)
      CHECKCOLOR(r, x - rad, y + j, tone)
      CHECKCOLOR(r, x + rad, y - j, tone)
      CHECKCOLOR(r, x + rad, y + j, tone)
    }
  }
  return TPoint(-1, -1);
}

//--------------------------------------------------

TPoint getClosestPurePaint(const TRasterCM32P &r, int y, int x) {
  return getClosestToneValue(r, y, x, 255);
}

//----------------------------------------------------

TPoint getClosestPureInk(const TRasterCM32P &r, int y, int x) {
  return getClosestToneValue(r, y, x, 0);
}

//----------------------------------------------------

bool firstIsUnpainted(const TRaster32P &r1, const TRaster32P &r2) {
  for (int i = 0; i < r1->getLy(); i++) {
    TPixel32 *pix1 = r1->pixels(i);
    TPixel32 *pix2 = r2->pixels(i);
    for (int j = 0; j < r1->getLx(); j++, pix1++, pix2++) {
      if (pix1->m == 255 && pix2->m == 0)
        return false;
      else if (pix1->m == 0 && pix2->m == 255)
        return true;
    }
  }
  return true;
}

//----------------------------------------------
// ritorna -1 se non ha il canale di matte (tutti i pixel a 255)

int getMaxMatte(const TRaster32P &r) {
  int maxMatte   = -1;
  bool withMatte = false;
  for (int i = 0; i < r->getLy(); i++) {
    TPixel32 *pix = r->pixels(i);
    for (int j = 0; j < r->getLx(); j++, pix++) {
      maxMatte = std::max(maxMatte, (int)pix->m);
      if (pix->m != 255) withMatte = true;
    }
  }
  return withMatte ? maxMatte : -1;
}

//----------------------------------------------

void normalize(const TRaster32P &r, int maxMatte) {
  int val;

  for (int i = 0; i < r->getLy(); i++) {
    TPixel32 *pix = r->pixels(i);
    for (int j = 0; j < r->getLx(); j++, pix++) {
      val    = pix->r * 255 / maxMatte;
      pix->r = tcrop(val, 0, 255);
      val    = pix->g * 255 / maxMatte;
      pix->g = tcrop(val, 0, 255);
      val    = pix->b * 255 / maxMatte;
      pix->b = tcrop(val, 0, 255);
      val    = pix->m * 255 / maxMatte;
      pix->m = tcrop(val, 0, 255);
    }
  }
}

//----------------------------------------------------

int getFramesCount(const TLevelP &l, int from, int to) {
  if (from == -1) return l->getFrameCount();

  int count           = 0;
  TLevel::Iterator it = l->begin();
  while (it != l->end() && it->first.getNumber() < from) it++;
  while (it != l->end() && it->first.getNumber() <= to) it++, count++;
  return count;
}
}  // namespace
// namespace

std::map<TPixel, int>::const_iterator Convert2Tlv::findNearestColor(
    const TPixel &color) {
  // assert((int)colorMap.size()>toIndex);
  // assert((int)colorMap.size()>fromIndex);
  std::map<TPixel, int>::const_iterator ret = m_colorMap.end(),
                                        it  = m_colorMap.begin();
  // std::advance(it, fromIndex);

  int mindist = 1000;
  for (; it != m_colorMap.end(); ++it) {
    const TPixel &curr = it->first;
    int dr             = abs(curr.r - color.r);
    if (dr > m_colorTolerance) continue;
    int dg = abs(curr.g - color.g);
    if (dg > m_colorTolerance) continue;
    int db = abs(curr.b - color.b);
    if (db > m_colorTolerance) continue;
    int dist = dr + dg + db;
    if (dist < mindist) {
      mindist = dist;
      ret     = it;
    }
  }
  return ret;
}

//-------------------------------------------------------------------

void Convert2Tlv::buildInks(TRasterCM32P &rout, const TRaster32P &rin) {
  std::map<TPixel, int>::const_iterator it;
  TPixel curColor = TPixel::Transparent;
  int i, j;
  int curIndex;

  // prima passata: identifico i colori di inchiostro e metto in rout i pixel di
  // inchiostro puro
  for (i = 0; i < rin->getLy(); i++) {
    TPixel *pixin      = rin->pixels(i);
    TPixelCM32 *pixout = rout->pixels(i);
    for (j = 0; j < rin->getLx(); j++, pixin++, pixout++) {
      TPixel colorIn;

      if (pixin->m != 255) continue;

      if (curColor != *pixin) {
        curColor = *pixin;
        if ((it = m_colorMap.find(curColor)) == m_colorMap.end()) {
          if (m_colorTolerance > 0) it = findNearestColor(curColor);
          // if (it==colorMap.end() && (int)colorMap.size()>origColorCount)
          //	it  = findNearestColor(curColor, colorMap, colorTolerance,
          // origColorCount, colorMap.size()-1);
          if (it == m_colorMap.end() && m_lastIndex < 4095) {
            m_colorMap[curColor] = ++m_lastIndex;
            curIndex             = m_lastIndex;
          } else if (it != m_colorMap.end()) {
            m_colorMap[curColor] = it->second;
            curIndex             = it->second;
          }
        } else
          curIndex = it->second;
      }
      *pixout = TPixelCM32(curIndex, 0, 0);
    }
  }

  // seconda  passata: metto gli inchiostri di antialiasing
  curColor = TPixel::Transparent;

  for (i = 0; i < rin->getLy(); i++) {
    TPixel *pixin      = rin->pixels(i);
    TPixelCM32 *pixout = rout->pixels(i);
    for (j = 0; j < rin->getLx(); j++, pixin++, pixout++) {
      TPixel colorIn;
      if (pixin->m == 255)  // gia' messo nel ciclo precedente
        continue;
      if (pixin->m == 0) continue;

      colorIn = unmultiply(*pixin);  // findClosestOpaque(rin, i, j);

      if (curColor != colorIn) {
        curColor = colorIn;
        if ((it = m_colorMap.find(curColor)) != m_colorMap.end())
          curIndex = it->second;
        else
          curIndex = findClosest(m_colorMap, curColor);
      }
      *pixout = TPixelCM32(curIndex, 0, 255 - pixin->m);
    }
  }
}

//----------------------------------------------

void Convert2Tlv::removeAntialias(TRasterCM32P &r) {
  int threshold = (int)(m_antialiasValue * 255.0 / 100.0);
  int tone;
  for (int i = 0; i < r->getLy(); i++) {
    TPixelCM32 *pix = r->pixels(i);
    for (int j = 0; j < r->getLx(); j++, pix++)
      if ((tone = pix->getTone()) !=
          0xff)  // tone==ff e tone==0 non vanno toccati mai
        pix->setTone(tone > threshold ? 0xff : 0);
  }
}

//------------------------------------------------------------------

void Convert2Tlv::buildInksFromGrayTones(TRasterCM32P &rout,
                                         const TRasterP &rin) {
  int i, j;

  TRasterGR8P r8 = (TRasterGR8P)rin;
  TRaster32P r32 = (TRaster32P)rin;
  if (r8)
    for (i = 0; i < rin->getLy(); i++) {
      TPixelGR8 *pixin   = r8->pixels(i);
      TPixelCM32 *pixout = rout->pixels(i);
      for (j = 0; j < rin->getLx(); j++, pixin++, pixout++)
        *pixout = TPixelCM32(1, 0, pixin->value);
    }
  else
    for (i = 0; i < rin->getLy(); i++) {
      TPixel *pixin      = r32->pixels(i);
      TPixelCM32 *pixout = rout->pixels(i);
      for (j = 0; j < rin->getLx(); j++, pixin++, pixout++)
        *pixout = TPixelCM32(1, 0, TPixelGR8::from(*pixin).value);
    }
}

//----------------------------------------------------------------------

void Convert2Tlv::buildInksForNAAImage(TRasterCM32P &rout,
                                       const TRaster32P &rin) {
  std::map<TPixel, int>::iterator it;
  TPixel curColor = TPixel::Transparent;
  int i, j;
  int curIndex;

  // prima passata: identifico i colori di inchiostro e metto in rout i pixel di
  // inchiostro puro
  for (i = 0; i < rin->getLy(); i++) {
    TPixel *pixin      = rin->pixels(i);
    TPixelCM32 *pixout = rout->pixels(i);
    for (j = 0; j < rin->getLx(); j++, pixin++, pixout++) {
      TPixel colorIn;

      /*- treat white/transparent pixels as transparent -*/
      if (*pixin == TPixel(255, 255, 255) || *pixin == TPixel::Transparent) {
        *pixout = TPixelCM32(0, 0, 255);
        continue;
      }

      if (curColor != *pixin) {
        curColor = *pixin;
        if ((it = m_colorMap.find(curColor)) == m_colorMap.end()) {
          if (m_lastIndex < 4095) m_colorMap[curColor] = ++m_lastIndex;
          curIndex = m_lastIndex;
        } else
          curIndex = it->second;
      }
      *pixout = TPixelCM32(curIndex, 0, 0);
    }
  }

  if (m_colorMap.empty()) m_colorMap[TPixel::Black] = ++m_lastIndex;
}

//----------------------------------------------

void Convert2Tlv::doFill(TRasterCM32P &rout, const TRaster32P &rin) {
  // prima passata: si filla  solo partendo da pixel senza inchiostro, senza
  // antialiasing(tone==255)
  for (int i = 0; i < rin->getLy(); i++) {
    TPixel *pixin      = rin->pixels(i);
    TPixelCM32 *pixout = rout->pixels(i);
    for (int j = 0; j < rin->getLx(); j++, pixin++, pixout++) {
      if (!(pixout->getTone() == 255 && pixout->getPaint() == 0 &&
            pixin->m == 255))
        continue;

      std::map<TPixel, int>::const_iterator it;
      int paintIndex;
      if ((it = m_colorMap.find(*pixin)) == m_colorMap.end()) {
        if (m_colorTolerance > 0) it = findNearestColor(*pixin);
        // if (it==colorMap.end() && (int)colorMap.size()>origColorCount) //se
        // non l'ho trovato tra i colori origari, lo cerco in quelli nuovi, ma
        // in questo caso deve essere esattamente uguale(tolerance = 0)
        //	 it  = findNearestColor(*pixin, colorMap, colorTolerance,
        // origColorCount, colorMap.size()-1);

        if (it == m_colorMap.end() && m_lastIndex < 4096) {
          m_colorMap[*pixin] = ++m_lastIndex;
          paintIndex         = m_lastIndex;
        } else if (it != m_colorMap.end()) {
          m_colorMap[*pixin] = it->second;
          paintIndex         = it->second;
        }
      } else
        paintIndex = it->second;
      FillParameters params;
      params.m_p         = TPoint(j, i);
      params.m_styleId   = paintIndex;
      params.m_emptyOnly = true;
      fill(rout, params);
      // if (*((ULONG *)rout->getRawData())!=0xff)
      //  {
      //  int cavolo=0;
      //  }
    }
  }

  // seconda passata: se son rimasti pixel antialiasati non fillati, si fillano,
  // cercando nelle vicinanze un pixel di paint puro per capire il colore da
  // usare
  for (int i = 0; i < rin->getLy(); i++) {
    TPixel *pixin      = rin->pixels(i);
    TPixelCM32 *pixout = rout->pixels(i);
    for (int j = 0; j < rin->getLx(); j++, pixin++, pixout++) {
      if (!(pixout->getTone() > 0 && pixout->getTone() < 255 &&
            pixout->getPaint() == 0 && pixin->m == 255))
        continue;

      TPoint p = getClosestPurePaint(rout, i, j);
      if (p.x == -1) continue;

      // pixout->setPaint( paintIndex);
      FillParameters params;
      params.m_p         = TPoint(j, i);
      params.m_styleId   = (rout->pixels(p.y) + p.x)->getPaint();
      params.m_emptyOnly = true;

      fill(rout, params);
    }
  }

  // infine, si filla di trasparente lo sfondo, percorrendo il bordo, nel caso
  // di trasbordamenti di colore
  TPixelCM32 *pixCm;
  TPixel *pix;

  pixCm = rout->pixels(0);
  pix   = rin->pixels(0);
  FillParameters params;
  params.m_styleId = 0;

  for (int i = 0; i < rout->getLx(); i++, pixCm++, pix++)
    if (pixCm->getTone() == 255 && pixCm->getPaint() != 0 && pix->m == 0) {
      params.m_p = TPoint(i, 0);
      fill(rout, params);
    }

  pixCm = rout->pixels(rout->getLy() - 1);
  pix   = rin->pixels(rout->getLy() - 1);
  for (int i = 0; i < rout->getLx(); i++, pixCm++, pix++)
    if (pixCm->getTone() == 255 && pixCm->getPaint() != 0 && pix->m == 0) {
      params.m_p = TPoint(i, rout->getLy() - 1);
      fill(rout, params);
    }
  int wrapCM = rout->getWrap();
  int wrap   = rin->getWrap();

  pixCm = rout->pixels(0);
  pix   = rin->pixels(0);
  for (int i = 0; i < rin->getLy(); i++, pixCm += wrapCM, pix += wrap)
    if (pixCm->getTone() == 255 && pixCm->getPaint() != 0 && pix->m == 0) {
      params.m_p = TPoint(0, i);
      fill(rout, params);
    }
  pixCm = rout->pixels(0) + rout->getLx() - 1;
  pix   = rin->pixels(0) + rin->getLx() - 1;
  for (int i = 0; i < rin->getLy(); i++, pixCm += wrapCM, pix += wrap)
    if (pixCm->getTone() == 255 && pixCm->getPaint() != 0 && pix->m == 0) {
      params.m_p = TPoint(rout->getLx() - 1, i);
      fill(rout, params);
    }
}

//----------------------------------------------

void Convert2Tlv::buildToonzRaster(TRasterCM32P &rout, const TRasterP &rin1,
                                   const TRasterP &rin2) {
  if (rin2) assert(rin1->getSize() == rin2->getSize());

  rout->clear();

  std::cout << "      computing inks...\n";
  TRaster32P r1    = (TRaster32P)rin1;
  TRasterGR8P r1gr = (TRasterGR8P)rin1;
  TRaster32P r2    = (TRaster32P)rin2;
  TRasterGR8P r2gr = (TRasterGR8P)rin2;
  TRasterP rU, rP;

  if (r1gr) {
    rU = r1gr;
    rP = r2;
  } else if (r2gr) {
    rU = r2gr;
    rP = r1;
  } else if (!r1)
    rU = r2;
  else if (!r2)
    rU = r1;
  else if (firstIsUnpainted(r1, r2)) {
    rU = r1;
    rP = r2;
  } else {
    rU = r2;
    rP = r1;
  }

  TRasterCM32P r;
  if (rout->getSize() != rU->getSize()) {
    int dx = rout->getLx() - rU->getLx();
    int dy = rout->getLy() - rU->getLy();
    assert(dx >= 0 && dy >= 0);

    r = rout->extract(dx / 2, dy / 2, dx / 2 + rU->getLx() - 1,
                      dy / 2 + rU->getLy() - 1);
  } else
    r = rout;

  if ((TRasterGR8P)rU)
    buildInksFromGrayTones(r, rU);
  else if (m_isUnpaintedFromNAA)
    buildInksForNAAImage(r, (TRaster32P)rU);
  else {
    int maxMatte = getMaxMatte((TRaster32P)rU);
    if (maxMatte == -1)
      buildInksFromGrayTones(r, rU);
    else if (maxMatte == 0)  // empty frame doesn't need further computation
      return;
    else {
      if (maxMatte < 255) normalize(rU, maxMatte);
      buildInks(r, (TRaster32P)rU /*rP,*/);
    }
  }

  if (m_autoclose)
    TAutocloser(r, AutocloseDistance, AutocloseAngle, 1, AutocloseOpacity)
        .exec();

  if (rP) {
    std::cout << "      computing paints...\n";
    doFill(r, rP);
  }
  if (m_antialiasType == 2)  // remove antialias
    removeAntialias(r);
  else if (m_antialiasType == 1)  // add antialias
  {
    TRasterCM32P raux(r->getSize());
    TRop::antialias(r, raux, 10, m_antialiasValue);
    rout = raux;
  }
}

//----------------------------------------------

TPalette *Convert2Tlv::buildPalette() {
  m_palette->setDirtyFlag(true);  // make sure to be overwritten

  std::map<TPixel, int>::const_iterator it = m_colorMap.begin();
  TPalette::Page *page                     = m_palette->getPage(0);

  QList<int> stylesToBeAddedToPage;

  for (; it != m_colorMap.end(); ++it) {
    if (it->second >
        m_maxPaletteIndex)  // colore nuovo da aggiungere alla paletta)
    {
      if (m_palette->getStyleCount() > it->second)
        m_palette->setStyle(it->second, it->first);
      else {
        while (m_palette->getStyleCount() < it->second)
          m_palette->addStyle(TPixel::Transparent);
        int id = m_palette->addStyle(it->first);
        assert(id == it->second);
      }
    }
    if (!m_palette->getStylePage(it->second))
      stylesToBeAddedToPage.push_back(it->second);
  }

  /*- インデックス順にページに格納する -*/
  if (!stylesToBeAddedToPage.isEmpty()) {
    std::sort(stylesToBeAddedToPage.begin(), stylesToBeAddedToPage.end());
    for (int s = 0; s < stylesToBeAddedToPage.size(); s++)
      page->addStyle(stylesToBeAddedToPage.at(s));
  }

  /*
    If the palette path is empty, an initial palette with four colors are set in
    the palette here.
    ( see Convert2Tlv::init() ) So here I make the latter three styles in the
    initial palette to set
    "AutoPaint" options.
  */
  if (m_palettePath.isEmpty()) {
    assert(m_palette->getStyleCount() >= 5);
    for (int id = 2; id <= 4; id++) m_palette->getStyle(id)->setFlags(1);
  }

  if (!m_appendDefaultPalette) return m_palette;

  /*-- Adding styles in the default palette to the result palette, starts here
   * --*/
  TFilePath palettePath =
      ToonzFolder::getStudioPaletteFolder() + "cleanup_default.tpl";
  TFileStatus pfs(palettePath);

  if (!pfs.doesExist() || !pfs.isReadable()) return m_palette;

  TIStream is(palettePath);
  if (!is) return m_palette;

  std::string tagName;
  if (!is.matchTag(tagName) || tagName != "palette") return m_palette;

  std::string gname;
  is.getTagParam("name", gname);
  TPalette *defaultPalette = new TPalette();
  defaultPalette->loadData(is);

  m_palette->setIsCleanupPalette(false);

  TPalette::Page *dstPage = m_palette->getPage(0);
  TPalette::Page *srcPage = defaultPalette->getPage(0);

  for (int srcIndexInPage = 0; srcIndexInPage < srcPage->getStyleCount();
       srcIndexInPage++) {
    int id = srcPage->getStyleId(srcIndexInPage);

    bool isUsedInDstPalette = false;

    for (int dstIndexInPage = 0; dstIndexInPage < dstPage->getStyleCount();
         dstIndexInPage++) {
      if (dstPage->getStyleId(dstIndexInPage) == id) {
        isUsedInDstPalette = true;
        break;
      }
    }

    if (isUsedInDstPalette)
      continue;
    else {
      int addedId =
          m_palette->addStyle(srcPage->getStyle(srcIndexInPage)->clone());
      dstPage->addStyle(addedId);
      /*-- StudioPalette由来のDefaultPaletteの場合、GrobalNameを消去する --*/
      m_palette->getStyle(addedId)->setGlobalName(L"");
      m_palette->getStyle(addedId)->setOriginalName(L"");
    }
  }
  delete defaultPalette;
  /*-- Adding styles in the default palette to the result palette, ends here
   * --*/

  return m_palette;
}

//------------------------------------------------------------------------------

Convert2Tlv::Convert2Tlv(const TFilePath &filepath1, const TFilePath &filepath2,
                         const TFilePath &outFolder, const QString &outName,
                         int from, int to, bool doAutoclose,
                         const TFilePath &palettePath, int colorTolerance,
                         int antialiasType, int antialiasValue,
                         bool isUnpaintedFromNAA, bool appendDefaultPalette,
                         double dpi)
    : m_size(0, 0)
    , m_level1()
    , m_levelIn1()
    , m_levelIn2()
    , m_levelOut()
    , m_autoclose(doAutoclose)
    , m_premultiply(false)
    , m_count(0)
    , m_from(from)
    , m_to(to)
    , m_palettePath(palettePath)
    , m_colorTolerance(colorTolerance)
    , m_palette(0)
    , m_antialiasType(antialiasType)
    , m_antialiasValue(antialiasValue)
    , m_isUnpaintedFromNAA(isUnpaintedFromNAA)
    , m_appendDefaultPalette(appendDefaultPalette)
    , m_dpi(dpi) {
  if (filepath1 != TFilePath()) {
    m_levelIn1 = filepath1.getParentDir() + filepath1.getLevelName();
    if (outFolder != TFilePath())
      m_levelOut =
          m_levelIn1.withParentDir(outFolder).withNoFrame().withType("tlv");
    else
      m_levelOut = m_levelIn1.withNoFrame().withType(
          "tlv");  // filePaths[0].getParentDir() +
                   // TFilePath(filePaths[0].getWideName() + L".tlv");

    if (outName != "") m_levelOut = m_levelOut.withName(outName.toStdString());
  }

  if (filepath2 != TFilePath())
    m_levelIn2 = filepath2.getParentDir() + filepath2.getLevelName();
}

//-------------------------------------------------------------------------------------

int Convert2Tlv::getFramesToConvertCount() {
  if (m_level1 && m_level1->getFrameCount() > 0)
    return getFramesCount(m_level1, m_from,
                          m_to);  // m_level1->getFrameCount();
  else {
    try {
      TLevelReaderP lr = TLevelReaderP(m_levelIn1);
      if (lr) {
        TLevelP l = lr->loadInfo();
        if (l) {
          return getFramesCount(l, m_from, m_to);
        }
      }
    } catch (...) {
      return 0;
    }
  }
  return 0;
}

//---------------------------------------------

bool Convert2Tlv::init(std::string &errorMessage) {
  m_lastIndex = m_maxPaletteIndex = 0;
  m_colorMap.clear();

  try {
    m_lr1 = TLevelReaderP(m_levelIn1);
    if (m_lr1) m_level1 = m_lr1->loadInfo();
  } catch (...) {
    errorMessage =
        "Error: can't read level " + ::to_string(m_levelIn1.getWideString());
    return false;
  }

  if (m_level1->getFrameCount() == 0) {
    errorMessage =
        "Error: can't find level " + ::to_string(m_levelIn1.getWideString());
    return false;
  }

  TLevelP level2;

  if (m_levelIn2 != TFilePath()) {
    try {
      m_lr2 = TLevelReaderP(m_levelIn2);
      if (m_lr2) level2 = m_lr2->loadInfo();
    } catch (...) {
      errorMessage =
          "Error: can't read level " + ::to_string(m_levelIn2.getWideString());
      return false;
    }

    if (level2->getFrameCount() == 0) {
      errorMessage =
          "Error: can't find level " + ::to_string(m_levelIn2.getWideString());
      return false;
    }

    if (m_level1->getFrameCount() != level2->getFrameCount()) {
      errorMessage = "Error: the two input levels must have same frame number";
      return false;
    }
  }

  m_size = TDimension();

  m_lw = TLevelWriterP(m_levelOut);
  m_it = m_level1->begin();

  TLevel::Iterator it2;

  if (level2->getFrameCount() > 0) it2 = level2->begin();

  for (; m_it != m_level1->end(); ++m_it) {
    TImageReaderP ir1       = m_lr1->getFrameReader(m_it->first);
    const TImageInfo *info1 = ir1->getImageInfo();
    if (!info1) {
      errorMessage = "Error: can't read frame " +
                     std::to_string(m_it->first.getNumber()) + " of level  " +
                     ::to_string(m_levelIn1.getWideString());
      return false;
    }

    if (info1->m_bitsPerSample != 8) {
      errorMessage = "Error: all frames must have 8 bits per channel!\n";
      return false;
    }
    m_size.lx = std::max(m_size.lx, info1->m_lx);
    m_size.ly = std::max(m_size.ly, info1->m_ly);

    if (m_lr2 != TLevelReaderP()) {
      TImageReaderP ir2 = m_lr2->getFrameReader(it2->first);

      if (ir2) {
        const TImageInfo *info2 = ir2->getImageInfo();
        if (!info1) {
          errorMessage = "Error: can't read frame " +
                         std::to_string(it2->first.getNumber()) +
                         " of level  " +
                         ::to_string(m_levelIn2.getWideString());
          ;
          return false;
        }

        if (info1->m_lx != info2->m_lx || info1->m_ly != info2->m_ly) {
          errorMessage =
              "Error: painted frames must have same resolution of matching "
              "unpainted frames!\n";
          return false;
        }
        if (info2->m_bitsPerSample != 8) {
          errorMessage = "Error: all frames must have 8 bits per channel!\n";
          return false;
        }
      }
      ++it2;
    }
  }

  m_palette = new TPalette();

  if (m_palettePath != TFilePath()) {
    TIStream is(m_palettePath);
    is >> m_palette;
    if (m_palette->getStyleInPagesCount() == 0) {
      errorMessage = "Error: invalid palette!\n";

      return false;
    }
    for (int i = 0; i < m_palette->getStyleCount(); i++)
      if (m_palette->getStylePage(i)) {
        m_colorMap[m_palette->getStyle(i)->getMainColor()] = i;
        if (i > m_lastIndex) m_lastIndex = i;
      }
    assert(m_colorMap.size() == m_palette->getStyleInPagesCount());
  }

  m_maxPaletteIndex = m_lastIndex;

  m_it = m_level1->begin();

  /*-
  If the palette is empty, make an initial palette with black, red, blue and
  green styles.
  For the latter three styles the "autopaint" option should be set.
  -*/
  if (m_lastIndex == 0) {
    m_colorMap[TPixel::Black] = ++m_lastIndex;
    m_colorMap[TPixel::Red]   = ++m_lastIndex;
    m_colorMap[TPixel::Blue]  = ++m_lastIndex;
    m_colorMap[TPixel::Green] = ++m_lastIndex;
  }

  return true;
}

//----------------------------------------------------------------------------------------

bool Convert2Tlv::convertNext(std::string &errorMessage) {
  if (m_count == 0 && m_from != -1)
    while (m_it != m_level1->end() && m_it->first.getNumber() < m_from) m_it++;

  std::cout << "Processing image " << ++m_count << " of "
            << getFramesCount(m_level1, m_from, m_to) << "...\n";
  std::cout << "      Loading frame " << m_it->first.getNumber() << "...\n";
  TImageReaderP ir1    = m_lr1->getFrameReader(m_it->first);
  TRasterImageP imgIn1 = (TRasterImageP)ir1->load();
  if (!imgIn1) {
    errorMessage = "Error: cannot read frame" +
                   std::to_string(m_it->first.getNumber()) + " of " +
                   ::to_string(m_levelIn1.getWideString()) + "!";
    return false;
  }
  TRasterP rin1 = imgIn1->getRaster();

  assert((TRaster32P)rin1 || (TRasterGR8P)rin1);

  TRasterP rin2;
  TRasterImageP imgIn2;

  if (m_lr2) {
    TImageReaderP ir2 = m_lr2->getFrameReader(m_it->first);
    imgIn2            = (TRasterImageP)ir2->load();
    if (!imgIn2) {
      errorMessage = "Error: cannot read frame " +
                     std::to_string(m_it->first.getNumber()) + " of " +
                     ::to_string(m_levelIn2.getWideString()) + "!";
      return false;
    }
    rin2 = imgIn2->getRaster();
    assert((TRaster32P)rin2 || (TRasterGR8P)rin2);
  }

  TRasterCM32P rout(m_size);
  buildToonzRaster(rout, rin1, rin2);

  std::cout << "      saving frame in level \'" << m_levelOut.getLevelName()
            << "\'...\n\n";
  TImageWriterP iw  = m_lw->getFrameWriter(m_it->first);
  TToonzImageP timg = TToonzImageP(rout, rout->getBounds());

  TRect bbox;
  TRop::computeBBox(rout, bbox);
  timg->setSavebox(bbox);

  if (m_dpi > 0.0)  // specify dpi in the convert popup
    timg->setDpi(m_dpi, m_dpi);
  else {
    double dpix, dpiy;
    imgIn1->getDpi(dpix, dpiy);
    timg->setDpi(dpix, dpiy);
  }

  TLevel::Iterator itaux = m_it;
  itaux++;
  if (itaux == m_level1->end() ||
      (m_to != -1 &&
       itaux->first.getNumber() > m_to))  // ultimo frame da scrivere.
    timg->setPalette(buildPalette());

  iw->save(timg);

  ++m_it;
  return true;
}

//----------------------------------------------------------------------------------------------

bool Convert2Tlv::abort() {
  try {
    m_lr1    = TLevelReaderP();
    m_lr2    = TLevelReaderP();
    m_lw     = TLevelWriterP();
    m_level1 = TLevelP();

    std::cout << "No output generated\n";
    TSystem::deleteFile(m_levelOut);
    TSystem::deleteFile(m_levelOut.withType("tpl"));
    return false;
  } catch (...) {
    return false;
  }
}

//==============================================================================================
//
// RasterToToonzRasterConverter
//
//----------------------------------------------------------------------------------------------

RasterToToonzRasterConverter::RasterToToonzRasterConverter() {
  m_palette = new TPalette();
}

RasterToToonzRasterConverter::~RasterToToonzRasterConverter() {}

void RasterToToonzRasterConverter::setPalette(const TPaletteP &palette) {
  m_palette = palette;
}

TRasterCM32P RasterToToonzRasterConverter::convert(
    const TRasterP &inputRaster) {
  int lx = inputRaster->getLx();
  int ly = inputRaster->getLy();

  TRaster32P r = inputRaster;
  /*
TRasterGR8P r1gr = (TRasterGR8P)inputRaster;
TRasterP rU, rP;
*/

  TRasterCM32P rout(lx, ly);

  for (int y = 0; y < ly; y++) {
    TPixel32 *pixin    = r->pixels(y);
    TPixel32 *pixinEnd = pixin + lx;
    TPixelCM32 *pixout = rout->pixels(y);
    while (pixin < pixinEnd) {
      int v = (pixin->r + pixin->g + pixin->b) / 3;
      ++pixin;
      *pixout++ = TPixelCM32(1, 0, v);
    }
  }
  return rout;
}

TRasterCM32P RasterToToonzRasterConverter::convert(
    const TRasterP &inksInputRaster, const TRasterP &paintInputRaster) {
  return TRasterCM32P();
}

TToonzImageP RasterToToonzRasterConverter::convert(const TRasterImageP &ri) {
  TRasterCM32P ras = convert(ri->getRaster());
  if (ras)
    return TToonzImageP(ras, TRect(ras->getBounds()));
  else
    return TToonzImageP();
}
