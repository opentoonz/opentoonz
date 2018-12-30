

// TnzCore includes
#include "tsystem.h"
#include "trasterimage.h"
#include "tiio.h"
#include "timageinfo.h"
#include "tcontenthistory.h"

// TnzLib includes
#include "toonz/txshleveltypes.h"
#include "toonz/imagemanager.h"
#include "toonz/toonzscene.h"
#include "toonz/levelproperties.h"
#include "toonz/preferences.h"
#include "toonz/sceneproperties.h"

#include "toonz/levelupdater.h"

//*****************************************************************************************
//    Local namespace stuff
//*****************************************************************************************

namespace {

inline bool supportsRandomAccess(const TFilePath &fp) {
  const std::string &type = fp.getType();
  return type == "tlv" ||  // TLVs do support random access
         // type == "pli" ||                                       // PLIs... I
         // thought they would - but no :(
         // type == "mov" ||                                       // MOVs are
         // 'on the way' to support it... for now, no
         fp.getDots() == "..";  // Multi-file levels of course do
}

//-----------------------------------------------------------------------------

void enforceBpp(TPropertyGroup *pg, int bpp, bool upgradeOnly) {
  // Most properties have a "Bits Per Pixel" property. Enforce the M there in
  // case.
  TEnumProperty *bppProp = (TEnumProperty *)pg->getProperty("Bits Per Pixel");
  if (bppProp) {
    typedef TEnumProperty::Range Range;
    const Range &range = bppProp->getRange();

    // Retrieve current index
    int idx = bppProp->getIndex();

    // Search for a suitable 32-bit or 64-bit value
    int currentBpp = upgradeOnly ? std::stoi(bppProp->getValueAsString()) : 0;
    int targetBpp = (std::numeric_limits<int>::max)(), targetIdx = -1;

    int i, count = (int)range.size();
    for (i = 0; i < count; ++i) {
      int bppEntry = std::stoi(range[i]);
      if ((bppEntry % bpp == 0) && currentBpp <= bppEntry &&
          bppEntry < targetBpp)
        targetBpp = bppEntry, targetIdx = i;
    }

    if (targetIdx >= 0) bppProp->setIndex(targetIdx);
  }

  // Some properties have an "Alpha Channel" TBoolProperty (PNGs, currently). In
  // case, check that.
  if (bpp % 32 == 0) {
    TBoolProperty *alphaProp =
        (TBoolProperty *)pg->getProperty("Alpha Channel");
    if (alphaProp) alphaProp->setValue(true);
  }
}

}  // namespace

//*****************************************************************************************
//    LevelUpdater implementation
//*****************************************************************************************

LevelUpdater::LevelUpdater()
    : m_pg(0)
    , m_inputLevel(0)
    , m_currIdx(0)
    , m_imageInfo(0)
    , m_usingTemporaryFile(false)
    , m_opened(false) {}

//-----------------------------------------------------------------------------

LevelUpdater::LevelUpdater(TXshSimpleLevel *sl)
    : m_pg(0)
    , m_inputLevel(0)
    , m_imageInfo(0)
    , m_currIdx(0)
    , m_opened(false)
    , m_usingTemporaryFile(false) {
  open(sl);
}

//-----------------------------------------------------------------------------

LevelUpdater::LevelUpdater(const TFilePath &fp, TPropertyGroup *lwProperties)
    : m_pg(0)
    , m_inputLevel(0)
    , m_imageInfo(0)
    , m_currIdx(0)
    , m_opened(false)
    , m_usingTemporaryFile(false) {
  open(fp, lwProperties);
}

//-----------------------------------------------------------------------------

LevelUpdater::~LevelUpdater() {
  // Please, observe that the try-catch below here is NOT OPTIONAL.
  // IT IS AN ERROR TO THROW INSIDE A DESTRUCTOR. EVER.
  // Doing so damages the stack unwinding process - namely, it interferes
  // with the destruction of OTHER objects going out of scope.
  // C++ does NOT react well to that (ie could terminate() the process).

  try {
    close();
  } catch (...) {
  }
}

//-----------------------------------------------------------------------------

