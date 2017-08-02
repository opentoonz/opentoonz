#pragma once

#ifndef T_RASTER_INCLUDED
#define T_RASTER_INCLUDED

#include "tutil.h"
#include "tgeometry.h"
#include "tpixel.h"
#include "tpixelgr.h"
#include "tsmartpointer.h"
#include "tbigmemorymanager.h"

#undef DVAPI
#undef DVVAR
#ifdef TRASTER_EXPORTS
#define DVAPI DV_EXPORT_API
#define DVVAR DV_EXPORT_VAR
#else
#define DVAPI DV_IMPORT_API
#define DVVAR DV_IMPORT_VAR
#endif

//=========================================================

class DVAPI TRasterType {
  int m_id;

  /*
public:
  enum Type {
          None,
          BW,
          WB,
          RGBM32,  // LPIXEL, matte channel considered
          RGBM64,  // SPIXEL, matte channel considered
          CM8,     // color-mapped,  8 bits
          GR8,     // grey tones, 8 bits
          CM16,    // color-mapped, 16 bits
          GR16,    // grey tones, 16 bits
          RGB555,
          RGB565,
          CM24,    // cmapped, 8+8+8 bits (ink, paint, ramp), +8 bits spare
(MSB)
          CM16S12,
          CM16S8   // cmapped, 16 bits, standard SGI 256-color colormap
          // ....
  };
*/

public:
  TRasterType(int id) : m_id(id){};
  int getId() const { return m_id; };
  bool operator==(TRasterType a) { return m_id == a.m_id; };
  bool operator!=(TRasterType a) { return m_id != a.m_id; };
};

//=========================================================

// forward declaration
template <class T>
class TRasterPT;
class TRaster;
typedef TSmartPointerT<TRaster> TRasterP;

//------------------------------------------------------------

//
// Smart Pointer a TRaster
//

#ifdef _WIN32
template class DVAPI TSmartPointerT<TRaster>;
#endif

//=========================================================
/*!This class stores bitmap images. */
class DVAPI TRaster : public TSmartObject {
  DECLARE_CLASS_CODE

protected:
  int m_pixelSize;
  int m_lx, m_ly;
  int m_wrap;
  int m_lockCount;
  TRaster *m_parent;  // nel caso di sotto-raster
  UCHAR *m_buffer;
  bool m_bufferOwner;
  // i costruttori sono qui per centralizzare la gestione della memoria
  // e' comunque impossibile fare new TRaster perche' e' una classe astratta
  // (clone, extract)

  // crea il buffer associato (NON fa addRef())
  TRaster(int lx, int ly, int pixelSize);

  // si attacca ad un buffer pre-esistente (NON fa addRef() - neanche a parent)
  TRaster(int lx, int ly, int pixelSize, int wrap, UCHAR *buffer,
          TRaster *parent, bool bufferOwner = false);

private:
  static TAtomicVar m_totalMemory;
  TThread::Mutex m_mutex;
  // not implemented
  TRaster(const TRaster &);
  TRaster &operator=(const TRaster &);

public:
#ifdef _DEBUG
  bool m_cashed;
  static unsigned long getTotalMemoryInKB();
#endif

  virtual ~TRaster();

  // accessors
  // TRasterType getType() const {return m_type;};
  int getLx() const { return m_lx; };
  int getLy() const { return m_ly; };
  TDimension getSize() const { return TDimension(m_lx, m_ly); };

  //! Returns the length of a row in pixel.
  int getWrap() const { return m_wrap; };  // lunghezza di una riga in pixel

  TPointD getCenterD() const { return TPointD(0.5 * m_lx, 0.5 * m_ly); };
  TPoint getCenter() const { return TPoint(m_lx / 2, m_ly / 2); };
  TRect getBounds() const { return TRect(0, 0, m_lx - 1, m_ly - 1); };
  int getPixelSize() const { return m_pixelSize; };

  int getRowSize() const { return m_pixelSize * m_lx; };
  // in bytes

  // when the bigMemoryManager is active, remapping can change buffers...need to
  // to lock/unlock them o use them.

  void lock() {
    if (!TBigMemoryManager::instance()->isActive()) return;
    TThread::MutexLocker sl(&m_mutex);
    if (m_parent)
      m_parent->lock();
    else
      ++m_lockCount;
  }
  void unlock() {
    if (!TBigMemoryManager::instance()->isActive()) return;
    TThread::MutexLocker sl(&m_mutex);
    if (m_parent)
      m_parent->unlock();
    else {
      assert(m_lockCount > 0);
      --m_lockCount;
    }
  }
  void beginRemapping();
  void endRemapping();
  //! Returns a pointer to the image buffer.
  // WARNING!!!!! before getting the buffer with getRawData(),
  // you have to lock the raster with'lock method, and unlock
  // it when you've done with the buffer
  const UCHAR *getRawData() const { return m_buffer; };

