#pragma once

#ifndef TTILESET_HEADER
#define TTILESET_HEADER

#include "trastercm.h"
#include <QString>

#undef DVAPI
#undef DVVAR
#ifdef TOONZLIB_EXPORTS
#define DVAPI DV_EXPORT_API
#define DVVAR DV_EXPORT_VAR
#else
#define DVAPI DV_IMPORT_API
#define DVVAR DV_IMPORT_VAR
#endif

class DVAPI TTileSet {
public:
  // per adesso, facciamo che comprime sempre,
  // appena costruisce l'oggetto. Poi potremmo dare la scelta,
  // anche per decidere se farlo subito o meno.
  class DVAPI Tile {
    TDimension m_dim;
    int m_pixelSize;

  public:
    TRect m_rasterBounds;

    Tile();
    Tile(const TRasterP &ras, const TPoint &p);
    virtual ~Tile();
    virtual QString id() const = 0;

    virtual Tile *clone() const = 0;

    // expressed in byte
    int getSize() { return m_dim.lx * m_dim.ly * m_pixelSize; }

  private:
    Tile(const Tile &tile);
    Tile &operator=(const Tile &tile);
  };

protected:
  TDimension m_srcImageSize;

  typedef std::vector<Tile *> Tiles;
  Tiles m_tiles;

public:
  TTileSet(const TDimension &dim) : m_srcImageSize(dim) {}
  virtual ~TTileSet();

  int getMemorySize() const;

  void add(Tile *tile);

  // crea un tile estraendo rect*ras->getBounds() da ras.
  // Nota: se rect e' completamente fuori non fa nulla
  // Nota: clona il raster!
  virtual void add(const TRasterP &ras, TRect rect) = 0;

  void getRects(std::vector<TRect> &rects) const;
  TRect getBBox() const;

  int getTileCount() const { return (int)m_tiles.size(); }
  TDimension getSrcImageSize() const { return m_srcImageSize; }

  virtual TTileSet *clone() const = 0;
};

//********************************************************************************

class DVAPI TTileSetCM32 final : public TTileSet {
public:
  // per adesso, facciamo che comprime sempre,
  // appena costruisce l'oggetto. Poi potremmo dare la scelta,
  // anche per decidere se farlo subito o meno.
  class DVAPI Tile final : public TTileSet::Tile {
  public:
    Tile();
    Tile(const TRasterCM32P &ras, const TPoint &p);
    ~Tile();
    QString id() const override {
      return "TileCM" + QString::number((uintptr_t) this);
    }

    Tile *clone() const override;

    void getRaster(TRasterCM32P &ras) const;

  private:
    Tile(const Tile &tile);
    Tile &operator=(const Tile &tile);
  };

public:
  TTileSetCM32(const TDimension &dim) : TTileSet(dim) {}
  ~TTileSetCM32();

  // crea un tile estraendo rect*ras->getBounds() da ras.
  // Nota: se rect e' completamente fuori non fa nulla
  // Nota: clona il raster!
  void add(const TRasterP &ras, TRect rect) override;

  const Tile *getTile(int index) const;
  Tile *editTile(int index) const;

  TTileSetCM32 *clone() const override;
};

//********************************************************************************

class DVAPI TTileSetFullColor final : public TTileSet {
public:
  // per adesso, facciamo che comprime sempre,
  // appena costruisce l'oggetto. Poi potremmo dare la scelta,
  // anche per decidere se farlo subito o meno.
  class DVAPI Tile final : public TTileSet::Tile {
  public:
    Tile();
    Tile(const TRasterP &ras, const TPoint &p);
    ~Tile();
    QString id() const override {
      return "TTileSet32::Tile" + QString::number((uintptr_t) this);
    }

    Tile *clone() const override;

    void getRaster(TRasterP &ras) const;

  private:
    Tile(const Tile &tile);
    Tile &operator=(const Tile &tile);
  };

public:
  TTileSetFullColor(const TDimension &dim) : TTileSet(dim) {}
  ~TTileSetFullColor();

  // crea un tile estraendo rect*ras->getBounds() da ras.
  // Nota: se rect e' completamente fuori non fa nulla
  // Nota: clona il raster!
  void add(const TRasterP &ras, TRect rect) override;

  const Tile *getTile(int index) const;
  Tile *editTile(int index) const;

  TTileSetFullColor *clone() const override;
};

#endif
