

/*----------------------------------------------------------------------
 * Predice la posizione corrente dei punti-immagine di un oggetto
 * tridimensionale piano, nota la posizione iniziale dei punti-immagine
 * e la posizione corrente di un sottoinsieme dei punti-immagine.
 *
 * DESCRIZIONE DEL PROBLEMA
 * Sono dati k punti bidimensionali che rappresentano la proiezione
 * sul piano-immagine di k punti tridimensionali appartenenti a uno
 * stesso oggetto; i punti tridimensionali sono complanari, e le loro
 * coordinate 3D NON sono note.
 * Successivamente, l'oggetto viene sottoposto a rotazioni e traslazioni,
 * e possibilmente l'immagine subisce uno zoom.
 * A seguito di questa trasformazione, viene rilevata la posizione sul
 * piano-immagine di un sottoinsieme dei k punti iniziali.
 * Con questi dati, si vuole predire la posizione corrente sul
 * piano-immagine dei rimanenti punti.
 * L'algoritmo richiede le seguenti precondizioni:
 *       - i punti tridimensionali giacciono sullo stesso piano,
 *         non necessariamente parallelo al piano-immagine
 *       - il piano dei punti tridimensionali non degenera in una
 *         retta se osservato dal punto di vista della camera
 *       - i punti di cui e' nota la posizione corrente sono almeno 4
 *       - i punti di cui e' nota la posizione corrente non sono
 *         allineati, ovvero non giacciono su un'unica retta
 ---------------------------------------------------------------------*/
#include <memory>

#include "predict3d.h"
#include "metnum.h"

using namespace Predict3D;
using namespace MetNum;

/*----------------------------------------------------------------------
 * Calcola la posizione dei punti non visibili data la posizione
 * iniziale di tutti i punti e la posizione corrente dei punti visibili.
 * PARAMETRI DI INGRESSO
 * 	k		numero di punti
 * 	initial		posizione iniziale dei punti nell'immagine
 * 	current		posizione corrente dei punti visibili;
 * 			il punto current[i] corrisponde al punto initial[i]
 * 	visible		indicatore di visibiltia'; se visible[i] e' true,
 * 			il punto i e' visibile e current[i] ne indica
 * 			la posizione corrente; altrimenti il valore di
 * 			current[i] non e' definito
 * PARAMETRI DI USCITA
 * 	current		posizione corrente di tutti i punti
 * VALORE DI RITORNO
 * 	true		la funzione non ha incontrato problemi
 * 	false		si e' verificato un errore; le cause di errore
 * 			possibili sono:
 * 			   - k < 4
 * 			   - il numero di punti visibili e' < 4
 *			   - i punti visibili sono allineati
 ----------------------------------------------------------------------------*/
bool Predict3D::Predict(int k, Point initial[], Point current[],
                        bool visible[]) {
  /* Definizione e allocazione delle variabili */
  int n, m, kvis;
  double **x;
  int i, ii, j, l;
  kvis = 0;
  for (i = 0; i < k; i++)
    if (visible[i]) kvis++;
  if (kvis < 4) return false;
  n = 3 * kvis + 1;
  m = kvis + 9;

  x = AllocMatrix(m, n);
  if (x == 0) return false;
  std::unique_ptr<double[]> y(new double[n]);
  std::unique_ptr<double[]> c(new double[m]);

  /* Costruzione dei coefficienti del sistema */

  for (j = 0; j < n - 1; j++) y[j] = 0.0;
  y[n - 1]                         = 1.0;

  for (l = 0; l < m; l++)
    for (j = 0; j < n; j++) x[l][j] = 0.0;

  for (l = 0; l < m; l++) c[l] = 0.0;

  ii = 0;
  for (i = 0; i < k; i++) {
    if (!visible[i]) continue;
    j       = 3 * ii;
    x[0][j] = x[3][j + 1] = x[6][j + 2] = -initial[i].x;
    x[1][j] = x[4][j + 1] = x[7][j + 2] = -initial[i].y;
    x[2][j] = x[5][j + 1] = x[8][j + 2] = -1.0;
    l                                   = ii + 9;
    x[l][j]                             = current[i].x;
    x[l][j + 1]                         = current[i].y;
    x[l][j + 2]                         = 1.0;
    ii++;
  }
  x[9][n - 1] = 1.0;

  /* Soluzione del sistema */

  int status = Approx(n, m, x, y.get(), c.get());
  if (status != 0) {
    FreeMatrix(m, x);
    return false;
  }

  /* Applicazione della soluzione */
  for (i = 0; i < k; i++) {
    if (visible[i]) continue;
    double x     = initial[i].x;
    double y     = initial[i].y;
    double uw1   = x * c[0] + y * c[1] + c[2];
    double vw1   = x * c[3] + y * c[4] + c[5];
    double w1    = x * c[6] + y * c[7] + c[8];
    current[i].x = uw1 / w1;
    current[i].y = vw1 / w1;
  }

  FreeMatrix(m, x);

  return true;
}
