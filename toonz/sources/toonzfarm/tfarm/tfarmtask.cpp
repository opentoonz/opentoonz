

#include "tfarmtask.h"

// TnzBase includes
#include "toutputproperties.h"

// TnzCore includes
#include "tconvert.h"
#include "texception.h"
#include "tstream.h"
#include "tsystem.h"
#include "tutil.h"

// QT includes
#include <QStringList>
#include <QSettings>

// MACOSX includes
#ifdef _WIN32
#include <winsock.h>
#elif MACOSX
#include "tversion.h"
using namespace TVER;
#include <netdb.h>      // gethostbyname
#include <arpa/inet.h>  // inet_ntoa
#else
// these were included for OSX, i'm not sure if they are required for linux or
// not? leaving them in as linux was building sucessfully already. damies13 -
// 2017-04-15.
#include <netdb.h>      // gethostbyname
#include <arpa/inet.h>  // inet_ntoa
#endif

//*************************************************************************
//    TFarmTask::Dependencies  implementation
//*************************************************************************

class TFarmTask::Dependencies::Data {
public:
  Data() : m_tasks() {}
  ~Data() {}

  std::vector<TFarmTask::Id> m_tasks;
};

//------------------------------------------------------------------------------

TFarmTask::Dependencies::Dependencies() : m_data(new Data) {}

//------------------------------------------------------------------------------

TFarmTask::Dependencies::Dependencies(const Dependencies &rhs)
    : m_data(new Data) {
  m_data->m_tasks = rhs.m_data->m_tasks;
}

//------------------------------------------------------------------------------

TFarmTask::Dependencies::~Dependencies() { delete m_data; }

//------------------------------------------------------------------------------

TFarmTask::Dependencies &TFarmTask::Dependencies::operator=(
    const Dependencies &rhs) {
  if (this != &rhs) {
    m_data->m_tasks = rhs.m_data->m_tasks;
  }

  return *this;
}

//------------------------------------------------------------------------------

bool TFarmTask::Dependencies::operator==(const Dependencies &rhs) {
  return m_data->m_tasks == rhs.m_data->m_tasks;
}

//------------------------------------------------------------------------------

bool TFarmTask::Dependencies::operator!=(const Dependencies &rhs) {
  return !operator==(rhs);
}

//------------------------------------------------------------------------------

void TFarmTask::Dependencies::add(const TFarmTask::Id &id) {
  m_data->m_tasks.push_back(id);
}

//------------------------------------------------------------------------------

void TFarmTask::Dependencies::remove(const TFarmTask::Id &id) {
  std::vector<TFarmTask::Id>::iterator it =
      std::find(m_data->m_tasks.begin(), m_data->m_tasks.end(), id);

  if (it != m_data->m_tasks.end()) m_data->m_tasks.erase(it);
}

//------------------------------------------------------------------------------

int TFarmTask::Dependencies::getTaskCount() const {
  return m_data->m_tasks.size();
}

//------------------------------------------------------------------------------

TFarmTask::Id TFarmTask::Dependencies::getTaskId(int i) const {
  if (i >= 0 && i < (int)m_data->m_tasks.size()) return m_data->m_tasks[i];

  return "";
}

//*************************************************************************
//    TFarmTask  implementation
//*************************************************************************

TFarmTask::TFarmTask(const QString &name)
    : m_isComposerTask()
    , m_name(name)
    , m_priority()
    , m_status(Suspended)
    , m_successfullSteps()
    , m_failedSteps()
    , m_stepCount()
    , m_from(-1)
    , m_to(-1)
    , m_step(-1)
    , m_shrink(-1)
    , m_chunkSize(-1)
    , m_multimedia(0)        // Full render, no multimedia
    , m_threadsIndex(2)      // All threads
    , m_maxTileSizeIndex(0)  // No tiling
    , m_overwrite()
    , m_onlyVisible()
    , m_platform(NoPlatform)
    , m_dependencies() {}

//------------------------------------------------------------------------------

