

// Toonz app and handles
#include "tapp.h"
#include "toonz/tpalettehandle.h"
#include "toonz/palettecontroller.h"
#include "toonz/tobjecthandle.h"
#include "toonz/tscenehandle.h"
#include "toonz/tframehandle.h"
#include "toonz/txshlevelhandle.h"
#include "toonz/txsheethandle.h"
#include "toonz/tfxhandle.h"

// Toonz scene
#include "toonz/toonzscene.h"

// Toonz schematic includes
#include "toonz/fxdag.h"
#include "toonz/tcolumnfxset.h"
#include "toonz/tcolumnfx.h"
#include "toonz/txshlevelcolumn.h"
#include "toonz/levelset.h"
#include "toonz/txshchildlevel.h"
#include "toonz/txshzeraryfxcolumn.h"

// Render cache includes
#include "tpassivecachemanager.h"
#include "tcacheresourcepool.h"

#include "cachefxcommand.h"

//******************************************************************************************
//    Preliminaries
//******************************************************************************************

namespace {
void buildNodeTreeDescription(std::string &desc, const TFxP &root);
void enableCacheOnAllowedChildren(TRasterFx *fx);

//-----------------------------------------------------------------------------------

void traceMaximalAncestors(TFx *fx, std::vector<TFx *> &ancestors) {
  // Retrieve the output port
  int outputPortsCount = fx->getOutputConnectionCount();
  if (outputPortsCount == 0) ancestors.push_back(fx);

  for (int i = 0; i < outputPortsCount; ++i) {
    TFx *outFx = fx->getOutputConnection(i)->getOwnerFx();  // Owner fx??
    if (outFx) traceMaximalAncestors(outFx, ancestors);
  }
}

//-----------------------------------------------------------------------------------

void enableCacheOnAllowedChildren(TRasterFx *fx) {
  int portsCount = fx->getInputPortCount();
  if (portsCount == 0) {
    // It's a Column Fx. In this case, zeraries may still have descendants,
    // in their inner fx.
    TZeraryColumnFx *zcolfx = dynamic_cast<TZeraryColumnFx *>(fx);
    if (zcolfx) fx          = static_cast<TRasterFx *>(zcolfx->getZeraryFx());
  }

  if (fx->isCacheEnabled()) return;  // Already done

  fx->enableCache(true);

  for (int i = 0; i < portsCount; ++i) {
    if (fx->allowUserCacheOnPort(i)) {
      TRasterFx *childFx =
          dynamic_cast<TRasterFx *>(fx->getInputPort(i)->getFx());

      if (childFx && !childFx->isCacheEnabled())
        enableCacheOnAllowedChildren(childFx);
    }
  }
}
}  // local namespace

//******************************************************************************************
//    CacheFxCommand implementation
//******************************************************************************************

CacheFxCommand::CacheFxCommand() {
  TApp *app = TApp::instance();
  bool ret  = true;

  // Connect the variuos handles to the associated function in the
  // CacheFxCommand singleton.
  // Please observe that these connections are performed BEFORE rendering
  // classes are instanced -
  // which is needed to ensure that the proper cache updates have already been
  // performed when
  // rendering instaces receive the update signal.

  // ret = ret &&
  // connect(app->getCurrentLevel(),SIGNAL(xshLevelChanged()),this,SLOT(onLevelChanged()));
  ret = ret && connect(app->getCurrentFx(), SIGNAL(fxChanged()), this,
                       SLOT(onFxChanged()));
  ret = ret && connect(app->getCurrentXsheet(), SIGNAL(xsheetChanged()), this,
                       SLOT(onXsheetChanged()));
  // ret = ret &&
  // connect(app->getCurrentXsheet(),SIGNAL(xsheetSwitched()),this,SLOT(onXsheetChanged()));
  ret = ret && connect(app->getCurrentObject(), SIGNAL(objectChanged(bool)),
                       this, SLOT(onObjectChanged()));

  TCacheResourcePool::instance();  // The resources pool must be instanced
                                   // before the passive delegate
  TPassiveCacheManager::instance()->setTreeDescriptor(&buildTreeDescription);
}

//---------------------------------------------------------------------------

