#pragma once

#ifndef DV_ITEM_VIEW_INCLUDED
#define DV_ITEM_VIEW_INCLUDED

// TnzQt includes
#include "toonzqt/selection.h"
#include "toonzqt/lineedit.h"

// TnzCore includes
#include "tfilepath.h"

// Qt includes
#include <QWidget>
#include <QScrollArea>
#include <QToolBar>

// STD includes
#include <set>

//==============================================================

//    Forward declarations

class DvItemViewer;
class DvItemSelection;
class QMimeData;

//==============================================================

//=============================================================================

class DvItemListModel {
public:
  enum DataType {
    Name,
    Thumbnail,
    Icon,
    ToolTip,
    CreationDate,
    ModifiedDate,
    FileSize,
    FrameCount,
    FullPath,
    PlayAvailable,
    VersionControlStatus,
    FileType,
    IsFolder
  };

  enum Status {
    VC_None = 0,
    VC_ReadOnly,
    VC_Edited,
    VC_Modified,
    VC_ToUpdate,
    VC_Locked,
    VC_Unversioned,
    VC_Missing,
    VC_PartialEdited,
    VC_PartialLocked,
    VC_PartialModified
  };

public:
  DvItemListModel() : m_isDiscendent(true), m_orderType(Name) {}
  virtual ~DvItemListModel() {}

  DataType getCurrentOrderType() { return m_orderType; }
  bool isDiscendentOrder() { return m_isDiscendent; }
  void setIsDiscendentOrder(bool isDiscendentOrder) {
    m_isDiscendent = isDiscendentOrder;
  }
  void setOrderType(DataType dataType) { m_orderType = dataType; }

  // n.b. refreshData viene chiamato PRIMA di getItemCount() e getItemData()
  // vanno messe dentro refreshData() le operazioni "costose" di getItemData() e
  // getItemCount()
  virtual void refreshData(){};
  virtual int getItemCount() const = 0;
  virtual int compareData(DataType dataType, int firstIndex, int secondIndex);
  virtual void sortByDataModel(DataType dataType, bool isDiscendent) {}
  virtual QVariant getItemData(int index, DataType dataType,
                               bool isSelected = false) = 0;
  virtual QString getItemDataAsString(int index, DataType dataType);
  virtual QString getItemDataIdentifierName(DataType dataType);
  virtual void startDragDrop() {}
  virtual QMenu *getContextMenu(QWidget *parent, int index) { return 0; }
  virtual void enableSelectionCommands(TSelection *selection) {}
  virtual bool canRenameItem(int index) const { return false; }
  virtual void renameItem(int index, const QString &newName) {}
  virtual bool isSceneItem(int index) const { return false; }
  virtual bool acceptDrop(const QMimeData *data) const { return false; }
  virtual bool drop(const QMimeData *data) { return false; }

protected:
  bool m_isDiscendent;
  DataType m_orderType;
};

//=============================================================================

class ItemViewPlayWidget final : public QWidget {
  Q_OBJECT

  int m_currentItemIndex;  //-1 -> pause
  int m_timerId;
  bool m_isSliderMode;

  class PlayManager {
    TFilePath m_path;
    std::vector<TFrameId> m_fids;
    int m_currentFidIndex;
    QPixmap m_pixmap;
    QSize m_iconSize;

  public:
    PlayManager();
    ~PlayManager() {}

    void reset();
    void setInfo(DvItemListModel *model, int index);
    /*! Increase current frame if icon is computed; return true if frame is
     * increased. */
    bool increaseCurrentFrame();
    /*! If icon is computed return true, overwise start to compute it. */
    bool getCurrentFrame();
    /*! Return true if current frame index is less than fids size. */
    bool isFrameIndexInRange();
    bool setCurrentFrameIndexFromXValue(int xValue, int length);
    double currentFrameIndexToXValue(int length);
    QPixmap getCurrentPixmap();
  };

  PlayManager *m_playManager;

public:
  ItemViewPlayWidget(QWidget *parent = 0);
  ~ItemViewPlayWidget() {}

  void play();
  void stop();
  void clear();

  void setIsPlaying(DvItemListModel *model, int index);
  void setIsPlayingCurrentFrameIndex(DvItemListModel *model, int index,
                                     int xValue, int length);
  int getCurrentFramePosition(int length);

  bool isIndexPlaying(int index) { return m_currentItemIndex == index; }
  bool isPlaying() { return m_currentItemIndex != -1; }
  bool isSliderMode() { return m_isSliderMode; }

  void paint(QPainter *painter, QRect rect);

protected:
  void timerEvent(QTimerEvent *event) override;
};

//=============================================================================

class DVItemViewPlayDelegate final : public QObject {
  Q_OBJECT

  ItemViewPlayWidget *m_itemViewPlay;

public:
  DVItemViewPlayDelegate(QWidget *parent);
  ~DVItemViewPlayDelegate() {}

