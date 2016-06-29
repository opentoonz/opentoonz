#include <memory>

#include "timestretchpopup.h"

// Tnz6 includes
#include "filmstripcommand.h"
#include "cellselection.h"
#include "tapp.h"
#include "menubarcommandids.h"

// TnzQt includes
#include "toonzqt/tselectionhandle.h"
#include "historytypes.h"

// TnzLib includes
#include "toonz/tscenehandle.h"
#include "toonz/txsheet.h"
#include "toonz/txsheethandle.h"
#include "toonz/txshcell.h"
#include "toonz/txshcolumn.h"

// TnzCore includes
#include "tundo.h"

// Qt includes
#include <QPushButton>
#include <QLabel>
#include <QHBoxLayout>
#include <QComboBox>
#include <QMainWindow>

//=============================================================================
// TimeStretch
//-----------------------------------------------------------------------------

//-----------------------------------------------------------------------------
namespace {
//-----------------------------------------------------------------------------

class TimeStretchUndo final : public TUndo {
  int m_r0, m_r1;
  int m_c0, m_c1;
  int m_newRange;
  std::unique_ptr<TXshCell[]> m_cells;

  // servono per modificare la selezione
  TimeStretchPopup::STRETCH_TYPE m_type;
  int m_c0Old;
  int m_c1Old;

public:
  TimeStretchUndo(int r0, int c0, int r1, int c1, int newRange,
                  TimeStretchPopup::STRETCH_TYPE type)
      : m_r0(r0)
      , m_c0(c0)
      , m_r1(r1)
      , m_c1(c1)
      , m_newRange(newRange)
      , m_type(type)
      , m_c0Old(0)
      , m_c1Old(0) {
    int nr = m_r1 - m_r0 + 1;
    int nc = m_c1 - m_c0 + 1;
    assert(nr > 0 && nc > 0);
    m_cells.reset(new TXshCell[nr * nc]);
    assert(m_cells);
    int k = 0;
    for (int c = c0; c <= c1; c++)
      for (int r = r0; r <= r1; r++)
        m_cells[k++] =
            TApp::instance()->getCurrentXsheet()->getXsheet()->getCell(r, c);
  }

  ~TimeStretchUndo() {}

  void setOldColumnRange(int c0, int c1) {
    m_c0Old = c0;
    m_c1Old = c1;
  }

  void undo() const override {
    TApp *app    = TApp::instance();
    TXsheet *xsh = app->getCurrentXsheet()->getXsheet();
    int oldNr    = m_newRange;
    int nr       = m_r1 - m_r0 + 1;

    if (nr > oldNr)  // Se avevo cancellato delle celle devo reinserirle
    {
      int c;
      for (c = m_c0; c <= m_c1; c++) {
        int dn = nr - oldNr;
        xsh->insertCells(m_r0, c, dn);
        int i;
        for (i = 0; i < nr; i++)
          xsh->setCell(i + m_r0, c, m_cells[i + nr * (c - m_c0)]);
      }
    } else  // Altrimenti devo rimuoverle
    {
      int c;
      for (c = m_c0; c <= m_c1; c++) {
        int i;
        for (i = 0; i < nr; i++)
          xsh->setCell(i + m_r0, c, m_cells[i + nr * (c - m_c0)]);
        int dn = oldNr - nr;
        xsh->removeCells(m_r1, c, dn);
      }
    }

    TCellSelection *selection = dynamic_cast<TCellSelection *>(
        app->getCurrentSelection()->getSelection());
    if (selection) {
      if (m_type == TimeStretchPopup::eRegion)
        selection->selectCells(m_r0, m_c0, m_r1, m_c1);
      else if (m_type == TimeStretchPopup::eFrameRange)
        selection->selectCells(m_r0, m_c0Old, m_r1, m_c1Old);
    }

    app->getCurrentXsheet()->notifyXsheetChanged();
    app->getCurrentXsheet()->notifyXsheetSoundChanged();
  }

