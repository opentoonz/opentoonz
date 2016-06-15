#pragma once

#ifndef FILEVIEWERPOPUP_H
#define FILEVIEWERPOPUP_H

#include <QWidget>

#include "tfilepath.h"
#include "tlevel_io.h"
#include "tpalette.h"

class TSoundTrack;
class TSoundOutputDevice;

using namespace std;

//=============================================================================
// FileViewer
//-----------------------------------------------------------------------------

class FileViewer : public QWidget {
  Q_OBJECT

  TFilePath m_path;
  std::vector<TFrameId> m_fids;
  TImageP m_image;
  int m_index;
  TLevelReaderP m_lr;
  TPaletteP m_palette;
  int m_fileSize;
  std::string m_fileDate;
  wstring m_levelName;
  TSoundOutputDevice *m_player;
  TSoundTrack *m_snd;
  bool m_soundOn;

public:
#if QT_VERSION >= 0x050500
  FileViewer(QWidget *parent = 0, Qt::WindowFlags flags = Qt::Tool);
#else
  FileViewer(QWidget *parent = 0, Qt::WFlags flags = Qt::Tool);
#endif

  void setPath(const TFilePath &fp, int from = 0, int to = 0, int step = 0,
               TSoundTrack *snd = 0);

  void clearViewerCache();

  void updateFileInfo(const TFilePath &fp);

  void showFrame(int frameIndex);

protected:
  void dragEnterEvent(QDragEnterEvent *event);
  void dropEvent(QDropEvent *event);

  void paintEvent(QPaintEvent *);
};

/*
class FileViewerPanel
: public ImageViewer
, public TDragDropListener
, public TSoundOutputDeviceListener
, public FlipPanel::VCR
{
public:
  class Listener {
  public:
    virtual void onPathChange(const TFilePath &fp) = 0;
    virtual void onFrameChange(const TFrameId &fid) = 0;
    virtual void onZoomChange() = 0;
    virtual ~Listener(){}
  };

private:

  TDropSource::DropEffect m_currentDropEffect;
  TFilePath m_path;
  std::vector<TFrameId> m_fids;
  int m_index;
  TLevelReaderP m_lr;
  TPaletteP m_palette;
  Listener *m_listener;
  int m_fileSize;
  string m_fileDate;
  wstring m_levelName;
  TSoundOutputDevice* m_player;
  TSoundTrack* m_snd;
  bool m_soundOn;
  void playSound();
public:

  FileViewerPanel(TWidget *parent, string name = "fileViewerPanel");

  void setListener(Listener *listener) {m_listener=listener;}

  TDropSource::DropEffect onEnter (const Event &event);
  TDropSource::DropEffect onOver (const Event &event);
  void onLeave ();
  TDropSource::DropEffect onDrop (const Event &event);

  void setPath(const TFilePath &fp, int from=0, int to=0, int step=0,
TSoundTrack*snd=0);
  TFilePath getPath() const {return m_path;}
  void zoom(const TPoint &center, double factor, bool isZoomWheel);

  void resetZoom();
  wstring getLevelName() const {return m_levelName;}
  const vector<TFrameId>& getLevelFids()const {return m_fids;}

  void showFrame(int frameIndex);
  TFrameId getFrameId() const {
    return 0<=m_index && m_index<(int)m_fids.size() ? m_fids[m_index] :
TFrameId::NO_FRAME;
  }

  int getFrameCount() const {return m_fids.size();}
  int getFileSize() const {return m_fileSize; }
  string getFileDate() const {return m_fileDate;}

  void updateFileInfo(const TFilePath &fp);

  void onTimer(int);

  void firstFrame() {if(!m_fids.empty()) showFrame(0);}
  void lastFrame()  {if(!m_fids.empty()) showFrame(m_fids.size()-1);}
  void nextFrame()  {if(m_index<(int)(m_fids.size()-1)) showFrame(m_index+1);}
  void prevFrame()  {if(m_index>0 && !m_fids.empty()) showFrame(m_index-1);}
  bool isLastFrame() const {return m_index == (int)(m_fids.size()-1);}
  int getCurrentFrame() const {return m_index;}
  void setColorFilter(UCHAR colorMask)  {setColorMask(colorMask); invalidate();}
  void setSound(bool on) {m_soundOn = on;}

  void setFrame(int index)
    {
    if(index>=(int)m_fids.size()) return;
    showFrame(index);
    }
  void onPlayCompleted(){}
  void clearViewerCache();
};
*/

//=============================================================================
// FileViewerPopup
//-----------------------------------------------------------------------------

class TSoundTrack;

class FileViewerPopup : public QWidget {
  Q_OBJECT

  FileViewer *m_viewer;

public:
#if QT_VERSION >= 0x050500
  FileViewerPopup(QWidget *parent = 0, Qt::WindowFlags flags = Qt::Tool);
#else
  FileViewerPopup(QWidget *parent = 0, Qt::WFlags flags = Qt::Tool);
#endif
};

/*

void viewFile(const TFilePath &fp, int from, int to, int step, TSoundTrack*
snd);
void setFrameRateInViewers(int frameRate);

//cancella le immagini
void resetViewer();*/
#endif  // FILEVIEWERPOPUP_H
