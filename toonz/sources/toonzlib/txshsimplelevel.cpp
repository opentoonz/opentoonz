

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
#include "toonz/tcamera.h"

// TnzBase includes
#include "tenv.h"

// TnzCore includes
#include "trasterimage.h"
#include "tvectorimage.h"
#include "tmetaimage.h"
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
#include "tfilepath.h"

// Qt includes
#include <QDir>
#include <QRegularExpression>
#include <QMessageBox>
#include <QtCore>

#include "../common/psdlib/psd.h"
#include "toonz/toonzfolders.h"

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
    0x00F1,  // Mask written. NOTE: Student main must be 0x00F2
             // NOTE: The 0x00F0 range is currently unused
    0x0000,  // Mandatory mask: Loaded levels MUST have all these bits set
             // NOTE: If != 0 old levels (without a mask) cannot be loaded
             // NOTE: This mask is currently not used.
    0x000E   // Forbidden mask:Loaded levels MUST NOT have any of these bits set
};

//-----------------------------------------------------------------------------

inline std::string rasterized(const std::string& id) {
  return id + "_rasterized";
}
inline std::string filled(const std::string& id) { return id + "_filled"; }

//-----------------------------------------------------------------------------

QString getCreatorString() {
  QString creator = QString::fromStdString(TEnv::getApplicationName()) + " " +
                    QString::fromStdString(TEnv::getApplicationVersion()) +
                    " CM(" + QString::number(compatibility.writeMask, 16) + ")";
  return creator;
}

//-----------------------------------------------------------------------------

bool checkCreatorString(const QString& creator) {
  int mask = 0;
  if (!creator.isEmpty()) {
    QRegularExpression rx(R"(CM\([0-9A-Fa-f]*\))");
    QRegularExpressionMatch match = rx.match(creator);
    if (match.hasMatch()) {
      QString v = match.captured(0);
      if (v.length() > 4) {
        v       = v.mid(3, v.length() - 4);
        bool ok = true;
        mask    = v.toInt(&ok, 16);
      }
    }
  }
  return (mask & compatibility.neededMask) == compatibility.neededMask &&
         (mask & compatibility.forbiddenMask) == 0;
}

//-----------------------------------------------------------------------------

bool isAreadOnlyLevel(const TFilePath& path) {
  if (path.isEmpty() || !path.isAbsolute()) return false;

  if (path.getDots() == "." ||
      (path.getDots() == ".." &&
       (path.getType() == "tlv" || path.getType() == "tpl"))) {
    if (path.isUneditable()) return true;
    if (!TSystem::doesExistFileOrLevel(path)) return false;
    TFileStatus fs(path);
    return !fs.isWritable();
  }

  return false;
}

//-----------------------------------------------------------------------------

void getIndexesRangefromFids(TXshSimpleLevel* level,
                             const std::set<TFrameId>& fids, int& fromIndex,
                             int& toIndex) {
  if (fids.empty()) {
    fromIndex = toIndex = -1;
    return;
  }

  toIndex   = 0;
  fromIndex = level->getFrameCount() - 1;

  for (const auto& fid : fids) {
    int index = level->guessIndex(fid);
    if (index > toIndex) toIndex = index;
    if (index < fromIndex) fromIndex = index;
  }
}

}  // namespace

//******************************************************************************************
//    TXshSimpleLevel implementation
//******************************************************************************************

bool TXshSimpleLevel::m_rasterizePli        = false;
bool TXshSimpleLevel::m_fillFullColorRaster = false;

//-----------------------------------------------------------------------------

TXshSimpleLevel::TXshSimpleLevel(const std::wstring& name)
    : TXshLevel(m_classCode, name)
    , m_properties(new LevelProperties)
    , m_palette(nullptr)
    , m_idBase(std::to_string(idBaseCode++))
    , m_editableRangeUserInfo(L"")
    , m_isSubsequence(false)
    , m_16BitChannelLevel(false)
    , m_floatChannelLevel(false)
    , m_isReadOnly(false)
    , m_temporaryHookMerged(false) {}

//-----------------------------------------------------------------------------

TXshSimpleLevel::~TXshSimpleLevel() {
  clearFrames();

  if (m_palette) {
    m_palette->release();
    m_palette = nullptr;
  }
}

//-----------------------------------------------------------------------------

void TXshSimpleLevel::setEditableRange(unsigned int from, unsigned int to,
                                       const std::wstring& userName) {
  assert(from <= to && to < static_cast<unsigned int>(getFrameCount()));

  for (unsigned int i = from; i <= to; i++) {
    m_editableRange.insert(index2fid(i));
  }

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
    for (TLevel::Iterator it = level->begin(); it != level->end(); ++it) {
      TImageP img = lr->getFrameReader(it->first)->load();
      setFrame(it->first, img);
    }
  }

  // Merge temporary hook file with current hookset
  const TFilePath& hookFile = getHookPath(dstPath);
  mergeTemporaryHookFile(from, to, hookFile);
}

//-----------------------------------------------------------------------------

