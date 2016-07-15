

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

OpenFloatingPanel openFileViewerCommand(MI_OpenFileViewer, "FlipBook",
                                        QObject::tr("FlipBook"));
