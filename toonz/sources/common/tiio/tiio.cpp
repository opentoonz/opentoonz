

#include "tiio.h"

#include "tiio_jpg.h"
#include "tproperty.h"

#include <map>

//--------------------
#include <fcntl.h>
#include <sys/types.h>
#include <sys/stat.h>

#ifdef _MSC_VER
#pragma warning(disable : 4786)
#endif

#ifdef _WIN32
#include <io.h>
#endif

#include <stdio.h>
//--------------------

namespace {

class TiioTable {  // singleton
public:
  typedef std::map<std::string, Tiio::ReaderMaker *> ReaderTable;

  typedef std::map<std::string, std::pair<Tiio::WriterMaker *, bool>>
      WriterTable;

  typedef std::map<std::string, Tiio::VectorReaderMaker *> VectorReaderTable;

  typedef std::map<std::string, std::pair<Tiio::VectorWriterMaker *, bool>>
      VectorWriterTable;

  typedef std::map<std::string, TPropertyGroup *> PropertiesTable;

  ReaderTable m_readers;
  WriterTable m_writers;
  VectorReaderTable m_vectorReaders;
  VectorWriterTable m_vectorWriters;
  PropertiesTable m_writerProperties;

  TiioTable() { initialize(); }

  static TiioTable *instance() {
    static TiioTable theTable;
    return &theTable;
  }
  ~TiioTable() {
    for (PropertiesTable::iterator it = m_writerProperties.begin();
         it != m_writerProperties.end(); ++it)
      delete it->second;
  }

  void add(std::string ext, Tiio::ReaderMaker *fn) { m_readers[ext] = fn; }
  void add(std::string ext, Tiio::WriterMaker *fn, bool isRenderFormat) {
    m_writers[ext] = std::pair<Tiio::WriterMaker *, bool>(fn, isRenderFormat);
  }
  void add(std::string ext, Tiio::VectorReaderMaker *fn) {
    m_vectorReaders[ext] = fn;
  }
  void add(std::string ext, Tiio::VectorWriterMaker *fn, bool isRenderFormat) {
    m_vectorWriters[ext] =
        std::pair<Tiio::VectorWriterMaker *, bool>(fn, isRenderFormat);
  }
  void addWriterProperties(std::string ext, TPropertyGroup *prop) {
    m_writerProperties[ext] = prop;
  }

  Tiio::ReaderMaker *findReader(std::string ext) const {
    ReaderTable::const_iterator it = m_readers.find(ext);
    if (it == m_readers.end())
      return 0;
    else
      return it->second;
  }
  Tiio::WriterMaker *findWriter(std::string ext) const {
    WriterTable::const_iterator it = m_writers.find(ext);
    if (it == m_writers.end())
      return 0;
    else
      return it->second.first;
  }
  Tiio::VectorReaderMaker *findVectorReader(std::string ext) const {
    VectorReaderTable::const_iterator it = m_vectorReaders.find(ext);
    if (it == m_vectorReaders.end())
      return 0;
    else
      return it->second;
  }
  Tiio::VectorWriterMaker *findVectorWriter(std::string ext) const {
    VectorWriterTable::const_iterator it = m_vectorWriters.find(ext);
    if (it == m_vectorWriters.end())
      return 0;
    else
      return it->second.first;
  }
  TPropertyGroup *findWriterProperties(std::string ext) const {
    PropertiesTable::const_iterator it = m_writerProperties.find(ext);
    if (it == m_writerProperties.end())
      return 0;
    else
      return it->second;
  }

  void initialize();
};

}  // namespace

//=========================================================

void TiioTable::initialize() {}

Tiio::Reader *Tiio::makeReader(std::string ext) {
  Tiio::ReaderMaker *reader = TiioTable::instance()->findReader(ext);
  if (!reader)
    return 0;
  else
    return reader();
}

Tiio::Writer *Tiio::makeWriter(std::string ext) {
  Tiio::WriterMaker *writer = TiioTable::instance()->findWriter(ext);
  if (!writer)
    return 0;
  else
    return writer();
}

