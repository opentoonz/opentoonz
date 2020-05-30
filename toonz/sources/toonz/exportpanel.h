#pragma once

#ifndef EXPORTPANEL_H
#define EXPORTPANEL_H

/******************************************************************************
 **
 ** exportpanel.h
 **
 ** comprende:
 ** - ClipListViewer che controlla l'elenco delle scene di cui fare il
 **   rendering
 ** - OutputIconViewer che visualizza l'icona del file generato
 **
 ******************************************************************************/
#include "pane.h"
#include "dvitemview.h"
#include "tfilepath.h"
#include "tgeometry.h"
#include "moviegenerator.h"

class QComboBox;
class QCheckBox;
class TOutputProperties;

class ToonzScene;
class TOutputProperties;
class RenderController;

namespace DVGui {
class ProgressDialog;
class FileField;
class LineEdit;
}

//-----------------------------------------------------------------------------

class ClipListViewer final : public DvItemViewer, public DvItemListModel {
  Q_OBJECT

  static const char *m_mimeFormat;
  int m_dropInsertionPoint;

  void setDropInsertionPoint(const QPoint &pos);

  bool m_loadSceneFromExportPanel;

public:
  ClipListViewer(QWidget *parent = 0);

  RenderController *getController() const;

  void removeSelectedClips();
  void moveSelectedClips(int dstIndex);

  void getSelectedClips(std::vector<TFilePath> &selectedClips);

  // da DvItemListModel
  int getItemCount() const override;
  QVariant getItemData(int index, DataType dataType,
                       bool isSelected = false) override;
  bool isSceneItem(int index) const override { return true; }
  QMenu *getContextMenu(QWidget *parent, int index) override;
  void enableSelectionCommands(TSelection *) override;
  void startDragDrop() override;

  void draw(QPainter &) override;
  void addCurrentScene();

protected:
  void dragEnterEvent(QDragEnterEvent *event) override;
  void dragMoveEvent(QDragMoveEvent *event) override;
  void dropEvent(QDropEvent *event) override;
  void dragLeaveEvent(QDragLeaveEvent *event) override;

public slots:
  void resetList();
  void loadScene();
};

//-----------------------------------------------------------------------------

class ExportPanel final : public TPanel {
  Q_OBJECT

  ClipListViewer *m_clipListViewer;
  DVGui::FileField *m_saveInFileFld;
  DVGui::LineEdit *m_fileNameFld;
  QComboBox *m_fileFormat;
  QCheckBox *m_useMarker;

public:
#if QT_VERSION >= 0x050500
  ExportPanel(QWidget *parent = 0, Qt::WindowFlags flags = 0);
#else
  ExportPanel(QWidget *parent = 0, Qt::WFlags flags = 0);
#endif
  ~ExportPanel();
  void loadExportSettings();
  void saveExportSettings();

public slots:
  void generateMovie();
  void onUseMarkerToggled(bool toggled);
  void onSceneSwitched();
  void onFormatChanged(const QString &);
  void openSettingsPopup();

protected:
  void showEvent(QShowEvent *) override;
};

class RenderController final : public QObject,
                               public MovieGenerator::Listener {  // singleton
  Q_OBJECT
  DVGui::ProgressDialog *m_progressDialog;
  int m_frame;
  bool m_rendering;
  bool m_cancelled;

  std::string m_movieExt;
  bool m_useMarkers;
  TOutputProperties *m_properties;
  std::vector<TFilePath> m_clipList;

protected slots:
  void myCancel();

signals:
  void movieGenerated();

public:
  RenderController();
  ~RenderController();

  static RenderController *instance() {
    static RenderController _instance;
    return &_instance;
  }

  void setMovieExt(std::string ext);
  std::string getMovieExt() const { return m_movieExt; }

  void setOutputPropertites(TOutputProperties *properties) {
    m_properties = properties;
  }
  TOutputProperties *getOutputPropertites() { return m_properties; }

  void setUseMarkers(bool useMarkers);
  bool getUseMarkers() const { return m_useMarkers; }

  // gestione cliplist
  int getClipCount() const { return (int)m_clipList.size(); }
  // n.b. ritorna TFilePath() se e' la scena corrente
  TFilePath getClipPath(int index) const;
  void setClipPath(int index, const TFilePath &path);
  void addClipPath(const TFilePath &path);
  void insertClipPath(int index, const TFilePath &path);
  void removeClipPath(int index);
  void resetClipList();

  void generateMovie(TFilePath outPath, bool emitSignal = true);

  static int computeClipFrameCount(const TFilePath &clipPath, bool useMarkers,
                                   int *frameOffset = 0);
  static int computeTotalFrameCount(const std::vector<TFilePath> &clipList,
                                    bool useMarkers = false);
  bool addScene(MovieGenerator &g, ToonzScene *scene);
  bool onFrameCompleted(int frameCount) override;
  void getMovieProperties(const TFilePath &scenePath, TDimension &resolution);
};

#endif  // EXPORTPANEL_H