TFarmTask::TFarmTask(const QString &id, const QString &name, bool composerTask,
                     const QString &user, const QString &host, int stepCount,
                     int priority, const TFilePath &taskFilePath,
                     const TFilePath &outputPath, int from, int to, int step,
                     int shrink, int multimedia, int chunksize,
                     int threadsIndex, int maxTileSizeIndex,
                     OverwriteBehavior overwrite, bool onlyvisible)

    : m_isComposerTask(composerTask)
    , m_id(id)
    , m_name(name)
    , m_user(user)
    , m_hostName(host)
    , m_priority(priority)
    , m_successfullSteps(0)
    , m_failedSteps(0)
    , m_stepCount(stepCount)
    , m_platform(NoPlatform)
    , m_dependencies(new Dependencies)
    , m_taskFilePath(taskFilePath)
    , m_outputPath(outputPath)
    , m_from(from)
    , m_to(to)
    , m_step(step)
    , m_shrink(shrink)
    , m_multimedia(multimedia)
    , m_threadsIndex(threadsIndex)
    , m_maxTileSizeIndex(maxTileSizeIndex)
    , m_chunkSize(chunksize)
    , m_overwrite(overwrite)
    , m_onlyVisible(onlyvisible)
    , m_status(Suspended)
    , m_callerMachineName() {}

//------------------------------------------------------------------------------

TFarmTask::TFarmTask(const QString &id, const QString &name,
                     const QString &cmdline, const QString &user,
                     const QString &host, int stepCount, int priority)
    : m_id(id)
    , m_name(name)
    , m_user(user)
    , m_hostName(host)
    , m_priority(priority)
    , m_successfullSteps(0)
    , m_failedSteps(0)
    , m_stepCount(stepCount)
    , m_platform(NoPlatform)
    , m_dependencies(new Dependencies)
    , m_status(Suspended)
    , m_callerMachineName() {
  parseCommandLine(cmdline);
}

//------------------------------------------------------------------------------

TFarmTask::TFarmTask(const TFarmTask &rhs) : m_dependencies() { *this = rhs; }

//------------------------------------------------------------------------------

TFarmTask &TFarmTask::operator=(const TFarmTask &rhs) {
  if (this != &rhs) {
    m_name             = rhs.m_name;
    m_priority         = rhs.m_priority;
    m_user             = rhs.m_user;
    m_hostName         = rhs.m_hostName;
    m_id               = rhs.m_id;
    m_parentId         = rhs.m_parentId;
    m_status           = rhs.m_status;
    m_server           = rhs.m_server;
    m_submissionDate   = rhs.m_submissionDate;
    m_startDate        = rhs.m_startDate;
    m_completionDate   = rhs.m_completionDate;
    m_successfullSteps = rhs.m_successfullSteps;
    m_failedSteps      = rhs.m_failedSteps;
    m_stepCount        = rhs.m_stepCount;
    m_from             = rhs.m_from;
    m_to               = rhs.m_to;
    m_step             = rhs.m_step;
    m_shrink           = rhs.m_shrink;
    m_onlyVisible      = rhs.m_onlyVisible;
    m_overwrite        = rhs.m_overwrite;
    m_multimedia       = rhs.m_multimedia;
    m_threadsIndex     = rhs.m_threadsIndex;
    m_maxTileSizeIndex = rhs.m_maxTileSizeIndex;
    m_chunkSize        = rhs.m_chunkSize;

    delete m_dependencies;
    m_dependencies = 0;

    if (rhs.m_dependencies)
      m_dependencies = new Dependencies(*rhs.m_dependencies);
  }

  return *this;
}

//------------------------------------------------------------------------------

bool TFarmTask::operator==(const TFarmTask &task) {
  bool equalDependencies;

  if (!task.m_dependencies && !m_dependencies)
    equalDependencies = true;
  else if (task.m_dependencies && !m_dependencies)
    equalDependencies = false;
  else if (!task.m_dependencies && m_dependencies)
    equalDependencies = false;
  else
    equalDependencies = (task.m_dependencies == m_dependencies);

  return (
      task.m_name == m_name && task.m_priority == m_priority &&
      task.m_user == m_user && task.m_hostName == m_hostName &&
      task.m_id == m_id && task.m_parentId == m_parentId &&
      task.m_status == m_status && task.m_server == m_server &&
      task.m_submissionDate == m_submissionDate &&
      task.m_startDate == m_startDate &&
      task.m_completionDate == m_completionDate &&
      task.m_successfullSteps == m_successfullSteps &&
      task.m_failedSteps == m_failedSteps && task.m_stepCount == m_stepCount &&
      task.m_from == m_from && task.m_to == m_to && task.m_step == m_step &&
      task.m_shrink == m_shrink && task.m_onlyVisible == m_onlyVisible &&
      task.m_overwrite == m_overwrite && task.m_multimedia == m_multimedia &&
      task.m_threadsIndex == m_threadsIndex &&
      task.m_maxTileSizeIndex == m_maxTileSizeIndex &&
      task.m_chunkSize == m_chunkSize && equalDependencies);
}

