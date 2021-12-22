

#if defined(_MSC_VER) && (_MSC_VER >= 1400)
#define _CRT_SECURE_NO_DEPRECATE 1
#endif

#include "psd.h"
#include "trasterimage.h"
#include "trop.h"
#include "tpixelutils.h"

/*
  The entire content of this file is ridden with LEAKS. A bug has been filed,
  will hopefully
  be dealt with ASAP.


  L'intero contenuto del file e' pieno di LEAK. Ho inserito la cosa in Bugilla e
  non ho potuto
  fare altro sotto rilascio. Non solo, il codice dei distruttori e' SBAGLIATO -
  l'ho esplicitamente
  disabilitato, tanto non era chiamato cmq.

  E' da rifare usando SMART POINTERS (boost::scoped_ptr<> o tcg::unique_ptr<>, o
  std::unique_ptr<> se facciamo l'upgrade del compilatore).

  Da osservare che anche il campo FILE in TPSDReader leaka se viene sganciata
  un'eccezione...
*/

#define LEVEL_NAME_INDEX_SEP "@"

//----forward declarations
std::string buildErrorString(int error);

void readChannel(FILE *f, TPSDLayerInfo *li, TPSDChannelInfo *chan,
                 int channels, TPSDHeaderInfo *h);
void readLongData(FILE *f, struct dictentry *parent, TPSDLayerInfo *li);
void readByteData(FILE *f, struct dictentry *parent, TPSDLayerInfo *li);
void readKey(FILE *f, struct dictentry *parent, TPSDLayerInfo *li);
void readLayer16(FILE *f, struct dictentry *parent, TPSDLayerInfo *li);
//----end forward declarations

static char swapByte(unsigned char src) {
  unsigned char out = 0;
  for (int i = 0; i < 8; ++i) {
    out = out << 1;
    out |= (src & 1);
    src = src >> 1;
  }
  return out;
}

TPSDReader::TPSDReader(const TFilePath &path)
    : m_shrinkX(1), m_shrinkY(1), m_region(TRect()) {
  m_layerId    = 0;
  QString name = path.getName().c_str();
  name.append(path.getDottedType().c_str());
  int sepPos = name.indexOf("#");
  int dotPos = name.indexOf(".", sepPos);
  name.remove(sepPos, dotPos - sepPos);
  m_path = path.getParentDir() + TFilePath(name.toStdString());
  // m_path = path;
  QMutexLocker sl(&m_mutex);
  openFile();
  if (!doInfo()) {
    fclose(m_file);
    throw TImageException(m_path, "Do PSD INFO ERROR");
  }
  fclose(m_file);
}
TPSDReader::~TPSDReader() {
  /*for(int i=0; i<m_headerInfo.layersCount;i++)
free(m_headerInfo.linfo + i);*/

  // NO - L'istruzione sopra e' SBAGLIATA, si tratta di uno pseudo-array
  // allocato in blocco
  // con una SINGOLA malloc() - quindi deve dovrebbe essere deallocato con una
  // SINGOLA free().

  // Se fino ad ora funzionava e' perche' questa funzione NON VENIVA MAI
  // CHIAMATA -
  // LEAKAVA TUTTO.

  // Non solo, ma il CONTENUTO delle singole strutture - tra cui altri array -
  // non viene
  // deallocato. Non ho idea se sia garantito che venga deallocato prima di
  // arrivare qui,
  // ma NON E' AFFATTO SICURO.
}
int TPSDReader::openFile() {
  m_file = fopen(m_path, "rb");
  if (!m_file) throw TImageException(m_path, buildErrorString(2));
  return 0;
}
bool TPSDReader::doInfo() {
  // Read Header Block
  if (!doHeaderInfo()) return false;
  // Read Color Mode Data Block
  if (!doColorModeData()) return false;
  // Read Image Resources Block
  if (!doImageResources()) return false;
  // Read Layer Info Block (Layers Number and merged alpha)
  if (!doLayerAndMaskInfo()) return false;
  // Read Layers and Mask Information Block
  // if(!doLayersInfo()) return false;
  m_headerInfo.layerDataPos = ftell(m_file);

  if (m_headerInfo.layersCount == 0)  // tento con extra data
  {
    fseek(m_file, m_headerInfo.layerDataPos, SEEK_SET);
    skipBlock(m_file);  // skip global layer mask info
    psdByte currentPos = ftell(m_file);
    psdByte len        = m_headerInfo.lmistart + m_headerInfo.lmilen -
                  currentPos;  // 4 = bytes skipped by global layer mask info
    doExtraData(NULL, len);
  }
  return true;
}
// Read Header Block
bool TPSDReader::doHeaderInfo() {
  fread(m_headerInfo.sig, 1, 4, m_file);
  m_headerInfo.version = read2UBytes(m_file);
  read4Bytes(m_file);
  read2Bytes(m_file);  // reserved[6];
  m_headerInfo.channels = read2UBytes(m_file);
  m_headerInfo.rows     = read4Bytes(m_file);
  m_headerInfo.cols     = read4Bytes(m_file);
  m_headerInfo.depth    = read2UBytes(m_file);
  m_headerInfo.mode     = read2UBytes(m_file);

  if (!feof(m_file) && !memcmp(m_headerInfo.sig, "8BPS", 4)) {
    if (m_headerInfo.version == 1) {
      if (m_headerInfo.channels <= 0 || m_headerInfo.channels > 64 ||
          m_headerInfo.rows <= 0 || m_headerInfo.cols <= 0 ||
          m_headerInfo.depth < 0 || m_headerInfo.depth > 32 ||
          m_headerInfo.mode < 0) {
        throw TImageException(m_path, "Reading PSD Header Info error");
        return false;
      }
    } else {
      throw TImageException(m_path, "PSD Version not supported");
      return false;
    }
  } else {
    throw TImageException(m_path, "Cannot read Header");
    return false;
  }
  return true;
}
// Read Color Mode Data Block
bool TPSDReader::doColorModeData() {
  m_headerInfo.colormodepos = ftell(m_file);
  skipBlock(m_file);  // skip "color mode data"
  return true;
}
// Read Image Resources Block
bool TPSDReader::doImageResources() {
  // skipBlock(m_file); //skip "image resources"
  long len = read4Bytes(m_file);  // lunghezza del blocco Image resources
  while (len > 0) {
    char type[4], name[0x100];
    int id, namelen;
    long size;
    fread(type, 1, 4, m_file);
    id      = read2Bytes(m_file);
    namelen = fgetc(m_file);
    fread(name, 1, NEXT2(1 + namelen) - 1, m_file);
    name[namelen] = 0;
    size          = read4Bytes(m_file);
    if (id == 1005)  // ResolutionInfo
    {
      psdByte savepos = ftell(m_file);
      long hres, vres;
      double hresd, vresd;
      hresd = FIXDPI(hres = read4Bytes(m_file));
      read2Bytes(m_file);
      read2Bytes(m_file);
      vresd = FIXDPI(vres = read4Bytes(m_file));
      m_headerInfo.vres = vresd;
      m_headerInfo.hres = hresd;
      fseek(m_file, savepos, SEEK_SET);
    }
    len -= 4 + 2 + NEXT2(1 + namelen) + 4 + NEXT2(size);
    fseek(m_file, NEXT2(size), SEEK_CUR);  // skip resource block data
  }
  if (len != 0) return false;
  return true;
}
// Read Layer Info Block (Layers Number and merged alpha)
bool TPSDReader::doLayerAndMaskInfo() {
  psdByte layerlen;

  m_headerInfo.layersCount = 0;
  m_headerInfo.lmilen      = read4Bytes(m_file);
  m_headerInfo.lmistart    = ftell(m_file);
  if (m_headerInfo.lmilen) {
    // process layer info section
    layerlen                     = read4Bytes(m_file);
    m_headerInfo.linfoBlockEmpty = false;
    m_headerInfo.mergedalpha     = 0;
    if (layerlen) {
      doLayersInfo();
    } else {
      // WARNING: layer info section empty
    }
  } else {
    // WARNING: layer & mask info section empty
  }
  return true;
}

