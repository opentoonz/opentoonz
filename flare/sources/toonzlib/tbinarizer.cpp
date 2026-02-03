

#include "toonz/tbinarizer.h"
#include "tpixelutils.h"

namespace {

//
// computeColor: RGB =>  colore(0=Black, 1=Red,2=Green,3=Blue), valore
// (=max(r,g,b)), croma (= max(r,g,b)-min(r,g,b))
//
inline void computeColor(int &color, int &value, int &chroma,
                         const TPixel32 &pix) {
  int c[3] = {pix.r, pix.g, pix.b};
  // unrolled bubble-sort
  if (c[0] < c[1]) qSwap(c[0], c[1]);
  if (c[1] < c[2]) qSwap(c[1], c[2]);
  if (c[0] < c[1]) qSwap(c[0], c[1]);
  assert(c[0] >= c[1] && c[1] >= c[2]);
  int cMax = c[0];
  int cMin = c[2];
  value    = cMax;
  chroma   = cMax - cMin;
  if (chroma == 0) {
    color = 0;
    return;
  }

  if (pix.r == cMax)
    color = 1;
  else if (pix.g == cMax)
    color = 2;
  else
    color = 3;

  /*
// quando gestiremo anche ciano, magenta e giallo
int cD0 = c[0]-c[1], cD1 = c[1]-c[2];
if(cD0>cD1)
{
if(pix.r==cMax) color=1;
else if(pix.g==cMax) color=2;
else color=3;
}
else
{
if(pix.r==cMin) color=4;
else if(pix.g==cMin) color=5;
else color=6;
}
*/
}

// ritorna true se i primi pixel del buffer non sono tutti opachi
bool hasAlpha(const TPixel32 *buffer, int w, int h) {
  int n = std::min(500, w * h);
  for (int i = 0; i < n; i++)
    if (buffer[i].m < 255) return true;
  return false;
}

// aggiunge un bianco opaco all'immagine (dovremmo gia' avere una funzione del
// genere, ma non l'ho trovata)
void removeAlpha(TPixel32 *buffer, int w, int h) {
  TPixel32 *pix    = buffer;
  TPixel32 *endPix = pix + w * h;
  while (pix < endPix) {
    *pix = overPixOnWhite(*pix);
    ++pix;
  }
}

}  // namespace

//=========================================================

TBinarizer::TBinarizer() : m_alphaEnabled(true) {}

//---------------------------------------------------------

