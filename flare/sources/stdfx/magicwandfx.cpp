

#include <stack>
#include <time.h>

#include "stdfx.h"
#include "tsystem.h"
#include "tconvert.h"
#include "trop.h"

/* Riferimenti:
[1] "Filling a region in a frame buffer", Ken Fishkin, su Graphics Gems vol.1,
pag 278;
*/

// TODO
// v integrare in zcomp
// v aggiungere: selezioni multiple, antialias, feather, .
// v aggiungere: gestione del mouse, indicatore del colore del pixel
// selezionato, media sui vicini del pixel selezionato
// . maglass
// . verifiche ed ottimizzazioni

namespace {}  // anonymous namespace

static int invocazioni = 0;  // tmp: numero di invocazioni della MagicWand

// Shadow Segment
// "Ombra" proiettata da una riga di pixel idonei sulle righe adiacenti
// superiore ed inferiore; vedi [1].
class ShadowSegment {
public:
  ShadowSegment(int Lx, int Rx, int pLx, int pRx, int y, int dir)
      : m_lx(Lx), m_rx(Rx), m_pLx(pLx), m_pRx(pRx), m_y(y), m_dir(dir) {}
  int m_rx,   // Right endpoint
      m_lx,   // Left endpoint
      m_pRx,  // parent Right endpoint
      m_pLx,  // parent Left endpoint
      m_y,    // this segment line
      m_dir;  // upward, downward
};

// Stack per la memorizzazione dei segmenti.
typedef std::stack<ShadowSegment> ShadowSegmentStack;

class MagicWandFx : public TStandardRasterFx {
  FX_PLUGIN_DECLARATION(MagicWandFx)

  TRasterFxPort m_input;
  TDoubleParamP m_tolerance;   // tolleranza
  TDoubleParamP m_blurRadius;  // ampiezza del campione per il SEED

  TPointParamP m_point;       // coordinate del SEED (passate da zviewer)
  TBoolParamP m_contiguous;   // selezione di regioni non connesse alla regione
                              // contentente il SEED
  TBoolParamP m_antialiased;  // applicazione dell'antialiasing
  TBoolParamP m_euclideanD;   // funzione alternativa per il calcolo della
                              // "similitudine" tra punti
  TBoolParamP m_preMolt;      // premoltiplicazione
  TBoolParamP m_isShiftPressed;  // per le selezioni multiple: SUB
  TBoolParamP m_isAltPressed;    // per le selezioni multiple: ADD

public:
  MagicWandFx()
      : m_tolerance(15.0)
      , m_blurRadius(0.0)
      , m_point(TPointD(0, 0))

  {
    m_contiguous     = TBoolParamP(true);
    m_antialiased    = TBoolParamP(true);
    m_euclideanD     = TBoolParamP(true);
    m_preMolt        = TBoolParamP(true);
    m_isShiftPressed = TBoolParamP(false);
    m_isAltPressed   = TBoolParamP(false);

    addParam("Tolerance", m_tolerance);
    addParam("Feather", m_blurRadius);
    addParam("Point", m_point);
    addParam("Contiguous", m_contiguous);
    addParam("Antialias", m_antialiased);
    addParam("EuclideanD", m_euclideanD);
    addParam("PreMultiply", m_preMolt);
    addParam("isShiftPressed", m_isShiftPressed);
    addParam("isAltPressed", m_isAltPressed);
    addInputPort("Source", m_input);

    m_tolerance->setValueRange(0, 255);
    m_blurRadius->setValueRange(0, 100);
  }

  ~MagicWandFx(){};

  TRect getInvalidRect(const TRect &max);
  void doCompute(TTile &tile, double frame, const TRasterFxRenderInfo *ri);
  void doMagicWand(TTile &tile, double frame, const TRasterFxRenderInfo *ri);
  void EnqueueSegment(int num, int dir, int pLx, int pRx, int Lx, int Rx,
                      int y);
  bool pixelProcessor(TPixel32 *testPix, TPixelGR8 *maskPix);

  bool getBBox(double frame, TRectD &rect, TPixel32 &bgColor) {
    return m_input->getBBox(frame, rect, bgColor);
  }

  TRasterGR8P m_maskGR8;  // maschera
  TPixel32 *m_pickedPix;  // puntatore al pixel SEED
  TPixelGR8 *m_maskPickedPix;