// Read Layers Information Block
// It is called by doLayerAndMaskInfo()
bool TPSDReader::doLayersInfo() {
  m_headerInfo.layersCount     = read2Bytes(m_file);
  m_headerInfo.linfoBlockEmpty = false;
  m_headerInfo.mergedalpha     = m_headerInfo.layersCount < 0;
  if (m_headerInfo.mergedalpha > 0) {
    m_headerInfo.layersCount = -m_headerInfo.layersCount;
  }
  if (!m_headerInfo.linfoBlockEmpty) {
    m_headerInfo.linfo = (TPSDLayerInfo *)mymalloc(
        m_headerInfo.layersCount * sizeof(struct TPSDLayerInfo));
    int i = 0;
    for (i = 0; i < m_headerInfo.layersCount; i++) {
      readLayerInfo(i);
    }
  }
  return true;
}
bool TPSDReader::readLayerInfo(int i) {
  psdByte chlen, extralen, extrastart;
  int j, chid, namelen;
  TPSDLayerInfo *li = m_headerInfo.linfo + i;

  // process layer record
  li->top      = read4Bytes(m_file);
  li->left     = read4Bytes(m_file);
  li->bottom   = read4Bytes(m_file);
  li->right    = read4Bytes(m_file);
  li->channels = read2UBytes(m_file);

  if (li->bottom < li->top || li->right < li->left ||
      li->channels > 64)  // sanity ck
  {
    // qualcosa è andato storto, skippo il livello
    fseek(m_file, 6 * li->channels + 12, SEEK_CUR);
    skipBlock(m_file);  // skip  "layer info: extra data";
  } else {
    li->chan = (TPSDChannelInfo *)mymalloc(li->channels *
                                           sizeof(struct TPSDChannelInfo));
    li->chindex = (int *)mymalloc((li->channels + 2) * sizeof(int));
    li->chindex += 2;  //

    for (j = -2; j < li->channels; ++j) li->chindex[j] = -1;

    // fetch info on each of the layer's channels
    for (j = 0; j < li->channels; ++j) {
      chid = li->chan[j].id = read2Bytes(m_file);
      chlen = li->chan[j].length = read4Bytes(m_file);

      if (chid >= -2 && chid < li->channels)
        li->chindex[chid] = j;
      else {
        // WARNING: unexpected channel id
      }
    }

    fread(li->blend.sig, 1, 4, m_file);
    fread(li->blend.key, 1, 4, m_file);
    li->blend.opacity  = fgetc(m_file);
    li->blend.clipping = fgetc(m_file);
    li->blend.flags    = fgetc(m_file);
    fgetc(m_file);  // padding

    extralen   = read4Bytes(m_file);
    extrastart = ftell(m_file);

    // layer mask data
    if ((li->mask.size = read4Bytes(m_file))) {
      li->mask.top            = read4Bytes(m_file);
      li->mask.left           = read4Bytes(m_file);
      li->mask.bottom         = read4Bytes(m_file);
      li->mask.right          = read4Bytes(m_file);
      li->mask.default_colour = fgetc(m_file);
      li->mask.flags          = fgetc(m_file);
      fseek(m_file, li->mask.size - 18, SEEK_CUR);  // skip remainder
      li->mask.rows = li->mask.bottom - li->mask.top;
      li->mask.cols = li->mask.right - li->mask.left;
    } else {
      // no layer mask data
    }

    skipBlock(m_file);  // skip "layer blending ranges";

    // layer name
    li->nameno = (char *)malloc(16);
    snprintf(li->nameno, 16, "layer%d", i + 1);
    namelen  = fgetc(m_file);
    li->name = (char *)mymalloc(NEXT4(namelen + 1));
    fread(li->name, 1, NEXT4(namelen + 1) - 1, m_file);
    li->name[namelen] = 0;
    if (namelen) {
      if (li->name[0] == '.') li->name[0] = '_';
    }

    // process layer's 'additional info'

    li->additionalpos = ftell(m_file);
    li->additionallen = extrastart + extralen - li->additionalpos;
    doExtraData(li, li->additionallen);

    // leave file positioned at end of layer's data
    fseek(m_file, extrastart + extralen, SEEK_SET);
  }
  return true;
}

