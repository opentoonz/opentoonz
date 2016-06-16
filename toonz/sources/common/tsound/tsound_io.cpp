

#include "tsound_io.h"
#include "tconvert.h"

DEFINE_CLASS_CODE(TSoundTrackReader, 13)
DEFINE_CLASS_CODE(TSoundTrackWriter, 14)

//-----------------------------------------------------------

std::map<QString, TSoundTrackReaderCreateProc *> SoundTrackReaderTable;
std::map<QString, TSoundTrackWriterCreateProc *> SoundTrackWriterTable;

//-----------------------------------------------------------

TSoundTrackReader::TSoundTrackReader(const TFilePath &fp)
    : TSmartObject(m_classCode), m_path(fp) {}

//-----------------------------------------------------------

TSoundTrackReader::~TSoundTrackReader() {}

//===========================================================

TSoundTrackReaderP::TSoundTrackReaderP(const TFilePath &path) {
  QString type = QString::fromStdString(toLower(path.getType()));
  std::map<QString, TSoundTrackReaderCreateProc *>::iterator it;
  it = SoundTrackReaderTable.find(type);
  if (it != SoundTrackReaderTable.end()) {
    m_pointer = it->second(path);
    assert(m_pointer);
    m_pointer->addRef();
  } else {
    m_pointer = 0;
    throw TException(path.getWideString() +
                     L": soundtrack reader not implemented");
  }
}

//===========================================================

TSoundTrackWriter::TSoundTrackWriter(const TFilePath &fp)
    : TSmartObject(m_classCode), m_path(fp) {}

//-----------------------------------------------------------

TSoundTrackWriter::~TSoundTrackWriter() {}

//===========================================================

TSoundTrackWriterP::TSoundTrackWriterP(const TFilePath &path) {
  QString type = QString::fromStdString(toLower(path.getType()));
  std::map<QString, TSoundTrackWriterCreateProc *>::iterator it;
  it = SoundTrackWriterTable.find(type);
  if (it != SoundTrackWriterTable.end()) {
    m_pointer = it->second(path);
    assert(m_pointer);
    m_pointer->addRef();
  } else {
    m_pointer = 0;
    throw TException(path.getWideString() +
                     L"soundtrack writer not implemented");
  }
}

//============================================================
//
// Helper functions statiche
//
//============================================================

bool TSoundTrackReader::load(const TFilePath &path, TSoundTrackP &st) {
  st = TSoundTrackReaderP(path)->load();
  return st;
}

//-----------------------------------------------------------

void TSoundTrackReader::getSupportedFormats(QStringList &names) {
  for (std::map<QString, TSoundTrackReaderCreateProc *>::iterator it =
           SoundTrackReaderTable.begin();
       it != SoundTrackReaderTable.end(); ++it) {
    names.push_back(it->first);
  }
}
//-----------------------------------------------------------

bool TSoundTrackWriter::save(const TFilePath &path, const TSoundTrackP &st) {
  return TSoundTrackWriterP(path)->save(st);
}

//-----------------------------------------------------------

void TSoundTrackWriter::getSupportedFormats(QStringList &names) {
  for (std::map<QString, TSoundTrackWriterCreateProc *>::iterator it =
           SoundTrackWriterTable.begin();
       it != SoundTrackWriterTable.end(); ++it) {
    names.push_back(it->first);
  }
}

//===========================================================
//
// funzioni per la registrazione dei formati (chiamate dal Plugin)
//
//===========================================================

void TSoundTrackReader::define(QString extension,
                               TSoundTrackReaderCreateProc *proc) {
  SoundTrackReaderTable[extension] = proc;
}

//-----------------------------------------------------------

void TSoundTrackWriter::define(QString extension,
                               TSoundTrackWriterCreateProc *proc) {
  SoundTrackWriterTable[extension] = proc;
}
