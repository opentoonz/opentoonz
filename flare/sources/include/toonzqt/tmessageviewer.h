#pragma once

#include <QFrame>
#include <QModelIndex>
#include <QListView>

#include "tcommon.h"
#include "tmsgcore.h"

#undef DVAPI
#undef DVVAR
#ifdef TOONZQT_EXPORTS
#define DVAPI DV_EXPORT_API
#define DVVAR DV_EXPORT_VAR
#else
#define DVAPI DV_IMPORT_API
#define DVVAR DV_IMPORT_VAR
#endif

class MySortFilterProxyModel;
class QCheckBox;
class QStandardItemModel;

class DVAPI TMessageRepository final : public QObject {
  QStandardItemModel *m_sim;

  Q_OBJECT
public:
  static TMessageRepository *instance();
  TMessageRepository();
  QStandardItemModel *getModel() const { return m_sim; }
  void clear();

public slots:
  void messageReceived(int, const QString &);

signals:
  void openMessageCenter();  // TMessageRepository emits this signal to indicate
                             // that the TMessageViewer should be made visible
};

//---------------------------------------------------------------------------------------

class DVAPI TMessageViewer final : public QFrame {
  Q_OBJECT

protected:
  static std::vector<TMessageViewer *> m_tmsgViewers;
  MySortFilterProxyModel *m_proxyModel;
  void rowsInserted(const QModelIndex &parent, int start, int end);

public:
  QCheckBox *m_redCheck, *m_greenCheck, *m_yellowCheck;
  TMessageViewer(QWidget *);
  static bool isTMsgVisible();
public slots:
  void onClicked(bool);
  void refreshFilter(int);
};
