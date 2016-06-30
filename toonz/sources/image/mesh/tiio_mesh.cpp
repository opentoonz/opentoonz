

// TnzCore includes
#include "tstream.h"
#include "timageinfo.h"
#include "tmeshimage.h"

#include "tiio_mesh.h"

// TODO: Eccezioni. Vedi come funziona in toonzscene.cpp

//********************************************************************************
//    TImageWriterMesh  definition
//********************************************************************************

class TImageWriterMesh final : public TImageWriter {
  TFrameId m_fid;  //!< The frame id

public:
  TImageWriterMesh(const TFilePath &fp, const TFrameId &frameId);
  ~TImageWriterMesh() {}

public:
  void save(const TImageP &) override;

private:
  TImageWriterMesh(const TImageWriterMesh &);
  TImageWriterMesh &operator=(const TImageWriterMesh &src);
};

//--------------------------------------------------------------------------

TImageWriterMesh::TImageWriterMesh(const TFilePath &fp, const TFrameId &frameId)
    : TImageWriter(fp), m_fid(frameId) {}

//--------------------------------------------------------------------------

void TImageWriterMesh::save(const TImageP &img) {
  TFilePath imagePath(this->m_path.withFrame(m_fid));

  TOStream ostream(imagePath, true);  // Use compression

  TMeshImageP mi = img;

  // Save Header
  ostream.openChild("header");
  {
    // Save version
    ostream.openChild("version");
    ostream << 1 << 19;
    ostream.closeChild();

    // Save dpi
    ostream.openChild("dpi");

    double dpiX, dpiY;
    mi->getDpi(dpiX, dpiY);

    ostream << dpiX << dpiY;
    ostream.closeChild();
  }
  ostream.closeChild();

  // Save meshes
  const std::vector<TTextureMeshP> &meshes = mi->meshes();

  int m, mCount = meshes.size();
  for (m = 0; m < mCount; ++m) {
    ostream.openChild("mesh");
    ostream << *meshes[m];
    ostream.closeChild();
  }
}

//********************************************************************************
//    TImageReaderMesh  definition
//********************************************************************************

class TImageReaderMesh final : public TImageReader {
  TFrameId m_fid;             //<! Current frame id
  mutable TImageInfo m_info;  //!< The image's infos

public:
  TImageReaderMesh(const TFilePath &fp, const TFrameId &frameId);
  ~TImageReaderMesh() {}

  const TImageInfo *getImageInfo() const override;
  TImageP load() override;

private:
  //! Reference to level reader
  TLevelReaderMesh *m_lrp;

private:
  void readHeader(TIStream &is) const;

  // not implemented
  TImageReaderMesh(const TImageReaderMesh &);
  TImageReaderMesh &operator=(const TImageReaderMesh &src);
};

//--------------------------------------------------------------------------

TImageReaderMesh::TImageReaderMesh(const TFilePath &fp, const TFrameId &frameId)
    : TImageReader(fp), m_fid(frameId) {}

//--------------------------------------------------------------------------

const TImageInfo *TImageReaderMesh::getImageInfo() const {
  if (!m_info.m_valid) {
    // Load info from file
    TFilePath imagePath(this->m_path.withFrame(m_fid));

    TIStream is(imagePath);
    readHeader(is);
  }

  return &m_info;
}

//--------------------------------------------------------------------------

void TImageReaderMesh::readHeader(TIStream &is) const {
  std::string tagName;

  // Open header tag
  is.openChild(tagName);
  assert(tagName == "header");
  {
    // Read header entries
    while (is.openChild(tagName)) {
      if (tagName == "version") {
        int major, minor;
        is >> major >> minor;
        is.setVersion(VersionNumber(major, minor));

        is.closeChild();
      } else if (tagName == "dpi") {
        is >> m_info.m_dpix >> m_info.m_dpiy;
        assert(m_info.m_dpix > 0.0 && m_info.m_dpiy > 0.0);

        is.closeChild();
      } else
        is.skipCurrentTag();
    }
  }
  is.closeChild();

  m_info.m_valid = true;
}

//--------------------------------------------------------------------------

TImageP TImageReaderMesh::load() {
  TMeshImageP mi(new TMeshImage);

  TFilePath imagePath(this->m_path.withFrame(m_fid));

  TIStream is(imagePath);
  readHeader(is);

  mi->setDpi(m_info.m_dpix, m_info.m_dpiy);

  // Meshes
  std::vector<TTextureMeshP> &meshes = mi->meshes();

  std::string tagName;
  while (is.openChild(tagName)) {
    if (tagName == "mesh") {
      meshes.push_back(new TTextureMesh);
      is >> *meshes.back();
      is.closeChild();
    } else
      is.skipCurrentTag();
  }

  return mi;
}

//********************************************************************************
//    TLevelWriterMesh  implementation
//********************************************************************************

TLevelWriterMesh::TLevelWriterMesh(const TFilePath &path, TPropertyGroup *winfo)
    : TLevelWriter(path, winfo) {}

//--------------------------------------------------------------------------

TLevelWriterMesh::~TLevelWriterMesh() {}

//--------------------------------------------------------------------------

TImageWriterP TLevelWriterMesh::getFrameWriter(TFrameId fid) {
  return new TImageWriterMesh(this->m_path, fid);
}

//********************************************************************************
//    TLevelReaderMesh  implementation
//********************************************************************************

TLevelReaderMesh::TLevelReaderMesh(const TFilePath &path)
    : TLevelReader(path) {}

//--------------------------------------------------------------------------

TLevelReaderMesh::~TLevelReaderMesh() {}

//--------------------------------------------------------------------------

TImageReaderP TLevelReaderMesh::getFrameReader(TFrameId fid) {
  return new TImageReaderMesh(this->m_path, fid);
}
