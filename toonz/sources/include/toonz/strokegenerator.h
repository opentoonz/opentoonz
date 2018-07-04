#pragma once

#ifndef MOUSETRACKER_INCLUDED
#define MOUSETRACKER_INCLUDED

#include "tcommon.h"
class TStroke;

#undef DVAPI
#undef DVVAR
#ifdef TOONZLIB_EXPORTS
#define DVAPI DV_EXPORT_API
#define DVVAR DV_EXPORT_VAR
#else
#define DVAPI DV_IMPORT_API
#define DVVAR DV_IMPORT_VAR
#endif

//===============================================================
//! Genera degli stroke
/*!
  Genera degli stroke sulla base di un vettore di punti,
        questo vettore e' fatto di punti che vengono inseriti.
        Consente di visualizzare dei frammenti fatti dai punti che acquisisce.
*/
class DVAPI StrokeGenerator {
  //! Vettore di TThickPoint
  std::vector<TThickPoint> m_points;

  //! Numero dei punti visualizzati
  int m_paintedPointCount;

  //! Rettangolo che contiene la regione modificata
  TRectD m_modifiedRegion;

  //! Rettangolo che contiene l'ultima regione modificata
  TRectD m_lastModifiedRegion;
  TRectD m_lastPointRect;

  //! Ultimo punto del frammento visualizzato
  TPointD m_p0, /*! Ultimo punto del frammento visualizzato*/ m_p1;

  //! Visualizza i frammenti
  /*!
  \param first Indice del primo punto di m_points da visualizzare
\param last  Indice dell'ulimo punto di m_points da visualizzare
  */
  void drawFragments(int first, int last);

public:
  //! Costruttore
  StrokeGenerator() : m_paintedPointCount(0) {}

  //! Distruttore
  ~StrokeGenerator() {}

  //! Libera le variabili della classe
  void clear();

  //! Restituisce true se m_points e' vuoto, false altrimenti.
  bool isEmpty() const;

  //! Aggiunge un TThickPoint al vettore m_points
  /*!
    Il punto viene aggiunto se la sua distanza dal punto precedente e' maggiore
    di 4*pixelSize2

    \param point      TThickPoint da aggiungere al vettore
    \param pixelSize2 Dimensione pixel
  */
  void add(const TThickPoint &point, double pixelSize2);

  TPointD getFirstPoint();  // returns the first point

  //! Filtra i punti di m_points
  /*!
    Verifica se i primi sei e gli ultimi sei punti successivi hanno una
variazione di thickness elevata.
In caso affermativo li cancella.
  */
  void filterPoints();

  //! Visualizza l'ultimo frammento
  void drawLastFragments();

  //! Visualizza tutti i frammenti
  void drawAllFragments();

  // Only keep first and last points. Used for straight lines
  void removeMiddlePoints();

  //! Restituisce il rettangolo che contiene la regione modificata
  TRectD getModifiedRegion() const;

  //! Restituisce il rettangolo che contiene l'ultima regione modificata
  TRectD getLastModifiedRegion();

  // if onlyLastPoints is not 0, create the stroke only on last 'onlyLastPoints'
  // points
  //! Genera uno stroke usando TStroke::interpolate
  /*!
    Se onlyLastPoints e' uguale a 0 genera lo stroke sulla base di m_points.
Se onlyLastPoints e' diverso da zero genera lo stroke sulla base degli ultimi
onlyLastPoint elementi di m_points

          \param error          Errore che viene passato a TStroke::interpolate
          \param onlyLastPoints Numero elementi sulla base dei quali creare la
stroke
  */
  TStroke *makeStroke(double error, UINT onlyLastPoints = 0) const;
};

//===============================================================

#endif
