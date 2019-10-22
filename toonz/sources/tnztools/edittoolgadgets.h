#pragma once

#ifndef _EDITTOOLGADGETS_H_
#define _EDITTOOLGADGETS_H_

// TnzCore includes
#include "tgeometry.h"
#include "tgl.h"

// TnzBase includes
#include "tparamchange.h"
#include "tdoubleparam.h"
#include "tparamset.h"

// Qt includes
#include <QObject>

//=================================================

//    Forward declarations

class TMouseEvent;
class TFxHandle;
class TTool;
class FxParamsGraphicEditor;

class FxGadgetController;
class TUndo;
class TParamUIConcept;

//=================================================

class FxGadget : public TParamObserver {
  GLuint m_id;

  std::vector<TDoubleParamP> m_params;
  double m_pixelSize;
  std::string m_label;
  TUndo *m_undo;
  double m_scaleFactor;

protected:
  FxGadgetController *m_controller;
  int m_handleCount = 1;
  // -1 : not selected, 0~ : handle id
  int m_selected;

public:
  static GLdouble m_selectedColor[3];  // rgb

  FxGadget(FxGadgetController *controller, int handleCount = 1);
  virtual ~FxGadget();

  void setLabel(std::string label) { m_label = label; }
  std::string getLabel() const { return m_label; }

  FxGadgetController *getController() const { return m_controller; }

  void addParam(const TDoubleParamP &param);
  double getValue(const TDoubleParamP &param) const;
  void setValue(const TDoubleParamP &param, double value);

  TPointD getValue(const TPointParamP &param) const;
  void setValue(const TPointParamP &param, const TPointD &pos);

  void setId(GLuint id) { m_id = id; }
  GLuint getId() const { return m_id; }

  int getHandleCount() { return m_handleCount; }

  void select(int selected) { m_selected = selected; }
  bool isSelected() const { return m_selected >= 0; }
  bool isSelected(int id) const { return m_selected == id; }

  void setPixelSize();  // uses tglGetPixelSize2()
  void setPixelSize(double pixelSize) { m_pixelSize = pixelSize; }
  inline double getPixelSize() const { return m_pixelSize; }

  void drawDot(const TPointD &pos);
  void drawDot(double x, double y) { drawDot(TPointD(x, y)); }

  void drawTooltip(const TPointD &tooltipPos, std::string tooltipPosText);

  virtual void draw(bool picking) = 0;

  virtual void leftButtonDown(const TPointD &pos, const TMouseEvent &) {}
  virtual void leftButtonDrag(const TPointD &pos, const TMouseEvent &) {}
  virtual void leftButtonUp(const TPointD &pos, const TMouseEvent &) {}

  void onChange(const TParamChange &) override;
  void createUndo();
  void commitUndo();

  void setScaleFactor(double s) { m_scaleFactor = s; }
  double getScaleFactor() const { return m_scaleFactor; }
};

namespace EditToolGadgets {

//=============================================================================
// DragTool
//-----------------------------------------------------------------------------

class DragTool {
public:
  virtual void leftButtonDown(const TPointD &pos, const TMouseEvent &) = 0;
  virtual void leftButtonDrag(const TPointD &pos, const TMouseEvent &) = 0;
  virtual void leftButtonUp(const TPointD &pos, const TMouseEvent &)   = 0;
  virtual ~DragTool() {}

  virtual void enableGlobalKeyframes(bool enabled) {}
};

//-----------------------------------------------------------------------------

}  // namespace EditToolGadgets

class FxGadgetController final : public QObject {
  Q_OBJECT

  TTool *m_tool;
  TFxHandle *m_fxHandle;
  std::vector<FxGadget *> m_gadgets;
  unsigned long m_idBase, m_nextId;
  std::map<GLuint, FxGadget *> m_idTable;
  FxGadget *m_selectedGadget;
  bool m_editingNonZeraryFx;

public:
  FxGadgetController(TTool *tool);
  ~FxGadgetController();

  void clearGadgets();

  TAffine getMatrix();

  void draw(bool picking = false);

  void selectById(unsigned int id);

  EditToolGadgets::DragTool *createDragTool(int gadgetId);

  void assignId(FxGadget *gadget);
  void addGadget(FxGadget *gadget);

  int getCurrentFrame() const;
  void invalidateViewer();

  bool isEditingNonZeraryFx() const { return m_editingNonZeraryFx; }

  bool hasGadget() { return m_gadgets.size() != 0; }

public slots:

  void onFxSwitched();

private:
  FxGadget *allocateGadget(const TParamUIConcept &uiConcept);
};

#endif
