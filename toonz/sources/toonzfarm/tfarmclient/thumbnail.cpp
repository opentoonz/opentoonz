

#include "thumbnail.h"
#include "tstream.h"
#include "tvectorrenderdata.h"
#include "tvectorimage.h"
#include "tlevel_io.h"
#include "tpalette.h"
#include "tsystem.h"
#include "tsound_io.h"
#include "trop.h"
#include "tw/mainshell.h"
//#include "zapplication.h"
#include "tw/dragdrop.h"

#include "tw/message.h"
#include "pixmapFiletype.h"
#include "tofflinegl.h"

//============================================================
//
// Thumbnail
//
//============================================================

Thumbnail::Thumbnail(const TDimension &size)
    : m_currentFrameIndex(0)
    , m_iconLoaded(false)
    , m_playing(false)
    , m_size(size)
    , m_raster() {
  m_raster = TRaster32P(m_size);
  m_raster->fill(TPixel32(100, 100, 100));
  m_raster->extractT(m_raster->getBounds().enlarge(-20))
      ->fill(TPixel32(80, 90, 100));
}

//-------------------------------------------------------------------

Thumbnail::~Thumbnail() { clearPointerContainer(m_frames); }

//-------------------------------------------------------------------

void Thumbnail::addFrame(const TFrameId &fid) {
  m_frames.push_back(new Frame(fid));
}

//-------------------------------------------------------------------

bool Thumbnail::gotoFrame(int index) {
  assert(m_playing);
  if (m_currentFrameIndex == index) return true;
  if (index < 0 || index >= (int)m_frames.size()) return false;
  m_currentFrameIndex = index;
  m_raster            = TRaster32P();
  if (!m_frames[index]->m_raster) loadFrame(index);
  m_raster = m_frames[index]->m_raster;
  return true;
}

//-------------------------------------------------------------------

TAffine Thumbnail::getAffine(const TDimension &cameraSize) const {
  double scx = 1 * m_size.lx / (double)cameraSize.lx;
  double scy = 1 * m_size.ly / (double)cameraSize.ly;
  double sc  = tmin(scx, scy);
  double dx  = (m_size.lx - cameraSize.lx * sc) * 0.5;
  double dy  = (m_size.ly - cameraSize.ly * sc) * 0.5;
  return TScale(sc) * TTranslation(0.5 * TPointD(cameraSize.lx, cameraSize.ly) +
                                   TPointD(dx, dy));
}

//============================================================
//
// Pli Thumbnail
//
//============================================================

class PliThumbnail : public FileThumbnail {
  TLevelReaderP m_lr;
  TPaletteP m_palette;

public:
  PliThumbnail(const TDimension &size, const TFilePath &fp);

  void loadIcon();
  void loadFrame(int index);
  void setPlaying(bool on);

  Type getType() const { return LEVEL; };

private:
  // not implemented
  PliThumbnail(const PliThumbnail &);
  PliThumbnail &operator=(const PliThumbnail &);
};

//============================================================

PliThumbnail::PliThumbnail(const TDimension &size, const TFilePath &fp)
    : FileThumbnail(size, fp), m_lr() {}

//-------------------------------------------------------------------

void PliThumbnail::loadIcon() {
  clearPointerContainer(m_frames);
  m_lr          = TLevelReaderP(m_filepath);
  TLevelP level = m_lr->loadInfo();
  m_palette     = level->getPalette();
  for (TLevel::Iterator it = level->begin(); it != level->end(); ++it)
    addFrame(it->first);
  loadFrame(0);
  m_lr         = TLevelReaderP();
  m_raster     = m_frames[0]->m_raster;
  m_iconLoaded = true;
}

//-------------------------------------------------------------------

