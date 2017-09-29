#include "toonz/txshsimplelevel.h"
#include "imagebuilders.h"

// TnzLib includes
#include "toonz/txshleveltypes.h"
#include "toonz/imagemanager.h"
#include "toonz/studiopalette.h"
#include "toonz/hook.h"
#include "toonz/toonzscene.h"
#include "toonz/levelproperties.h"
#include "toonz/levelupdater.h"
#include "toonz/fullcolorpalette.h"
#include "toonz/preferences.h"
#include "toonz/stage.h"
#include "toonz/textureutils.h"
#include "toonz/levelset.h"

// TnzBase includes
#include "tenv.h"

// TnzCore includes
#include "trasterimage.h"
#include "tvectorimage.h"
#include "tmeshimage.h"
#include "timagecache.h"
#include "tofflinegl.h"
#include "tvectorgl.h"
#include "tvectorrenderdata.h"
#include "tropcm.h"
#include "tpixelutils.h"
#include "timageinfo.h"
#include "tlogger.h"
#include "tstream.h"
#include "tsystem.h"
#include "tcontenthistory.h"

// Qt includes
#include <QDir>
#include <QRegExp>
#include <QMessageBox>
#include <QtCore>

#include "../common/psdlib/psd.h"

namespace bc = boost::container;

//******************************************************************************************
//    Global stuff
//******************************************************************************************

DEFINE_CLASS_CODE(TXshSimpleLevel, 20)
PERSIST_IDENTIFIER(TXshSimpleLevel, "level")

//******************************************************************************************
//    Local namespace stuff
//******************************************************************************************

namespace {

int idBaseCode = 1;

//-----------------------------------------------------------------------------

struct CompatibilityStruct {
  int writeMask, neededMask, forbiddenMask;
};

CompatibilityStruct compatibility = {
    0x00F1,  // mask written.   Note: Student main must be 0x00F2
    //                 Note: the 0x00F0 part is currently not used.
    0x0000,  // mandatory mask: loaded levels MUST have a mask with these bits
             // set
    //                 Note: if mandatory mask != 0 then no old level (without
    //                 mask)
    //                       can be loaded
    //                 Note: this mask is currently not used.
    0x000E  // forbidden mask: loaded levels MUST NOT have a mask with these
            // bits set
            //
};

//-----------------------------------------------------------------------------

inline std::string rasterized(std::string id) { return id + "_rasterized"; }
inline std::string filled(std::string id) { return id + "_filled"; }

//-----------------------------------------------------------------------------

QString getCreatorString() {
  QString creator = QString::fromStdString(TEnv::getApplicationName()) + " " +
                    QString::fromStdString(TEnv::getApplicationVersion()) +
                    " CM(" + QString::number(compatibility.writeMask, 16) + ")";
  return creator;
}

//-----------------------------------------------------------------------------

bool checkCreatorString(const QString &creator) {
  int mask = 0;
  if (creator != "") {
    QRegExp rx("CM\\([0-9A-Fa-f]*\\)");
    int pos = rx.indexIn(creator);
    int len = rx.matchedLength();
    if (pos >= 0 && len >= 4) {
      QString v;
      if (len > 4) v = creator.mid(pos + 3, len - 4);
      bool ok        = true;
      mask           = v.toInt(&ok, 16);
    }
  }
  return (mask & compatibility.neededMask) == compatibility.neededMask &&
         (mask & compatibility.forbiddenMask) == 0;
}

//-----------------------------------------------------------------------------

bool isAreadOnlyLevel(const TFilePath &path) {
  if (path.isEmpty() || !path.isAbsolute()) return false;
  if (path.getDots() == "." ||
      (path.getDots() == ".." &&
       (path.getType() == "tlv" || path.getType() == "tpl"))) {
    if (!TSystem::doesExistFileOrLevel(path)) return false;
    TFileStatus fs(path);
    return !fs.isWritable();
  }
  /*- 処理が重くなるので、連番ファイルは全てfalseを返す -*/
  /*
else if(path.getDots() == "..")
{
TFilePath dir = path.getParentDir();
QDir qDir(QString::fromStdWString(dir.getWideString()));
QString levelName =
QRegExp::escape(QString::fromStdWString(path.getWideName()));
QString levelType = QString::fromStdString(path.getType());
QString exp(levelName+".[0-9]{1,4}."+levelType);
QRegExp regExp(exp);
QStringList list = qDir.entryList(QDir::Files);
QStringList livelFrames = list.filter(regExp);

bool isReadOnly=false;
int i;
for(i=0; i<livelFrames.size() && !isReadOnly; i++)
{
TFilePath frame = dir+TFilePath(livelFrames[i].toStdWString());
if(frame.isEmpty() || !frame.isAbsolute()) continue;
TFileStatus fs(frame);
isReadOnly = !fs.isWritable();
}
return isReadOnly;
}
*/
  else
    return false;
}

//-----------------------------------------------------------------------------

void getIndexesRangefromFids(TXshSimpleLevel *level,
                             const std::set<TFrameId> &fids, int &fromIndex,
                             int &toIndex) {
  if (fids.empty()) {
    fromIndex = toIndex = -1;
    return;
  }

  toIndex   = 0;
  fromIndex = level->getFrameCount() - 1;

  std::set<TFrameId>::const_iterator it;
  for (it = fids.begin(); it != fids.end(); ++it) {
    int index                        = level->guessIndex(*it);
    if (index > toIndex) toIndex     = index;
    if (index < fromIndex) fromIndex = index;
  }
}

}  // namespace

//******************************************************************************************
//    TXshSimpleLevel  impementation
//******************************************************************************************

bool TXshSimpleLevel::m_rasterizePli        = false;
bool TXshSimpleLevel::m_fillFullColorRaster = false;

//-----------------------------------------------------------------------------

TXshSimpleLevel::TXshSimpleLevel(const std::wstring &name)
    : TXshLevel(m_classCode, name)
    , m_properties(new LevelProperties)
    , m_palette(0)
    , m_idBase(std::to_string(idBaseCode++))
    , m_editableRangeUserInfo(L"")
    , m_isSubsequence(false)
    , m_16BitChannelLevel(false)
    , m_isReadOnly(false)
    , m_temporaryHookMerged(false) {}

//-----------------------------------------------------------------------------

TXshSimpleLevel::~TXshSimpleLevel() {
  clearFrames();

  if (m_palette) m_palette->release();
}

//-----------------------------------------------------------------------------

void TXshSimpleLevel::setEditableRange(unsigned int from, unsigned int to,
                                       const std::wstring &userName) {
  assert(from <= to && to < (unsigned int)getFrameCount());
  unsigned int i;
  for (i = from; i <= to; i++) m_editableRange.insert(index2fid(i));

  QString hostName        = TSystem::getHostName();
  m_editableRangeUserInfo = userName + L"_" + hostName.toStdWString();

  std::wstring fileName = getEditableFileName();
  TFilePath dstPath     = getScene()->decodeFilePath(m_path);
  dstPath = dstPath.withName(fileName).withType(dstPath.getType());

  // Load temporary level file (for pli and tlv types only)
  if (getType() != OVL_XSHLEVEL && TSystem::doesExistFileOrLevel(dstPath)) {
    TLevelReaderP lr(dstPath);
    TLevelP level = lr->loadInfo();
    setPalette(level->getPalette());
    for (TLevel::Iterator it = level->begin(); it != level->end(); it++) {
      TImageP img = lr->getFrameReader(it->first)->load();
      setFrame(it->first, img);
    }
  }

  // Merge temporary hook file with current hookset
  const TFilePath &hookFile = getHookPath(dstPath);
  mergeTemporaryHookFile(from, to, hookFile);
}

//-----------------------------------------------------------------------------

void TXshSimpleLevel::mergeTemporaryHookFile(unsigned int from, unsigned int to,
                                             const TFilePath &hookFile) {
  if (!TFileStatus(hookFile).doesExist()) return;

  HookSet *tempHookSet = new HookSet;
  TIStream is(hookFile);
  std::string tagName;
  try {
    if (is.matchTag(tagName) && tagName == "hooks") tempHookSet->loadData(is);
  } catch (...) {
  }

  HookSet *hookSet  = getHookSet();
  int tempHookCount = tempHookSet->getHookCount();

  if (tempHookCount == 0) {
    for (unsigned int f = from; f <= to; f++) {
      TFrameId fid = index2fid(f);
      hookSet->eraseFrame(fid);
    }
  } else {
    for (int i = 0; i < tempHookCount; i++) {
      Hook *hook    = tempHookSet->getHook(i);
      Hook *newHook = hookSet->touchHook(hook->getId());
      newHook->setTrackerObjectId(hook->getTrackerObjectId());
      newHook->setTrackerRegionHeight(hook->getTrackerRegionHeight());
      newHook->setTrackerRegionWidth(hook->getTrackerRegionWidth());
      for (unsigned int f = from; f <= to; f++) {
        TFrameId fid = index2fid(f);
        newHook->setAPos(fid, hook->getAPos(fid));
        newHook->setBPos(fid, hook->getBPos(fid));
      }
    }
  }

  m_temporaryHookMerged = true;
}

//-----------------------------------------------------------------------------

void TXshSimpleLevel::clearEditableRange() {
  m_editableRange.clear();
  m_editableRangeUserInfo = L"";
}

//-----------------------------------------------------------------------------

std::wstring TXshSimpleLevel::getEditableFileName() {
#ifdef MACOSX
  std::wstring fileName = L"." + m_path.getWideName();
#else
  std::wstring fileName = m_path.getWideName();
#endif
  fileName += L"_" + m_editableRangeUserInfo;
  int from, to;
  getIndexesRangefromFids(this, m_editableRange, from, to);
  if (from == -1 && to == -1) return L"";
  fileName += L"_" + std::to_wstring(from + 1) + L"-" + std::to_wstring(to + 1);
  return fileName;
}

//-----------------------------------------------------------------------------