void TBinarizer::process(const TRaster32P &ras) {
  // palette di colori puri che verranno usati per l'output: nero, rosso, verde,
  // blu
  static const TPixel32 colors[] = {
      TPixel32(0, 0, 0),    TPixel32(255, 0, 0),   TPixel32(0, 255, 0),
      TPixel32(0, 0, 255),  TPixel32(0, 255, 255), TPixel32(255, 0, 255),
      TPixel32(255, 255, 0)};

  int w = ras->getLx(), h = ras->getLy();
  TPixel32 *buffer = ras->pixels();  // WARNING! non funziona sui sotto-raster
  assert(ras->getWrap() == ras->getLx());

  // mi cautelo contro immagini trasparenti
  if (hasAlpha(buffer, w, h)) removeAlpha(buffer, w, h);

  // divido l'immagine in quadrati grandi 2^b pixel
  int b     = 5;
  int bsize = 1 << b;
  int w1 = ((w - 1) >> b) + 1, h1 = ((h - 1) >> b) + 1;

  // buffer di appoggio
  std::vector<unsigned char> vBuffer(
      w * h, 0);  // per ogni pixel: v = min(r,g,b) (inchiostro scuro su sfondo
                  // chiaro: mi sembra che min funzioni meglio di max)
  std::vector<unsigned char> thrBuffer(w1 * h1, 0);  // per ogni quadrato: se
                                                     // v>thrBuffer[k1] allora
                                                     // siamo sicuro che e'
                                                     // sfondo
  std::vector<unsigned char> qBuffer(w1 * h1, 0);  // per ogni quadrato: quel v
                                                   // tale che solo 20 pixel in
                                                   // tutto il quadrato sono
                                                   // piu' scuri
  std::vector<unsigned char> tBuffer(w * h, 255);  // per ogni pixel: 'tipo' del
                                                   // pixel (0..6=>colore,
                                                   // 20=cornice esterna, )
  std::vector<unsigned char> sBuffer(w * h, 255);  // per ogni pixel: quando
                                                   // faccio il fill il colore
                                                   // si puo' estendere solo sui
                                                   // vicini con v<sBuffer[k]

  std::vector<int> boundary1, boundary2;  // usati per il fill
  boundary1.reserve(w * h / 2);
  boundary2.reserve(w * h / 2);

  int goodBgQuadCount = 0;

  // per ogni quadrato costruisco l'istrogramma e determino il threshold per il
  // bg
  // conto i quadrati che hanno un "buon" background
  for (int y1 = 0; y1 < h1; y1++) {
    int ya = y1 * bsize;
    int yb = std::min(h, ya + bsize) - 1;
    for (int x1 = 0; x1 < w1; x1++) {
      int xa = x1 * bsize;
      int xb = std::min(w, xa + bsize) - 1;

      int tot = 0;
      std::vector<int> histo(32, 0);
      for (int y = ya; y <= yb; y++) {
        for (int x = xa; x <= xb; x++) {
          int k        = y * w + x;
          TPixel32 pix = buffer[k];
          int v        = std::min({pix.r, pix.g, pix.b});
          vBuffer[k]   = v;
          histo[v >> 3] += 1;
          tot += 1;
        }
      }

      int bgThreshold = 31;
      if (histo[31] > tot / 2 && histo[30] < 4) {
        // "buon" background. Picco su histo[31] che finisce in histo[30]
        goodBgQuadCount++;
        bgThreshold = 29;
      } else {
        // background normale. salto eventuali zeri (lo sfondo puo' essere
        // grigio, anche scuro)
        int i = 31;
        while (i >= 0 && histo[i] == 0) i--;
        int i0 = i;
        // cerco il massimo
        while (i > 0 && histo[i - 1] > histo[i]) i--;
        int i1 = i;
        // presuppongo che il picco del BG sia symmetrico: i0-i1 == i1-i2
        int i2      = 2 * i1 - i0;
        bgThreshold = i2 - 1;
      }
      // calcolo qBuffer[k1] : e' un valore di v tale che pochi pixel (<20)
      // hanno un valore inferiore.
      // Se qBuffer[k1] e' molto grande vuol dire che il quadrato e' vuoto
      int i = 0;
      int c = histo[i];
      while (i < bgThreshold && c < 20) c += histo[++i];
      int k1        = y1 * w1 + x1;
      qBuffer[k1]   = i * 255 / 31;
      bgThreshold   = bgThreshold * 255 / 31;
      thrBuffer[k1] = bgThreshold;
    }
  }

  // L'immagine ha un buon background se la maggior parte dei quadrati ha un
  // buon background
  bool goodBackground = goodBgQuadCount > w1 * h1 / 2;

  // thrDelta e' una correzione sul threshold per immagini con cattivo
  // background
  // un quadrato che abbia meno di 20 (v.s sopra c<20) sotto
  // thrBuffer[k1]-thrDelta e' considerato sfondo
  int thrDelta                  = 0;
  if (!goodBackground) thrDelta = 20;

  // inizializzo la cornice dell'immagine (mi serve per non sconfinare quando
  // faccio fill)
  for (int y = 0; y < h; y++) tBuffer[y * w] = tBuffer[y * w + w - 1] = 20;
  for (int x = 0; x < w; x++) tBuffer[x] = tBuffer[(h - 1) * w + x] = 20;

  // cerco i pixel sicuramente colorati e i sicuramente neri
  for (int y1 = 0; y1 < h1; y1++) {
    int ya = std::max(1, y1 * bsize);
    int yb = std::min(h - 1, ya + bsize) - 1;
    for (int x1 = 0; x1 < w1; x1++) {
      int k1 = y1 * w1 + x1;
      // salto i quadrati completamente bianchi
      if (qBuffer[k1] >= thrBuffer[k1] - thrDelta) continue;

      for (int y = ya + 1; y <= yb; y++) {
        int xa = std::max(1, x1 * bsize);
        int xb = std::min(w - 1, xa + bsize) - 1;
        for (int x = xa + 1; x <= xb; x++) {
          int k = y * w + x;

          int vk  = vBuffer[k];
          int vk2 = vk;
          if (vk < thrBuffer[k1]) {
            // vk<thrBuffer[k1] : non e' detto che il pixel sia sfondo
            bool isSeed = false;
            if (vk2 <= vBuffer[k + 1] && vk2 <= vBuffer[k - 1] &&
                vk2 <= vBuffer[k + w] && vk2 <= vBuffer[k - w]) {
              // il pixel e' un minimo locale. calcolo il colore
              int color, value, chroma;
              computeColor(color, value, chroma, buffer[k]);

              const int chromaThreshold = 40;
              if (chroma > chromaThreshold) {
                // il pixel e' un buon candidato ad essere "sicuramente
                // colorato": controllo i vicini
                // per evitare i pixel colorati sparsi (sulle linee nere).
                int m    = 0;
                int dd[] = {1, -1, w, -w, 1 + w, 1 - w, -1 + w, -1 - w};
                for (int j = 0; j < 8; j++) {
                  int c1, v1, ch1;
                  computeColor(c1, v1, ch1, buffer[k + dd[j]]);
                  if (ch1 > 20 && c1 == color) m++;
                }
                // scarto se intorno non ci sono almeno due pixel dello stesso
                // colore
                // n.b. lascio un valore alto per chroma per evitare che il
                // pixel possa diventare "sicuramente nero". vedi sotto
                if (m < 2) chroma = chromaThreshold - 1;
              }

              if (chroma > chromaThreshold) {
                // "sicuramente" colorato
                tBuffer[k] = color;
                isSeed     = true;
              } else if (chroma < 15 &&
                         vBuffer[k] * 100 <
                             qBuffer[k1] * 25 +
                                 (thrBuffer[k1] - thrDelta) * 75) {
                // molto poco colorato e scuro (al 25% della distanza fra
                // qBuffer[] e bg) => "sicuramente" nero
                tBuffer[k] = 0;
                isSeed     = true;
              }
            }
            if (isSeed) {
              // se il pixel e' sicuramente colorato o sicuramente nero diventa
              // un seme per il fill
              // il fill si deve fermare quando sono al 50% della distanza verso
              // il bg
              sBuffer[k] = (vBuffer[k] + thrBuffer[k1]) / 2;
              boundary1.push_back(k);
            }
          }
        }
      }
    }
  }

  // vado avanti finche' ci sono semi, ma non oltre 10 iterazioni
  for (int it = 0; it < 10 && !boundary1.empty(); it++) {
    // per tutti i semi
    for (int i = 0; i < (int)boundary1.size(); i++) {
      int k = boundary1[i];
      // per ogni direzione
      const int dd[] = {1, -1, w, -w};
      for (int j = 0; j < 4; j++) {
        int ka = dd[j] + k;
        // se il pixel adiacente non e' gia' colorato (e non e' la cornice
        // esterna) e ha un v ancora abbastanza scuro...
        if (tBuffer[ka] > 20 && vBuffer[ka] < sBuffer[k]) {
          // lo coloro e lo faccio diventare un nuovo seme
          tBuffer[ka] = tBuffer[k];
          sBuffer[ka] = sBuffer[k];
          boundary2.push_back(ka);
        }
      }
    }
    // scambio i buffer
    boundary1.clear();
    boundary1.swap(boundary2);
  }

  // output
  TPixel32 bgColor =
      m_alphaEnabled ? TPixel32(0, 0, 0, 0) : TPixel32(255, 255, 255);
  for (int y = 0; y < h; y++) {
    for (int x = 0; x < w; x++) {
      int k = y * w + x;
      if (tBuffer[k] <= 6)
        buffer[k] = colors[tBuffer[k]];
      else
        buffer[k] = bgColor;
    }
  }
}