//------------------------------------------------------------------------------

void TFarmTask::loadData(TIStream &is) {
  std::string tagName;
  while (is.matchTag(tagName)) {
    if (tagName == "taskId") {
      is >> m_id;
    } else if (tagName == "parentId") {
      is >> m_parentId;
    } else if (tagName == "name") {
      is >> m_name;
    } else if (tagName == "cmdline") {
      QString commandLine;
      is >> commandLine;
      parseCommandLine(commandLine);
    } else if (tagName == "priority") {
      is >> m_priority;
    } else if (tagName == "submittedBy") {
      is >> m_user;
    } else if (tagName == "submittedOn") {
      is >> m_hostName;
    } else if (tagName == "submissionDate") {
      QString date;
      is >> date;
      m_submissionDate = QDateTime::fromString(date);
    } else if (tagName == "stepCount") {
      is >> m_stepCount;
    } else if (tagName == "chunkSize") {
      assert(dynamic_cast<TFarmTaskGroup *>(this));
      is >> m_chunkSize;
    } else if (tagName == "threadsIndex") {
      assert(dynamic_cast<TFarmTaskGroup *>(this));
      is >> m_threadsIndex;
    } else if (tagName == "maxTileSizeIndex") {
      assert(dynamic_cast<TFarmTaskGroup *>(this));
      is >> m_maxTileSizeIndex;
    }

    else if (tagName == "platform") {
      int val;
      is >> val;
      m_platform = (TFarmPlatform)val;
    } else if (tagName == "dependencies") {
      m_dependencies = new Dependencies();
      while (!is.eos()) {
        is.matchTag(tagName);
        if (tagName == "taskId") {
          QString dependsOn;
          is >> dependsOn;
          m_dependencies->add(dependsOn);
        } else
          throw TException(tagName + " : unexpected tag");

        if (!is.matchEndTag()) throw TException(tagName + " : missing end tag");
      }
    } else {
      throw TException(tagName + " : unexpected tag");
    }

    if (!is.matchEndTag()) throw TException(tagName + " : missing end tag");
  }
}

//------------------------------------------------------------------------------

void TFarmTask::saveData(TOStream &os) {
  os.child("taskId") << m_id;
  os.child("parentId") << m_parentId;
  os.child("name") << m_name;
  os.child("cmdline") << getCommandLine();
  os.child("priority") << m_priority;
  os.child("submittedBy") << m_user;
  os.child("submittedOn") << m_hostName;
  os.child("submissionDate") << m_submissionDate.toString();
  os.child("stepCount") << m_stepCount;
  if (dynamic_cast<TFarmTaskGroup *>(this))
    os.child("chunkSize") << m_chunkSize;
  os.child("threadsIndex") << m_threadsIndex;
  os.child("maxTileSizeIndex") << m_maxTileSizeIndex;
  os.child("platform") << m_platform;

  os.openChild("dependencies");
  if (m_dependencies) {
    for (int i = 0; i < m_dependencies->getTaskCount(); ++i) {
      TFarmTask::Id id = m_dependencies->getTaskId(i);
      os.child("taskId") << id;
    }
  }
  os.closeChild();  // "dependencies"
}

//------------------------------------------------------------------------------

namespace {
QString getExeName(bool isComposer) {
  QString name = isComposer ? "tcomposer" : "tcleanup";

#ifdef _WIN32
  return name + ".exe ";
#elif MACOSX
  TVER::ToonzVersion tver;
  return "\"./" + QString::fromStdString(tver.getAppName()) + "_" +
         QString::fromStdString(tver.getAppVersionString()) +
         ".app/Contents/MacOS/" + name + "\" ";
#else
  return name;
#endif
}

//------------------------------------------------------------------------------

QString toString(int value, int w, char c = ' ') {
  QString s = QString::number(value);
  while (s.size() < w) s= c + s;
  return s;
}

}  // namespace

//------------------------------------------------------------------------------

