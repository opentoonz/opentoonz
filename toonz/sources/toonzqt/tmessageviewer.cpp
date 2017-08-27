

#include "toonzqt/tmessageviewer.h"
#include "toonzqt/dvdialog.h"
#include <QVBoxLayout>
#include <QPushButton>
#include <QListView>
#include <QStandardItemModel>
#include <QSortFilterProxyModel>
#include <QCheckBox>
#include <QLabel>

namespace {

QIcon gRedIcon;
QIcon gGreenIcon;
QIcon gYellowIcon;

//-------------------------------------------------------------------------

class MyQListView final : public QListView {
public:
  MyQListView(QWidget *parent) : QListView(parent) {}

protected:
  void rowsInserted(const QModelIndex &parent, int start, int end) override {
    QListView::rowsInserted(parent, start, end);
    scrollToBottom();
  }
};

//----------------------------------------------------------------------------------

}  // namespace

class MySortFilterProxyModel final : public QSortFilterProxyModel {
public:
  MySortFilterProxyModel(QObject *parent) : QSortFilterProxyModel(parent) {}

  bool filterAcceptsRow(int sourceRow,
                        const QModelIndex &sourceParent) const override {
    TMessageViewer *v = dynamic_cast<TMessageViewer *>(parent());
    assert(v);

    QIcon ic = ((QStandardItemModel *)sourceModel())->item(sourceRow)->icon();
    if (ic.cacheKey() == gGreenIcon.cacheKey())
      return v->m_greenCheck->isChecked();
    else if (ic.cacheKey() == gYellowIcon.cacheKey())
      return v->m_yellowCheck->isChecked();
    else if (ic.cacheKey() == gRedIcon.cacheKey())
      return v->m_redCheck->isChecked();
    else
      return true;
  }
};

//----------------------------------------------------------------------------------

void TMessageViewer::onClicked(bool) {
  TMessageRepository::instance()->clear();

  /*return;
  static int count=0;
  count++;
  DVGui::MsgBox((count%3)==1?INFORMATION:(count%3)==2?WARNING:CRITICAL,
  "messaggio "+QString::number(count));*/
}

//-----------------------------------------------------------

void TMessageViewer::refreshFilter(int) { m_proxyModel->invalidate(); }

//---------------------------------------------------

void TMessageRepository::clear() { m_sim->clear(); }

//---------------------------------------------------

TMessageRepository::TMessageRepository() : m_sim(new QStandardItemModel()) {}

//---------------------------------------------------

TMessageRepository *TMessageRepository::instance() {
  static TMessageRepository *theObject = 0;
  if (theObject == 0) {
    theObject = new TMessageRepository();
    bool ret =
        connect(TMsgCore::instance(), SIGNAL(sendMessage(int, const QString &)),
                theObject, SLOT(messageReceived(int, const QString &)));
    assert(ret);
    // TMsgCore::instance()->getConnectionName();
  }
  return theObject;
}

//---------------------------------------------------

void TMessageRepository::messageReceived(int type, const QString &message) {
  if (gRedIcon.isNull()) {
    gRedIcon    = QIcon(":Resources/tmsg_error.svg");
    gYellowIcon = QIcon(":Resources/tmsg_warning.svg");
    gGreenIcon  = QIcon(":Resources/tmsg_info.svg");
  }

  switch (type) {
  case DVGui::INFORMATION:
    m_sim->appendRow(new QStandardItem(gGreenIcon, message));
    break;
  case DVGui::WARNING:
    m_sim->appendRow(new QStandardItem(gYellowIcon, message));
    break;
  case DVGui::CRITICAL:
    m_sim->appendRow(new QStandardItem(gRedIcon, message));
    break;
  }

  if (type == DVGui::CRITICAL ||
      (type == DVGui::WARNING && !TMessageViewer::isTMsgVisible()))
    DVGui::MsgBoxInPopup(DVGui::MsgType(type), message);
}

//---------------------------------------------------------------------------------

std::vector<TMessageViewer *> TMessageViewer::m_tmsgViewers;

bool TMessageViewer::isTMsgVisible() {
  for (int i = 0; i < (int)m_tmsgViewers.size(); i++)
    if (m_tmsgViewers[i]->isVisible()) return true;
  return false;
}

//--------------------------------------------------

TMessageViewer::TMessageViewer(QWidget *parent) : QFrame(parent) {
  m_tmsgViewers.push_back(this);

  bool ret = true;
  setFrameStyle(QFrame::StyledPanel);
  setObjectName("OnePixelMarginFrame");
  QVBoxLayout *vLayout = new QVBoxLayout();
  vLayout->setMargin(0);
  QFrame *fr = new QFrame();

  QHBoxLayout *hLayout = new QHBoxLayout();
  hLayout->setMargin(0);
  fr->setLayout(hLayout);
  fr->setFixedHeight(24);
  fr->setStyleSheet("background-color: rgb(210,210,210);");
  hLayout->addSpacing(4);

  hLayout->addWidget(new QLabel("Display:  "));

  m_redCheck = new QCheckBox(tr("Errors"));
  m_redCheck->setChecked(true);
  ret = ret && connect(m_redCheck, SIGNAL(stateChanged(int)),
                       SLOT(refreshFilter(int)));
  hLayout->addWidget(m_redCheck);

  m_yellowCheck = new QCheckBox(tr("Warnings"));
  m_yellowCheck->setChecked(true);
  ret = ret && connect(m_yellowCheck, SIGNAL(stateChanged(int)),
                       SLOT(refreshFilter(int)));
  hLayout->addWidget(m_yellowCheck);

  m_greenCheck = new QCheckBox(tr("Infos"));
  m_greenCheck->setChecked(true);
  ret = ret && connect(m_greenCheck, SIGNAL(stateChanged(int)),
                       SLOT(refreshFilter(int)));
  hLayout->addWidget(m_greenCheck);

  hLayout->addStretch();

  QPushButton *pb = new QPushButton(tr(" Clear "));
  ret = ret && connect(pb, SIGNAL(clicked(bool)), SLOT(onClicked(bool)));
  hLayout->addWidget(pb);

  hLayout->addSpacing(4);

  vLayout->addWidget(fr);

  MyQListView *lv = new MyQListView(this);
  lv->setAlternatingRowColors(true);
  lv->setEditTriggers(QAbstractItemView::NoEditTriggers);
  lv->setAutoScroll(true);

  m_proxyModel = new MySortFilterProxyModel(this);
  m_proxyModel->setDynamicSortFilter(true);

  m_proxyModel->setSourceModel(TMessageRepository::instance()->getModel());
  lv->setModel(m_proxyModel);

  vLayout->addWidget(lv);
  setLayout(vLayout);

  assert(ret);
}