void LevelUpdater::reset() {
  m_lw     = TLevelWriterP();
  m_lwPath = TFilePath();

  m_lr         = TLevelReaderP();
  m_inputLevel = TLevelP();
  m_sl         = TXshSimpleLevelP();

  delete m_pg;
  m_pg = 0;

  if (m_imageInfo) {
    delete m_imageInfo->m_properties;
    delete m_imageInfo;
    m_imageInfo = 0;
  }

  m_fids.clear();
  m_currIdx = 0;

  m_usingTemporaryFile = false;
  m_opened             = false;
}

//-----------------------------------------------------------------------------

void LevelUpdater::buildSourceInfo(const TFilePath &fp) {
  try {
    m_lr = TLevelReaderP(fp);
    assert(m_lr);

    m_lr->enableRandomAccessRead(
        true);  // Movie files are intended with a constant fps
                // should be made the default... TODO!
    m_inputLevel = m_lr->loadInfo();

    const TImageInfo *info = m_lr->getImageInfo();
    if (info) {
      m_imageInfo = new TImageInfo(
          *info);  // Clone the info. The originals are owned by the reader.
      if (info->m_properties)
        m_imageInfo->m_properties =
            info->m_properties->clone();  // Same for these (unfortunately,
                                          // TImageInfo is currently
                                          // no more than a struct...)
    }
  } catch (...) {
    // The level exists but could not be read.
    // Allowing write to a surviving temporary in this case...

    m_lr         = TLevelReaderP();
    m_inputLevel = TLevelP(0);

    if (m_imageInfo) {
      delete m_imageInfo->m_properties;
      delete m_imageInfo;
      m_imageInfo = 0;
    }
  }
}

//-----------------------------------------------------------------------------

void LevelUpdater::buildProperties(const TFilePath &fp) {
  // Ensure that at least the default properties for specified fp.getType()
  // exist.
  m_pg = (m_imageInfo && m_imageInfo->m_properties)
             ? m_imageInfo->m_properties->clone()
             : Tiio::makeWriterProperties(fp.getType());

  if (!m_pg) {
    // If no suitable pg could be found, the extension must be wrong. Reset and
    // throw.
    reset();
    throw TException("Unrecognized file format");
  }

  assert(m_pg);
}

//-----------------------------------------------------------------------------

void LevelUpdater::open(const TFilePath &fp, TPropertyGroup *pg) {
  assert(!m_lw);

  // Find out if a corresponding level already exists on disk - in that case,
  // load it
  bool existsLevel = TSystem::doesExistFileOrLevel(fp);
  if (existsLevel)
    buildSourceInfo(fp);  // Could be !m_lr if level could not be read

  // Build Output Properties if needed
  if (pg)
    m_pg = pg->clone();
  else
    buildProperties(fp);  // Throws only if not even the default properties
                          // could be found - ie, bad file type
  try {
    // Decide whether the update procedure requires a temporary file for
    // appending
    m_usingTemporaryFile = existsLevel && !supportsRandomAccess(fp);
    if (m_usingTemporaryFile) {
      // The level requires a temporary to write frames to. Upon closing, the
      // original level
      // is deleted and the temporary takes its place. Note that m_lw takes
      // ownership of the properties group.
      m_lwPath = getNewTemporaryFilePath(fp);
      m_lw     = TLevelWriterP(m_lwPath, m_pg->clone());

      if (m_inputLevel)
        for (TLevel::Iterator it = m_inputLevel->begin();
             it != m_inputLevel->end(); ++it)
          m_fids.push_back(it->first);
    } else {
      m_lr =
          TLevelReaderP();  // Release the reader. This is necessary since the
      m_lw = TLevelWriterP(
          fp, m_pg->clone());  // original file itself will be MODIFIED.
      m_lwPath = fp;
    }
  } catch (...) {
    // In this case, TLevelWriterP(..) failed, that object was never contructed,
    // the assignment m_lw never took place. And m_lw == 0.

    // Reset state and rethrow
    reset();
    throw;
  }

  // In case the writer saves icons inside the output level (TLV case), set the
  // associated icon size now
  TDimension iconSize = Preferences::instance()->getIconSize();
  assert(iconSize.lx > 0 && iconSize.ly > 0);
  m_lw->setIconSize(iconSize);

  m_opened = true;
}

