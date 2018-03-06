

#include "tiio_pli.h"
//#include "tstrokeoutline.h"
#include "tsystem.h"
#include "pli_io.h"
//#include "tstrokeutil.h"
#include "tregion.h"
#include "tsimplecolorstyles.h"
#include "tpalette.h"
//#include "tspecialstyleid.h"
#include "tiio.h"
#include "tconvert.h"
#include "tcontenthistory.h"
#include "tstroke.h"

typedef TVectorImage::IntersectionBranch IntersectionBranch;

//=============================================================================
const TSolidColorStyle ConstStyle(TPixel32::Black);

static TSolidColorStyle *CurrStyle = NULL;

namespace {

//---------------------------------------------------------------------------

class PliOuputStream final : public TOutputStreamInterface {
  std::vector<TStyleParam> *m_stream;

public:
  PliOuputStream(std::vector<TStyleParam> *stream) : m_stream(stream) {}
  TOutputStreamInterface &operator<<(double x) override {
    m_stream->push_back(TStyleParam(x));
    return *this;
  }
  TOutputStreamInterface &operator<<(int x) override {
    m_stream->push_back(TStyleParam(x));
    return *this;
  }
  TOutputStreamInterface &operator<<(std::string x) override {
    m_stream->push_back(TStyleParam(x));
    return *this;
  }
  TOutputStreamInterface &operator<<(USHORT x) override {
    m_stream->push_back(TStyleParam(x));
    return *this;
  }
  TOutputStreamInterface &operator<<(BYTE x) override {
    m_stream->push_back(TStyleParam(x));
    return *this;
  }
  TOutputStreamInterface &operator<<(const TRaster32P &x) override {
    m_stream->push_back(TStyleParam(x));
    return *this;
  }
};

//---------------------------------------------------------------------------

class PliInputStream final : public TInputStreamInterface {
  std::vector<TStyleParam> *m_stream;
  VersionNumber m_version;
  int m_count;

public:
  PliInputStream(std::vector<TStyleParam> *stream, int majorVersion,
                 int minorVersion)
      : m_stream(stream), m_version(majorVersion, minorVersion), m_count(0) {}

  TInputStreamInterface &operator>>(double &x) override {
    assert((*m_stream)[m_count].m_type == TStyleParam::SP_DOUBLE);
    x = (*m_stream)[m_count++].m_numericVal;
    return *this;
  }
  TInputStreamInterface &operator>>(int &x) override {
    assert((*m_stream)[m_count].m_type == TStyleParam::SP_INT);
    x = (int)(*m_stream)[m_count++].m_numericVal;
    return *this;
  }
  TInputStreamInterface &operator>>(std::string &x) override {
    if ((*m_stream)[m_count].m_type == TStyleParam::SP_INT)
      x = std::to_string(static_cast<int>((*m_stream)[m_count++].m_numericVal));
    else {
      assert((*m_stream)[m_count].m_type == TStyleParam::SP_STRING);
      x = (*m_stream)[m_count++].m_string;
    }
    return *this;
  }
  TInputStreamInterface &operator>>(BYTE &x) override {
    assert((*m_stream)[m_count].m_type == TStyleParam::SP_BYTE);
    x = (BYTE)(*m_stream)[m_count++].m_numericVal;
    return *this;
  }
  TInputStreamInterface &operator>>(USHORT &x) override {
    assert((*m_stream)[m_count].m_type == TStyleParam::SP_USHORT);
    x = (USHORT)(*m_stream)[m_count++].m_numericVal;
    return *this;
  }
  TInputStreamInterface &operator>>(TRaster32P &x) override {
    assert((*m_stream)[m_count].m_type == TStyleParam::SP_RASTER);
    x = (*m_stream)[m_count++].m_r;
    return *this;
  }