  int m_imageHeigth;  // altezza del raster
  int m_imageWidth;   // larghezza del raster
  double m_tol;   // le uso per evitare di dover richiamare la funzione getValue
                  // per ogni punto: sistemare?
  int m_cont;     // le uso per evitare di dover richiamare la funzione getValue
                  // per ogni punto: sistemare?
  bool m_antial;  // le uso per evitare di dover richiamare la funzione getValue
                  // per ogni punto: sistemare?
  bool m_euclid;  // le uso per evitare di dover richiamare la funzione getValue
                  // per ogni punto: sistemare?
  bool m_add;
  bool m_sub;
  int m_id_invocazione;          // contatore delle invocazioni
  ShadowSegmentStack m_sSStack;  // stack dei segmenti shadow
};

const int EmptyPixel = 0;
const int FullPixel  = 255;

//--------------------------------------

// tmp: per l'analisi delle prestazioni
int pixelProcessed;
int pixelMasked;
int shadowEnqueued;
int pixelReprocessed;
int shadowOutOfBorder;
bool maskValue;

bool MagicWandFx::pixelProcessor(TPixel32 *testPix, TPixelGR8 *maskPix) {
  pixelProcessed++;
  unsigned int maskValue = 0;
  double diff            = 0;

  // valuto la distanza tra il testPix ed il SEED e la metto in diff
  if (m_euclid) {
    // calcolo la Distanza Euclidea tra i punti nello spazio RGB
    diff = sqrt((m_pickedPix->r - testPix->r) * (m_pickedPix->r - testPix->r) +
                (m_pickedPix->g - testPix->g) * (m_pickedPix->g - testPix->g) +
                (m_pickedPix->b - testPix->b) * (m_pickedPix->b - testPix->b));
  } else {
    // GIMP-like: confronto la tolleranza con il massimo tra gli scarti delle
    // componenti
    diff                       = abs(m_pickedPix->r - testPix->r);
    double diffNext            = abs(m_pickedPix->g - testPix->g);
    if (diffNext >= diff) diff = diffNext;
    diffNext                   = abs(m_pickedPix->b - testPix->b);
    if (diffNext >= diff) diff = diffNext;
  }

  if (diff <= m_tol)
    maskValue = FullPixel;
  else
    maskValue = EmptyPixel;

  if (maskValue) {
    // il pixel soddisfa il criterio di compatibilita

    if (m_add) {
      // sto aggiungendo selezioni
      if (testPix->m != EmptyPixel) {
        pixelReprocessed++;
        return false;
      }  // gia' trattato

      //* DECIDERE SE VOGLIO CHE LA SELEZIONE INTERESSI AREE GIA' SELEZIONATE IN
      // PRECEDENZA
      //      if (maskPix->value == EmptyPixel) { //pixel c-compatibile, non
      //      gia' mascherato
      testPix->m     = maskValue;  // set(mV)m
      maskPix->value = maskValue;  // set(mV)a
      pixelMasked++;
      //      } else { // pixel c-compatibile gia' mascherato precedentemente
      //               testPix->m = maskValue;  // set(mV)m
      //     }
    } else if (m_sub) {
      // sto togliendo selezioni
      if (testPix->m != EmptyPixel) return false;  // gia' trattato
      testPix->m     = maskValue;                  // set(mV)m
      maskPix->value = EmptyPixel;                 // set(0)a
    } else {                                       // prima selezione
      if (testPix->m != EmptyPixel) return false;  // gia' trattato
      testPix->m     = maskValue;                  // set(mV)m
      maskPix->value = maskValue;                  // set(mV)a
      pixelMasked++;
    }
    return true;
  } else
    return false;
}  // pixelProcessor