std::set<TFrameId> TXshSimpleLevel::getEditableRange() {
  return m_editableRange;
}

//-----------------------------------------------------------------------------

void TXshSimpleLevel::setRenumberTable() {
  m_renumberTable.clear();

  FramesSet::iterator ft, fEnd = m_frames.end();
  for (ft = m_frames.begin(); ft != fEnd; ++ft) m_renumberTable[*ft] = *ft;
}

//-----------------------------------------------------------------------------

void TXshSimpleLevel::setDirtyFlag(bool on) { m_properties->setDirtyFlag(on); }

//-----------------------------------------------------------------------------

bool TXshSimpleLevel::getDirtyFlag() const {
  return m_properties->getDirtyFlag();
}

//-----------------------------------------------------------------------------

void TXshSimpleLevel::touchFrame(const TFrameId &fid) {
  m_properties->setDirtyFlag(true);
  TContentHistory *ch = getContentHistory();
  if (!ch) {
    ch = new TContentHistory(true);
    setContentHistory(ch);
  }
  ch->frameModifiedNow(fid);

  if (getType() == PLI_XSHLEVEL) {
    std::string id = rasterized(getImageId(fid));
    ImageManager::instance()->invalidate(id);
  }
  if (getType() & FULLCOLOR_TYPE) {
    std::string id = filled(getImageId(fid));
    ImageManager::instance()->invalidate(id);
  }
}

//-----------------------------------------------------------------------------

void TXshSimpleLevel::onPaletteChanged() {
  FramesSet::iterator ft, fEnd = m_frames.end();
  for (ft = m_frames.begin(); ft != fEnd; ++ft) {
    const TFrameId &fid = *ft;

    if (getType() == PLI_XSHLEVEL) {
      std::string id = rasterized(getImageId(fid));
      ImageManager::instance()->invalidate(id);
    }
    if (getType() & FULLCOLOR_TYPE) {
      std::string id = filled(getImageId(fid));
      ImageManager::instance()->invalidate(id);
    }

    texture_utils::invalidateTexture(this, fid);
  }
}

//-----------------------------------------------------------------------------

void TXshSimpleLevel::setScannedPath(const TFilePath &fp) {
  m_scannedPath = fp;
}

//-----------------------------------------------------------------------------

void TXshSimpleLevel::setPath(const TFilePath &fp, bool keepFrames) {
  m_path = fp;
  if (!keepFrames) {
    clearFrames();
    assert(getScene());
    try {
      load();
    } catch (...) {
    }
  }

  if (getType() != PLI_XSHLEVEL) {
    if (!m_frames.empty()) {
      std::string imageId = getImageId(getFirstFid());
      const TImageInfo *imageInfo =
          ImageManager::instance()->getInfo(imageId, ImageManager::none, 0);
      if (imageInfo) {
        TDimension imageRes(0, 0);
        TPointD imageDpi;
        imageRes.lx = imageInfo->m_lx;
        imageRes.ly = imageInfo->m_ly;
        imageDpi.x  = imageInfo->m_dpix;
        imageDpi.y  = imageInfo->m_dpiy;
        m_properties->setImageDpi(imageDpi);
        m_properties->setImageRes(imageRes);
        m_properties->setBpp(imageInfo->m_bitsPerSample *
                             imageInfo->m_samplePerPixel);
      }
    }
  }
}

//-----------------------------------------------------------------------------

void TXshSimpleLevel::clonePropertiesFrom(const TXshSimpleLevel *oldSl) {
  m_properties->setImageDpi(
      oldSl->m_properties
          ->getImageDpi());  // Watch out - may change dpi policy!
  m_properties->setDpi(oldSl->m_properties->getDpi());
  m_properties->setDpiPolicy(oldSl->m_properties->getDpiPolicy());
  m_properties->setImageRes(oldSl->m_properties->getImageRes());
  m_properties->setBpp(oldSl->m_properties->getBpp());
  m_properties->setSubsampling(oldSl->m_properties->getSubsampling());
}

//-----------------------------------------------------------------------------

TPalette *TXshSimpleLevel::getPalette() const { return m_palette; }

//-----------------------------------------------------------------------------

void TXshSimpleLevel::setPalette(TPalette *palette) {
  if (m_palette != palette) {
    if (m_palette) m_palette->release();

    m_palette = palette;
    if (m_palette) {
      m_palette->addRef();
      if (!(getType() & FULLCOLOR_TYPE)) m_palette->setPaletteName(getName());
    }
  }
}

//-----------------------------------------------------------------------------

void TXshSimpleLevel::getFids(std::vector<TFrameId> &fids) const {
  fids.assign(m_frames.begin(), m_frames.end());
}

//-----------------------------------------------------------------------------

std::vector<TFrameId> TXshSimpleLevel::getFids() const {
  return std::vector<TFrameId>(m_frames.begin(), m_frames.end());
}

//-----------------------------------------------------------------------------

bool TXshSimpleLevel::isFid(const TFrameId &fid) const {
  return m_frames.count(fid);
}

//-----------------------------------------------------------------------------

const TFrameId &TXshSimpleLevel::getFrameId(int index) const {
  return *(m_frames.begin() += index);
}

//-----------------------------------------------------------------------------

TFrameId TXshSimpleLevel::getFirstFid() const {
  return !isEmpty() ? *m_frames.begin() : TFrameId(TFrameId::NO_FRAME);
}

//-----------------------------------------------------------------------------

TFrameId TXshSimpleLevel::getLastFid() const {
  return !isEmpty() ? *m_frames.rbegin() : TFrameId(TFrameId::NO_FRAME);
}

//-----------------------------------------------------------------------------

int TXshSimpleLevel::guessStep() const {
  int frameCount = m_frames.size();
  if (frameCount < 2)
    return 1;  // un livello con zero o un frame ha per def. step=1

  FramesSet::const_iterator ft = m_frames.begin();

  TFrameId firstFid = *ft++, secondFid = *ft++;

  if (firstFid.getLetter() != 0 || secondFid.getLetter() != 0) return 1;

  int step = secondFid.getNumber() - firstFid.getNumber();
  if (step == 1) return 1;

  // controllo subito se lo step vale per l'ultimo frame
  // (cerco di limitare il numero di volte in cui devo controllare tutta la
  // lista)
  TFrameId lastFid = *m_frames.rbegin();
  if (lastFid.getLetter() != 0) return 1;

  if (lastFid.getNumber() != firstFid.getNumber() + step * (frameCount - 1))
    return 1;

  for (int i = 2; ft != m_frames.end(); ++ft, ++i) {
    const TFrameId &fid = *ft;

    if (fid.getLetter() != 0) return 1;

    if (fid.getNumber() != firstFid.getNumber() + step * i) return 1;
  }

  return step;
}

//-----------------------------------------------------------------------------

int TXshSimpleLevel::fid2index(const TFrameId &fid) const {
  FramesSet::const_iterator ft = m_frames.find(fid);
  return (ft != m_frames.end()) ? std::distance(m_frames.begin(), ft)
                                :  // Note: flat_set has random access
             -1;                   // iterators, so this is FAST
}

//-----------------------------------------------------------------------------

int TXshSimpleLevel::guessIndex(const TFrameId &fid) const {
  if (m_frames.empty()) return 0;  // no frames, return 0 (by definition)

  FramesSet::const_iterator ft = m_frames.lower_bound(fid);
  if (ft == m_frames.end()) {
    const TFrameId &maxFid = *m_frames.rbegin();
    assert(fid > maxFid);

    // fid not in the table, but greater than the last one.
    // return a suitable index. (e.g. frames are 1,3,5,7; fid2index(11) should
    // return index=5)
    int step = guessStep();
    int i    = (fid.getNumber() - maxFid.getNumber()) / step;
    return m_frames.size() - 1 + i;
  } else
    return std::distance(m_frames.begin(), ft);
}

//-----------------------------------------------------------------------------

TFrameId TXshSimpleLevel::index2fid(int index) const {
  if (index < 0) return TFrameId(-2);

  int frameCount = m_frames.size();
  if (frameCount == 0) return TFrameId(1);  // o_o?

  if (index < frameCount) {
    FramesSet::const_iterator ft = m_frames.begin();
    std::advance(ft, index);
    return *ft;
  } else {
    int step        = guessStep();
    TFrameId maxFid = *m_frames.rbegin();
    int d           = step * (index - frameCount + 1);
    return TFrameId(maxFid.getNumber() + d);
  }
}

//-----------------------------------------------------------------------------

TImageP TXshSimpleLevel::getFrame(const TFrameId &fid, UCHAR imFlags,
                                  int subsampling) const {
  assert(m_type != UNKNOWN_XSHLEVEL);

  // If the required frame is not in range, quit
  if (m_frames.count(fid) == 0) return TImageP();

  const std::string &imgId = getImageId(fid);

  ImageLoader::BuildExtData extData(this, fid, subsampling);
  TImageP img = ImageManager::instance()->getImage(imgId, imFlags, &extData);

  if (imFlags & ImageManager::toBeModified) {
    // The image will be modified. Perform any related invalidation.
    texture_utils::invalidateTexture(
        this, fid);  // We must rebuild associated textures
  }

  return img;
}

//-----------------------------------------------------------------------------

TImageInfo *TXshSimpleLevel::getFrameInfo(const TFrameId &fid,
                                          bool toBeModified) {
  assert(m_type != UNKNOWN_XSHLEVEL);

  // If the required frame is not in range, quit
  if (m_frames.count(fid) == 0) return 0;

  const std::string &imgId = getImageId(fid);

  TImageInfo *info = ImageManager::instance()->getInfo(
      imgId, toBeModified ? ImageManager::toBeModified : ImageManager::none, 0);

  return info;
}

//-----------------------------------------------------------------------------