static TFilePath getFilePath(const QStringList &l, int &i) {
  QString outStr = l.at(i++);
  if (outStr.startsWith('"')) {
    outStr = outStr.remove(0, 1);
    if (!outStr.endsWith('"')) {
      do
        outStr += " " + l.at(i);
      while (i < l.size() && !l.at(i++).endsWith('"'));
    }
    outStr.chop(1);
  }

  return TFilePath(outStr.toStdString());
}

//------------------------------------------------------------------------------

void TFarmTask::parseCommandLine(QString commandLine) {
  QStringList l = commandLine.split(" ", QString::SkipEmptyParts);
  assert(l.size() >= 2);

  // serve per skippare il path dell'eseguibile su mac che contiene spazi
  int i = 0;
  while (i < l.size() &&
         (!l.at(i).contains("tcomposer") && !l.at(i).contains("tcleanup")))
    ++i;

  m_isComposerTask = l.at(i++).contains("tcomposer");

  if (i < l.size() && !l.at(i).startsWith('-'))
    m_taskFilePath = getFilePath(l, i);

  m_step   = 1;
  m_shrink = 1;

  while (i < l.size()) {
    QString str = l.at(i);
    if (l.at(i) == "-o")
      m_outputPath = getFilePath(l, ++i);
    else if (l.at(i) == "-range") {
      m_from = (l.at(i + 1).toInt());
      m_to   = (l.at(i + 2).toInt());
      i += 3;
    } else if (l.at(i) == "-step") {
      m_step = (l.at(i + 1).toInt());
      i += 2;
    } else if (l.at(i) == "-shrink") {
      m_shrink = (l.at(i + 1).toInt());
      i += 2;
    } else if (l.at(i) == "-multimedia") {
      m_multimedia = (l.at(i + 1).toInt());
      i += 2;
    } else if (l.at(i) == "-nthreads") {
      QString str(l.at(i + 1));

      m_threadsIndex = (str == "single") ? 0 : (str == "half") ? 1 : 2;
      i += 2;
    } else if (l.at(i) == "-maxtilesize") {
      QString str(l.at(i + 1));

      const QString maxTileSizeIndexes[3] = {
          QString::number(TOutputProperties::LargeVal),
          QString::number(TOutputProperties::MediumVal),
          QString::number(TOutputProperties::SmallVal)};

      m_maxTileSizeIndex = (str == maxTileSizeIndexes[2])
                               ? 3
                               : (str == maxTileSizeIndexes[1])
                                     ? 2
                                     : (str == maxTileSizeIndexes[0]) ? 1 : 0;
      i += 2;
    }

    else if (l.at(i) == "-overwriteAll")
      m_overwrite = Overwrite_All, i++;
    else if (l.at(i) == "-overwriteNoPaint")
      m_overwrite = Overwrite_NoPaint, i++;

    else if (l.at(i) == "-onlyvisible")
      m_onlyVisible = true, ++i;
    else if (l.at(i) == "-farm" || l.at(i) == "-id")
      i += 2;
    else if (l.at(i) == "-tmsg") {
      m_callerMachineName = l.at(i + 1);
      i += 2;
    } else
      assert(false);
  }
}

//------------------------------------------------------------------------------

QString TFarmTask::getCommandLine(bool isFarmTask) const {
  QString cmdline = getExeName(m_isComposerTask);

  if (!m_taskFilePath.isEmpty())
    cmdline += " \"" +
               QString::fromStdWString(
                   TSystem::toUNC(m_taskFilePath).getWideString()) +
               "\"";

  if (m_callerMachineName != "") {
#if QT_VERSION >= 0x050500
    struct hostent *he = gethostbyname(m_callerMachineName.toLatin1());
#else
    struct hostent *he = gethostbyname(m_callerMachineName.toAscii());
#endif
    if (he) {
      char *ipAddress = inet_ntoa(*(struct in_addr *)*(he->h_addr_list));
#if QT_VERSION >= 0x050500
      cmdline += " -tmsg " + QString::fromUtf8(ipAddress);
#else
      cmdline += " -tmsg " + QString::fromAscii(ipAddress);
#endif
    }
  }

  if (!m_isComposerTask) {
    if (m_overwrite == Overwrite_All)
      cmdline += " -overwriteAll ";
    else if (m_overwrite == Overwrite_NoPaint)
      cmdline += " -overwriteNoPaint ";
    if (m_onlyVisible) cmdline += " -onlyvisible ";
    return cmdline;
  }

  if (!m_outputPath.isEmpty()) {
    TFilePath outputPath;
    try {
      outputPath = TSystem::toUNC(m_outputPath);
    } catch (TException &) {
    }

    cmdline +=
        " -o \"" + QString::fromStdWString(outputPath.getWideString()) + "\"";
  }

  cmdline += " -range " + QString::number(m_from) + " " + QString::number(m_to);
  cmdline += " -step " + QString::number(m_step);
  cmdline += " -shrink " + QString::number(m_shrink);
  cmdline += " -multimedia " + QString::number(m_multimedia);

  const QString threadCounts[3] = {"single", "half", "all"};
  cmdline += " -nthreads " + threadCounts[m_threadsIndex];

  const QString maxTileSizes[4] = {
      "none", QString::number(TOutputProperties::LargeVal),
      QString::number(TOutputProperties::MediumVal),
      QString::number(TOutputProperties::SmallVal)};
  cmdline += " -maxtilesize " + maxTileSizes[m_maxTileSizeIndex];

  QString appname = QSettings().applicationName();

  return cmdline;
}