namespace {
void doRender(const TVectorImageP &vi, const TVectorRenderData &rd,
              const TRaster32P &ras) {
  TOfflineGL *glContext = TOfflineGL::getStock(ras->getSize());
  glContext->getRaster()->copy(ras);
  glContext->draw(vi, rd);
  ras->copy(glContext->getRaster());
}
}
void PliThumbnail::loadFrame(int index) {
  assert(0 <= index && index < (int)m_frames.size());
  if (!m_lr) {
    assert(m_playing);
    m_lr = TLevelReaderP(m_filepath);
    m_lr->loadInfo();
  }
  TVectorImageP vi   = m_lr->getFrameReader(m_frames[index]->m_fid)->load();
  TRaster32P &raster = m_frames[index]->m_raster;
  raster             = TRaster32P(m_size);
  if (vi) {
    raster->fill(TPixel32::White);
    TDimension cameraSize(640, 480);
    const TVectorRenderData rd(getAffine(cameraSize),
                               TRect(),  // raster->getBounds(),
                               m_palette.getPointer(), 0);
    // vi->render(rd,raster);
    doRender(vi, rd, raster);
  } else {
    raster->fill(TPixel32(255, 0, 0));
  }
}

//-------------------------------------------------------------------

void PliThumbnail::setPlaying(bool on) {
  if (m_playing == on) return;
  m_playing = on;
  if (!m_playing) {
    m_lr      = TLevelReaderP();
    m_palette = TPaletteP();
  }
}

//============================================================
//
// Raster Thumbnail
//
//============================================================

class RasterThumbnail : public FileThumbnail {
  TLevelReaderP m_lr;

public:
  RasterThumbnail(const TDimension &size, const TFilePath &fp);

  void loadIcon();
  void loadFrame(int index);
  void setPlaying(bool on);

  Type getType() const { return LEVEL; };

private:
  // not implemented
  RasterThumbnail(const RasterThumbnail &);
  RasterThumbnail &operator=(const RasterThumbnail &);
};

//============================================================

RasterThumbnail::RasterThumbnail(const TDimension &size, const TFilePath &fp)
    : FileThumbnail(size, fp), m_lr() {}

//-------------------------------------------------------------------

void RasterThumbnail::loadIcon() {
  clearPointerContainer(m_frames);
  m_lr = TLevelReaderP(m_filepath);
  try {
    TLevelP level = m_lr->loadInfo();
    if (level && level->getFrameCount() != 0) {
      for (TLevel::Iterator it = level->begin(); it != level->end(); ++it)
        addFrame(it->first);
      loadFrame(0);

      m_raster = m_frames[0]->m_raster;
    } else {
      m_raster = TRaster32P(m_size);
      m_raster->fill(TPixel32::Red);
    }
  } catch (TException &e) {
    TMessage::error(toString(e.getMessage()));

    m_raster = TRaster32P(m_size);
    m_raster->fill(TPixel32::Red);
  }

  m_lr         = TLevelReaderP();
  m_iconLoaded = true;
}

//-------------------------------------------------------------------

void RasterThumbnail::loadFrame(int index) {
  assert(0 <= index && index < (int)m_frames.size());
  if (!m_lr) {
    assert(m_playing);
    m_lr = TLevelReaderP(m_filepath);
    m_lr->loadInfo();
  }
  TRasterImageP ri   = m_lr->getFrameReader(m_frames[index]->m_fid)->load();
  TRaster32P &raster = m_frames[index]->m_raster;
  raster             = TRaster32P(m_size);
  TRaster32P inputRaster;
  if (ri) inputRaster = ri->getRaster();
  if (inputRaster) {
    raster->fill(TPixel32::White);
    double sx = (double)(raster->getLx()) / (double)(inputRaster->getLx());
    double sy = (double)(raster->getLy()) / (double)(inputRaster->getLy());
    double sc = tmin(sx, sy);

    TAffine aff =
        TScale(sc).place(inputRaster->getCenterD(), raster->getCenterD());
    TRop::resample(raster, inputRaster, aff);
  } else {
    raster->fill(TPixel32(255, 0, 0));
  }
}

