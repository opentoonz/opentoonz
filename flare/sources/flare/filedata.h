#pragma once

#ifndef FILEDATA_H
#define FILEDATA_H

#include "toonzqt/dvmimedata.h"
#include "tfilepath.h"

//=============================================================================
// FileData
//-----------------------------------------------------------------------------

class FileData final : public DvMimeData {
public:
  std::vector<TFilePath> m_files;

  FileData();
  FileData(const FileData *src);
  ~FileData();

  FileData *clone() const override { return new FileData(this); }

  // data <- files.
  void setFiles(std::vector<TFilePath> &files);

  // data -> files. Create a m_files copy in folder \b folder; get a vector of
  // new files path.
  void getFiles(TFilePath folder, std::vector<TFilePath> &newFiles) const;

  FileData *clone() {
    FileData *data = new FileData(this);
    return data;
  }
};

#endif  // FILEDATA_H
