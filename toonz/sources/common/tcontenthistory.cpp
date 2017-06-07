

#include "tcontenthistory.h"
#include <QString>
#include <QDateTime>
#include <QStringList>
#include <QProcess>
#include <QTextStream>
#ifdef _WIN32
#include <windows.h>
#endif
#include <map>
#include <QFile>
/*
class HistoryRec
{
QDateTime m_date;

HistoryRec(const QDateTime&date QString&user, const QString&machine)
 : date(date)
 , m_user
};
*/

using namespace std;

//-------------------------------------------------------

TContentHistory::TContentHistory(bool isLevel)
    : m_isLevel(isLevel), m_frozenHistory() {}

//-------------------------------------------------------

TContentHistory::~TContentHistory() {}

//-------------------------------------------------------

TContentHistory *TContentHistory::clone() const {
  TContentHistory *history = new TContentHistory(m_isLevel);
  history->deserialize(serialize());
  return history;
}

//-------------------------------------------------------

void TContentHistory::deserialize(const QString &currentHistory) {
  m_frozenHistory = currentHistory;
}

/*
QStringList strlist = str.split("#");

//skip strlist[0], is the header
int i;
for (i=1; i<strlist.size();i++)
  {
  QString str = strlist.at(i);
  parseLine(&str);
   }
}*/

//--------------------------------------------------------------------

inline QString blanks(const QString &str, int count = 15) {
  QString res = str;
  if (res.size() >= count)
    res.truncate(count);
  else
    res += QString(count - str.size(), ' ');
  return res;
}

//--------------------------------------------------------------------

inline QString getStr(const TFrameId &id) {
  if (id.getLetter() != 0)
    return QString::number(id.getNumber()) + id.getLetter();
  else
    return QString::number(id.getNumber());
}

//--------------------------------------------------------------------

const QString Fmt = "dd MMM yy   hh:mm";

static QString getLine(int counter, const QDateTime &date,
                       const set<TFrameId> &frames) {
  static QString user;
  static QString machine;
  if (user == "") {
    QStringList list = QProcess::systemEnvironment();
    int j;
    for (j = 0; j < list.size(); j++) {
      QString value = list.at(j);
      if (value.startsWith("USERNAME="))
        user = blanks(value.right(value.size() - 9));
      else if (value.startsWith("COMPUTERNAME="))
        machine = blanks(value.right(value.size() - 13));
    }
  }

  if (frames.empty())
    return "| #" + blanks(QString::number(counter), 4) +
           blanks(date.toString(Fmt), 20) + blanks(machine, 12) + user + " |";

  QString framesStr;

  set<TFrameId>::const_iterator it = frames.begin();
  TFrameId first, last;
  while (it != frames.end()) {
    first = last = *it;
    ++it;
    while (it != frames.end() &&
           ((*it).getNumber() == last.getNumber() ||  // 1a, 1b....
            (*it).getNumber() == last.getNumber() + 1)) {
      assert(*it > last);
      last = *it, ++it;
    }
    framesStr += getStr(first) + ((first != last) ? "-" + getStr(last) : "");
    if (it != frames.end()) framesStr += ", ";
  }

  return "| #" + blanks(QString::number(counter), 4) +
         blanks(date.toString(Fmt), 20) + blanks(machine, 12) + user +
         blanks(framesStr, 20) + " |";
}

//--------------------------------------------------------------------

static int getCurrentCount(const QString &str) {
  if (str == "") return 0;

  int from = str.lastIndexOf('#') + 1;
  assert(from != -1);

  int to = str.indexOf(" ", from) - 1;
  assert(to != -1);
  assert(from <= to);

  return (str.mid(from, to - from + 1)).toInt();
}

//--------------------------------------------------------------------

const QString TContentHistory::currentToString() const {
  if (m_records.empty()) return "";

  int counter = getCurrentCount(m_frozenHistory);

  if (!m_isLevel) {
    assert(m_records.size() == 1);
    return getLine(++counter, m_records.begin()->second, set<TFrameId>());
  }

  QString out;
  std::multimap<QDateTime, TFrameId> dateSorted;
  std::map<TFrameId, QDateTime>::const_iterator it;
  for (it = m_records.begin(); it != m_records.end(); ++it)
    dateSorted.insert(pair<QDateTime, TFrameId>(it->second, it->first));

  std::multimap<QDateTime, TFrameId>::const_iterator it1 = dateSorted.begin();
  QDateTime currDate = it1->first;

  while (it1 != dateSorted.end()) {
    set<TFrameId> frames;
    while (it1 != dateSorted.end() && currDate == it1->first) {
      frames.insert(it1->second);
      ++it1;
    }
    assert(!frames.empty());
    out += getLine(++counter, currDate, frames);
    if (it1 != dateSorted.end()) currDate = it1->first;
  }

  return out;
}

