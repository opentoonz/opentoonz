

#include "tsystem.h"

#include <set>
#include <atomic>
#include "tfilepath_io.h"
#include "tconvert.h"

#include <cwctype>  // Include the header for towlower

#ifndef TNZCORE_LIGHT

#include <QDateTime>
#include <QStringList>
#include <QProcess>
#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QSettings>
#include <QVariant>
#include <QThread>
#include <QUrl>
#include <QCoreApplication>
#include <QUuid>
#include <QRegularExpression>
#include <QDesktopServices>
#include <QHostInfo>

#ifdef _WIN32
#include <shlobj.h>
#include <shellapi.h>
#include <winnt.h>
#endif

namespace {

inline QString toQString(const TFilePath &path) {
  return QString::fromStdWString(path.getWideString());
}

std::atomic<int> HasMainLoop{-1};  // Changed to atomic for thread safety

}  // namespace
//-----------------------------------------------------------------------------------

TFileStatus::TFileStatus(const TFilePath &path)
    : m_fileInfo(QString::fromStdWString(path.getWideString()))
    , m_exist(false)  // safe default value; see NOTE below
// During static init (e.g., EnvGlobals) before QCoreApplication exists.
// m_exist defaults to false. Proper fix requires refactoring init order in
// tenv.cpp, tcontenthistory.cpp, etc. to create QCoreApplication first.
// Workaround avoids crashes.
{
  if (QCoreApplication::instance() == nullptr) {
    return;  // avoids premature exists() doesn't crash during static
             // initialization
  }
  m_exist = m_fileInfo.exists();
}

//-----------------------------------------------------------------------------------

QString TFileStatus::getGroup() const {
  if (!m_exist) return QString();
  return m_fileInfo.group();
}

//-----------------------------------------------------------------------------------

QString TFileStatus::getUser() const {
  if (!m_exist) return QString();
  return m_fileInfo.owner();
}

//-----------------------------------------------------------------------------------

TINT64 TFileStatus::getSize() const {
  if (!m_exist) return 0;
  return m_fileInfo.size();
}

//-----------------------------------------------------------------------------------

QDateTime TFileStatus::getLastAccessTime() const {
  if (!m_exist) return QDateTime();
  return m_fileInfo.lastRead();
}

//-----------------------------------------------------------------------------------

QDateTime TFileStatus::getLastModificationTime() const {
  if (!m_exist) return QDateTime();
  return m_fileInfo.lastModified();
}

//-----------------------------------------------------------------------------------

QDateTime TFileStatus::getCreationTime() const {
  if (!m_exist) return QDateTime();
  return m_fileInfo.birthTime();
}

//-----------------------------------------------------------------------------------

QFile::Permissions TFileStatus::getPermissions() const {
  if (!m_exist) return QFileDevice::Permissions();
  return m_fileInfo.permissions();
}

//-----------------------------------------------------------------------------------

bool TFileStatus::isDirectory() const {
  if (!m_exist) return 0;
  return m_fileInfo.isDir();
}

//-----------------------------------------------------------------------------------

bool TFileStatus::isLink() const { return m_fileInfo.isSymLink(); }

//-----------------------------------------------------------------------------------

bool TSystem::doHaveMainLoop() {
  int current = HasMainLoop.load(std::memory_order_acquire);

  if (current == -1) {
#ifdef QT_DEBUG
    qFatal("TSystem::hasMainLoop was not called");
#else
    qWarning("Main loop not initialized");
#endif
    return false;
  }
  return current == 1;
}

//-----------------------------------------------------------------------------------

void TSystem::hasMainLoop(bool state) {
  int expected = -1;
  int desired  = state ? 1 : 0;

  if (!HasMainLoop.compare_exchange_strong(expected, desired,
                                           std::memory_order_acq_rel,
                                           std::memory_order_acquire)) {
#ifdef QT_DEBUG
    qWarning("TSystem::hasMainLoop called multiple times (debug check)");
#endif
  }
}

//-----------------------------------------------------------------------------------

QString TSystem::getHostName() { return QHostInfo::localHostName(); }

//------------------------------------------------------------

QString TSystem::getUserName() {
  QStringList list = QProcess::systemEnvironment();
  for (int j = 0; j < list.size(); j++) {
    QString value = list.at(j);
    QString user;
#ifdef _WIN32
    if (value.startsWith("USERNAME=")) user = value.right(value.size() - 9);
#else
    if (value.startsWith("USER=")) user = value.right(value.size() - 5);
#endif
    if (!user.isEmpty()) return user;
  }
  return QString("none");
}