  void redo() const override {
    if (m_r1 - m_r0 < 0 || m_c1 - m_c0 < 0) return;
    TApp *app = TApp::instance();
    app->getCurrentXsheet()->getXsheet()->timeStretch(m_r0, m_c0, m_r1, m_c1,
                                                      m_newRange);

    TCellSelection *selection = dynamic_cast<TCellSelection *>(
        app->getCurrentSelection()->getSelection());
    if (selection) {
      if (m_type == TimeStretchPopup::eRegion)
        selection->selectCells(m_r0, m_c0, m_r0 + m_newRange - 1, m_c1);
      else if (m_type == TimeStretchPopup::eFrameRange)
        selection->selectCells(m_r0, m_c0Old, m_r0 + m_newRange - 1, m_c1Old);
    }
    app->getCurrentXsheet()->notifyXsheetChanged();
    app->getCurrentXsheet()->notifyXsheetSoundChanged();
  }

  int getSize() const override { return sizeof(*this); }

  QString getHistoryString() override { return QObject::tr("Time Stretch"); }
  int getHistoryType() override { return HistoryType::Xsheet; }
};

//-----------------------------------------------------------------------------

void timeStretch(int newRange, TimeStretchPopup::STRETCH_TYPE type) {
  TCellSelection::Range range;
  TCellSelection *selection = dynamic_cast<TCellSelection *>(
      TApp::instance()->getCurrentSelection()->getSelection());
  if (type != TimeStretchPopup::eWholeXsheet) {
    if (!selection) {
      DVGui::error(QObject::tr("The current selection is invalid."));
      return;
    }
    range = selection->getSelectedCells();
  }
  int r0 = range.m_r0;
  int r1 = range.m_r1;
  int c0 = 0, c1 = 0;
  TXsheet *xsheet = TApp::instance()->getCurrentXsheet()->getXsheet();
  if (type == TimeStretchPopup::eRegion) {
    c0 = range.m_c0;
    c1 = range.m_c1;
  } else {
    if (type == TimeStretchPopup::eWholeXsheet) {
      r0 = 0;
      r1 = xsheet->getFrameCount() - 1;
    }
    c1 = xsheet->getColumnCount() - 1;
    int c;
    for (c = c1; c >= c0; c--)
      if (!xsheet->getColumn(c) && !xsheet->getColumn(c)->isEmpty()) break;
  }
  assert(r1 >= r0 && c1 >= c0);
  if (r1 - r0 + 1 == newRange) return;

  TimeStretchUndo *undo = new TimeStretchUndo(r0, c0, r1, c1, newRange, type);

  xsheet->timeStretch(r0, c0, r1, c1, newRange);

  if (type == TimeStretchPopup::eFrameRange)
    undo->setOldColumnRange(range.m_c0, range.m_c1);
  TUndoManager::manager()->add(undo);

  // Modifico la selezione in base al tipo di comando effettuato
  if (type != TimeStretchPopup::eWholeXsheet) {
    assert(selection);
    selection->selectCells(r0, range.m_c0, r0 + newRange - 1, range.m_c1);
  }

  TApp::instance()->getCurrentScene()->setDirtyFlag(true);
  TApp::instance()->getCurrentXsheet()->notifyXsheetChanged();
  TApp::instance()->getCurrentXsheet()->notifyXsheetSoundChanged();
}

//-----------------------------------------------------------------------------
}  // namespace
//-----------------------------------------------------------------------------

//=============================================================================
// TimeStretchPopup
//-----------------------------------------------------------------------------