void TPSDReader::doImage(TRasterP &rasP, int layerId) {
  m_layerId         = layerId;
  int layerIndex    = getLayerInfoIndexById(layerId);
  TPSDLayerInfo *li = getLayerInfo(layerIndex);
  psdByte imageDataEnd;
  // retrieve start data pos
  psdByte startPos = ftell(m_file);
  if (m_headerInfo.layersCount > 0) {
    struct TPSDLayerInfo *lilast =
        &m_headerInfo.linfo[m_headerInfo.layersCount - 1];
    startPos = lilast->additionalpos + lilast->additionallen;
  }
  if (layerIndex > 0) {
    for (int j = 0; j < layerIndex; j++) {
      struct TPSDLayerInfo *liprev = &m_headerInfo.linfo[j];
      for (int ch = 0; ch < liprev->channels; ch++) {
        startPos += liprev->chan[ch].length;
      }
    }
  }
  fseek(m_file, startPos, SEEK_SET);

  long pixw    = li ? li->right - li->left : m_headerInfo.cols;
  long pixh    = li ? li->bottom - li->top : m_headerInfo.rows;
  int channels = li ? li->channels : m_headerInfo.channels;

  if (li == NULL)
    fseek(m_file, m_headerInfo.lmistart + m_headerInfo.lmilen, SEEK_SET);

  psdPixel rows = pixh;
  psdPixel cols = pixw;

  int ch = 0;
  psdByte **rowpos;

  rowpos = (psdByte **)mymalloc(channels * sizeof(psdByte *));

  for (ch = 0; ch < channels; ++ch) {
    psdPixel chrows =
        li && !m_headerInfo.linfoBlockEmpty && li->chan[ch].id == -2
            ? li->mask.rows
            : rows;
    rowpos[ch] = (psdByte *)mymalloc((chrows + 1) * sizeof(psdByte));
  }

  int tnzchannels = 0;

  int depth = m_headerInfo.depth;
  switch (m_headerInfo.mode) {
  // default: // multichannel, cmyk, lab etc
  //	split = 1;
  case ModeBitmap:
  case ModeGrayScale:
  case ModeGray16:
  case ModeDuotone:
  case ModeDuotone16:
    tnzchannels = 1;
    // check if there is an alpha channel, or if merged data has alpha
    if (li ? li->chindex[-1] != -1 : channels > 1 && m_headerInfo.mergedalpha) {
      tnzchannels = 2;
    }
    break;
  case ModeIndexedColor:
    tnzchannels = 1;
    break;
  case ModeRGBColor:
  case ModeRGB48:
    tnzchannels = 3;
    if (li ? li->chindex[-1] != -1 : channels > 3 && m_headerInfo.mergedalpha) {
      tnzchannels = 4;
    }
    break;
  default:
    tnzchannels = channels;
    // assert(0);
    break;
  }

  if (!li || m_headerInfo.linfoBlockEmpty) {  // merged channel
    TPSDChannelInfo *mergedChans =
        (TPSDChannelInfo *)mymalloc(channels * sizeof(struct TPSDChannelInfo));

    readChannel(m_file, NULL, mergedChans, channels, &m_headerInfo);
    imageDataEnd = ftell(m_file);
    readImageData(rasP, NULL, mergedChans, tnzchannels, rows, cols);
    free(mergedChans);
  } else {
    for (ch = 0; ch < channels; ++ch) {
      readChannel(m_file, li, li->chan + ch, 1, &m_headerInfo);
    }
    imageDataEnd = ftell(m_file);
    readImageData(rasP, li, li->chan, tnzchannels, rows, cols);
  }
  fseek(m_file, imageDataEnd, SEEK_SET);

  for (ch = 0; ch < channels; ++ch) free(rowpos[ch]);
  free(rowpos);
}

void TPSDReader::load(TRasterImageP &img, int layerId) {
  QMutexLocker sl(&m_mutex);
  TPSDLayerInfo *li = NULL;
  int layerIndex    = 0;
  if (layerId > 0) {
    layerIndex = getLayerInfoIndexById(layerId);
    li         = getLayerInfo(layerIndex);
  }
  if (layerId < 0) throw TImageException(m_path, "Layer ID not exists");

  if (m_headerInfo.mode == 4 || m_headerInfo.depth == 32) {
    img = TRasterImageP();
    return;
  }

  try {
    TRasterP rasP;
    openFile();
    doImage(rasP, layerId);
    fclose(m_file);
    /*
    // do savebox
    long sbx0 = li ? li->left : 0;
    long sby0 = li ? m_headerInfo.rows-li->bottom : 0;
    long sbx1 = li ? li->right - 1 : m_headerInfo.cols - 1;
    long sby1 = li ? m_headerInfo.rows - li->top - 1 : m_headerInfo.rows - 1;
    TRect layerSaveBox;
    layerSaveBox = TRect(sbx0,sby0,sbx1,sby1);
    TRect imageRect = TRect(0,0,m_headerInfo.cols-1,m_headerInfo.rows-1);
    // E' possibile che il layer sia in parte o tutto al di fuori della'immagine
    // in questo caso considero solo la parte visibile, cioè che rientra
    nell'immagine.
    // Se è tutta fuori restutuisco TRasterImageP()
    layerSaveBox *= imageRect;

    if(layerSaveBox== TRect()) {
            img = TRasterImageP();
            return;
    }			 */

    if (!rasP) {
      img = TRasterImageP();
      return;
    }  // Happens if layer image has 0 rows and (or?)
    // cols (dont ask me why, but I've seen it)
    TRect layerSaveBox = m_layersSavebox[layerId];
    TRect savebox(layerSaveBox);
    TDimension imgSize(rasP->getLx(), rasP->getLy());
    assert(TRect(imgSize).contains(savebox));

    if (TRasterGR8P ras = rasP) {
      TPixelGR8 bgColor;
      ras->fillOutside(savebox, bgColor);
      img = TRasterImageP(ras);
    } else if (TRaster32P ras = rasP) {
      TPixel32 bgColor(0, 0, 0, 0);
      if (savebox != TRect())
        ras->fillOutside(savebox, bgColor);
      else
        ras->fill(bgColor);
      img = TRasterImageP(ras);
    } else if ((TRaster64P)rasP) {
      TRaster32P raux(rasP->getLx(), rasP->getLy());
      TRop::convert(raux, rasP);
      TPixel32 bgColor(0, 0, 0, 0);
      raux->fillOutside(savebox, bgColor);
      img = TRasterImageP(raux);
    } else {
      throw TImageException(m_path, "Invalid Raster");
    }
    img->setDpi(m_headerInfo.hres, m_headerInfo.vres);
    img->setSavebox(savebox);
  } catch (...) {
  }
}

int TPSDReader::getLayerInfoIndexById(int layerId) {
  int layerIndex = -1;
  for (int i = 0; i < m_headerInfo.layersCount; i++) {
    TPSDLayerInfo *litemp = m_headerInfo.linfo + i;
    if (litemp->layerId == layerId) {
      layerIndex = i;
      break;
    }
  }
  if (layerIndex < 0 && layerId != 0)
    throw TImageException(m_path, "Layer ID not exists");
  return layerIndex;
}
TPSDLayerInfo *TPSDReader::getLayerInfo(int index) {
  if (index < 0 || index >= m_headerInfo.layersCount) return NULL;
  return m_headerInfo.linfo + index;
}
TPSDHeaderInfo TPSDReader::getPSDHeaderInfo() { return m_headerInfo; }

