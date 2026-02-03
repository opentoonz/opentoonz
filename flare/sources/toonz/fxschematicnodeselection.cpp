

#include "fxschematicnodeselection.h"
#include "fxschematicnode.h"
#include "tapp.h"
#include "toonz/tfxhandle.h"
#include "menubarcommandids.h"
#include "toonz/tcolumnfx.h"
#include "fxcommand.h"

#include "tundo.h"
#include "toonz/fxdag.h"
#include "toonz/txsheethandle.h"
#include "toonz/txsheet.h"
#include "toonz/tcolumnfxset.h"

//=========================================================
//
// DeleteFxsUndo
//
//=========================================================

class DeleteFxsUndo : public TUndo {
  struct Node {
    TFxP m_fx;
    std::vector<std::pair<int, TFxP>> m_outputLinks;
    std::vector<TFxP> m_inputLinks;
    bool m_xsheetConnected;
  };
  QList<Node> m_fxs;
  QList<TFxP> m_inputConnectedToXsheet;

public:
  DeleteFxsUndo(const QList<TFx *> &fxs) {
    TApp *app    = TApp::instance();
    FxDag *fxDag = app->getCurrentXsheet()->getXsheet()->getFxDag();
    for (int i = 0; i < (int)fxs.size(); i++) {
      TFx *fx              = fxs[i];
      TZeraryColumnFx *zfx = dynamic_cast<TZeraryColumnFx *>(fx);
      if (zfx) fx          = zfx->getZeraryFx();
      Node node;
      node.m_fx              = fx;
      node.m_xsheetConnected = fxDag->getTerminalFxs()->containsFx(fx);
      int j;
      for (j = 0; j < fx->getInputPortCount(); j++) {
        TFxP inputFx(fx->getInputPort(j)->getFx());
        int i;
        if (inputFx &&
            !fxDag->getTerminalFxs()->containsFx(inputFx.getPointer())) {
          for (i = 0; i < (int)m_inputConnectedToXsheet.size(); i++)
            if (m_inputConnectedToXsheet[i].getPointer() ==
                inputFx.getPointer())
              break;
          if (i == (int)m_inputConnectedToXsheet.size())
            m_inputConnectedToXsheet.push_back(inputFx);
        }
        node.m_inputLinks.push_back(inputFx);
      }
      for (j = 0; j < fx->getOutputConnectionCount(); j++) {
        TFxPort *port = fx->getOutputConnection(j);
        TFx *outFx    = port->getOwnerFx();
        if (outFx) {
          int k;
          for (k = 0; k < outFx->getInputPortCount(); k++)
            if (outFx->getInputPort(k)->getFx() == fx) break;
          if (k < outFx->getInputPortCount())
            node.m_outputLinks.push_back(std::make_pair(k, TFxP(outFx)));
        }
      }
      m_fxs.push_back(node);
    }
  }

  void onAdd() {
    FxDag *fxDag =
        TApp::instance()->getCurrentXsheet()->getXsheet()->getFxDag();
    int i;
    for (i = 0; i < (int)m_inputConnectedToXsheet.size();) {
      TFx *inputFx = m_inputConnectedToXsheet[i].getPointer();
      if (!fxDag->getTerminalFxs()->containsFx(inputFx))
        i++;
      else
        m_inputConnectedToXsheet.erase(m_inputConnectedToXsheet.begin() + i);
    }
  }

