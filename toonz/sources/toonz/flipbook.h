#pragma once

#ifndef FLIPBOOK_H
#define FLIPBOOK_H

#include "toonzqt/flipconsole.h"
#include "imageviewer.h"

#include "tlevel_io.h"
#include "tpalette.h"
#include "tsound.h"
#include "filebrowserpopup.h"

#include "tfx.h"
#include "toonz/txsheet.h"

#include <QTimer>

#include "toonzqt/flipconsoleowner.h"

class QPoint;
class TPalette;
class TFilePath;
class FlipBook;

//=============================================================================

class SaveImagesPopup final : public FileBrowserPopup {
  Q_OBJECT

  FlipBook *m_flip;

public:
  SaveImagesPopup(FlipBook *flip);

  bool execute() override;
};

class LoadImagesPopup final : public FileBrowserPopup {
  Q_OBJECT

  DVGui::LineEdit *m_fromField;
  DVGui::LineEdit *m_toField;
  DVGui::LineEdit *m_stepField;
  DVGui::LineEdit *m_shrinkField;

  int m_minFrame, m_maxFrame;
  int m_from, m_to, m_step, m_shrink;

  QStringList m_appendFilterTypes;
  QStringList m_loadFilterTypes;

  FlipBook *m_flip;

public:
  LoadImagesPopup(FlipBook *flip);

  bool execute() override;
  bool executeApply() override;
  bool doLoad(bool append);

public slots:

  void onFilePathClicked(const TFilePath &);
  void onEditingFinished();
};

//=============================================================================
// FlipBookPool
//-----------------------------------------------------------------------------

class FlipBook;

class FlipBookPool {
  std::map<int, FlipBook *> m_pool;
  std::map<int, QRect> m_geometryPool;
  int m_overallFlipCount;

  TFilePath m_historyPath;

public:
  FlipBookPool();
  ~FlipBookPool();

  static FlipBookPool *instance();

  void load(const TFilePath &historyPath);
  void save() const;

  FlipBook *pop();
  void push(FlipBook *flipbook);
};

//=============================================================================
// FlipBook
//-----------------------------------------------------------------------------
class TSoundOutputDevice;
class TSoundTrack;
class SaveImagesPopup;
class LoadImagesPopup;
class TFx;
class TXsheet;
class TXshSimpleLevel;

class TPanelTitleBar;
class TPanelTitleBarButton;