void TPSDReader::readImageData(TRasterP &rasP, TPSDLayerInfo *li,
                               TPSDChannelInfo *chan, int chancount,
                               psdPixel rows, psdPixel cols) {
  int channels = li ? li->channels : m_headerInfo.channels;

  short depth = m_headerInfo.depth;

  psdByte savepos = ftell(m_file);
  if (rows == 0 || cols == 0) return;
  psdPixel j;

  unsigned char *inrows[4], *rledata;

  int ch, map[4];

  rledata = (unsigned char *)mymalloc(chan->rowbytes * 2);

  for (ch = 0; ch < chancount; ++ch) {
    inrows[ch] = (unsigned char *)mymalloc(chan->rowbytes);
    map[ch]    = li && chancount > 1 ? li->chindex[ch] : ch;
  }

  // find the alpha channel, if needed
  if (li && (chancount == 2 || chancount == 4)) {  // grey+alpha
    if (li->chindex[-1] == -1) {
      // WARNING no alpha found?;
    } else
      map[chancount - 1] = li->chindex[-1];
  }

  // region dimensions with shrink
  // x0 e x1 non tengono conto dello shrink.
  int x0 = 0;
  int x1 = m_headerInfo.cols - 1;
  int y0 = 0;
  int y1 = m_headerInfo.rows - 1;

  if (!m_region.isEmpty()) {
    x0 = m_region.getP00().x;
    // se x0 è fuori dalle dimensioni dell'immagine ritorna un'immagine vuota
    if (x0 >= m_headerInfo.cols) {
      free(rledata);
      return;
    }
    x1 = x0 + m_region.getLx() - 1;
    // controllo che x1 rimanga all'interno dell'immagine
    if (x1 >= m_headerInfo.cols) x1 = m_headerInfo.cols - 1;
    y0                              = m_region.getP00().y;
    // se y0 è fuori dalle dimensioni dell'immagine ritorna un'immagine vuota
    if (y0 >= m_headerInfo.rows) {
      free(rledata);
      return;
    }
    y1 = y0 + m_region.getLy() - 1;
    // controllo che y1 rimanga all'interno dell'immagine
    if (y1 >= m_headerInfo.rows) y1 = m_headerInfo.rows - 1;
  }
  if (m_shrinkX > x1 - x0) m_shrinkX = x1 - x0;
  if (m_shrinkY > y1 - y0) m_shrinkY = y1 - y0;
  assert(m_shrinkX > 0 && m_shrinkY > 0);
  if (m_shrinkX > 1) {
    x1 -= (x1 - x0) % m_shrinkX;
  }
  if (m_shrinkY > 1) {
    y1 -= (y1 - y0) % m_shrinkY;
  }
  assert(x0 <= x1 && y0 <= y1);

  TDimension imgSize((x1 - x0) / m_shrinkX + 1, (y1 - y0) / m_shrinkY + 1);
  if (depth == 1 && chancount == 1) {
    rasP = TRasterGR8P(imgSize);
  } else if (depth == 8 && chancount > 1) {
    rasP = TRaster32P(imgSize);
  } else if (m_headerInfo.depth == 8 && chancount == 1) {
    rasP = TRasterGR8P(imgSize);
  } else if (m_headerInfo.depth == 16 && chancount == 1 &&
             m_headerInfo.mergedalpha) {
    rasP = TRasterGR8P(imgSize);
  } else if (m_headerInfo.depth == 16) {
    rasP = TRaster64P(imgSize);
  }

  // do savebox
  // calcolo la savebox in coordinate dell'immagine
  long sbx0 = li ? li->left - x0 : 0;
  long sby0 = li ? m_headerInfo.rows - li->bottom - y0 : 0;
  long sbx1 = li ? li->right - 1 - x0 : x1 - x0;
  long sby1 = li ? m_headerInfo.rows - li->top - 1 - y0 : y1 - y0;

  TRect layerSaveBox;
  layerSaveBox = TRect(sbx0, sby0, sbx1, sby1);

  TRect imageRect;
  if (!m_region.isEmpty())
    imageRect = TRect(0, 0, m_region.getLx() - 1, m_region.getLy() - 1);
  else
    imageRect = TRect(0, 0, m_headerInfo.cols - 1, m_headerInfo.rows - 1);
  // E' possibile che il layer sia in parte o tutto al di fuori della'immagine
  // in questo caso considero solo la parte visibile, cioè che rientra
  // nell'immagine.
  // Se è tutta fuori restutuisco TRasterImageP()
  layerSaveBox *= imageRect;

  if (layerSaveBox == TRect() || layerSaveBox.isEmpty()) {
    free(rledata);
    return;
  }
  // Estraggo da rasP solo il rettangolo che si interseca con il livello
  // corrente
  // stando attento a prendere i pixel giusti.
  int firstXPixIndexOfLayer = layerSaveBox.getP00().x - 1 + m_shrinkX -
                              (abs(layerSaveBox.getP00().x - 1) % m_shrinkX);
  int lrx0                  = firstXPixIndexOfLayer / m_shrinkX;
  int firstLineIndexOfLayer = layerSaveBox.getP00().y - 1 + m_shrinkY -
                              (abs(layerSaveBox.getP00().y - 1) % m_shrinkY);
  int lry0 = firstLineIndexOfLayer / m_shrinkY;
  int lrx1 =
      (layerSaveBox.getP11().x - abs(layerSaveBox.getP11().x % m_shrinkX)) /
      m_shrinkX;
  int lry1 =
      (layerSaveBox.getP11().y - abs(layerSaveBox.getP11().y % m_shrinkY)) /
      m_shrinkY;
  TRect layerSaveBox2 = TRect(lrx0, lry0, lrx1, lry1);
  if (layerSaveBox2.isEmpty()) return;
  assert(TRect(imgSize).contains(layerSaveBox2));
  if (li)
    m_layersSavebox[li->layerId] = layerSaveBox2;
  else
    m_layersSavebox[0] = layerSaveBox2;
  TRasterP smallRas    = rasP->extract(layerSaveBox2);
  assert(smallRas);
  if (!smallRas) return;
  // Trovo l'indice di colonna del primo pixel del livello che deve essere letto
  // L'indice è riferito al livello.
  int colOffset = firstXPixIndexOfLayer - layerSaveBox.getP00().x;
  assert(colOffset >= 0);
  // Trovo l'indice della prima riga del livello che deve essere letta
  // L'indice è riferito al livello.
  // Nota che nel file photoshop le righe sono memorizzate dall'ultima alla
  // prima.
  int rowOffset = std::abs(sby1) % m_shrinkY;
  int rowCount  = rowOffset;
  // if(m_shrinkY==3) rowCount--;
  for (j = 0; j < smallRas->getLy(); j++) {
    for (ch = 0; ch < chancount; ++ch) {
      /* get row data */
      if (map[ch] < 0 || map[ch] > chancount) {
        // warn("bad map[%d]=%d, skipping a channel", i, map[i]);
        memset(inrows[ch], 0, chan->rowbytes);  // zero out the row
      } else
        readrow(m_file, chan + map[ch], rowCount, inrows[ch], rledata);
    }
    // se la riga corrente non rientra nell'immagine salto la copia
    if (sby1 - rowCount < 0 || sby1 - rowCount > m_headerInfo.rows - 1) {
      rowCount += m_shrinkY;
      continue;
    }
    if (depth == 1 && chancount == 1) {
      if (!(layerSaveBox.getP00().x - sbx0 >= 0 &&
            layerSaveBox.getP00().x - sbx0 + smallRas->getLx() / 8 - 1 <
                chan->rowbytes))
        throw TImageException(
            m_path, "Unable to read image with this depth and channels values");
      smallRas->lock();
      unsigned char *rawdata =
          (unsigned char *)smallRas->getRawData(0, smallRas->getLy() - j - 1);
      TPixelGR8 *pix = (TPixelGR8 *)rawdata;
      int colCount   = colOffset;
      for (int k = 0; k < smallRas->getLx(); k += 8) {
        char value = ~inrows[0][layerSaveBox.getP00().x - sbx0 + colCount];
        pix[k].setValue(value);
        pix[k + 1].setValue(value);
        pix[k + 2].setValue(value);
        pix[k + 3].setValue(value);
        pix[k + 4].setValue(value);
        pix[k + 5].setValue(value);
        pix[k + 6].setValue(value);
        pix[k + 7].setValue(value);
        colCount += m_shrinkX;
      }
      smallRas->unlock();
    } else if (depth == 8 && chancount > 1) {
      if (!(layerSaveBox.getP00().x - sbx0 >= 0 &&
            layerSaveBox.getP00().x - sbx0 + smallRas->getLx() - 1 <
                chan->rowbytes))
        throw TImageException(
            m_path, "Unable to read image with this depth and channels values");
      smallRas->lock();
      unsigned char *rawdata =
          (unsigned char *)smallRas->getRawData(0, smallRas->getLy() - j - 1);
      TPixel32 *pix = (TPixel32 *)rawdata;
      int colCount  = colOffset;
      for (int k = 0; k < smallRas->getLx(); k++) {
        if (chancount >= 3) {
          pix[k].r = inrows[0][layerSaveBox.getP00().x - sbx0 + colCount];
          pix[k].g = inrows[1][layerSaveBox.getP00().x - sbx0 + colCount];
          pix[k].b = inrows[2][layerSaveBox.getP00().x - sbx0 + colCount];
          if (chancount == 4)  // RGB + alpha
            pix[k].m = inrows[3][layerSaveBox.getP00().x - sbx0 + colCount];
          else
            pix[k].m = 255;
        } else if (chancount <= 2)  // gray + alpha
        {
          pix[k].r = inrows[0][layerSaveBox.getP00().x - sbx0 + colCount];
          pix[k].g = inrows[0][layerSaveBox.getP00().x - sbx0 + colCount];
          pix[k].b = inrows[0][layerSaveBox.getP00().x - sbx0 + colCount];
          if (chancount == 2)
            pix[k].m = inrows[1][layerSaveBox.getP00().x - sbx0 + colCount];
          else
            pix[k].m = 255;
        }
        colCount += m_shrinkX;
      }

      smallRas->unlock();
    } else if (m_headerInfo.depth == 8 && chancount == 1) {
      if (!(layerSaveBox.getP00().x - sbx0 >= 0 &&
            layerSaveBox.getP00().x - sbx0 + smallRas->getLx() - 1 <
                chan->rowbytes))
        throw TImageException(
            m_path, "Unable to read image with this depth and channels values");
      smallRas->lock();
      unsigned char *rawdata =
          (unsigned char *)smallRas->getRawData(0, smallRas->getLy() - j - 1);

      TPixelGR8 *pix = (TPixelGR8 *)rawdata;
      int colCount   = colOffset;
      for (int k = 0; k < smallRas->getLx(); k++) {
        pix[k].setValue(inrows[0][layerSaveBox.getP00().x - sbx0 + colCount]);
        colCount += m_shrinkX;
      }
      smallRas->unlock();
    } else if (m_headerInfo.depth == 16 && chancount == 1 &&
               m_headerInfo.mergedalpha)  // mergedChannels
    {
      if (!(layerSaveBox.getP00().x - sbx0 >= 0 &&
            layerSaveBox.getP00().x - sbx0 + smallRas->getLx() - 1 <
                chan->rowbytes))
        throw TImageException(
            m_path, "Unable to read image with this depth and channels values");
      smallRas->lock();
      unsigned char *rawdata =
          (unsigned char *)smallRas->getRawData(0, smallRas->getLy() - j - 1);
      TPixelGR8 *pix = (TPixelGR8 *)rawdata;
      int colCount   = colOffset;
      for (int k = 0; k < smallRas->getLx(); k++) {
        pix[k].setValue(inrows[0][layerSaveBox.getP00().x - sbx0 + colCount]);
        colCount += m_shrinkX;
      }
      smallRas->unlock();
    } else if (m_headerInfo.depth == 16) {
      if (!(layerSaveBox.getP00().x - sbx0 >= 0 &&
            layerSaveBox.getP00().x - sbx0 + smallRas->getLx() - 1 <
                chan->rowbytes))
        throw TImageException(
            m_path, "Unable to read image with this depth and channels values");
      smallRas->lock();
      unsigned short *rawdata =
          (unsigned short *)smallRas->getRawData(0, smallRas->getLy() - j - 1);
      TPixel64 *pix = (TPixel64 *)rawdata;
      int colCount  = colOffset;
      for (int k = 0; k < smallRas->getLx(); k++) {
        if (chancount >= 3) {
          pix[k].r = swapShort(
              ((psdUint16 *)
                   inrows[0])[layerSaveBox.getP00().x - sbx0 + colCount]);
          pix[k].g = swapShort(
              ((psdUint16 *)
                   inrows[1])[layerSaveBox.getP00().x - sbx0 + colCount]);
          pix[k].b = swapShort(
              ((psdUint16 *)
                   inrows[2])[layerSaveBox.getP00().x - sbx0 + colCount]);
        } else if (chancount <= 2) {
          pix[k].r = swapShort(
              ((psdUint16 *)
                   inrows[0])[layerSaveBox.getP00().x - sbx0 + colCount]);
          pix[k].g = swapShort(
              ((psdUint16 *)
                   inrows[0])[layerSaveBox.getP00().x - sbx0 + colCount]);
          pix[k].b = swapShort(
              ((psdUint16 *)
                   inrows[0])[layerSaveBox.getP00().x - sbx0 + colCount]);
          if (chancount == 2)
            pix[k].m = swapShort(
                ((psdUint16 *)
                     inrows[1])[layerSaveBox.getP00().x - sbx0 + colCount]);
        }
        if (chancount == 4) {
          pix[k].m = swapShort(
              ((psdUint16 *)
                   inrows[3])[layerSaveBox.getP00().x - sbx0 + colCount]);
        } else
          pix[k].m = 0xffff;
        colCount += m_shrinkX;
      }
      smallRas->unlock();
    } else {
      throw TImageException(
          m_path, "Unable to read image with this depth and channels values");
    }
    rowCount += m_shrinkY;
  }
  fseek(m_file, savepos, SEEK_SET);  // restoring filepos

  free(rledata);
  for (ch = 0; ch < chancount; ++ch) free(inrows[ch]);
}