void TXshSimpleLevel::mergeTemporaryHookFile(unsigned int from, unsigned int to,
                                             const TFilePath& hookFile) {
  if (!TFileStatus(hookFile).doesExist()) return;

  std::unique_ptr<HookSet> tempHookSet(new HookSet);
  TIStream is(hookFile);
  std::string tagName;
  try {
    if (is.matchTag(tagName) && tagName == "hooks") {
      tempHookSet->loadData(is);
    }
  } catch (...) {
    return;
  }

  HookSet* hookSet  = getHookSet();
  int tempHookCount = tempHookSet->getHookCount();

  if (tempHookCount == 0) {
    for (unsigned int f = from; f <= to; f++) {
      TFrameId fid = index2fid(f);
      hookSet->eraseFrame(fid);
    }
  } else {
    for (int i = 0; i < tempHookCount; i++) {
      Hook* hook    = tempHookSet->getHook(i);
      Hook* newHook = hookSet->touchHook(hook->getId());
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
  m_editableRangeUserInfo.clear();
}

//-----------------------------------------------------------------------------

std::wstring TXshSimpleLevel::getEditableFileName() {
#ifdef MACOSX
  std::wstring fileName = L"." + m_path.getWideName();
#else
  std::wstring fileName = m_path.getWideName();
#endif
  fileName += L"_" + m_editableRangeUserInfo;
  int from = 0, to = 0;
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

  for (const auto& fid : m_frames) {
    m_renumberTable[fid] = fid;
  }
}

//-----------------------------------------------------------------------------

void TXshSimpleLevel::setDirtyFlag(bool on) { m_properties->setDirtyFlag(on); }

//-----------------------------------------------------------------------------

bool TXshSimpleLevel::getDirtyFlag() const {
  return m_properties->getDirtyFlag();
}

//-----------------------------------------------------------------------------

void TXshSimpleLevel::touchFrame(const TFrameId& fid) {
  m_properties->setDirtyFlag(true);
  TContentHistory* ch = getContentHistory();
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
  for (const auto& fid : m_frames) {
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

void TXshSimpleLevel::setScannedPath(const TFilePath& fp) {
  m_scannedPath = fp;
}

//-----------------------------------------------------------------------------

void TXshSimpleLevel::setPath(const TFilePath& fp, bool keepFrames) {
  m_path = fp;
  if (!keepFrames) {
    clearFrames();
    assert(getScene());
    try {
      load();
    } catch (...) {
    }
  }

  if (getType() != PLI_XSHLEVEL && !m_frames.empty()) {
    std::string imageId = getImageId(getFirstFid());
    const TImageInfo* imageInfo =
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

//-----------------------------------------------------------------------------

void TXshSimpleLevel::clonePropertiesFrom(const TXshSimpleLevel* oldSl) {
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

TPalette* TXshSimpleLevel::getPalette() const { return m_palette; }

//-----------------------------------------------------------------------------

void TXshSimpleLevel::setPalette(TPalette* palette) {
  if (m_palette != palette) {
    if (m_palette) {
      m_palette->release();
    }

    m_palette = palette;
    if (m_palette) {
      m_palette->addRef();
      if (!(getType() & FULLCOLOR_TYPE)) {
        m_palette->setPaletteName(getName());
      }
    }
  }
}

//-----------------------------------------------------------------------------

void TXshSimpleLevel::getFids(std::vector<TFrameId>& fids) const {
  fids.assign(m_frames.begin(), m_frames.end());
}

//-----------------------------------------------------------------------------

std::vector<TFrameId> TXshSimpleLevel::getFids() const {
  return std::vector<TFrameId>(m_frames.begin(), m_frames.end());
}

//-----------------------------------------------------------------------------

bool TXshSimpleLevel::isFid(const TFrameId& fid) const {
  return m_frames.count(fid) > 0;
}

//-----------------------------------------------------------------------------

const TFrameId& TXshSimpleLevel::getFrameId(int index) const {
  static const TFrameId emptyFrameId(TFrameId::NO_FRAME);
  if (index < 0 || index >= static_cast<int>(m_frames.size())) {
    return emptyFrameId;
  }
  auto it = m_frames.begin();
  std::advance(it, index);
  return *it;
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
  int frameCount = static_cast<int>(m_frames.size());
  if (frameCount < 2) {
    return 1;  // a level with zero or one frame has step=1 by definition
  }

  auto ft            = m_frames.begin();
  TFrameId firstFid  = *ft++;
  TFrameId secondFid = *ft++;

  if (!firstFid.getLetter().isEmpty() || !secondFid.getLetter().isEmpty()) {
    return 1;
  }

  int step = secondFid.getNumber() - firstFid.getNumber();
  if (step == 1) return 1;

  // check immediately if the step applies to the last frame
  TFrameId lastFid = *m_frames.rbegin();
  if (!lastFid.getLetter().isEmpty()) return 1;

  if (lastFid.getNumber() != firstFid.getNumber() + step * (frameCount - 1)) {
    return 1;
  }

  for (int i = 2; ft != m_frames.end(); ++ft, ++i) {
    const TFrameId& fid = *ft;
    if (!fid.getLetter().isEmpty()) return 1;
    if (fid.getNumber() != firstFid.getNumber() + step * i) return 1;
  }

  return step;
}

//-----------------------------------------------------------------------------

int TXshSimpleLevel::fid2index(const TFrameId& fid) const {
  auto ft = m_frames.find(fid);
  return (ft != m_frames.end())
             ? static_cast<int>(std::distance(m_frames.begin(), ft))
             : -1;
}

//-----------------------------------------------------------------------------

int TXshSimpleLevel::guessIndex(const TFrameId& fid) const {
  if (m_frames.empty()) return 0;  // no frames, return 0 (by definition)

  auto ft = m_frames.lower_bound(fid);
  if (ft == m_frames.end()) {
    const TFrameId& maxFid = *m_frames.rbegin();
    assert(fid > maxFid);

    // fid not in the table, but greater than the last one.
    int step = guessStep();
    int i    = (fid.getNumber() - maxFid.getNumber()) / step;
    return static_cast<int>(m_frames.size()) - 1 + i;
  } else {
    return static_cast<int>(std::distance(m_frames.begin(), ft));
  }
}

//-----------------------------------------------------------------------------

TFrameId TXshSimpleLevel::index2fid(int index) const {
  if (index < 0) return TFrameId(-2);

  int frameCount = static_cast<int>(m_frames.size());
  if (frameCount == 0) return TFrameId(1);  // o_o?

  if (index < frameCount) {
    auto ft = m_frames.begin();
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

TImageP TXshSimpleLevel::getFrame(const TFrameId& fid, UCHAR imFlags,
                                  int subsampling) const {
  assert(m_type != UNKNOWN_XSHLEVEL);

  // If the required frame is not in range, quit
  if (m_frames.count(fid) == 0) return TImageP();

  const std::string& imgId = getImageId(fid);

  ImageLoader::BuildExtData extData(this, fid, subsampling);
  TImageP img = ImageManager::instance()->getImage(imgId, imFlags, &extData);

  if (imFlags & ImageManager::toBeModified) {
    texture_utils::invalidateTexture(this, fid);
  }

  return img;
}

//-----------------------------------------------------------------------------

TImageInfo* TXshSimpleLevel::getFrameInfo(const TFrameId& fid,
                                          bool toBeModified) {
  assert(m_type != UNKNOWN_XSHLEVEL);

  // If the required frame is not in range, quit
  if (m_frames.count(fid) == 0) return nullptr;

  const std::string& imgId = getImageId(fid);

  TImageInfo* info = ImageManager::instance()->getInfo(
      imgId, toBeModified ? ImageManager::toBeModified : ImageManager::none, 0);

  return info;
}

//-----------------------------------------------------------------------------

TImageP TXshSimpleLevel::getFrameIcon(const TFrameId& fid) const {
  assert(m_type != UNKNOWN_XSHLEVEL);

  if (m_frames.count(fid) == 0) return TImageP();

  ImageLoader::BuildExtData extData(this, fid);
  extData.m_subs = 1;
  extData.m_icon = true;

  const std::string& imgId = getImageId(fid);
  TImageP img              = ImageManager::instance()->getImage(
      imgId, ImageManager::dontPutInCache, &extData);

  if (TToonzImageP timg = img) {
    if (m_palette) timg->setPalette(m_palette);
  }

  return img;
}

//-----------------------------------------------------------------------------

void TXshSimpleLevel::loadAllIconsAndPutInCache(bool cacheImagesAsWell) {
  if (m_type != TZP_XSHLEVEL && m_type != OVL_XSHLEVEL) return;

  std::vector<TFrameId> fids;
  getFids(fids);

  std::vector<std::string> iconIds;
  iconIds.reserve(fids.size());

  for (const auto& fid : fids) {
    iconIds.push_back(getIconId(fid));
  }

  ImageManager::instance()->loadAllTlvIconsAndPutInCache(this, fids, iconIds,
                                                         cacheImagesAsWell);
}

//-----------------------------------------------------------------------------

TRasterImageP TXshSimpleLevel::getFrameToCleanup(const TFrameId& fid,
                                                 bool toBeLineProcessed) const {
  assert(m_type != UNKNOWN_XSHLEVEL);

  auto ft = m_frames.find(fid);
  if (ft == m_frames.end()) return TRasterImageP();

  bool flag           = (m_scannedPath != TFilePath());
  std::string imageId = getImageId(fid, flag ? Scanned : 0);

  ImageLoader::BuildExtData extData(this, fid, 1);

  UCHAR imFlags = ImageManager::dontPutInCache;
  if (!toBeLineProcessed) imFlags |= ImageManager::is64bitEnabled;

  TRasterImageP img =
      ImageManager::instance()->getImage(imageId, imFlags, &extData);
  if (!img) return img;

  double x_dpi = 0, y_dpi = 0;
  img->getDpi(x_dpi, y_dpi);
  if (x_dpi == 0.0 && y_dpi == 0.0) {
    TPointD dpi = m_properties->getDpi();
    img->setDpi(dpi.x, dpi.y);
  }

  return img;
}

//-----------------------------------------------------------------------------

TImageP TXshSimpleLevel::getFullsampledFrame(const TFrameId& fid,
                                             UCHAR imFlags) const {
  assert(m_type != UNKNOWN_XSHLEVEL);

  auto it = m_frames.find(fid);
  if (it == m_frames.end()) return TImageP();

  std::string imageId = getImageId(fid);

  ImageLoader::BuildExtData extData(this, fid, 1);
  TImageP img = ImageManager::instance()->getImage(imageId, imFlags, &extData);

  if (imFlags & ImageManager::toBeModified) {
    texture_utils::invalidateTexture(this, fid);
  }

  return img;
}

//-----------------------------------------------------------------------------

TRasterImageP TXshSimpleLevel::getFrameRasterized(const TFrameId& fid,
                                                  TPointD dpi) const {
  assert(m_type == PLI_XSHLEVEL);

  auto it = m_frames.find(fid);
  if (it == m_frames.end()) return TRasterImageP();

  std::string imageId = rasterized(getImageId(fid));
  ImageLoader::BuildExtData extData(this, fid, 1);
  extData.m_cameraDPI = dpi;
  TImageP img =
      ImageManager::instance()->getImage(imageId, ImageManager::none, &extData);
  return TRasterImageP(img);
}

//-----------------------------------------------------------------------------

std::string TXshSimpleLevel::getIconId(const TFrameId& fid,
                                       int frameStatus) const {
  return "icon:" + getImageId(fid, frameStatus);
}

//-----------------------------------------------------------------------------

std::string TXshSimpleLevel::getIconId(const TFrameId& fid,
                                       const TDimension& size) const {
  return getImageId(fid) + ":" + std::to_string(size.lx) + "x" +
         std::to_string(size.ly);
}

//-----------------------------------------------------------------------------

namespace {

TAffine getAffine(const TDimension& srcSize, const TDimension& dstSize) {
  double scx = 1.0 * dstSize.lx / static_cast<double>(srcSize.lx);
  double scy = 1.0 * dstSize.ly / static_cast<double>(srcSize.ly);
  double sc  = std::min(scx, scy);
  double dx  = (dstSize.lx - srcSize.lx * sc) * 0.5;
  double dy  = (dstSize.ly - srcSize.ly * sc) * 0.5;
  return TScale(sc) *
         TTranslation(0.5 * TPointD(srcSize.lx, srcSize.ly) + TPointD(dx, dy));
}

//-----------------------------------------------------------------------------

TImageP buildIcon(const TImageP& img, const TDimension& size) {
  TRaster32P raster(size);
  if (TVectorImageP vi = img) {
    std::unique_ptr<TOfflineGL> glContext(new TOfflineGL(size));
    TDimension cameraSize(1920, 1080);
    TPalette* vPalette = img->getPalette();
    assert(vPalette);
    const TVectorRenderData rd(getAffine(cameraSize, size), TRect(), vPalette,
                               0, false);
    glContext->clear(TPixel32::White);
    glContext->draw(vi, rd);
    raster->copy(glContext->getRaster());
  } else if (TToonzImageP ti = img) {
    raster->fill(TPixel32(255, 255, 255, 255));
    TRasterCM32P rasCM32 = ti->getRaster();
    TRect bbox           = ti->getSavebox();
    if (!bbox.isEmpty()) {
      rasCM32   = rasCM32->extract(bbox);
      double sx = static_cast<double>(raster->getLx()) / rasCM32->getLx();
      double sy = static_cast<double>(raster->getLy()) / rasCM32->getLy();
      double sc = std::min(sx, sy);
      TAffine aff =
          TScale(sc).place(rasCM32->getCenterD(), raster->getCenterD());
      TRop::resample(raster, rasCM32, ti->getPalette(), aff);

      raster->lock();
      for (int y = 0; y < raster->getLy(); y++) {
        TPixel32* pix    = raster->pixels(y);
        TPixel32* endPix = pix + raster->getLx();
        while (pix < endPix) {
          *pix = overPix(TPixel32::White, *pix);
          ++pix;
        }
      }
      raster->unlock();
    }
  } else if (TRasterImageP ri = img) {
    ri->makeIcon(raster);
    TRop::addBackground(raster, TPixel32::White);
  } else {
    raster->fill(TPixel32(127, 50, 20));
  }

  return TRasterImageP(raster);
}

}  // anonymous namespace

//-----------------------------------------------------------------------------

void TXshSimpleLevel::formatFId(TFrameId& fid, TFrameId _tmplFId) {
  if (m_type != OVL_XSHLEVEL && m_type != TZI_XSHLEVEL) return;

  if (!m_frames.empty()) {
    TFrameId tmplFId = *m_frames.begin();
    fid.setZeroPadding(tmplFId.getZeroPadding());
    fid.setStartSeqInd(tmplFId.getStartSeqInd());
  } else {
    QChar sepChar = m_path.getSepChar();
    if (!sepChar.isNull()) _tmplFId.setStartSeqInd(sepChar.toLatin1());
    fid.setZeroPadding(_tmplFId.getZeroPadding());
    fid.setStartSeqInd(_tmplFId.getStartSeqInd());
  }
}

//-----------------------------------------------------------------------------

void TXshSimpleLevel::setFrame(const TFrameId& fid, const TImageP& img) {
  assert(m_type != UNKNOWN_XSHLEVEL);

  if (img) {
    img->setPalette(getPalette());
  }

  m_frames.insert(fid);

  TFilePath path                         = m_path;
  int frameStatus                        = getFrameStatus(fid);
  static const int SCANNED_OR_CLEANUPPED = (Scanned | Cleanupped);

  if ((frameStatus & SCANNED_OR_CLEANUPPED) == Scanned) {
    path = m_scannedPath;
  }

  const std::string& imageId = getImageId(fid);

  if (!ImageManager::instance()->isBound(imageId)) {
    const TFilePath& decodedPath = getScene()->decodeFilePath(path);
    ImageManager::instance()->bind(imageId, new ImageLoader(decodedPath, fid));
  }

  ImageManager::instance()->setImage(imageId, img);

  if (frameStatus == Normal) {
    if (m_type == PLI_XSHLEVEL) {
      const std::string& imageId2 = rasterized(imageId);
      if (!ImageManager::instance()->isBound(imageId2)) {
        ImageManager::instance()->bind(imageId2, new ImageRasterizer);
      } else {
        ImageManager::instance()->invalidate(imageId2);
      }
    }

    if (m_type == OVL_XSHLEVEL || m_type == TZI_XSHLEVEL) {
      const std::string& imageId2 = filled(imageId);
      if (!ImageManager::instance()->isBound(imageId2)) {
        ImageManager::instance()->bind(imageId2, new ImageFiller);
      } else {
        ImageManager::instance()->invalidate(imageId2);
      }
    }
  }
}

//-----------------------------------------------------------------------------

void TXshSimpleLevel::eraseFrame(const TFrameId& fid) {
  auto ft = m_frames.find(fid);
  if (ft == m_frames.end()) return;

  // Erase the corresponding entry in the renumber table
  for (auto it = m_renumberTable.begin(); it != m_renumberTable.end(); ++it) {
    if (it->second == fid) {
      m_renumberTable.erase(it);
      break;
    }
  }

  m_frames.erase(ft);
  getHookSet()->eraseFrame(fid);

  ImageManager* im = ImageManager::instance();
  TImageCache* ic  = TImageCache::instance();

  im->unbind(getImageId(fid, Normal));
  im->unbind(getImageId(fid, Scanned));
  im->unbind(getImageId(fid, CleanupPreview));

  ic->remove(getIconId(fid, Normal));
  ic->remove(getIconId(fid, Scanned));
  ic->remove(getIconId(fid, CleanupPreview));

  if (m_type == PLI_XSHLEVEL) {
    im->unbind(rasterized(getImageId(fid)));
  }

  if (m_type == OVL_XSHLEVEL || m_type == TZI_XSHLEVEL) {
    im->unbind(filled(getImageId(fid)));
  }

  texture_utils::invalidateTexture(this, fid);
}

//-----------------------------------------------------------------------------

void TXshSimpleLevel::clearFrames() {
  ImageManager* im = ImageManager::instance();
  TImageCache* ic  = TImageCache::instance();

  for (const auto& fid : m_frames) {
    im->unbind(getImageId(fid, Scanned));
    im->unbind(getImageId(fid, Cleanupped));
    im->unbind(getImageId(fid, CleanupPreview));

    ic->remove(getIconId(fid, Normal));
    ic->remove(getIconId(fid, Scanned));
    ic->remove(getIconId(fid, CleanupPreview));

    if (m_type == PLI_XSHLEVEL) {
      im->unbind(rasterized(getImageId(fid)));
    }

    if (m_type == OVL_XSHLEVEL || m_type == TZI_XSHLEVEL) {
      im->unbind(filled(getImageId(fid)));
    }

    texture_utils::invalidateTexture(this, fid);
  }

  m_frames.clear();
  m_editableRange.clear();
  m_editableRangeUserInfo.clear();
  m_renumberTable.clear();
  m_framesStatus.clear();
}

//-----------------------------------------------------------------------------

void TXshSimpleLevel::loadData(TIStream& is) {
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
        int subsampling        = 1;
        int doPremultiply      = 0;
        int whiteTransp        = 0;
        int antialiasSoftness  = 0;
        int isStopMotionLevel  = 0;
        double colorSpaceGamma = LevelOptions::DefaultColorSpaceGamma;
        LevelProperties::DpiPolicy dpiPolicy = LevelProperties::DP_ImageDpi;

        if (is.getTagParam("dpix", v)) xdpi = std::stod(v);
        if (is.getTagParam("dpiy", v)) ydpi = std::stod(v);
        if (xdpi != 0 && ydpi != 0) dpiPolicy = LevelProperties::DP_CustomDpi;

        std::string dpiType = is.getTagAttribute("dpiType");
        if (dpiType == "image") dpiPolicy = LevelProperties::DP_ImageDpi;

        if (is.getTagParam("type", v) && v == "s") type = TZI_XSHLEVEL;
        if (is.getTagParam("subsampling", v)) subsampling = std::stoi(v);
        if (is.getTagParam("premultiply", v)) doPremultiply = std::stoi(v);
        if (is.getTagParam("antialias", v)) antialiasSoftness = std::stoi(v);
        if (is.getTagParam("whiteTransp", v)) whiteTransp = std::stoi(v);
        if (is.getTagParam("isStopMotionLevel", v))
          isStopMotionLevel = std::stoi(v);
        if (is.getTagParam("colorSpaceGamma", v))
          colorSpaceGamma = std::stod(v);

        m_properties->setDpiPolicy(dpiPolicy);
        m_properties->setDpi(TPointD(xdpi, ydpi));
        m_properties->setSubsampling(subsampling);
        m_properties->setDoPremultiply(doPremultiply);
        m_properties->setDoAntialias(antialiasSoftness);
        m_properties->setWhiteTransp(whiteTransp);
        m_properties->setIsStopMotion(isStopMotionLevel);
        m_properties->setColorSpaceGamma(colorSpaceGamma);

        if (isStopMotionLevel == 1) setIsReadOnly(true);
      } else {
        throw TException("unexpected tag " + tagName);
      }
    } else {
      if (flag) break;
      flag = true;
      std::wstring token;
      is >> token;
      if (token == L"__empty") {
        is >> token;
      }

      if (token == L"_raster") {
        double xdpi = 1, ydpi = 1;
        is >> xdpi >> ydpi >> m_name;
        setName(m_name);
        type = OVL_XSHLEVEL;
        m_properties->setDpi(TPointD(xdpi, ydpi));
        setType(type);
        setPath(
            TFilePath("+drawings/") + (getName() + L"." + ::to_wstring("bmp")),
            true);
      } else if (token == L"__raster") {
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
    else if (ext == "tzm")
      type = META_XSHLEVEL;
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

  bool match(const TFrameId& fid) const {
    return ((m_fromFid <= fid && fid <= m_toFid) || m_fromFid > m_toFid);
  }
  bool isEnabled() const { return m_fromFid <= m_toFid; }
  void reset() {
    m_fromFid = TFrameId(1);
    m_toFid   = TFrameId(0);
  }
} loadingLevelRange;

}  // namespace

//-----------------------------------------------------------------------------

void setLoadingLevelRange(const TFrameId& fromFid, const TFrameId& toFid) {
  loadingLevelRange.m_fromFid = fromFid;
  loadingLevelRange.m_toFid   = toFid;
}

void getLoadingLevelRange(TFrameId& fromFid, TFrameId& toFid) {
  fromFid = loadingLevelRange.m_fromFid;
  toFid   = loadingLevelRange.m_toFid;
}

//-----------------------------------------------------------------------------

static TFilePath getLevelPathAndSetNameWithPsdLevelName(
    TXshSimpleLevel* xshLevel) {
  TFilePath retfp = xshLevel->getPath();

  QString name        = QString::fromStdWString(retfp.getWideName());
  bool removeFileName = name.contains("##");
  if (removeFileName) {
    retfp = TFilePath(
        QString::fromStdWString(retfp.getWideString()).replace("##", "#"));
  }
  QStringList list = name.split("#", Qt::SkipEmptyParts);

  if (list.size() >= 2 && list.at(1) != "frames") {
    bool hasLayerId;
    int layid                  = list.at(1).toInt(&hasLayerId);
    QTextCodec* layerNameCodec = QTextCodec::codecForName(
        Preferences::instance()->getLayerNameEncoding().c_str());

    if (hasLayerId) {
      TPSDParser psdparser(xshLevel->getScene()->decodeFilePath(retfp));
      std::string levelName = psdparser.getLevelNameWithCounter(layid);

      list[1]                 = layerNameCodec->toUnicode(levelName.c_str());
      std::wstring wLevelName = list.join("#").toStdWString();
      retfp                   = retfp.withName(wLevelName);

      if (removeFileName) wLevelName = list[1].toStdWString();

      TLevelSet* levelSet = xshLevel->getScene()->getLevelSet();
      if (levelSet && levelSet->hasLevel(wLevelName)) {
        levelSet->renameLevel(xshLevel, wLevelName);
      }

      xshLevel->setName(wLevelName);
    }
  }

  return retfp;
}

//-----------------------------------------------------------------------------

void TXshSimpleLevel::load() {
  getProperties()->setCreator("");
  QString creator;

  assert(getScene());
  if (!getScene()) return;

  m_isSubsequence = loadingLevelRange.isEnabled();

  TFilePath checkpath = getScene()->decodeFilePath(m_path);
  std::string type    = checkpath.getType();

  if (m_scannedPath != TFilePath()) {
    getProperties()->setDirtyFlag(false);

    static const int ScannedCleanuppedMask = Scanned | Cleanupped;
    TFilePath path = getScene()->decodeFilePath(m_scannedPath);
    if (TSystem::doesExistFileOrLevel(path)) {
      TLevelReaderP lr(path);
      assert(lr);
      TLevelP level = lr->loadInfo();
      if (!checkCreatorString(creator = lr->getCreator())) {
        getProperties()->setIsForbidden(true);
      } else {
        for (TLevel::Iterator it = level->begin(); it != level->end(); ++it) {
          TFrameId fid = it->first;
          if (!loadingLevelRange.match(fid)) continue;
          setFrameStatus(
              fid, (getFrameStatus(fid) & ~ScannedCleanuppedMask) | Scanned);
          setFrame(fid, TImageP());
        }
      }
    }

    path = getScene()->decodeFilePath(m_path);
    if (TSystem::doesExistFileOrLevel(path)) {
      TLevelReaderP lr(path);
      assert(lr);
      TLevelP level = lr->loadInfo();
      if (getType() & FULLCOLOR_TYPE) {
        setPalette(FullColorPalette::instance()->getPalette(getScene()));
      } else {
        setPalette(level->getPalette());
      }
      if (!checkCreatorString(creator = lr->getCreator())) {
        getProperties()->setIsForbidden(true);
      } else {
        for (TLevel::Iterator it = level->begin(); it != level->end(); ++it) {
          TFrameId fid = it->first;
          if (!loadingLevelRange.match(fid)) continue;
          setFrameStatus(fid, getFrameStatus(fid) | Cleanupped);
          setFrame(fid, TImageP());
        }
      }
      setContentHistory(
          lr->getContentHistory() ? lr->getContentHistory()->clone() : nullptr);
    }
  } else {
    if (m_path.getType() == "psd" && !this->getScene()->isLoading()) {
      m_path = getLevelPathAndSetNameWithPsdLevelName(this);
    }

    TFilePath path = getScene()->decodeFilePath(m_path);
    getProperties()->setDirtyFlag(false);

    TLevelReaderP lr(path);
    assert(lr);

    TLevelP level = lr->loadInfo();
    if (level->isPartialLoad()) {
      QString msg =
          QString(
              "File '%1' partially loaded. Not all frames were found. "
              "Possible file corruption. Loaded what could be found.\n"
              "Recommend replacing any bad frames in Level Strip and saving.")
              .arg(QString::fromStdWString(m_path.getWideString()));
      QMessageBox::warning(nullptr, "File load warning", msg);
      setDirtyFlag(true);
    }

    if (level->getFrameCount() > 0) {
      const TImageInfo* info = lr->getImageInfo(level->begin()->first);
      if (info && info->m_samplePerPixel >= 5) {
        QString msg = QString(
                          "Failed to open %1.\nSamples per pixel is more than "
                          "4. It may contain more than one alpha channel.")
                          .arg(QString::fromStdWString(m_path.getWideString()));
        QMessageBox::warning(nullptr, "Image format not supported", msg);
        return;
      }

      if (info) {
        set16BitChannelLevel(info->m_bitsPerSample == 16);
        setFloatChannelLevel(info->m_bitsPerSample == 32);
      }
    }

    if ((getType() & FULLCOLOR_TYPE) && !is16BitChannelLevel()) {
      setPalette(FullColorPalette::instance()->getPalette(getScene()));
    } else {
      setPalette(level->getPalette());
    }

    if (!checkCreatorString(creator = lr->getCreator())) {
      getProperties()->setIsForbidden(true);
    } else {
      for (TLevel::Iterator it = level->begin(); it != level->end(); ++it) {
        m_renumberTable[it->first] = it->first;
        if (!loadingLevelRange.match(it->first)) continue;
        setFrame(it->first, TImageP());
      }
    }

    setContentHistory(lr->getContentHistory() ? lr->getContentHistory()->clone()
                                              : nullptr);
  }

  getProperties()->setCreator(creator.toStdString());
  loadingLevelRange.reset();

  if (getType() != PLI_XSHLEVEL) {
    if (m_properties->getImageDpi() == TPointD() && !m_frames.empty()) {
      TDimension imageRes(0, 0);
      TPointD imageDpi;

      const TFrameId& firstFid = getFirstFid();
      std::string imageId      = getImageId(firstFid);

      const TImageInfo* imageInfo =
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

  if (getPalette() && StudioPalette::isEnabled()) {
    StudioPalette::instance()->updateLinkedColors(getPalette());
  }

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
            img->setPalette(nullptr);
            getPalette()->setRefImg(img);
            std::vector<TFrameId> fids = getPalette()->getRefLevelFids();
            if (fids.empty()) {
              for (TLevel::Iterator it = level->begin(); it != level->end();
                   ++it) {
                fids.push_back(it->first);
              }
              getPalette()->setRefLevelFids(fids, false);
            }
          }
        }
      }
    }
  }

  HookSet* hookSet = getHookSet();
  hookSet->clearHooks();

  const TFilePath& hookFile =
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

void TXshSimpleLevel::load(const std::vector<TFrameId>& fIds) {
  getProperties()->setCreator("");
  QString creator;
  assert(getScene());
  getProperties()->setDirtyFlag(false);

  m_isSubsequence = loadingLevelRange.isEnabled();

  TFilePath path = getScene()->decodeFilePath(m_path);
  TLevelReaderP lr(path);
  assert(lr);

  if (!checkCreatorString(creator = lr->getCreator())) {
    getProperties()->setIsForbidden(true);
  } else {
    if (!fIds.empty()) {
      for (const auto& fid : fIds) {
        m_renumberTable[fid] = fid;
        if (!loadingLevelRange.match(fid)) continue;
        setFrame(fid, TImageP());
      }
      const TImageInfo* info = lr->getImageInfo(fIds[0]);
      if (info) {
        set16BitChannelLevel(info->m_bitsPerSample == 16);
        setFloatChannelLevel(info->m_bitsPerSample == 32);
      }
    } else {
      TLevelP level = lr->loadInfo();
      for (TLevel::Iterator it = level->begin(); it != level->end(); ++it) {
        m_renumberTable[it->first] = it->first;
        if (!loadingLevelRange.match(it->first)) continue;
        setFrame(it->first, TImageP());
      }
      const TImageInfo* info = lr->getImageInfo(level->begin()->first);
      if (info) {
        set16BitChannelLevel(info->m_bitsPerSample == 16);
        setFloatChannelLevel(info->m_bitsPerSample == 32);
      }
    }

    if ((getType() & FULLCOLOR_TYPE) && !is16BitChannelLevel()) {
      setPalette(FullColorPalette::instance()->getPalette(getScene()));
    }
  }

  setContentHistory(lr->getContentHistory() ? lr->getContentHistory()->clone()
                                            : nullptr);

  getProperties()->setCreator(creator.toStdString());
  loadingLevelRange.reset();

  if (getType() != PLI_XSHLEVEL) {
    if (m_properties->getImageDpi() == TPointD() && !m_frames.empty()) {
      TDimension imageRes(0, 0);
      TPointD imageDpi;
      std::string imageId = getImageId(getFirstFid());
      const TImageInfo* imageInfo =
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

void TXshSimpleLevel::saveData(TOStream& os) {
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
  } else if (getProperties()->isStopMotionLevel()) {
    attr["isStopMotionLevel"] =
        std::to_string(getProperties()->isStopMotionLevel());
  }
  if (!areAlmostEqual(getProperties()->colorSpaceGamma(),
                      LevelOptions::DefaultColorSpaceGamma)) {
    attr["colorSpaceGamma"] =
        std::to_string(getProperties()->colorSpaceGamma());
  }

  if (m_type == TZI_XSHLEVEL) attr["type"] = "s";

  os.openCloseChild("info", attr);

  os.child("path") << m_path;
  if (m_scannedPath != TFilePath()) os.child("scannedPath") << m_scannedPath;
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
  if (path.isLevelName()) {
    TFilePathSet files =
        TSystem::readDirectory(path.getParentDir(), false, true);
    for (TFilePathSet::iterator file = files.begin(); file != files.end();
         ++file) {
      if (file->getLevelName() == path.getLevelName()) saveBackup(*file);
    }
    return;
  }

  int totalBackups = Preferences::instance()->getBackupKeepCount();
  totalBackups -= 1;
  TFilePath backup = path.withType(path.getType() + ".bak");
  TFilePath prevBackup =
      path.withType(path.getType() + ".bak" + std::to_string(totalBackups));
  while (--totalBackups >= 0) {
    std::string bakExt =
        ".bak" + (totalBackups > 0 ? std::to_string(totalBackups) : "");
    backup = path.withType(path.getType() + bakExt);
    if (TSystem::doesExistFileOrLevel(backup)) {
      try {
        TSystem::copyFileOrLevel_throw(prevBackup, backup);
      } catch (...) {
      }
    }
    prevBackup = backup;
  }

  try {
    if (TSystem::doesExistFileOrLevel(backup))
      TSystem::removeFileOrLevel_throw(backup);
    TSystem::copyFileOrLevel_throw(backup, path);
  } catch (...) {
  }
}

//-----------------------------------------------------------------------------

void TXshSimpleLevel::save(const TFilePath& fp, const TFilePath& oldFp,
                           bool overwritePalette) {
  TFilePath dOldPath =
      (!oldFp.isEmpty()) ? oldFp : getScene()->decodeFilePath(m_path);

  TFilePath dDstPath = getScene()->decodeFilePath(fp);
  if (!TSystem::touchParentDir(dDstPath))
    throw TSystemException(
        dDstPath,
        "The level cannot be saved: failed to access the target folder.");

  if (Preferences::instance()->isBackupEnabled() && dOldPath == dDstPath &&
      TSystem::doesExistFileOrLevel(dDstPath) &&
      !getProperties()->isStopMotionLevel()) {
    saveBackup(dDstPath);
  }

  if (isAreadOnlyLevel(dDstPath)) {
    if (m_editableRange.empty() && !m_temporaryHookMerged) {
      throw TSystemException(
          dDstPath, "The level cannot be saved: it is a read only level.");
    } else if (getType() != OVL_XSHLEVEL) {
      std::wstring fileName = getEditableFileName();
      assert(!fileName.empty());

      TFilePath app = dDstPath.withName(fileName).withType(dDstPath.getType());

      if (TSystem::doesExistFileOrLevel(app)) TSystem::removeFileOrLevel(app);

      TFilePathSet oldFilePaths;
      getFiles(app, oldFilePaths);

      for (auto it = oldFilePaths.begin(); it != oldFilePaths.end(); ++it) {
        if (TSystem::doesExistFileOrLevel(*it)) TSystem::removeFileOrLevel(*it);
      }

      TXshSimpleLevel* sl = new TXshSimpleLevel;
      sl->setScene(getScene());
      sl->setPalette(getPalette());
      sl->setPath(getScene()->codeFilePath(app));
      sl->setType(getType());
      sl->setDirtyFlag(getDirtyFlag());
      sl->addRef();

      for (const auto& fid : m_editableRange) {
        sl->setFrame(fid, getFrame(fid, false));
      }

      HookSet* hookSet = sl->getHookSet();
      *hookSet         = *getHookSet();

      for (const auto& fid : m_frames) {
        if (m_editableRange.find(fid) == m_editableRange.end()) {
          hookSet->eraseFrame(fid);
        }
      }

      sl->setRenumberTable();
      sl->save(app);

#ifdef _WIN32
      if (TSystem::doesExistFileOrLevel(app)) TSystem::hideFileOrLevel(app);
      oldFilePaths.clear();
      getFiles(app, oldFilePaths);
      for (auto it = oldFilePaths.begin(); it != oldFilePaths.end(); ++it) {
        if (TSystem::doesExistFileOrLevel(*it)) TSystem::hideFileOrLevel(*it);
      }
#endif
      return;
    }
  }

  if (dOldPath != dDstPath && m_path != TFilePath()) {
    const TFilePath& dSrcPath = dOldPath;
    try {
      if (TSystem::doesExistFileOrLevel(dSrcPath)) {
        if (TSystem::doesExistFileOrLevel(dDstPath))
          TSystem::removeFileOrLevel(dDstPath);
        copyFiles(dDstPath, dSrcPath);
      }
    } catch (...) {
    }
  }

  if (overwritePalette && getType() == TZP_XSHLEVEL && getPalette() &&
      getPalette()->getGlobalName() != L"") {
    overwritePalette      = false;
    TFilePath palettePath = dDstPath.withNoFrame().withType("tpl");
    StudioPalette::instance()->save(palettePath, getPalette());
    getPalette()->setDirtyFlag(false);
  }

  saveSimpleLevel(dDstPath, overwritePalette);
}

//-----------------------------------------------------------------------------

void TXshSimpleLevel::saveSimpleLevel(const TFilePath& decodedFp,
                                      bool overwritePalette) {
  TFilePath oldPath  = m_path;
  TFilePath dOldPath = getScene()->decodeFilePath(oldPath);

  struct CopyOnExit {
    TFilePath &m_dstPath, &m_srcPath;
    ~CopyOnExit() { m_dstPath = m_srcPath; }
  } copyOnExit = {m_path = decodedFp, oldPath};

  bool savingOriginal = (decodedFp == dOldPath), paletteNotSaved = false;

  int imFlags = savingOriginal
                    ? ImageManager::dontPutInCache | ImageManager::toBeSaved
                    : ImageManager::dontPutInCache;

  std::vector<TFrameId> fids;
  getFids(fids);

  bool isLevelModified   = getProperties()->getDirtyFlag();
  bool isPaletteModified = false;
  if (getPalette()) isPaletteModified = getPalette()->getDirtyFlag();

  if (isLevelModified || isPaletteModified) {
    TDimension oldRes(0, 0);
    bool fileOrLevelRemoved = false;
    if (TSystem::doesExistFileOrLevel(decodedFp)) {
      TLevelReaderP lr(decodedFp);
      lr->doReadPalette(false);
      try {
        const TImageInfo* imageInfo =
            m_frames.empty() ? lr->getImageInfo()
                             : lr->getImageInfo(*(m_frames.begin()));
        if (getType() != MESH_XSHLEVEL && imageInfo) {
          oldRes.lx = imageInfo->m_lx;
          oldRes.ly = imageInfo->m_ly;
          lr        = TLevelReaderP();
          if (getProperties()->getImageRes() != oldRes) {
            TSystem::removeFileOrLevel(decodedFp);
            fileOrLevelRemoved = true;
          }
        }
      } catch (...) {
      }
    }

    if (decodedFp.getType() == "tlv" &&
        TSystem::doesExistFileOrLevel(decodedFp)) {
      if (isLevelModified) {
        int oldSubs = getProperties()->getSubsampling();
        TLevelWriterP lw;
        try {
          lw = TLevelWriterP(decodedFp);
        } catch (...) {
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

        for (auto const& fid : fids) {
          std::string imageId = getImageId(fid, Normal);
          if (!fileOrLevelRemoved &&
              !ImageManager::instance()->isModified(imageId))
            continue;

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
            TRect saveBox;
            TRop::computeBBox(ti->getRaster(), saveBox);
            ti->setSavebox(saveBox);
          }

          lw->getFrameWriter(fid)->save(img);
        }

        lw = TLevelWriterP();
      } else if (isPaletteModified && overwritePalette) {
        TFilePath palettePath = decodedFp.withNoFrame().withType("tpl");
        if (Preferences::instance()->isBackupEnabled() &&
            TSystem::doesExistFileOrLevel(palettePath))
          saveBackup(palettePath);
        TOStream os(palettePath);
        if (os.checkStatus())
          os << getPalette();
        else
          paletteNotSaved = true;
      }
    } else {
      LevelUpdater updater(this);
      updater.getLevelWriter()->setCreator(getCreatorString());
      if (updater.getImageInfo())
        updater.getLevelWriter()->setFrameRate(
            updater.getImageInfo()->m_frameRate);

      if (isLevelModified) {
        updater.getLevelWriter()->renumberFids(m_renumberTable);

        if (!m_editableRange.empty())
          fids = std::vector<TFrameId>(m_editableRange.begin(),
                                       m_editableRange.end());

        ImageLoader::BuildExtData extData(this, TFrameId());

        for (auto const& fid : fids) {
          std::string imageId = getImageId(fid, Normal);
          if (!fileOrLevelRemoved &&
              !ImageManager::instance()->isModified(imageId))
            continue;

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
      updater.close();
      if ((getType() & FULLCOLOR_TYPE) && isPaletteModified)
        FullColorPalette::instance()->savePalette(getScene());
    }
  }

  TFilePath hookFile;
  HookSet* hookSet = nullptr;

  if (getType() == OVL_XSHLEVEL && !m_editableRange.empty()) {
    hookSet = new HookSet(*getHookSet());

    for (const auto& fid : m_frames) {
      if (m_editableRange.find(fid) == m_editableRange.end()) {
        hookSet->eraseFrame(fid);
      }
    }

    std::wstring fileName = getEditableFileName();
    assert(!fileName.empty());
    TFilePath app = decodedFp.withName(fileName).withType(decodedFp.getType());
    hookFile      = getHookPath(app);
  } else {
    hookFile = getHookPath(decodedFp);
    hookSet  = getHookSet();
  }

#ifdef _WIN32
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
    setRenumberTable();
    if (m_properties) m_properties->setDirtyFlag(false);
    if (getPalette() && overwritePalette) getPalette()->setDirtyFlag(false);
  }

  if (paletteNotSaved) {
    throw TSystemException(m_path,
                           "The palette of the level could not be saved.");
  }
}

//-----------------------------------------------------------------------------

std::string TXshSimpleLevel::getImageId(const TFrameId& fid,
                                        int frameStatus) const {
  if (frameStatus < 0) frameStatus = getFrameStatus(fid);
  std::string prefix = "L";
  if (frameStatus & CleanupPreview)
    prefix = "P";
  else if ((frameStatus & (Scanned | Cleanupped)) == Scanned)
    prefix = "S";
  std::string imageId = m_idBase + "_" + prefix + fid.expand();
  return imageId;
}

//-----------------------------------------------------------------------------

int TXshSimpleLevel::getFrameStatus(const TFrameId& fid) const {
  auto it = m_framesStatus.find(fid);
  return (it != m_framesStatus.end()) ? it->second : Normal;
}

//-----------------------------------------------------------------------------

void TXshSimpleLevel::setFrameStatus(const TFrameId& fid, int status) {
  assert((status & ~(Scanned | Cleanupped | CleanupPreview)) == 0);
  m_framesStatus[fid] = status;
}

//-----------------------------------------------------------------------------

void TXshSimpleLevel::makeTlv(const TFilePath& tlvPath) {
  int ltype = getType();

  if (!(ltype & FULLCOLOR_TYPE)) {
    assert(ltype & FULLCOLOR_TYPE);
    return;
  }

  setType(TZP_XSHLEVEL);
  m_scannedPath = m_path;
  assert(tlvPath.getType() == "tlv");
  m_path = tlvPath;

  for (const auto& fid : m_frames) {
    setFrameStatus(fid, Scanned);
    ImageManager::instance()->rebind(getImageId(fid, Scanned),
                                     getImageId(fid, 0));
    ImageManager::instance()->rebind(getIconId(fid, Scanned),
                                     getIconId(fid, 0));
  }
}

//-----------------------------------------------------------------------------

void TXshSimpleLevel::invalidateFrames() {
  for (const auto& fid : m_frames) {
    ImageManager::instance()->invalidate(getImageId(fid));
  }
}

//-----------------------------------------------------------------------------

void TXshSimpleLevel::invalidateFrame(const TFrameId& fid) {
  std::string id = getImageId(fid);
  ImageManager::instance()->invalidate(id);
}

//-----------------------------------------------------------------------------

void TXshSimpleLevel::initializePalette() {
  ToonzScene* scene = getScene();
  assert(scene);

  TFilePath fullPath;
  TPalette* palette = new TPalette();
  int type          = getType();
  switch (type) {
  case TZP_XSHLEVEL:
    fullPath =
        scene->decodeFilePath(TFilePath("+palettes\\Toonz_Raster_Palette.tpl"));
    if (TSystem::doesExistFileOrLevel(fullPath)) {
      TIStream is(fullPath);
      is >> palette;
    } else {
      TFilePath globalPath(
          ToonzFolder::getStudioPaletteFolder().getQString().append(
              "\\Global Palettes\\Default "
              "Palettes\\Toonz_Raster_Palette.tpl"));
      if (TSystem::doesExistFileOrLevel(globalPath)) {
        TIStream is(globalPath);
        is >> palette;
        TSystem::copyFile(fullPath, globalPath);
      }
    }
    break;
  case PLI_XSHLEVEL:
    fullPath =
        scene->decodeFilePath(TFilePath("+palettes\\Toonz_Vector_Palette.tpl"));
    if (TSystem::doesExistFileOrLevel(fullPath)) {
      TIStream is(fullPath);
      is >> palette;
    } else {
      TFilePath globalPath(
          ToonzFolder::getStudioPaletteFolder().getQString().append(
              "\\Global Palettes\\Default "
              "Palettes\\Toonz_Vector_Palette.tpl"));
      if (TSystem::doesExistFileOrLevel(globalPath)) {
        TIStream is(globalPath);
        is >> palette;
        TSystem::copyFile(fullPath, globalPath);
      }
    }
    break;
  case OVL_XSHLEVEL:
    palette = FullColorPalette::instance()->getPalette(getScene());
  default:
    break;
  }

  if (palette && type != OVL_XSHLEVEL) {
    palette->setPaletteName(getName());
  }

  palette->setDirtyFlag(true);
  setPalette(palette);
}

//-----------------------------------------------------------------------------

void TXshSimpleLevel::initializeResolutionAndDpi(const TDimension& dim,
                                                 double dpi) {
  assert(getScene());
  if (getProperties()->getImageRes() != TDimension() &&
      getProperties()->getDpi() != TPointD())
    return;

  double dpiY = dpi;
  getProperties()->setDpiPolicy(LevelProperties::DP_ImageDpi);
  if (dim == TDimension()) {
    double w = 0, h = 0;
    Preferences* pref = Preferences::instance();
    if (pref->isNewLevelSizeToCameraSizeEnabled()) {
      TDimensionD camSize = getScene()->getCurrentCamera()->getSize();
      w                   = camSize.lx;
      h                   = camSize.ly;
      getProperties()->setDpiPolicy(LevelProperties::DP_CustomDpi);
      dpi  = getScene()->getCurrentCamera()->getDpi().x;
      dpiY = getScene()->getCurrentCamera()->getDpi().y;
    } else {
      w    = pref->getDefLevelWidth();
      h    = pref->getDefLevelHeight();
      dpi  = pref->getDefLevelDpi();
      dpiY = dpi;
    }

    getProperties()->setImageRes(TDimension(tround(w * dpi), tround(h * dpiY)));
  } else {
    getProperties()->setImageRes(dim);
  }

  getProperties()->setImageDpi(TPointD(dpi, dpiY));
  getProperties()->setDpi(dpi);
}

//-----------------------------------------------------------------------------

TImageP TXshSimpleLevel::createEmptyFrame() {
  if (isEmpty()) {
    if (!getPalette()) initializePalette();
    initializeResolutionAndDpi();
  }

  TImageP result;

  switch (m_type) {
  case PLI_XSHLEVEL:
    result = new TVectorImage;
    break;

  case META_XSHLEVEL:
    result = new TMetaImage();
    break;

  case MESH_XSHLEVEL:
    assert(false);
    break;

  default: {
    TPointD dpi    = getProperties()->getImageDpi();
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

TDimension TXshSimpleLevel::getResolution() {
  if (isEmpty() || getType() == PLI_XSHLEVEL) return TDimension();
  return m_properties->getImageRes();
}

//-----------------------------------------------------------------------------

TPointD TXshSimpleLevel::getImageDpi(const TFrameId& fid, int frameStatus) {
  if (isEmpty() || getType() == PLI_XSHLEVEL) return TPointD();

  const TFrameId& theFid =
      (fid == TFrameId::NO_FRAME || !isFid(fid)) ? getFirstFid() : fid;
  const std::string& imageId = getImageId(theFid, frameStatus);

  const TImageInfo* imageInfo =
      ImageManager::instance()->getInfo(imageId, ImageManager::none, 0);

  if (!imageInfo) return TPointD();

  return TPointD(imageInfo->m_dpix, imageInfo->m_dpiy);
}

//-----------------------------------------------------------------------------

int TXshSimpleLevel::getImageSubsampling(const TFrameId& fid) const {
  if (isEmpty() || getType() == PLI_XSHLEVEL) return 1;
  TImageP img = TImageCache::instance()->get(getImageId(fid), false);
  if (!img) return 1;
  if (TRasterImageP ri = img) return ri->getSubsampling();
  if (TToonzImageP ti = img) return ti->getSubsampling();
  return 1;
}

//-----------------------------------------------------------------------------

TPointD TXshSimpleLevel::getDpi(const TFrameId& fid, int frameStatus) {
  TPointD dpi;
  if (m_properties->getDpiPolicy() == LevelProperties::DP_ImageDpi)
    dpi = getImageDpi(fid, frameStatus);
  else
    dpi = m_properties->getDpi();
  return dpi;
}

//-----------------------------------------------------------------------------

void TXshSimpleLevel::renumber(const std::vector<TFrameId>& fids) {
  assert(fids.size() == m_frames.size());
  int n = static_cast<int>(fids.size());

  std::vector<TFrameId> oldFids;
  getFids(oldFids);
  std::map<TFrameId, TFrameId> table;
  std::map<TFrameId, TFrameId> newRenumberTable;

  for (int i = 0; i < n; ++i) {
    TFrameId oldFrameId = oldFids[i];
    TFrameId newFrameId = fids[i];
    table[oldFrameId]   = newFrameId;
    for (const auto& renumber : m_renumberTable) {
      if (renumber.second == oldFrameId) {
        newRenumberTable[renumber.first] = newFrameId;
        break;
      }
    }
  }

  for (const auto& renumber : newRenumberTable) {
    m_renumberTable[renumber.first] = renumber.second;
  }

  m_frames.clear();
  for (int i = 0; i < n; ++i) {
    TFrameId fid(fids[i]);
    assert(m_frames.count(fid) == 0);
    m_frames.insert(fid);
  }

  ImageManager* im = ImageManager::instance();
  TImageCache* ic  = TImageCache::instance();

  std::map<TFrameId, TFrameId>::iterator jt;
  {
    int i = 0;
    for (jt = table.begin(); jt != table.end(); ++jt, ++i) {
      std::string Id = getImageId(jt->first);
      ImageLoader::BuildExtData extData(this, jt->first);
      TImageP img = im->getImage(Id, ImageManager::none, &extData);
      ic->add(getIconId(jt->first), img, false);
      im->rebind(Id, "^" + std::to_string(i));
      ic->remap("^icon:" + std::to_string(i), getIconId(jt->first));
    }

    i = 0;
    for (jt = table.begin(); jt != table.end(); ++jt, ++i) {
      std::string Id = getImageId(jt->second);
      im->rebind("^" + std::to_string(i), Id);
      ic->remap(getIconId(jt->second), "^icon:" + std::to_string(i));
      im->renumber(Id, jt->second);
    }
  }

  if (getType() == PLI_XSHLEVEL) {
    int i = 0;
    for (jt = table.begin(); jt != table.end(); ++jt, ++i) {
      const std::string& id = rasterized(getImageId(jt->first));
      if (im->isBound(id)) im->rebind(id, rasterized("^" + std::to_string(i)));
    }
    i = 0;
    for (jt = table.begin(); jt != table.end(); ++jt, ++i) {
      const std::string& id = rasterized("^" + std::to_string(i));
      if (im->isBound(id)) im->rebind(id, rasterized(getImageId(jt->second)));
    }
  }

  if (getType() & FULLCOLOR_TYPE) {
    int i = 0;
    for (jt = table.begin(); jt != table.end(); ++jt, ++i) {
      const std::string& id = filled(getImageId(jt->first));
      if (im->isBound(id)) im->rebind(id, filled("^" + std::to_string(i)));
    }
    i = 0;
    for (jt = table.begin(); jt != table.end(); ++jt, ++i) {
      const std::string& id = filled("^" + std::to_string(i));
      if (im->isBound(id)) im->rebind(id, filled(getImageId(jt->second)));
    }
  }

  m_properties->setDirtyFlag(true);

  if (getHookSet()) getHookSet()->renumber(table);
}

//-----------------------------------------------------------------------------

void TXshSimpleLevel::copyFiles(const TFilePath& dst, const TFilePath& src) {
  if (dst == src) return;
  TSystem::touchParentDir(dst);
  TSystem::copyFileOrLevel_throw(dst, src);
  if (dst.getType() == "tlv") {
    TFilePath srcPltPath =
        src.getParentDir() + TFilePath(src.getWideName() + L".tpl");
    if (TFileStatus(srcPltPath).doesExist())
      TSystem::copyFile(
          dst.getParentDir() + TFilePath(dst.getWideName() + L".tpl"),
          srcPltPath, true);
  }
  if (dst.getType() == "tzp" || dst.getType() == "tzu") {
    TFilePath srcPltPath =
        src.getParentDir() + TFilePath(src.getWideName() + L".plt");
    if (TFileStatus(srcPltPath).doesExist())
      TSystem::copyFile(
          dst.getParentDir() + TFilePath(dst.getWideName() + L".plt"),
          srcPltPath, true);
  }

  const TFilePath& srcHookFile = TXshSimpleLevel::getExistingHookFile(src);
  if (!srcHookFile.isEmpty()) {
    const TFilePath& dstHookFile = getHookPath(dst);
    TSystem::copyFile(dstHookFile, srcHookFile, true);
  }
  TFilePath files = src.getParentDir() + (src.getName() + "_files");
  if (TFileStatus(files).doesExist() && TFileStatus(files).isDirectory())
    TSystem::copyDir(dst.getParentDir() + (src.getName() + "_files"), files);
}

//-----------------------------------------------------------------------------

void TXshSimpleLevel::renameFiles(const TFilePath& dst, const TFilePath& src) {
  if (dst == src) return;
  TSystem::touchParentDir(dst);
  if (TSystem::doesExistFileOrLevel(dst)) TXshSimpleLevel::removeFiles(dst);
  TSystem::renameFileOrLevel_throw(dst, src);
  if (dst.getType() == "tlv")
    TSystem::renameFile(dst.withType("tpl"), src.withType("tpl"));

  const TFilePath& srcHookFile = TXshSimpleLevel::getExistingHookFile(src);
  if (!srcHookFile.isEmpty()) {
    const TFilePath& dstHookFile = getHookPath(dst);
    TSystem::renameFile(dstHookFile, srcHookFile);
  }

  TFilePath files = src.getParentDir() + (src.getName() + "_files");
  if (TFileStatus(files).doesExist() && TFileStatus(files).isDirectory())
    TSystem::renameFile(dst.getParentDir() + (dst.getName() + "_files"), files);
}

//-----------------------------------------------------------------------------

void TXshSimpleLevel::removeFiles(const TFilePath& fp) {
  TSystem::moveFileOrLevelToRecycleBin(fp);
  if (fp.getType() == "tlv") {
    TFilePath tpl = fp.withType("tpl");
    if (TFileStatus(tpl).doesExist()) TSystem::moveFileToRecycleBin(tpl);
  }

  const QStringList& hookFiles = TXshSimpleLevel::getHookFiles(fp);
  for (const auto& hookFile : hookFiles) {
    TSystem::moveFileToRecycleBin(TFilePath(hookFile.toStdWString()));
  }

  TFilePath files = fp.getParentDir() + (fp.getName() + "_files");
  if (TFileStatus(files).doesExist() && TFileStatus(files).isDirectory())
    TSystem::rmDirTree(files);
}

//-----------------------------------------------------------------------------

void TXshSimpleLevel::getFiles(const TFilePath& fp, TFilePathSet& fpset) {
  if (fp.getType() == "tlv") {
    TFilePath tpl = fp.withType("tpl");
    if (TFileStatus(tpl).doesExist()) fpset.push_back(tpl);
  }

  const TFilePath& hookFile = getExistingHookFile(fp);
  if (!hookFile.isEmpty()) fpset.push_back(hookFile);
}

//-----------------------------------------------------------------------------

void TXshSimpleLevel::setContentHistory(TContentHistory* contentHistory) {
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

TFilePath TXshSimpleLevel::getHookPath(const TFilePath& path) {
  return TFilePath(path.withName(path.getName() + "_hooks").getWideString() +
                   L".xml");
}

//-----------------------------------------------------------------------------

QStringList TXshSimpleLevel::getHookFiles(const TFilePath& decodedLevelPath) {
  const TFilePath& dirPath = decodedLevelPath.getParentDir();
  QDir levelDir(QString::fromStdWString(dirPath.getWideString()));

  QStringList hookFileFilter(
      QString::fromStdWString(decodedLevelPath.getWideName() + L"_hooks*.xml"));

  return levelDir.entryList(hookFileFilter, QDir::Files | QDir::NoDotAndDotDot,
                            QDir::Time);
}

//-----------------------------------------------------------------------------

TFilePath TXshSimpleLevel::getExistingHookFile(
    const TFilePath& decodedLevelPath) {
  static const int pCount                         = 3;
  static const QRegularExpression pattern[pCount] = {
      QRegularExpression(R"(.*\.\.?.+\.xml$)"),  // whatever.(.)ext.xml
      QRegularExpression(R"(.*\.xml$)"),         // whatever.xml
      QRegularExpression(R"(.*\.\.?xml$)")       // whatever.(.)xml
  };

  struct locals {
    static inline int getPattern(const QString& fp) {
      for (int p = 0; p != pCount; ++p) {
        if (pattern[p].match(fp).hasMatch()) return p;
      }
      return -1;
    }
  };

  const QStringList& hookFiles = getHookFiles(decodedLevelPath);
  if (hookFiles.empty()) return TFilePath();

  int fPattern = 0, p = pCount, h = -1;

  for (int f = 0; f < hookFiles.size(); ++f) {
    fPattern = locals::getPattern(hookFiles[f]);
    if (fPattern < p) {
      p = fPattern;
      h = f;
    }
  }

  assert(h >= 0);
  return (h < 0) ? TFilePath()
                 : decodedLevelPath.getParentDir() +
                       TFilePath(hookFiles[h].toStdWString());
}

//-----------------------------------------------------------------------------

TRectD TXshSimpleLevel::getBBox(const TFrameId& fid) const {
  TRectD bbox;
  double dpiX = Stage::inch, dpiY = dpiX;

  switch (getType()) {
  case PLI_XSHLEVEL:
  case MESH_XSHLEVEL: {
    TImageP img = getFrame(fid, false);
    if (!img) return TRectD();

    bbox = img->getBBox();

    if (TMeshImageP mi = img) mi->getDpi(dpiX, dpiY);

    break;
  }

  default: {
    const std::string& imageId = getImageId(fid);

    const TImageInfo* info =
        ImageManager::instance()->getInfo(imageId, ImageManager::none, 0);
    if (!info) return TRectD();

    bbox = TRectD(TPointD(info->m_x0, info->m_y0),
                  TPointD(info->m_x1, info->m_y1)) -
           0.5 * TPointD(info->m_lx, info->m_ly);

    if (info->m_dpix > 0.0 && info->m_dpiy > 0.0)
      dpiX = info->m_dpix, dpiY = info->m_dpiy;

    break;
  }
  }

  return TScale(1.0 / dpiX, 1.0 / dpiY) * bbox;
}

//-----------------------------------------------------------------------------

bool TXshSimpleLevel::isFrameReadOnly(TFrameId fid) {
  if (getType() == OVL_XSHLEVEL || getType() == TZI_XSHLEVEL ||
      getType() == MESH_XSHLEVEL) {
    if (getProperties()->isStopMotionLevel()) return true;
    TFilePath fullPath = getScene()->decodeFilePath(m_path);
    if (fullPath.isUneditable()) return true;
    TFilePath path =
        fullPath.getDots() == ".." ? fullPath.withFrame(fid) : fullPath;
    if (!TSystem::doesExistFileOrLevel(path)) return false;
    TFileStatus fs(path);
    return !fs.isWritable();
  }

  if (m_isReadOnly && !m_editableRange.empty() &&
      m_editableRange.count(fid) != 0)
    return false;

  return m_isReadOnly;
}