TimeStretchPopup::TimeStretchPopup()
    : Dialog(TApp::instance()->getMainWindow(), true, true, "TimeStretch")
    , m_currentStretchType(eRegion) {
  setModal(false);
  setWindowTitle(tr("Time Stretch"));

  beginVLayout();

  m_stretchType = new QComboBox(this);
  m_stretchType->setFixedHeight(DVGui::WidgetHeight);
  QStringList viewType;
  viewType << tr("Selected Cells") << tr("Selected Frame Range")
           << tr("Whole Xsheet");
  m_stretchType->addItems(viewType);
  connect(m_stretchType, SIGNAL(currentIndexChanged(int)),
          SLOT(setCurrentStretchType(int)));
  addWidget(tr("Stretch:"), m_stretchType);

  QHBoxLayout *rangeLayout = new QHBoxLayout(this);
  m_oldRange               = new QLabel("0", this);
  m_oldRange->setFixedSize(43, DVGui::WidgetHeight);
  rangeLayout->addWidget(m_oldRange);
  rangeLayout->addSpacing(10);
  m_newRangeFld = new DVGui::IntLineEdit(this);
  rangeLayout->addWidget(new QLabel(tr("New Range:")), 1, Qt::AlignRight);
  rangeLayout->addWidget(m_newRangeFld, 0, Qt::AlignRight);
  addLayout(tr("Old Range:"), rangeLayout);

  endVLayout();

  m_okBtn = new QPushButton(tr("Stretch"), this);
  m_okBtn->setDefault(true);
  m_cancelBtn = new QPushButton(tr("Cancel"), this);
  bool ret    = connect(m_okBtn, SIGNAL(clicked()), this, SLOT(stretch()));
  ret = ret && connect(m_cancelBtn, SIGNAL(clicked()), this, SLOT(reject()));
  assert(ret);

  addButtonBarWidget(m_okBtn, m_cancelBtn);
}

//-------------------------------------------------------------------

void TimeStretchPopup::showEvent(QShowEvent *) {
  TSelectionHandle *selectionHandle = TApp::instance()->getCurrentSelection();
  bool ret = connect(selectionHandle, SIGNAL(selectionChanged(TSelection *)),
                     this, SLOT(updateValues(TSelection *)));
  ret = ret && connect(selectionHandle,
                       SIGNAL(selectionSwitched(TSelection *, TSelection *)),
                       this, SLOT(updateValues(TSelection *, TSelection *)));
  TXsheetHandle *xsheetHandle = TApp::instance()->getCurrentXsheet();
  ret = ret && connect(xsheetHandle, SIGNAL(xsheetChanged()), this,
                       SLOT(updateValues()));
  assert(ret);
  updateValues(selectionHandle->getSelection());
}

//-------------------------------------------------------------------

void TimeStretchPopup::hideEvent(QHideEvent *e) {
  TSelectionHandle *selectionHandle = TApp::instance()->getCurrentSelection();
  bool ret = disconnect(selectionHandle, SIGNAL(selectionChanged(TSelection *)),
                        this, SLOT(updateValues(TSelection *)));
  ret = ret && connect(selectionHandle,
                       SIGNAL(selectionSwitched(TSelection *, TSelection *)),
                       this, SLOT(updateValues(TSelection *, TSelection *)));
  TXsheetHandle *xsheetHandle = TApp::instance()->getCurrentXsheet();
  ret = ret && disconnect(xsheetHandle, SIGNAL(xsheetChanged()), this,
                          SLOT(updateValues()));
  assert(ret);
  Dialog::hideEvent(e);
}
//-------------------------------------------------------------------

void TimeStretchPopup::updateValues() {
  updateValues(TApp::instance()->getCurrentSelection()->getSelection());
}

//-------------------------------------------------------------------

void TimeStretchPopup::updateValues(TSelection *selection) {
  int from = 0, to = 0, newRange = 0;
  if (m_currentStretchType == eWholeXsheet) {
    TXsheet *xsheet = TApp::instance()->getCurrentXsheet()->getXsheet();
    newRange        = xsheet->getFrameCount();
  } else {
    TCellSelection *cellCelection = dynamic_cast<TCellSelection *>(selection);
    if (cellCelection) {
      int c0, c1;
      cellCelection->getSelectedCells(from, c0, to, c1);
      newRange = to - from + 1;
    }
  }

  m_newRangeFld->setText(QString::number(newRange));
  m_oldRange->setText(QString::number(newRange));
}

//-------------------------------------------------------------------

void TimeStretchPopup::setCurrentStretchType(int index) {
  if (m_currentStretchType == STRETCH_TYPE(index)) return;
  m_currentStretchType = STRETCH_TYPE(index);
  updateValues();
}

//-------------------------------------------------------------------

void TimeStretchPopup::stretch() {
  timeStretch(m_newRangeFld->text().toInt(), m_currentStretchType);
  accept();
}