void TPSDReader::doExtraData(TPSDLayerInfo *li, psdByte length) {
  static struct dictentry extradict[] = {
      // v4.0
      {0, "levl", "LEVELS", "Levels", NULL /*adj_levels*/},
      {0, "curv", "CURVES", "Curves", NULL /*adj_curves*/},
      {0, "brit", "BRIGHTNESSCONTRAST", "Brightness/contrast", NULL},
      {0, "blnc", "COLORBALANCE", "Color balance", NULL},
      {0, "hue ", "HUESATURATION4", "Old Hue/saturation, Photoshop 4.0",
       NULL /*adj_huesat4*/},
      {0, "hue2", "HUESATURATION5", "New Hue/saturation, Photoshop 5.0",
       NULL /*adj_huesat5*/},
      {0, "selc", "SELECTIVECOLOR", "Selective color", NULL /*adj_selcol*/},
      {0, "thrs", "THRESHOLD", "Threshold", NULL},
      {0, "nvrt", "INVERT", "Invert", NULL},
      {0, "post", "POSTERIZE", "Posterize", NULL},
      // v5.0
      {0, "lrFX", "EFFECT", "Effects layer", NULL /*ed_layereffects*/},
      {0, "tySh", "TYPETOOL5", "Type tool (5.0)", NULL /*ed_typetool*/},
      {0, "luni", "-UNICODENAME", "Unicode layer name",
       NULL /*ed_unicodename*/},
      {0, "lyid", "-LAYERID", "Layer ID",
       readLongData},  // '-' prefix means keep tag value on one line
      // v6.0
      {0, "lfx2", "OBJECTEFFECT", "Object based effects layer",
       NULL /*ed_objecteffects*/},
      {0, "Patt", "PATTERN", "Pattern", NULL},
      {0, "Pat2", "PATTERNCS", "Pattern (CS)", NULL},
      {0, "Anno", "ANNOTATION", "Annotation", NULL /*ed_annotation*/},
      {0, "clbl", "-BLENDCLIPPING", "Blend clipping", readByteData},
      {0, "infx", "-BLENDINTERIOR", "Blend interior", readByteData},
      {0, "knko", "-KNOCKOUT", "Knockout", readByteData},
      {0, "lspf", "-PROTECTED", "Protected", readLongData},
      {0, "lclr", "SHEETCOLOR", "Sheet color", NULL},
      {0, "fxrp", "-REFERENCEPOINT", "Reference point",
       NULL /*ed_referencepoint*/},
      {0, "grdm", "GRADIENT", "Gradient", NULL},
      {0, "lsct", "-SECTION", "Section divider", readLongData},  // CS doc
      {0, "SoCo", "SOLIDCOLORSHEET", "Solid color sheet",
       NULL /*ed_versdesc*/},  // CS doc
      {0, "PtFl", "PATTERNFILL", "Pattern fill",
       NULL /*ed_versdesc*/},  // CS doc
      {0, "GdFl", "GRADIENTFILL", "Gradient fill",
       NULL /*ed_versdesc*/},                          // CS doc
      {0, "vmsk", "VECTORMASK", "Vector mask", NULL},  // CS doc
      {0, "TySh", "TYPETOOL6", "Type tool (6.0)",
       NULL /*ed_typetool*/},  // CS doc
      {0, "ffxi", "-FOREIGNEFFECTID", "Foreign effect ID",
       readLongData},  // CS doc (this is probably a key too, who knows)
      {0, "lnsr", "-LAYERNAMESOURCE", "Layer name source",
       readKey},  // CS doc (who knew this was a signature? docs fail again -
                  // and what do the values mean?)
      {0, "shpa", "PATTERNDATA", "Pattern data", NULL},  // CS doc
      {0, "shmd", "METADATASETTING", "Metadata setting",
       NULL /*ed_metadata*/},  // CS doc
      {0, "brst", "BLENDINGRESTRICTIONS", "Channel blending restrictions",
       NULL},  // CS doc
      // v7.0
      {0, "lyvr", "-LAYERVERSION", "Layer version", readLongData},  // CS doc
      {0, "tsly", "-TRANSPARENCYSHAPES", "Transparency shapes layer",
       readByteData},  // CS doc
      {0, "lmgm", "-LAYERMASKASGLOBALMASK", "Layer mask as global mask",
       readByteData},  // CS doc
      {0, "vmgm", "-VECTORMASKASGLOBALMASK", "Vector mask as global mask",
       readByteData},  // CS doc
      // CS
      {0, "mixr", "CHANNELMIXER", "Channel mixer", NULL},  // CS doc
      {0, "phfl", "PHOTOFILTER", "Photo Filter", NULL},    // CS doc

      {0, "Lr16", "LAYER16", "Layer 16", readLayer16},
      {0, NULL, NULL, NULL, NULL}};

  while (length >= 12) {
    psdByte block = sigkeyblock(m_file, extradict, li);
    if (!block) {
      // warn("bad signature in layer's extra data, skipping the rest");
      break;
    }
    length -= block;
  }
}

