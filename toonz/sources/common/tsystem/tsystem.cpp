

#include "tsystem.h"

using namespace std;

#include <set>
#include "tfilepath_io.h"
#include "tconvert.h"

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

int HasMainLoop = -1;

}  // namespace
//-----------------------------------------------------------------------------------

TFileStatus::TFileStatus(const TFilePath &path) {
  m_fileInfo = QFileInfo(QString::fromStdWString(path.getWideString()));
  m_exist    = m_fileInfo.exists();
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
  return m_fileInfo.created();
}

//-----------------------------------------------------------------------------------

QFile::Permissions TFileStatus::getPermissions() const {
  if (!m_exist) return 0;
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
  if (HasMainLoop == -1)
    assert(!"you MUST call the TSystem::hasMainLoop function in the main of the program!");
  return HasMainLoop == 1;
}

//-----------------------------------------------------------------------------------

void TSystem::hasMainLoop(bool state) {
  assert(HasMainLoop == -1);
  HasMainLoop = state ? 1 : 0;
}

//-----------------------------------------------------------------------------------

QString TSystem::getHostName() { return QHostInfo::localHostName(); }

//------------------------------------------------------------

QString TSystem::getUserName() {
  QStringList list = QProcess::systemEnvironment();
  int j;
  for (j = 0; j < list.size(); j++) {
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

TFilePath TSystem::getTestDir(string name) {
  return TFilePath("C:") + TFilePath(name);
}

//------------------------------------------------------------

QString TSystem::getSystemValue(const TFilePath &name) {
  QStringList strlist = toQString(name).split("\\", QString::SkipEmptyParts);

  assert(strlist.size() > 3);
  assert(strlist.at(0) == "SOFTWARE");

  QSettings qs(QSettings::SystemScope, strlist.at(1), strlist.at(2));

  int i;
  QString varName;

  for (i = 3; i < strlist.size(); i++) {
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
                     .toString()
                     .replace("-", "")
                     .replace("{", "")
                     .replace("}", "")
                     .toLatin1()
                     .data();

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
  TFilePathSet::const_iterator it;
  for (it = pathSet.begin(); it != pathSet.end(); it++) {
    QFile f(toQString(*it));
    f.setPermissions(permissions);
  }
}
}

// gestire exception
void TSystem::mkDir(const TFilePath &path) {
  TFilePathSet pathSet = getPathsToCreate(path);
  QString qPath        = toQString(path);
  assert(!qPath.contains("+"));
  if (!QDir::current().mkpath(qPath))
    throw TSystemException(path, "can't create folder!");

  setPathsPermissions(
      pathSet, QFile::ReadUser | QFile::WriteUser | QFile::ExeUser |
                   QFile::ReadGroup | QFile::WriteGroup | QFile::ExeGroup |
                   QFile::ReadOther | QFile::WriteOther | QFile::ExeOther);
}

//------------------------------------------------------------
// gestire exception
void TSystem::rmDir(const TFilePath &path) {
  if (!QDir(toQString(path.getParentDir()))
           .rmdir(QString::fromStdString(path.getName())))
    throw TSystemException(path, "can't remove folder!");
}

// vinz

//------------------------------------------------------------

namespace {
void rmDirTree(const QString &path) {
  int i;
  QFileInfoList fil = QDir(path).entryInfoList();
  for (i = 0; i < fil.size(); i++) {
    QFileInfo fi = fil.at(i);
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

  int i;
  for (i = 0; i < fil.size(); i++) {
    QFileInfo fi = fil.at(i);
    if (fi.fileName() == QString(".") || fi.fileName() == QString(".."))
      continue;
    if (fi.isDir()) {
      TFilePath srcDir = TFilePath(fi.filePath().toStdString());
      TFilePath dstDir = dst + srcDir.getName();
      copyDir(dstDir, srcDir);
    } else
      QFile::copy(fi.filePath(),
                  toQString(dst) + QString("\\") + fi.fileName());
  }
}

//------------------------------------------------------------
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

wstring getFormattedMessage(DWORD lastError)
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
  return wstring();

wchar_t* wBuffer = new wchar_t [wSize+1];
MultiByteToWideChar(0,0,(char*)lpMsgBuf,-1,wBuffer,wSize);
wBuffer[wSize]='\0';
wstring wmsg(wBuffer);

delete []wBuffer;
LocalFree(lpMsgBuf);
return wmsg;
}

//------------------------------------------------------------

#endif
*/
//------------------------------------------------------------

void TSystem::copyFile(const TFilePath &dst, const TFilePath &src,
                       bool overwrite) {
  assert(dst != TFilePath());

  if (dst == src) return;

  // Create the containing folder before trying to copy or it will crash!
  touchParentDir(dst);

  const QString &qDst = toQString(dst);
  if (overwrite && QFile::exists(qDst)) QFile::remove(qDst);

  if (!QFile::copy(toQString(src), qDst))
    throw TSystemException(dst, "can't copy file!");
}

//------------------------------------------------------------

void TSystem::renameFile(const TFilePath &dst, const TFilePath &src,
                         bool overwrite) {
  assert(dst != TFilePath());

  if (dst == src) return;

  const QString &qDst = toQString(dst);
  if (overwrite && QFile::exists(qDst)) QFile::remove(qDst);

  if (!QFile::rename(toQString(src), qDst))
    throw TSystemException(dst, "can't rename file!");
}

//------------------------------------------------------------

// gestire gli errori con GetLastError?
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

class CaselessFilepathLess final
    : public std::binary_function<TFilePath, TFilePath, bool> {
public:
  bool operator()(const TFilePath &a, const TFilePath &b) const {
    // Perform case sensitive compare, fallback to case insensitive.
    const wstring a_str = a.getWideString();
    const wstring b_str = b.getWideString();

    unsigned int i   = 0;
    int case_compare = -1;
    while (a_str[i] || b_str[i]) {
      if (a_str[i] != b_str[i]) {
        const wchar_t a_wchar = towlower(a_str[i]);
        const wchar_t b_wchar = towlower(b_str[i]);
        if (a_wchar < b_wchar) {
          return true;
        } else if (a_wchar > b_wchar) {
          return false;
        } else if (case_compare == -1) {
          case_compare = a_str[i] < b_str[i];
        }
      }
      i++;
    }
    return (case_compare == 1);
  }
};

//------------------------------------------------------------
/*! return the file list which is readable and executable
*/
void TSystem::readDirectory_Dir_ReadExe(TFilePathSet &dst,
                                        const TFilePath &path) {
  if (!TFileStatus(path).isDirectory())
    throw TSystemException(path, " is not a directory");

  std::set<TFilePath, CaselessFilepathLess> fileSet;

  QStringList fil =
      QDir(toQString(path))
          .entryList(QDir::Dirs | QDir::NoDotAndDotDot | QDir::Readable);

  int i;
  for (i = 0; i < fil.size(); i++) {
    QString fi = fil.at(i);

    TFilePath son = path + TFilePath(fi.toStdWString());

    fileSet.insert(son);
  }

  dst.insert(dst.end(), fileSet.begin(), fileSet.end());
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

  QStringList fil =
      QDir(toQString(path))
          .entryList(QDir::Files | QDir::NoDotAndDotDot | QDir::Readable);

  if (fil.size() == 0) return;

  for (int i = 0; i < fil.size(); i++) {
    QString fi = fil.at(i);

    TFilePath son = path + TFilePath(fi.toStdWString());

    // store all file paths
    fileSet_all.insert(son);

    // in case of the sequencial files
    if (son.getDots() == "..") son = son.withFrame();

    // store the group. insersion avoids duplication of the item
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

  QStringList entries(dir.entryList(dir.filter() | QDir::NoDotAndDotDot));
  TFilePath dirPath(dir.path().toStdWString());

  std::set<TFilePath, CaselessFilepathLess> fpSet;

  int e, eCount = entries.size();
  for (e = 0; e != eCount; ++e) {
    TFilePath path(dirPath + TFilePath(entries.at(e).toStdWString()));

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
  for (TFilePathSet::const_iterator it = pathSet.begin(); it != pathSet.end();
       it++)
    readDirectory(dst, *it, groupFrames, onlyFiles);
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

  QFileInfoList fil = QDir(toQString(path)).entryInfoList();
  int i;
  for (i = 0; i < fil.size(); i++) {
    QFileInfo fi = fil.at(i);
    if (fi.fileName() == QString(".") || fi.fileName() == QString(".."))
      continue;
    TFilePath son = TFilePath(fi.filePath().toStdWString());
    if (TFileStatus(son).isDirectory()) {
      if (!onlyFiles) dst.push_back(son);
      readDirectoryTree(dst, son, groupFrames, onlyFiles);
    } else
      dst.push_back(son);
  }
}

//------------------------------------------------------------

void TSystem::readDirectoryTree(TFilePathSet &dst, const TFilePathSet &pathSet,
                                bool groupFrames, bool onlyFiles) {
  for (TFilePathSet::const_iterator it = pathSet.begin(); it != pathSet.end();
       it++)
    readDirectoryTree(dst, *it, groupFrames, onlyFiles);
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
  TFilePathSet::const_iterator cit;
  for (cit = fps.begin(); cit != fps.end(); ++cit)
    tmpSet.insert(cit->getParentDir() + cit->getLevelName());

  TFilePathSet fps2;
  for (std::set<TFilePath>::const_iterator c_sit = tmpSet.begin();
       c_sit != tmpSet.end(); ++c_sit) {
    fps2.push_back(*c_sit);
  }
  return fps2;
}

//------------------------------------------------------------

TFilePathSet TSystem::getDisks() {
  TFilePathSet filePathSet;
  QFileInfoList fil = QDir::drives();
  int i;
  for (i = 0; i < fil.size(); i++)
    filePathSet.push_back(TFilePath(fil.at(i).filePath().toStdWString()));

  return filePathSet;
}

//------------------------------------------------------------

class LocalThread final : public QThread {
public:
  static LocalThread *currentThread() {
    return (LocalThread *)QThread::currentThread();
  }
  void sleep(TINT64 delay) { msleep(delay); }
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
    }

    TFilePathSet::iterator it, end = files.end();
    for (it = files.begin(); it != end; ++it) {
      if (it->getLevelNameW() == fp.getLevelNameW()) return true;
    }
  } else if (fp.getType() == "psd") {
    QString name(QString::fromStdWString(fp.getWideName()));
    name.append(QString::fromStdString(fp.getDottedType()));

    int sepPos                               = name.indexOf("#");
    int dotPos                               = name.indexOf(".", sepPos);
    int removeChars                          = dotPos - sepPos;
    int doubleUnderscorePos                  = name.indexOf("__", sepPos);
    if (doubleUnderscorePos > 0) removeChars = doubleUnderscorePos - sepPos;

    name.remove(sepPos, removeChars);

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

    TFilePathSet::iterator it, end = files.end();
    for (it = files.begin(); it != end; ++it) {
      if (it->getLevelNameW() == src.getLevelNameW()) {
        TFilePath src1 = *it;
        TFilePath dst1 = dst.withFrame(it->getFrame());

        TSystem::copyFile(dst1, src1);
      }
    }
  } else
    TSystem::copyFile(dst, src);
}

//--------------------------------------------------------------

void TSystem::renameFileOrLevel_throw(const TFilePath &dst,
                                      const TFilePath &src,
                                      bool renamePalette) {
  if (renamePalette && ((src.getType() == "tlv") || (src.getType() == "tzp") ||
                        (src.getType() == "tzu"))) {
    // Special case: since renames cannot be 'grouped' in the UI, palettes are
    // automatically
    // renamed here if required
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

    for (TFilePathSet::iterator it = files.begin(); it != files.end(); it++) {
      if (it->getLevelName() == src.getLevelName()) {
        TFilePath src1 = *it;
        TFilePath dst1 = dst.withFrame(it->getFrame());

        TSystem::renameFile(dst1, src1);
      }
    }
  } else
    TSystem::renameFile(dst, src);
}

//--------------------------------------------------------------

void TSystem::removeFileOrLevel_throw(const TFilePath &fp) {
  if (fp.isLevelName()) {
    TFilePathSet files;
    files = TSystem::readDirectory(fp.getParentDir(), false, true, true);

    TFilePathSet::iterator it, end = files.end();
    for (it = files.begin(); it != end; ++it) {
      if (it->getLevelName() == fp.getLevelName()) TSystem::deleteFile(*it);
    }
  } else
    TSystem::deleteFile(fp);
}

//--------------------------------------------------------------

void TSystem::hideFileOrLevel_throw(const TFilePath &fp) {
  if (fp.isLevelName()) {
    TFilePathSet files;
    files = TSystem::readDirectory(fp.getParentDir(), false);

    TFilePathSet::iterator it, end = files.end();
    for (it = files.begin(); it != end; ++it) {
      if (it->getLevelNameW() == fp.getLevelNameW()) TSystem::hideFile(*it);
    }
  } else
    TSystem::hideFile(fp);
}

//--------------------------------------------------------------

void TSystem::moveFileOrLevelToRecycleBin_throw(const TFilePath &fp) {
  if (fp.isLevelName()) {
    TFilePathSet files;
    files = TSystem::readDirectory(fp.getParentDir(), false, true, true);

    TFilePathSet::iterator it, end = files.end();
    for (it = files.begin(); it != end; ++it) {
      if (it->getLevelNameW() == fp.getLevelNameW())
        TSystem::moveFileToRecycleBin(*it);
    }
  } else
    TSystem::moveFileToRecycleBin(fp);
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
  int ret = (int)ShellExecuteW(0, L"open", path.getWideString().c_str(), 0, 0,
                               SW_SHOWNORMAL);
  if (ret <= 32) {
    return false;
    throw TSystemException(path, "Can't open");
  }
  return true;
#else
  string cmd = "open ";
  string thePath(::to_string(path));
  UINT pos = 0, count = 0;
  // string newPath;
  char newPath[2048];

  while (pos < thePath.size()) {
    char c                         = thePath[pos];
    if (c == ' ') newPath[count++] = '\\';

    newPath[count++] = c;
    ++pos;
  }
  newPath[count] = 0;

  cmd = cmd + string(newPath);
  system(cmd.c_str());
  return true;
#endif
}

#else

#include <windows.h>

void TSystem::sleep(TINT64 delay) { Sleep((DWORD)delay); }

// gestire gli errori con GetLastError?
void TSystem::deleteFile(const TFilePath &fp) { assert(false); }

void TSystem::rmDirTree(const TFilePath &path) { assert(false); }

//------------------------------------------------------------

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

TSystemException::TSystemException(const TFilePath &fname, const wstring &msg)
    : m_fname(fname), m_err(-1), m_msg(msg) {}

//--------------------------------------------------------------

TSystemException::TSystemException(const std::string &msg)
    : m_fname(""), m_err(-1), m_msg(::to_wstring(msg)) {}
//--------------------------------------------------------------

TSystemException::TSystemException(const wstring &msg)
    : m_fname(""), m_err(-1), m_msg(msg) {}
