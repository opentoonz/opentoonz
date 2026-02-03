#pragma once

#ifndef HISTORY_INCLUDED
#define HISTORY_INCLUDED

//#include "tfilepath.h"
#include "tsystem.h"

#include <set>
#include <QDateTime>

class History {  // singleton
public:
  class Item {
  public:
    TFilePath m_filePath;
    Item(const TFilePath &fp) : m_filePath(fp) {}
    bool operator<(const Item &item) const {
      return m_filePath < item.m_filePath;
    }
  };

  class Day {
  public:
    QDate m_timeStamp;
    std::string getDate() const;
    std::set<Item> m_items;
    Day(const QDate &t) : m_timeStamp(t){};
    void getFiles(std::vector<TFilePath> &files) const;
  };

  ~History();

  static History *instance();
  int getDayCount() const { return m_days.size(); };
  const Day *getDay(int i) const;
  const Day *getDay(const QDate &time) const;
  const Day *getDay(std::string dateString)
      const;  // getDay(dateString)->getDate() == dateString

  // nota. ritorna il giorno (se c'e') che ha un timeStamp IDENTICO a time
  void addItem(const TFilePath &fp);

private:
  std::vector<Day *> m_days;

  History();
  void load();
  void save();
};

#endif