struct dictentry *TPSDReader::findbykey(FILE *f, struct dictentry *parent,
                                        char *key, TPSDLayerInfo *li) {
  struct dictentry *d;

  for (d = parent; d->key; ++d)
    if (!memcmp(key, d->key, 4)) {
      // char *tagname = d->tag + (d->tag[0] == '-');
      // fprintf(stderr, "matched tag %s\n", d->tag);
      if (d->func) {
        psdByte savepos = ftell(f);
        // int oneline = d->tag[0] == '-';
        // char *tagname = d->tag + oneline;
        if (memcmp(key, "Lr16", 4) == 0) {
          doLayersInfo();
        } else
          d->func(f, d, li);  // parse contents of this datum
        fseek(f, savepos, SEEK_SET);
      } else {
        // there is no function to parse this block.
        // because tag is empty in this case, we only need to consider
        // parent's one-line-ness.
      }
      return d;
    }
  return NULL;
}

int TPSDReader::sigkeyblock(FILE *f, struct dictentry *dict,
                            TPSDLayerInfo *li) {
  char sig[4], key[4];
  long len;
  struct dictentry *d;

  fread(sig, 1, 4, f);
  fread(key, 1, 4, f);
  len = read4Bytes(f);
  if (!memcmp(sig, "8BIM", 4)) {
    if (dict && (d = findbykey(f, dict, key, li)) && !d->func) {
      // there is no function to parse this block
      // UNQUIET("    (data: %s)\n", d->desc);
    }
    fseek(f, len, SEEK_CUR);
    return len + 12;  // return number of bytes consumed
  }
  return 0;  // bad signature
}

