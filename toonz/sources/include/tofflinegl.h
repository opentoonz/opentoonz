#pragma once

#ifndef TOFFLINEGL_INCLUDED
#define TOFFLINEGL_INCLUDED

#include <memory>

#include "tgl.h"

#undef DVAPI
#undef DVVAR

#ifdef TVRENDER_EXPORTS
#define DVAPI DV_EXPORT_API
#define DVVAR DV_EXPORT_VAR
#else
#define DVAPI DV_IMPORT_API
#define DVVAR DV_IMPORT_VAR
#endif

class TVectorRenderData;
class TVectorImageP;
class TRasterImageP;

//------------------------------------------------------

class DVAPI TGLContextManager {
public:
  virtual void store()   = 0;
  virtual void restore() = 0;
};

//------------------------------------------------------

class DVAPI TOfflineGL {
public:
  class Imp {
  protected:
    int m_lx, m_ly;

  public:
    Imp(int lx, int ly) : m_lx(lx), m_ly(ly) {}
    virtual ~Imp(){};
    virtual void makeCurrent() = 0;
    virtual void doneCurrent() = 0;  // Da implementare in Imp
    virtual void createContext(TDimension rasterSize,
                               std::shared_ptr<Imp> shared) = 0;
    virtual void getRaster(TRaster32P)                      = 0;
    virtual int getLx() const { return m_lx; }
    virtual int getLy() const { return m_ly; }
  };
  typedef std::shared_ptr<Imp> ImpGenerator(const TDimension &dim,
                                            std::shared_ptr<Imp> shared);
  static ImpGenerator *defineImpGenerator(ImpGenerator *impGenerator);

  TOfflineGL(TDimension dim, const TOfflineGL *shared = 0);
  TOfflineGL(const TRaster32P &raster, const TOfflineGL *shared = 0);

  ~TOfflineGL();

  void makeCurrent();
  void doneCurrent();

  static void setContextManager(TGLContextManager *contextManager);

  void initMatrix();

  void clear(TPixel32 color);

  void draw(TVectorImageP image, const TVectorRenderData &rd,
            bool doInitMatrix = false);

  void draw(TRasterImageP image, const TAffine &aff, bool doInitMatrix = false);

  void flush();

  TRaster32P getRaster();  // Ritorna il raster copiandolo dal buffer offline
  void getRaster(TRaster32P raster);
  void getRaster(TRasterP raster);

  int getLx() const;
  int getLy() const;
  TDimension getSize() const { return TDimension(getLx(), getLy()); };
  TRect getBounds() const { return TRect(0, 0, getLx() - 1, getLy() - 1); };

  static TOfflineGL *getStock(TDimension dim);
  // si usa cosi': TOfflineGL *ogl = TOfflineGL::getStock(d);
  // non bisogna liberare ogl
  std::shared_ptr<Imp> m_imp;

private:
private:
  // not implemented
  TOfflineGL(const TOfflineGL &);
  TOfflineGL &operator=(const TOfflineGL &);
};

#endif
