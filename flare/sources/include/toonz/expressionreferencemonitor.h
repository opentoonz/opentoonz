#pragma once

#ifndef EXPRESSIONREFERENCEMONITOR_H
#define EXPRESSIONREFERENCEMONITOR_H

#undef DVAPI
#undef DVVAR
#ifdef TOONZLIB_EXPORTS
#define DVAPI DV_EXPORT_API
#define DVVAR DV_EXPORT_VAR
#else
#define DVAPI DV_IMPORT_API
#define DVVAR DV_IMPORT_VAR
#endif

#include <QMap>
#include <QSet>
#include <QString>

class TDoubleParam;

class DVAPI ExpressionReferenceMonitorInfo {
  // name of the parameter
  QString m_name = "";
  // true if the parameter is not monitored
  bool m_ignored = false;
  // column indices to which the parameter refers.
  // note that the columns referred by the "cell" syntax will be
  // registered in this container, but not in the paramRefMap.
  QSet<int> m_colRefMap;
  // parameters to which the parameter refers
  QSet<TDoubleParam*> m_paramRefMap;

public:
  QString& name() { return m_name; }
  bool& ignored() { return m_ignored; }
  QSet<int>& colRefMap() { return m_colRefMap; }
  QSet<TDoubleParam*>& paramRefMap() { return m_paramRefMap; }
};

class DVAPI ExpressionReferenceMonitor {
  QMap<TDoubleParam*, ExpressionReferenceMonitorInfo> m_info;

public:
  ExpressionReferenceMonitor() {}
  QMap<TDoubleParam*, ExpressionReferenceMonitorInfo>& info() { return m_info; }

  void clearAll() { m_info.clear(); }

  ExpressionReferenceMonitor* clone() {
    ExpressionReferenceMonitor* ret = new ExpressionReferenceMonitor();
    ret->info()                     = m_info;
    return ret;
  }
};

#endif