  void paint(QPainter *painter, QRect rect, int index);

  bool setPlayWidget(DvItemListModel *model, int index, QRect rect, QPoint pos);

public slots:

  void resetPlayWidget();

protected:
  QRect getPlayButtonRect(QRect rect);
  QRect getPlaySliderRect(QRect rect);
};

//=============================================================================

class DvItemSelection : public QObject, public TSelection {
  Q_OBJECT

  std::set<int> m_selectedIndices;
  DvItemListModel *m_model;

public:
  DvItemSelection();

  void select(int index, bool on = true);
  void select(int *indices, int indicesCount);
  void selectNone() override;
  void selectAll();

  bool isSelected(int index) const {
    return m_selectedIndices.count(index) > 0;
  }
  bool isEmpty() const override { return m_selectedIndices.empty(); }

  const std::set<int> &getSelectedIndices() const { return m_selectedIndices; }

  void setModel(DvItemListModel *model);
  DvItemListModel *getModel() const { return m_model; }

  void enableCommands() override;

signals:

  void itemSelectionChanged();
};

//=============================================================================

class DvItemViewerPanel final : public QFrame, public TSelection::View {
  Q_OBJECT

  QColor m_alternateBackground;     // alaternate bg color for teble view
                                    // (170,170,170)
  QColor m_textColor;               // text color (black)
  QColor m_selectedTextColor;       // selected item text color (white)
  QColor m_folderTextColor;         // folder item text color (blue)
  QColor m_selectedItemBackground;  // selected item background color (0,0,128)

  Q_PROPERTY(QColor AlternateBackground READ getAlternateBackground WRITE
                 setAlternateBackground)
  Q_PROPERTY(QColor TextColor READ getTextColor WRITE setTextColor)
  Q_PROPERTY(QColor SelectedTextColor READ getSelectedTextColor WRITE
                 setSelectedTextColor)
  Q_PROPERTY(
      QColor FolderTextColor READ getFolderTextColor WRITE setFolderTextColor)
  Q_PROPERTY(QColor SelectedItemBackground READ getSelectedItemBackground WRITE
                 setSelectedItemBackground)

public:
  enum ViewType { ListView = 0, TableView, ThumbnailView };
  void setAlternateBackground(const QColor &color) {
    m_alternateBackground = color;
  }
  QColor getAlternateBackground() const { return m_alternateBackground; }
  void setTextColor(const QColor &color) { m_textColor = color; }
  QColor getTextColor() const { return m_textColor; }
  void setSelectedTextColor(const QColor &color) {
    m_selectedTextColor = color;
  }
  QColor getSelectedTextColor() const { return m_selectedTextColor; }
  void setFolderTextColor(const QColor &color) { m_folderTextColor = color; }
  QColor getFolderTextColor() const { return m_folderTextColor; }
  void setSelectedItemBackground(const QColor &color) {
    m_selectedItemBackground = color;
  }
  QColor getSelectedItemBackground() const { return m_selectedItemBackground; }

private:
  ViewType to_enum(int n) {
    switch (n) {
    case 0:
      return ListView;
    case 1:
      return TableView;
    case 2:
      return ThumbnailView;
    default:
      return ThumbnailView;
    }
  }

  DvItemSelection *m_selection;
  bool m_globalSelectionEnabled;
  bool m_multiSelectionEnabled;

  DvItemViewer *m_viewer;

  DVItemViewPlayDelegate *m_itemViewPlayDelegate;
  bool m_isPlayDelegateDisable;

  DVGui::LineEdit *m_editFld;
  ViewType m_viewType;
  bool m_singleColumnEnabled;
  bool m_centerAligned;

  // view parameters
  int m_itemSpacing;
  int m_xMargin, m_yMargin, m_itemPerRow;
  QSize m_itemSize;
  QSize m_iconSize;
  bool m_noContextMenu;
  void updateViewParameters(int panelWidth);
  void updateViewParameters() { updateViewParameters(width()); }
  QPoint m_startDragPosition;
  QPoint m_lastMousePos;
  int m_currentIndex;
  std::vector<std::pair<DvItemListModel::DataType, std::pair<int, bool>>>
      m_columns;  // type, <width, visibility>
  DvItemListModel *getModel() const;
  int getItemCount() const {
    return getModel() ? getModel()->getItemCount() : 0;
  }
  QColor m_missingColor;

public:
  DvItemViewerPanel(DvItemViewer *viewer, bool noContextMenu,
                    bool multiselectionEnabled, QWidget *parent);

  void enableGlobalSelection(bool enabled) {
    if (!enabled) m_selection->makeNotCurrent();
    m_globalSelectionEnabled = enabled;
  }