  UCHAR *getRawData() { return m_buffer; };
  //! Returns a pointer to the image buffer positioned in the (x,y) coords.
  const UCHAR *getRawData(int x, int y) const {
    assert(0 <= x && x < m_lx && 0 <= y && y < m_ly);
    return m_buffer + (y * m_wrap + x) * m_pixelSize;
  };
  UCHAR *getRawData(int x, int y) {
    assert(0 <= x && x < m_lx && 0 <= y && y < m_ly);
    return m_buffer + (y * m_wrap + x) * m_pixelSize;
  };
  bool isEmpty() const { return getSize() == TDimension(); };

  TRasterP getParent() { return m_parent; }
  // creazione di TRaster derivati

  // devono essere virtuali puri perche' il nuovo raster creato deve essere del
  // tipo giusto
  virtual TRasterP clone() const        = 0;
  virtual TRasterP extract(TRect &rect) = 0;
  virtual TRasterP create() const       = 0;
  virtual TRasterP create(int lx, int ly) const = 0;

  // definita in termini di extract(rect); non lo posso fare subito perche'
  // manca il
  // costruttore di copia di TRasterP
  inline virtual TRasterP extract(int x0, int y0, int x1, int y1);

  // operazioni sui pixel

  // Copia il contenuto di src (spostato di offset) nel raster corrente.
  // Il tipo deve essere lo stesso
  // In caso di dimensione diversa l'area copiata e' l'intersezione dei due
  // getBounds()
  // e i due raster sono allineati in basso a sinistra (src[0,0] -> dst[offset])
  /*!Copies the content of the source raster in the current raster.
*/
  void copy(const TRasterP &src, const TPoint &offset = TPoint());

  void xMirror();
  void yMirror();
  void rotate180();
  void rotate90();

  void clear();
  void clearOutside(const TRect &rect);

  friend class TBigMemoryManager;

protected:
  void fillRawData(const UCHAR *pixel);
  void fillRawDataOutside(const TRect &rect, const UCHAR *pixel);

private:
  void remap(UCHAR *newLocation);
};

//------------------------------------------------------------

inline void detach(TRasterP &r) {
  if (!r || r->getRefCount() == 1) return;
  TRasterP tmp(r->clone());
  r = tmp;
}

//=========================================================

// forward declaration
template <class T>
class TRasterT;

//
// TRasterPT<Pixel>:
//
// Smart Pointer to TRasterT<Pixel>
//

//!\include raster_ex1.cpp
//! \include rasterpt_ex1.cpp
// class TRasterPT<T>
template <class T>
class TRasterPT final : public TSmartPointerT<TRasterT<T>> {
public:
  typedef T Pixel;
  typedef TRasterT<T> Raster;

  TRasterPT(){};

  TRasterPT(int lx, int ly) { create(lx, ly); };
  TRasterPT(const TDimension &d) { create(d.lx, d.ly); };

  inline TRasterPT(const TRasterP &src);
  inline TRasterPT(int lx, int ly, int wrap, T *buffer,
                   bool bufferOwner = false);

  ~TRasterPT(){};

  void create(int lx, int ly);
  void create(const TDimension &d) { create(d.lx, d.ly); };

  inline void detach();

  operator TRasterP() const;
};

//=========================================================

//
// TRasterT<Pixel>
//
// e' la classe concreta che discende da TRaster

template <class T>
class TRasterT : public TRaster {
protected:
  // Constructors are protected to prevent direct allocation of TRasterT
  // instances.
  // Users must adopt the TRasterPT smart pointer syntax instead.

  // Buffer Allocation
  TRasterT(int lx, int ly) : TRaster(lx, ly, sizeof(T)) {}

  // Buffer Attachment
  TRasterT(int lx, int ly, int wrap, T *buffer, TRasterT<T> *parent,
           bool bufferOwner = false)
      : TRaster(lx, ly, sizeof(T), wrap, reinterpret_cast<UCHAR *>(buffer),
                parent, bufferOwner) {}

public:
  typedef T Pixel;

  ~TRasterT(){};

  // accessors
  // WARNING!!!!! before getting the buffer with pixels(int y),
  // you have to lock the raster with'lock method, and unlock
  // it when you've done with the buffer

  const T *pixels(int y = 0) const {
    assert(0 <= y && y < getLy());
    return reinterpret_cast<T *>(m_buffer) + getWrap() * y;
  };
  T *pixels(int y = 0) {
    assert(0 <= y && y < getLy());
    return reinterpret_cast<T *>(m_buffer) + getWrap() * y;
  };

  // Derived rasters creation

  TRasterP clone() const override {
    TRasterP dst = TRasterPT<T>(m_lx, m_ly);
    TRasterP src(const_cast<TRaster *>((const TRaster *)this));
    dst->copy(src);
    return dst;
  }

  TRasterP create() const override { return TRasterPT<T>(m_lx, m_ly); }

  TRasterP create(int lx, int ly) const override {
    return TRasterPT<T>(lx, ly);
  }

  //!\include raster_ex2.cpp
  TRasterP extract(int x0, int y0, int x1, int y1) override {
    TRect rect(x0, y0, x1, y1);
    return extract(rect);
  };

