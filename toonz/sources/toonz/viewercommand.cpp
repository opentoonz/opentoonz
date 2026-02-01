
#include "toonzqt/menubarcommand.h"
#include "menubarcommandids.h"
#include "tapp.h"
#include "filmstripcommand.h"

// TnzLib includes
#include "toonz/tscenehandle.h"
#include "toonz/txshlevelhandle.h"
#include "toonz/tframehandle.h"
#include "toonz/txsheethandle.h"
#include "toonz/tcolumnhandle.h"
#include "toonz/txshsimplelevel.h"
#include "toonz/toonzscene.h"
#include "toonz/txsheet.h"
#include "toonz/txshcell.h"

// TnzQt includes
#include "toonzqt/dvdialog.h"

#include <set>

//=============================================================================
// Clear Viewer Content Command
// Clears the drawing content of the current frame visible in the viewer
//-----------------------------------------------------------------------------

class ClearViewerContentCommand final : public MenuItemHandler {
public:
  ClearViewerContentCommand() : MenuItemHandler(MI_ClearViewerContent) {}
  
  void execute() override {
    TApp *app = TApp::instance();
    
    // Get current frame and level
    TFrameHandle *fh = app->getCurrentFrame();
    TXshSimpleLevel *sl = app->getCurrentLevel()->getSimpleLevel();
    
    if (!sl || sl->isReadOnly()) {
      DVGui::warning(QObject::tr("Cannot clear: level is read-only or no level is selected."));
      return;
    }
    
    // Get the current frame ID based on viewing mode
    TFrameId currentFid;
    
    if (fh->isEditingLevel()) {
      // In Level Strip mode or Level editing mode
      currentFid = fh->getFid();
    } else {
      // In Scene Viewer mode - get frame from xsheet
      TXsheet *xsh = app->getCurrentXsheet()->getXsheet();
      int row = fh->getFrame();
      int col = app->getCurrentColumn()->getColumnIndex();
      
      if (row >= 0 && col >= 0) {
        TXshCell cell = xsh->getCell(row, col);
        if (!cell.isEmpty() && cell.getSimpleLevel() == sl) {
          currentFid = cell.getFrameId();
        }
      }
    }
    
    // Check if we have a valid frame to clear
    if (currentFid == TFrameId::NO_FRAME || currentFid == TFrameId()) {
      DVGui::warning(QObject::tr("No frame to clear."));
      return;
    }
    
    // Clear the frame content (replace with empty frame)
    std::set<TFrameId> frames;
    frames.insert(currentFid);
    
    // Use FilmstripCmd::clear to replace frame content with empty frame
    // This preserves the frame in the timeline but clears its drawing content
    FilmstripCmd::clear(sl, frames);
    
    // Refresh viewer and xsheet
    app->getCurrentXsheet()->notifyXsheetChanged();
  }
} clearViewerContentCommand;