TImageP TXshSimpleLevel::getFrameIcon(const TFrameId &fid) const {
  assert(m_type != UNKNOWN_XSHLEVEL);

  if (m_frames.count(fid) == 0) return TImageP();

  // NOTE: Icons caching is DISABLED at this stage. It is now responsibility of
  // ToonzQt's IconGenerator class.

  ImageLoader::BuildExtData extData(this, fid);
  extData.m_subs = 1, extData.m_icon = true;

  const std::string &imgId = getImageId(fid);
  TImageP img              = ImageManager::instance()->getImage(
      imgId, ImageManager::dontPutInCache, &extData);

  TToonzImageP timg = (TToonzImageP)img;
  if (timg && m_palette) timg->setPalette(m_palette);

  return img;
}

//-----------------------------------------------------------------------------
// load icon (and image) data of all frames into cache
void TXshSimpleLevel::loadAllIconsAndPutInCache(bool cacheImagesAsWell) {
  if (m_type != TZP_XSHLEVEL) return;

  std::vector<TFrameId> fids;
  getFids(fids);

  std::vector<std::string> iconIds;

  for (int i = 0; i < (int)fids.size(); i++) {
    iconIds.push_back(getIconId(fids[i]));
  }

  ImageManager::instance()->loadAllTlvIconsAndPutInCache(this, fids, iconIds,
                                                         cacheImagesAsWell);
}

//-----------------------------------------------------------------------------

TRasterImageP TXshSimpleLevel::getFrameToCleanup(const TFrameId &fid) const {
  assert(m_type != UNKNOWN_XSHLEVEL);

  FramesSet::const_iterator ft = m_frames.find(fid);
  if (ft == m_frames.end()) return TImageP();

  bool flag           = (m_scannedPath != TFilePath());
  std::string imageId = getImageId(fid, flag ? Scanned : 0);

  ImageLoader::BuildExtData extData(this, fid, 1);
  TRasterImageP img = ImageManager::instance()->getImage(
      imageId, ImageManager::dontPutInCache, &extData);
  if (!img) return img;

  double x_dpi, y_dpi;
  img->getDpi(x_dpi, y_dpi);
  if (!x_dpi && !y_dpi) {
    TPointD dpi = m_properties->getDpi();
    img->setDpi(dpi.x, dpi.y);
  }

  return img;
}

//-----------------------------------------------------------------------------

TImageP TXshSimpleLevel::getFullsampledFrame(const TFrameId &fid,
                                             UCHAR imFlags) const {
  assert(m_type != UNKNOWN_XSHLEVEL);

  FramesSet::const_iterator it = m_frames.find(fid);
  if (it == m_frames.end()) return TRasterImageP();

  std::string imageId = getImageId(fid);

  ImageLoader::BuildExtData extData(this, fid, 1);
  TImageP img = ImageManager::instance()->getImage(imageId, imFlags, &extData);

  if (imFlags & ImageManager::toBeModified) {
    // The image will be modified. Perform any related invalidation.
    texture_utils::invalidateTexture(
        this, fid);  // We must rebuild associated textures
  }

  return img;
}

//-----------------------------------------------------------------------------

std::string TXshSimpleLevel::getIconId(const TFrameId &fid,
                                       int frameStatus) const {
  return "icon:" + getImageId(fid, frameStatus);
}

//-----------------------------------------------------------------------------

std::string TXshSimpleLevel::getIconId(const TFrameId &fid,
                                       const TDimension &size) const {
  return getImageId(fid) + ":" + std::to_string(size.lx) + "x" +
         std::to_string(size.ly);
}

//-----------------------------------------------------------------------------

namespace {

TAffine getAffine(const TDimension &srcSize, const TDimension &dstSize) {
  double scx = 1 * dstSize.lx / (double)srcSize.lx;
  double scy = 1 * dstSize.ly / (double)srcSize.ly;
  double sc  = std::min(scx, scy);
  double dx  = (dstSize.lx - srcSize.lx * sc) * 0.5;
  double dy  = (dstSize.ly - srcSize.ly * sc) * 0.5;
  return TScale(sc) *
         TTranslation(0.5 * TPointD(srcSize.lx, srcSize.ly) + TPointD(dx, dy));
}

//-----------------------------------------------------------------------------

/*!Costruisce l'icona di dimesione \b size dell'immagine \b img.*/
TImageP buildIcon(const TImageP &img, const TDimension &size) {
  TRaster32P raster(size);
  if (TVectorImageP vi = img) {
    TOfflineGL *glContext = new TOfflineGL(size);
    // TDimension cameraSize(768, 576);
    TDimension cameraSize(1920, 1080);
    TPalette *vPalette = img->getPalette();
    assert(vPalette);
    const TVectorRenderData rd(getAffine(cameraSize, size), TRect(), vPalette,
                               0, false);
    glContext->clear(TPixel32::White);
    glContext->draw(vi, rd);
    raster->copy(glContext->getRaster());
    delete glContext;
  } else if (TToonzImageP ti = img) {
    raster->fill(TPixel32(255, 255, 255, 255));
    TRasterCM32P rasCM32 = ti->getRaster();
    TRect bbox;
    bbox = ti->getSavebox();
    if (!bbox.isEmpty()) {
      rasCM32   = rasCM32->extractT(bbox);
      double sx = raster->getLx() / (double)rasCM32->getLx();
      double sy = raster->getLy() / (double)rasCM32->getLy();
      double sc = std::min(sx, sy);
      TAffine aff =
          TScale(sc).place(rasCM32->getCenterD(), raster->getCenterD());
      TRop::resample(raster, rasCM32, ti->getPalette(), aff);
      raster->lock();
      for (int y = 0; y < raster->getLy(); y++) {
        TPixel32 *pix    = raster->pixels(y);
        TPixel32 *endPix = pix + raster->getLx();
        while (pix < endPix) {
          *pix = overPix(TPixel32::White, *pix);
          pix++;
        }
      }
      raster->unlock();
    }
  } else {
    TRasterImageP ri = img;
    if (ri) {
      ri->makeIcon(raster);
      TRop::addBackground(raster, TPixel32::White);
    } else
      raster->fill(TPixel32(127, 50, 20));
  }

  return TRasterImageP(raster);
}

}  // anonymous namespace

//-----------------------------------------------------------------------------

void TXshSimpleLevel::setFrame(const TFrameId &fid, const TImageP &img) {
  assert(m_type != UNKNOWN_XSHLEVEL);

  if (img) img->setPalette(getPalette());

  m_frames.insert(fid);

  TFilePath path = m_path;

  int frameStatus                        = getFrameStatus(fid);
  static const int SCANNED_OR_CLEANUPPED = (Scanned | Cleanupped);

  if ((frameStatus & SCANNED_OR_CLEANUPPED) == Scanned) path = m_scannedPath;

  // Deal with the ImageManger: ensure the identifiers are bound, and the
  // associated image is either modified to img or (if !img) invalidated.
  const std::string &imageId = getImageId(fid);

  if (!ImageManager::instance()->isBound(imageId)) {
    const TFilePath &decodedPath = getScene()->decodeFilePath(path);
    ImageManager::instance()->bind(imageId, new ImageLoader(decodedPath, fid));
  }

  ImageManager::instance()->setImage(imageId, img);  // Invalidates if !img

  if (frameStatus == Normal) {
    // Only a normal frame can have these. Justified since:
    //  a) PLIs have nothing to share with cleanup stuff
    //  b) The latter is used only in LineTest - which does not have Cleanup

    if (m_type == PLI_XSHLEVEL) {
      const std::string &imageId2 = rasterized(imageId);
      if (!ImageManager::instance()->isBound(imageId2))
        ImageManager::instance()->bind(imageId2, new ImageRasterizer);
      else
        ImageManager::instance()->invalidate(imageId2);
    }

    if (m_type == OVL_XSHLEVEL || m_type == TZI_XSHLEVEL) {
      const std::string &imageId2 = filled(imageId);
      if (!ImageManager::instance()->isBound(imageId2))
        ImageManager::instance()->bind(imageId2, new ImageFiller);
      else
        ImageManager::instance()->invalidate(imageId2);
    }
  }
}

//-----------------------------------------------------------------------------

void TXshSimpleLevel::eraseFrame(const TFrameId &fid) {
  FramesSet::iterator ft = m_frames.find(fid);
  if (ft == m_frames.end()) return;

  // Erase the corresponding entry in the renumber table
  std::map<TFrameId, TFrameId>::iterator rt, rEnd(m_renumberTable.end());
  for (rt = m_renumberTable.begin(); rt != rEnd; ++rt) {
    if (rt->second == fid) {
      m_renumberTable.erase(rt->first);
      break;
    }
  }

  m_frames.erase(ft);
  getHookSet()->eraseFrame(fid);

  ImageManager *im = ImageManager::instance();
  {
    im->unbind(getImageId(fid, Normal));
    im->unbind(getImageId(fid, Scanned));
    im->unbind(getImageId(fid, CleanupPreview));

    if (m_type == PLI_XSHLEVEL) im->unbind(rasterized(getImageId(fid)));

    if (m_type == OVL_XSHLEVEL || m_type == TZI_XSHLEVEL)
      im->unbind(filled(getImageId(fid)));

    texture_utils::invalidateTexture(this, fid);
  }
}

//-----------------------------------------------------------------------------

void TXshSimpleLevel::clearFrames() {
  ImageManager *im = ImageManager::instance();

  // Unbind frames
  FramesSet::iterator ft, fEnd = m_frames.end();
  for (ft = m_frames.begin(); ft != fEnd; ++ft) {
    im->unbind(getImageId(*ft, Scanned));
    im->unbind(getImageId(*ft, Cleanupped));
    im->unbind(getImageId(*ft, CleanupPreview));

    if (m_type == PLI_XSHLEVEL) im->unbind(rasterized(getImageId(*ft)));

    if (m_type == OVL_XSHLEVEL || m_type == TZI_XSHLEVEL)
      im->unbind(filled(getImageId(*ft)));

    texture_utils::invalidateTexture(this, *ft);
  }

  // Clear level
  m_frames.clear();
  m_editableRange.clear();
  m_editableRangeUserInfo.clear();
  m_renumberTable.clear();
  m_framesStatus.clear();
}

