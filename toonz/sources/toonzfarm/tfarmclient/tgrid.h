#pragma once

#ifndef TGRID_H
#define TGRID_H

#include "tcolumnset.h"
#include "tw/tw.h"
#include "tw/scrollbar.h"

//-------------------------------------------------------------------
//-------------------------------------------------------------------
//-------------------------------------------------------------------
//-------------------------------------------------------------------

class TGenericGridAction {
public:
  virtual ~TGenericGridAction() {}
  virtual void sendCommand(int item) = 0;
};

//-------------------------------------------------------------------

template <class T>
class TGridAction : public TGenericGridAction {
public:
  typedef void (T::*Method)(int item);
  TGridAction(T *target, Method method) : m_target(target), m_method(method) {}
  void sendCommand(int item) { (m_target->*m_method)(item); }

private:
  T *m_target;
  Method m_method;
};

//-------------------------------------------------------------------

class TGridCell {
public:
  TGridCell() {}
  TGridCell(const string &text) : m_text(text) {}

  string m_text;
};

//-------------------------------------------------------------------

class TGridColumn : public TColumnHeader {
public:
  enum Alignment { LeftAlignment, CenterAlignment, RightAlignment };

  TGridColumn(const string &name = "", Alignment alignment = CenterAlignment);
  ~TGridColumn();

  static TGridColumn *createEmpty();

  int getRowCount() const;

  const TGridCell &getCell(int row) const;
  void setCell(int row, const TGridCell &cell);
  void setCell(int row, const string &text) { setCell(row, TGridCell(text)); }

  void getCells(int row, int rowCount, TGridCell cells[]);
  void setCells(int row, int rowCount, const TGridCell cells[]);

  void insertEmptyCells(int row, int rowCount = 1);
  void removeCells(int row, int rowCount = 1);
  void clearCells(int row, int rowCount = 1);

  string getName() const;
  Alignment getAlignment() const;

private:
  class Imp;
  Imp *m_imp;
};

typedef TSmartPointerT<TGridColumn> TGridColumnP;

//-------------------------------------------------------------------
//-------------------------------------------------------------------

class TGrid : public TWidget {
public:
  TGrid(TWidget *parent, string name = "grid");
  ~TGrid();

  void addColumn(const string &name, int width,
                 TGridColumn::Alignment align = TGridColumn::CenterAlignment);

  void addRow();
  void removeRow(int row);

  int getColIndex(const string &colName);

  void setCell(int row, int col, const string &text);
  string getCell(int row, int col);

  int getColCount() const;
  int getRowCount() const;

  int getSelectedRowCount() const;
  int getSelectedRowIndex(
      int i) const;  // returns the index of the i-th item selected

  void select(int row, bool on);
  void unselectAll();

  bool isSelected(int row) const;

  void setSelAction(TGenericGridAction *action);
  void setDblClickAction(TGenericGridAction *action);

  void draw();
  void configureNotify(const TDimension &d);

  void leftButtonDown(const TMouseEvent &e);
  void leftButtonDrag(const TMouseEvent &e);
  void leftButtonUp(const TMouseEvent &e);

  /*
void leftButtonDoubleClick(const TMouseEvent &e);
void keyDown(int key, unsigned long mod, const TPoint &);
*/
  void scrollTo(int y);

private:
  class Data;
  Data *m_data;
};

#endif