  VersionNumber versionNumber() const override { return m_version; }
};

//---------------------------------------------------------------------------

TPixel32 getColor(const TStroke *stroke) {
  // const TStrokeStyle* style = stroke->getStyle();
  // const TSolidColorStrokeStyle* style =   dynamic_cast<const
  // TSolidColorStrokeStyle*>(  );

  // if(style) return style->getAverageColor();

  return TPixel32::Transparent;
}

//---------------------------------------------------------------------------

/*
  Crea la palette dei colori, in funzione di quelli
  trovati dalla funzione findColor
  */
UINT findColor(const TPixel32 &color, const std::vector<TPixel> &colorArray) {
  for (UINT i = 0; i < colorArray.size(); i++)
    if (colorArray[i] == color) return i;
  return colorArray.size();
}

//---------------------------------------------------------------------------

void buildPalette(ParsedPli *pli, const TImageP img) {
  if (!pli->m_palette_tags.empty()) return;
  TVectorImageP tempVecImg = img;
  TPalette *vPalette       = tempVecImg->getPalette();
  unsigned int i;
  // if (pli->m_idWrittenColorsArray.empty())
  //  {
  //  assert(vPalette);
  //  pli->m_idWrittenColorsArray.resize(vPalette->getStyleCount());
  //   }

  // se c'e' una reference image, uso il primo stile della palette per
  // memorizzare il path
  TFilePath fp;
  if ((fp = vPalette->getRefImgPath()) != TFilePath()) {
    TStyleParam styleParam("refimage" + ::to_string(fp));
    StyleTag *refImageTag = new StyleTag(0, 0, 1, &styleParam);
    pli->m_palette_tags.push_back((PliObjectTag *)refImageTag);
  }

  // per scrivere le pages della palette, uso in modo improprio uno stile: il
  // primo stile
  // della paletta(o il secondo, se c'e' una refimage)  ha tutti parametri
  // stringa, che coincidono con i nomi delle pages
  // ilcampo m_id viene usato anche per mettere il frameIndex(per multi palette)
  assert(vPalette->getPageCount());

  std::vector<TStyleParam> pageNames(vPalette->getPageCount());
  for (i         = 0; i < pageNames.size(); i++)
    pageNames[i] = TStyleParam(::to_string(vPalette->getPage(i)->getName()));
  StyleTag *pageNamesTag =
      new StyleTag(0, 0, pageNames.size(), pageNames.data());

  pli->m_palette_tags.push_back((PliObjectTag *)pageNamesTag);

  /*
for(i=1 ; i<pli->m_idWrittenColorsArray.size(); i++ )
pli->m_idWrittenColorsArray[i]=false;
pli->m_idWrittenColorsArray[0]=true;
*/

  for (i = 1; i < (unsigned)vPalette->getStyleCount(); i++) {
    TColorStyle *style   = vPalette->getStyle(i);
    TPalette::Page *page = vPalette->getStylePage(i);
    if (!page) continue;
    int pageIndex = 65535;
    // if (page)
    pageIndex = page->getIndex();

    // TColorStyle*style = tempVecImg->getPalette()->getStyle(styleId);
    std::vector<TStyleParam> stream;
    PliOuputStream chan(&stream);
    style->save(chan);  // viene riempito lo stream;

    assert(pageIndex >= 0 && pageIndex <= 65535);
    StyleTag *styleTag =
        new StyleTag(i, pageIndex, stream.size(), stream.data());
    pli->m_palette_tags.push_back((PliObjectTag *)styleTag);
  }

  if (vPalette->isAnimated()) {
    std::set<int> keyFrames;
    for (i = 0; i < (unsigned)vPalette->getStyleCount(); i++)
      for (int j = 0; j < vPalette->getKeyframeCount(i); j++)
        keyFrames.insert(vPalette->getKeyframe(i, j));

    std::set<int>::const_iterator it = keyFrames.begin();
    for (; it != keyFrames.end(); ++it) {
      int frame = *it;
      vPalette->setFrame(frame);
      StyleTag *pageNamesTag = new StyleTag(
          frame, 0, 0, 0);  // lo so, e' orrendo. devo mettere un numero intero
      pli->m_palette_tags.push_back((PliObjectTag *)pageNamesTag);
      for (i = 1; i < (unsigned)vPalette->getStyleCount(); i++) {
        if (vPalette->isKeyframe(i, frame)) {
          TColorStyle *style   = vPalette->getStyle(i);
          TPalette::Page *page = vPalette->getStylePage(i);
          if (!page) continue;
          int pageIndex = 65535;
          // if (page)
          pageIndex = page->getIndex();

          // TColorStyle*style = tempVecImg->getPalette()->getStyle(styleId);
          std::vector<TStyleParam> stream;
          PliOuputStream chan(&stream);
          style->save(chan);  // viene riempito lo stream;

          assert(pageIndex >= 0 && pageIndex <= 65535);
          StyleTag *styleTag =
              new StyleTag(i, pageIndex, stream.size(), stream.data());
          pli->m_palette_tags.push_back((PliObjectTag *)styleTag);
        }
      }
    }
  }
}

//---------------------------------------------------------------------------

}  // unnamed namespace

