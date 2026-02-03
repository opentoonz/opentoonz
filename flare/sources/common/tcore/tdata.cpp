

#include "tdata.h"
#include "tconvert.h"

DEFINE_CLASS_CODE(TData, 16)

TTextData::TTextData(std::string text) : m_text(::to_wstring(text)) {}

TDataP TTextData::clone() const { return new TTextData(m_text); }

TDataP TFilePathListData::clone() const {
  return new TFilePathListData(m_filePaths);
}

TFilePath TFilePathListData::getFilePath(int i) const {
  assert(0 <= i && i < (int)m_filePaths.size());
  return m_filePaths[i];
}