//------------------------------------------------------------

TFilePath TSystem::getTempDir() {
  return TFilePath(QDir::tempPath().toStdString());
}

//------------------------------------------------------------

TFilePath TSystem::getTestDir(std::string name) {
  return TFilePath("C:") + TFilePath(name);
}

//------------------------------------------------------------

QString TSystem::getSystemValue(const TFilePath &name) {
  QStringList strlist = toQString(name).split("\\", Qt::SkipEmptyParts);

  if (strlist.size() <= 3 || strlist.at(0) != "SOFTWARE") {
    qWarning("Invalid system value path format");
    return QString();
  }

  QSettings qs(QSettings::SystemScope, strlist.at(1), strlist.at(2));

  QString varName;
  for (int i = 3; i < strlist.size(); i++) {
    varName += strlist.at(i);
    if (i < strlist.size() - 1) varName += "//";
  }

  return qs.value(varName).toString();
}

//------------------------------------------------------------

TFilePath TSystem::getBinDir() {
  TFilePath fp =
      TFilePath(QCoreApplication::applicationFilePath().toStdString());
  return fp.getParentDir();
}

//------------------------------------------------------------

TFilePath TSystem::getDllDir() { return getBinDir(); }
//------------------------------------------------------------

TFilePath TSystem::getUniqueFile(QString field) {
  QString uuid = QUuid::createUuid()
                     .toString(QUuid::Id128)
                     .replace("{", "")
                     .replace("}", "")
                     .replace("-", "");

  QString path = QDir::tempPath() + QString("\\") + field + uuid;

  return TFilePath(path.toStdString());
}

//------------------------------------------------------------

namespace {
TFilePathSet getPathsToCreate(const TFilePath &path) {
  TFilePathSet pathList;
  if (path.isEmpty()) return pathList;
  TFilePath parentDir = path;
  while (!TFileStatus(parentDir).doesExist()) {
    if (parentDir == parentDir.getParentDir()) return TFilePathSet();
    pathList.push_back(parentDir);
    parentDir = parentDir.getParentDir();
  }
  return pathList;
}

void setPathsPermissions(const TFilePathSet &pathSet,
                         QFile::Permissions permissions) {
  for (const auto &path : pathSet) {
    QFile f(toQString(path));
    f.setPermissions(permissions);
  }
}
}  // namespace

// handle exception
void TSystem::mkDir(const TFilePath &path) {
  TFilePathSet pathSet = getPathsToCreate(path);
  QString qPath        = toQString(path);

  if (qPath.contains("+")) {
    throw TSystemException(path, "Invalid character '+' in path!");
  }

  if (!QDir::current().mkpath(qPath))
    throw TSystemException(path, "can't create folder!");

  setPathsPermissions(
      pathSet, QFile::ReadUser | QFile::WriteUser | QFile::ExeUser |
                   QFile::ReadGroup | QFile::WriteGroup | QFile::ExeGroup |
                   QFile::ReadOther | QFile::WriteOther | QFile::ExeOther);
}

//------------------------------------------------------------
// handle exception
void TSystem::rmDir(const TFilePath &path) {
  if (!QDir(toQString(path.getParentDir()))
           .rmdir(QString::fromStdString(path.getName())))
    throw TSystemException(path, "can't remove folder!");
}

// vinz

//------------------------------------------------------------

namespace {
void rmDirTree(const QString &path) {
  QFileInfoList fil = QDir(path).entryInfoList();
  for (const QFileInfo &fi : fil) {
    if (fi.fileName() == QString(".") || fi.fileName() == QString(".."))
      continue;
    QString son = fi.absoluteFilePath();
    if (QFileInfo(son).isDir())
      rmDirTree(son);
    else if (QFileInfo(son).isFile())
      if (!QFile::remove(son))
        throw TSystemException("can't remove file" + son.toStdString());
  }
  if (!QDir::current().rmdir(path))
    throw TSystemException("can't remove path!");
}

}  // namespace

//------------------------------------------------------------

void TSystem::rmDirTree(const TFilePath &path) { ::rmDirTree(toQString(path)); }

//------------------------------------------------------------

void TSystem::copyDir(const TFilePath &dst, const TFilePath &src) {
  QFileInfoList fil = QDir(toQString(src)).entryInfoList();

  QDir::current().mkdir(toQString(dst));

  for (const QFileInfo &fi : fil) {
    if (fi.fileName() == QString(".") || fi.fileName() == QString(".."))
      continue;
    if (fi.isDir()) {
      TFilePath srcDir = TFilePath(fi.filePath().toStdString());
      TFilePath dstDir = dst + srcDir.getName();
      copyDir(dstDir, srcDir);
    } else {
      TFilePath srcFi = dst + TFilePath(fi.fileName());
      QFile::copy(fi.filePath(), toQString(srcFi));
    }
  }
}