//-----------------------------------------------------------------------------

void LevelUpdater::open(TXshSimpleLevel *sl) {
  assert(!m_lw);

  assert(sl && sl->getScene());
  m_sl = sl;

  const TFilePath &fp = sl->getScene()->decodeFilePath(sl->getPath());

  // Find out if a corresponding level already exists on disk - in that case,
  // load it
  bool existsLevel = TSystem::doesExistFileOrLevel(fp);
  if (existsLevel)
    buildSourceInfo(fp);  // Could be !m_lr if level could not be read

  // Build Output Properties
  buildProperties(fp);  // May throw if not even the default properties could be
                        // retrieved

  // If there was no level on disk, or the level properties require the alpha
  // channel, enforce the
  // bpp accordingly on m_pg.
  LevelProperties *levelProperties = sl->getProperties();
  assert(levelProperties);

  if (levelProperties->hasAlpha() || !existsLevel) {
    int bpp = levelProperties->hasAlpha()
                  ? std::min(32, levelProperties->getBpp())
                  : levelProperties->getBpp();
    enforceBpp(m_pg, bpp, existsLevel);
  }

  // Should sl->getPalette() be enforced on m_lw too? It was not present in the
  // old code...

  try {
    // Decide whether the update procedure requires a temporary file for
    // appending
    m_usingTemporaryFile = existsLevel && !supportsRandomAccess(fp);
    if (m_usingTemporaryFile) {
      // The level requires a temporary to write frames to. Upon closing, the
      // original level
      // is deleted and the temporary takes its place.
      m_lwPath = getNewTemporaryFilePath(fp);
      m_lw     = TLevelWriterP(m_lwPath, m_pg->clone());
    } else {
      m_lr = TLevelReaderP();                   // Release the reader
      m_lw = TLevelWriterP(fp, m_pg->clone());  // Open for write the usual way
      m_lwPath = fp;
    }
  } catch (...) {
    // Reset state and rethrow
    reset();
    throw;
  }

  // Load the frames directly from sl
  sl->getFids(m_fids);

  // In case the writer saves icons inside the output level (TLV case), set the
  // associated icon size now
  TDimension iconSize = Preferences::instance()->getIconSize();
  assert(iconSize.lx > 0 && iconSize.ly > 0);
  m_lw->setIconSize(iconSize);

  if (sl->getContentHistory())
    m_lw->setContentHistory(m_sl->getContentHistory()->clone());

  m_opened = true;
}

//-----------------------------------------------------------------------------

TFilePath LevelUpdater::getNewTemporaryFilePath(const TFilePath &fp) {
  TFilePath fp2;
  int count = 1;

  for (;;) {
    // changed the temporary name as the previous naming (like
    // "filename__1.png") had been misteken as sequential images
    fp2 = fp.withName(fp.getWideName() + L"_ottmp" + std::to_wstring(count++));
    if (!TSystem::doesExistFileOrLevel(fp2)) break;
  }

  return fp2;
}

//-----------------------------------------------------------------------------

void LevelUpdater::addFramesTo(int endIdx) {
  if (m_sl) {
    // The simple level case can be optimized since some level's images could
    // already be present
    // in memory. Images are accessed through the level itself.

    for (; m_currIdx < endIdx; ++m_currIdx) {
      TImageP img = m_sl->getFullsampledFrame(m_fids[m_currIdx],
                                              ImageManager::dontPutInCache);
      assert(img);

      if (!img && m_lr) {
        // This should actually never happen. ImageManager should already ensure
        // that img exists.
        // However, as last resort let's just look at the file too...
        img = m_lr->getFrameReader(m_fids[m_currIdx])->load();
        if (img) img->setPalette(m_sl->getPalette());
      }

      if (img) m_lw->getFrameWriter(m_fids[m_currIdx])->save(img);
    }
  } else if (m_lr) {
    // Otherwise, just look in the file directly
    for (; m_currIdx < endIdx; ++m_currIdx) {
      TImageP img = m_lr->getFrameReader(m_fids[m_currIdx])->load();

      if (img) m_lw->getFrameWriter(m_fids[m_currIdx])->save(img);
    }
  }
}

