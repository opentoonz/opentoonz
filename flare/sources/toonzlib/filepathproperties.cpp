#include "toonz/filepathproperties.h"

// TnzCore includes
#include "tstream.h"

FilePathProperties::FilePathProperties()
    : m_useStandard(true)
    , m_acceptNonAlphabetSuffix(false)
    , m_letterCountForSuffix(1) {}

bool FilePathProperties::isDefault() {
  return (m_useStandard == true && m_acceptNonAlphabetSuffix == false &&
          m_letterCountForSuffix == 1);
}

void FilePathProperties::saveData(TOStream& os) const {
  os.child("useStandard") << ((m_useStandard) ? 1 : 0);
  os.child("acceptNonAlphabetSuffix") << ((m_acceptNonAlphabetSuffix) ? 1 : 0);
  os.child("letterCountForSuffix") << m_letterCountForSuffix;
}

// make sure to let TFilePath to know the new properties!
void FilePathProperties::loadData(TIStream& is) {
  int val;
  std::string tagName;
  while (is.matchTag(tagName)) {
    if (tagName == "useStandard") {
      is >> val;
      m_useStandard = (val == 1);
    } else if (tagName == "acceptNonAlphabetSuffix") {
      is >> val;
      m_acceptNonAlphabetSuffix = (val == 1);
    } else if (tagName == "letterCountForSuffix") {
      is >> m_letterCountForSuffix;
    }
    is.closeChild();
  }
}