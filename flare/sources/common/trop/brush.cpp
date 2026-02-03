#include <memory>

#include <cstring>
#include "trop.h"

//=======================================================================

class HalfCord {
  std::unique_ptr<int[]> m_array;
  int m_radius;

public:
  HalfCord(int radius) : m_radius(radius), m_array(new int[radius + 1]) {
    assert(radius >= 0);
    memset(m_array.get(), 0, (m_radius + 1) * sizeof(int));

    float dCircle = 1.25f - m_radius;  //  inizializza decision variable
    int y         = m_radius;          //  inizializzazione indice scanline
    int x         = 0;                 //  inizializzazione indice colonna
    do {
      m_array[y] = std::max(x, m_array[y]);
      m_array[x] = y;
      if (dCircle <= 0) {
        dCircle = dCircle + 2 * x + 3;
      } else {
        y--;
        dCircle = dCircle + 2 * (x - y) + 5;
      }
      x++;

    } while (y >= x);
  }

  inline int getCord(int x) {
    assert(0 <= x && x <= m_radius);
    return m_array[x];
  };

private:
  // not implemented
  HalfCord(const HalfCord &);
  HalfCord &operator=(const HalfCord &);
};

//=======================================================================

void TRop::brush(TRaster32P ras, const TPoint &aa, const TPoint &bb, int radius,
                 const TPixel32 &col) {
  TPoint a = aa;
  TPoint b = bb;
  if (a.y > b.y) std::swap(a, b);  //  a e' piu' in basso di b

  int lx = ras->getLx();
  int ly = ras->getLy();
  ras->lock();

  // ----- radius = 0
  if (radius == 0) {
    //  k = +1/-1 se il rettangolo e' inclinato positivamente
    //  (0<=m)/negativamente (m<0)
    //  (se k<0 viene fatta una riflessione sulle ascisse prima di tornare alle
    //  coordinate "di schermo")
    int k  = 1;
    int dy = b.y - a.y;
    int dx = b.x - a.x;
    if (dx < 0) {
      dx = -dx;
      k  = -1;
    }

    assert(dx >= 0);
    assert(dy >= 0);

    double m;  //  m sara' definita solo per dx!=0)
    if (dx > 0) {
      m = dy / (double)dx;
    }
    // double length = sqrt(dx*dx + dy*dy);
    const int alpha = dy, beta = -dx;
    const int incE  = alpha;
    const int incNE = alpha + beta;
    const int incN  = beta;

    //  N.B. le coordinate sono relative ad un sist. di rif. con l'origine in a
    //  l'eq. della retta e' alpha * x + beta * y = 0

    int yMin = std::max(a.y, 0) - a.y;  //  clipping y + cambio  riferimento
    int yMax = std::min(b.y, ly - 1) - a.y;  //  (trasporto dell'origine in a)
    if (dx > 0 && m <= 1) {
      //  midpoint algorithm
      TPoint segm;
      if (dy == 0)  //  segmento orizzontale: inizializza segm
      {
        segm.x = 0;
        segm.y = yMin;
      } else  //  0<m<=1 :  inizializza segm
      {
        segm.x = tceil((yMin - 0.5) / m);
        segm.y = yMin;
      }

      int dSegm = tfloor(alpha * (segm.x + 1) + beta * (segm.y + 0.5));
      while (segm.y <= yMax) {
        int count =
            0;  //  i trati orizzontali di segm vengono disegnati in "blocco"
        while (dSegm < 0 && segm.x <= dx)  //  Est:  segm.x<=dx evita il ciclo
        {                                  //  infinito quando m=0 (incE=0)
          dSegm = dSegm + incE;
          segm.x++;
          count++;
        }
        //  NordEst
        int xMin, xMax;
        if (k > 0) {
          xMin = std::max(
              {a.x + segm.x - count, a.x, 0});  //  clipping x + ritorno alle
          xMax = std::min(
              {a.x + segm.x, b.x, lx - 1});  //  coordinate "di schermo"

        } else {
          xMin = std::max({a.x - segm.x, a.x - dx,
                           0});  //  clipping x + riflessione + ritorno
          xMax = std::min({a.x - segm.x + count, a.x,
                           lx - 1});  //  alle  coordinate "di schermo"
        }

        TPixel32 *p = ras->pixels(segm.y + a.y) + xMin;
        TPixel32 *q = p + (xMax - xMin);

        while (p <= q) *p++ = col;

        dSegm = dSegm + incNE;
        segm.x++;
        segm.y++;
      }
    } else  //  m>1 oppure segmento verticale
    {
      //  midpoint algorithm
      TPoint segm;
      if (dx == 0)  //  segmento verticale: inizializza segm
      {
        segm.x = 0;
        segm.y = yMin;
      } else  //  m>1 :  inizializza segm
      {
        segm.x = tround(yMin / m);
        segm.y = yMin;
      }

      int dSegm = tfloor(alpha * (segm.x + 0.5) + beta * (segm.y + 1));
      while (segm.y <= yMax) {
        int xMin, xMax;
        if (k > 0) {
          xMin = std::max(a.x + segm.x, 0);       //  clipping x + ritorno alle
          xMax = std::min(a.x + segm.x, lx - 1);  //  coordinate "di schermo"

        } else {
          xMin =
              std::max(a.x - segm.x, 0);  //  clipping x + riflessione + ritorno
          xMax =
              std::min(a.x - segm.x, lx - 1);  //  alle  coordinate "di schermo"
        }

        TPixel32 *p = ras->pixels(segm.y + a.y) + xMin;
        TPixel32 *q = p + (xMax - xMin);

        while (p <= q) *p++ = col;

        if (dSegm <= 0)  //  NordEst
        {
          dSegm = dSegm + incNE;
          segm.x++;
        } else  //  Nord
        {
          dSegm = dSegm + incN;
        }
        segm.y++;
      }
    }
    ras->unlock();
    return;
  }

  HalfCord halfCord(radius);

  int x, y;

  // ----- punti iniziali coincidenti: disegna un cerchio
  if (a == b) {
    int yMin = std::max(a.y - radius, 0);       //  clipping y
    int yMax = std::min(a.y + radius, ly - 1);  //  clipping y
    for (y = yMin; y <= yMax; y++) {
      int deltay = abs(y - a.y);
      int xMin   = std::max(a.x - halfCord.getCord(deltay), 0);  //  clipping x
      int xMax =
          std::min(a.x + halfCord.getCord(deltay), lx - 1);  //  clipping x
      TPixel32 *p         = ras->pixels(y) + xMin;
      TPixel32 *q         = p + (xMax - xMin);
      while (p <= q) *p++ = col;
    }
    ras->unlock();
    return;
  }

  // -----  rettangolo orizzontale (a.y = b.y, a.x != b.x)
  if (a.y == b.y) {
    int yMin   = std::max((a.y - radius), 0);       //  clipping y
    int yMax   = std::min((a.y + radius), ly - 1);  //  clipping y
    int xLeft  = std::min(a.x, b.x);
    int xRight = std::max(a.x, b.x);
    for (y = yMin; y <= yMax; y++) {
      int deltay = abs(y - a.y);
      int xMin = std::max(xLeft - halfCord.getCord(deltay), 0);  //  clipping x
      int xMax =
          std::min(xRight + halfCord.getCord(deltay), lx - 1);  //  clipping x
      TPixel32 *p         = ras->pixels(y) + xMin;
      TPixel32 *q         = p + (xMax - xMin);
      while (p <= q) *p++ = col;
    }
    ras->unlock();
    return;
  }

  // -----  rettangolo verticale (a.x = b.x, a.y != b.y)
  if (a.x == b.x) {
    int xMin = std::max(a.x - radius, 0);       //  clipping x
    int xMax = std::min(a.x + radius, lx - 1);  //  clipping x
    for (x = xMin; x <= xMax; x++) {
      int deltax = abs(x - a.x);
      int yMin   = std::max(a.y - halfCord.getCord(deltax), 0);  //  clipping y
      int yMax =
          std::min(b.y + halfCord.getCord(deltax), ly - 1);  //  clipping y
      if (yMin <= yMax) {
        TPixel32 *p = ras->pixels(yMin) + x;
        TPixel32 *q = ras->pixels(yMax) + x;
        int wrap    = ras->getWrap();
        while (p <= q) {
          *p = col;
          p += wrap;
        }
      }
    }
    ras->unlock();
    return;
  }

  // -----  rettangolo inclinato
  //	k = +1/-1 se il rettangolo e' inclinato positivamente/negativamente
  int k  = 1;
  int dx = b.x - a.x;
  if (dx < 0) {
    dx = -dx;
    k  = -1;
  }
  int dy = b.y - a.y;

  assert(dx > 0);
  assert(dy > 0);

  double length  = sqrt((double)(dx * dx + dy * dy));
  const double m = dy / (double)dx;

  // punto di tangenza superiore nel sistema di riferimento del cerchio
  TPointD up(-radius * dy / length, radius * dx / length);

  // semi-ampiezza orizzontale delle "calotte" circolari
  int halfAmplCap = tfloor(-up.x);

  //  A meno di intersezioni relative tra le diverse zone:

  //  le scanline della "calotta" circolare superiore sono
  //  (b.y+cutExt,b.y+radius]
  //  le scanline del trapezoide circolare superiore sono [b.y-cutIn,b.y+cutExt]
  //  le scanline del parallelogramma sono (a.y+cutIn,b.y-cutIn)
  //  le scanline del trapezoide circolare inferiore sono [a.y-cutExt,a.y+cutIn]
  //  le scanline della "calotta" circolare inferiore sono
  //  [a.y-radius,a.y-cutExt)
  int cutExt, cutIn;

  // vertici del parallelogramma
  TPointD rightUp;
  TPointD rightDown;
  TPointD leftUp;
  TPointD leftDown;
  double mParall;  // coeff. angolare parallelogramma

  //  NOTA BENE:  halfAmplCap=0 <=> (radius=0 (caso a parte) , 1)
  if (radius > 1) {
    for (cutExt = radius;
         cutExt >= 0 && halfCord.getCord(cutExt) <= halfAmplCap; cutExt--)
      ;
    cutIn       = cutExt;  //  vedi else successivo
    rightUp.x   = dx + halfCord.getCord(cutIn);
    rightUp.y   = dy - cutIn;
    rightDown.x = halfCord.getCord(cutIn);
    rightDown.y = -cutIn;
    leftUp.x    = dx - halfCord.getCord(cutIn);
    leftUp.y    = dy + cutIn;
    leftDown.x  = -halfCord.getCord(cutIn);
    leftDown.y  = cutIn;
    mParall     = dy / (double)dx;
  } else  //  N.B. cutExt != cutIn solo quando radius=1
  {
    cutExt = radius;  //  radius=1 => halfAmplCap=0 (non ci sono mai le
                      //  "calotte" circolari)
    cutIn = 0;        //  anche per radius=1 il limite "interno" dei trapezoidi
                      //  circolari e' < radius
    rightUp.x   = dx - up.x;
    rightUp.y   = dy - up.y;
    rightDown.x = -up.x;
    rightDown.y = -up.y;
    leftUp.x    = dx + up.x;
    leftUp.y    = dy + up.y;
    leftDown.x  = up.x;
    leftDown.y  = up.y;
    mParall     = m;
  }
  // -----  riempie "calotte" circolari

  // -----  riempie "calotta" circolare inferiore
  int yMin = std::max(a.y - radius, 0);           //  clipping y
  int yMax = std::min(a.y - cutExt - 1, ly - 1);  //  clipping y
  for (y = yMin; y <= yMax; y++) {
    int r               = halfCord.getCord(a.y - y);
    int xMin            = std::max(a.x - r, 0);       //  clipping x
    int xMax            = std::min(a.x + r, lx - 1);  //  clipping x
    TPixel32 *p         = ras->pixels(y) + xMin;
    TPixel32 *q         = p + (xMax - xMin);
    while (p <= q) *p++ = col;
  }
  // -----  riempie "calotta" circolare superiore
  yMin = std::max(b.y + cutExt + 1, 0);   //  clipping y
  yMax = std::min(b.y + radius, ly - 1);  //  clipping y
  for (y = yMin; y <= yMax; y++) {
    int r               = halfCord.getCord(y - b.y);
    int xMin            = std::max(b.x - r, 0);       //  clipping x
    int xMax            = std::min(b.x + r, lx - 1);  //  clipping x
    TPixel32 *p         = ras->pixels(y) + xMin;
    TPixel32 *q         = p + (xMax - xMin);
    while (p <= q) *p++ = col;
  }
  // -----  riempie trapezoidi

  // (se k<0 viene fatta una riflessione sulle ascisse prima di tornare alle
  // coordinate "di schermo")

  //  limite destro assoluto delle scanline trapezoide:
  int xSegmMax = tround(dx - up.x);  //  coordinata x del punto di tangenza
                                     //  inferiore sul cerchio superiore

  //  limite sinistro assoluto delle scanline:
  int xSegmMin = tround(up.x);  //  coordinata x del punto di tangenza superiore
                                //  sul cerchio inferiore

  // -----  riempie trapezoide inferiore

  // N.B. le coordinate sono relative ad un sist. di rif. con l'origine sul
  // centro
  // del cerchio inferiore

  yMin = std::max(a.y - cutExt, 0) - a.y;                         //  clipping y
  yMax = std::min({a.y + cutIn, b.y - cutIn - 1, ly - 1}) - a.y;  //  clipping y

  // l'eq. della retta e' alpha * x + beta * y + gammaRight = 0
  const int alpha = dy, beta = -dx;
  const double gammaRight = rightDown.y * dx - rightDown.x * dy;
  const int incE          = alpha;
  const int incNE         = alpha + beta;
  const int incN          = beta;

  if (m <= 1) {
    //  midpoint algorithm; le scanline vengono disegnate solo
    //  sul NordEst. L'ultima scanline non viene disegnata
    TPoint segmRight(
        tceil((yMin + 0.5 - rightDown.y) / mParall + rightDown.x) - 1, yMin);
    int dSegmRight = tfloor(alpha * (segmRight.x + 1) +
                            beta * (segmRight.y + 0.5) + gammaRight);
    while (segmRight.y <= yMax) {
      if (dSegmRight < 0)  //  Est
      {
        dSegmRight = dSegmRight + incE;
        segmRight.x++;
      } else  //  NordEst
      {
        int xMin, xMax;
        if (k > 0) {
          xMin = std::max(a.x - halfCord.getCord(abs(segmRight.y)),
                          0);  //  clipping x
          xMax = std::min(a.x + std::min(segmRight.x, xSegmMax),
                          lx - 1);  //  clipping x
        } else {
          xMin = std::max(a.x - std::min(segmRight.x, xSegmMax),
                          0);  //  clipping x + ritorno alle
          xMax = std::min(a.x + halfCord.getCord(abs(segmRight.y)),
                          lx - 1);  //   coordinate "di schermo"
        }
        TPixel32 *p         = ras->pixels(segmRight.y + a.y) + xMin;
        TPixel32 *q         = p + (xMax - xMin);
        while (p <= q) *p++ = col;

        dSegmRight = dSegmRight + incNE;
        segmRight.x++;
        segmRight.y++;
      }
    }
  } else  //  m>1
  {
    //  midpoint algorithm; le scanline vengono disegnate sempre
    TPoint segmRight(tround((yMin - rightDown.y) / mParall + rightDown.x),
                     yMin);
    int dSegmRight = tfloor(alpha * (segmRight.x + 0.5) +
                            beta * (segmRight.y + 1) + gammaRight);
    while (segmRight.y <= yMax) {
      int xMin, xMax;
      if (k > 0) {
        xMin = std::max(a.x - halfCord.getCord(abs(segmRight.y)),
                        0);                          //  clipping x
        xMax = std::min(a.x + segmRight.x, lx - 1);  //  clipping x
      } else {
        xMin = std::max(a.x - segmRight.x,
                        0);  //  clipping x + ritorno alle coordinate
        xMax = std::min(a.x + halfCord.getCord(abs(segmRight.y)),
                        lx - 1);  //  "di schermo"
      }
      TPixel32 *p         = ras->pixels(segmRight.y + a.y) + xMin;
      TPixel32 *q         = p + (xMax - xMin);
      while (p <= q) *p++ = col;

      if (dSegmRight <= 0)  //  NordEst
      {
        dSegmRight = dSegmRight + incNE;
        segmRight.x++;
      } else  //  Nord
      {
        dSegmRight = dSegmRight + incN;
      }
      segmRight.y++;
    }
  }

  // -----  riempie trapezoide superiore

  //  N.B. le coordinate sono relative ad un sist. di rif. con l'origine sul
  //  centro
  //  del cerchio superiore
  yMin = std::max({b.y - cutIn, a.y + cutIn + 1, 0}) - b.y;  //  clipping y
  yMax = std::min(b.y + cutExt, ly - 1) - b.y;               //  clipping y

  //   l'eq. della retta e' alpha * x + beta * y + gammaLeft = 0
  const double gammaLeft = leftDown.y * dx - leftDown.x * dy;

  if (m <= 1) {
    //  midpoint algorithm; le scanline vengono disegnate solo
    //  sul NordEst. L'ultima scanline non viene disegnata
    TPoint segmLeft(tceil((yMin - 0.5 - leftDown.y) / mParall + leftDown.x),
                    yMin);
    int dSegmLeft = tfloor(alpha * (segmLeft.x + 1) +
                           beta * (segmLeft.y + 0.5) + gammaLeft);
    while (segmLeft.y <= yMax) {
      int xMin, xMax;
      if (k > 0) {
        xMin = std::max(b.x + std::max(segmLeft.x, xSegmMin - dx),
                        0);  //  clipping x
        xMax = std::min(b.x + halfCord.getCord(abs(segmLeft.y)),
                        lx - 1);  //  clipping x
      } else {
        xMin = std::max(b.x - halfCord.getCord(abs(segmLeft.y)),
                        0);  //  clipping x + ritorno alle
        xMax = std::min(b.x - std::max(segmLeft.x, xSegmMin - dx),
                        lx - 1);  //   coordinate "di schermo"
      }
      TPixel32 *p = ras->pixels(segmLeft.y + b.y) + xMin;
      TPixel32 *q = p + (xMax - xMin);

      while (p <= q) *p++ = col;
      while (dSegmLeft < 0) {
        dSegmLeft = dSegmLeft + incE;
        segmLeft.x++;
      }
      dSegmLeft = dSegmLeft + incNE;
      segmLeft.x++;
      segmLeft.y++;
    }
  } else  //  m>1
  {
    //  midpoint algorithm; le scanline vengono disegnate sempre
    TPoint segmLeft(tround((yMin - leftDown.y) / mParall + leftDown.x), yMin);
    int dSegmLeft = tfloor(alpha * (segmLeft.x + 0.5) +
                           beta * (segmLeft.y + 1) + gammaLeft);
    while (segmLeft.y <= yMax) {
      int xMin, xMax;
      if (k > 0) {
        xMin = std::max(b.x + segmLeft.x, 0);  //  clipping x
        xMax = std::min(b.x + halfCord.getCord(abs(segmLeft.y)),
                        lx - 1);  //  clipping x
      } else {
        xMin = std::max(b.x - halfCord.getCord(abs(segmLeft.y)),
                        0);  //  clipping x + ritorno alle
        xMax = std::min(b.x - segmLeft.x, lx - 1);  //   coordinate "di schermo"
      }
      TPixel32 *p = ras->pixels(segmLeft.y + b.y) + xMin;
      TPixel32 *q = p + (xMax - xMin);

      while (p <= q) *p++ = col;

      if (dSegmLeft <= 0)  //  NordEst
      {
        dSegmLeft = dSegmLeft + incNE;
        segmLeft.x++;
      } else  //  Nord
      {
        dSegmLeft = dSegmLeft + incN;
      }
      segmLeft.y++;
    }
  }

  // -----  parallelogramma (in alternativa a "parallelogrammoide circolare")

  // N.B. le coordinate sono relative ad un sist. di rif. con l'origine sul
  // centro
  // del cerchio inferiore

  //  retta destra di equaz.   alpha * x + beta * y + gammaRight = 0
  //  retta sinistra di equaz. alpha * x + beta * y + gammaLeft = 0

  yMin = std::max(a.y + cutIn + 1, 0) - a.y;       // clipping y
  yMax = std::min(b.y - cutIn - 1, ly - 1) - a.y;  // clipping y
  if (m <= 1) {
    //  midpoint algorithm; le scanline vengono disegnate solo
    //  sul NordEst. L'ultima scanline non viene disegnata
    TPoint segmRight(
        tceil((yMin + 0.5 - rightDown.y) / mParall + rightDown.x) - 1, yMin);
    TPoint segmLeft =
        TPoint(tceil((yMin - 0.5 - leftDown.y) / mParall + leftDown.x), yMin);
    int dSegmRight = tfloor(alpha * (segmRight.x + 1) +
                            beta * (segmRight.y + 0.5) + gammaRight);
    int dSegmLeft = tfloor(alpha * (segmLeft.x + 1) +
                           beta * (segmLeft.y + 0.5) + gammaLeft);
    while (segmRight.y <= yMax) {
      if (dSegmRight < 0)  //  segmRight a Est
      {
        dSegmRight = dSegmRight + incE;
        segmRight.x++;
      } else  //  segmRight a NordEst
      {
        int xMin, xMax;
        if (k > 0) {
          xMin =
              std::max(a.x + std::max(segmLeft.x, xSegmMin), 0);  //  clipping x
          xMax = std::min(a.x + std::min(segmRight.x, xSegmMax),
                          lx - 1);  //  clipping x
        } else {
          xMin = std::max(a.x - std::min(segmRight.x, xSegmMax),
                          0);  //  clipping x + ritorno alle
          xMax = std::min(a.x - std::max(segmLeft.x, xSegmMin),
                          lx - 1);  //   coordinate "di schermo"
        }

        TPixel32 *p = ras->pixels(segmRight.y + a.y) + xMin;
        TPixel32 *q = p + (xMax - xMin);

        while (p <= q) *p++ = col;

        dSegmRight = dSegmRight + incNE;
        segmRight.x++;
        segmRight.y++;

        while (dSegmLeft < 0)  //  segmLeft a Est
        {
          dSegmLeft = dSegmLeft + incE;
          segmLeft.x++;
        }
        //  segmLeft a NordEst
        dSegmLeft = dSegmLeft + incNE;
        segmLeft.x++;
        segmLeft.y++;
      }
    }
  } else  //  m>1
  {
    //  midpoint algorithm; le scanline vengono disegnate sempre
    TPoint segmRight(tround((yMin - rightDown.y) / mParall + rightDown.x),
                     yMin);
    TPoint segmLeft(tround((yMin - leftDown.y) / mParall + leftDown.x), yMin);
    int dSegmRight = tfloor(alpha * (segmRight.x + 0.5) +
                            beta * (segmRight.y + 1) + gammaRight);
    int dSegmLeft = tfloor(alpha * (segmLeft.x + 0.5) +
                           beta * (segmLeft.y + 1) + gammaLeft);
    while (segmRight.y <= yMax) {
      int xMin, xMax;
      if (k > 0) {
        xMin = std::max(a.x + segmLeft.x, 0);        //  clipping x
        xMax = std::min(a.x + segmRight.x, lx - 1);  //  clipping x
      } else {
        xMin = std::max(a.x - segmRight.x, 0);  //  clipping x + ritorno alle
        xMax = std::min(a.x - segmLeft.x, lx - 1);  //   coordinate "di schermo"
      }

      TPixel32 *p = ras->pixels(segmRight.y + a.y) + xMin;
      TPixel32 *q = p + (xMax - xMin);

      while (p <= q) *p++ = col;

      if (dSegmRight <= 0)  //  segmRight a NordEst
      {
        dSegmRight = dSegmRight + incNE;
        segmRight.x++;
      } else  //  segmRight a Nord
      {
        dSegmRight = dSegmRight + incN;
      }
      segmRight.y++;

      if (dSegmLeft <= 0)  //  segmLeft a NordEst
      {
        dSegmLeft = dSegmLeft + incNE;
        segmLeft.x++;
      } else  //  segmLeft a Nord
      {
        dSegmLeft = dSegmLeft + incN;
      }
    }
  }

  // ----  parallelogrammoide circolare (in alternativa a parallelogramma)

  // N.B. coordinate di schermo (riflessione per k<0 )

  yMin = std::max(b.y - cutIn, 0);
  yMax = std::min(a.y + cutIn, ly - 1);
  for (y = yMin; y <= yMax; y++) {
    int xMin, xMax;
    if (k > 0) {
      xMin = std::max(a.x - halfCord.getCord(abs(y - a.y)), 0);  //  clipping x
      xMax = std::min(b.x + halfCord.getCord(abs(b.y - y)),
                      lx - 1);  //  clipping x
    } else {
      xMin = std::max(b.x - halfCord.getCord(abs(b.y - y)),
                      0);  //  clipping x + ritorno alle
      xMax = std::min(a.x + halfCord.getCord(abs(y - a.y)),
                      lx - 1);  //   coordinate "di schermo"
    }
    TPixel32 *p         = ras->pixels(y) + xMin;
    TPixel32 *q         = p + (xMax - xMin);
    while (p <= q) *p++ = col;
  }
  ras->unlock();
}