//-----------------------------------------------------------------------------

void TXshSimpleLevel::loadData(TIStream &is) {
  std::string tagName;
  bool flag = false;

  int type = UNKNOWN_XSHLEVEL;

  for (;;) {
    if (is.matchTag(tagName)) {
      if (tagName == "path") {
        is >> m_path;
        is.matchEndTag();
      } else if (tagName == "scannedPath") {
        is >> m_scannedPath;
        is.matchEndTag();
      } else if (tagName == "info") {
        std::string v;
        double xdpi = 0, ydpi = 0;
        int subsampling                      = 1;
        int doPremultiply                    = 0;
        int whiteTransp                      = 0;
        int antialiasSoftness                = 0;
        LevelProperties::DpiPolicy dpiPolicy = LevelProperties::DP_ImageDpi;
        if (is.getTagParam("dpix", v)) xdpi = std::stod(v);
        if (is.getTagParam("dpiy", v)) ydpi = std::stod(v);
        if (xdpi != 0 && ydpi != 0) dpiPolicy = LevelProperties::DP_CustomDpi;
        std::string dpiType                   = is.getTagAttribute("dpiType");
        if (dpiType == "image") dpiPolicy     = LevelProperties::DP_ImageDpi;
        if (is.getTagParam("type", v) && v == "s") type       = TZI_XSHLEVEL;
        if (is.getTagParam("subsampling", v)) subsampling     = std::stoi(v);
        if (is.getTagParam("premultiply", v)) doPremultiply   = std::stoi(v);
        if (is.getTagParam("antialias", v)) antialiasSoftness = std::stoi(v);
        if (is.getTagParam("whiteTransp", v)) whiteTransp     = std::stoi(v);

        m_properties->setDpiPolicy(dpiPolicy);
        m_properties->setDpi(TPointD(xdpi, ydpi));
        m_properties->setSubsampling(subsampling);
        m_properties->setDoPremultiply(doPremultiply);
        m_properties->setDoAntialias(antialiasSoftness);
        m_properties->setWhiteTransp(whiteTransp);
      } else
        throw TException("unexpected tag " + tagName);
    } else {
      if (flag) break;  // ci puo' essere un solo nome
      flag = true;
      std::wstring token;
      is >> token;
      if (token == L"__empty") {
        // empty = true;
        is >> token;
      }

      if (token == L"_raster")  // obsoleto (Tab2.2)
      {
        double xdpi = 1, ydpi = 1;
        is >> xdpi >> ydpi >> m_name;
        setName(m_name);
        type = OVL_XSHLEVEL;
        m_properties->setDpi(TPointD(xdpi, ydpi));
        setType(type);
        setPath(
            TFilePath("+drawings/") + (getName() + L"." + ::to_wstring("bmp")),
            true);
      } else if (token == L"__raster")  // obsoleto (Tab2.2)
      {
        double xdpi = 1, ydpi = 1;
        std::string extension;
        is >> xdpi >> ydpi >> m_name >> extension;
        setName(m_name);
        type = OVL_XSHLEVEL;
        m_properties->setDpi(TPointD(xdpi, ydpi));
        setType(type);
        setPath(TFilePath("+drawings/") +
                    (getName() + L"." + ::to_wstring(extension)),
                true);
      } else {
        m_name = token;
        setName(m_name);
      }
    }
  }
  if (type == UNKNOWN_XSHLEVEL) {
    std::string ext = m_path.getType();
    if (ext == "pli" || ext == "svg")
      type = PLI_XSHLEVEL;
    else if (ext == "tlv" || ext == "tzu" || ext == "tzp" || ext == "tzl")
      type = TZP_XSHLEVEL;
    else if (ext == "tzi")
      type = TZI_XSHLEVEL;
    else if (ext == "mesh")
      type = MESH_XSHLEVEL;
    else
      type = OVL_XSHLEVEL;
  }
  setType(type);
}

//-----------------------------------------------------------------------------

namespace {
class LoadingLevelRange {
public:
  TFrameId m_fromFid, m_toFid;
  LoadingLevelRange() : m_fromFid(1), m_toFid(0) {}

  bool match(const TFrameId &fid) const {
    /*-- ↓SubSequent範囲内にある条件		↓全部ロードする場合 --*/
    return m_fromFid <= fid && fid <= m_toFid || m_fromFid > m_toFid;
  }
  bool isEnabled() const { return m_fromFid <= m_toFid; }
  void reset() {
    m_fromFid = TFrameId(1);
    m_toFid   = TFrameId(0);
  }

} loadingLevelRange;

//-----------------------------------------------------------------------------
}  // namespace
//-----------------------------------------------------------------------------

void setLoadingLevelRange(const TFrameId &fromFid, const TFrameId &toFid) {
  loadingLevelRange.m_fromFid = fromFid;
  loadingLevelRange.m_toFid   = toFid;
}

void getLoadingLevelRange(TFrameId &fromFid, TFrameId &toFid) {
  fromFid = loadingLevelRange.m_fromFid;
  toFid   = loadingLevelRange.m_toFid;
}

static TFilePath getLevelPathAndSetNameWithPsdLevelName(
    TXshSimpleLevel *xshLevel) {
  TFilePath retfp = xshLevel->getPath();

  QString name        = QString::fromStdWString(retfp.getWideName());
  bool removeFileName = name.contains("##");
  if (removeFileName) {
    retfp = TFilePath(
        QString::fromStdWString(retfp.getWideString()).replace("##", "#"));
  }
  QStringList list = name.split("#", QString::SkipEmptyParts);

  if (list.size() >= 2 && list.at(1) != "frames") {
    bool hasLayerId;
    int layid                  = list.at(1).toInt(&hasLayerId);
    QTextCodec *layerNameCodec = QTextCodec::codecForName(
        Preferences::instance()->getLayerNameEncoding().c_str());

    if (hasLayerId) {
      // An explicit photoshop layer id must be converted to the associated
      // level name
      TPSDParser psdparser(xshLevel->getScene()->decodeFilePath(retfp));
      std::string levelName = psdparser.getLevelNameWithCounter(
          layid);  // o_o  what about UNICODE names??

      list[1]                 = layerNameCodec->toUnicode(levelName.c_str());
      std::wstring wLevelName = list.join("#").toStdWString();
      retfp                   = retfp.withName(wLevelName);

      if (removeFileName) wLevelName = list[1].toStdWString();

      TLevelSet *levelSet = xshLevel->getScene()->getLevelSet();
      if (levelSet &&
          levelSet->hasLevel(
              wLevelName))  // levelSet should be asserted instead
        levelSet->renameLevel(xshLevel, wLevelName);

      xshLevel->setName(wLevelName);
    }
  }

  return retfp;
}
//-----------------------------------------------------------------------------