/*

void TSystem::touchFile(const TFilePath &path)

{

QFile f(toQString(path));


if (!f.open(QIODevice::ReadWrite))

  throw TSystemException(path, "can't touch file!");

else

  f.close();

}

*/

//------------------------------------------------------------

/*

#ifdef _WIN32


std::wstring getFormattedMessage(DWORD lastError)

{

LPVOID lpMsgBuf;

FormatMessage(

    FORMAT_MESSAGE_ALLOCATE_BUFFER |

    FORMAT_MESSAGE_FROM_SYSTEM |

    FORMAT_MESSAGE_IGNORE_INSERTS,

    NULL,

    lastError,

    MAKELANGID(LANG_NEUTRAL, SUBLANG_DEFAULT), // Default language

    (LPTSTR) &lpMsgBuf,

    0,

    NULL

);


int wSize = MultiByteToWideChar(0,0,(char*)lpMsgBuf,-1,0,0);

if(!wSize)

  return std::wstring();


wchar_t* wBuffer = new wchar_t [wSize+1];

MultiByteToWideChar(0,0,(char*)lpMsgBuf,-1,wBuffer,wSize);

wBuffer[wSize]='\0';

std::wstring wmsg(wBuffer);


delete []wBuffer;

LocalFree(lpMsgBuf);

return wmsg;

}


#endif

*/

void TSystem::copyFile(const TFilePath &dst, const TFilePath &src,
                       bool overwrite) {
  if (dst.isEmpty()) {
    throw TSystemException(dst, "Destination path is empty!");
  }

  if (dst == src) return;

  // Create the containing folder before trying to copy or it will crash!
  touchParentDir(dst);

  const QString &qDst = toQString(dst);
  if (overwrite && QFile::exists(qDst)) {
    if (!QFile::remove(qDst))
      throw TSystemException(dst, "can't remove existing file!");
  }

  if (!QFile::copy(toQString(src), qDst))
    throw TSystemException(dst, "can't copy file!");
}

//------------------------------------------------------------

void TSystem::renameFile(const TFilePath &dst, const TFilePath &src,
                         bool overwrite) {
  if (dst.isEmpty()) {
    throw TSystemException(dst, "Destination path is empty!");
  }

  if (dst == src) return;

  const QString &qDst = toQString(dst);
  if (overwrite && QFile::exists(qDst)) {
    if (!QFile::remove(qDst))
      throw TSystemException(dst, "can't remove existing file!");
  }

  if (!QFile::rename(toQString(src), qDst))
    throw TSystemException(dst, "can't rename file!");
}

//------------------------------------------------------------

// handle errors with GetLastError?
void TSystem::deleteFile(const TFilePath &fp) {
  if (!QFile::remove(toQString(fp)))
    throw TSystemException(fp, "can't delete file!");
}

//------------------------------------------------------------

void TSystem::hideFile(const TFilePath &fp) {
#ifdef _WIN32
  if (!SetFileAttributesW(fp.getWideString().c_str(), FILE_ATTRIBUTE_HIDDEN))
    throw TSystemException(fp, "can't hide file!");
#else  // MACOSX, and others
  TSystem::renameFile(TFilePath(fp.getParentDir() + L"." + fp.getLevelNameW()),
                      fp);
#endif
}

//------------------------------------------------------------

class CaselessFilepathLess final {
public:
  bool operator()(const TFilePath &a, const TFilePath &b) const {
    // Perform case sensitive compare, fallback to case insensitive.
    const std::wstring a_str = a.getWideString();
    const std::wstring b_str = b.getWideString();

    size_t i            = 0;
    int case_compare    = -1;
    const size_t maxLen = std::max(a_str.size(), b_str.size());
    for (size_t i = 0; i < maxLen; ++i) {
      wchar_t a_char = (i < a_str.size()) ? a_str[i] : L'\0';
      wchar_t b_char = (i < b_str.size()) ? b_str[i] : L'\0';

      if (a_char != b_char) {
        const wchar_t a_wchar = towlower(a_char);
        const wchar_t b_wchar = towlower(b_char);
        if (a_wchar < b_wchar) {
          return true;
        } else if (a_wchar > b_wchar) {
          return false;
        } else if (case_compare == -1) {
          case_compare = (a_char < b_char) ? 1 : 0;
        }
      }
      i++;
    }
    return (case_compare == 1);
  }
};