//-----------------------------------------------------------------------------

//===========================================================================

/*
Classe locale per la scrittura di un frame del livello.
*/
class TImageWriterPli final : public TImageWriter {
public:
  TImageWriterPli(const TFilePath &, const TFrameId &frameId,
                  TLevelWriterPli *);
  ~TImageWriterPli() {}

private:
  UCHAR m_precision;
  // double m_maxThickness;
  // not implemented
  TImageWriterPli(const TImageWriterPli &);
  TImageWriterPli &operator=(const TImageWriterPli &src);

public:
  void save(const TImageP &) override;
  TFrameId m_frameId;

private:
  TLevelWriterPli *m_lwp;
};

//=============

TImageP TImageReaderPli::load() {
  if (!m_lrp->m_doesExist)
    throw TImageException(getFilePath(), "Error file doesn't exist");

  UINT majorVersionNumber, minorVersionNumber;
  m_lrp->m_pli->getVersion(majorVersionNumber, minorVersionNumber);
  assert(majorVersionNumber > 5 ||
         (majorVersionNumber == 5 && minorVersionNumber >= 5));

  return doLoad();
}

//===========================================================================

static void readRegionVersion4x(IntersectionDataTag *tag, TVectorImage *img) {
#ifndef NEW_REGION_FILL
  img->setFillData(tag->m_branchArray, tag->m_branchCount);
#endif
}

//-----------------------------------------------------------------------------

namespace {

struct CreateStrokeData {
  int m_styleId;
  TStroke::OutlineOptions m_options;

  CreateStrokeData() : m_styleId(-1) {}
};

void createStroke(ThickQuadraticChainTag *quadTag, TVectorImage *outVectImage,
                  const CreateStrokeData &data) {
  std::vector<TThickQuadratic *> chunks(quadTag->m_numCurves);

  for (UINT k = 0; k < quadTag->m_numCurves; k++)
    chunks[k] = &quadTag->m_curve[k];

  TStroke *stroke = TStroke::create(chunks);

  assert(data.m_styleId != -1);
  stroke->setStyle(data.m_styleId);
  stroke->outlineOptions() = data.m_options;

  if (quadTag->m_isLoop) stroke->setSelfLoop();
  // stroke->setSketchMode(groupTag->m_type==GroupTag::SKETCH_STROKE);
  outVectImage->addStroke(stroke, false);
}

}  // namespace

//-----------------------------------------------------------------------------

