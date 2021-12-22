#pragma once

#ifndef FXNODESELECTION_H
#define FXNODESELECTION_H

#include "toonzqt/selection.h"
#include "tfx.h"
#include "toonz/fxcommand.h"
#include <QList>
#include <QPair>
#include <QMap>
#include <QSet>
#include <set>

// forward declaration
class TXsheetHandle;
class TFxHandle;
class SchematicLink;
class SchematicPort;
class FxSchematicScene;

using namespace TFxCommand;

//=========================================================
//
// FxSelection
//
//---------------------------------------------------------

class FxSelection final : public QObject, public TSelection {
  Q_OBJECT

  QList<Link> m_selectedLinks;
  QList<TFxP> m_selectedFxs;
  QList<int> m_selectedColIndexes;
  TXsheetHandle *m_xshHandle;
  TFxHandle *m_fxHandle;
  TPointD m_pastePosition;

  FxSchematicScene *m_schematicScene;

public:
  FxSelection();
  FxSelection(const FxSelection &src);
  ~FxSelection();

  TSelection *clone() const;

  void setPastePosition(const TPointD &pos) { m_pastePosition = pos; }

  void enableCommands() override;

  //! Return true if the selection is empty
  bool isEmpty() const override {
    return m_selectedFxs.empty() && m_selectedLinks.empty() &&
           m_selectedColIndexes.isEmpty();
  }

  //! Empty the selection
  void selectNone() override {
    m_selectedFxs.clear();
    m_selectedLinks.clear();
    m_selectedColIndexes.clear();
  }

  //! Adds the \b fx to the m_selectedFxs container;
  void select(TFxP fx);
  //! Adds the \b colIndex to the m_selectedColIndexes container;
  void select(int colIndex);
  //! Removes the \b fx from the m_selectedFxs container;
  void unselect(TFxP fx);
  //! Removes the \b colIndex from the m_selectedColIndexes container;
  void unselect(int colIndex);
  //! Adds a QPair<TFxP,TFxP> to the m_selectedLinks container.
  //! The first element of of the QPair is the fx that has the link in input.
  //! The second element of of the QPair is the fx that has the link in output
  void select(SchematicLink *link);
  //! Removes a QPair<TFxP,TFxP> from the m_selectedLinks container.
  void unselect(SchematicLink *link);

  //! Returns true if the given \b fx is selected.
  bool isSelected(TFxP fx) const;
  //! Returns true if the given \b columnIndex is selected.
  bool isSelected(int columnIndex) const;
  //! Returns true if the QPair<TFxP,TFxP> of the fxs that bounds the given \b
  //! link is contained in the
  //! m_selectedLinks container.
  bool isSelected(SchematicLink *link);

  const QList<TFxP> &getFxs() const { return m_selectedFxs; }
  const QList<Link> &getLinks() const { return m_selectedLinks; }
  const QList<int> &getColumnIndexes() const { return m_selectedColIndexes; }

  void setXsheetHandle(TXsheetHandle *xshHandle) { m_xshHandle = xshHandle; }
  void setFxHandle(TFxHandle *fxHandle) { m_fxHandle = fxHandle; }
  //! Returns the size of the selection.
  //! The size is the number of fxs and link selected.
  int size() { return m_selectedFxs.size() + m_selectedLinks.size(); }

  // Commands

  //! Calls the TFxCommand::deleteSelection() to remove selected links or fxs.
  void deleteSelection();
  //! Copy selected fxs in the clipboard.
  void copySelection();
  //! Copy selected fxs in the clipboard and alls the
  //! TFxCommand::deleteSelection()
  //! to remove selected links or fxs.
  void cutSelection();
  //! Get fxs from the clipboards and calls the TFxCommand::pasteFxs(const
  //! QList<TFxP> &fxs, TXsheetHandle*)
  //! to insert fxs in the scene.
  void pasteSelection();

  //! Get fxs from the clipboards and calls the TFxCommand::insertPasteFxs(const
  //! QList<TFxP> &fxs, TXsheetHandle*)
  //! to insert fxs in the scene.
  //! Fxs are pasted only if they are connected!
  bool insertPasteSelection();
  //! Get fxs from the clipboards and calls the TFxCommand::addPasteFxs(const
  //! QList<TFxP> &fxs, TXsheetHandle*)
  //! to insert fxs in the scene.
  //! Fxs are pasted only if they are connected!
  bool addPasteSelection();
  //! Get fxs from the clipboards and calls the
  //! TFxCommand::replacePasteFxs(const QList<TFxP> &fxs, TXsheetHandle*)
  //! to insert fxs in the scene.
  //! Fxs are pasted only if they are connected!
  bool replacePasteSelection();

  void groupSelection();
  void ungroupSelection();
  void collapseSelection();
  void explodeChild();
  Link getBoundingFxs(SchematicLink *link);

  //! Return true if the selection is connected;
  //! A selection is connected if nodes and links selected create a connected
  //! graph
  bool isConnected();

  void setFxSchematicScene(FxSchematicScene *schematicScene) {
    m_schematicScene = schematicScene;
  }

private:
  // not implemented
  FxSelection &operator=(const FxSelection &);

  Link getBoundingFxs(SchematicPort *inputPort, SchematicPort *outputPort);

  void visitFx(TFx *fx, QList<TFx *> &visitedFxs);
  bool areLinked(TFx *outFx, TFx *inFx);

signals:
  void doCollapse(const QList<TFxP> &);
  void doExplodeChild(const QList<TFxP> &);
  void doDelete();
  void columnPasted(const QList<TXshColumnP> &);
};

#endif