//------------------------------------------------------------
/*! return the folder path list which is readable and executable
 */
void TSystem::readDirectory_Dir_ReadExe(TFilePathSet &dst,
                                        const TFilePath &path) {
  QStringList dirItems;
  readDirectory_DirItems(dirItems, path);

  for (const QString &item : dirItems) {
    TFilePath son = path + TFilePath(item.toStdWString());
    dst.push_back(son);
  }
}

//------------------------------------------------------------
// return the folder item list which is readable and executable
// (returns only names, not full path)
void TSystem::readDirectory_DirItems(QStringList &dst, const TFilePath &path) {
  if (!TFileStatus(path).isDirectory())
    throw TSystemException(path, " is not a directory");

  QDir dir(toQString(path));

#ifdef _WIN32
  QString pathStr = toQString(path);
  if (pathStr.startsWith("\\\\") || pathStr.startsWith("//")) {
    dst = dir.entryList(QDir::AllDirs | QDir::NoDotAndDotDot | QDir::Readable,
                        QDir::Name | QDir::LocaleAware);
    return;
  }

  // equivalent to sorting with QDir::LocaleAware
  auto const strCompare = [](const QString &s1, const QString &s2) {
    return QString::localeAwareCompare(s1, s2) < 0;
  };

  std::set<QString, decltype(strCompare)> entries(strCompare);

  WIN32_FIND_DATA find_dir_data;
  QString dir_search_path = dir.absolutePath() + "\\*";
  auto addEntry           = [&]() {
    // QDir::NoDotAndDotDot condition
    if (wcscmp(find_dir_data.cFileName, L".") != 0 &&
        wcscmp(find_dir_data.cFileName, L"..") != 0) {
      // QDir::AllDirs condition
      if (find_dir_data.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY &&
          (find_dir_data.dwFileAttributes & FILE_ATTRIBUTE_HIDDEN) == 0) {
        entries.insert(QString::fromWCharArray(find_dir_data.cFileName));
      }
    }
  };
  HANDLE hFind =
      FindFirstFile((const wchar_t *)dir_search_path.utf16(), &find_dir_data);
  if (hFind != INVALID_HANDLE_VALUE) {
    do {
      addEntry();
    } while (FindNextFile(hFind, &find_dir_data));
    FindClose(hFind);  // FIXED: Added FindClose
  }

  for (const QString &name : entries) dst.push_back(QString(name));

#else
  dst = dir.entryList(QDir::AllDirs | QDir::NoDotAndDotDot | QDir::Readable,
                      QDir::Name | QDir::LocaleAware);
#endif
}

//------------------------------------------------------------
/*! to retrieve the both lists with groupFrames option = on and off.
 */
void TSystem::readDirectory(TFilePathSet &groupFpSet, TFilePathSet &allFpSet,
                            const TFilePath &path) {
  if (!TFileStatus(path).isDirectory())
    throw TSystemException(path, " is not a directory");

  std::set<TFilePath, CaselessFilepathLess> fileSet_group;
  std::set<TFilePath, CaselessFilepathLess> fileSet_all;

  QStringList fil;
#ifdef _WIN32
  WIN32_FIND_DATA find_dir_data;
  QString dir_search_path = QDir(toQString(path)).absolutePath() + "\\*";
  auto addEntry           = [&]() {
    // QDir::NoDotAndDotDot condition
    if (wcscmp(find_dir_data.cFileName, L".") != 0 &&
        wcscmp(find_dir_data.cFileName, L"..") != 0) {
      // QDir::Files condition
      if ((find_dir_data.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY) == 0 &&
          (find_dir_data.dwFileAttributes & FILE_ATTRIBUTE_HIDDEN) == 0) {
        fil.append(QString::fromWCharArray(find_dir_data.cFileName));
      }
    }
  };
  HANDLE hFind =
      FindFirstFile((const wchar_t *)dir_search_path.utf16(), &find_dir_data);
  if (hFind != INVALID_HANDLE_VALUE) {
    do {
      addEntry();
    } while (FindNextFile(hFind, &find_dir_data));
    FindClose(hFind);  // FIXED: Added FindClose
  }
#else
  fil = QDir(toQString(path))
            .entryList(QDir::Files | QDir::NoDotAndDotDot | QDir::Readable);
#endif

  if (fil.empty()) return;

  for (const QString &fi : fil) {
    TFilePath son = path + TFilePath(fi.toStdWString());

    // store all file paths
    fileSet_all.insert(son);

    // in case of the sequential files
    if (son.getDots() == "..") son = son.withFrame();

    // store the group. insertion avoids duplication of the item
    fileSet_group.insert(son);
  }

  groupFpSet.insert(groupFpSet.end(), fileSet_group.begin(),
                    fileSet_group.end());
  allFpSet.insert(allFpSet.end(), fileSet_all.begin(), fileSet_all.end());
}