static void createGroup(GroupTag *groupTag, TVectorImage *vi,
                        CreateStrokeData &data) {
  int count = vi->getStrokeCount();
  for (int j = 0; j < groupTag->m_numObjects; j++) {
    if (groupTag->m_object[j]->m_type == PliTag::COLOR_NGOBJ)
      data.m_styleId = ((ColorTag *)groupTag->m_object[j])->m_color[0];
    else if (groupTag->m_object[j]->m_type == PliTag::OUTLINE_OPTIONS_GOBJ)
      data.m_options =
          ((StrokeOutlineOptionsTag *)groupTag->m_object[j])->m_options;
    else if (groupTag->m_object[j]->m_type == PliTag::GROUP_GOBJ)
      createGroup((GroupTag *)groupTag->m_object[j], vi, data);
    else {
      assert(groupTag->m_object[j]->m_type ==
             PliTag::THICK_QUADRATIC_CHAIN_GOBJ);
      createStroke((ThickQuadraticChainTag *)groupTag->m_object[j], vi, data);
    }
  }

  vi->group(count, vi->getStrokeCount() - count);
}

//-----------------------------------------------------------------------------

TImageP TImageReaderPli::doLoad() {
  CreateStrokeData strokeData;

  // preparo l'immagine da restituire
  TVectorImage *outVectImage = new TVectorImage(true);
  // fisso il colore di default a nero opaco
  // TPixel currentColor=TPixel::Black;
  // TStrokeStyle *currStyle = NULL;
  // chiudo tutto dentro un blocco try per  cautelarmi
  //  dalle eccezioni generate in lettura
  // try
  //{
  // un contatore
  UINT i;
  outVectImage->setAutocloseTolerance(m_lrp->m_pli->getAutocloseTolerance());

  ImageTag *imageTag;

  imageTag = m_lrp->m_pli->loadFrame(m_frameId);
  if (!imageTag)
    throw TImageException(m_path, "Corrupted or invalid image data");

  if (m_lrp->m_mapOfImage[m_frameId].second == false)
    m_lrp->m_mapOfImage[m_frameId].second = true;

  // per tutti gli oggetti presenti nel tag
  for (i = 0; i < imageTag->m_numObjects; i++) {
    switch (imageTag->m_object[i]->m_type) {
    case PliTag::GROUP_GOBJ:
      assert(((GroupTag *)imageTag->m_object[i])->m_type == GroupTag::STROKE);
      createGroup((GroupTag *)imageTag->m_object[i], outVectImage, strokeData);
      break;

    case PliTag::INTERSECTION_DATA_GOBJ:
      readRegionVersion4x((IntersectionDataTag *)imageTag->m_object[i],
                          outVectImage);
      break;

    case PliTag::THICK_QUADRATIC_CHAIN_GOBJ:
      // aggiunge le stroke quadratiche
      createStroke((ThickQuadraticChainTag *)imageTag->m_object[i],
                   outVectImage, strokeData);
      break;

    case PliTag::COLOR_NGOBJ: {
      // aggiunge curve quadratiche con spessore costante
      ColorTag *colorTag = (ColorTag *)imageTag->m_object[i];

      assert(colorTag->m_numColors == 1);
      strokeData.m_styleId = colorTag->m_color[0];
      // isSketch=(colorTag->m_color[0] < c_maxSketchColorNum);
      // isSketch=(colorTag->m_color[0] < c_maxSketchColorNum);

      break;
    }

    case PliTag::OUTLINE_OPTIONS_GOBJ:
      // adds outline options data
      strokeData.m_options =
          ((StrokeOutlineOptionsTag *)imageTag->m_object[i])->m_options;
      break;
    case PliTag::AUTOCLOSE_TOLERANCE_GOBJ:
      // aggiunge curve quadratiche con spessore costante
      AutoCloseToleranceTag *toleranceTag =
          (AutoCloseToleranceTag *)imageTag->m_object[i];
      assert(toleranceTag->m_autoCloseTolerance >= 0);
      outVectImage->setAutocloseTolerance(
          ((double)toleranceTag->m_autoCloseTolerance) / 1000);
      break;
    }  // switch(groupTag->m_object[j]->m_type)
  }    // for (i=0; i<imageTag->m_numObjects; i++)

//} // try

// catch(...) // cosi' e' inutile o raccolgo qualcosa prima di rilanciare o lo
// elimino
//{
//  throw;
// }

//  if (regionsComputed) //WARNING !!! la seedFill mette il flag a ValidRegion a
//  TRUE
//    outVectImage->seedFill(); //le vecchie immagini hanno il seed
//    (version<3.1)

#ifdef _DEBUG
  outVectImage->checkIntersections();
#endif

  outVectImage->findRegions();

  return TImageP(outVectImage);
}