// [1]: aggiunge le ombre necessarie alla pila.
void MagicWandFx::EnqueueSegment(int num, int dir, int pLx, int pRx, int Lx,
                                 int Rx, int y) {
  int pushRx = Rx + 1;
  int pushLx = Lx + 1;

  //  TSystem::outputDebug("[MWfx("+toString(m_id_invocazione)+":"+toString(num)+")<PUSH
  //  1>]\tStack Size:"+toString((int)
  //  m_sSStack.size())+"\tLx:"+toString(Lx)+"\tRx:"+toString(Rx)+"\tpLx:"+toString(pushLx)+"\tpRx:"+toString(pushRx)+"\ty:"+toString(y)+"\tdir:"+toString(dir)+"\n");
  assert((Lx <= Rx) && (pushLx <= pushRx) && (Lx >= 0));
  m_sSStack.push(ShadowSegment(Lx, Rx, pushLx, pushRx, (y + dir), dir));
  shadowEnqueued++;

  if (Rx > pRx) {  // U-turn a destra
                   //  TSystem::outputDebug("[MWfx("+toString(m_id_invocazione)+":"+toString(num)+")<PUSH
                   //  2>]\tStack Size:"+toString((int)
                   //  m_sSStack.size())+"\tLx:"+toString(pRx+1)+"\tRx:"+toString(Rx)+"\tpLx:"+toString(pushLx)+"\tpRx:"+toString(pushRx)+"\ty:"+toString(y-dir)+"\tdir:"+toString(dir)+"\n");
    assert(((pRx + 1) <= (Rx)) && (pushLx <= pushRx) && ((pRx + 1) >= 0));
    m_sSStack.push(
        ShadowSegment((pRx + 1), Rx, pushLx, pushRx, (y - dir), (-dir)));
    shadowEnqueued++;
  }
  if (Lx < pLx) {  // U-turn a sinistra
                   //    TSystem::outputDebug("[MWfx("+toString(m_id_invocazione)+":"+toString(num)+")<PUSH
                   //    3>]\tStack Size:"+toString((int)
                   //    m_sSStack.size())+"\tLx:"+toString(Lx)+"\tRx:"+toString(pLx-1)+"\tpLx:"+toString(pushLx)+"\tpRx:"+toString(pushRx)+"\ty:"+toString(y-dir)+"\tdir:"+toString(dir)+"\n");
    assert(((Lx) <= (pLx - 1)) && (pushLx <= pushRx) && (Lx >= 0));
    m_sSStack.push(
        ShadowSegment(Lx, (pLx - 1), pushLx, pushRx, (y - dir), (-dir)));
    shadowEnqueued++;
  }
  // W-Turn = 2 U-Turn
}  // EnqueueSegment

