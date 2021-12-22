

#include "filmstripselection.h"

// Tnz6 includes
#include "menubarcommandids.h"
#include "filmstripcommand.h"
#include "addfilmstripframespopup.h"
#include "renumberpopup.h"
#include "tapp.h"

// TnzTools includes
#include "tools/rasterselection.h"
#include "tools/strokeselection.h"
#include "tools/toolhandle.h"
#include "tools/tool.h"

// TnzQt includes
#include "toonzqt/strokesdata.h"
#include "toonzqt/gutil.h"

// TnzLib includes
#include "toonz/txshlevelhandle.h"
#include "toonz/tframehandle.h"
#include "toonz/txshlevel.h"
#include "toonz/txshsimplelevel.h"
#include "toonz/txshleveltypes.h"

// TnzCore includes
#include "tundo.h"

// Qt includes
#include <QApplication>
#include <QClipboard>

//-----------------------------------------------------------------------------

TFilmstripSelection::TFilmstripSelection()
    : m_inbetweenRange(TFrameId(1), TFrameId(0)) {}

//-----------------------------------------------------------------------------

TFilmstripSelection::~TFilmstripSelection() {}

//-----------------------------------------------------------------------------

void TFilmstripSelection::enableCommands() {
  TXshSimpleLevel *sl = TApp::instance()->getCurrentLevel()->getSimpleLevel();
  if (!sl) return;

  enableCommand(this, MI_SelectAll, &TFilmstripSelection::selectAll);
  enableCommand(this, MI_InvertSelection,
                &TFilmstripSelection::invertSelection);

  int type       = sl->getType();
  TFilePath path = sl->getPath();

  bool doEnable =
      (type == PLI_XSHLEVEL || type == TZP_XSHLEVEL || type == MESH_XSHLEVEL ||
       (type == OVL_XSHLEVEL && path.getType() != "psd" &&
        path.getType() != "gif" && path.getType() != "mp4" &&
        path.getType() != "webm"));

  TRasterImageP ri = (TRasterImageP)sl->getSimpleLevel()->getFrame(
      sl->getSimpleLevel()->getFirstFid(), false);

  bool isNotEditableFullColorLevel =
      ((type == OVL_XSHLEVEL && path.getFrame() == TFrameId::NO_FRAME) ||
       (ri && ri->isScanBW()));

  if (doEnable && !isNotEditableFullColorLevel) {
    enableCommand(this, MI_Cut, &TFilmstripSelection::cutFrames);
    enableCommand(this, MI_Copy, &TFilmstripSelection::copyFrames);
    enableCommand(this, MI_Paste, &TFilmstripSelection::pasteFrames);
    enableCommand(this, MI_PasteInto, &TFilmstripSelection::pasteInto);
    enableCommand(this, MI_Clear, &TFilmstripSelection::deleteFrames);
    enableCommand(this, MI_Reverse, &TFilmstripSelection::reverseFrames);
    enableCommand(this, MI_Swing, &TFilmstripSelection::swingFrames);
    enableCommand(this, MI_Step2, &TFilmstripSelection::stepFrames, 2);
    enableCommand(this, MI_Step3, &TFilmstripSelection::stepFrames, 3);
    enableCommand(this, MI_Step4, &TFilmstripSelection::stepFrames, 4);
    enableCommand(this, MI_Each2, &TFilmstripSelection::eachFrames, 2);
    enableCommand(this, MI_Each3, &TFilmstripSelection::eachFrames, 3);
    enableCommand(this, MI_Each4, &TFilmstripSelection::eachFrames, 4);
    enableCommand(this, MI_Duplicate, &TFilmstripSelection::duplicateFrames);

    if (type != MESH_XSHLEVEL) {
      enableCommand(this, MI_Insert, &TFilmstripSelection::insertEmptyFrames);
      enableCommand(this, MI_MergeFrames, &TFilmstripSelection::mergeFrames);

      enableCommand(this, MI_AddFrames, &TFilmstripSelection::addFrames);
    }
  } else if (isNotEditableFullColorLevel)
    enableCommand(this, MI_Copy, &TFilmstripSelection::copyFrames);

  enableCommand(this, MI_ExposeResource, &TFilmstripSelection::exposeFrames);

  if (doEnable && !isNotEditableFullColorLevel)
    enableCommand(this, MI_Renumber, &TFilmstripSelection::renumberFrames);
}

//-----------------------------------------------------------------------------

bool TFilmstripSelection::isEmpty() const { return m_selectedFrames.empty(); }

//-----------------------------------------------------------------------------