//-------------------------------------------------------------------

void RasterThumbnail::setPlaying(bool on) {
  if (m_playing == on) return;
  m_playing = on;
  if (!m_playing) {
    m_lr = TLevelReaderP();
  }
}

//============================================================
//
// Soundtrack Thumbnail
//
//============================================================

class SoundtrackThumbnail : public FileThumbnail {
public:
  SoundtrackThumbnail(const TDimension &size, const TFilePath &fp);

  void loadIcon();
  void loadFrame(int index);
  void setPlaying(bool on);

  Type getType() const { return LEVEL; };

private:
  // not implemented
  SoundtrackThumbnail(const SoundtrackThumbnail &);
  SoundtrackThumbnail &operator=(const SoundtrackThumbnail &);
};

//============================================================

SoundtrackThumbnail::SoundtrackThumbnail(const TDimension &size,
                                         const TFilePath &fp)
    : FileThumbnail(size, fp) {}

//-------------------------------------------------------------------

void SoundtrackThumbnail::loadIcon() { m_iconLoaded = true; }

//-------------------------------------------------------------------

void SoundtrackThumbnail::loadFrame(int index) {}

//-------------------------------------------------------------------

void SoundtrackThumbnail::setPlaying(bool on) {
  if (m_playing == on) return;
  m_playing = on;
  /*
if(!m_playing)
{
m_lr = TLevelReaderP();
}
*/
}

//============================================================
//
// Casm Thumbnail
//
//============================================================

class CasmThumbnail : public FileThumbnail {
public:
  CasmThumbnail(const TDimension &size, const TFilePath &fp);

  void loadIcon();
  void loadFrame(int index);
  void setPlaying(bool on);

  Type getType() const { return LEVEL; };

private:
  // not implemented
  CasmThumbnail(const CasmThumbnail &);
  CasmThumbnail &operator=(const CasmThumbnail &);
};

//============================================================

CasmThumbnail::CasmThumbnail(const TDimension &size, const TFilePath &fp)
    : FileThumbnail(size, fp) {}

//-------------------------------------------------------------------

void CasmThumbnail::loadIcon() {
  m_iconLoaded    = true;
  TRaster32P icon = casm_browser_icon;
  if (!icon) return;

  double sx = (double)(m_raster->getLx()) / (double)(icon->getLx());
  double sy = (double)(m_raster->getLy()) / (double)(icon->getLy());
  double sc = tmax(sx, sy);

  TAffine aff = TScale(sc).place(icon->getCenterD(), m_raster->getCenterD());
  TRop::resample(m_raster, icon, aff);
}

//-------------------------------------------------------------------

void CasmThumbnail::loadFrame(int index) {}

//-------------------------------------------------------------------

void CasmThumbnail::setPlaying(bool on) {
  if (m_playing == on) return;
  m_playing = on;
  /*
if(!m_playing)
{
m_lr = TLevelReaderP();
}
*/
}

//============================================================
//
// Setup file Thumbnail
//
//============================================================

class SetupThumbnail : public FileThumbnail {
public:
  SetupThumbnail(const TDimension &size, const TFilePath &fp);

  void loadIcon();
  void loadFrame(int index);
  void setPlaying(bool on);

  Type getType() const { return LEVEL; };

private:
  // not implemented
  SetupThumbnail(const SetupThumbnail &);
  SetupThumbnail &operator=(const SetupThumbnail &);
};

//============================================================

SetupThumbnail::SetupThumbnail(const TDimension &size, const TFilePath &fp)
    : FileThumbnail(size, fp) {}

//-------------------------------------------------------------------

void SetupThumbnail::loadIcon() {
  m_iconLoaded    = true;
  TRaster32P icon = setup_browser_icon;
  if (!icon) return;

  double sx = (double)(m_raster->getLx()) / (double)(icon->getLx());
  double sy = (double)(m_raster->getLy()) / (double)(icon->getLy());
  double sc = tmax(sx, sy);

  TAffine aff = TScale(sc).place(icon->getCenterD(), m_raster->getCenterD());
  TRop::resample(m_raster, icon, aff);
}