//---------------------------- Utility functions
std::string buildErrorString(int error) {
  std::string message = "";
  switch (error) {
  case 0:
    message = "NO Error Found";
    break;
  case 1:
    message = "Reading File Error";
    break;
  case 2:
    message = "Opening File Error";
    break;
  default:
    message = "Unknown Error";
  }
  return message;
}

void readChannel(FILE *f, TPSDLayerInfo *li,
                 TPSDChannelInfo *chan,  // channel info array
                 int channels, TPSDHeaderInfo *h) {
  int comp, ch;
  psdByte pos, chpos, rb;
  unsigned char *zipdata;
  psdPixel count, last, j;
  chpos = ftell(f);

  if (li) {
    // If this is a layer mask, the pixel size is a special case
    if (chan->id == -2) {
      chan->rows = li->mask.rows;
      chan->cols = li->mask.cols;
    } else {
      // channel has dimensions of the layer
      chan->rows = li->bottom - li->top;
      chan->cols = li->right - li->left;
    }
  } else {
    // merged image, has dimensions of PSD
    chan->rows = h->rows;
    chan->cols = h->cols;
  }

  // Compute image row bytes
  rb = ((long)chan->cols * h->depth + 7) / 8;

  // Read compression type
  comp = read2UBytes(f);

  // Prepare compressed data for later access:
  pos = chpos + 2;

  // skip rle counts, leave pos pointing to first compressed image row
  if (comp == RLECOMP) pos += (channels * chan->rows) << h->version;

  for (ch = 0; ch < channels; ++ch) {
    if (!li) chan[ch].id = ch;
    chan[ch].rowbytes    = rb;
    chan[ch].comptype    = comp;
    chan[ch].rows        = chan->rows;
    chan[ch].cols        = chan->cols;
    chan[ch].filepos     = pos;

    if (!chan->rows) continue;

    // For RLE, we read the row count array and compute file positions.
    // For ZIP, read and decompress whole channel.
    switch (comp) {
    case RAWDATA:
      pos += chan->rowbytes * chan->rows;
      break;

    case RLECOMP:
      /* accumulate RLE counts, to make array of row start positions */
      chan[ch].rowpos =
          (psdByte *)mymalloc((chan[ch].rows + 1) * sizeof(psdByte));
      last = chan[ch].rowbytes;
      for (j = 0; j < chan[ch].rows && !feof(f); ++j) {
        count = h->version == 1 ? read2UBytes(f) : (unsigned long)read4Bytes(f);

        if (count > 2 * chan[ch].rowbytes)  // this would be impossible
          count = last;                     // make a guess, to help recover
        last    = count;

        chan[ch].rowpos[j] = pos;
        pos += count;
      }
      if (j < chan[ch].rows) {
        // fatal error couldn't read RLE counts
      }
      chan[ch].rowpos[j] = pos; /* = end of last row */
      break;

    case ZIPWITHOUTPREDICTION:
    case ZIPWITHPREDICTION:
      if (li) {
        pos += chan->length - 2;

        zipdata = (unsigned char *)mymalloc(chan->length);
        count   = fread(zipdata, 1, chan->length - 2, f);
        // if(count < chan->length - 2)
        //	alwayswarn("ZIP data short: wanted %d bytes, got %d",
        // chan->length, count);

        chan->unzipdata =
            (unsigned char *)mymalloc(chan->rows * chan->rowbytes);
        if (comp == ZIPWITHOUTPREDICTION)
          psdUnzipWithoutPrediction(zipdata, count, chan->unzipdata,
                                    chan->rows * chan->rowbytes);
        else
          psdUnzipWithPrediction(zipdata, count, chan->unzipdata,
                                 chan->rows * chan->rowbytes, chan->cols,
                                 h->depth);

        free(zipdata);
      } else {
        // WARNING cannot process ZIP compression outside layer
      }
      break;
    default: {
      // BAD COMPRESSION TYPE
    }
      if (li) fseek(f, chan->length - 2, SEEK_CUR);
      break;
    }
  }

  // if(li && pos != chpos + chan->length)
  //	alwayswarn("# channel data is %ld bytes, but length = %ld\n",
  //			   pos - chpos, chan->length);

  // set at the end of channel's data
  fseek(f, pos, SEEK_SET);
}

void readLongData(FILE *f, struct dictentry *parent, TPSDLayerInfo *li) {
  unsigned long id = read4Bytes(f);
  if (strcmp(parent->key, "lyid") == 0)
    li->layerId = id;
  else if (strcmp(parent->key, "lspf") == 0)
    li->protect = id;
  else if (strcmp(parent->key, "lsct") == 0)
    li->section = id;
  else if (strcmp(parent->key, "ffxi") == 0)
    li->foreignEffectID = id;
  else if (strcmp(parent->key, "lyvr") == 0)
    li->layerVersion = id;
}

void readByteData(FILE *f, struct dictentry *parent, TPSDLayerInfo *li) {
  int id = fgetc(f);
  if (strcmp(parent->key, "clbl") == 0)
    li->blendClipping = id;
  else if (strcmp(parent->key, "infx") == 0)
    li->blendInterior = id;
  else if (strcmp(parent->key, "knko") == 0)
    li->knockout = id;
  else if (strcmp(parent->key, "tsly") == 0)
    li->transparencyShapes = id;
  else if (strcmp(parent->key, "lmgm") == 0)
    li->layerMaskAsGlobalMask = id;
  else if (strcmp(parent->key, "vmgm") == 0)
    li->vectorMaskAsGlobalMask = id;
}

void readKey(FILE *f, struct dictentry *parent, TPSDLayerInfo *li) {
  static char key[5];
  if (fread(key, 1, 4, f) == 4)
    key[4] = 0;
  else
    key[0] = 0;  // or return NULL?

  if (strcmp(parent->key, "lnsr") == 0) li->layerNameSource = key;
}

void readLayer16(FILE *f, struct dictentry *parent, TPSDLayerInfo *li) {
  // struct psd_header h2 = *psd_header; // a kind of 'nested' set of layers;
  // don't alter main PSD header

  // overwrite main PSD header, mainly because of the 'merged alpha' flag

  // I *think* they mean us to respect the one in Lr16 because in my test data,
  // the main layer count is zero, so cannot convey this information.
  // dolayerinfo(f, psd_header);
  // processlayers(f, psd_header);
}
//---------------------------- END Utility functions

// TPSD PARSER
TPSDParser::TPSDParser(const TFilePath &path) {
  m_path       = path;
  QString name = path.getName().c_str();
  name.append(path.getDottedType().c_str());
  int sepPos = name.indexOf("#");
  int dotPos = name.indexOf(".", sepPos);
  name.remove(sepPos, dotPos - sepPos);
  TFilePath psdpath = m_path.getParentDir() + TFilePath(name.toStdString());
  m_psdreader       = new TPSDReader(psdpath);
  doLevels();
}