//------------------------------------------------------------

void TSystem::readDirectory(TFilePathSet &dst, const QDir &dir,
                            bool groupFrames) {
  if (!(dir.exists() && QFileInfo(dir.path()).isDir()))
    throw TSystemException(TFilePath(dir.path().toStdWString()),
                           " is not a directory");
  QStringList entries;
#ifdef _WIN32
  WIN32_FIND_DATA find_dir_data;
  QString dir_search_path = dir.absolutePath() + "\\*";
  QDir::Filters filter    = dir.filter();

  // store name filters
  bool hasNameFilter = false;
  QList<QRegularExpression> nameFilters;
  for (const QString &nameFilter : dir.nameFilters()) {
    if (nameFilter == "*") {
      hasNameFilter = false;
      break;
    }
    // Convert wildcard pattern to regular expression
    QString pattern =
        QRegularExpression::wildcardToRegularExpression(nameFilter);
    nameFilters.append(QRegularExpression(pattern));
    hasNameFilter = true;
  }

  auto addEntry = [&]() {
    // QDir::NoDotAndDotDot condition
    if (wcscmp(find_dir_data.cFileName, L".") != 0 &&
        wcscmp(find_dir_data.cFileName, L"..") != 0) {
      // QDir::Files condition
      if ((filter & QDir::Files) == 0 &&
          (find_dir_data.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY) == 0)
        return;
      // QDir::Dirs condition
      if ((filter & QDir::Dirs) == 0 &&
          (find_dir_data.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY))
        return;
      // QDir::Hidden condition
      if ((filter & QDir::Hidden) == 0 &&
          (find_dir_data.dwFileAttributes & FILE_ATTRIBUTE_HIDDEN))
        return;

      QString fileName = QString::fromWCharArray(find_dir_data.cFileName);

      // name filter
      if (hasNameFilter) {
        bool matched = false;
        for (const QRegularExpression &regExp : nameFilters) {
          QRegularExpressionMatch match = regExp.match(fileName);
          if (match.hasMatch()) {
            matched = true;
            break;
          }
        }
        if (!matched) return;
      }

      entries.append(fileName);
    }
  };
  HANDLE hFind =
      FindFirstFile((const wchar_t *)dir_search_path.utf16(), &find_dir_data);
  if (hFind != INVALID_HANDLE_VALUE) {
    do {
      addEntry();
    } while (FindNextFile(hFind, &find_dir_data));
    FindClose(hFind);  // FIXED: Added FindClose
  }
#else
  entries = (dir.entryList(dir.filter() | QDir::NoDotAndDotDot));
#endif

  TFilePath dirPath(dir.path().toStdWString());

  std::set<TFilePath, CaselessFilepathLess> fpSet;

  for (const QString &entry : entries) {
    TFilePath path(dirPath + TFilePath(entry.toStdWString()));

    if (groupFrames && path.getDots() == "..") path = path.withFrame();

    fpSet.insert(path);
  }

  dst.insert(dst.end(), fpSet.begin(), fpSet.end());
}

//------------------------------------------------------------

void TSystem::readDirectory(TFilePathSet &dst, const TFilePath &path,
                            bool groupFrames, bool onlyFiles,
                            bool getHiddenFiles) {
  QDir dir(toQString(path));

  QDir::Filters filters(QDir::Files);
  if (!onlyFiles) filters |= QDir::Dirs;
  if (getHiddenFiles) filters |= QDir::Hidden;
  dir.setFilter(filters);

  readDirectory(dst, dir, groupFrames);
}

//------------------------------------------------------------

void TSystem::readDirectory(TFilePathSet &dst, const TFilePathSet &pathSet,
                            bool groupFrames, bool onlyFiles,
                            bool getHiddenFiles) {
  for (const auto &path : pathSet)
    readDirectory(dst, path, groupFrames, onlyFiles, getHiddenFiles);
}

//------------------------------------------------------------