CacheFxCommand::~CacheFxCommand() {}

//---------------------------------------------------------------------------

CacheFxCommand *CacheFxCommand::instance() {
  static CacheFxCommand theInstance;
  return &theInstance;
}

//---------------------------------------------------------------------------

void CacheFxCommand::onNewScene() {
  TPassiveCacheManager::instance()->reset();
  TCacheResourcePool::instance()->reset();
}

//---------------------------------------------------------------------------

void CacheFxCommand::onSceneLoaded() {
  TPassiveCacheManager::instance()->onSceneLoaded();
}

//---------------------------------------------------------------------------

void CacheFxCommand::onLevelChanged(const std::string &levelName) {
  TPassiveCacheManager::instance()->invalidateLevel(levelName);
}

//---------------------------------------------------------------------------

void CacheFxCommand::onFxChanged() {
  TFx *fx = TApp::instance()->getCurrentFx()->getFx();
  if (fx) TPassiveCacheManager::instance()->onFxChanged(fx);
}

//---------------------------------------------------------------------------

void CacheFxCommand::onXsheetChanged() {
  TPassiveCacheManager::instance()->onXsheetChanged();

  // Update the disabled cache fx commands - retrieve the current dag
  // and analyze each fx.
  /*TXsheet* xsh = TApp::instance()->getCurrentXsheet()->getXsheet();
FxDag* dag = xsh->getFxDag();

{
//Disable all internal fxs
std::set<TFx*> internalFxs;
dag->getInternalFxs()->getFxs(internalFxs);

std::set<TFx*>::iterator it;
for(it = internalFxs.begin(); it != internalFxs.end(); ++it)
{
TRasterFx* rfx = dynamic_cast<TRasterFx*>(*it);
rfx->enableCache(false);
}
}

//Disable each column
int i, columnsCount = xsh->getColumnCount();
for(i=0; i<columnsCount; ++i)
{
TXshColumn* col = xsh->getColumn(i);
assert(col);
TFx* fx = col->getFx();

TZeraryColumnFx* zcolfx = dynamic_cast<TZeraryColumnFx*>(fx);
if(zcolfx) fx = zcolfx->getZeraryFx();

TRasterFx* rfx = dynamic_cast<TRasterFx*>(fx);
rfx->enableCache(false);
}

//Now, we have to enable the user cache for all fxs which have allowed output
connections
//to another user-cachable fx (every root node is considered cachable).

//First off, deal with the XSheet fx
TXsheetFx* xshFx = static_cast<TXsheetFx*>(dag->getXsheetFx());

//Trace all maximal ancestors of the fx
std::vector<TFx*> maximalAncestors;
::traceMaximalAncestors(xshFx, maximalAncestors);

//Then, for each, enable the cache for all allowed children
unsigned int j, size = maximalAncestors.size();
for(j=0; j<size; ++j)
::enableCacheOnAllowedChildren(dynamic_cast<TRasterFx*>(maximalAncestors[j]));

//Now, if the XSheetFx is user-cache enabled, propagate to terminal fxs (which
were not
//automatical, since connections to them do not exist).
if(xshFx->isCacheEnabled())
{
std::set<TFx*> terminalFxs;
dag->getTerminalFxs()->getFxs(terminalFxs);

std::set<TFx*>::iterator it;
for(it = terminalFxs.begin(); it != terminalFxs.end(); ++it)
::enableCacheOnAllowedChildren(dynamic_cast<TRasterFx*>(*it));
}


//Now, deal with leaves the same way
for(i=0; i<columnsCount; ++i)
{
TXshColumn* col = xsh->getColumn(i);
assert(col);
TFx* fx = col->getFx();
if(fx)
{
std::vector<TFx*> maximalAncestors;
::traceMaximalAncestors(fx, maximalAncestors);

unsigned int j, size = maximalAncestors.size();
for(j=0; j<size; ++j)
  ::enableCacheOnAllowedChildren(dynamic_cast<TRasterFx*>(maximalAncestors[j]));
}
}*/
}

//---------------------------------------------------------------------------

void CacheFxCommand::onObjectChanged() {}

