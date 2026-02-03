

/*------------------------  modulo metnum.cpp  --------------------------
 * SCOPO
 * Implementazione di alcuni algoritmi relativi a metodi numerici.
 *
 * Autore: P. Foggia (1991)
 *---------------------------------------------------------------------*/

#include <math.h>
#include <float.h>
#include <stdlib.h>
#include "metnum.h"

#ifndef DBL_EPSILON
#define DBL_EPSILON 2.2204460492503131e-16
#endif

using namespace MetNum;

/*-----------------------  funzioni globali  ----------------------------*/

/*-----------------------  AllocMatrix()  -------------------------------*/
/*   SCOPO                                                               */
/* Alloca una matrice nell'heap come vettore di puntatori a righe        */
/*                                                                       */
/*   SPECIFICHE                                                          */
/* double **AllocMatrix(int n, int m)                                     */
/*                                                                       */
/*   PARAMETRI DI INPUT                                                  */
/* int n               numero di righe; deve essere > 0                  */
/* int m               numero di colonne; deve essere > 0                */
/*                                                                       */
/*   PARAMETRI DI OUTPUT                                                 */
/* Il valore restituito dalla funzione  il puntatore al primo elemento  */
/* del vettore di puntatori.                                             */
/*                                                                       */
/*   INDICATORI DI ERRORE                                                */
/* Se il valore restituito  NULL, si  verificato un errore: le cause   */
/* di errore possono essere la mancanza di memoria oppure il passaggio   */
/* di un valore < 1 per n o m.                                           */
/*-----------------------------------------------------------------------*/
double **MetNum::AllocMatrix(int n, int m) {
  double **A;
  int i, j;

  if ((n < 1) || (m < 1)) return NULL;

  A = new double *[n];
  if (A == NULL) return NULL;

  for (i = 0; i < n; i++) {
    if ((A[i] = new double[m]) == NULL) {
      for (j = 0; j < i; j++) delete[] A[j];
      delete[] A;
      return NULL;
    }
  }

  return A;
}

/*--------------------------- FreeMatrix() ------------------------------*/
/*   SCOPO                                                               */
/* Dealloca una matrice allocata mediante AllocMatrix (v. n. 8)          */
/*                                                                       */
/*   SPECIFICHE                                                          */
/* void FreeMatrix(int n, double **A)                                     */
/*                                                                       */
/*   PARAMETRI DI INPUT                                                  */
/* int n               numero di righe                                   */
/* double **A           puntatore alla matrice da deallocare              */
/*-----------------------------------------------------------------------*/
void MetNum::FreeMatrix(int n, double **A) {
  int i;

  for (i = 0; i < n; i++) delete[] A[i];
  delete[] A;
}

/*-----------------------  AllocTriangMatrix()  -------------------------*/
/*   SCOPO                                                               */
/* Alloca una matrice triangolare inferiore nell'heap come vettore di    */
/* puntatori a righe                                                     */
/*                                                                       */
/*   SPECIFICHE                                                          */
/* double **AllocTriangMatrix(int n)                                      */
/*                                                                       */
/*   PARAMETRI DI INPUT                                                  */
/* int n               numero di righe; deve essere > 0                  */
/*                                                                       */
/*   PARAMETRI DI OUTPUT                                                 */
/* Il valore restituito dalla funzione  il puntatore al primo elemento  */
/* del vettore di puntatori.                                             */
/*                                                                       */
/*   INDICATORI DI ERRORE                                                */
/* Se il valore restituito  NULL, si  verificato un errore: le cause   */
/* di errore possono essere la mancanza di memoria oppure il passaggio   */
/* di un valore < 1 per n                                                */
/*-----------------------------------------------------------------------*/
double **MetNum::AllocTriangMatrix(int n) {
  double **A;
  int i, j;

  if (n < 1) return NULL;

  A = new double *[n];
  if (A == NULL) return NULL;

  for (i = 0; i < n; i++) {
    if ((A[i] = new double[i + 1]) == NULL) {
      for (j = 0; j < i; j++) delete[] A[j];
      delete[] A;
      return NULL;
    }
  }

  return A;
}