TFilePathSet TSystem::readDirectory(const TFilePath &path, bool groupFrames,
                                    bool onlyFiles, bool getHiddenFiles) {
  TFilePathSet filePathSet;
  readDirectory(filePathSet, path, groupFrames, onlyFiles, getHiddenFiles);
  return filePathSet;
}

//------------------------------------------------------------

TFilePathSet TSystem::readDirectory(const TFilePathSet &pathSet,
                                    bool groupFrames, bool onlyFiles,
                                    bool getHiddenFiles) {
  TFilePathSet dst;
  readDirectory(dst, pathSet, groupFrames, onlyFiles, getHiddenFiles);
  return dst;
}

//------------------------------------------------------------

void TSystem::readDirectoryTree(TFilePathSet &dst, const TFilePath &path,
                                bool groupFrames, bool onlyFiles) {
  if (!TFileStatus(path).isDirectory())
    throw TSystemException(path, " is not a directory");

  std::set<TFilePath, CaselessFilepathLess> fpSet;

  QFileInfoList fil = QDir(toQString(path)).entryInfoList();
  for (const QFileInfo &fi : fil) {
    if (fi.fileName() == QString(".") || fi.fileName() == QString(".."))
      continue;
    TFilePath son = TFilePath(fi.filePath().toStdWString());
    if (TFileStatus(son).isDirectory()) {
      if (!onlyFiles) dst.push_back(son);
      readDirectoryTree(dst, son, groupFrames, onlyFiles);
    } else {
      if (groupFrames && son.getDots() == "..") {
        son = son.withFrame();
      }
      fpSet.insert(son);
    }
  }

  dst.insert(dst.end(), fpSet.begin(), fpSet.end());
}

//------------------------------------------------------------

void TSystem::readDirectoryTree(TFilePathSet &dst, const TFilePathSet &pathSet,
                                bool groupFrames, bool onlyFiles) {
  for (const auto &path : pathSet)
    readDirectoryTree(dst, path, groupFrames, onlyFiles);
}

//------------------------------------------------------------

TFilePathSet TSystem::readDirectoryTree(const TFilePath &path, bool groupFrames,
                                        bool onlyFiles) {
  TFilePathSet dst;
  readDirectoryTree(dst, path, groupFrames, onlyFiles);
  return dst;
}

//------------------------------------------------------------

TFilePathSet TSystem::readDirectoryTree(const TFilePathSet &pathSet,
                                        bool groupFrames, bool onlyFiles) {
  TFilePathSet dst;
  readDirectoryTree(dst, pathSet, groupFrames, onlyFiles);
  return dst;
}

//------------------------------------------------------------

TFilePathSet TSystem::packLevelNames(const TFilePathSet &fps) {
  std::set<TFilePath> tmpSet;
  for (const auto &fp : fps)
    tmpSet.insert(fp.getParentDir() + fp.getLevelName());

  TFilePathSet fps2;
  for (const auto &fp : tmpSet) {
    fps2.push_back(fp);
  }
  return fps2;
}

//------------------------------------------------------------

TFilePathSet TSystem::getDisks() {
  TFilePathSet filePathSet;
  QFileInfoList fil = QDir::drives();
  for (const QFileInfo &fi : fil)
    filePathSet.push_back(TFilePath(fi.filePath().toStdWString()));

  return filePathSet;
}

//------------------------------------------------------------

class LocalThread final : public QThread {
public:
  static LocalThread *currentThread() {
    QThread *current = QThread::currentThread();
    return dynamic_cast<LocalThread *>(current);  // Safer cast
  }
  void sleep(TINT64 delay) { msleep(static_cast<unsigned long>(delay)); }
};

void TSystem::sleep(TINT64 delay) {
  LocalThread::currentThread()->sleep(delay);
}

//--------------------------------------------------------------

int TSystem::getProcessorCount() { return QThread::idealThreadCount(); }

//--------------------------------------------------------------

bool TSystem::doesExistFileOrLevel(const TFilePath &fp) {
  if (TFileStatus(fp).doesExist()) return true;

  if (fp.isLevelName()) {
    const TFilePath &parentDir = fp.getParentDir();
    if (!TFileStatus(parentDir).doesExist()) return false;

    TFilePathSet files;
    try {
      files = TSystem::readDirectory(parentDir, false, true, true);
    } catch (...) {
      return false;
    }

    for (const auto &file : files) {
      if (file.getLevelNameW() == fp.getLevelNameW()) return true;
    }
  } else if (fp.getType() == "psd") {
    QString name(QString::fromStdWString(fp.getWideName()));
    name.append(QString::fromStdString(fp.getDottedType()));

    int sepPos = name.indexOf("#");
    if (sepPos >= 0) {
      int dotPos              = name.indexOf(".", sepPos);
      int doubleUnderscorePos = name.indexOf("__", sepPos);

      int removeChars = 0;
      if (dotPos > sepPos) {
        removeChars = dotPos - sepPos;
      }
      if (doubleUnderscorePos > 0 && doubleUnderscorePos < dotPos) {
        removeChars = doubleUnderscorePos - sepPos;
      }

      if (removeChars > 0) {
        name.remove(sepPos, removeChars);
      }
    }

    TFilePath psdpath(fp.getParentDir() + TFilePath(name.toStdWString()));
    if (TFileStatus(psdpath).doesExist()) return true;
  }

  return false;
}