//-------------------------------------------------------------------

void SetupThumbnail::loadFrame(int index) {}

//-------------------------------------------------------------------

void SetupThumbnail::setPlaying(bool on) {
  if (m_playing == on) return;
  m_playing = on;
  /*
if(!m_playing)
{
m_lr = TLevelReaderP();
}
*/
}

//============================================================
//
// Txt Thumbnail
//
//============================================================

class TxtThumbnail : public FileThumbnail {
public:
  TxtThumbnail(const TDimension &size, const TFilePath &fp);

  void loadIcon();
  void loadFrame(int index);
  void setPlaying(bool on);

  Type getType() const { return LEVEL; };

private:
  // not implemented
  TxtThumbnail(const TxtThumbnail &);
  TxtThumbnail &operator=(const TxtThumbnail &);
};

//============================================================

TxtThumbnail::TxtThumbnail(const TDimension &size, const TFilePath &fp)
    : FileThumbnail(size, fp) {}

//-------------------------------------------------------------------

void TxtThumbnail::loadIcon() { m_iconLoaded = true; }

//-------------------------------------------------------------------

void TxtThumbnail::loadFrame(int index) {}

//-------------------------------------------------------------------

void TxtThumbnail::setPlaying(bool on) {
  if (m_playing == on) return;
  m_playing = on;
  /*
if(!m_playing)
{
m_lr = TLevelReaderP();
}
*/
}

//============================================================
//
// Script Thumbnail
//
//============================================================

class ScriptThumbnail : public FileThumbnail {
public:
  ScriptThumbnail(const TDimension &size, const TFilePath &fp);

  void loadIcon();
  void loadFrame(int index);
  void setPlaying(bool on);

  Type getType() const { return LEVEL; };

private:
  // not implemented
  ScriptThumbnail(const ScriptThumbnail &);
  ScriptThumbnail &operator=(const ScriptThumbnail &);
};

//============================================================

ScriptThumbnail::ScriptThumbnail(const TDimension &size, const TFilePath &fp)
    : FileThumbnail(size, fp) {}

//-------------------------------------------------------------------

void ScriptThumbnail::loadIcon() {
  m_iconLoaded    = true;
  TRaster32P icon = script_browser_icon;
  if (!icon) return;

  double sx = (double)(m_raster->getLx()) / (double)(icon->getLx());
  double sy = (double)(m_raster->getLy()) / (double)(icon->getLy());
  double sc = tmax(sx, sy);

  TAffine aff = TScale(sc).place(icon->getCenterD(), m_raster->getCenterD());
  TRop::resample(m_raster, icon, aff);
}

//-------------------------------------------------------------------

void ScriptThumbnail::loadFrame(int index) {}

//-------------------------------------------------------------------

void ScriptThumbnail::setPlaying(bool on) {
  if (m_playing == on) return;
  m_playing = on;
  /*
if(!m_playing)
{
m_lr = TLevelReaderP();
}
*/
}

//============================================================
//
// Scene Thumbnail
//
//============================================================

class ZSceneThumbnail : public FileThumbnail {
public:
  ZSceneThumbnail(const TDimension &size, const TFilePath &fp);

  void loadIcon();
  void loadFrame(int index);
  void setPlaying(bool on);

  void setName(string name);

  Type getType() const { return SCENE; };

private:
  // not implemented
  ZSceneThumbnail(const ZSceneThumbnail &);
  ZSceneThumbnail &operator=(const ZSceneThumbnail &);
};

//============================================================

ZSceneThumbnail::ZSceneThumbnail(const TDimension &size, const TFilePath &fp)
    : FileThumbnail(size, fp) {}

//-------------------------------------------------------------------