//------------------------------------------------------------------------------

namespace {

class TFarmTaskDeclaration final : public TPersistDeclaration {
public:
  TFarmTaskDeclaration(const std::string &id) : TPersistDeclaration(id) {}

  TPersist *create() const override { return new TFarmTask; }

} FarmTaskDeclaration("ttask");

}  // namespace

const TPersistDeclaration *TFarmTask::getDeclaration() const {
  return &FarmTaskDeclaration;
}

//------------------------------------------------------------------------------

bool TFarmTask::operator!=(const TFarmTask &task) { return !operator==(task); }

//==============================================================================

class TFarmTaskGroup::Imp {
public:
  Imp() {}
  ~Imp() {
    // clearPointerContainer(m_tasks);
    std::vector<TFarmTask *>::iterator it = m_tasks.begin();
    for (; it != m_tasks.end(); ++it) delete *it;
  }

  std::vector<TFarmTask *> m_tasks;
};

//------------------------------------------------------------------------------

TFarmTaskGroup::TFarmTaskGroup() : m_imp(new Imp()) {}

//------------------------------------------------------------------------------

TFarmTaskGroup::TFarmTaskGroup(const QString &id, const QString &name,
                               const QString &cmdline, const QString &user,
                               const QString &host, int stepCount, int priority)
    : TFarmTask(id, name, cmdline, user, host, stepCount, priority)
    , m_imp(new Imp()) {}

//------------------------------------------------------------------------------

bool TFarmTaskGroup::changeChunkSize(int chunksize) {
  m_chunkSize  = chunksize;
  int subCount = tceil((m_to - m_from + 1) / (double)m_chunkSize);

  if (subCount > 1) {
    int ra = m_from;
    for (int i = 1; i <= subCount; ++i) {
      int rb = std::min(ra + m_chunkSize - 1, m_to);

      try {
        QString subName =
            m_name + " " + toString(ra, 2, '0') + "-" + toString(rb, 2, '0');

        TFarmTask *subTask = new TFarmTask(
            m_id + "." + toString(i, 2, '0'), subName, true, m_user, m_hostName,
            rb - ra + 1, m_priority, m_taskFilePath, m_outputPath, ra, rb,
            m_step, m_shrink, m_multimedia, m_chunkSize, m_threadsIndex,
            m_maxTileSizeIndex, Overwrite_Off, false);

        subTask->m_parentId = m_id;
        addTask(subTask);
      } catch (TException &) {
        // TMessage::error(toString(e.getMessage()));
      }

      ra = rb + 1;
    }
  }

  return true;
}

//------------------------------------------------------------------------------

TFarmTaskGroup::TFarmTaskGroup(const QString &id, const QString &name,
                               const QString &user, const QString &host,
                               int stepCount, int priority,
                               const TFilePath &taskFilePath,
                               const TFilePath &outputPath, int from, int to,
                               int step, int shrink, int multimedia,
                               int chunksize, int threadsIndex,
                               int maxTileSizeIndex)

    : TFarmTask(id, name, true, user, host, stepCount, priority, taskFilePath,
                outputPath, from, to, step, shrink, multimedia, chunksize,
                threadsIndex, maxTileSizeIndex, Overwrite_Off, false)
    , m_imp(new Imp()) {
  int subCount = 0;
  if (chunksize > 0) subCount= tceil((to - from + 1) / (double)chunksize);

  int ra = from;
  if (subCount > 1) {
    for (int i = 1; i <= subCount; ++i) {
      int rb = std::min(ra + chunksize - 1, to);

      try {
        QString subName =
            name + " " + toString(ra, 2, '0') + "-" + toString(rb, 2, '0');
        stepCount = rb - ra + 1;

        TFarmTask *subTask =
            new TFarmTask(id + "." + toString(i, 2, '0'), subName, true, user,
                          host, stepCount, priority, taskFilePath, outputPath,
                          ra, rb, step, shrink, multimedia, chunksize,
                          threadsIndex, maxTileSizeIndex, Overwrite_Off, false);

        subTask->m_parentId = id;
        addTask(subTask);
      } catch (TException &) {
        // TMessage::error(toString(e.getMessage()));
      }

      ra = rb + 1;
    }
  }
}