/*-------------------------  Approx()  -----------------------------------*/
/*   SCOPO                                                                */
/* La funzione risolve il problema di miglior approssimazione in norma 2  */
/* di una funzione discreta, una volta assegnata una base dello spazio X  */
/* al cui interno si cerca la funzione approssimante.                     */
/*                                                                        */
/*   SPECIFICHE                                                           */
/* int Approx(int n, int m, double **x, double y[], double c[])              */
/*                                                                        */
/*   PARAMETRI DI INPUT                                                   */
/* int n               numero di punti                                    */
/* int m               ordine della base dello spazio di funzioni         */
/*                     in cui si cerca la funzione approssimante          */
/* double **x           matrice, allocata come vettore di puntatori        */
/*                     a righe, contenente i valori che le funzioni       */
/*                     della base assumono nei punti 0,...,n-1:           */
/*                     x[h][i] rappresenta il valore della funzione x[h]  */
/*                     nel punto i                                        */
/* double y[n]          valori della funzione y nei punti 0,...,n-1        */
/*                                                                        */
/*   PARAMETRI DI OUTPUT                                                  */
/* double c[m]          coefficienti della funzione approssimante          */
/*                                                                        */
/*   INDICATORI DI ERRORE                                                 */
/* Il valore restituito  0 se non si sono verificati errori; pi         */
/* in dettaglio tale valore pu essere:                                   */
/*                  0      nessun errore                                  */
/*                 -1      n<1 o m<1                                      */
/*                 -2      memoria insufficiente                          */
/*                 -3      errore nella risoluzione del sistema           */
/*------------------------------------------------------------------------*/
int MetNum::Approx(int n, int m, double **x, double y[], double c[]) {
  int i, j, k;
  double **A;
  int retval;

  /* controllo sui parametri di input */
  if ((n < 1) || (m < 1)) return -1;

  /* allocazione della memoria per la matrice */
  if ((A = AllocTriangMatrix(m)) == NULL) return -2;

  /* prepara la matrice dei coefficienti */
  for (i = 0; i < m; i++)
    for (j = 0; j <= i; j++) /* la matrice  symmetrica */
    {
      A[i][j] = 0;
      for (k = 0; k < n; k++) A[i][j] += x[i][k] * x[j][k];
    }

  /* prepara il vettore dei termini noti */
  for (i = 0; i < m; i++) {
    c[i] = 0;
    for (k = 0; k < n; k++) c[i] += x[i][k] * y[k];
  }

  /* risolve il sistema */
  if (Cholesky(m, A) || CholForw(m, A, c, c) || CholBack(m, A, c, c))
    retval = -3;
  else
    retval = 0;

  FreeMatrix(m, A);

  return retval;
}

/*---------------------------  Cholesky  ---------------------------------*/
/*  SCOPO                                                                 */
/*Esegue la fattorizzazione di Cholesky ( A = L*t(L) ) di una matrice     */
/*symmetrica definita positiva.                                           */
/*                                                                        */
/*   SPECIFICHE                                                           */
/* int Cholesky(int n, double **A)                                         */
/*                                                                        */
/*   PARAMETRI DI INPUT                                                   */
/* int n               numero di righe e di colonne di A                  */
/* double **A           matrice del sistema, allocata mediante la funzione */
/*                     AllocMatrix (n. 8) o AllocTriangMatrix (n. 14)     */
/*                                                                        */
/*   PARAMETRI DI OUTPUT                                                  */
/* double **A           contiene la matrice L                              */
/*                                                                        */
/*   INDICATORE DI ERRORE                                                 */
/* Il valore restituito  0 se non si sono verificati errori; pi         */
/* precisamente pu essere:                                               */
/*             0          nessun errore                                   */
/*            -1          n<1                                             */
/*             k>0        la matrice non  symmetrica def. positiva;      */
/*                        la riga k non  accettabile                     */
/*------------------------------------------------------------------------*/
int MetNum::Cholesky(int n, double **A) {
  int i, j, k;
  double acc, radq;

  if (n < 1) return -1;

  for (j = 0; j < n; j++) {
    acc = A[j][j];
    for (k = 0; k < j; k++) acc -= A[j][k] * A[j][k];

    if (acc <= 0) {
      return j + 1;
    }

    radq    = sqrt(acc);
    A[j][j] = radq;
    for (i = j + 1; i < n; i++) {
      acc = A[i][j];
      for (k  = 0; k < j; k++) acc -= A[i][k] * A[j][k];
      A[i][j] = acc / radq;
    }
  }

  return 0;
}

