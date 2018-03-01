

// TnzCore includes
#include "tsystem.h"
#include "tiio.h"
#include "tcontenthistory.h"
#include "tconvert.h"
#include "tmsgcore.h"

// STD includes
#include <map>

// Qt includes
#include <QDir>

#include "tlevel_io.h"

using namespace std;

DEFINE_CLASS_CODE(TLevelReader, 8)
DEFINE_CLASS_CODE(TLevelWriter, 9)
// DEFINE_CLASS_CODE(TLevelReaderWriter, 25)  //brutto

//-----------------------------------------------------------

typedef std::pair<QString, int> LevelReaderKey;
std::map<LevelReaderKey, TLevelReaderCreateProc *> LevelReaderTable;
std::map<QString, std::pair<TLevelWriterCreateProc *, bool>> LevelWriterTable;
// std::map<std::string, TLevelReaderWriterCreateProc*> LevelReaderWriterTable;

//-----------------------------------------------------------

TLevelReader::TLevelReader(const TFilePath &path)
    : TSmartObject(m_classCode)
    , m_info(0)
    , m_path(path)
    , m_contentHistory(0)
    , m_frameFormat(TFrameId::FOUR_ZEROS) {}

//-----------------------------------------------------------

TLevelReader::~TLevelReader() {
  delete m_contentHistory;
  delete m_info;
}

//-----------------------------------------------------------

TLevelReaderP::TLevelReaderP(const TFilePath &path, int reader) {
  QString extension = QString::fromStdString(toLower(path.getType()));
  LevelReaderKey key(extension, reader);
  std::map<LevelReaderKey, TLevelReaderCreateProc *>::iterator it;
  it = LevelReaderTable.find(key);
  if (it != LevelReaderTable.end()) {
    m_pointer = it->second(path);
    assert(m_pointer);
  } else {
    m_pointer = new TLevelReader(path);
  }
  m_pointer->addRef();
}

//-----------------------------------------------------------

namespace {
bool myLess(const TFilePath &l, const TFilePath &r) {
  return l.getFrame() < r.getFrame();
}
}

//-----------------------------------------------------------

const TImageInfo *TLevelReader::getImageInfo(TFrameId fid) {
  if (m_info)
    return m_info;
  else {
    TImageReaderP frameReader = getFrameReader(fid);
    if (!frameReader) return 0;

    const TImageInfo *fInfo = frameReader->getImageInfo();
    if (!fInfo) return 0;

    m_info = new TImageInfo(*fInfo);
    if (m_info->m_properties)
      m_info->m_properties = m_info->m_properties->clone();

    return m_info;
  }
}

//-----------------------------------------------------------

const TImageInfo *TLevelReader::getImageInfo() {
  if (m_info) return m_info;
  TLevelP level = loadInfo();
  if (level->getFrameCount() == 0) return 0;
  return getImageInfo(level->begin()->first);
}

//-----------------------------------------------------------