//-----------------------------------------------------------------------------

void LevelUpdater::update(const TFrameId &fid, const TImageP &img) {
  // Resume open for write
  resume();

  if (!m_usingTemporaryFile) {
    // Plain random access write if supported
    m_lw->getFrameWriter(fid)->save(img);
    return;
  }

  // Otherwise, we must add every frame preceding fid, and *then* add img.
  // NOTE: This requires that the image sequence is already sorted by fid.
  addFramesTo(std::lower_bound(m_fids.begin() + m_currIdx, m_fids.end(), fid) -
              m_fids.begin());

  // Save the passed image. In case it overwrites a frame, erase that from the
  // list too.
  m_lw->getFrameWriter(fid)->save(img);
  if (m_currIdx < int(m_fids.size()) && m_fids[m_currIdx] == fid) ++m_currIdx;
}

//-----------------------------------------------------------------------------

void LevelUpdater::close() {
  if (!m_opened) return;

  // Resume open for write
  resume();

  try {
    if (m_usingTemporaryFile) {
      // Add all remaining frames still in m_fids
      addFramesTo((int)m_fids.size());

      // Currently written level is temporary. It must be renamed to its
      // originally intended path,
      // if it's possible to write there. Now, if it's writable, in particular
      // it should be readable,
      // so m_lr should exist.

      // If not... well, the file was corrupt or something. Instead than
      // attempting to delete it,
      // we're begin conservative - this means that no data is lost, but
      // unfortunately temporaries
      // might pile up...
      if (m_lr) {
        TFilePath finalPath(m_lr->getFilePath()), tempPath(m_lw->getFilePath());

        // Release m_lr and m_lw - to be sure that no file is kept open while
        // renaming.
        // NOTE: releasing m_lr and m_lw should not throw anything. As stated
        // before, throwing
        //       in destructors is bad. I'm not sure this is actually guaranteed
        //       in Toonz, however :(
        m_lr = TLevelReaderP(), m_lw = TLevelWriterP();

        // Rename the level
        TSystem::removeFileOrLevel_throw(finalPath);
        TSystem::renameFileOrLevel_throw(finalPath,
                                         tempPath);  // finalPath <- tempPath

        // If present, add known trailing files
        if (finalPath.getType() == "tlv") {
          // Palette file
          TFilePath finalPalette = finalPath.withType("tpl");
          TFilePath tempPalette  = tempPath.withType("tpl");

          if (TFileStatus(finalPalette).doesExist()) {
            if (TFileStatus(tempPalette).doesExist())
              TSystem::deleteFile(finalPalette);
            TSystem::renameFile(finalPalette, tempPalette);
          }

          // History file
          TFilePath finalHistory = finalPath.withType("hst");
          TFilePath tempHistory  = tempPath.withType("hst");

          if (TFileStatus(tempHistory).doesExist()) {
            if (TFileStatus(finalHistory).doesExist())
              TSystem::deleteFile(finalHistory);
            TSystem::renameFile(finalHistory, tempHistory);
          }
        }
      }

      // NOTE: If for some reason m_lr was not present and we were using a
      // temporary file, no
      // renaming takes place. Users could see the __x temporaries and,
      // eventually, rename them manually
      // or see what's wrong with the unwritable file.
    }

    // Reset the updater's status
    reset();
  } catch (...) {
    // Some temporary object could not be renamed. Or some remaining frame could
    // not be added.
    // Hopefully, it was not about closing m_lr or m_lw.

    // However, we still intend to reset the updater's status before rethrowing.
    reset();
    throw;
  }
}

//-----------------------------------------------------------------------------

void LevelUpdater::flush() {
  assert(m_opened);
  if (!m_lw) return;

  // In case the level writer could not be destroyed (bad, should really not
  // throw btw),
  // reset and rethrow
  try {
    m_lw = TLevelWriterP();
  } catch (...) {
    reset();
    throw;
  }
}

//-----------------------------------------------------------------------------

void LevelUpdater::resume() {
  assert(m_opened);
  if (m_lw) return;

  try {
    m_lw = TLevelWriterP(m_lwPath, m_pg->clone());
  } catch (...) {
    reset();
    throw;
  }
}
