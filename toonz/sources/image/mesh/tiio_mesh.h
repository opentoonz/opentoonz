#pragma once

#ifndef TIIO_MESH_H
#define TIIO_MESH_H

#include "tlevel_io.h"

//*****************************************************************************************
//    TLevelWriterMesh  declaration
//*****************************************************************************************

class TLevelWriterMesh final : public TLevelWriter {
public:
  TLevelWriterMesh(const TFilePath &path, TPropertyGroup *winfo);
  ~TLevelWriterMesh();

  TImageWriterP getFrameWriter(TFrameId fid) override;

public:
  static TLevelWriter *create(const TFilePath &f, TPropertyGroup *winfo) {
    return new TLevelWriterMesh(f, winfo);
  }

private:
  // not implemented
  TLevelWriterMesh(const TLevelWriterMesh &);
  TLevelWriterMesh &operator=(const TLevelWriterMesh &);
};

//*****************************************************************************************
//    TLevelReaderMesh  declaration
//*****************************************************************************************

class TLevelReaderMesh final : public TLevelReader {
public:
  TLevelReaderMesh(const TFilePath &path);
  ~TLevelReaderMesh();

  TImageReaderP getFrameReader(TFrameId fid) override;

public:
  static TLevelReader *create(const TFilePath &f) {
    return new TLevelReaderMesh(f);
  }

private:
  // not implemented
  TLevelReaderMesh(const TLevelReaderMesh &);
  TLevelReaderMesh &operator=(const TLevelReaderMesh &);
};

#endif /* TIIO_MESH_H */