// Nota: load() NON fa clearFrames(). si limita ad aggiungere le informazioni
// relative ai frames su disco
void TXshSimpleLevel::load() {
  getProperties()->setCreator("");
  QString creator;

  assert(getScene());
  if (!getScene()) return;

  m_isSubsequence = loadingLevelRange.isEnabled();

  TFilePath checkpath = getScene()->decodeFilePath(m_path);
  std::string type    = checkpath.getType();

  if (m_scannedPath != TFilePath()) {
    getProperties()->setDirtyFlag(
        false);  // Level is now supposedly loaded from disk

    static const int ScannedCleanuppedMask = Scanned | Cleanupped;
    TFilePath path = getScene()->decodeFilePath(m_scannedPath);
    if (TSystem::doesExistFileOrLevel(path)) {
      TLevelReaderP lr(path);
      assert(lr);
      TLevelP level = lr->loadInfo();
      if (!checkCreatorString(creator = lr->getCreator()))
        getProperties()->setIsForbidden(true);
      else
        for (TLevel::Iterator it = level->begin(); it != level->end(); it++) {
          TFrameId fid = it->first;
          if (!loadingLevelRange.match(fid)) continue;
          setFrameStatus(
              fid, (getFrameStatus(fid) & ~ScannedCleanuppedMask) | Scanned);
          setFrame(fid, TImageP());
        }
    }

    path = getScene()->decodeFilePath(m_path);
    if (TSystem::doesExistFileOrLevel(path)) {
      TLevelReaderP lr(path);
      assert(lr);
      TLevelP level = lr->loadInfo();
      if (getType() & FULLCOLOR_TYPE)
        setPalette(FullColorPalette::instance()->getPalette(getScene()));
      else
        setPalette(level->getPalette());
      if (!checkCreatorString(creator = lr->getCreator()))
        getProperties()->setIsForbidden(true);
      else
        for (TLevel::Iterator it = level->begin(); it != level->end(); it++) {
          TFrameId fid = it->first;
          if (!loadingLevelRange.match(fid)) continue;
          setFrameStatus(fid, getFrameStatus(fid) | Cleanupped);
          setFrame(fid, TImageP());
        }
      setContentHistory(
          lr->getContentHistory() ? lr->getContentHistory()->clone() : 0);
    }

  } else {
    // Not a scan + cleanup level

    if (m_path.getType() == "psd" &&
        this->getScene()->getVersionNumber().first < 71)
      m_path = getLevelPathAndSetNameWithPsdLevelName(this);

    TFilePath path = getScene()->decodeFilePath(m_path);

    getProperties()->setDirtyFlag(
        false);  // Level is now supposedly loaded from disk

    TLevelReaderP lr(path);  // May throw
    assert(lr);

    TLevelP level = lr->loadInfo();
    if (level->getFrameCount() > 0) {
      const TImageInfo *info = lr->getImageInfo(level->begin()->first);

      if (info && info->m_samplePerPixel >= 5) {
        QString msg = QString(
                          "Failed to open %1.\nSamples per pixel is more than "
                          "4. It may containt more than one alpha channel.")
                          .arg(QString::fromStdWString(m_path.getWideString()));
        QMessageBox::warning(0, "Image format not supported", msg);
        return;
      }

      if (info) set16BitChannelLevel(info->m_bitsPerSample == 16);
    }
    if ((getType() & FULLCOLOR_TYPE) && !is16BitChannelLevel())
      setPalette(FullColorPalette::instance()->getPalette(getScene()));
    else
      setPalette(level->getPalette());

    if (!checkCreatorString(creator = lr->getCreator()))
      getProperties()->setIsForbidden(true);
    else
      for (TLevel::Iterator it = level->begin(); it != level->end(); it++) {
        m_renumberTable[it->first] = it->first;  // Voglio che la tabella
                                                 // contenga anche i frame che
                                                 // non vengono caricati
        if (!loadingLevelRange.match(it->first)) continue;
        setFrame(it->first, TImageP());
      }

    setContentHistory(lr->getContentHistory() ? lr->getContentHistory()->clone()
                                              : 0);
  }
  getProperties()->setCreator(creator.toStdString());

  loadingLevelRange.reset();
  if (getType() != PLI_XSHLEVEL) {
    if (m_properties->getImageDpi() == TPointD() && !m_frames.empty()) {
      TDimension imageRes(0, 0);
      TPointD imageDpi;

      const TFrameId &firstFid = getFirstFid();
      std::string imageId      = getImageId(firstFid);

      const TImageInfo *imageInfo =
          ImageManager::instance()->getInfo(imageId, ImageManager::none, 0);
      if (imageInfo) {
        imageRes.lx = imageInfo->m_lx;
        imageRes.ly = imageInfo->m_ly;
        imageDpi.x  = imageInfo->m_dpix;
        imageDpi.y  = imageInfo->m_dpiy;
        m_properties->setImageDpi(imageDpi);
        m_properties->setImageRes(imageRes);
        m_properties->setBpp(imageInfo->m_bitsPerSample *
                             imageInfo->m_samplePerPixel);
      }
    }
    setRenumberTable();
  }

  if (getPalette() && StudioPalette::isEnabled())
    StudioPalette::instance()->updateLinkedColors(getPalette());

  TFilePath refImgName;
  if (m_palette) {
    refImgName           = m_palette->getRefImgPath();
    TFilePath refImgPath = refImgName;
    if (refImgName != TFilePath() && TFileStatus(refImgPath).doesExist()) {
      TLevelReaderP lr(refImgPath);
      if (lr) {
        TLevelP level = lr->loadInfo();
        if (level->getFrameCount() > 0) {
          TImageP img = lr->getFrameReader(level->begin()->first)->load();
          if (img && getPalette()) {
            img->setPalette(0);
            getPalette()->setRefImg(img);
            std::vector<TFrameId> fids;
            for (TLevel::Iterator it = level->begin(); it != level->end(); ++it)
              fids.push_back(it->first);
            getPalette()->setRefLevelFids(fids);
          }
        }
      }
    }
  }

  // Load hooks
  HookSet *hookSet = getHookSet();
  hookSet->clearHooks();

  const TFilePath &hookFile =
      TXshSimpleLevel::getExistingHookFile(getScene()->decodeFilePath(m_path));

  if (!hookFile.isEmpty()) {
    TIStream is(hookFile);
    std::string tagName;
    try {
      if (is.matchTag(tagName) && tagName == "hooks") hookSet->loadData(is);
    } catch (...) {
    }
  }
  updateReadOnly();
}

//-----------------------------------------------------------------------------

void TXshSimpleLevel::load(const std::vector<TFrameId> &fIds) {
  getProperties()->setCreator("");
  QString creator;
  assert(getScene());
  getProperties()->setDirtyFlag(false);

  m_isSubsequence = loadingLevelRange.isEnabled();

  // non e' un livello scan+cleanup
  TFilePath path = getScene()->decodeFilePath(m_path);

  TLevelReaderP lr(path);
  assert(lr);

  if (!checkCreatorString(creator = lr->getCreator()))
    getProperties()->setIsForbidden(true);
  else {
    if (fIds.size() != 0) {
      for (int i = 0; i < (int)fIds.size(); i++) {
        m_renumberTable[fIds[i]] = fIds[i];
        if (!loadingLevelRange.match(fIds[i])) continue;
        setFrame(fIds[i], TImageP());
      }
    } else {
      TLevelP level = lr->loadInfo();
      for (TLevel::Iterator it = level->begin(); it != level->end(); it++) {
        m_renumberTable[it->first] = it->first;
        if (!loadingLevelRange.match(it->first)) continue;
        setFrame(it->first, TImageP());
      }
    }
  }

  setContentHistory(lr->getContentHistory() ? lr->getContentHistory()->clone()
                                            : 0);

  getProperties()->setCreator(creator.toStdString());

  loadingLevelRange.reset();

  if (getType() != PLI_XSHLEVEL) {
    if (m_properties->getImageDpi() == TPointD() && !m_frames.empty()) {
      TDimension imageRes(0, 0);
      TPointD imageDpi;
      std::string imageId = getImageId(getFirstFid());
      const TImageInfo *imageInfo =
          ImageManager::instance()->getInfo(imageId, ImageManager::none, 0);
      if (imageInfo) {
        imageRes.lx = imageInfo->m_lx;
        imageRes.ly = imageInfo->m_ly;
        imageDpi.x  = imageInfo->m_dpix;
        imageDpi.y  = imageInfo->m_dpiy;
        m_properties->setImageDpi(imageDpi);
        m_properties->setImageRes(imageRes);
      }
    }
    setRenumberTable();
  }
}

//-----------------------------------------------------------------------------

void TXshSimpleLevel::updateReadOnly() {
  TFilePath path = getScene()->decodeFilePath(m_path);
  m_isReadOnly   = isAreadOnlyLevel(path);
}

//-----------------------------------------------------------------------------

void TXshSimpleLevel::saveData(TOStream &os) {
  os << m_name;

  std::map<std::string, std::string> attr;
  if (getProperties()->getDpiPolicy() == LevelProperties::DP_CustomDpi) {
    TPointD dpi = getProperties()->getDpi();
    if (dpi.x != 0 && dpi.y != 0) {
      attr["dpix"] = std::to_string(dpi.x);
      attr["dpiy"] = std::to_string(dpi.y);
    }
  } else {
    attr["dpiType"] = "image";
  }

  if (getProperties()->getSubsampling() != 1) {
    attr["subsampling"] = std::to_string(getProperties()->getSubsampling());
  }
  if (getProperties()->antialiasSoftness() > 0) {
    attr["antialias"] = std::to_string(getProperties()->antialiasSoftness());
  }
  if (getProperties()->doPremultiply()) {
    attr["premultiply"] = std::to_string(getProperties()->doPremultiply());
  } else if (getProperties()->whiteTransp()) {
    attr["whiteTransp"] = std::to_string(getProperties()->whiteTransp());
  }

  if (m_type == TZI_XSHLEVEL) attr["type"] = "s";

  os.openCloseChild("info", attr);

  os.child("path") << m_path;  // fp;
  if (m_scannedPath != TFilePath())
    os.child("scannedPath") << m_scannedPath;  // fp;
}

//-----------------------------------------------------------------------------

void TXshSimpleLevel::save() {
  assert(getScene());
  TFilePath path = getScene()->decodeFilePath(m_path);
  TSystem::outputDebug("save() : " + ::to_string(m_path) + " = " +
                       ::to_string(path) + "\n");

  if (getProperties()->getDirtyFlag() == false &&
      getPalette()->getDirtyFlag() == false &&
      TSystem::doesExistFileOrLevel(path))
    return;

  if (!TFileStatus(path.getParentDir()).doesExist()) {
    try {
      TSystem::mkDir(path.getParentDir());
    } catch (...) {
    }
  }
  save(path);
}

//-----------------------------------------------------------------------------

static void saveBackup(TFilePath path) {
  try {
    TFilePath backup = path.withName(path.getName() + "_backup");
    if (TSystem::doesExistFileOrLevel(backup))
      TSystem::removeFileOrLevel_throw(backup);
    TSystem::copyFileOrLevel_throw(backup, path);
  } catch (...) {
  }
}

//-----------------------------------------------------------------------------