//-----------------------------------------------------------------------------

TDimension TImageReaderPli::getSize() const { return TDimension(-1, -1); }

//-----------------------------------------------------------------------------

TRect TImageReaderPli::getBBox() const { return TRect(); }

//=============================================================================

TImageWriterPli::TImageWriterPli(const TFilePath &f, const TFrameId &frameId,
                                 TLevelWriterPli *pli)
    : TImageWriter(f)
    , m_frameId(frameId)
    , m_lwp(pli)
    , m_precision(2)
//, m_maxThickness(0)
{}

//-----------------------------------------------------------------------------

static void putStroke(TStroke *stroke, int &currStyleId,
                      std::vector<PliObjectTag *> &tags) {
  double maxThickness = 0;
  assert(stroke);

  int chunkCount = stroke->getChunkCount();
  std::vector<TThickQuadratic> strokeChain(chunkCount);

  int styleId = stroke->getStyle();
  assert(styleId >= 0);
  if (currStyleId == -1 || styleId != currStyleId) {
    currStyleId = styleId;
    std::unique_ptr<TUINT32[]> color(new TUINT32[1]);
    color[0] = (TUINT32)styleId;

    std::unique_ptr<ColorTag> colorTag(new ColorTag(
        ColorTag::SOLID, ColorTag::STROKE_COLOR, 1, std::move(color)));
    tags.push_back(colorTag.release());
  }

  // If the outline options are non-standard (not round), add the outline infos
  TStroke::OutlineOptions &options = stroke->outlineOptions();
  if (options.m_capStyle != TStroke::OutlineOptions::ROUND_CAP ||
      options.m_joinStyle != TStroke::OutlineOptions::ROUND_JOIN) {
    StrokeOutlineOptionsTag *outlineOptionsTag =
        new StrokeOutlineOptionsTag(options);
    tags.push_back((PliObjectTag *)outlineOptionsTag);
  }

  UINT k;
  for (k = 0; k < (UINT)chunkCount; ++k) {
    const TThickQuadratic *q = stroke->getChunk(k);
    maxThickness =
        std::max({maxThickness, q->getThickP0().thick, q->getThickP1().thick});
    strokeChain[k] = *q;
  }
  maxThickness = std::max(maxThickness,
                          stroke->getChunk(chunkCount - 1)->getThickP2().thick);

  ThickQuadraticChainTag *quadChainTag =
      new ThickQuadraticChainTag(k, &strokeChain[0], maxThickness);
  quadChainTag->m_isLoop = stroke->isSelfLoop();

  // pli->addTag((PliObjectTag *)quadChainTag);

  tags.push_back((PliObjectTag *)quadChainTag);
  // pli->addTag(groupTag[count++]);
}

//-----------------------------------------------------------------------------
GroupTag *makeGroup(TVectorImageP &vi, int &currStyleId, int &index,
                    int currDepth);