void ZSceneThumbnail::loadIcon() {
  m_iconLoaded = true;
  clearPointerContainer(m_frames);

  string name        = m_filepath.getName();
  TFilePath dir      = m_filepath.getParentDir();
  TFilePath iconPath = dir + TFilePath(name + "_files") + "icon.bmp";
  if (!TFileStatus(iconPath).isReadable()) return;

  TRasterP ras;
  TImageReader::load(iconPath, ras);
  TRaster32P icon = ras;
  if (!icon) return;

  double sx = (double)(m_raster->getLx()) / (double)(icon->getLx());
  double sy = (double)(m_raster->getLy()) / (double)(icon->getLy());
  double sc = tmax(sx, sy);

  TAffine aff = TScale(sc).place(icon->getCenterD(), m_raster->getCenterD());
  TRop::resample(m_raster, icon, aff);
}

//-------------------------------------------------------------------

void ZSceneThumbnail::loadFrame(int index) {}

//-------------------------------------------------------------------

void ZSceneThumbnail::setPlaying(bool on) {
  if (m_playing == on) return;
  m_playing = on;
  /*
if(on)
{
if(m_frames.empty()) loadXsheet();
}
else
{
m_xsh = TXsheetP();
m_palette = TPaletteP();
}
*/
}

//-------------------------------------------------------------------

void ZSceneThumbnail::setName(string name) {
  assert(!m_playing);

  TFilePath oldPath = m_filepath;
  TFilePath newPath = m_filepath.withName(name);
  if (oldPath == newPath) return;

  string oldName          = m_filepath.getName();
  TFilePath oldRepository = oldPath.getParentDir() + (oldName + "_files");
  TFilePath newRepository = newPath.getParentDir() + (name + "_files");

  TSystem::renameFile(newPath, oldPath);
  TSystem::renameFile(newRepository, oldRepository);

  m_filepath = newPath;
}

//============================================================
//
// File Thumbnail
//
//============================================================

FileThumbnail::FileThumbnail(const TDimension &size, const TFilePath &path)
    : Thumbnail(size), m_filepath(path) {}

//-------------------------------------------------------------------

void FileThumbnail::setName(string name) {
  assert(!m_playing);
  TFilePath fp = m_filepath.withName(name);
  if (fp != m_filepath) {
    TSystem::renameFile(fp, m_filepath);
    m_filepath = fp;
  }
}

//-------------------------------------------------------------------

FileThumbnail *FileThumbnail::create(const TDimension &size,
                                     const TFilePath &path) {
  vector<string> fileTypes;
  vector<string> soundTypes;
  TImageReader::getSupportedFormats(fileTypes);
  TLevelReader::getSupportedFormats(fileTypes);
  TSoundTrackReader::getSupportedFormats(soundTypes);
  string type = path.getType();
  if (type == "pli") return new PliThumbnail(size, path);
  if (type == "casm") return new CasmThumbnail(size, path);
  if (type == "setup") return new SetupThumbnail(size, path);
  if (type == "txt") return new TxtThumbnail(size, path);
  if (type == "bat" || type == "sh" || type == "csh" || type == "tcsh")
    return new ScriptThumbnail(size, path);
  /*
else if (type == ZComp::getApplication()->getSceneFileExt())
return new ZSceneThumbnail(size,path);
*/
  else if (std::find(soundTypes.begin(), soundTypes.end(), type) !=
           soundTypes.end())
    return new SoundtrackThumbnail(size, path);
  else if (std::find(fileTypes.begin(), fileTypes.end(), type) !=
           fileTypes.end())
    return new RasterThumbnail(size, path);
  else
    return 0;
}

//-------------------------------------------------------------------

bool FileThumbnail::startDragDrop() {
  TDropSource dropSource;
  std::vector<std::string> v;
  v.push_back(toString(m_filepath.getWideString()));
  TDataObject data(v);
  dropSource.doDragDrop(data);
  return true;
}

//-------------------------------------------------------------------