void TXshSimpleLevel::save(const TFilePath &fp, const TFilePath &oldFp,
                           bool overwritePalette) {
  TFilePath dOldPath =
      (!oldFp.isEmpty()) ? oldFp : getScene()->decodeFilePath(m_path);

  TFilePath dDstPath = getScene()->decodeFilePath(fp);
  TSystem::touchParentDir(dDstPath);

  // backup
  if (Preferences::instance()->isLevelsBackupEnabled() &&
      dOldPath == dDstPath && TSystem::doesExistFileOrLevel(dDstPath))
    saveBackup(dDstPath);

  if (isAreadOnlyLevel(dDstPath)) {
    if (m_editableRange.empty() &&
        !m_temporaryHookMerged)  // file interaly locked
      throw TSystemException(
          dDstPath, "The level cannot be saved: it is a read only level.");
    else if (getType() != OVL_XSHLEVEL) {
      // file partially unlocked
      std::wstring fileName = getEditableFileName();
      assert(!fileName.empty());

      TFilePath app = dDstPath.withName(fileName).withType(dDstPath.getType());

      // removes old files
      if (TSystem::doesExistFileOrLevel(app)) TSystem::removeFileOrLevel(app);

      TFilePathSet oldFilePaths;
      getFiles(app, oldFilePaths);

      TFilePathSet::iterator it;
      for (it = oldFilePaths.begin(); it != oldFilePaths.end(); ++it) {
        if (TSystem::doesExistFileOrLevel(*it)) TSystem::removeFileOrLevel(*it);
      }

      // save new files
      TXshSimpleLevel *sl = new TXshSimpleLevel;
      sl->setScene(getScene());
      sl->setPalette(getPalette());
      sl->setPath(getScene()->codeFilePath(app));
      sl->setType(getType());

      std::set<TFrameId>::iterator eft, efEnd;
      for (eft = m_editableRange.begin(); eft != efEnd; ++eft) {
        const TFrameId &fid = *eft;
        sl->setFrame(fid, getFrame(fid, false));
      }

      // Copy hooks
      HookSet *hookSet = sl->getHookSet();
      *hookSet         = *getHookSet();

      FramesSet::iterator ft, fEnd = m_frames.end();
      for (ft = m_frames.begin(); ft != fEnd; ++ft) {
        const TFrameId &fid = *ft;

        if (m_editableRange.find(fid) == m_editableRange.end())
          hookSet->eraseFrame(fid);
      }

      // Copy mesh level
      sl->save(app);

#ifdef _WIN32

      // hides files
      oldFilePaths.clear();

      if (TSystem::doesExistFileOrLevel(app)) TSystem::hideFileOrLevel(app);

      getFiles(app, oldFilePaths);

      for (it = oldFilePaths.begin(); it != oldFilePaths.end(); ++it) {
        if (TSystem::doesExistFileOrLevel(*it)) TSystem::hideFileOrLevel(*it);
      }
#endif
      return;
    }
  }

  if (dOldPath != dDstPath && m_path != TFilePath()) {
    const TFilePath &dSrcPath = dOldPath;

    try {
      if (TSystem::doesExistFileOrLevel(dSrcPath)) {
        if (TSystem::doesExistFileOrLevel(dDstPath))
          TSystem::removeFileOrLevel(dDstPath);

        copyFiles(dDstPath, dSrcPath);
      }
    } catch (...) {
    }
  }
  // when saving the level palette with global name
  if (overwritePalette && getType() == TZP_XSHLEVEL && getPalette() &&
      getPalette()->getGlobalName() != L"") {
    overwritePalette      = false;
    TFilePath palettePath = dDstPath.withNoFrame().withType("tpl");
    StudioPalette::instance()->save(palettePath, getPalette());
  }

  saveSimpleLevel(dDstPath, overwritePalette);
}

//-----------------------------------------------------------------------------

void TXshSimpleLevel::saveSimpleLevel(const TFilePath &decodedFp,
                                      bool overwritePalette) {
  /* Precondition: Destination level with path decodedFp is supposed to already
             store those image frames not tagged as 'modified' in this level
             instance. */

  TFilePath oldPath  = m_path;
  TFilePath dOldPath = getScene()->decodeFilePath(oldPath);

  // Substitute m_path with decodedFp until the function quits.
  struct CopyOnExit {
    TFilePath &m_dstPath, &m_srcPath;
    ~CopyOnExit() { m_dstPath = m_srcPath; }
  } copyOnExit = {m_path = decodedFp,
                  oldPath};  // m_path substituted here until function quits

  bool savingOriginal = (decodedFp == dOldPath), paletteNotSaved = false;

  int imFlags = savingOriginal
                    ? ImageManager::dontPutInCache | ImageManager::toBeSaved
                    : ImageManager::dontPutInCache;

  std::vector<TFrameId> fids;
  getFids(fids);

  bool isLevelModified                = getProperties()->getDirtyFlag();
  bool isPaletteModified              = false;
  if (getPalette()) isPaletteModified = getPalette()->getDirtyFlag();

  if (isLevelModified || isPaletteModified) {
    // gmt (8/8/08. provo a risolvere il pasticcio della scrittura dei tlv.
    // Dobbiamo
    // ripensarci con piu' calma. Per ora cerco di fare meno danno possibile).
    TDimension oldRes(0, 0);

    if (TSystem::doesExistFileOrLevel(decodedFp)) {
      TLevelReaderP lr(decodedFp);
      const TImageInfo *imageInfo = m_frames.empty()
                                        ? lr->getImageInfo()
                                        : lr->getImageInfo(*(m_frames.begin()));

      if (imageInfo) {
        oldRes.lx = imageInfo->m_lx;
        oldRes.ly = imageInfo->m_ly;
        lr        = TLevelReaderP();
        if (getProperties()->getImageRes() != oldRes) {
          // Il comando canvas size cambia le dimensioni del livello!!!
          // Se il file già esiste, nel level writer vengono risettate le
          // dimesnioni del file esistente
          // e salva male
          TSystem::removeFileOrLevel(decodedFp);
        }
      }
    }
    // overwrite tlv
    if (decodedFp.getType() == "tlv" &&
        TSystem::doesExistFileOrLevel(decodedFp)) {
      if (isLevelModified) {
        // in questo caso dovrei scrivere solo i frame modificati.
        // certamente NON DEVO scrivere quelli che non ho (e che dovrei
        // rileggere dallo stesso file che sto scrivendo

        int oldSubs = getProperties()->getSubsampling();

        TLevelWriterP lw;
        try {
          lw = TLevelWriterP(decodedFp);
        } catch (...) {
          // revert subsampling
          m_properties->setSubsampling(oldSubs);
          m_path = oldPath;
          throw TSystemException(decodedFp,
                                 "Can't open file.\nAccess may be denied or \n"
                                 "someone else may be saving the same file.\n"
                                 "Please wait and try again.");
        }

        lw->setOverwritePaletteFlag(overwritePalette);

        lw->setCreator(getCreatorString());
        lw->setPalette(getPalette());

        // Filter out of the renumber table all non-tlv frames (could happen if
        // the level
        // is a scan-cleanup mix). This is fine even on the temporarily
        // substituted m_path.
        std::map<TFrameId, TFrameId> renumberTable;

        for (auto it = m_renumberTable.rbegin(); it != m_renumberTable.rend();
             ++it) {
          TFrameId id = (*it).first;
          if ((getFrameStatus(id) != Scanned) &&
              (getFrameStatus(id) != CleanupPreview)) {
            renumberTable[id] = (*it).second;
          }
        }

        m_renumberTable.clear();
        m_renumberTable = renumberTable;

        lw->setIconSize(Preferences::instance()->getIconSize());
        if (!isSubsequence()) lw->renumberFids(m_renumberTable);

        if (getContentHistory())
          lw->setContentHistory(getContentHistory()->clone());

        ImageLoader::BuildExtData extData(this, TFrameId());

        for (auto const &fid : fids) {
          std::string imageId = getImageId(
              fid, Normal);  // Retrieve the actual level frames ("L_whatever")
          if (!ImageManager::instance()->isModified(imageId)) continue;

          extData.m_fid = fid;
          TImageP img =
              ImageManager::instance()->getImage(imageId, imFlags, &extData);

          assert(img);
          if (!img) continue;

          int subs = 1;
          if (TToonzImageP ti = img)
            subs = ti->getSubsampling();
          else if (TRasterImageP ri = img)
            subs = ri->getSubsampling();

          assert(subs == 1);
          if (subs != 1) continue;

          if (TToonzImageP ti = img) {
            /*-
             * SaveBoxを塗り漏れ防止に使用している場合、実際の画像範囲とSaveBoxのサイズが異なるため、ここで更新しておく-*/
            TRect saveBox;
            TRop::computeBBox(ti->getRaster(), saveBox);
            ti->setSavebox(saveBox);
          }

          lw->getFrameWriter(fid)->save(img);
        }

        lw = TLevelWriterP();  // TLevelWriterP's destructor saves the palette
      } else if (isPaletteModified && overwritePalette) {
        TFilePath palettePath = decodedFp.withNoFrame().withType("tpl");
        if (Preferences::instance()->isLevelsBackupEnabled() &&
            TSystem::doesExistFileOrLevel(palettePath))
          saveBackup(palettePath);
        TOStream os(palettePath);
        if (os.checkStatus())
          os << getPalette();
        else
          paletteNotSaved = true;
      }
    } else {
      // per ora faccio quello che facevo prima, ma dobbiamo rivedere tutta la
      // strategia
      LevelUpdater updater(this);
      updater.getLevelWriter()->setCreator(getCreatorString());

      if (isLevelModified) {
        // Apply the level's renumber table, before saving other files.
        // NOTE: This is currently NOT under LevelUpdater's responsibility, as
        // renumber tables
        // are set/manipulated heavily here. The approach should be re-designed,
        // though...
        updater.getLevelWriter()->renumberFids(m_renumberTable);

        if (!m_editableRange.empty())
          fids = std::vector<TFrameId>(m_editableRange.begin(),
                                       m_editableRange.end());

        ImageLoader::BuildExtData extData(this, TFrameId());

        for (auto const &fid : fids) {
          std::string imageId = getImageId(
              fid, Normal);  // Retrieve the actual level frames ("L_whatever")
          if (!ImageManager::instance()->isModified(imageId)) continue;

          extData.m_fid = fid;
          TImageP img =
              ImageManager::instance()->getImage(imageId, imFlags, &extData);

          assert(img);
          if (!img) continue;

          int subs = 1;
          if (TToonzImageP ti = img)
            subs = ti->getSubsampling();
          else if (TRasterImageP ri = img)
            subs = ri->getSubsampling();

          assert(subs == 1);
          if (subs != 1) continue;

          updater.update(fid, img);
        }
      }
      updater.close();  // Needs the original level subs
      if ((getType() & FULLCOLOR_TYPE) && isPaletteModified)
        FullColorPalette::instance()->savePalette(getScene());
    }
  }

  // Save hooks

  TFilePath hookFile;
  HookSet *hookSet = 0;

  // Save the hookSet in a temporary hook file
  if (getType() == OVL_XSHLEVEL && !m_editableRange.empty()) {
    hookSet = new HookSet(*getHookSet());

    FramesSet::const_iterator it;
    for (it = m_frames.begin(); it != m_frames.end(); ++it) {
      TFrameId fid = *it;
      if (m_editableRange.find(fid) == m_editableRange.end())
        hookSet->eraseFrame(fid);
    }

    // file partially unlocked
    std::wstring fileName = getEditableFileName();
    assert(!fileName.empty());
    TFilePath app = decodedFp.withName(fileName).withType(decodedFp.getType());
    hookFile      = getHookPath(app);
  } else {
    hookFile = getHookPath(decodedFp);
    hookSet  = getHookSet();
  }

#ifdef _WIN32
  // Remove the hidden attribute (since TOStream's fopen fails on hidden files)
  if (getType() == OVL_XSHLEVEL && !m_editableRange.empty())
    SetFileAttributesW(hookFile.getWideString().c_str(), FILE_ATTRIBUTE_NORMAL);
#endif

  if (hookSet && hookSet->getHookCount() > 0) {
    TOStream os(hookFile);
    os.openChild("hooks");
    hookSet->saveData(os);
    os.closeChild();
  } else if (TFileStatus(hookFile).doesExist()) {
    try {
      TSystem::deleteFile(hookFile);
    } catch (...) {
    }
  }

#ifdef _WIN32
  if (getType() == OVL_XSHLEVEL && !m_editableRange.empty())
    TSystem::hideFileOrLevel(hookFile);
#endif

  if (savingOriginal) {
    setRenumberTable();  // Since the renumber table refers to the
                         // 'original' frames saved on disk
    if (m_properties) m_properties->setDirtyFlag(false);

    if (getPalette() && overwritePalette) getPalette()->setDirtyFlag(false);
  }

  if (paletteNotSaved)
    throw TSystemException(m_path,
                           "The palette of the level could not be saved.");
}