//*****************************************************************************************
//    Fx tree description generator
//*****************************************************************************************

namespace {
void buildXsheetTreeDescription(std::string &desc, const FxDag *dag,
                                bool xshFx) {
  if (xshFx) desc += "xsh,";

  std::set<TFx *> terminalFxs;
  dag->getTerminalFxs()->getFxs(terminalFxs);

  std::set<TFx *>::iterator it;
  for (it = terminalFxs.begin(); it != terminalFxs.end(); ++it)
    buildNodeTreeDescription(desc, *it);
}

//------------------------------------------------------------------------------------------

void buildNodeTreeDescription(std::string &desc, const TFxP &root) {
  if (!root) {
    desc += ";";
    return;
  }

  // Look if this node is a sub-xsheet one
  TLevelColumnFx *lcfx = dynamic_cast<TLevelColumnFx *>(root.getPointer());
  if (lcfx) {
    TXshLevelColumn *levelColumn = lcfx->getColumn();
    if (levelColumn) {
      int startRow = lcfx->getTimeRegion().getFirstFrame();
      TXshChildLevel *xshChildLevel =
          lcfx->getColumn()->getCell(startRow).getChildLevel();

      if (xshChildLevel) {
        buildXsheetTreeDescription(desc, xshChildLevel->getXsheet()->getFxDag(),
                                   true);
        return;
      }
    }
  }

  desc += std::to_string(root->getIdentifier()) + ";";

  unsigned int count = root->getInputPortCount();
  for (unsigned int i = 0; i < count; ++i)
    buildNodeTreeDescription(desc, root->getInputPort(i)->getFx());
}

//---------------------------------------------------------------------------------

void searchXsheetContainingInternalFx(TXsheet *&xsh, TXsheet *currXsh,
                                      const TFxP &fx) {
  // Test if root belongs to xsh's dag
  /*if(currXsh->getFxDag()->getInternalFxs()->containsFx(fx.getPointer()))
{
xsh = currXsh;
return;
}

//Otherwise, search among all sub-xsheets.
int colsCount = currXsh->getColumnCount();
for(int i=0; i<colsCount; ++i)
{
TXsheet* childXsh = currXsh->getColumn(i)->getXsheet();
if(childXsh)
  searchXsheetContainingInternalFx(xsh, childXsh, fx);
}*/

  // NOTE: If we search the first column, and perform the
  // getColumn()->getXsheet(),
  // we DO find the xsheet containing the column? That could be good...

  // Dummy proc, for now...
  return;
}
}  // namespace

//---------------------------------------------------------------------------------

//! Builds a description of a \b schematic fx tree which is unique in a Toonz
//! scene session.
//! The returned description is made of fx ids in the form of numbers separated
//! by semicolons -
//! so it will not be familiar at glance - however, it has the property that
//! nephews
//! of passed root actually build descriptions contained in that of the root.
void buildTreeDescription(std::string &desc, const TFxP &root) {
  TXsheetFx *xsheetFx = dynamic_cast<TXsheetFx *>(root.getPointer());
  if (xsheetFx) {
    buildXsheetTreeDescription(desc, xsheetFx->getFxDag(), true);
    return;
  }

  // Start from passed root. Find if the root is children of an xsheet node. In
  // this case,
  // and the input at the leftXSheet port is empty, return the description of
  // the xsheet node.

  TFxPort *xsheetPort = root->getXsheetPort();
  if (xsheetPort && !xsheetPort->getFx()) {
    // Retrieve the xsheet containing the fx
    ToonzScene *scene = TApp::instance()->getCurrentScene()->getScene();
    TXsheet *xsh = 0, *topXsh = scene->getTopXsheet();
    searchXsheetContainingInternalFx(xsh, topXsh, root);

    if (!xsh) {
      desc = "ERROR";
      return;
    }

    // If the node is a terminal fx, return the secription of the xsheet.
    FxDag *dag = xsh->getFxDag();
    if (dag->getTerminalFxs()->containsFx(root.getPointer())) {
      buildXsheetTreeDescription(desc, xsh->getFxDag(), false);
      return;
    }
  }

  buildNodeTreeDescription(desc, root);
}
