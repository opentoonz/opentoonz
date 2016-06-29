#pragma once

#ifndef TLEVEL_INCLUDED
#define TLEVEL_INCLUDED

#include "timage.h"
#include "tfilepath.h"

#undef DVAPI
#undef DVVAR
#ifdef TIMAGE_EXPORTS
#define DVAPI DV_EXPORT_API
#define DVVAR DV_EXPORT_VAR
#else
#define DVAPI DV_IMPORT_API
#define DVVAR DV_IMPORT_VAR
#endif

//-------------------------------------------------------------------

class DVAPI TLevel final : public TSmartObject {
  DECLARE_CLASS_CODE

public:
  typedef std::map<const TFrameId, TImageP> Table;
  typedef Table::iterator Iterator;

private:
  std::string m_name;
  Table *m_table;
  TPalette *m_palette;

public:
  TLevel();
  ~TLevel();

private:
  // not implemented
  TLevel(const TLevel &);
  TLevel &operator=(const TLevel &);

public:
  // nome
  std::string getName() const;
  void setName(std::string name);

  // frames
  int getFrameCount() const { return (int)m_table->size(); };

  const TImageP &frame(const TFrameId fid);
  const TImageP &frame(int f) { return frame(TFrameId(f)); };

  void setFrame(const TFrameId &fid, const TImageP &img);

  // ritorna la posizione (0..getNFrames()-1) del frame f
  // se il frame f non c'e' ritorna -1
  // int getIndex(const TFrameId fid);
  // int getIndex(int f) {return getIndex(TFrameId(f));};

  Iterator begin() { return m_table->begin(); };
  Iterator end() { return m_table->end(); };

  // uh - oh; serve a tinytoonz/filmstrip.
  // PROVVISORIO !!
  Table *getTable() { return m_table; }

  TPalette *getPalette();
  void setPalette(TPalette *);
};

//-------------------------------------------------------------------

#ifdef _WIN32
template class DVAPI TSmartPointerT<TLevel>;
#endif

class DVAPI TLevelP final : public TSmartPointerT<TLevel> {
public:
  TLevelP() : TSmartPointerT<TLevel>(new TLevel) {}
  TLevelP(TLevel *level) : TSmartPointerT<TLevel>(level) {}
};

//-------------------------------------------------------------------

#endif