//--------------------------------------------------------------------

const QString TContentHistory::serialize() const {
  const QString currentHistory = currentToString();

  if (m_frozenHistory != "")
    return m_frozenHistory + currentHistory;
  else if (currentHistory != "") {
    if (m_isLevel)
      return "| #    DATE:       Time:   MACHINE:    USER:          FRAMES "
             "MODIFIED:     |" +
             currentHistory;
    else
      return "| #    DATE:       Time:   MACHINE:    USER:           |" +
             currentHistory;
  } else
    return "";
}

//--------------------------------------------------------------------

void TContentHistory::fixCurrentHistory() {
  m_frozenHistory = serialize();
  m_records.clear();
}

//--------------------------------------------------------------------

void TContentHistory::frameRangeModifiedNow(const TFrameId &fromId,
                                            const TFrameId &toId) {
  assert(m_isLevel);
  QDateTime date = QDateTime::currentDateTime();
  QDateTime dateNoSecs(date.date(), date.time().addSecs(-date.time().second()));
  assert(dateNoSecs.secsTo(date) == date.time().second());

  int i;
  m_records[fromId] = dateNoSecs;

  if (fromId == toId) return;

  for (i = fromId.getNumber() + 1; i <= toId.getNumber() - 1; i++)
    m_records[TFrameId(i)] = dateNoSecs;

  m_records[toId] = dateNoSecs;
}

//--------------------------------------------------------------------

void TContentHistory::modifiedNow() {
  assert(!m_isLevel);
  QDateTime date = QDateTime::currentDateTime();
  QDateTime dateNoSecs(date.date(), date.time().addSecs(-date.time().second()));
  assert(dateNoSecs.secsTo(date) == date.time().second());

  m_records[0] = dateNoSecs;
}

/*
void testHistory()
{
                {
                TContentHistory ch(true);

                ch.frameModifiedNow(TFrameId(13));
                Sleep(1000);

                ch.frameModifiedNow(TFrameId(3, 'c'));
                ch.frameRangeModifiedNow(TFrameId(5), TFrameId(7));
                ch.fixCurrentHistory();
                Sleep(2000);
                ch.frameRangeModifiedNow(TFrameId(6), TFrameId(9));
                ch.frameModifiedNow(TFrameId(11));
                QString str1 = ch.serialize();

                TContentHistory ch1(true);
                ch1.deserialize(str1);
                ch1.frameRangeModifiedNow(TFrameId(2), TFrameId(3, 'c'));
                Sleep(2000);
                ch1.frameModifiedNow(TFrameId(3, 'a'));
                ch1.fixCurrentHistory();
                Sleep(2000);
                ch1.frameModifiedNow(TFrameId(11, 'b'));
                ch1.frameModifiedNow(TFrameId(12));
                QString str2 = ch1.serialize();

                QFile f("C:\\temp\\out.txt");
                f.open(QIODevice::WriteOnly | QIODevice::Text);
                QTextStream out(&f);
                out << str2;
                }


    {
                TContentHistory ch(false);

                ch.modifiedNow();
                Sleep(1000);

                ch.modifiedNow();
                ch.modifiedNow();
                ch.fixCurrentHistory();
                Sleep(2000);
                ch.modifiedNow();
                ch.modifiedNow();
                QString str1 = ch.serialize();

                TContentHistory ch1(false);
                ch1.deserialize(str1);
                ch1.modifiedNow();
                Sleep(2000);
                ch1.modifiedNow();
                ch1.fixCurrentHistory();
                Sleep(2000);
                ch1.modifiedNow();
                ch1.modifiedNow();
                QString str2 = ch1.serialize();

                QFile f("C:\\temp\\out1.txt");
                f.open(QIODevice::WriteOnly | QIODevice::Text);
                QTextStream out(&f);
                out << str2;
                }
}
*/