TPSDParser::~TPSDParser() { delete m_psdreader; }

void TPSDParser::doLevels() {
  QString path     = m_path.getName().c_str();
  QStringList list = path.split("#");
  m_levels.clear();
  if (list.size() > 1) {
    TPSDHeaderInfo psdheader = m_psdreader->getPSDHeaderInfo();
    if (list.contains("frames") && list.at(0) != "frames") {
      int firstLayerId = 0;
      Level level;
      for (int i = 0; i < psdheader.layersCount; i++) {
        TPSDLayerInfo *li = m_psdreader->getLayerInfo(i);
        long width        = li->right - li->left;
        long height       = li->bottom - li->top;
        if (width > 0 && height > 0) {
          assert(li->layerId >= 0);
          if (i == 0) firstLayerId = li->layerId;
          level.addFrame(li->layerId);
        }
      }
      // non ha importanza quale layerId assegno, l'importante è che esista
      level.setLayerId(0);
      if (psdheader.layersCount == 0)
        level.addFrame(firstLayerId);  // succede nel caso in cui la psd non ha
                                       // blocco layerInfo
      m_levels.push_back(level);
    } else if (list.size() == 3) {
      if (list.at(2) == "group") {
        int folderTagOpen = 0;
        int scavenge      = 0;
        for (int i = 0; i < psdheader.layersCount; i++) {
          TPSDLayerInfo *li = m_psdreader->getLayerInfo(i);
          long width        = li->right - li->left;
          long height       = li->bottom - li->top;
          if (width > 0 && height > 0 && folderTagOpen == 0) {
            assert(li->layerId >= 0);
            Level level(li->name, li->layerId);
            level.addFrame(li->layerId);
            m_levels.push_back(level);
            scavenge = 0;
          } else {
            if (width != 0 && height != 0) {
              m_levels[m_levels.size() - 1 - (scavenge - folderTagOpen)]
                  .addFrame(li->layerId);
            } else {
              if (strcmp(li->name, "</Layer group>") == 0 ||
                  strcmp(li->name, "</Layer set>") == 0) {
                assert(li->layerId >= 0);
                Level level(li->name, li->layerId, true);
                m_levels.push_back(level);
                folderTagOpen++;
                scavenge = folderTagOpen;
              } else if (li->section > 0 &&
                         li->section <= 3)  // vedi specifiche psd
              {
                assert(li->layerId >= 0);
                m_levels[m_levels.size() - 1 - (scavenge - folderTagOpen)]
                    .setName(li->name);
                m_levels[m_levels.size() - 1 - (scavenge - folderTagOpen)]
                    .setLayerId(li->layerId);
                folderTagOpen--;
                if (folderTagOpen > 0)
                  m_levels[m_levels.size() - 1 - (scavenge - folderTagOpen)]
                      .addFrame(li->layerId, true);
              }
            }
          }
        }
        if (psdheader.layersCount ==
            0)  // succede nel caso in cui la psd non ha blocco layerInfo
        {
          Level level;
          level.setLayerId(0);
          level.addFrame(0);
          m_levels.push_back(level);
        }
      } else
        throw TImageException(m_path, "PSD code name error");
    } else if (list.size() == 2)  // each psd layer is a tlevel
    {
      TPSDHeaderInfo psdheader = m_psdreader->getPSDHeaderInfo();
      for (int i = 0; i < psdheader.layersCount; i++) {
        TPSDLayerInfo *li = m_psdreader->getLayerInfo(i);
        long width        = li->right - li->left;
        long height       = li->bottom - li->top;
        if (width > 0 && height > 0) {
          assert(li->layerId >= 0);
          Level level(li->name, li->layerId);
          level.addFrame(li->layerId);
          m_levels.push_back(level);
        }
      }
      if (psdheader.layersCount ==
          0)  // succede nel caso in cui la psd non ha blocco layerInfo
      {
        Level level;
        level.setLayerId(0);
        level.addFrame(0);
        m_levels.push_back(level);
      }
    } else
      throw TImageException(m_path, "PSD code name error");
  } else  // list.size()==1. float
  {
    Level level;
    level.setName(m_path.getName());
    level.addFrame(0);
    m_levels.push_back(level);
  }
}

void TPSDParser::load(TRasterImageP &rasP, int layerId) {
  m_psdreader->load(rasP, layerId);
}

int TPSDParser::getLevelIndexById(int layerId) {
  int layerIndex = -1;
  for (int i = 0; i < (int)m_levels.size(); i++) {
    if (m_levels[i].getLayerId() == layerId) {
      layerIndex = i;
      break;
    }
  }
  if (layerId == 0 && layerIndex < 0) layerIndex = 0;
  if (layerIndex < 0 && layerId != 0)
    throw TImageException(m_path, "Layer ID not exists");
  return layerIndex;
}
int TPSDParser::getLevelIdByName(std::string levelName) {
  int pos     = levelName.find_last_of(LEVEL_NAME_INDEX_SEP);
  int counter = 0;
  if (pos != std::string::npos) {
    counter   = atoi(levelName.substr(pos + 1).c_str());
    levelName = levelName.substr(0, pos);
  }
  int lyid           = -1;
  int levelNameCount = 0;
  for (int i = 0; i < (int)m_levels.size(); i++) {
    if (m_levels[i].getName() == levelName) {
      lyid = m_levels[i].getLayerId();
      if (counter == levelNameCount) break;
      levelNameCount++;
    }
  }
  if (lyid == 0 && lyid < 0) lyid = 0;
  if (lyid < 0 && lyid != 0)
    throw TImageException(m_path, "Layer ID not exists");
  return lyid;
}
int TPSDParser::getFramesCount(int levelId) {
  int levelIndex = getLevelIndexById(levelId);
  assert(levelIndex >= 0 && levelIndex < (int)m_levels.size());
  return m_levels[levelIndex].getFrameCount();
}
std::string TPSDParser::getLevelName(int levelId) {
  int levelIndex = getLevelIndexById(levelId);
  assert(levelIndex >= 0 && levelIndex < (int)m_levels.size());
  return m_levels[levelIndex].getName();
}
std::string TPSDParser::getLevelNameWithCounter(int levelId) {
  std::string levelName = getLevelName(levelId);
  int count             = 0;
  for (int i = 0; i < (int)m_levels.size(); i++) {
    if (m_levels[i].getName() == levelName) {
      if (m_levels[i].getLayerId() == levelId) {
        break;
      }
      count++;
    }
  }
  if (count > 0) {
    levelName.append(LEVEL_NAME_INDEX_SEP);
    std::string c = QString::number(count).toStdString();
    levelName.append(c);
  }
  return levelName;
}