void TImageWriterPli::save(const TImageP &img) {
  // Allocate an image
  TVectorImageP tempVecImg = img;
  int currStyleId          = -1;
  if (!tempVecImg) throw TImageException(m_path, "No data to save");

  //  Check that the frame about to insert is not already present
  //   So you do not increase the number of current frames
  ++m_lwp->m_frameNumber;

  std::unique_ptr<IntersectionBranch[]> v;
  UINT intersectionSize = tempVecImg->getFillData(v);

  // alloco l'oggetto m_lwp->m_pli ( di tipo ParsedPli ) che si occupa di
  // costruire la struttura
  if (!m_lwp->m_pli) {
    m_lwp->m_pli.reset(new ParsedPli(m_lwp->m_frameNumber, m_precision, 40,
                                     tempVecImg->getAutocloseTolerance()));
    m_lwp->m_pli->setCreator(m_lwp->m_creator);
  }

  buildPalette(m_lwp->m_pli.get(), img);

  ParsedPli *pli = m_lwp->m_pli.get();

  /*
comunico che il numero di frame e' aumentato (il parsed lo riceve nel
solo nel costruttore)
*/
  pli->setFrameCount(m_lwp->m_frameNumber);

  // alloco la struttura che conterra' i tag per la vector image corrente
  std::vector<PliObjectTag *> tags;
  // tags = new   std::vector<PliObjectTag *>;

  // Store the precision scale to be used in saving the quadratics
  {
    int precisionScale    = sq(128);
    pli->precisionScale() = precisionScale;

    PliTag *tag = new PrecisionScaleTag(precisionScale);
    tags.push_back((PliObjectTag *)tag);
  }
  // Store the auto close tolerance
  double pliTolerance = m_lwp->m_pli->getAutocloseTolerance();
  // write the tag if the frame's tolerance has been changed or
  // if the first frame's tolerance (and therefore the level's tolerance)
  // has been changed.
  if (!areAlmostEqual(tempVecImg->getAutocloseTolerance(), 1.15, 0.001) ||
      !areAlmostEqual(pliTolerance, 1.15, 0.001)) {
    int tolerance =
        (int)((roundf(tempVecImg->getAutocloseTolerance() * 100) / 100) * 1000);
    PliTag *tag = new AutoCloseToleranceTag(tolerance);
    tags.push_back((PliObjectTag *)tag);
    pli->setVersion(120, 0);
  } else {
    pli->setVersion(71, 0);
  }
  // recupero il numero di stroke dall'immagine
  int numStrokes = tempVecImg->getStrokeCount();

  int i = 0;
  while (i < (UINT)numStrokes) {
    if (tempVecImg->isStrokeGrouped(i))
      tags.push_back(makeGroup(tempVecImg, currStyleId, i, 1));
    else
      putStroke(tempVecImg->getStroke(i++), currStyleId, tags);
  }

  if (intersectionSize > 0) {
    PliTag *tag = new IntersectionDataTag(intersectionSize, std::move(v));
    tags.push_back((PliObjectTag *)tag);
  }

  int tagsSize = tags.size();
  std::unique_ptr<ImageTag> imageTagPtr(new ImageTag(
      m_frameId, tagsSize, (tagsSize > 0) ? tags.data() : nullptr));

  pli->addTag(imageTagPtr.release());

  // il ritorno e' fissato a false in quanto la
  //  scrittura avviene alla distruzione dello scrittore di livelli
  return;  // davedere false;
}

//=============================================================================
TLevelWriterPli::TLevelWriterPli(const TFilePath &path, TPropertyGroup *winfo)
    : TLevelWriter(path, winfo), m_frameNumber(0) {}

//-----------------------------------------------------------------------------

TLevelWriterPli::~TLevelWriterPli() {
  if (!m_pli) {
    return;
  }
  try {
    // aggiungo il tag della palette
    CurrStyle = NULL;
    assert(!m_pli->m_palette_tags.empty());
    std::unique_ptr<GroupTag> groupTag(
        new GroupTag(GroupTag::PALETTE, m_pli->m_palette_tags.size(),
                     m_pli->m_palette_tags.data()));
    m_pli->addTag(groupTag.release(), true);
    if (m_contentHistory) {
      QString his = m_contentHistory->serialize();
      std::unique_ptr<TextTag> textTag(new TextTag(his.toStdString()));
      m_pli->addTag(textTag.release(), true);
    }
    m_pli->writePli(m_path);
  } catch (...) {
  }
}