void MagicWandFx::doMagicWand(TTile &tile, double frame,
                              const TRasterFxRenderInfo *ri) {
  clock_t start_time = clock();  // debug
  clock_t stop_time;

  invocazioni++;
  m_id_invocazione = invocazioni;
  m_tol            = m_tolerance->getValue(frame);
  m_antial         = m_antialiased->getValue();
  m_euclid         = m_euclideanD->getValue();  // temporaneo?
  m_cont = m_contiguous->getValue();  // selezione di aree cromaticamente
                                      // compatibili ma non contigue: Selezione
                                      // ByColor
  m_add = m_isShiftPressed->getValue();
  m_sub = m_isAltPressed->getValue();

  tile.getRaster()->lock();
  TRaster32P ras32 = tile.getRaster();

  TPixel32 vPixel;
  TPixel32 *tmpPix;
  TPixel32 *rowStart;
  TPixelGR8 *maskRowStart;
  TPixelGR8 *maskPix;

  if (ras32) {
    pixelProcessed    = 0;
    pixelMasked       = 1;
    shadowEnqueued    = 2;
    pixelReprocessed  = 0;
    shadowOutOfBorder = 0;

    m_imageWidth  = ras32->getLx();
    m_imageHeigth = ras32->getLy();
    // assert(m_imageWidth == 800);
    assert(m_imageHeigth <= 600);
    int lx = m_imageWidth;
    int ly = m_imageHeigth;

    if (!m_maskGR8) {
      // prima esecuzione creo il raster gr8 x la maschera e azzero gli alpha
      TRectD bBoxD;
      TPixel32 bgColor;
      bool getBBoxOk = getBBox(frame, bBoxD, bgColor);
      assert(getBBoxOk);
      TRect bBoxI = convert(bBoxD);
      m_maskGR8   = TRasterGR8P(bBoxI.getLx(), bBoxI.getLy());
      m_maskGR8->clear();
    }
    m_maskGR8->lock();
    // sono arrivato qui: sto verificando se serve davvero il gr8.

    for (int iy = 0; iy < m_imageHeigth; iy++) {  // y
      tmpPix = ras32->pixels(iy);
      for (int ix = 0; ix < m_imageWidth; ix++) {  // x
        tmpPix->m = EmptyPixel;
        tmpPix++;
      }  // x
    }    // y

    if (m_add) {  // ho premuto Shift sto aggiungendo alla selezione

    } else if (m_sub) {
      // ho premuto Alt sto sottraendo dalla selezione

    } else {
      // non ho premuto niente nuova selezione

      // ripulisco il canale alpha dell'immagine e la maschera
      for (int iy = 0; iy < m_imageHeigth; iy++) {
        tmpPix  = ras32->pixels(iy);
        maskPix = m_maskGR8->pixels(iy);
        for (int ix = 0; ix < m_imageWidth; ix++) {
          tmpPix->m = 0;

          maskPix->value = EmptyPixel;
          tmpPix++;
          maskPix++;
        }  // x
      }    // y
    }

    // trovo il pixel in X,Y soluzione temporanea in attesa della gestione del
    // mouse.
    // converto le coordinate mondo (-500;+500) in coordinate raster
    // (0;m_imageWidth);
    TPointD point = m_point->getValue(frame);

    // coordinate dagli sliders
    //    int x = (int) (500+point.x)*m_imageWidth/1000; if (x>0) x--;
    //    int y = (int) (500+point.y)*m_imageHeigth/1000; if (y>0) y--;

    // coordinate dalla ZViewer:leftButtonClick

    int x = tcrop((int)(point.x + m_imageWidth / 2), 0, (m_imageWidth - 1));
    int y = tcrop((int)(point.y + m_imageHeigth / 2), 0, (m_imageHeigth - 1));

    TSystem::outputDebug("\n[MWfx(" + toString(m_id_invocazione) +
                         ")<begin>]\nSize:" + toString(m_imageWidth) + "x" +
                         toString(m_imageHeigth) + "\tx:" + toString(x) +
                         "\ty:" + toString(y) + "\tToll:" + toString(m_tol) +
                         /*      "\tRadius:" + toString(radius) +*/ (
                             (m_cont) ? "\tContiguous" : "\tNon Contiguous") +
                         ((m_antial) ? "\tAnti Aliased" : "\tAliased") +
                         ((m_euclid) ? "\tEuclidean\n" : "\tNon Euclidean\n"));

    lx = m_imageWidth;
    ly = m_imageHeigth;

    m_pickedPix     = ras32->pixels(y) + x;
    m_maskPickedPix = m_maskGR8->pixels(y) + x;

    pixelProcessed = 1;

    if (m_cont) {  // seleziono esclusivamente i pixel connessi al pixel SEED

      //- ALGORITMO FLOOD FILL: GRAPHICS GEM 1 p.280
      //----------------------------------------
      int xAux, yAux, lxAux, rxAux, dirAux, pRxAux, pLxAux;
      bool inSpan = true;

      // trova Rx e Lx dello span contentente il SEED point
      int xCont = x;
      tmpPix    = m_pickedPix;      // puntatore al SEED
      maskPix   = m_maskPickedPix;  //******

      // cerco Lx
      maskValue  = pixelProcessor(tmpPix, maskPix);
      bool tmpMv = maskValue;
      while ((xCont >= 0) && (maskValue)) {
        tmpPix--;
        maskPix--;
        xCont--;
        if (xCont >= 0) maskValue = pixelProcessor(tmpPix, maskPix);
      }
      if (tmpMv)
        lxAux = xCont + 1;
      else
        lxAux = xCont;

      // cerco Rx
      tmpPix  = m_pickedPix;
      maskPix = m_maskPickedPix;  //******

      xCont     = x;
      maskValue = tmpMv;
      while ((xCont < m_imageWidth) && (maskValue)) {
        tmpPix++;
        maskPix++;
        xCont++;
        if (xCont < m_imageWidth) maskValue = pixelProcessor(tmpPix, maskPix);
      }
      if (tmpMv)
        rxAux = xCont - 1;
      else
        rxAux = xCont;

      assert((lxAux <= rxAux) && (lxAux >= 0));

      // metto nella pila delle ombre la riga sopra e sotto quella contentente
      // il seed.
      //            TSystem::outputDebug("[MWfx("+toString(m_id_invocazione)+")]\tStack
      //            Size:"+toString((int)
      //            m_sSStack.size())+"\tLx:"+toString(lxAux)+"\tRx:"+toString(rxAux)+"\tpLx:"+toString(lxAux)+"\tpRx:"+toString(rxAux)+"\ty:"+toString(y+1)+"\tdir:"+toString(1)+"\n");
      m_sSStack.push(ShadowSegment(lxAux, rxAux, lxAux, rxAux, y + 1,
                                   +1));  // cerca in alto
                                          //            TSystem::outputDebug("[MWfx("+toString(m_id_invocazione)+")]\tStack
                                          //            Size:"+toString((int)
                                          //            m_sSStack.size())+"\tLx:"+toString(lxAux)+"\tRx:"+toString(rxAux)+"\tpLx:"+toString(lxAux)+"\tpRx:"+toString(rxAux)+"\ty:"+toString(y-1)+"\tdir:"+toString(-1)+"\n");
      m_sSStack.push(ShadowSegment(lxAux, rxAux, lxAux, rxAux, y - 1,
                                   -1));  // cerca in basso

      while (!m_sSStack.empty()) {
        ShadowSegment sSegment = m_sSStack.top();
        m_sSStack.pop();
        //        TSystem::outputDebug("[MWfx("+toString(m_id_invocazione)+":0)<POP
        //        >]\tStack Size:"+toString((int)
        //        m_sSStack.size())+"\tLx:"+toString(sSegment.m_lx)+"\tRx:"+toString(sSegment.m_rx)+"\tpLx:"+toString(sSegment.m_pLx)+"\tpRx:"+toString(sSegment.m_pRx)+"\ty:"+toString(sSegment.m_y)+"\tdir:"+toString(sSegment.m_dir)+"\n");

        dirAux = sSegment.m_dir;
        pRxAux = sSegment.m_pRx;
        pLxAux = sSegment.m_pLx;
        lxAux  = sSegment.m_lx;
        rxAux  = sSegment.m_rx;
        yAux   = sSegment.m_y;

        if ((yAux < 0) || (yAux >= m_imageHeigth)) {
          shadowOutOfBorder++;
          continue;  // questo segmento sta fuori dal raster oppure l'ho gia'
                     // colorato: lo salto
        }
        assert((lxAux <= rxAux) && (pLxAux <= pRxAux));
        assert((m_sSStack.size() <= 1000));

        xAux = lxAux + 1;

        rowStart = ras32->pixels(yAux);

        maskRowStart = m_maskGR8->pixels(yAux);  //**

        tmpPix  = rowStart + lxAux;
        maskPix = maskRowStart + lxAux;

        maskValue = pixelProcessor(tmpPix, maskPix);

        inSpan = (maskValue);

        if (maskValue) {  // il punto e' cromaticompatibile
          lxAux--;
          if (lxAux >= 0) {
            tmpPix--;
            maskPix--;
            maskValue = pixelProcessor(tmpPix, maskPix);
          }
          while ((maskValue) &&
                 (lxAux >= 0)) {  // sto nello span E nell'immagine
            lxAux--;
            if (lxAux >= 0) {
              tmpPix--;
              maskPix--;
              maskValue = pixelProcessor(tmpPix, maskPix);
            }
          }  // sto nello span E nell'immagine
        }    // il punto e' cromaticompatibile
        lxAux++;
        //        rowStart = ras32->pixels(yAux);
        while (xAux < m_imageWidth) {  // mi sposto a destra lungo la X
          if (inSpan) {
            tmpPix    = rowStart + xAux;
            maskPix   = maskRowStart + xAux;  //***
            maskValue = pixelProcessor(tmpPix, maskPix);
            if (maskValue) {  // case 1
                              // fa tutto nella pixel processor
            }                 // case 1
            else {            // case 2
              EnqueueSegment(1, dirAux, pLxAux, pRxAux, lxAux, (xAux - 1),
                             yAux);
              inSpan = false;
            }     // case 2
          }       // inSpan
          else {  // non ero nello span
            if (xAux > rxAux) break;
            tmpPix    = rowStart + xAux;
            maskPix   = maskRowStart + xAux;
            maskValue = pixelProcessor(tmpPix, maskPix);
            if (maskValue) {  // case 3
              inSpan = true;
              lxAux  = xAux;
            }       // case 3
            else {  // case 4
            }
          }  // non ero nello span
          xAux++;
          //          TSystem::outputDebug("[MWfx("+toString(m_id_invocazione)+")]\tStack
          //          Size:"+toString((int)
          //          m_sSStack.size())+"\txAux:"+toString(xAux)+"\ty:"+toString(yAux)+"\n");
        }  // mi sposto a destra lungo la X: endloop 1
        if (inSpan) {
          EnqueueSegment(2, dirAux, pLxAux, pRxAux, lxAux, (xAux - 1), yAux);
        }
      }     // finche' la pila non e' vuota: endloop 2
    }       // if m_cont
    else {  // anche le regioni simili NON contigue: questo rimane anche in caso
            // di modifica della parte m_cont
      for (int iy = 0; iy < m_imageHeigth; iy++) {
        tmpPix  = ras32->pixels(iy);
        maskPix = m_maskGR8->pixels(iy);
        for (int ix = 0; ix < m_imageWidth; ix++) {
          maskValue = pixelProcessor(tmpPix, maskPix);
          //                 if (maskValue) { } // if// il colore e' simile =>
          //                 va incluso nella selezione // fa tutto nella pixel
          //                 processor
          tmpPix++;
          maskPix++;
        }  // ix
      }    // iy
    }      // else m_cont

    int blurRadius = (int)(m_blurRadius->getValue(frame));
    if ((m_antial) && (blurRadius < 2)) blurRadius = 1;

    if (blurRadius > 0)
      TRop::blur(m_maskGR8, m_maskGR8, (blurRadius + 1), 0, 0);

    // copio la maschera sull'alpha channel dell'immagine
    // lo faccio a mano chiedere se esiste una funziona apposita
    for (iy = 0; iy < m_imageHeigth; iy++) {
      tmpPix  = ras32->pixels(iy);
      maskPix = m_maskGR8->pixels(iy);
      for (int ix = 0; ix < m_imageWidth; ix++) {
        tmpPix->m = maskPix->value;
        tmpPix++;
        maskPix++;
      }  // ix
    }    // iy

    if (m_preMolt->getValue()) TRop::premultiply(ras32);
    stop_time     = clock();
    double durata = (double)(stop_time - start_time) / CLOCKS_PER_SEC;

    TSystem::outputDebug(
        "\n#Pixel:\t" + toString(m_imageWidth * m_imageHeigth) + "\nProc:\t" +
        toString(pixelProcessed) + "\t[" +
        toString((pixelProcessed * 100 / (m_imageWidth * m_imageHeigth))) +
        "%t]" + "\nMask:\t" + toString(pixelMasked) + "\t[" +
        toString((pixelMasked * 100 / (m_imageWidth * m_imageHeigth))) + "%t]" +
        "\t[" + toString((pixelMasked * 100 / (pixelProcessed))) + "%p]" +
        "\nEnqu:\t" + toString(shadowEnqueued) + "\nRepr:\t" +
        toString(pixelReprocessed) + "\t[" +
        toString((pixelReprocessed * 100 / (m_imageWidth * m_imageHeigth))) +
        "%t]" + "\t[" + toString((pixelReprocessed * 100 / (pixelProcessed))) +
        "%p]" + "\nOutB:\t" + toString(shadowOutOfBorder) + "\t[" +
        toString((shadowOutOfBorder * 100 / (shadowEnqueued))) + "%t]" +
        "\nTime:\t" + toString(durata, 3) + " sec\n[MagicWandFX <end>]\n");

  }  // if (ras32)
  else {
    TRasterGR8P rasGR8 = tile.getRaster();
    if (rasGR8) {
    }
  }
  tile.getRaster()->unlock();
  m_maskGR8->unlock();
}  // doMagicWand

void MagicWandFx::doCompute(TTile &tile, double frame,
                            const TRasterFxRenderInfo *ri) {
  if (!m_input.isConnected()) return;

  m_input->compute(tile, frame, ri);

  doMagicWand(tile, frame, ri);
}

//------------------------------------------------------------------

TRect MagicWandFx::getInvalidRect(const TRect &max) { return max; }

//------------------------------------------------------------------

FX_PLUGIN_IDENTIFIER(MagicWandFx, magicWandFx);