void TFilmstripSelection::updateInbetweenRange() {
  // ibrange = (la prima) sequenza di almeno tre frame selezionati consecutivi
  m_inbetweenRange = std::make_pair(TFrameId(1), TFrameId(0));
  if (m_selectedFrames.size() < 3)
    return;  // ci vogliono almeno tre frames selezionati
  TXshSimpleLevel *sl = TApp::instance()->getCurrentLevel()->getSimpleLevel();
  if (sl) {
    std::vector<TFrameId> fids;
    sl->getFids(fids);
    int m = (int)fids.size();
    int i, j = -1;
    for (i = 0; i < (int)fids.size(); i++)
      if (isSelected(fids[i])) {
        if (j < 0) j = i;
      } else {
        if (j >= 0 && i - j >= 3) break;
        j = -1;
      }
    if (j >= 0 && i - j >= 3) {
      m_inbetweenRange = std::make_pair(fids[j], fids[i - 1]);
    }
  }
}

//-----------------------------------------------------------------------------

void TFilmstripSelection::select(const TFrameId &fid, bool selected) {
  TApp *app = TApp::instance();

  if (selected)
    m_selectedFrames.insert(fid);
  else
    m_selectedFrames.erase(fid);

  updateInbetweenRange();

  TTool *tool = app->getCurrentTool()->getTool();
  if (tool) tool->setSelectedFrames(m_selectedFrames);

  TXshSimpleLevel *sl = app->getCurrentLevel()->getSimpleLevel();
  bool rasterLevel    = sl->getType() == TZP_XSHLEVEL ||
                     sl->getType() == OVL_XSHLEVEL ||
                     sl->getType() == TZI_XSHLEVEL;

  CommandManager::instance()->enable(MI_CanvasSize, rasterLevel);
}

//-----------------------------------------------------------------------------

bool TFilmstripSelection::isSelected(const TFrameId &fid) const {
  return m_selectedFrames.count(fid) > 0;
}

//-----------------------------------------------------------------------------

void TFilmstripSelection::selectNone() {
  m_selectedFrames.clear();
  updateInbetweenRange();
  TXshSimpleLevel *sl = TApp::instance()->getCurrentLevel()->getSimpleLevel();
  CommandManager::instance()->enable(MI_CanvasSize, false);
}

//-----------------------------------------------------------------------------

void TFilmstripSelection::selectAll() {
  TXshSimpleLevel *sl = TApp::instance()->getCurrentLevel()->getSimpleLevel();
  if (!sl) return;
  std::vector<TFrameId> fids;
  sl->getFids(fids);
  m_selectedFrames.clear();
  m_selectedFrames.insert(fids.begin(), fids.end());
  updateInbetweenRange();
  TTool *tool = TApp::instance()->getCurrentTool()->getTool();
  tool->setSelectedFrames(m_selectedFrames);
  bool rasterLevel = sl->getType() == TZP_XSHLEVEL ||
                     sl->getType() == OVL_XSHLEVEL ||
                     sl->getType() == TZI_XSHLEVEL;
  CommandManager::instance()->enable(MI_CanvasSize, rasterLevel);
  notifyView();
}

//-----------------------------------------------------------------------------

void TFilmstripSelection::invertSelection() {
  TXshSimpleLevel *sl = TApp::instance()->getCurrentLevel()->getSimpleLevel();
  if (!sl) return;
  std::vector<TFrameId> fids;
  sl->getFids(fids);
  std::set<TFrameId> oldSelectedFrames = m_selectedFrames;
  m_selectedFrames.clear();
  std::vector<TFrameId>::iterator it;
  for (it = fids.begin(); it != fids.end(); it++) {
    if (oldSelectedFrames.find(*it) != oldSelectedFrames.end()) continue;
    m_selectedFrames.insert(*it);
  }
  updateInbetweenRange();
  TTool *tool = TApp::instance()->getCurrentTool()->getTool();
  tool->setSelectedFrames(m_selectedFrames);
  if (sl->getType() == TZP_XSHLEVEL || sl->getType() == OVL_XSHLEVEL ||
      sl->getType() == TZI_XSHLEVEL)
    CommandManager::instance()->enable(MI_CanvasSize, true);
  notifyView();
}

//-----------------------------------------------------------------------------

void TFilmstripSelection::addFrames() {
  TXshSimpleLevel *sl = TApp::instance()->getCurrentLevel()->getSimpleLevel();
  if (!sl) return;
  AddFilmstripFramesPopup popup;
  int ret = popup.exec();
  if (ret == 0) return;

  int startFrame = 1;
  int endFrame   = 1;
  int stepFrame  = 1;

  popup.getParameters(startFrame, endFrame, stepFrame);
  FilmstripCmd::addFrames(sl, startFrame, endFrame, stepFrame);
}

//-----------------------------------------------------------------------------

void TFilmstripSelection::copyFrames() {
  TXshSimpleLevel *sl = TApp::instance()->getCurrentLevel()->getSimpleLevel();
  if (sl) FilmstripCmd::copy(sl, m_selectedFrames);
}

//-----------------------------------------------------------------------------