Tiio::VectorReader *Tiio::makeVectorReader(std::string ext) {
  Tiio::VectorReaderMaker *reader =
      TiioTable::instance()->findVectorReader(ext);
  if (!reader)
    return 0;
  else
    return reader();
}

Tiio::VectorWriter *Tiio::makeVectorWriter(std::string ext) {
  Tiio::VectorWriterMaker *writer =
      TiioTable::instance()->findVectorWriter(ext);
  if (!writer)
    return 0;
  else
    return writer();
}

TPropertyGroup *Tiio::makeWriterProperties(std::string ext) {
  TPropertyGroup *prop = TiioTable::instance()->findWriterProperties(ext);
  if (!prop) return new TPropertyGroup();
  return prop->clone();
}

void Tiio::defineReaderMaker(const char *ext, Tiio::ReaderMaker *fn) {
  TiioTable::instance()->add(ext, fn);
}

void Tiio::defineWriterMaker(const char *ext, Tiio::WriterMaker *fn,
                             bool isRenderFormat) {
  TiioTable::instance()->add(ext, fn, isRenderFormat);
}

void Tiio::defineVectorReaderMaker(const char *ext,
                                   Tiio::VectorReaderMaker *fn) {
  TiioTable::instance()->add(ext, fn);
}

void Tiio::defineVectorWriterMaker(const char *ext, Tiio::VectorWriterMaker *fn,
                                   bool isRenderFormat) {
  TiioTable::instance()->add(ext, fn, isRenderFormat);
}

void Tiio::defineWriterProperties(const char *ext, TPropertyGroup *prop) {
  TiioTable::instance()->addWriterProperties(ext, prop);
}

void Tiio::updateFileWritersPropertiesTranslation() {
  TiioTable::PropertiesTable propTable =
      TiioTable::instance()->m_writerProperties;
  TiioTable::PropertiesTable::const_iterator it;
  for (it = propTable.begin(); it != propTable.end(); ++it)
    it->second->updateTranslation();
}

/*
#ifdef _WIN32
int Tiio::openForReading(char *fn)
{
  int fd = _open(fn, _O_BINARY|_O_RDONLY);
  if(fd == -1)
    {
     fprintf(stderr, "File not found\n");
     exit(1);
    }
  return fd;
}

void* Tiio::openForReading2(char *fn)
{
  FILE *chan = fopen(fn, "rb");
  if(!chan)
    {
     fprintf(stderr, "File not found\n");
     exit(1);
    }
  return (void*)chan;
}


int Tiio::openForWriting(char *fn)
{
  int fd = _open(
      fn,
      _O_BINARY | _O_WRONLY | _O_CREAT | _O_TRUNC,
      _S_IREAD | _S_IWRITE);
  if(fd == -1)
    {
     fprintf(stderr, "Can't open file\n");
     exit(1);
    }
  return fd;
}

#endif
*/

Tiio::Reader::Reader() {}

Tiio::Reader::~Reader() {}

int Tiio::Writer::m_bwThreshold = 128;

Tiio::Writer::Writer() : m_properties(0) {}

Tiio::Writer::~Writer() {}

void Tiio::Writer::getSupportedFormats(QStringList &formats,
                                       bool onlyRenderFormats) {
  TiioTable::VectorWriterTable::const_iterator vit =
      TiioTable::instance()->m_vectorWriters.begin();
  TiioTable::VectorWriterTable::const_iterator vit_e =
      TiioTable::instance()->m_vectorWriters.end();
  for (; vit != vit_e; ++vit)
    if (onlyRenderFormats && vit->second.second)
      formats.push_back(QString::fromStdString(vit->first));
  TiioTable::WriterTable::const_iterator it =
      TiioTable::instance()->m_writers.begin();
  TiioTable::WriterTable::const_iterator it_e =
      TiioTable::instance()->m_writers.end();
  for (; it != it_e; ++it)
    if (onlyRenderFormats && it->second.second)
      formats.push_back(QString::fromStdString(it->first));
}

//-----------------------------------------------------

void Tiio::Writer::setProperties(TPropertyGroup *properties) {
  m_properties = properties ? properties->clone() : 0;
}