TLevelP TLevelReader::loadInfo() {
  TFilePath parentDir = m_path.getParentDir();
  TFilePath levelName(m_path.getLevelName());
  //  cout << "Parent dir = '" << parentDir << "'" << endl;
  //  cout << "Level name = '" << levelName << "'" << endl;
  TFilePathSet files;
  try {
    files = TSystem::readDirectory(parentDir, false, true, true);
  } catch (...) {
    throw TImageException(m_path, "unable to read directory content");
  }
  TLevelP level;
  vector<TFilePath> data;
  for (TFilePathSet::iterator it = files.begin(); it != files.end(); it++) {
    TFilePath ln(it->getLevelName());
    // cout << "try " << *it << "  " << it->getLevelName() <<  endl;
    if (levelName == TFilePath(it->getLevelName())) {
      try {
        level->setFrame(it->getFrame(), TImageP());
        data.push_back(*it);
      } catch (TMalformedFrameException tmfe) {
        // skip frame named incorrectly warning to the user in the message
        // center.
        DVGui::warning(QString::fromStdWString(
            tmfe.getMessage() + L": " +
            QObject::tr("Skipping frame.").toStdWString()));
        continue;
      }
    }
  }
  if (!data.empty()) {
    std::vector<TFilePath>::iterator it =
        std::min_element(data.begin(), data.end(), myLess);
    TFilePath fr = (*it).withoutParentDir().withName("").withType("");
    wstring ws   = fr.getWideString();
    if (ws.length() == 5) {
      if (ws.rfind(L'_') == (int)wstring::npos)
        m_frameFormat = TFrameId::FOUR_ZEROS;
      else
        m_frameFormat = TFrameId::UNDERSCORE_FOUR_ZEROS;
    } else if (ws.rfind(L'0') == 1) {  // leads with any number of zeros
      if (ws.rfind(L'_') == (int)wstring::npos)
        m_frameFormat = TFrameId::CUSTOM_PAD;
      else
        m_frameFormat = TFrameId::UNDERSCORE_CUSTOM_PAD;
    } else {
      if (ws.rfind(L'_') == (int)wstring::npos)
        m_frameFormat = TFrameId::NO_PAD;
      else
        m_frameFormat = TFrameId::UNDERSCORE_NO_PAD;
    }

  } else
    m_frameFormat = TFrameId::FOUR_ZEROS;

  return level;
}

//-----------------------------------------------------------

TImageReaderP TLevelReader::getFrameReader(TFrameId fid) {
  return TImageReaderP(m_path.withFrame(fid, m_frameFormat));
}

//-----------------------------------------------------------

void TLevelReader::getSupportedFormats(QStringList &names) {
  for (std::map<LevelReaderKey, TLevelReaderCreateProc *>::iterator it =
           LevelReaderTable.begin();
       it != LevelReaderTable.end(); ++it) {
    names.push_back(it->first.first);
  }
}

//-----------------------------------------------------------

TSoundTrack *TLevelReader::loadSoundTrack() { return 0; }

//===========================================================

TLevelWriter::TLevelWriter(const TFilePath &path, TPropertyGroup *prop)
    : TSmartObject(m_classCode)
    , m_path(path)
    , m_properties(prop)
    , m_contentHistory(0) {
  string ext              = path.getType();
  if (!prop) m_properties = Tiio::makeWriterProperties(ext);
}

//-----------------------------------------------------------

TLevelWriter::~TLevelWriter() {
  delete m_properties;
  delete m_contentHistory;
}

//-----------------------------------------------------------

TLevelWriterP::TLevelWriterP(const TFilePath &path, TPropertyGroup *winfo) {
  QString type = QString::fromStdString(toLower(path.getType()));
  std::map<QString, std::pair<TLevelWriterCreateProc *, bool>>::iterator it;
  it = LevelWriterTable.find(type);
  if (it != LevelWriterTable.end())
    m_pointer = it->second.first(
        path,
        winfo ? winfo->clone() : Tiio::makeWriterProperties(path.getType()));
  else
    m_pointer = new TLevelWriter(
        path,
        winfo ? winfo->clone() : Tiio::makeWriterProperties(path.getType()));

  assert(m_pointer);
  m_pointer->addRef();
}

//-----------------------------------------------------------

void TLevelWriter::save(const TLevelP &level) {
  for (TLevel::Iterator it = level->begin(); it != level->end(); it++) {
    if (it->second) getFrameWriter(it->first)->save(it->second);
  }
}

//-----------------------------------------------------------

void TLevelWriter::saveSoundTrack(TSoundTrack *) {
  return;
  throw TException("The level format doesn't support soundtracks");
}

//-----------------------------------------------------------

void TLevelWriter::setFrameRate(double fps) { m_frameRate = fps; }

//-----------------------------------------------------------

