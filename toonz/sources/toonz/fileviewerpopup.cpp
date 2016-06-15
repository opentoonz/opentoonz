

#include "fileviewerpopup.h"
#include "menubarcommand.h"
#include "menubarcommandids.h"
#include "viewerpane.h"

#include <QHBoxLayout>
#include <QDragEnterEvent>
#include <QUrl>
#include <QPainter>

#include "toonz/toonzimageutils.h"
#include "tsystem.h"
#include "timagecache.h"
/*
#include "imageviewer.h"
#include "tdata.h"
#include "tw/mainshell.h"
#include "tlevel_io.h"
#include "vcrpanel.h"
#include "tframe.h"
#include "tsound.h"
#include "tsop.h"
#include "toonz/application.h"
#include "toonz/sceneproperties.h"
#include "toutputproperties.h"
#include "toonz/tabscene.h"
class TSoundTrack;
using namespace TwConsts;*/

//=============================================================================
// FileViewer
//-----------------------------------------------------------------------------

#if QT_VERSION >= 0x050500
FileViewer::FileViewer(QWidget *parent, Qt::WindowFlags flags)
#else
FileViewer::FileViewer(QWidget *parent, Qt::WFlags flags)
#endif
    : QWidget(parent), m_fileSize(0), m_player(0), m_snd(0), m_soundOn(false) {
  setAcceptDrops(true);
}

//-----------------------------------------------------------------------------

void FileViewer::dragEnterEvent(QDragEnterEvent *event) {
  if (event->mimeData()->hasUrls()) event->acceptProposedAction();
}

//-----------------------------------------------------------------------------

void FileViewer::dropEvent(QDropEvent *event) {
  QList<QUrl> urls = event->mimeData()->urls();
  TFilePath path   = TFilePath(urls[0].toLocalFile().toStdString());
  if (path.isEmpty()) return;
  setPath(path);
  event->acceptProposedAction();
}

//-----------------------------------------------------------------------------

void FileViewer::setPath(const TFilePath &fp, int from, int to, int step,
                         TSoundTrack *snd) {
  if ((int)m_fids.size() > 0) clearViewerCache();
  m_path = fp;
  m_snd  = snd;
  m_fids.clear();
  m_index    = -1;
  m_fileSize = 0;
  m_fileDate = "";
  if (fp.getDots() == "..") {
    m_levelName = fp.withoutParentDir().withFrame().getWideString();
  } else {
    m_levelName = fp.withoutParentDir().getWideString();
    updateFileInfo(m_path);
  }
  m_lr = TLevelReaderP();
  if (step != 0)  // e' un render
    for (int i = from; i <= to; i += step) m_fids.push_back(TFrameId(i));
  else if (fp != TFilePath() &&
           TSystem::doesExistFileOrLevel(fp))  // e' un view
  {
    try {
      m_lr          = TLevelReaderP(fp);
      TLevelP level = m_lr->loadInfo();
      if (!level || level->getFrameCount() == 0) return;
      m_palette = level->getPalette();
      if (!m_palette && (fp.getType() == "tzp" || fp.getType() == "tzu"))
        m_palette =
            ToonzImageUtils::loadTzPalette(fp.withType("plt").withNoFrame());

      for (TLevel::Iterator it = level->begin(); it != level->end(); ++it)
        m_fids.push_back(it->first);
      showFrame(0);
    } catch (...) {
      return;
    }
  } else
    m_image = TImageP();
  //  else
  //    setImage(TImageP());
  // resetZoom();

  showFrame(0);
  // configureNotify();
  //  if(m_listener) m_listener->onPathChange(fp);
}

//-----------------------------------------------------------------------------

void FileViewer::clearViewerCache() {
  int i;
  for (i = 0; i < (int)m_fids.size(); i++) {
    TFrameId fid = m_fids[i];
    string id    = toString(m_levelName) + toString(fid.getNumber());
    if (TImageCache::instance()->isCached(id))
      TImageCache::instance()->remove(id);
  }
}

//-----------------------------------------------------------------------------