//-----------------------------------------------------------------------------

TImageWriterP TLevelWriterPli::getFrameWriter(TFrameId fid) {
  TImageWriterPli *iwm = new TImageWriterPli(m_path, fid, this);
  return TImageWriterP(iwm);
}

//=============================================================================

TLevelReaderPli::TLevelReaderPli(const TFilePath &path)
    : TLevelReader(path)
    , m_palette(0)
    , m_paletteCount(0)
    , m_doesExist(false)
    , m_pli(0)
    , m_readPalette(true)
    , m_level()
    , m_init(false) {
  if (!(m_doesExist = TFileStatus(path).doesExist()))
    throw TImageException(getFilePath(), "Error file doesn't exist");
}

//-----------------------------------------------------------------------------

TLevelReaderPli::~TLevelReaderPli() { delete m_pli; }

//-----------------------------------------------------------------------------

TImageReaderP TLevelReaderPli::getFrameReader(TFrameId fid) {
  TImageReaderPli *irm = new TImageReaderPli(m_path, fid, this);
  return TImageReaderP(irm);
}

//-----------------------------------------------------------------------------

TPalette *readPalette(GroupTag *paletteTag, int majorVersion,
                      int minorVersion) {
  bool newPli = (majorVersion > 5 || (majorVersion == 5 && minorVersion >= 6));

  // wstring pageName(L"colors");
  TPalette *palette = new TPalette();
  // palette->addStyleToPage(0, pageName);
  // palette->setStyle(0,TPixel32(255,255,255,0));

  // palette->setVersion(isNew?0:-1);

  // tolgo dalla pagina lo stile #1
  palette->getPage(0)->removeStyle(1);
  int frame = -1;

  // int m = page->getStyleCount();
  bool pagesRead = false;

  // i primi due styletag della palette sono speciali;
  // il primo, che potrebbe non esserci, contiene l'eventuale reference image
  // path;
  // il secondo, che c'e' sempre contiene i nomi delle pagine.

  for (unsigned int i = 0; i < paletteTag->m_numObjects; i++) {
    StyleTag *styleTag = (StyleTag *)paletteTag->m_object[i];

    if (i == 0 && styleTag->m_numParams == 1 &&
        strncmp(styleTag->m_param[0].m_string.c_str(), "refimage", 8) ==
            0)  // questo stile contiene l'eventuale refimagepath
    {
      palette->setRefImgPath(
          TFilePath(styleTag->m_param[0].m_string.c_str() + 8));
      continue;
    }

    assert(styleTag->m_type == PliTag::STYLE_NGOBJ);
    int id        = styleTag->m_id;
    int pageIndex = styleTag->m_pageIndex;
    if (!pagesRead && newPli)  // quewsto stile contiene le stringhe dei nomi
                               // delle pagine della paletta!
    {
      pagesRead = true;
      assert(id == 0 && pageIndex == 0);
      assert(palette->getPageCount() == 1);
      for (int j = 0; j < styleTag->m_numParams; j++) {
        assert(styleTag->m_param[j].m_type == TStyleParam::SP_STRING);
        if (j == 0)
          palette->getPage(0)->setName(
              ::to_wstring(styleTag->m_param[j].m_string));
        else {
          palette->addPage(::to_wstring(styleTag->m_param[j].m_string));
          // palette->getPage(j)->addStyle(TPixel32::Red);
        }
      }
      continue;
    }
    if (styleTag->m_numParams ==
        0)  // styletag contiene il frame di una multipalette!
    {
      frame = styleTag->m_id;
      palette->setFrame(frame);
      continue;
    }
    TPalette::Page *page = 0;

    if (pageIndex < 65535)  // questo valore pseciale significa che il colore
                            // non sta in alcuna pagina
    {
      page = palette->getPage(pageIndex);
      assert(page);
    } else
      continue;  // i pli prima salvavano pure i colori non usati...evito di
                 // caricarli!

    std::vector<TStyleParam> params(styleTag->m_numParams);
    for (int j  = 0; j < styleTag->m_numParams; j++)
      params[j] = styleTag->m_param[j];

    PliInputStream chan(&params, majorVersion, minorVersion);
    TColorStyle *style = TColorStyle::load(chan);  // leggo params
    assert(id > 0);
    if (id < palette->getStyleCount()) {
      if (frame > -1) {
        TColorStyle *oldStyle = palette->getStyle(id);
        oldStyle->copy(*style);
        palette->setKeyframe(id, frame);
      } else
        palette->setStyle(id, style);
    } else {
      assert(frame ==
             -1);  // uno stile animato, ci deve gia' essere nella paletta!
      while (palette->getStyleCount() < id) palette->addStyle(TPixel32::Red);
      if (page)
        page->addStyle(palette->addStyle(style));
      else
        palette->addStyle(style);
    }
    if (id > 0 && page && frame == -1) page->addStyle(id);
    // m = page->getStyleCount();
  }
  palette->setFrame(0);
  return palette;
}