  void setItemViewPlayDelegate(DVItemViewPlayDelegate *playDelegate);
  DVItemViewPlayDelegate *getItemViewPlayDelegate();

  void addColumn(DvItemListModel::DataType dataType, int width);
  void setColumnWidth(DvItemListModel::DataType dataType, int width);
  void getColumns(
      std::vector<std::pair<DvItemListModel::DataType, std::pair<int, bool>>>
          &columns) {
    columns = m_columns;
  };
  bool getVisibility(DvItemListModel::DataType dataType);
  void setVisibility(DvItemListModel::DataType dataType, bool value);
  int getContentMinimumWidth();
  int getContentHeight(int width);

  QSize getItemSize() const;

  QSize getIconSize() const { return m_iconSize; }

  int pos2index(const QPoint &pos) const;
  QRect index2pos(int index) const;

  QRect getCaptionRect(int index) const;

  void paintThumbnailItem(QPainter &p, int index);
  void paintListItem(QPainter &p, int index);
  void paintTableItem(QPainter &p, int index);

  // da TSelection::View
  void onSelectionChanged() override { update(); }

  const std::set<int> &getSelectedIndices() const;

  void enableSingleColumn(bool enabled) { m_singleColumnEnabled = enabled; }
  bool isSingleColumnEnabled() const { return m_singleColumnEnabled; }

  void setAlignCenter() { m_centerAligned = true; }

  DvItemSelection *getSelection() const { return m_selection; }
  void setSelection(DvItemSelection *selection);  // gets ownership
  void setMissingTextColor(const QColor &color);

protected:
  void paintEvent(QPaintEvent *) override;
  void mousePressEvent(QMouseEvent *) override;
  void mouseMoveEvent(QMouseEvent *) override;
  void mouseReleaseEvent(QMouseEvent *) override;
  void mouseDoubleClickEvent(QMouseEvent *) override;
  void contextMenuEvent(QContextMenuEvent *) override;
  bool event(QEvent *event) override;

signals:
  void viewTypeChange(DvItemViewerPanel::ViewType viewType);

public slots:
  void setListView();
  void setTableView();
  void setThumbnailsView();
  void rename();
  // for exporting the file information to text format file
  void exportFileList();
};

//=============================================================================

class DvItemViewer : public QScrollArea {
  Q_OBJECT

public:
  enum WindowType { Browser = 0, Cast };
  WindowType m_windowType;

private:
  DvItemListModel *m_model;
  DvItemViewerPanel *m_panel;

protected:
  void resizeEvent(QResizeEvent *) override;

public:
  DvItemViewer(QWidget *parent, bool noContextMenu = false,
               bool multiSelectionEnabled = false, enum WindowType = Browser);

  void resetVerticalScrollBar();
  DvItemListModel *getModel() const { return m_model; }
  void setModel(DvItemListModel *model);  // gets ownership

  const std::set<int> &getSelectedIndices() const {
    return m_panel->getSelectedIndices();
  }

  void updateContentSize();
  void refresh();

  DvItemViewerPanel *getPanel() { return m_panel; }

  virtual void draw(QPainter &p) {}
  void notifyClick(int index) {
    emit clickedItem(index);
    emit selectedItems(m_panel->getSelectedIndices());
  }

  void enableGlobalSelection(bool enabled) {
    m_panel->enableGlobalSelection(enabled);
  }
  void selectNone();
  void keyPressEvent(QKeyEvent *event) override;

protected:
  void dragEnterEvent(QDragEnterEvent *event) override;
  void dropEvent(QDropEvent *event) override;

signals:
  void clickedItem(int index);
  void selectedItems(const std::set<int> &indexes);
};

//=============================================================================

class DvItemViewerTitleBar final : public QWidget {
  Q_OBJECT

  DvItemViewer *m_itemViewer;
  bool m_isInteractive;
  int m_dragColumnIndex;
  QPoint m_pos;

public:
  DvItemViewerTitleBar(DvItemViewer *itemViewer, QWidget *parent = 0,
                       bool isInteractive = true);

protected slots:
  void onViewTypeChanged(DvItemViewerPanel::ViewType viewType);

protected:
  void mouseMoveEvent(QMouseEvent *) override;
  void openContextMenu(QMouseEvent *);
  void mousePressEvent(QMouseEvent *) override;
  void paintEvent(QPaintEvent *) override;
};

//=============================================================================

class DvItemViewerButtonBar final : public QToolBar {
  Q_OBJECT
  QAction *m_folderBack;
  QAction *m_folderFwd;

public:
  DvItemViewerButtonBar(DvItemViewer *itemViewer, QWidget *parent = 0);

public slots:
  void onHistoryChanged(bool, bool);
  void onPreferenceChanged(const QString &);

signals:
  void folderUp();
  void newFolder();
  void folderBack();
  void folderFwd();
};

#endif  // DV_ITEM_VIEW_INCLUDED