  TRasterP extract(TRect &rect) override {
    if (isEmpty() || getBounds().overlaps(rect) == false) return TRasterP();
    rect = getBounds() * rect;
    // addRef();
    return TRasterP(new TRasterT<T>(rect.getLx(), rect.getLy(), m_wrap,
                                    pixels(rect.y0) + rect.x0, this));
  };

  TRasterPT<T> extractT(TRect &rect);

  TRasterPT<T> extractT(int x0, int y0, int x1, int y1) {
    TRect rect(x0, y0, x1, y1);
    return extractT(rect);
  };

  friend class TRasterPT<T>;

  // Pixel Operations
  void fill(const T &a) { fillRawData(reinterpret_cast<const UCHAR *>(&a)); }

  void fillOutside(const TRect &rect, const T &a) {
    fillRawDataOutside(rect, reinterpret_cast<const UCHAR *>(&a));
  }
};

//---------------------------------------------------------

inline TRasterP TRaster::extract(int x0, int y0, int x1, int y1) {
  TRect rect(x0, y0, x1, y1);
  return extract(rect);
}

//---------------------------------------------------------

template <class T>
TRasterPT<T> TRasterT<T>::extractT(TRect &rect) {
  if (isEmpty() || getBounds().overlaps(rect) == false) {
    return TRasterPT<T>();
  }
  rect = getBounds() * rect;
  // addRef();
  return TRasterPT<T>(new TRasterT<T>(rect.getLx(), rect.getLy(), m_wrap,
                                      pixels(rect.y0) + rect.x0, this));
}

//=========================================================
//
// metodi inline di TRasterPT
// (n.b. se non si fanno esplicitament "inline" NT si confonde con dll
// exort/import)
//

template <class T>
inline TRasterPT<T>::TRasterPT(const TRasterP &src) {
  TSmartPointerT<TRasterT<T>>::m_pointer =
      dynamic_cast<TRasterT<T> *>(src.getPointer());
  if (TSmartPointerT<TRasterT<T>>::m_pointer)
    TSmartPointerT<TRasterT<T>>::m_pointer->addRef();
}

template <class T>
inline void TRasterPT<T>::create(int lx, int ly) {
  TRasterT<T> *raster = new TRasterT<T>(lx, ly);
  *this               = TRasterPT<T>(raster);
}

template <class T>
inline void TRasterPT<T>::detach() {
  if (!TSmartPointerT<TRasterT<T>>::m_pointer ||
      TSmartPointerT<TRasterT<T>>::m_pointer->getRefCount() == 1)
    return;
  *this = TRasterPT(TSmartPointerT<TRasterT<T>>::m_pointer->clone());
  // uso l'operator di assign per aggiornare correttamente
  // i reference counts del vecchio e del nuovo raster
}

template <class T>
inline TRasterPT<T>::operator TRasterP() const {
  return TRasterP(TSmartPointerT<TRasterT<T>>::m_pointer);
}

//---------------------------------------------------------

template <class T>
inline TRasterPT<T>::TRasterPT(int lx, int ly, int wrap, T *buffer,
                               bool bufferOwner) {
  TSmartPointerT<TRasterT<T>>::m_pointer =
      new TRasterT<T>(lx, ly, wrap, buffer, 0, bufferOwner);
  TSmartPointerT<TRasterT<T>>::m_pointer->addRef();
}

//---------------------------------------------------------

#ifdef _WIN32
// su NT e' necessario per evitare un warning nelle classi
// esportate che si riferiscono a TRaster32P/TRaster64P
// su IRIX non compila perche' non riesce ad instanziare le
// funzioni online (!!!)

template class DVAPI TSmartPointerT<TRasterT<TPixel32>>;
template class DVAPI TRasterPT<TPixel32>;

template class DVAPI TSmartPointerT<TRasterT<TPixel64>>;
template class DVAPI TRasterPT<TPixel64>;

template class DVAPI TSmartPointerT<TRasterT<TPixelGR8>>;
template class DVAPI TRasterPT<TPixelGR8>;

template class DVAPI TSmartPointerT<TRasterT<TPixelGR16>>;
template class DVAPI TRasterPT<TPixelGR16>;

template class DVAPI TSmartPointerT<TRasterT<TPixelGRD>>;
template class DVAPI TRasterPT<TPixelGRD>;

template class DVAPI TSmartPointerT<TRasterT<TPixelCY>>;
template class DVAPI TRasterPT<TPixelCY>;

#endif

typedef TRasterPT<TPixel32> TRaster32P;
typedef TRasterPT<TPixel64> TRaster64P;
typedef TRasterPT<TPixelGR8> TRasterGR8P;
typedef TRasterPT<TPixelGR16> TRasterGR16P;
typedef TRasterPT<TPixelGRD> TRasterGRDP;
typedef TRasterPT<TPixelCY> TRasterYUV422P;

//=========================================================

// functions

// trastercentroid.cpp
TPoint computeCentroid(const TRaster32P &r);

//=========================================================

//=========================================================

#endif  //__T_RASTER_INCLUDED