void FileViewer::updateFileInfo(const TFilePath &fp) {
  TFileStatus fs(fp);
  if (fs.doesExist()) {
    m_fileSize = (int)((fs.getSize() + 1023) / 1024);
    m_fileDate = fs.getLastModificationTime().getFormattedString();
  } else {
    m_fileSize = 0;
    m_fileDate = "";
  }
}

//-----------------------------------------------------------------------------

void FileViewer::showFrame(int index) {
  if (m_fids.empty()) return;

  index   = tcrop(index, 0, (int)(m_fids.size() - 1));
  m_index = index;

  TFrameId fid = m_fids[index];
  string id    = toString(m_levelName) + toString(fid.getNumber());

  try {
    TImageP img;
    if (TImageCache::instance()->isCached(id))
      img = TImageCache::instance()->get(id, false);
    else if (m_lr) {
      TImageReaderP ir = m_lr->getFrameReader(fid);
      if (m_path.getDots() == "..") {
        if (ir)
          updateFileInfo(ir->getFilePath());
        else {
          m_fileSize = 0;
          m_fileDate = "";
        }
      }
      img = ir->load();
      if (img) {
        if (!img->getPalette() && m_palette)
          img->setPalette(m_palette.getPointer());
        TImageCache::instance()->add(id, img);
      }
    }
    m_image = img;
    //    setImage(img);
  } catch (...) {
    m_image = TImageP();
    //    setImage(TImageP());
  }
  //  invalidate();
  update();

  //  if(m_listener)
  //  {
  //      m_listener->onFrameChange(getFrameId());
  //  }
  //  playSound();
}

//-----------------------------------------------------------------------------

void FileViewer::paintEvent(QPaintEvent *) { QPainter p(this); }