void TFilmstripSelection::cutFrames() {
  TXshSimpleLevel *sl = TApp::instance()->getCurrentLevel()->getSimpleLevel();
  if (sl) {
    int firstSelectedIndex = sl->fid2index(*m_selectedFrames.begin());
    assert(firstSelectedIndex >= 0);
    FilmstripCmd::cut(sl, m_selectedFrames);
    selectNone();
    TFrameId fId = (firstSelectedIndex == 0)
                       ? sl->getFirstFid()
                       : sl->getFrameId(firstSelectedIndex - 1);
    TApp::instance()->getCurrentFrame()->setFid(fId);
    select(fId);
  }
}

//-----------------------------------------------------------------------------

void TFilmstripSelection::pasteFrames() {
  TXshSimpleLevel *sl = TApp::instance()->getCurrentLevel()->getSimpleLevel();
  if (!sl) return;

  std::set<TFrameId> fids;
  if (m_selectedFrames.empty()) {
    if (sl->isSubsequence()) return;
    fids.insert(TApp::instance()->getCurrentFrame()->getFid());
  } else
    fids = m_selectedFrames;

  FilmstripCmd::paste(sl, fids);
}

//-----------------------------------------------------------------------------

void TFilmstripSelection::mergeFrames() {
  TXshSimpleLevel *sl = TApp::instance()->getCurrentLevel()->getSimpleLevel();
  if (sl) FilmstripCmd::merge(sl, m_selectedFrames);
}

//-----------------------------------------------------------------------------

void TFilmstripSelection::pasteInto() {
  TXshSimpleLevel *sl = TApp::instance()->getCurrentLevel()->getSimpleLevel();
  if (sl) FilmstripCmd::pasteInto(sl, m_selectedFrames);
}

//-----------------------------------------------------------------------------

void TFilmstripSelection::deleteFrames() {
  TXshSimpleLevel *sl = TApp::instance()->getCurrentLevel()->getSimpleLevel();
  if (sl) FilmstripCmd::clear(sl, m_selectedFrames);
  updateInbetweenRange();
}

//-----------------------------------------------------------------------------

void TFilmstripSelection::insertEmptyFrames() {
  TXshSimpleLevel *sl = TApp::instance()->getCurrentLevel()->getSimpleLevel();
  if (sl) FilmstripCmd::insert(sl, m_selectedFrames, true);
}

//-----------------------------------------------------------------------------

void TFilmstripSelection::reverseFrames() {
  TXshSimpleLevel *sl = TApp::instance()->getCurrentLevel()->getSimpleLevel();
  if (sl) FilmstripCmd::reverse(sl, m_selectedFrames);
}

//-----------------------------------------------------------------------------

void TFilmstripSelection::swingFrames() {
  TXshSimpleLevel *sl = TApp::instance()->getCurrentLevel()->getSimpleLevel();
  if (sl) FilmstripCmd::swing(sl, m_selectedFrames);
}

//-----------------------------------------------------------------------------

void TFilmstripSelection::stepFrames(int step) {
  if (step < 2) return;
  TXshSimpleLevel *sl = TApp::instance()->getCurrentLevel()->getSimpleLevel();
  if (sl) FilmstripCmd::step(sl, m_selectedFrames, step);
  updateInbetweenRange();
}

//-----------------------------------------------------------------------------

void TFilmstripSelection::eachFrames(int each) {
  if (each < 2) return;
  TXshSimpleLevel *sl = TApp::instance()->getCurrentLevel()->getSimpleLevel();
  if (sl) FilmstripCmd::each(sl, m_selectedFrames, each);
  updateInbetweenRange();
}

//-----------------------------------------------------------------------------

void TFilmstripSelection::duplicateFrames() {
  TXshSimpleLevel *sl = TApp::instance()->getCurrentLevel()->getSimpleLevel();
  if (sl) FilmstripCmd::duplicate(sl, m_selectedFrames, true);
}

//-----------------------------------------------------------------------------

void TFilmstripSelection::exposeFrames() {
  TXshSimpleLevel *sl = TApp::instance()->getCurrentLevel()->getSimpleLevel();
  if (sl) FilmstripCmd::moveToScene(sl, m_selectedFrames);
}

//-----------------------------------------------------------------------------

void TFilmstripSelection::renumberFrames() {
  TXshSimpleLevel *sl = TApp::instance()->getCurrentLevel()->getSimpleLevel();
  if (!sl) return;
  if (m_selectedFrames.empty()) return;
  TFrameId fid = *m_selectedFrames.begin();
  RenumberPopup popup;
  popup.setValues(fid.getNumber(), 1);
  int ret = popup.exec();
  if (ret != QDialog::Accepted) return;
  int startFrame = 0, stepFrame = 0;
  popup.getValues(startFrame, stepFrame);
  if (startFrame < 1 || stepFrame < 1) {
    DVGui::error(("Bad renumber values"));
    return;
  }
  FilmstripCmd::renumber(sl, m_selectedFrames, startFrame, stepFrame);
  updateInbetweenRange();
}
