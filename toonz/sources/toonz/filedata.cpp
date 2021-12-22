

#include "filedata.h"
#include "tsystem.h"
#include "tconvert.h"

#include "toonz/namebuilder.h"
#include "toonzqt/dvdialog.h"
#include "toonzqt/gutil.h"

using namespace DVGui;

//=============================================================================
// FileData
//-----------------------------------------------------------------------------

FileData::FileData() {}

//-----------------------------------------------------------------------------

FileData::FileData(const FileData *src) : m_files(src->m_files) {}

//-----------------------------------------------------------------------------

FileData::~FileData() {}

//-----------------------------------------------------------------------------

void FileData::setFiles(std::vector<TFilePath> &files) { m_files = files; }

//-----------------------------------------------------------------------------

void FileData::getFiles(TFilePath folder,
                        std::vector<TFilePath> &newFiles) const {
  int i;
  for (i = 0; i < (int)m_files.size(); i++) {
    TFilePath oldPath = m_files[i];
    // Per ora non permettiamo il copia e incolla delle scene.
    if (oldPath.getType() == "tnz") continue;
    TFilePath path = folder + TFilePath(oldPath.getLevelNameW());

    if (!TSystem::doesExistFileOrLevel(oldPath)) {
      DVGui::warning(
          QObject::tr("It is not possible to find the %1 level.", "FileData")
              .arg(QString::fromStdWString(oldPath.getWideString())));
      return;
    }

    NameBuilder *nameBuilder =
        NameBuilder::getBuilder(::to_wstring(path.getName()));
    std::wstring levelNameOut;
    do
      levelNameOut = nameBuilder->getNext();
    while (TSystem::doesExistFileOrLevel(path.withName(levelNameOut)));

    TFilePath levelPathOut = path.withName(levelNameOut);

    if (TSystem::copyFileOrLevel(levelPathOut, oldPath))
      newFiles.push_back(levelPathOut);
    else {
      QString msg = QObject::tr("There was an error copying %1", "FileData")
                        .arg(toQString(oldPath));
      DVGui::error(msg);
      return;
    }
  }
}