  void undo() const {
    FxDag *fxDag =
        TApp::instance()->getCurrentXsheet()->getXsheet()->getFxDag();
    int i, j;
    for (i = 0; i < (int)m_fxs.size(); i++) {
      TApp::instance()->getCurrentFx()->setFx(m_fxs[i].m_fx.getPointer());
      TFxCommand::addFx(m_fxs[i].m_fx.getPointer());
    }
    for (i = 0; i < (int)m_fxs.size(); i++) {
      const Node &node = m_fxs[i];
      TFx *fx          = node.m_fx.getPointer();
      if (node.m_xsheetConnected) fxDag->addToXsheet(fx);
      for (j = 0; j < (int)node.m_inputLinks.size(); j++)
        fx->getInputPort(j)->setFx(node.m_inputLinks[j].getPointer());
      for (j = 0; j < (int)node.m_outputLinks.size(); j++) {
        TFx *outFx = node.m_outputLinks[j].second.getPointer();
        outFx->getInputPort(node.m_outputLinks[j].first)->setFx(fx);
      }
    }
    for (i = 0; i < (int)m_inputConnectedToXsheet.size(); i++) {
      TFx *inputFx = m_inputConnectedToXsheet[i].getPointer();
      fxDag->getTerminalFxs()->removeFx(inputFx);
    }
    TApp::instance()->getCurrentXsheet()->notifyXsheetChanged();
  }
  void redo() const {
    int i;
    for (i = 0; i < (int)m_fxs.size(); i++) {
      TFxCommand::removeFx(m_fxs[i].m_fx.getPointer());
    }
    TApp::instance()->getCurrentXsheet()->notifyXsheetChanged();
  }
  int getSize() const {
    return sizeof(*this) + m_fxs.size() * (sizeof(TFx) + 100);
  }
};

//=========================================================
//
// FxSchematicNodeSelection
//
//=========================================================

FxSchematicNodeSelection::FxSchematicNodeSelection() {}

//---------------------------------------------------------

FxSchematicNodeSelection::FxSchematicNodeSelection(
    const FxSchematicNodeSelection &src)
    : m_selectedNodes(src.m_selectedNodes) {}

//---------------------------------------------------------

FxSchematicNodeSelection::~FxSchematicNodeSelection() {}

//---------------------------------------------------------

void FxSchematicNodeSelection::enableCommands() {
  enableCommand(this, MI_Clear, &FxSchematicNodeSelection::deleteSelection);
}

//---------------------------------------------------------

TSelection *FxSchematicNodeSelection::clone() const {
  assert(0);
  return new FxSchematicNodeSelection();
}

//---------------------------------------------------------

void FxSchematicNodeSelection::select(FxSchematicNode *node) {
  m_selectedNodes.append(node);
}

//---------------------------------------------------------

void FxSchematicNodeSelection::unselect(FxSchematicNode *node) {
  int index = m_selectedNodes.indexOf(node);
  if (index >= 0) m_selectedNodes.removeAt(index);
  TFxHandle *fxHandle = TApp::instance()->getCurrentFx();
  if (fxHandle->getFx() == node->getFx()) {
    if (m_selectedNodes.empty())
      fxHandle->setFx(0);
    else
      fxHandle->setFx((*m_selectedNodes.begin())->getFx());
  }
}

//---------------------------------------------------------

bool FxSchematicNodeSelection::isSelected(FxSchematicNode *node) const {
  return m_selectedNodes.contains(node);
}

//---------------------------------------------------------

void FxSchematicNodeSelection::deleteSelection() {
  QList<TFx *> fxs;
  QList<FxSchematicNode *>::const_iterator it;
  for (it = m_selectedNodes.begin(); it != m_selectedNodes.end(); ++it) {
    TFx *fx = (*it)->getFx();
    if (0 != dynamic_cast<TXsheetFx *>(fx) ||
        0 != dynamic_cast<TLevelColumnFx *>(fx))
      continue;
    fxs.push_back((*it)->getFx());
  }
  selectNone();
  removeFxs(fxs);
}

//---------------------------------------------------------

void FxSchematicNodeSelection::removeFxs(const QList<TFx *> &fxs) {
  TUndoManager::manager()->add(new DeleteFxsUndo(fxs));
  for (int i = 0; i < (int)fxs.size(); i++) TFxCommand::removeFx(fxs[i]);
  TApp::instance()->getCurrentXsheet()->notifyXsheetChanged();
}
