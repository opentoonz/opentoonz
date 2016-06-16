#pragma once

#ifndef T_THUMBNAIL_INCLUDED
#define T_THUMBNAIL_INCLUDED

#include "tfilepath.h"
#include "traster.h"

class TXshLevel;
class TXsheet;
class TPalette;

//-------------------------------------------------------------------

class Thumbnail {
public:
  class Frame {
  public:
    Frame(const TFrameId &fid) : m_fid(fid) {}
    const TFrameId m_fid;
    TRaster32P m_raster;
  };

  enum Type { LEVEL, SCENE };

protected:
  vector<Frame *> m_frames;

  int m_currentFrameIndex;
  bool m_iconLoaded;
  bool m_playing;

  TDimension m_size;
  TRaster32P m_raster;

public:
  Thumbnail(const TDimension &size);
  virtual ~Thumbnail();

  void addFrame(const TFrameId &fid);

  bool isIconLoaded() const { return m_iconLoaded; }
  bool isPlaying() const { return m_playing; }

  // post: assert(m_iconLoaded); assert(m_raster);
  virtual void loadIcon() = 0;

  // pre: 0<=index && index<m_frames.size()
  // post: assert(m_frames[index]);
  virtual void loadFrame(int index) = 0;

  virtual void setPlaying(bool on) = 0;

  virtual string getName() const { return ""; }
  virtual TFilePath getPath() const { return "__none__"; }

  virtual Type getType() const = 0;

  // pre: assert(!m_playing);
  virtual void setName(string name) {}

  const TRaster32P &getRaster() const { return m_raster; }
  TDimension getSize() const { return m_size; }

  bool nextFrame() { return gotoFrame(m_currentFrameIndex + 1); }
  int getCurrentFrameIndex() { return m_currentFrameIndex; }

  // pre: assert(m_playing)
  bool gotoFrame(int index);

  TAffine getAffine(const TDimension &cameraSize) const;
  virtual bool startDragDrop() { return false; }

private:
  // not implemented
  Thumbnail(const Thumbnail &);
  Thumbnail &operator=(const Thumbnail &);
};

//============================================================

class LevelThumbnail : public Thumbnail {
protected:
  TXshLevel *m_level;

public:
  LevelThumbnail(const TDimension &size, TXshLevel *level);
  ~LevelThumbnail();

  void loadIcon();
  void loadFrame(int index);
  void setPlaying(bool on);

  string getName() const;
  TFilePath getPath() const;

  void setName(string name);

  Type getType() const { return LEVEL; };

private:
  // not implemented
  LevelThumbnail(const LevelThumbnail &);
  LevelThumbnail &operator=(const LevelThumbnail &);
};

//============================================================

class SceneThumbnail : public Thumbnail {
protected:
  string m_name;
  TXsheet *m_xsheet;
  TPalette *m_palette;

public:
  SceneThumbnail(const TDimension &size, TXsheet *xsheet, TPalette *palette);
  ~SceneThumbnail();

  void loadIcon();
  void loadFrame(int index);
  void setPlaying(bool on);

  string getName() const;
  TFilePath getPath() const;

  Type getType() const { return SCENE; };

  void setName(string name);

private:
  // not implemented
  SceneThumbnail(const LevelThumbnail &);
  SceneThumbnail &operator=(const LevelThumbnail &);
};

//============================================================

class FileThumbnail : public Thumbnail {
protected:
  TFilePath m_filepath;

public:
  FileThumbnail(const TDimension &size, const TFilePath &path);

  string getName() const { return m_filepath.getName(); }
  TFilePath getPath() const { return m_filepath; }

  void setName(string name);

  static FileThumbnail *create(const TDimension &size, const TFilePath &path);

  bool startDragDrop();

private:
  // not implemented
  FileThumbnail(const FileThumbnail &);
  FileThumbnail &operator=(const FileThumbnail &);
};

#endif