//-----------------------------------------------------------------------------

std::string TXshSimpleLevel::getImageId(const TFrameId &fid,
                                        int frameStatus) const {
  if (frameStatus < 0) frameStatus = getFrameStatus(fid);
  std::string prefix               = "L";
  if (frameStatus & CleanupPreview)
    prefix = "P";
  else if ((frameStatus & (Scanned | Cleanupped)) == Scanned)
    prefix            = "S";
  std::string imageId = m_idBase + "_" + prefix + fid.expand();
  return imageId;
}

//-----------------------------------------------------------------------------

int TXshSimpleLevel::getFrameStatus(const TFrameId &fid) const {
  std::map<TFrameId, int>::const_iterator it = m_framesStatus.find(fid);
  return (it != m_framesStatus.end()) ? it->second : Normal;
}

//-----------------------------------------------------------------------------

void TXshSimpleLevel::setFrameStatus(const TFrameId &fid, int status) {
  assert((status & ~(Scanned | Cleanupped | CleanupPreview)) == 0);
  m_framesStatus[fid] = status;
}

//-----------------------------------------------------------------------------
/*- CleanupPopup::setCurrentLevel / TCleanupper で使用 -*/
void TXshSimpleLevel::makeTlv(const TFilePath &tlvPath) {
  int ltype = getType();

  if (!(ltype & FULLCOLOR_TYPE)) {
    assert(ltype & FULLCOLOR_TYPE);
    return;
  }

  setType(TZP_XSHLEVEL);

  m_scannedPath = m_path;

  assert(tlvPath.getType() == "tlv");
  m_path = tlvPath;

  FramesSet::const_iterator it;
  for (it = m_frames.begin(); it != m_frames.end(); ++it) {
    TFrameId fid = *it;
    setFrameStatus(fid, Scanned);
    ImageManager::instance()->rebind(getImageId(fid, Scanned),
                                     getImageId(fid, 0));
    ImageManager::instance()->rebind(getIconId(fid, Scanned),
                                     getIconId(fid, 0));
  }
}

//-----------------------------------------------------------------------------

void TXshSimpleLevel::invalidateFrames() {
  FramesSet::iterator ft, fEnd = m_frames.end();
  for (ft = m_frames.begin(); ft != fEnd; ++ft)
    ImageManager::instance()->invalidate(getImageId(*ft));
}

//-----------------------------------------------------------------------------
/*- 指定したFIdのみInvalidateする -*/
void TXshSimpleLevel::invalidateFrame(const TFrameId &fid) {
  std::string id = getImageId(fid);
  ImageManager::instance()->invalidate(id);
}

//-----------------------------------------------------------------------------

// crea un frame con tipo, dimensioni, dpi, ecc. compatibili con il livello
TImageP TXshSimpleLevel::createEmptyFrame() {
  TImageP result;

  switch (m_type) {
  case PLI_XSHLEVEL:
    result = new TVectorImage;
    break;

  case MESH_XSHLEVEL:
    assert(false);  // Not implemented yet
    break;

  default: {
    // normally the image must have the level->getProperties()->getImageDpi().
    // if this value is missing (for some reason - can this happen, ever?) then
    // we use the getDpi() (that is the current dpi, e.g. cameraDpi or
    // customDpi).

    TPointD dpi = getProperties()->getImageDpi();
    /*--
    tgaからtlvにconvertしたものをInsert Pasteしたとき、
    ペーストしたフレームにのみDPIが付いてしまうので、この処理は省く
    --*/
    // if(dpi.x==0.0 || dpi.y==0.0)
    //  dpi = getProperties()->getDpi();

    TDimension res = getProperties()->getImageRes();

    if (m_type == TZP_XSHLEVEL) {
      TRasterCM32P raster(res);
      raster->fill(TPixelCM32());
      TToonzImageP ti(raster, TRect());
      ti->setDpi(dpi.x, dpi.y);
      ti->setSavebox(TRect(0, 0, res.lx - 1, res.ly - 1));

      result = ti;
    } else {
      TRaster32P raster(res);
      raster->fill(TPixel32(0, 0, 0, 0));
      TRasterImageP ri(raster);
      ri->setDpi(dpi.x, dpi.y);

      result = ri;
    }

    break;
  }
  }

  return result;
}

//-----------------------------------------------------------------------------

// ritorna la risoluzione dei frames del livello (se il livello non e'
// vettoriale)
TDimension TXshSimpleLevel::getResolution() {
  if (isEmpty() || getType() == PLI_XSHLEVEL) return TDimension();
  return m_properties->getImageRes();
}

//-----------------------------------------------------------------------------

// ritorna il dpi letto da file
TPointD TXshSimpleLevel::getImageDpi(const TFrameId &fid, int frameStatus) {
  if (isEmpty() || getType() == PLI_XSHLEVEL) return TPointD();

  const TFrameId &theFid =
      (fid == TFrameId::NO_FRAME || !isFid(fid)) ? getFirstFid() : fid;
  const std::string &imageId = getImageId(theFid, frameStatus);

  const TImageInfo *imageInfo =
      ImageManager::instance()->getInfo(imageId, ImageManager::none, 0);

  if (!imageInfo) return TPointD();

  return TPointD(imageInfo->m_dpix, imageInfo->m_dpiy);
}

//-----------------------------------------------------------------------------

int TXshSimpleLevel::getImageSubsampling(const TFrameId &fid) const {
  if (isEmpty() || getType() == PLI_XSHLEVEL) return 1;
  TImageP img = TImageCache::instance()->get(getImageId(fid), false);
  if (!img) return 1;
  if (TRasterImageP ri = img) return ri->getSubsampling();
  if (TToonzImageP ti = img) return ti->getSubsampling();
  return 1;
}

//-----------------------------------------------------------------------------

// ritorna il dpi corrente del livello
TPointD TXshSimpleLevel::getDpi(const TFrameId &fid, int frameStatus) {
  TPointD dpi;
  if (m_properties->getDpiPolicy() == LevelProperties::DP_ImageDpi)
    dpi = getImageDpi(fid, frameStatus);
  else
    dpi = m_properties->getDpi();
  return dpi;
}

//-----------------------------------------------------------------------------

void TXshSimpleLevel::renumber(const std::vector<TFrameId> &fids) {
  assert(fids.size() == m_frames.size());
  int n = fids.size();

  int i = 0;
  std::vector<TFrameId> oldFids;
  getFids(oldFids);
  std::map<TFrameId, TFrameId> table;
  std::map<TFrameId, TFrameId> newRenumberTable;
  for (std::vector<TFrameId>::iterator it = oldFids.begin();
       it != oldFids.end(); ++it) {
    TFrameId oldFrameId = *it;
    TFrameId newFrameId = fids[i++];
    table[oldFrameId]   = newFrameId;
    for (auto const &renumber : m_renumberTable) {
      if (renumber.second == oldFrameId) {
        newRenumberTable[renumber.first] = newFrameId;
        break;
      }
    }
  }
  for (auto const &renumber : newRenumberTable) {
    m_renumberTable[renumber.first] = renumber.second;
  }

  m_frames.clear();
  for (i = 0; i < n; ++i) {
    TFrameId fid(fids[i]);
    assert(m_frames.count(fid) == 0);
    m_frames.insert(fid);
  }

  ImageManager *im = ImageManager::instance();

  std::map<TFrameId, TFrameId>::iterator jt;

  {
    for (i = 0, jt = table.begin(); jt != table.end(); ++jt, ++i) {
      std::string Id = getImageId(jt->first);
      im->rebind(Id, "^" + std::to_string(i));
      TImageCache::instance()->remap("^icon:" + std::to_string(i),
                                     "icon:" + Id);
    }

    for (i = 0, jt = table.begin(); jt != table.end(); ++jt, ++i) {
      std::string Id = getImageId(jt->second);
      im->rebind("^" + std::to_string(i), Id);
      TImageCache::instance()->remap("icon:" + Id,
                                     "^icon:" + std::to_string(i));
      im->renumber(Id, jt->second);
    }
  }

  if (getType() == PLI_XSHLEVEL) {
    for (i = 0, jt = table.begin(); jt != table.end(); ++jt, ++i) {
      const std::string &id = rasterized(getImageId(jt->first));
      if (im->isBound(id)) im->rebind(id, rasterized("^" + std::to_string(i)));
    }
    for (i = 0, jt = table.begin(); jt != table.end(); ++jt, ++i) {
      const std::string &id = rasterized("^" + std::to_string(i));
      if (im->isBound(id)) im->rebind(id, rasterized(getImageId(jt->second)));
    }
  }

  if (getType() & FULLCOLOR_TYPE) {
    for (i = 0, jt = table.begin(); jt != table.end(); ++jt, ++i) {
      const std::string &id = filled(getImageId(jt->first));
      if (im->isBound(id)) im->rebind(id, filled("^" + std::to_string(i)));
    }
    for (i = 0, jt = table.begin(); jt != table.end(); ++jt, ++i) {
      const std::string &id = filled("^" + std::to_string(i));
      if (im->isBound(id)) im->rebind(id, filled(getImageId(jt->second)));
    }
  }

  m_properties->setDirtyFlag(true);

  if (getHookSet()) getHookSet()->renumber(table);
}

