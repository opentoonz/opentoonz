

#include "tfiletype.h"
#include "tfilepath.h"
#include <QString>

namespace {

class FileTypeData {
public:
  std::map<std::string, int> m_table;

public:
  FileTypeData() {
    // Base, hard-coded known file types
    m_table["tnz"] = TFileType::TOONZSCENE;
    m_table["tab"] = TFileType::TABSCENE;
  }

  static FileTypeData *instance() {
    static FileTypeData data;
    return &data;
  }
};

}  // namespace

//================================================================================

TFileType::Type TFileType::getInfo(const TFilePath &fp) {
  FileTypeData *data = FileTypeData::instance();
  std::map<std::string, int>::iterator it = data->m_table.find(fp.getType());

  int type = (it == data->m_table.end()) ? TFileType::UNKNOW_FILE : it->second;
  if ((type & TFileType::LEVEL) == 0 && (fp.getDots() == ".."))
    type |= TFileType::LEVEL;

  return (TFileType::Type)type;
}

//--------------------------------------------------------------------------------

TFileType::Type TFileType::getInfoFromExtension(const std::string &extension) {
  FileTypeData *data = FileTypeData::instance();
  std::map<std::string, int>::iterator it = data->m_table.find(extension);
  int type = (it == data->m_table.end()) ? TFileType::UNKNOW_FILE : it->second;
  return (TFileType::Type)type;
}

TFileType::Type TFileType::getInfoFromExtension(const QString &type) {
  return getInfoFromExtension(type.toStdString());
}

//--------------------------------------------------------------------------------

void TFileType::declare(std::string extension, Type type) {
  FileTypeData *data       = FileTypeData::instance();
  data->m_table[extension] = type;
}