//--------------------------------------------------------------

void TSystem::copyFileOrLevel_throw(const TFilePath &dst,
                                    const TFilePath &src) {
  if (src.isLevelName()) {
    TFilePathSet files;
    files = TSystem::readDirectory(src.getParentDir(), false);

    for (const auto &file : files) {
      if (file.getLevelNameW() == src.getLevelNameW()) {
        TFilePath src1 = file;
        TFilePath dst1 = dst.withFrame(file.getFrame());

        TSystem::copyFile(dst1, src1);
      }
    }
  } else {
    TSystem::copyFile(dst, src);
  }
}

//--------------------------------------------------------------

bool TSystem::renameImageSequence(const TFilePathSet &files,
                                  const TFilePath &levelPath,
                                  int prefixLength) {
  std::string levelBaseName = levelPath.withoutParentDir().getName();
  if (files.empty() || !files.begin()->isAbsolute() ||
      !levelPath.isAbsolute()) {
    return false;
  }

  std::wstring wstr;
  TFilePath dst;

  for (const TFilePath &file : files) {
    wstr = file.getWideName();
    if (wstr.size() > static_cast<size_t>(prefixLength))
      wstr = wstr.substr(prefixLength);
    else
      wstr.clear();

    dst = file.getParentDir() + (levelBaseName + "." + TFrameId(wstr).expand() +
                                 file.getDottedType());
    try {
      TSystem::renameFile(dst, file, false);
    } catch (...) {
      return false;
    }
  }
  return true;
}

void TSystem::renameFileOrLevel_throw(const TFilePath &dst,
                                      const TFilePath &src,
                                      bool renamePalette) {
  if (renamePalette && ((src.getType() == "tlv") || (src.getType() == "tzp") ||
                        (src.getType() == "tzu"))) {
    // Special case: since renames cannot be 'grouped' in the UI, palettes are
    // automatically renamed here if required
    const char *type = (src.getType() == "tlv") ? "tpl" : "plt";

    TFilePath srcpltname(src.withNoFrame().withType(type));
    TFilePath dstpltname(dst.withNoFrame().withType(type));

    if (TSystem::doesExistFileOrLevel(src) &&
        TSystem::doesExistFileOrLevel(srcpltname))
      TSystem::renameFile(dstpltname, srcpltname, false);
  }

  if (src.isLevelName()) {
    TFilePathSet files;
    files = TSystem::readDirectory(src.getParentDir(), false);

    for (const auto &file : files) {
      if (file.getLevelName() == src.getLevelName()) {
        TFilePath src1 = file;
        TFilePath dst1 = dst.withFrame(file.getFrame());

        TSystem::renameFile(dst1, src1);
      }
    }
  } else {
    TSystem::renameFile(dst, src);
  }
}

//--------------------------------------------------------------

void TSystem::removeFileOrLevel_throw(const TFilePath &fp) {
  if (fp.isLevelName()) {
    TFilePathSet files;
    files = TSystem::readDirectory(fp.getParentDir(), false, true, true);

    for (const auto &file : files) {
      if (file.getLevelName() == fp.getLevelName()) TSystem::deleteFile(file);
    }
  } else {
    TSystem::deleteFile(fp);
  }
}

//--------------------------------------------------------------

void TSystem::hideFileOrLevel_throw(const TFilePath &fp) {
  if (fp.isLevelName()) {
    TFilePathSet files;
    files = TSystem::readDirectory(fp.getParentDir(), false);

    for (const auto &file : files) {
      if (file.getLevelNameW() == fp.getLevelNameW()) TSystem::hideFile(file);
    }
  } else {
    TSystem::hideFile(fp);
  }
}

//--------------------------------------------------------------