/*
//-------------------------------------------------------------------

FileViewerPanel::FileViewerPanel(TWidget *parent, string name)
: ImageViewer(parent, name)
, m_listener(0)
, m_fileSize(0)
, m_player(0)
, m_snd(0)
, m_soundOn(false)
{
  enableDropTarget(this);
}

//-------------------------------------------------------------------

TDropSource::DropEffect FileViewerPanel::onEnter (const Event &event)
{
  return TDropSource::Copy;
}

//-------------------------------------------------------------------

TDropSource::DropEffect FileViewerPanel::onOver (const Event &event)
{
  return TDropSource::Copy;
}

//-------------------------------------------------------------------

void FileViewerPanel::onLeave ()
{
}

//-------------------------------------------------------------------

TDropSource::DropEffect FileViewerPanel::onDrop (const Event &event)
{
  const TFilePathListData *fd =
    dynamic_cast<const TFilePathListData *>(event.m_data);
  if( fd && fd->getFilePathCount()==1)
    {
          setPath(fd->getFilePath(0));
          return TDropSource::Copy;
    }
  else
    return TDropSource::None;
}

//-------------------------------------------------------------------

void FileViewerPanel::updateFileInfo(const TFilePath &fp)
{
  TFileStatus fs(fp);
  if(fs.doesExist())
    {
     m_fileSize = (int)((fs.getSize()+1023)/1024);
     m_fileDate = fs.getLastModificationTime().getFormattedString();
    }
  else
    {
     m_fileSize = 0;
     m_fileDate = "";
    }
}

//-------------------------------------------------------------------

void FileViewerPanel::setPath(const TFilePath &fp, int from, int to, int step,
TSoundTrack*snd)
{
  if ((int)m_fids.size() > 0)
        clearViewerCache();
  m_path = fp;
  m_snd = snd;
  m_fids.clear();
  m_index = -1;
  m_fileSize = 0;
  m_fileDate = "";
  if(fp.getDots()=="..")
  {
     m_levelName = fp.withoutParentDir().withFrame().getWideString();
  }
  else
  {
     m_levelName = fp.withoutParentDir().getWideString();
     updateFileInfo(m_path);
  }
  m_lr = TLevelReaderP();
  if (step!=0) //e' un render
    for (int i=from; i<=to; i+=step)
      m_fids.push_back(TFrameId(i));
  else if(fp != TFilePath() && TSystem::doesExistFileOrLevel(fp)) //e' un view
    {
    try
       {
       m_lr = TLevelReaderP(fp);
       TLevelP level = m_lr->loadInfo();
       if(!level || level->getFrameCount()==0) return;
       m_palette = level->getPalette();
       if (!m_palette && (fp.getType()=="tzp" || fp.getType()=="tzu"))
         m_palette =
ToonzImageUtils::loadTzPalette(fp.withType("plt").withNoFrame());

       for(TLevel::Iterator it = level->begin(); it != level->end(); ++it)
           m_fids.push_back(it->first);
       showFrame(0);
       } catch(...) {return;}
     }
   else
     setImage(TImageP());
  //resetZoom();

 showFrame(0);
 configureNotify();
  if(m_listener) m_listener->onPathChange(fp);
}

//-------------------------------------------------------------------

void FileViewerPanel::resetZoom()
{
  ImageViewer::resetZoom();
  if(m_listener) m_listener->onZoomChange();
}

//-------------------------------------------------------------------

void FileViewerPanel::zoom(const TPoint &center, double factor, bool
isZoomWheel)
{
  ImageViewer::zoom(center, factor, isZoomWheel);
  if(m_listener) m_listener->onZoomChange();
}
//-------------------------------------------------------------------

void FileViewerPanel::playSound()
{
  static bool first = true;
  static bool audioCardInstalled;
  if (!m_snd || !m_soundOn)
    return;

  if (first)
    {
    audioCardInstalled = TSoundOutputDevice::installed();
    first = false;
    }

  if (!audioCardInstalled)
    return;

  if (!m_player)
    {
    m_player = new TSoundOutputDevice();
    m_player->attach(this);
    m_player->setVolume(1);
    }
  if (m_player)
    {
    int fps = TApplication::instance()->getCurrentScene()->getProperties()
              ->getOutputProperties()->getFrameRate();

    int samplePerFrame = (int) m_snd->getSampleRate() / fps;
    TINT32 firstSample = (m_fids[m_index].getNumber()-1) * samplePerFrame;
    TINT32 lastSample = firstSample + samplePerFrame;

    try
      {
      m_player->play(m_snd, firstSample, lastSample, false, false);
      if(m_player->isPlaying())
        m_player->setVolume(1);
      }
    catch (TSoundDeviceException &e)
          {
      string msg;
      if (e.getType() == TSoundDeviceException::UnsupportedFormat)
          {
        try {
          TSoundTrackFormat fmt =
m_player->getPreferredFormat(m_snd->getFormat());
          m_player->play( TSop::convert(m_snd, fmt), firstSample, lastSample,
false, false);
          if(m_player->isPlaying())
            m_player->setVolume(1);
          }
        catch (TSoundDeviceException &ex) {
          throw TException(ex.getMessage());
          return;
          }
          }
                  }
          }
}

void FileViewerPanel::showFrame(int index)
{
  if(m_fids.empty()) return;

  index = tcrop(index, 0, (int)(m_fids.size()-1));
  m_index = index;

  TFrameId fid = m_fids[index];
  string id = toString(m_levelName) + toString(fid.getNumber());

  try
    {
                TImageP img;
                if(TImageCache::instance()->isCached(id))
                  img = TImageCache::instance()->get(id,false);
          else if (m_lr)
            {
                  TImageReaderP ir = m_lr->getFrameReader(fid);
      if(m_path.getDots()=="..")
        {
                          if(ir) updateFileInfo(ir->getFilePath());
        else {m_fileSize=0;m_fileDate="";}
                    }
      img = ir->load();
      if (img)
        {
        if(!img->getPalette() && m_palette)
                     img->setPalette(m_palette.getPointer());
        TImageCache::instance()->add(id,img);
              }
            }
    setImage(img);
    }
  catch(...)
    {
    setImage(TImageP());
    }
  invalidate();

  if(m_listener)
  {
        //if(m_path.getDots()=="..")
        //  m_listener->onPathChange(m_path);
  //  else
      m_listener->onFrameChange(getFrameId());
  }
  playSound();

}

//-------------------------------------------------------------------


void FileViewerPanel::clearViewerCache()
{
  int i;
  for(i = 0; i<(int)m_fids.size(); i++)
  {
        TFrameId fid = m_fids[i];
    string id = toString(m_levelName) + toString(fid.getNumber());
    if(TImageCache::instance()->isCached(id))
          TImageCache::instance()->remove(id);
  }
}

//-------------------------------------------------------------------

void FileViewerPanel::onTimer(int)
{
  if(m_fids.empty() || m_index>=(int)(m_fids.size()-1))
    {
     showFrame(0);
     stopTimer();
    }
  else
    {
     showFrame(m_index+1);
    }
}

//===================================================================


class FileViewerPopup : public TPopup, public FileViewerPanel::Listener {
  FileViewerPanel *m_viewer;
  FlipPanel *m_flipPanel;
  const int m_topBarHeight, m_botBarHeight;
  TRect m_titleBox, m_frameBox;
public:
  FileViewerPopup(string name)
  : TPopup(TMainshell::getMainshell(), name)
  , m_topBarHeight(15)
  , m_botBarHeight(20+15) {
    m_isMenu = false;
    m_viewer = new FileViewerPanel(this, "imageViewer");
    m_viewer->setListener(this);
    m_flipPanel = new FlipPanel(this, FlipPanel::WITH_COLORFILTER|
                                            FlipPanel::WITH_SPEEDSLIDER);
    m_flipPanel->setVCR(m_viewer);

    setSize(600,400);
    setCaption("Viewer");
    m_viewer->resetZoom();
  }

  void configureNotify(const TDimension &size) {
    m_titleBox = TRect(4,size.ly-m_topBarHeight+1,getLx()-5,size.ly-2);
    m_frameBox = m_titleBox;
    m_viewer->setGeometry(1,m_botBarHeight+2,size.lx-2,size.ly-m_topBarHeight-2);
    TDimension vcrSize(size.lx-8,m_botBarHeight);
    TPoint p((size.lx-vcrSize.lx)/2,1);
    m_flipPanel->setGeometry(p, vcrSize);
    m_viewer->invalidate();
    invalidate();
  }

  void drawTitle() {
    TPoint p(m_titleBox.getP00()+TPoint(4,1));
    setColor(Black);
    wstring nameStr = m_viewer->getLevelName();
    TFrameId fid = m_viewer->getFrameId();
    string fidStr;
    if(fid.getNumber()>0) fidStr = m_viewer->getFrameId().expand();

    string framesStr = toString(m_viewer->getFrameCount())+" fr";
    string sizeStr = toString(m_viewer->getFileSize()) + "K";
    string dateStr = m_viewer->getFileDate();

    wstring w = nameStr + L" ::";
    drawText(p,w);
    p.x += getTextSize(w).lx;
    m_frameBox.x0 = p.x;
    string s = " " + fidStr + " ";
    drawText(p,s);
    p.x += getTextSize(s).lx;
    m_frameBox.x1 = p.x;

    s = ":: " + framesStr + " :: " + sizeStr + " :: " +  dateStr;

        drawText(p,s);

        string zoomStr = "zoom "+toString(troundp(m_viewer->getZoomFactor())) +
"%";
        TPoint p1(m_titleBox.getP10());
        p1.x -= getTextSize(zoomStr).lx+4;
        drawText(p1,zoomStr);

  }

  void repaint() {
    TDimension size = getSize();
    int lx = size.lx;
    int ly = size.ly;
    int y = ly-1-m_topBarHeight;
    TRect r(1,y+1,lx-2,ly-1);
    setColor(ToonzBgColor);
    fillRect(r);
    drawTitle();
    r.y0=1;r.y1=m_botBarHeight;
    setColor(Gray210);
    fillRect(r);

    setColor(Gray150);
    drawLine(1,y,lx-2,y);
    y = m_botBarHeight+1;
    drawLine(1,y,lx-2,y);

    setColor(Black);

    TFrame::drawCorner(this, 0,0,1,1);
    TFrame::drawCorner(this, lx-1,0,-1,1);
    TFrame::drawCorner(this, 0,ly-1,1,-1);
    TFrame::drawCorner(this, lx-1,ly-1,-1,-1);
    int d = 3;
    drawLine(0,d,0,ly-1-d);
    drawLine(lx-1,d,lx-1,ly-1-d);
    drawLine(d,ly-1,lx-1-d,ly-1);
    drawLine(d,0,lx-1-d,0);
  }

  void setFrameRate(int frameRate)
    {
    m_flipPanel->setFrameRate(frameRate);
    }

  void load(const TFilePath &fp) {
    m_flipPanel->enableAudioButton(false);
    m_viewer->setPath(fp);
    m_flipPanel->reset();
    m_flipPanel->setLevel(m_viewer->getLevelFids());
  }

  void load(const TFilePath &fp, int from, int to, int step, TSoundTrack* snd) {

    m_viewer->setPath(fp, from, to, step, snd);
    m_flipPanel->enableAudioButton(snd!=0);
    m_flipPanel->setLevel(m_viewer->getLevelFids());

  }

  bool onClose()
  {
        m_viewer->setPath(TFilePath());
    return true;
  }

  void onPathChange(const TFilePath &fp) {invalidate(m_titleBox);}
  void onFrameChange(const TFrameId &fid) {m_flipPanel->onViewerRepaint();
invalidate(m_frameBox); }
  void onZoomChange() {invalidate(m_titleBox);}
};

//===================================================================
namespace {

class FileViewerPopupPool { // singleton
  public:
  std::vector<FileViewerPopup*> m_popups;
  int m_index;

  FileViewerPopupPool() : m_index(-1){}

  static FileViewerPopupPool *instance() {
    static FileViewerPopupPool _instance;
    return &_instance;
  }
  FileViewerPopup *getNew(bool doOpen) {
    // prima cerco fra i chiusi
    for(int i=0;i<(int)m_popups.size();i++)
      if(!m_popups[i]->isOpen())
        {
         m_index = i;
         if (doOpen)
           m_popups[m_index]->openPopup();
         return m_popups[m_index];
        }
    string name("fileViewerPopup");
    if(m_popups.size()>0)
      name += toString((int)m_popups.size()+1);
    FileViewerPopup *popup = new FileViewerPopup(name);
    m_index = m_popups.size();
    m_popups.push_back(popup);
    if (doOpen)
      popup->openPopup();
    return popup;
  }
  FileViewerPopup *getCurrent(bool doOpen=true) {
    if(0<=m_index && m_index<(int)m_popups.size() &&
       m_popups[m_index]->isOpen())
      return m_popups[m_index];
    return getNew(doOpen);
  }

};



}//namespace

//===================================================================


void setFrameRateInViewers(int frameRate)
  {
  for (UINT i=0; i<FileViewerPopupPool::instance()->m_popups.size(); i++)
    FileViewerPopupPool::instance()->m_popups[i]->setFrameRate(frameRate);
  }


void resetViewer()
  {
  FileViewerPopupPool::instance()->getCurrent(false)->onClose();
  }


void viewFile(const TFilePath &fp)
{
  if(fp != TFilePath())
    FileViewerPopupPool::instance()->getCurrent()->load(fp);
}


void viewFile(const TFilePath &fp, int from, int to, int step, TSoundTrack* snd)
{
  if(fp != TFilePath())
    FileViewerPopupPool::instance()->getCurrent()->load(fp, from, to, step,
snd);
}


//===================================================================

class OpenFileViewerCommand : public TGuiCommandExecutor
{
public:
  OpenFileViewerCommand()
  : TGuiCommandExecutor("MI_OpenFileViewer")
  {}

  void onCommand() {
    FileViewerPopupPool::instance()->getNew(true);
  }

} openFileViewerCommand;
*/

//=============================================================================
// FileViewerPopup
//-----------------------------------------------------------------------------

#if QT_VERSION >= 0x050500
FileViewerPopup::FileViewerPopup(QWidget *parent, Qt::WindowFlags flags)
#else
FileViewerPopup::FileViewerPopup(QWidget *parent, Qt::WFlags flags)
#endif
    : QWidget(parent) {
  setWindowTitle(tr("Viewer"));
  QHBoxLayout *layout    = new QHBoxLayout(this);
  FileViewer *fileViewer = new FileViewer(this);

  layout->addWidget(fileViewer);

  setLayout(layout);
}

//-----------------------------------------------------------------------------

OpenPopupCommandHandler<FileViewerPopup> openPltGizmoPopup(MI_OpenFileViewer);