//------------------------------------------------------------------------------

TFarmTaskGroup::TFarmTaskGroup(const QString &id, const QString &name,
                               const QString &user, const QString &host,
                               int stepCount, int priority,
                               const TFilePath &taskFilePath,
                               OverwriteBehavior overwrite, bool onlyvisible)
    : TFarmTask(id, name, false, user, host, stepCount, priority, taskFilePath,
                TFilePath(), 0, 0, 0, 0, 0, 0, 0, 0, overwrite, onlyvisible)
    , m_imp(new Imp()) {}

//------------------------------------------------------------------------------

TFarmTaskGroup::TFarmTaskGroup(const TFarmTaskGroup &src)
    : TFarmTask(src), m_imp(new TFarmTaskGroup::Imp()) {
  int count = src.getTaskCount();
  for (int i = 0; i < count; ++i) {
    TFarmTaskGroup &ssrc = const_cast<TFarmTaskGroup &>(src);
    addTask(new TFarmTask(*ssrc.getTask(i)));
  }
}

//------------------------------------------------------------------------------

TFarmTaskGroup::~TFarmTaskGroup() {}

//------------------------------------------------------------------------------

void TFarmTaskGroup::addTask(TFarmTask *task) {
  m_imp->m_tasks.push_back(task);
}

//------------------------------------------------------------------------------

void TFarmTaskGroup::removeTask(TFarmTask *task) {
  std::vector<TFarmTask *>::iterator it =
      std::find(m_imp->m_tasks.begin(), m_imp->m_tasks.end(), task);
  if (it != m_imp->m_tasks.end()) m_imp->m_tasks.erase(it);
}

//------------------------------------------------------------------------------

int TFarmTaskGroup::getTaskCount() const { return m_imp->m_tasks.size(); }

//------------------------------------------------------------------------------

TFarmTask *TFarmTaskGroup::getTask(int index) {
  std::vector<TFarmTask *>::iterator it = m_imp->m_tasks.begin();
  std::advance(it, index);
  return it != m_imp->m_tasks.end() ? *it : 0;
}

//------------------------------------------------------------------------------

void TFarmTaskGroup::loadData(TIStream &is) {
  std::string tagName;
  while (is.matchTag(tagName)) {
    if (tagName == "info") {
      TFarmTask::loadData(is);
    } else if (tagName == "tasks") {
      while (!is.eos()) {
        TPersist *p = 0;
        is >> p;
        TFarmTask *task = dynamic_cast<TFarmTask *>(p);
        if (task) addTask(task);
      }
    } else {
      throw TException(tagName + " : unexpected tag");
    }

    if (!is.matchEndTag()) throw TException(tagName + " : missing end tag");
  }
}

//------------------------------------------------------------------------------

void TFarmTaskGroup::saveData(TOStream &os) {
  os.openChild("info");
  TFarmTask::saveData(os);
  os.closeChild();

  os.openChild("tasks");
  std::vector<TFarmTask *>::iterator it = m_imp->m_tasks.begin();
  for (; it != m_imp->m_tasks.end(); ++it) os << *it;
  os.closeChild();
}

//------------------------------------------------------------------------------

namespace {

class TFarmTaskGroupDeclaration final : public TPersistDeclaration {
public:
  TFarmTaskGroupDeclaration(const std::string &id) : TPersistDeclaration(id) {}

  TPersist *create() const override { return new TFarmTaskGroup; }

} FarmTaskGroupDeclaration("ttaskgroup");

}  // namespace

const TPersistDeclaration *TFarmTaskGroup::getDeclaration() const {
  return &FarmTaskGroupDeclaration;
}