/*-------------------------  CholForw  ---------------------------------*/
/*  SCOPO                                                               */
/* Effettua la forward substitution su un sistema triangolare ottenuto  */
/* dalla funzione Cholesky (n. 15)                                      */
/*                                                                      */
/*   SPECIFICHE                                                         */
/* int CholForw(int n, double **A, double b[], double x[])                 */
/*                                                                      */
/*   PARAMETRI DI INPUT                                                 */
/* int n               numero di equazioni                              */
/* double **A           matrice triangolare inferiore di n righe e       */
/*                     n colonne allocata mediante la funzione          */
/*                     AllocMatrix (n. 8) o AllocTriangMatrix (n. 14)   */
/* double b[n]          vettore dei termini noti                         */
/*                                                                      */
/*   PARAMETRI DI OUTPUT                                                */
/* double x[n]          vettore delle soluzioni                          */
/*                                                                      */
/*   NOTA                                                               */
/* E' possibile passare un medesimo vettore come b e come x             */
/*                                                                      */
/*   INDICATORI DI ERRORE                                               */
/* Il valore restituito  0 se non si sono verificati errori; pi       */
/* precisamente pu essere:                                             */
/*              0          nessun errore                                */
/*             -1          n<1                                          */
/*              k>0        la riga k ha uno 0 sulla diagonale           */
/*----------------------------------------------------------------------*/
int MetNum::CholForw(int n, double **A, double b[], double x[]) {
  int i, j;

  if (n < 1) return -1;

  for (i = 0; i < n; i++) {
    if (fabs(A[i][i]) <= n * DBL_EPSILON) return i + 1;
    x[i] = b[i];
    for (j = 0; j < i; j++) x[i] -= A[i][j] * x[j];
    x[i] /= A[i][i];
  }

  return 0;
}

/*-------------------------  CholBack  ---------------------------------*/
/*  SCOPO                                                               */
/* Effettua la backward substitution su un sistema triangolare ottenuto */
/* dalla funzione Cholesky (n. 15)                                      */
/*                                                                      */
/*   SPECIFICHE                                                         */
/* int CholBack(int n, double **A, double b[], double x[])                 */
/*                                                                      */
/*   PARAMETRI DI INPUT                                                 */
/* int n               numero di equazioni                              */
/* double **A           matrice triangolare inferiore di n righe e       */
/*                     n colonne allocata mediante la funzione          */
/*                     AllocMatrix (n. 8) o AllocTriangMatrix (n. 14);  */
/*                     la matrice del sistema  la trasposta di A       */
/* double b[n]          vettore dei termini noti                         */
/*                                                                      */
/*   PARAMETRI DI OUTPUT                                                */
/* double x[n]          vettore delle soluzioni                          */
/*                                                                      */
/*   NOTA                                                               */
/* E' possibile passare un medesimo vettore come b e come x             */
/*                                                                      */
/*   INDICATORI DI ERRORE                                               */
/* Il valore restituito  0 se non si sono verificati errori; pi       */
/* precisamente pu essere:                                             */
/*              0          nessun errore                                */
/*             -1          n<1                                          */
/*              k>0        la riga k ha uno 0 sulla diagonale           */
/*----------------------------------------------------------------------*/
int MetNum::CholBack(int n, double **A, double b[], double x[]) {
  int i, j;

  if (n < 1) return -1;

  for (i = n - 1; i >= 0; i--) {
    if (fabs(A[i][i]) <= n * DBL_EPSILON) return i + 1;
    x[i] = b[i];
    for (j = i + 1; j < n; j++) x[i] -= A[j][i] * x[j];
    x[i] /= A[i][i];
  }

  return 0;
}
