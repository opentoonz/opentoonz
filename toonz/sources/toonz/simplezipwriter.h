#pragma once

#include <QByteArray>
#include <QFile>
#include <QFileInfo>
#include <QString>
#include <QVector>

#include <limits>

// Minimal streaming ZIP writer used for non-destructive scene packaging.
// Entries use the ZIP "store" method so no additional compression library is
// required. The resulting archive is a standard ZIP readable by common tools.
class SimpleZipWriter {
  struct Entry {
    QByteArray name;
    quint32 crc = 0;
    quint32 size = 0;
    quint32 offset = 0;
    bool directory = false;
  };

  QFile m_output;
  QVector<Entry> m_entries;
  QString m_error;
  bool m_closed = false;

  static void append16(QByteArray &data, quint16 value) {
    data.append(char(value & 0xff));
    data.append(char((value >> 8) & 0xff));
  }

  static void append32(QByteArray &data, quint32 value) {
    data.append(char(value & 0xff));
    data.append(char((value >> 8) & 0xff));
    data.append(char((value >> 16) & 0xff));
    data.append(char((value >> 24) & 0xff));
  }

  static quint32 updateCrc32(quint32 crc, const QByteArray &data) {
    crc = ~crc;
    for (unsigned char byte : data) {
      crc ^= byte;
      for (int bit = 0; bit < 8; ++bit)
        crc = (crc >> 1) ^ (0xedb88320u & (0u - (crc & 1u)));
    }
    return ~crc;
  }

  bool writeBytes(const QByteArray &data) {
    if (m_output.write(data) != data.size()) {
      m_error = m_output.errorString();
      return false;
    }
    return true;
  }

  bool beginEntry(Entry &entry) {
    const qint64 offset = m_output.pos();
    if (offset < 0 || offset > std::numeric_limits<quint32>::max()) {
      m_error = QStringLiteral("The ZIP archive exceeds the supported 4 GB size.");
      return false;
    }
    entry.offset = quint32(offset);

    QByteArray header;
    append32(header, 0x04034b50u);
    append16(header, 20);       // version needed
    append16(header, 0x0808);   // UTF-8 name + data descriptor
    append16(header, 0);        // store method
    append16(header, 0);        // time
    append16(header, 0);        // date
    append32(header, 0);        // crc follows in descriptor
    append32(header, 0);        // compressed size
    append32(header, 0);        // uncompressed size
    append16(header, quint16(entry.name.size()));
    append16(header, 0);        // extra length
    header.append(entry.name);
    return writeBytes(header);
  }

  bool finishEntry(const Entry &entry) {
    QByteArray descriptor;
    append32(descriptor, 0x08074b50u);
    append32(descriptor, entry.crc);
    append32(descriptor, entry.size);
    append32(descriptor, entry.size);
    return writeBytes(descriptor);
  }

public:
  explicit SimpleZipWriter(const QString &fileName) : m_output(fileName) {
    if (!m_output.open(QIODevice::WriteOnly | QIODevice::Truncate))
      m_error = m_output.errorString();
  }

  ~SimpleZipWriter() {
    if (m_output.isOpen() && !m_closed) m_output.close();
  }

  bool isOpen() const { return m_output.isOpen(); }
  QString errorString() const { return m_error; }

  bool addDirectory(QString archiveName) {
    if (!archiveName.endsWith('/')) archiveName.append('/');
    Entry entry;
    entry.name = archiveName.toUtf8();
    entry.directory = true;
    if (entry.name.size() > std::numeric_limits<quint16>::max()) {
      m_error = QStringLiteral("A ZIP entry name is too long.");
      return false;
    }
    if (!beginEntry(entry) || !finishEntry(entry)) return false;
    m_entries.append(entry);
    return true;
  }

  bool addFile(const QString &archiveName, const QString &sourceFile) {
    QFile input(sourceFile);
    if (!input.open(QIODevice::ReadOnly)) {
      m_error = input.errorString();
      return false;
    }

    const qint64 fileSize = input.size();
    if (fileSize < 0 || fileSize > std::numeric_limits<quint32>::max()) {
      m_error = QStringLiteral("A file exceeds the supported 4 GB ZIP entry size: %1")
                    .arg(sourceFile);
      return false;
    }

    Entry entry;
    entry.name = archiveName.toUtf8();
    if (entry.name.size() > std::numeric_limits<quint16>::max()) {
      m_error = QStringLiteral("A ZIP entry name is too long.");
      return false;
    }
    if (!beginEntry(entry)) return false;

    while (!input.atEnd()) {
      const QByteArray block = input.read(1024 * 1024);
      if (block.isEmpty() && input.error() != QFile::NoError) {
        m_error = input.errorString();
        return false;
      }
      entry.crc = updateCrc32(entry.crc, block);
      entry.size += quint32(block.size());
      if (!writeBytes(block)) return false;
    }

    if (!finishEntry(entry)) return false;
    m_entries.append(entry);
    return true;
  }

  bool close() {
    if (m_closed) return m_error.isEmpty();
    if (!m_output.isOpen()) return false;

    const qint64 centralOffset64 = m_output.pos();
    if (centralOffset64 < 0 ||
        centralOffset64 > std::numeric_limits<quint32>::max()) {
      m_error = QStringLiteral("The ZIP archive exceeds the supported 4 GB size.");
      return false;
    }
    const quint32 centralOffset = quint32(centralOffset64);

    for (const Entry &entry : m_entries) {
      QByteArray record;
      append32(record, 0x02014b50u);
      append16(record, 20);      // made by
      append16(record, 20);      // version needed
      append16(record, 0x0808);  // UTF-8 + descriptor
      append16(record, 0);       // store method
      append16(record, 0);
      append16(record, 0);
      append32(record, entry.crc);
      append32(record, entry.size);
      append32(record, entry.size);
      append16(record, quint16(entry.name.size()));
      append16(record, 0);       // extra
      append16(record, 0);       // comment
      append16(record, 0);       // disk
      append16(record, 0);       // internal attributes
      append32(record, entry.directory ? 0x10u : 0u);
      append32(record, entry.offset);
      record.append(entry.name);
      if (!writeBytes(record)) return false;
    }

    const qint64 centralEnd64 = m_output.pos();
    const qint64 centralSize64 = centralEnd64 - centralOffset64;
    if (m_entries.size() > std::numeric_limits<quint16>::max() ||
        centralSize64 > std::numeric_limits<quint32>::max()) {
      m_error = QStringLiteral("The ZIP archive has too many entries or is too large.");
      return false;
    }

    QByteArray end;
    append32(end, 0x06054b50u);
    append16(end, 0);
    append16(end, 0);
    append16(end, quint16(m_entries.size()));
    append16(end, quint16(m_entries.size()));
    append32(end, quint32(centralSize64));
    append32(end, centralOffset);
    append16(end, 0);
    if (!writeBytes(end)) return false;

    m_output.close();
    m_closed = true;
    return true;
  }
};
