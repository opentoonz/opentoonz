

#include "viewerpopup.h"
#include "floatingpanelcommand.h"
#include "menubarcommandids.h"
#include "pane.h"

#include "toonzqt/gutil.h"

#include <QSet>
#include <QMimeData>
#include <QMouseEvent>
#include <QUrl>
#include <QDesktopServices>
#include <QSettings>

//-----------------------------------------------------------------------------
// ||
// || OBSOLETO
// \/
//-----------------------------------------------------------------------------
/*
//=============================================================================
namespace {
        QSet<ViewerPopup*> openViewers;
}
//-----------------------------------------------------------------------------

//=============================================================================
// ViewerPopup
//-----------------------------------------------------------------------------

ViewerPopup::ViewerPopup(QWidget *parent , Qt::WFlags flags)
: FlipBook(parent, flags, QString(tr("FlipBook")))
{
  connect(parentWidget(), SIGNAL(visibilityChanged(bool)), this,
SLOT(onVisibilityChange(bool)));
  openViewers.insert(this);
}

//-----------------------------------------------------------------------------

//-----------------------------------------------------------------------------

void ViewerPopup::onVisibilityChange (bool visible)
{
        if (!visible)
                openViewers.remove(this);
        else if (openViewers.empty())
                openViewers.insert(this);
}



TFilePath getFirstFullPath(const TFilePath &levelPath)
{
        TLevelReaderP lr = TLevelReaderP(levelPath);
  if (!lr) return TFilePath();
  TLevelP level =  lr->loadInfo();
  if(!level || level->getFrameCount()==0) return TFilePath();
  TLevel::Iterator it = level->begin();
  TFrameId fid = it->first;
  return levelPath.withFrame(fid);
}
*/

//-----------------------------------------------------------------------------
// /\
// || OBSOLETO
// ||
//-----------------------------------------------------------------------------

//=============================================================================
// openFileViewerCommand
//-----------------------------------------------------------------------------

OpenFloatingPanel openFileViewerCommand(MI_OpenFileViewer, "FlipBook",
                                        QObject::tr("FlipBook"));