/*
Carico le informazioni relative al livello
*/

void TLevelReaderPli::doReadPalette(bool doReadIt) { m_readPalette = doReadIt; }

QString TLevelReaderPli::getCreator() {
  loadInfo();

  if (m_pli) return m_pli->getCreator();

  return "";
}

TLevelP TLevelReaderPli::loadInfo() {
  if (m_init) return m_level;

  m_init = true;

  // m_level = TLevelP();

  // TLevelP level;

  // chiudo tutto dentro un blocco try per  cautelarmi
  //  dalle eccezioni generate in lettura
  try {
    // alloco l'oggetto parsed
    assert(!m_pli);
    m_pli = new ParsedPli(getFilePath());
    UINT majorVersionNumber, minorVersionNumber;
    m_pli->getVersion(majorVersionNumber, minorVersionNumber);
    if (majorVersionNumber <= 5 &&
        (majorVersionNumber != 5 || minorVersionNumber < 5))
      return m_level;
    TPalette *palette = 0;
    m_pli->loadInfo(m_readPalette, palette, m_contentHistory);
    if (palette) m_level->setPalette(palette);

    for (int i = 0; i < m_pli->getFrameCount(); i++)
      m_level->setFrame(m_pli->getFrameNumber(i), TVectorImageP());
  } catch (std::exception &e) {
    TSystem::outputDebug(e.what());

    throw TImageException(getFilePath(), "Unknow error on reading file");
  } catch (...) {
    throw;
  }

  return m_level;
}

//-----------------------------------------------------------------------------

TImageReaderPli::TImageReaderPli(const TFilePath &f, const TFrameId &frameId,
                                 TLevelReaderPli *pli)
    : TImageReader(f), m_frameId(frameId), m_lrp(pli) {}

//=============================================================================

GroupTag *makeGroup(TVectorImageP &vi, int &currStyleId, int &index,
                    int currDepth) {
  std::vector<PliObjectTag *> tags;
  int i = index;
  while (i < (UINT)vi->getStrokeCount() &&
         vi->getCommonGroupDepth(i, index) >= currDepth) {
    int strokeDepth = vi->getGroupDepth(i);
    if (strokeDepth == currDepth)
      putStroke(vi->getStroke(i++), currStyleId, tags);
    else if (strokeDepth > currDepth)
      tags.push_back(makeGroup(vi, currStyleId, i, currDepth + 1));
    else
      assert(false);
  }
  index = i;
  return new GroupTag(GroupTag::STROKE, tags.size(), tags.data());
}

//=============================================================================