void TLevelWriter::getSupportedFormats(QStringList &names,
                                       bool onlyRenderFormats) {
  for (std::map<QString, std::pair<TLevelWriterCreateProc *, bool>>::iterator
           it = LevelWriterTable.begin();
       it != LevelWriterTable.end(); ++it) {
    if (!onlyRenderFormats || it->second.second) names.push_back(it->first);
  }
}

//-----------------------------------------------------------

TImageWriterP TLevelWriter::getFrameWriter(TFrameId fid) {
  TImageWriterP iw(m_path.withFrame(fid));
  iw->setProperties(m_properties);
  return iw;
}

//-----------------------------------------------------------

void TLevelWriter::setContentHistory(TContentHistory *contentHistory) {
  if (contentHistory != m_contentHistory) {
    delete m_contentHistory;
    m_contentHistory = contentHistory;
  }
}

//-----------------------------------------------------------

void TLevelWriter::renumberFids(const std::map<TFrameId, TFrameId> &table) {
  typedef std::map<TFrameId, TFrameId> Table;

  struct locals {
    static inline QString qstring(const TFilePath &fp) {
      return QString::fromStdWString(fp.getWideString());
    }
    static inline QString temp(const QString &str) {
      return str + QString("_");
    }
  };

  if (m_path.getDots() == "..") {
    try {
      // Extract all image file paths of the level
      QDir parentDir(
          QString::fromStdWString(m_path.getParentDir().getWideString()));
      parentDir.setFilter(QDir::Files);

      QStringList nameFilters(QString::fromStdWString(m_path.getWideName()) +
                              ".*." + QString::fromStdString(m_path.getType()));
      parentDir.setNameFilters(nameFilters);

      TFilePathSet fpset;
      TSystem::readDirectory(fpset, parentDir, false);  // Could throw

      // Traverse each file, trying to match it with a table entry
      std::vector<QString> storedDstPaths;

      TFilePathSet::iterator st, sEnd(fpset.end());
      for (st = fpset.begin(); st != sEnd; ++st) {
        const QString &src  = locals::qstring(*st);
        const TFrameId &fid = st->getFrame();  // Could throw ! (and I'm quite
                                               // appalled of that  o.o')

        Table::const_iterator dt(table.find(fid));
        if (dt == table.end()) {
          // The frame must be removed
          QFile::remove(src);
        } else {
          if (fid == dt->second) continue;

          // The frame must be renumbered
          const QString &dst = locals::qstring(st->withFrame(dt->second));

          if (!QFile::rename(src, dst)) {
            // Use a temporary file rename to ensure that other frames to be
            // renumbered
            // are not overwritten.
            if (QFile::rename(locals::qstring(*st), locals::temp(dst)))
              storedDstPaths.push_back(dst);

            // If the second rename did not happen, the problem was not on dst,
            // but on src.
            // Alas, it means that rename on source is not possible - skip.
          }
        }
      }

      // At this point, temporaries should be restored to originals. In case the
      // rename of one of those files cannot be finalized, leave the temporary -
      // as
      // it may be impossible to roll back (another frame could have been
      // renumbered
      // to the would-roll-back frame) !

      std::vector<QString>::iterator dt, dEnd(storedDstPaths.end());
      for (dt = storedDstPaths.begin(); dt != dEnd; ++dt)
        QFile::rename(locals::temp(*dt), *dt);
    } catch (...) {
      // Could not read the directory - skip silently
    }
  }
}

//============================================================

void TLevelReader::define(QString extension, int reader,
                          TLevelReaderCreateProc *proc) {
  LevelReaderKey key(extension, reader);
  LevelReaderTable[key] = proc;
  // cout << "LevelReader " << extension << " registred" << endl;
}

//-----------------------------------------------------------

void TLevelWriter::define(QString extension, TLevelWriterCreateProc *proc,
                          bool isRenderFormat) {
  LevelWriterTable[extension] =
      std::pair<TLevelWriterCreateProc *, bool>(proc, isRenderFormat);
  // cout << "LevelWriter " << extension << " registred" << endl;
}