//-----------------------------------------------------------------------------

void TXshSimpleLevel::copyFiles(const TFilePath &dst, const TFilePath &src) {
  if (dst == src) return;
  TSystem::touchParentDir(dst);
  TSystem::copyFileOrLevel_throw(dst, src);
  if (dst.getType() == "tlv") {
    // Copio la palette del livello
    TFilePath srcPltPath =
        src.getParentDir() + TFilePath(src.getWideName() + L".tpl");
    if (TFileStatus(srcPltPath).doesExist())
      TSystem::copyFile(
          dst.getParentDir() + TFilePath(dst.getWideName() + L".tpl"),
          srcPltPath, true);
  }
  if (dst.getType() == "tzp" || dst.getType() == "tzu") {
    // Copio la palette del livello
    TFilePath srcPltPath =
        src.getParentDir() + TFilePath(src.getWideName() + L".plt");
    if (TFileStatus(srcPltPath).doesExist())
      TSystem::copyFile(
          dst.getParentDir() + TFilePath(dst.getWideName() + L".plt"),
          srcPltPath, true);
  }

  const TFilePath &srcHookFile = TXshSimpleLevel::getExistingHookFile(src);
  if (!srcHookFile.isEmpty()) {
    const TFilePath &dstHookFile = getHookPath(dst);
    TSystem::copyFile(dstHookFile, srcHookFile, true);
  }
  TFilePath files = src.getParentDir() + (src.getName() + "_files");
  if (TFileStatus(files).doesExist() && TFileStatus(files).isDirectory())
    TSystem::copyDir(dst.getParentDir() + (src.getName() + "_files"), files);
}

//-----------------------------------------------------------------------------

void TXshSimpleLevel::renameFiles(const TFilePath &dst, const TFilePath &src) {
  if (dst == src) return;
  TSystem::touchParentDir(dst);
  if (TSystem::doesExistFileOrLevel(dst)) TXshSimpleLevel::removeFiles(dst);
  TSystem::renameFileOrLevel_throw(dst, src);
  if (dst.getType() == "tlv")
    TSystem::renameFile(dst.withType("tpl"), src.withType("tpl"));

  const TFilePath &srcHookFile = TXshSimpleLevel::getExistingHookFile(src);
  if (!srcHookFile.isEmpty()) {
    const TFilePath &dstHookFile = getHookPath(dst);
    TSystem::renameFile(dstHookFile, srcHookFile);
  }

  TFilePath files = src.getParentDir() + (src.getName() + "_files");
  if (TFileStatus(files).doesExist() && TFileStatus(files).isDirectory())
    TSystem::renameFile(dst.getParentDir() + (dst.getName() + "_files"), files);
}

//-----------------------------------------------------------------------------

void TXshSimpleLevel::removeFiles(const TFilePath &fp) {
  TSystem::moveFileOrLevelToRecycleBin(fp);
  if (fp.getType() == "tlv") {
    TFilePath tpl = fp.withType("tpl");
    if (TFileStatus(tpl).doesExist()) TSystem::moveFileToRecycleBin(tpl);
  }

  // Delete ALL hook files (ie from every Toonz version)
  const QStringList &hookFiles = TXshSimpleLevel::getHookFiles(fp);

  int f, fCount = hookFiles.size();
  for (f = 0; f != fCount; ++f)
    TSystem::moveFileToRecycleBin(TFilePath(hookFiles[f].toStdWString()));

  TFilePath files = fp.getParentDir() + (fp.getName() + "_files");
  if (TFileStatus(files).doesExist() && TFileStatus(files).isDirectory())
    TSystem::rmDirTree(files);
}

//-----------------------------------------------------------------------------

void TXshSimpleLevel::getFiles(const TFilePath &fp, TFilePathSet &fpset) {
  if (fp.getType() == "tlv") {
    TFilePath tpl = fp.withType("tpl");
    if (TFileStatus(tpl).doesExist()) fpset.push_back(tpl);
  }

  // Store the hooks file if any (NOTE: there could be more than one hooks
  // file. I'm retaining the behavior I've seen, but was this really intended?
  // Shouldn't we return ALL the hooks files?)
  const TFilePath &hookFile = getExistingHookFile(fp);
  if (!hookFile.isEmpty()) fpset.push_back(hookFile);

  // Needed for TAB Manga & Kids and not used in Toonz

  // TFilePath files = fp.getParentDir() + (fp.getName() + "_files");
  // if(TFileStatus(files).doesExist() && TFileStatus(files).isDirectory())
  //  TSystem::rmDirTree(files);
}

//-----------------------------------------------------------------------------

void TXshSimpleLevel::setContentHistory(TContentHistory *contentHistory) {
  if (contentHistory != m_contentHistory.get())
    m_contentHistory.reset(contentHistory);
}

//-----------------------------------------------------------------------------

void TXshSimpleLevel::setCompatibilityMasks(int writeMask, int neededMask,
                                            int forbiddenMask) {
  compatibility.writeMask     = writeMask;
  compatibility.neededMask    = neededMask;
  compatibility.forbiddenMask = forbiddenMask;
}

//-----------------------------------------------------------------------------

TFilePath TXshSimpleLevel::getHookPath(const TFilePath &path) {
  // Translates:  levelName..ext  into  levelName_hooks..ext.xml
  //              levelName.ext   into  levelName_hooks.ext.xml

  // Observe that retaining the original level extension IS IMPORTANT, as it
  // ensures
  // the UNICITY of the association between a level path and its hook path (ie
  // levels  test..png  and  test..tif  have separate hook files).

  return TFilePath(path.withName(path.getName() + "_hooks").getWideString() +
                   L".xml");
}

//-----------------------------------------------------------------------------

QStringList TXshSimpleLevel::getHookFiles(const TFilePath &decodedLevelPath) {
  const TFilePath &dirPath = decodedLevelPath.getParentDir();
  QDir levelDir(QString::fromStdWString(dirPath.getWideString()));

  QStringList hookFileFilter(
      QString::fromStdWString(  // We have to scan for files of the
          decodedLevelPath.getWideName() +
          L"_hooks*.xml"));   // form  levelName_hooks*.xml  to
                              // retain backward compatibility
  return levelDir.entryList(  //
      hookFileFilter,
      QDir::Files | QDir::NoDotAndDotDot,  // Observe that we cleverly sort by
      QDir::Time);                         // mod date :)
}

//-----------------------------------------------------------------------------

TFilePath TXshSimpleLevel::getExistingHookFile(
    const TFilePath &decodedLevelPath) {
  static const int pCount              = 3;
  static const QRegExp pattern[pCount] = {
      // Prioritized in this order
      QRegExp(".*\\.\\.?.+\\.xml$"),  // whatever.(.)ext.xml
      QRegExp(".*\\.xml$"),           // whatever.xml
      QRegExp(".*\\.\\.?xml$")        // whatever.(.)xml
  };

  struct locals {
    static inline int getPattern(const QString &fp) {
      for (int p = 0; p != pCount; ++p)
        if (pattern[p].exactMatch(fp)) return p;
      return -1;
    }
  };  // locals

  const QStringList &hookFiles = getHookFiles(decodedLevelPath);
  if (hookFiles.empty()) return TFilePath();

  // Return the hook file with the most recent (smallest) identified
  // regexp pattern
  int fPattern, p = pCount, h = -1;

  int f, fCount = hookFiles.size();
  for (f = 0; f != fCount; ++f) {
    fPattern            = locals::getPattern(hookFiles[f]);
    if (fPattern < p) p = fPattern, h = f;
  }

  assert(h >= 0);
  return (h < 0) ? TFilePath() : decodedLevelPath.getParentDir() +
                                     TFilePath(hookFiles[h].toStdWString());
}

//-----------------------------------------------------------------------------

TRectD TXshSimpleLevel::getBBox(const TFrameId &fid) const {
  TRectD bbox;
  double dpiX = Stage::inch, dpiY = dpiX;

  // Get the frame bbox in image coordinates
  switch (getType()) {
  case PLI_XSHLEVEL:
  case MESH_XSHLEVEL: {
    // Load the image and extract its bbox forcibly
    TImageP img = getFrame(fid, false);
    if (!img) return TRectD();

    bbox = img->getBBox();

    if (TMeshImageP mi = img) mi->getDpi(dpiX, dpiY);

    break;
  }

  default: {
    // Raster case: retrieve the image info from the ImageManager
    const std::string &imageId = getImageId(fid);

    const TImageInfo *info =
        ImageManager::instance()->getInfo(imageId, ImageManager::none, 0);
    if (!info) return TRectD();

    bbox = TRectD(TPointD(info->m_x0, info->m_y0),
                  TDimensionD(info->m_lx,
                              info->m_ly)) -  // Using lx, ly is less ambiguous
           0.5 * TPointD(info->m_lx, info->m_ly);

    if (info->m_dpix > 0.0 && info->m_dpiy > 0.0)
      dpiX = info->m_dpix, dpiY = info->m_dpiy;

    break;
  }
  }

  // Get the frame's dpi and traduce the bbox to inch coordinates
  return TScale(1.0 / dpiX, 1.0 / dpiY) * bbox;
}
