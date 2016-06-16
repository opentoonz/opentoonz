#pragma once

/*-----------------------------------------------------
 * Header della libreria metnum.cpp
 * Alcuni algoritmi di Metodi Numerici
 * Autore: P. Foggia
 *--------------------------------------------------*/

#ifndef METNUM_H
#define METNUM_H

/*----------------  prototipi di funzioni  ---------------------*/

namespace MetNum {

double **AllocMatrix(int n, int m);
void FreeMatrix(int n, double **A);
double **AllocTriangMatrix(int n);

int Approx(int n, int m, double **x, double y[], double c[]);

int Cholesky(int n, double **A);
int CholForw(int n, double **A, double b[], double x[]);
int CholBack(int n, double **A, double b[], double x[]);
}

#endif