class FlipBook : public QWidget,
                 public TSoundOutputDeviceListener,
                 public FlipConsoleOwner {
  Q_OBJECT

protected:
  int m_poolIndex;

  QString m_viewerTitle;
  QString m_title, m_title1;
  ImageViewer *m_imageViewer;
  FlipConsole *m_flipConsole;

  int m_shrink;
  int m_framesCount;
  TRect m_loadbox;
  TDimension m_dim;
  std::map<std::string, TRect>
      m_loadboxes;  // id in the cash, rect loaded actually
  class Level {
  public:
    Level(const TLevelP &level, const TFilePath &fp, int fromIndex, int toIndex,
          int step)
        : m_level(level)
        , m_fp(fp)
        , m_fromIndex(fromIndex)
        , m_toIndex(toIndex)
        , m_step(step)
        , m_randomAccessRead(false)
        , m_incrementalIndexing(false)
        , m_premultiply(false) {}
    TLevelP m_level;
    int m_fromIndex, m_toIndex, m_step;
    bool m_incrementalIndexing;
    bool m_randomAccessRead;

    // Specified if the level is needed to be premultiplied on display.
    // It will be true for the files of which the "premultiply" option is
    // activated in the Preferences > Loading > "Level Settings by File Format".
    // By default, PNG will be loaded with being premultiplied so that it will
    // be displayed properly.
    bool m_premultiply;

    TFilePath m_fp;

    TFrameId flipbookIndexToLevelFrame(int index);
    int getIndexesCount();
  };

  std::vector<Level> m_levels;
  std::vector<QString> m_levelNames;
  TPalette *m_palette;

  bool m_playSound;
  std::vector<std::string> m_addedInCache;
  TSoundOutputDevice *m_player;
  TSoundTrack *m_snd;

  // bool m_doCompare;

  // questi sono per il saving dei frame
  TLevelWriterP m_lw;
  TLevelReaderP m_lr;
  TXshSimpleLevel *m_xl;
  int m_currentFrameToSave;
  SaveImagesPopup *m_savePopup;
  LoadImagesPopup *m_loadPopup;

  bool m_isPreviewFx;
  TFxP m_previewedFx;
  TXsheetP m_previewXsh;
  QTimer m_previewUpdateTimer;
  bool m_freezed;
  UCHAR m_flags;

  TPanelTitleBarButton *m_freezeButton;

public:
  enum Flags { eDontKeepFilesOpened = 0x1 };

  FlipBook(QWidget *parent = 0, QString viewerTitle = QString(),
           UINT flipConsoleButtonMask = FlipConsole::cFullConsole &
                                        (~(FlipConsole::eFilledRaster |
                                           FlipConsole::eDefineSubCamera |
                                           FlipConsole::eLocator)),
           UCHAR flags = 0, bool isColorModel = false);
  ~FlipBook();
  void setLevel(const TFilePath &path, TPalette *palette = 0, int from = -1,
                int to = -1, int step = 1, int shrink = 1, TSoundTrack *snd = 0,
                bool append = false, bool isToonzOutput = false);
  void setLevel(TFx *previewedFx, TXsheet *xsh, TLevel *level,
                TPalette *palette = 0, int from = -1, int to = -1, int step = 1,
                int currentFrame = 1, TSoundTrack *snd = 0);
  void setLevel(TXshSimpleLevel *xl);
  void onPlayCompleted() override {}
  bool doSaveImages(TFilePath fp);
  int getCurrentFrame() { return m_flipConsole->getCurrentFrame(); }
  QString getLevelZoomTitle() const { return m_title1 + m_title; }
  void setTitle(const QString &title);
  QString getTitle() const { return m_viewerTitle; }
  void showFrame(int frame);

  TFx *getPreviewedFx() const;
  TXsheet *getPreviewXsheet() const;
  bool isFreezed() const { return m_freezed; }
  TRectD getPreviewedImageGeometry() const;
  void schedulePreviewedFxUpdate();

  bool canAppend();
  bool isSavable() const;

  void setPoolIndex(int poolIndex) { m_poolIndex = poolIndex; }
  int getPoolIndex() const { return m_poolIndex; }

  // show / hide the red border line indicating the frame is under rendering
  void setIsRemakingPreviewFx(bool on) {
    m_imageViewer->setIsRemakingPreviewFx(on);
  }
  void addFreezeButtonToTitleBar();

  void setProgressBarStatus(const std::vector<UCHAR> *pbStatus);
  const std::vector<UCHAR> *getProgressBarStatus() const;

  void adaptGeometry(const TRect &interestingImgRect, const TRect &imgRect);

  // When Fx preview is called without the subcamera,
  // render the full region of camera by resize flipbook and
  // zoom-out the rendered image.
  void adaptGeometryForFullPreview(const TRect &imgRect);

  void reset();

  void onDrawFrame(int frame, const ImagePainter::VisualSettings &vs) override;

  void minimize(bool doMinimize);

  ImageViewer *getImageViewer() { return m_imageViewer; }
  TRect getLoadbox() const { return m_loadbox; }
  void setLoadbox(const TRect &box);
  TDimension getImageSize() const { return m_dim; }

  void swapBuffers() override;
  void changeSwapBehavior(bool enable) override;

private:
  // When viewing the tlv, try to cache all frames at the beginning.
  // NOTE : fromFrame and toFrame are frame numbers displayed on the flipbook
  void loadAndCacheAllTlvImages(Level level, int fromFrame, int toFrame);

  friend class PreviewFxManager;

protected:
  void clearCache();
  void adaptWidGeometry(const TRect &interestWidRect, const TRect &imgWidRect,
                        bool keepPosition);

  void dragEnterEvent(QDragEnterEvent *e) override;
  void dropEvent(QDropEvent *e) override;

  void playAudioFrame(int frame);
  TImageP getCurrentImage(int frame);

  void showEvent(QShowEvent *e) override;
  void hideEvent(QHideEvent *e) override;
  void focusInEvent(QFocusEvent *e) override;
  void resizeEvent(QResizeEvent *e) override;

  void enterEvent(QEvent *e) override { m_flipConsole->makeCurrent(); }

signals:

  void closeFlipBook(FlipBook *);
  // when freeze button is released, emit the signal to PreviewFxManager for
  // re-rendering
  void unfreezed(FlipBook *);
  // when using the flip module, this signal is to show the loaded level names
  // in application's title bar
  void imageLoaded(QString &);

protected slots:

  void onDoubleClick(QMouseEvent *me);
  void onButtonPressed(FlipConsole::EGadget button);
  void onCloseButtonPressed();
  void saveImage();

  void freeze(bool on);

public slots:

  void saveImages();
  void loadImages();

  void performFxUpdate();
  void regenerate();
  void regenerateFrame();
  void clonePreview();
  void freezePreview();
  void unfreezePreview();
};

// utility
// returns pointer to the opened flipbook to control modality.

FlipBook *viewFile(const TFilePath &fp, int from = -1, int to = -1,
                   int step = -1, int shrink = -1, TSoundTrack *snd = 0,
                   FlipBook *flipbook = 0, bool append = false,
                   bool isToonzOutput = false);

#endif  // FLIPBOOK_H