void TSystem::moveFileOrLevelToRecycleBin_throw(const TFilePath &fp) {
  if (fp.isLevelName()) {
    TFilePathSet files;
    files = TSystem::readDirectory(fp.getParentDir(), false, true, true);

    for (const auto &file : files) {
      if (file.getLevelNameW() == fp.getLevelNameW())
        TSystem::moveFileToRecycleBin(file);
    }
  } else {
    TSystem::moveFileToRecycleBin(fp);
  }
}

//--------------------------------------------------------------

bool TSystem::copyFileOrLevel(const TFilePath &dst, const TFilePath &src) {
  try {
    copyFileOrLevel_throw(dst, src);
  } catch (...) {
    return false;
  }
  return true;
}

//--------------------------------------------------------------

bool TSystem::renameFileOrLevel(const TFilePath &dst, const TFilePath &src,
                                bool renamePalette) {
  try {
    renameFileOrLevel_throw(dst, src, renamePalette);
  } catch (...) {
    return false;
  }
  return true;
}

//--------------------------------------------------------------

bool TSystem::removeFileOrLevel(const TFilePath &fp) {
  try {
    removeFileOrLevel_throw(fp);
  } catch (...) {
    return false;
  }
  return true;
}

//--------------------------------------------------------------

bool TSystem::hideFileOrLevel(const TFilePath &fp) {
  try {
    hideFileOrLevel_throw(fp);
  } catch (...) {
    return false;
  }
  return true;
}

//--------------------------------------------------------------

bool TSystem::moveFileOrLevelToRecycleBin(const TFilePath &fp) {
  try {
    moveFileOrLevelToRecycleBin_throw(fp);
  } catch (...) {
    return false;
  }
  return true;
}

//--------------------------------------------------------------

bool TSystem::touchParentDir(const TFilePath &fp) {
  TFilePath parentDir = fp.getParentDir();
  TFileStatus fs(parentDir);
  if (fs.isDirectory())
    return true;
  else if (fs.doesExist())
    return false;
  try {
    mkDir(parentDir);
  } catch (...) {
    return false;
  }
  return true;
}

//--------------------------------------------------------------

bool TSystem::showDocument(const TFilePath &path) {
#ifdef _WIN32
  HINSTANCE ret = ShellExecuteW(0, L"open", path.getWideString().c_str(), 0, 0,
                                SW_SHOWNORMAL);

  // ShellExecute returns a value greater than 32 if successful
  if (reinterpret_cast<intptr_t>(ret) <= 32) {
    qWarning("Can't open document: %ls", path.getWideString().c_str());
    return false;
  }
  return true;
#else
  // Use QDesktopServices for cross-platform support
  QUrl url = QUrl::fromLocalFile(toQString(path));
  if (!QDesktopServices::openUrl(url)) {
    qWarning("Can't open document: %s", toQString(path).toUtf8().constData());
    return false;
  }
  return true;
#endif
}

bool TSystem::isDLLBlackListed(QString dllFile) {
  static const QStringList dllBlackList = {"lvcod64.dll", "ff_vfw.dll",
                                           "tsccvid64.dll", "hapcodec.dll"};

  for (const QString &blacklisted : dllBlackList) {
    if (dllFile.contains(blacklisted, Qt::CaseInsensitive)) {
      return true;
    }
  }

  return false;
}

#else

#include <windows.h>

void TSystem::sleep(TINT64 delay) { Sleep(static_cast<DWORD>(delay)); }

// handle errors with GetLastError?
void TSystem::deleteFile(const TFilePath &fp) {
  // Empty implementation for light version
}

void TSystem::rmDirTree(const TFilePath &path) {
  // Empty implementation for light version
}

//------------------------------------------------------------

#endif  // TNZCORE_LIGHT

//--------------------------------------------------------------

TSystemException::TSystemException(const TFilePath &fname, int err)
    : m_fname(fname)
    , m_err(err)
    , m_msg(L"")

{}

//--------------------------------------------------------------

TSystemException::TSystemException(const TFilePath &fname,
                                   const std::string &msg)
    : m_fname(fname), m_err(-1), m_msg(::to_wstring(msg)) {}
//--------------------------------------------------------------

TSystemException::TSystemException(const TFilePath &fname,
                                   const std::wstring &msg)
    : m_fname(fname), m_err(-1), m_msg(msg) {}

//--------------------------------------------------------------

TSystemException::TSystemException(const std::string &msg)
    : m_fname(""), m_err(-1), m_msg(::to_wstring(msg)) {}
//--------------------------------------------------------------

TSystemException::TSystemException(const std::wstring &msg)
    : m_fname(""), m_err(-1), m_msg(msg) {}
