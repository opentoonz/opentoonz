#pragma once

#ifndef FUNCTIONSELECTION_H
#define FUNCTIONSELECTION_H

#include "tcommon.h"
#include "functiontreeviewer.h"
#include "toonzqt/selection.h"
#include "toonzqt/dvmimedata.h"

#include "tdoublekeyframe.h"

#include <QWidget>
#include <QList>
#include <QSet>

#undef DVAPI
#undef DVVAR
#ifdef TOONZQT_EXPORTS
#define DVAPI DV_EXPORT_API
#define DVVAR DV_EXPORT_VAR
#else
#define DVAPI DV_IMPORT_API
#define DVVAR DV_IMPORT_VAR
#endif

// forward declaration
class TDoubleParam;
class TFrameHandle;

//-----------------------------------------------------------------------------

class ColumnToCurveMapper {
public:
  virtual TDoubleParam *getCurve(int columnIndex) const = 0;
};

//-----------------------------------------------------------------------------

class FunctionSelection : public QObject, public TSelection {
  Q_OBJECT
  QRect m_selectedCells;  // yrange = rowrange of the selected keyframes;
                          // xrange = columnrange (functionsheet only)
  QList<QPair<TDoubleParam *, QSet<int>>> m_selectedKeyframes;
  // first = curve, second = set of selected keyframes index

  int m_selectedSegment;  // index of the first keyframe of the segment; -1 if
                          // no segment selected
                          // (functionpanel only)
  // assert(m_selectedSegment<0 || m_selectedKeyframes.size()==1)

  TFrameHandle *m_frameHandle;
  ColumnToCurveMapper *m_columnToCurveMapper;

  int getCurveIndex(TDoubleParam *curve) const;
  // finds i : m_selectedKeyframes[i].first == curve
  //-1 if curve not found
  int touchCurveIndex(TDoubleParam *curve);
  // as getCurve(); if curve not found then add it

public:
  FunctionSelection();
  ~FunctionSelection();

  void setFrameHandle(TFrameHandle *frameHandle) {
    m_frameHandle = frameHandle;
  }

  // function graph
  void selectCurve(TDoubleParam *curve);
  void deselectAllKeyframes();

  // function sheet
  QRect getSelectedCells() const { return m_selectedCells; }
  void selectCells(const QRect &selectedCells,
                   const QList<TDoubleParam *> &curves);
  void selectCells(const QRect &selectedCells);
  void deselectAllCells();

  bool isEmpty() const { return m_selectedKeyframes.empty(); }
  void selectNone();
  void select(TDoubleParam *curve, int k);
  bool isSelected(TDoubleParam *curve, int k) const;
  void selectSegment(TDoubleParam *, int k,
                     QRect selectedCells = QRect());  // note: if a segment is
                                                      // selected then also the
                                                      // segment ends are
                                                      // selected
  int getSelectedKeyframeCount() const;
  QPair<TDoubleParam *, int> getSelectedKeyframe(int index)
      const;  // if index<0 || index>=getSelectedKeyframeCount() returns (0,-1)

  QPair<TDoubleParam *, int> getSelectedSegment()
      const;  // if no segment is selected returns (0,-1)
  bool isSegmentSelected(TDoubleParam *, int k) const;

  void setColumnToCurveMapper(ColumnToCurveMapper *mapper);  // gets ownership

  TDoubleParam *getCurveFromColumn(int columnIndex) const {
    return m_columnToCurveMapper ? m_columnToCurveMapper->getCurve(columnIndex)
                                 : 0;
  }

  void enableCommands();

  void doCopy();
  void doPaste();
  void doCut();
  void doDelete();
  void insertCells();

signals:
  void selectionChanged();
};

//-----------------------------------------------------------------------------

class FunctionKeyframesData : public DvMimeData {
public:
  FunctionKeyframesData();
  ~FunctionKeyframesData();

  typedef std::vector<TDoubleKeyframe> Keyframes;

  void setColumnCount(int columnCount);
  int getColumnCount() const { return (int)m_keyframes.size(); }

  int getRowCount() const;

  void getData(int columnIndex, TDoubleParam *curve, double frame,
               const QSet<int> &kIndices);
  void setData(int columnIndex, TDoubleParam *curve, double frame) const;

  const Keyframes &getKeyframes(int columnIndex) const;

  DvMimeData *clone() const;

  bool isCircularReferenceFree(int columnIndex, TDoubleParam *curve) const;

private:
  std::vector<Keyframes> m_keyframes;
};

#endif
