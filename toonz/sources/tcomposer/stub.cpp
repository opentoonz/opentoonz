

#include "toolmanager.h"
#include "toonz/tsecurity.h"
#include "drawingobserver.h"
#include "tdata.h"
#include "selection.h"
#include "thumbnail.h"
#include "movieoptions.h"
#include "tpalette.h"
#include "toonz/application.h"
#include "dagviewer.h"
#include "toonz/txshsimplelevel.h"
#include "toonz/imagemanager.h"
#include "xshselection.h"
#include "fxcontroller.h"
#include "tthread.h"

//=========================================================
// TDrawingView
//---------------------------------------------------------

void TDrawingView::notify(TXshSimpleLevel *, TFrameId id) {}

TDrawingView::TDrawingView(TXshSimpleLevel *level, TFrameId fid,
                           const TDimension &size, TDrawingViewObserver *o)
    : m_size(size) {}

TDrawingView::~TDrawingView() {}

const TRaster32P TDrawingView::getRaster() const { return TRaster32P(); }

const TImageP TDrawingView::getImage() const { return TImageP(); }

TImageP TDrawingView::build(TImageInfo &info) { return TImageP(); }

void TDrawingView::onImageBuilt(const string &id, const TImageP &img) {}

string TDrawingView::getDrawingId() const { return ""; }
//=========================================================
// TImageManager
//---------------------------------------------------------

//=========================================================
// TToolManager
//---------------------------------------------------------

class TToolManager::Imp {
public:
  Imp() {}
};

TToolManager::TToolManager() : m_imp(new Imp()) {}

TToolManager::~TToolManager() { delete m_imp; }

TToolManager *TToolManager::instance() {
  static TToolManager theInstance;
  return &theInstance;
}

TTool *TToolManager::getCurrentTool() const { return 0; }

void TToolManager::setCurrentTool(string toolName) {}

int TToolManager::getCurrentTargetType() { return 0; }

void TToolManager::setCurrentTargetType(int tt) {}

//=========================================================
// TSelection
//---------------------------------------------------------

void TSelection::setCurrent(TSelection *) {}

//=========================================================
// TFramesMovieInfo
//---------------------------------------------------------

TFramesMovieInfo::TFramesMovieInfo() : m_type("tif"), m_options(0) {}

TFramesMovieInfo::~TFramesMovieInfo() {}

//=========================================================
// getxxxinfo()
//---------------------------------------------------------

TWriterInfo *getRasterMovieInfo(string, bool) { return 0; }

bool getFramesMovieInfo(struct TFramesMovieInfo &, bool) { return true; }

//=========================================================
// makeScreenSaver
//---------------------------------------------------------

void makeScreenSaver(TFilePath, TFilePath, string) {}

//=========================================================
// bigBoxSize[]
//---------------------------------------------------------

double bigBoxSize[3];  // per stage.cpp

//=========================================================
// TFxNode
//---------------------------------------------------------

// TFxNode::TFxNode(TFx *fx) : m_fx(fx) {}

/*
  // da Connectable
  int getInputCount() const;
  Connectable *getInput(int inputPortId) const;

  bool setInput(Connectable*, int inputPortId);
  bool isCompatibleInput(Connectable*, int inputPortId);

  const DagViewer::Node *getDagNode() const {return this;}
  DagViewer::Node *getDagNode() {return this;}


  // da DagViewer::Node
  wstring getName() const;
  Connectable *getConnectable() const;

  void onClick(const TPointD& pos, const TMouseEvent &e);
  TContextMenu *makeContextMenu(TWidget *parent) const;

private:
  // not implemented
  TFxNode(const TFxNode &);
  TFxNode&operator=(const TFxNode &);
};

*/

//---------------------------------------------------------

//=========================================================
// TSecurity
//---------------------------------------------------------

string TSecurity::getRegistryRoot(void) {
#ifdef WIN32
  string reg = "SOFTWARE\\Digital Video\\Toonz\\5.0\\";
  return reg;
#else
  assert(false);
  return "";
#endif
}

//=========================================================
// TSelection
//---------------------------------------------------------

TSelection *TSelection::getCurrent() { return 0; }

//=========================================================
// XshPasteBuffer
//---------------------------------------------------------

XshPasteBuffer::XshPasteBuffer(int, int) : m_columnCount(0), m_rowCount(0) {}

TXshCell &XshPasteBuffer::cell(int, int) {
  static TXshCell empty;
  return empty;
}

TDataP XshPasteBuffer::clone() const { return TDataP(); }

/*
LevelThumbnail::LevelThumbnail(const TDimension &d, TXshSimpleLevel *)
: Thumbnail(d)
{
}
*/

void FxController::setCurrentFx(TFx *) {}
FxController::FxController() {}
FxController *FxController::instance() {
  static FxController _instance;
  return &_instance;
}

#ifndef WIN32
// to avoid twin dep
void postThreadMsg(TThread::Msg *) { return; }

DEFINE_CLASS_CODE(TData, 16)

#endif
