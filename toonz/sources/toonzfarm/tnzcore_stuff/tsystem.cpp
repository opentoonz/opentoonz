

#include "tsystem.h"

#include <time.h>
#include <sys/timeb.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <errno.h>
#include <set>

#undef PLATFORM

#ifdef WIN32
#define PLATFORM WIN32
#include <process.h>
#include <io.h>
#include <stdlib.h>
#include <direct.h>
#include "winsock2.h"
#include "lmcons.h"
#include <sys/utime.h>
#endif

#ifdef LINUX
#define PLATFORM LINUX
#include <grp.h>
#include <utime.h>
#include <sys/param.h>
#include <unistd.h>
#include <sys/types.h>
#include <stdio.h>
#include <unistd.h>
#include <dirent.h>
#include <sys/dir.h>
#include <sys/sysinfo.h>
#include <sys/swap.h>
#include <sys/statfs.h>
#include <pwd.h>
#include <mntent.h>
#include <dlfcn.h>

extern "C" {
int gethostname(char *, size_t);
// int L_cuserid;
char *cuserid(char *);
char *tempnam(const char *, const char *);
}

#endif

#ifdef __sgi
#define PLATFORM SGI
#include <sys/param.h>
#include <unistd.h>
#include <grp.h>
#include <sys/dir.h>  // dirent.h
#include <sys/utime.h>
#include <sys/swap.h>
#include <sys/statfs.h>
#include <pwd.h>
#include <mntent.h>

#include <dlfcn.h>

#endif

#ifndef PLATFORM
PLATFORM_NOT_SUPPORTED
#endif

using namespace std;

#ifdef WIN32
const int TSystem::MaxPathLen     = _MAX_PATH;
const int TSystem::MaxFNameLen    = _MAX_FNAME;
const int TSystem::MaxHostNameLen = 64;

const unsigned short TFileStatus::UserReadable   = _S_IREAD;
const unsigned short TFileStatus::UserWritable   = _S_IWRITE;
const unsigned short TFileStatus::UserExecutable = _S_IEXEC;
const unsigned short TFileStatus::OwnerReadWriteExec =
    TFileStatus::UserReadable | TFileStatus::UserWritable |
    TFileStatus::UserExecutable;
const unsigned short TFileStatus::OwnerReadable   = TFileStatus::UserReadable;
const unsigned short TFileStatus::OwnerWritable   = TFileStatus::UserWritable;
const unsigned short TFileStatus::OwnerExecutable = TFileStatus::UserExecutable;
const unsigned short TFileStatus::GroupReadWriteExec =
    TFileStatus::OwnerReadWriteExec;
const unsigned short TFileStatus::GroupReadable   = TFileStatus::UserReadable;
const unsigned short TFileStatus::GroupWritable   = TFileStatus::UserWritable;
const unsigned short TFileStatus::GroupExecutable = TFileStatus::UserExecutable;
const unsigned short TFileStatus::OtherReadWriteExec =
    TFileStatus::UserReadable | TFileStatus::UserWritable |
    TFileStatus::UserExecutable;
;
const unsigned short TFileStatus::OtherReadable   = TFileStatus::UserReadable;
const unsigned short TFileStatus::OtherWritable   = TFileStatus::UserWritable;
const unsigned short TFileStatus::OtherExecutable = TFileStatus::UserReadable;

const unsigned short TFileStatus::IfDir = _S_IFDIR;

#define MKDIR _mkdir
#define RMDIR _rmdir
#define TEMPNAM _tempnam
#define ACCESS _access
#define STAT _stat
#define UTIME _utime
#define FTIME _ftime
#else  // these are common for IRIX & LINUX
const int TSystem::MaxPathLen     = MAXPATHLEN;
const int TSystem::MaxHostNameLen = MAXHOSTNAMELEN;

const unsigned short TFileStatus::OwnerReadWriteExec = S_IRWXU;
const unsigned short TFileStatus::OwnerReadable      = S_IRUSR;
const unsigned short TFileStatus::OwnerWritable      = S_IWUSR;
const unsigned short TFileStatus::OwnerExecutable    = S_IXUSR;
const unsigned short TFileStatus::GroupReadWriteExec = S_IRWXG;
const unsigned short TFileStatus::GroupReadable      = S_IRGRP;
const unsigned short TFileStatus::GroupWritable      = S_IWGRP;
const unsigned short TFileStatus::GroupExecutable    = S_IXGRP;
const unsigned short TFileStatus::OtherReadWriteExec = S_IRWXO;
const unsigned short TFileStatus::OtherReadable      = S_IROTH;
const unsigned short TFileStatus::OtherWritable      = S_IWOTH;
const unsigned short TFileStatus::OtherExecutable    = S_IXOTH;

#define MKDIR mkdir
#define RMDIR rmdir
#define TEMPNAM tempnam
#define ACCESS access
#define STAT stat
#define UTIME utime
#define FTIME ftime

#ifdef __sgi
const int TSystem::MaxFNameLen                       = MAXNAMELEN;
const unsigned short TFileStatus::UserReadable       = S_IREAD;
const unsigned short TFileStatus::UserWritable       = S_IWRITE;
const unsigned short TFileStatus::UserExecutable     = S_IEXEC;
const unsigned short TFileStatus::IfDir              = S_IFDIR;
#else
const int TSystem::MaxFNameLen                   = 1024;
const unsigned short TFileStatus::UserReadable   = __S_IREAD;
const unsigned short TFileStatus::UserWritable   = __S_IWRITE;
const unsigned short TFileStatus::UserExecutable = __S_IEXEC;
const unsigned short TFileStatus::IfDir          = __S_IFDIR;
#endif

#endif

//-----------------------------------------------------------------------------------

ostream &operator<<(ostream &out, const TSystemException &e) {
  return out << "TSystem Exception : " << e.getMessage() << endl;
}

//-----------------------------------------------------------------------------------
// conversion functions
namespace {

//---------------------------------------------------------

void makeTm(const TTime &t, tm *p) {
  ::memset(p, 0, sizeof(tm));
  p->tm_year  = t.getYear() - 1900;
  p->tm_mon   = t.getMonth() - 1;  //[0,11]
  p->tm_mday  = t.getDay();
  p->tm_hour  = t.getHour();
  p->tm_min   = t.getMinutes();
  p->tm_sec   = t.getSeconds();
  p->tm_isdst = -1;
}

//---------------------------------------------------------

TTime makeTTime(const tm &p, int msec = 0) {
  return TTime(p.tm_year + 1900, p.tm_mon + 1, p.tm_mday, p.tm_hour, p.tm_min,
               p.tm_sec, msec);
}

//---------------------------------------------------------

time_t makeTimeT(const TTime &t) {
  tm x;
  makeTm(t, &x);
  time_t tt = mktime(&x);
  return tt;
}

}  // namespace

//-----------------------------------------------------------------------------------

TDeltaTime::TDeltaTime(long sec, long msec) {
  // assert((sec && msec>=0) ||(sec==0 ));
  assert(!((sec < 0 && msec > 0) || (sec > 0 && msec < 0)));

  /*
0   0   ok
0   1   ok
1   0   ok
1   1   ok
0  -1   ok
-1   0   ok
-1  -1   ok
-1   1   NO
1  -1   NO
*/

  m_dsec  = sec + msec / 1000;
  m_dmsec = msec % 1000;
}

//------------------------------------------------------------

TDeltaTime TDeltaTime::operator+(const TDeltaTime &tdt) {
  long sumSec  = m_dsec + tdt.m_dsec;
  long sumMsec = m_dmsec + tdt.m_dmsec;

  if (sumSec > 0 && sumMsec < 0) return TDeltaTime(sumSec - 1, sumMsec + 1000);

  if (sumSec < 0 && sumMsec > 0) return TDeltaTime(sumSec + 1, sumMsec - 1000);

  return TDeltaTime(sumSec, sumMsec);
}

//------------------------------------------------------------

TDeltaTime TDeltaTime::operator-(const TDeltaTime &tdt) {
  long diffSec  = m_dsec - tdt.m_dsec;
  long diffMsec = m_dmsec - tdt.m_dmsec;

  if (diffSec < 0 && diffMsec > 0)
    return TDeltaTime(diffSec + 1, diffMsec - 1000);

  if (diffSec > 0 && diffMsec < 0)
    return TDeltaTime(diffSec - 1, diffMsec + 1000);

  return TDeltaTime(diffSec, diffMsec);
}

//------------------------------------------------------------

ostream &operator<<(ostream &out, const TDeltaTime &dt) {
  return out << dt.getSeconds() << "." << dt.getMilliSeconds();
}

//-----------------------------------------------------------------------------------

// correggere gli assert => invalid (esiste il metodo bool isValid()const;
// operazioni con
// un TTIme invalid danno un risultato invalid
//
//
TTime::TTime(int yyyy, int mm, int dd, int hh, int min, int sec, int msec) {
  if (yyyy < 10)
    yyyy += 2000;
  else if (yyyy < 99)
    yyyy += 1900;
  assert(1970 <= yyyy && yyyy <= 2038);
  m_y = yyyy;
  assert(1 <= mm && mm <= 12);
  m_m = mm;

  int table[] = {31, 28, 31, 30, 31, 30, 31, 31, 30, 31, 30, 31};
  int ddlimit = mm == 2 ? (28 + (isLeap() ? 1 : 0)) : table[mm - 1];
  assert(1 <= dd && dd <= ddlimit);
  m_d = dd;

  assert(0 <= hh && hh <= 23);
  m_h = hh;

  assert(0 <= min && min <= 59);
  m_min = min;

  assert(0 <= sec && sec <= 59);
  m_sec = sec;

  assert(0 <= msec && msec <= 999);
  m_msec = msec;
}

//-----------------------------------------------------------------------------------

TTime::TTime(const TTime &t)
    : m_y(t.m_y)
    , m_m(t.m_m)
    , m_d(t.m_d)
    , m_h(t.m_h)
    , m_min(t.m_min)
    , m_sec(t.m_sec)
    , m_msec(t.m_msec) {}

//-----------------------------------------------------------------------------------

bool TTime::operator==(const TTime &time) const {
  return (m_y == time.m_y) && (m_m == time.m_m) && (m_d == time.m_d) &&
         (m_h == time.m_h) && (m_min == time.m_min) && (m_sec == time.m_sec) &&
         (m_msec == time.m_msec);
}

//-----------------------------------------------------------------------------------

bool TTime::operator>(const TTime &time) const {
  return (m_y > time.m_y) || (m_y == time.m_y && m_m > time.m_m) ||
         (m_y == time.m_y && m_m == time.m_m && m_d > time.m_d) ||
         (m_y == time.m_y && m_m == time.m_m && m_d == time.m_d &&
          m_h > time.m_h) ||
         (m_y == time.m_y && m_m == time.m_m && m_d == time.m_d &&
          m_h == time.m_h && m_min > time.m_min) ||
         (m_y == time.m_y && m_m == time.m_m && m_d == time.m_d &&
          m_h == time.m_h && m_min == time.m_min && m_sec > time.m_sec) ||
         (m_y == time.m_y && m_m == time.m_m && m_d == time.m_d &&
          m_h == time.m_h && m_min == time.m_min && m_sec == time.m_sec &&
          m_msec > time.m_msec);
}

//-----------------------------------------------------------------------------------

TDeltaTime TTime::operator-(const TTime &t) const {
  time_t t2 = makeTimeT(*this);
  assert(t2 != -1);
  time_t t1 = makeTimeT(t);
  assert(t1 != -1);
  double d = difftime(t2, t1);
  if (m_msec >= t.m_msec) {
    if (d <= -1)
      return TDeltaTime((long)(d + 1), -(long)(1000 - (m_msec - t.m_msec)));
    else
      return TDeltaTime((long)d, (long)(m_msec - t.m_msec));
  } else {
    if (d <= 0)
      return TDeltaTime((long)(d), (long)(m_msec - t.m_msec));
    else
      return TDeltaTime((long)(d - 1), (long)(1000 + m_msec - t.m_msec));
  }
}

//-----------------------------------------------------------------------------------

TTime TTime::operator+(const TDeltaTime &tdt) const {
  tm x;
  makeTm(*this, &x);
  int ms = m_msec + tdt.getMilliSeconds();
  if (ms < 0) {
    ms += 1000;
    x.tm_sec += tdt.getSeconds() - 1;
  } else
    x.tm_sec += tdt.getSeconds() + ms / 1000;

  mktime(&x);
  return makeTTime(x, ms % 1000);
}

//-----------------------------------------------------------------------------------

TTime TTime::operator-(const TDeltaTime &tdt) const {
  tm x;
  makeTm(*this, &x);
  int ms = m_msec - tdt.getMilliSeconds();
  if (ms < 0) {
    ms += 1000;
    x.tm_sec -= tdt.getSeconds() + 1;
  } else
    x.tm_sec -= tdt.getSeconds() + ms / 1000;

  mktime(&x);
  return makeTTime(x, ms % 1000);
}
//-----------------------------------------------------------------------------------

string TTime::getFormattedString(char *fmt) const {
  tm x;
  makeTm(*this, &x);
  char strDest[256];
  if (!fmt)
    // fmt="%b %d %Y %X";
    return getDate() + " " + getTime();
  strftime(strDest, 256, fmt, &x);
  return string(strDest);
}

//-----------------------------------------------------------------------------------

string TTime::getDate() const { return getFormattedString("%b %d %Y"); }

//-----------------------------------------------------------------------------------

string TTime::getTime() const {  // hh:mm:ss
  stringstream buff_s;
  buff_s << "." << m_msec << '\0';
  return getFormattedString("%X") + buff_s.str();
}

//-----------------------------------------------------------------------------------

TTime TTime::floor() const {
  return TTime(m_y, m_m, m_d, m_h, m_min, m_sec, 0);
}

//-----------------------------------------------------------------------------------

TTime TTime::ceil() const {
  if (m_msec == 0)
    return *this;
  else
    return *this + TDeltaTime(0, 1000 - m_msec);
}

//===================================================================================

TFileStatus::TFileStatus(const TFilePath &path) {
  int acc = ACCESS(path.getFullPath().c_str(), 00);  // 00 == solo esistenza
  m_exist = acc != -1;
  // gestire eccezioni controllando il valore di ritorno di access
  // controllare il valore di errno
  // int ret=
  STAT(path.getFullPath().c_str(),
       &m_fStatus);  // returns 0 if the file-status information is obtained
  /* if(ret!=0) ::memset(&m_fStatus,0,sizeof(m_fStatus)):*/
}

//-----------------------------------------------------------------------------------

string TFileStatus::getGroup() const {
#ifndef WIN32
  struct group *grp = getgrgid(m_fStatus.st_gid);
  if (grp) return string(grp->gr_name);
#endif
  stringstream buff;
  buff << m_fStatus.st_gid;
  return buff.str();
}

//-----------------------------------------------------------------------------------

string TFileStatus::getUser() const {
#ifndef WIN32
  struct passwd *pw = getpwuid(m_fStatus.st_uid);
  if (pw) return string(pw->pw_name);
#endif
  stringstream buff;
  buff << m_fStatus.st_uid;
  return buff.str();
}

//-----------------------------------------------------------------------------------

long TFileStatus::getSize() const { return (long)m_fStatus.st_size; }

//-----------------------------------------------------------------------------------

TTime TFileStatus::getLastAccessTime() const {
  struct tm *t = localtime(&(m_fStatus.st_atime));
  return TTime(t->tm_year + 1900, t->tm_mon + 1, t->tm_mday, t->tm_hour,
               t->tm_min, t->tm_sec);
}

//-----------------------------------------------------------------------------------

TTime TFileStatus::getLastModificationTime() const {
  struct tm *t = localtime(&(m_fStatus.st_mtime));
  return TTime(t->tm_year + 1900, t->tm_mon + 1, t->tm_mday, t->tm_hour,
               t->tm_min, t->tm_sec);
}

//-----------------------------------------------------------------------------------

TTime TFileStatus::getCreationTime() const {
  struct tm *t = localtime(&(m_fStatus.st_ctime));
  return TTime(t->tm_year + 1900, t->tm_mon + 1, t->tm_mday, t->tm_hour,
               t->tm_min, t->tm_sec);
}

//-----------------------------------------------------------------------------------

unsigned long TFileStatus::getPermissions() const { return m_fStatus.st_mode; }

//-----------------------------------------------------------------------------------

bool TFileStatus::isDirectory() const {
  return doesExist() && (getPermissions() & TFileStatus::IfDir) != 0;
}

//-----------------------------------------------------------------------------------

string TSystem::getHostName() {
  char hostName[MaxHostNameLen];
#ifdef WIN32
  WSADATA wdata;
  int err;
  err = WSAStartup(MAKEWORD(1, 1), &wdata);
  if (!err) strcpy(hostName, "localhost");  // la rete non e' partita  ?!?
#endif
  gethostname((char *)&hostName, MaxHostNameLen);
#ifdef WIN32
  if (err) WSACleanup();
#endif

  return hostName;
}

//------------------------------------------------------------

string TSystem::getUserName() {
#ifdef WIN32
#ifndef UNLEN
#define UNLEN (256)
#endif
  const long bufSize = UNLEN;
#else
  const long bufSize                                 = L_cuserid;
#endif

  char userName[bufSize + 1];

#ifdef WIN32
  GetUserName((char *)&userName, (unsigned long *)(&bufSize));
#else
  cuserid(userName);
  if (userName[0] == 0x00) strcpy(userName, "User Unknown");
#endif
  return string(userName);
}

//------------------------------------------------------------

string TSystem::getGroupName() {
#ifdef WIN32

  return string();

#else
  struct group *currentGroup;
  currentGroup = getgrgid(getgid());
  if (currentGroup)
    return string(currentGroup->gr_name);
  else
    return string();
#endif
}

//------------------------------------------------------------

int TSystem::getProcessId() { return getpid(); }

//------------------------------------------------------------

TFilePath TSystem::getHomeDirectory() {
#ifndef WIN32
  struct passwd *pwd = getpwnam(getUserName().c_str());
  if (!pwd) {
    return TFilePath();
  }
  return TFilePath(pwd->pw_dir);
#else
  char *s = getenv("USERPROFILE");
  if (!s)
    return TFilePath();
  else
    return TFilePath(s);
#endif
}

//------------------------------------------------------------

TFilePath TSystem::getTempDir() {
#ifdef WIN32
  // gestire eccezioni se dw==0
  DWORD dw   = GetTempPath(0, 0);  // non include il terminatore
  char *path = new char[dw + 1];
  GetTempPath(dw, path);
  TFilePath tempDir(path);
  delete[] path;
  return tempDir;
#else
  return TFilePath(tmpnam(0)).getParentDir();
#endif
}

//------------------------------------------------------------

TFilePath TSystem::getTestDir() {
#ifdef WIN32
  return TFilePath("\\\\sirio\\toonz5.0\\TNZCORE_TEST");
#else
  return TFilePath("/ULTRA/toonz5.0/TNZCORE_TEST");
#endif
}

//------------------------------------------------------------

string TSystem::getSystemValue(const TFilePath &name) {
#ifdef WIN32

  string keyName = name.getParentDir().getFullPath();
  string varName = name.getName();

  HKEY hkey;
  if (RegOpenKeyEx(HKEY_LOCAL_MACHINE, keyName.c_str(), 0, KEY_READ, &hkey) !=
      ERROR_SUCCESS) {
    /*key not defined*/
    return "";
  }
  unsigned char buffer[1024];
  unsigned long bufferSize = sizeof(buffer);

  string value;
  if (RegQueryValueEx(hkey, varName.c_str(), NULL, NULL, buffer, &bufferSize) ==
      ERROR_SUCCESS)
    value = string((char *)buffer);

  RegCloseKey(hkey);

  return value;

#else
  char *s = getenv(name.getFullPath().c_str());
  return string(s ? s : "");
#endif
}

//------------------------------------------------------------

TFilePath TSystem::getBinDir() {
#ifdef WIN32
  char buf[MaxPathLen];
  GetModuleFileName(0, buf, MaxPathLen);
  return TFilePath(buf).getParentDir();
#else
  string binroot = TSystem::getSystemValue("BINROOT");
  if (binroot == "") {
    assert(!"BINROOT variable undefined");
  }
  return TFilePath(binroot) + "bin";
#endif
}

//------------------------------------------------------------

TFilePath TSystem::getUniqueFile(string field) {
  char *tempName = TEMPNAM(getTempDir().getFullPath().c_str(), field.c_str());
  if (tempName == 0)
    throw TSystemException(TFilePath(getTempDir().getFullPath() + field), 1);
  TFilePath outPath(tempName);
  free(tempName);
  return outPath;
}

//------------------------------------------------------------

const TDeltaTime TDeltaTime::second = TDeltaTime(1, 0);
const TDeltaTime TDeltaTime::minute = 60 * TDeltaTime::second;
const TDeltaTime TDeltaTime::hour   = 60 * TDeltaTime::minute;
const TDeltaTime TDeltaTime::day    = 24 * TDeltaTime::hour;

//------------------------------------------------------------

TTime TSystem::getCurrentTime() {
#ifdef WIN32
  struct _timeb curTime;
#else
  struct timeb curTime;
#endif
  FTIME(&curTime);
  struct tm *localTime = localtime(&(curTime.time));

  return TTime(localTime->tm_year + 1900, localTime->tm_mon + 1,
               localTime->tm_mday, localTime->tm_hour, localTime->tm_min,
               localTime->tm_sec, curTime.millitm);
}

//------------------------------------------------------------
// gestire exception
void TSystem::mkDir(const TFilePath &path) {
  if (path == "") {
    throw TSystemException("mkdir: Empty argument");
  }

  if (TFileStatus(path).doesExist() || path.isRoot()) return;
  while (!TFileStatus(path.getParentDir()).doesExist() &&
         !path.getParentDir().isRoot()) {
    mkDir(path.getParentDir());
  }
#ifdef WIN32
  int ret = MKDIR(path.getFullPath().c_str());  // ret e' EEXIST o ENOENT
#else
  umask(0);
  mode_t attr = TFileStatus::UserReadable | TFileStatus::UserWritable |
                TFileStatus::UserExecutable | TFileStatus::OwnerReadWriteExec |
                TFileStatus::GroupReadWriteExec |
                TFileStatus::OtherReadWriteExec;
  int ret = MKDIR(path.getFullPath().c_str(), attr);  // ret e` EEXIST o ENOENT
#endif
  if (ret == -1) throw TSystemException(path, errno);
}

//------------------------------------------------------------
// gestire exception
void TSystem::rmDir(const TFilePath &path) {
  //  if (!TFileStatus(path).doesExist())
  //	return;
  int ret = RMDIR(path.getFullPath().c_str());
  if (ret == -1)  // ret e' ENOTEMPTY o ENOENT
    throw TSystemException(path, errno);
}

//------------------------------------------------------------

void TSystem::rmDirTree(const TFilePath &path) {
  TFilePathSet pathSet = readDirectory(path);
  for (TFilePathSet::iterator it = pathSet.begin(); it != pathSet.end(); it++) {
    TFilePath path = *it;
    if (TFileStatus(path).isDirectory())
      rmDirTree(path);
    else
      deleteFile(path);
  }
  rmDir(path);
}

//------------------------------------------------------------

void TSystem::copyDir(const TFilePath &dst, const TFilePath &src) {
  if (!TFileStatus(dst).doesExist()) mkDir(dst);

  TFilePathSet pathSet = readDirectory(src);
  for (TFilePathSet::iterator it = pathSet.begin(); it != pathSet.end(); it++) {
    TFilePath path = *it;
    if (TFileStatus(path).isDirectory())
      copyDir(path.withParentDir(dst), path);
    else
      copyFile(path.withParentDir(dst), path);
  }
}

//------------------------------------------------------------

void TSystem::touchFile(const TFilePath &path) {
  string filename = path.getFullPath();
  if (TFileStatus(path).doesExist()) {
    if (0 != UTIME(filename.c_str(), 0)) throw TSystemException(path, errno);
  } else {
    std::ofstream file(filename.c_str());
    if (!file) {
      throw TSystemException(path, errno);
    }
    file.close();  // altrimenti il compilatore da' un warning:
                   // variabile non utilizzata
  }
}

//------------------------------------------------------------

#ifdef WIN32

namespace {

//------------------------------------------------------------

string getFormattedMessage(DWORD lastError) {
  LPVOID lpMsgBuf;
  FormatMessage(FORMAT_MESSAGE_ALLOCATE_BUFFER | FORMAT_MESSAGE_FROM_SYSTEM |
                    FORMAT_MESSAGE_IGNORE_INSERTS,
                NULL, lastError,
                MAKELANGID(LANG_NEUTRAL, SUBLANG_DEFAULT),  // Default language
                (LPTSTR)&lpMsgBuf, 0, NULL);
  return (char *)lpMsgBuf;
}

//------------------------------------------------------------

}  // namespace
#endif
//------------------------------------------------------------

void TSystem::copyFile(const TFilePath &dst, const TFilePath &src) {
  assert(dst != src);
#ifdef WIN32
  // o e' meglio usare CopyFileEx??
  BOOL res = CopyFile(
      src.getFullPath().c_str(),  // pointer to name of an existing file
      dst.getFullPath().c_str(),  // pointer to filename to copy to
      TRUE);                      // flag for operation if file exists
  if (res == 0) {
    throw TSystemException(src, getFormattedMessage(GetLastError()));
  }
#else
  std::ifstream fpin(src.getFullPath().c_str(), ios::in);
  std::ofstream fpout(dst.getFullPath().c_str(), ios::out);
  if (!fpin || !fpout) throw TSystemException(src, "unable to copy file");
  int c = fpin.get();
  while (!fpin.eof()) {
    fpout.put(c);
    c = fpin.get();
  }
#endif
}

//------------------------------------------------------------

void TSystem::renameFile(const TFilePath &dst, const TFilePath &src) {
  int ret = rename(src.getFullPath().c_str(), dst.getFullPath().c_str());
  if (ret != 0) throw TSystemException(dst, errno);
}

//------------------------------------------------------------

// gestire gli errori con GetLastError?
void TSystem::deleteFile(const TFilePath &dst) {
#ifdef WIN32
  BOOL res = DeleteFile(
      dst.getFullPath().c_str());  // pointer to name of file to delete
  // se fallisce perche' un file e' aperto chiudiamo prima il file?
  // To close an open file, use the CloseHandle function.
  if (res == 0)
    throw TSystemException(dst, getFormattedMessage(GetLastError()));
#else
  int ret = remove(dst.getFullPath().c_str());
  if (ret != 0) throw TSystemException(dst, errno);
#endif
}

//------------------------------------------------------------

void TSystem::moveFileToRecycleBin(const TFilePath &fp) {
#ifdef WIN32
  //
  // from http://msdn.microsoft.com/msdnmag/issues/01/04/c/default.aspx
  //

  // Copy pathname to double-NULL-terminated string.
  //
  char buf[_MAX_PATH + 1];                // allow one more character
  strcpy(buf, fp.getFullPath().c_str());  // copy caller's path name
  buf[strlen(buf) + 1] = 0;               // need two NULLs at end

  SHFILEOPSTRUCT data;
  memset(&data, 0, sizeof(SHFILEOPSTRUCT));
  data.fFlags |= FOF_SILENT;          // don't report progress
  data.fFlags |= FOF_NOERRORUI;       // don't report errors
  data.fFlags |= FOF_NOCONFIRMATION;  // don't confirm delete

  data.wFunc = FO_DELETE;        // REQUIRED: delete operation
  data.pFrom = buf;              // REQUIRED: which file(s)
  data.pTo   = NULL;             // MUST be NULL
  data.fFlags |= FOF_ALLOWUNDO;  // ..send to Recycle Bin
  SHFileOperation(&data);        // do it!
#else
  assert(!"Not implemented yet");
#endif
}

//------------------------------------------------------------

void TSystem::readDirectory(TFilePathSet &dst, const TFilePath &path) {
  if (!TFileStatus(path).isDirectory()) {
    throw TSystemException(path, " is not a directory");
  }
#ifdef WIN32
  WIN32_FIND_DATA findFileData;
  string search = (path + "*").getFullPath();
  HANDLE h      = FindFirstFile(search.c_str(), &findFileData);
  if (h == INVALID_HANDLE_VALUE)
    throw TSystemException(path, getFormattedMessage(GetLastError()));
  do {
    string filename(findFileData.cFileName);
    if (filename == "." || filename == "..") continue;
    TFilePath son = path + filename;
    dst.push_back(son);
  } while (FindNextFile(h, &findFileData));
  FindClose(h);
#else
  DIR *dirp;
  struct direct *directp;
  dirp = opendir(path.getFullPath().c_str());
  if (dirp == 0) throw TSystemException(path, errno);
  while (directp = readdir(dirp)) {
    string filename(directp->d_name);
    if (filename == "." || filename == "..") continue;
    TFilePath son = path + filename;
    dst.push_back(son);
  }
  closedir(dirp);
#endif
}

//------------------------------------------------------------

void TSystem::readDirectory(TFilePathSet &dst, const TFilePathSet &pathSet) {
  for (TFilePathSet::const_iterator it = pathSet.begin(); it != pathSet.end();
       it++)
    readDirectory(dst, *it);
}

//------------------------------------------------------------

TFilePathSet TSystem::readDirectory(const TFilePath &path) {
  TFilePathSet filePathSet;
  readDirectory(filePathSet, path);
  return filePathSet;
}

//------------------------------------------------------------

TFilePathSet TSystem::readDirectory(const TFilePathSet &pathSet) {
  TFilePathSet dst;
  readDirectory(dst, pathSet);
  return dst;
}

//------------------------------------------------------------

void TSystem::readDirectoryTree(TFilePathSet &dst, const TFilePath &path) {
  if (!TFileStatus(path).isDirectory()) {
    throw TSystemException(path, " is not a directory");
  }
#ifdef WIN32
  WIN32_FIND_DATA findFileData;
  string search = (path + "*").getFullPath();
  HANDLE h      = FindFirstFile(search.c_str(), &findFileData);
  if (h == INVALID_HANDLE_VALUE) return;
  do {
    string filename(findFileData.cFileName);
    if (filename == "." || filename == "..") continue;
    TFilePath son = path + findFileData.cFileName;
    if (findFileData.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY)
      readDirectoryTree(dst, son);
    else
      dst.push_back(son);
  } while (FindNextFile(h, &findFileData));
  FindClose(h);
#else
  DIR *dirp;
  struct direct *directp;
  dirp = opendir(path.getFullPath().c_str());
  if (dirp == 0) throw TSystemException(path, errno);
  while (directp = readdir(dirp)) {
    string filename(directp->d_name);
    if (filename == "." || filename == "..") continue;
    TFilePath son = path + filename;
    if (TFileStatus(son).isDirectory())
      readDirectoryTree(dst, son);
    else
      dst.push_back(son);
  }
  closedir(dirp);
#endif
}

//------------------------------------------------------------

void TSystem::readDirectoryTree(TFilePathSet &dst,
                                const TFilePathSet &pathSet) {
  for (TFilePathSet::const_iterator it = pathSet.begin(); it != pathSet.end();
       it++)
    readDirectoryTree(dst, *it);
}

//------------------------------------------------------------

TFilePathSet TSystem::readDirectoryTree(const TFilePath &path) {
  TFilePathSet dst;
  readDirectoryTree(dst, path);
  return dst;
}

//------------------------------------------------------------

TFilePathSet TSystem::readDirectoryTree(const TFilePathSet &pathSet) {
  TFilePathSet dst;
  readDirectoryTree(dst, pathSet);
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

#ifdef WIN32
  DWORD size   = GetLogicalDriveStrings(0, NULL);
  char *buffer = new char[size + 1];
  char *ptr    = buffer;
  DWORD rc     = GetLogicalDriveStrings(size, buffer);
  if (rc == 0) throw TSystemException("", getFormattedMessage(GetLastError()));

  while (*ptr) {
    filePathSet.push_back(ptr);
    while (*ptr) ptr++;
    ptr++;
  }

  delete[] buffer;
  return filePathSet;
#else
  FILE *f = setmntent("/etc/fstab", "r");
  if (f) {
    while (struct mntent *m = getmntent(f)) {
      // cout << "machine "<< m->mnt_fsname << " dir " <<m->mnt_dir << " type "
      // <<  m->mnt_type << endl;
      filePathSet.push_back(m->mnt_dir);
    }
    endmntent(f);
  }

  return filePathSet;
#endif
}

//------------------------------------------------------------

ULONG TSystem::getDiskSize(const TFilePath &diskName) {
  ULONG size = 0;
  if (!diskName.isAbsolute()) {
    assert(0);
    return 0;
  }
#ifndef WIN32
  struct statfs buf;
#ifdef __sgi
  statfs(diskName.getFullPath().c_str(), &buf, sizeof(struct statfs), 0);
#else
  statfs(diskName.getFullPath().c_str(), &buf);
#endif
  size = (ULONG)((buf.f_blocks * buf.f_bsize) >> 10);
#else
  DWORD sectorsPerCluster;     // sectors per cluster
  DWORD bytesPerSector;        // bytes per sector
  DWORD numberOfFreeClusters;  // free clusters
  DWORD totalNumberOfClusters;

  BOOL rc = GetDiskFreeSpace(diskName.getFullPath().c_str(),  // root path
                             &sectorsPerCluster,     // sectors per cluster
                             &bytesPerSector,        // bytes per sector
                             &numberOfFreeClusters,  // free clusters
                             &totalNumberOfClusters  // total clusters
                             );

  if (!rc)
    throw TSystemException(diskName, getFormattedMessage(GetLastError()));
  else
    size = (totalNumberOfClusters * sectorsPerCluster * bytesPerSector) >> 10;
#endif
  return size;
}

//------------------------------------------------------------

ULONG TSystem::getFreeDiskSize(const TFilePath &diskName) {
  ULONG size = 0;
  if (!diskName.isAbsolute()) {
    assert(0);
    return 0;
  }
#ifndef WIN32
  struct statfs buf;
#ifdef __sgi
  statfs(diskName.getFullPath().c_str(), &buf, sizeof(struct statfs), 0);
#else
  statfs(diskName.getFullPath().c_str(), &buf);
#endif
  size = (ULONG)(buf.f_bfree * buf.f_bsize) >> 10;
#else
  DWORD sectorsPerCluster;     // sectors per cluster
  DWORD bytesPerSector;        // bytes per sector
  DWORD numberOfFreeClusters;  // free clusters
  DWORD totalNumberOfClusters;

  BOOL rc = GetDiskFreeSpace(diskName.getFullPath().c_str(),  // root path
                             &sectorsPerCluster,     // sectors per cluster
                             &bytesPerSector,        // bytes per sector
                             &numberOfFreeClusters,  // free clusters
                             &totalNumberOfClusters  // total clusters
                             );

  if (!rc)  // eccezione... getLastError etc...
    throw TSystemException(diskName, getFormattedMessage(GetLastError()));
  else
    size = (numberOfFreeClusters * sectorsPerCluster * bytesPerSector) >> 10;
#endif
  return size;
}

//------------------------------------------------------------

ULONG TSystem::getFreeMemorySize() {
  ULONG totalFree = 0;

#ifdef WIN32
  ULONG virtualFree  = 0;
  ULONG physicalFree = 0;
  MEMORYSTATUS buff;
  GlobalMemoryStatus(&buff);
  physicalFree = buff.dwAvailPhys;
  virtualFree  = buff.dwAvailVirtual;
  totalFree    = (physicalFree + virtualFree) >> 10;
#else
#ifdef __sgi
  // check for virtual memory
  int numberOfResources =
      swapctl(SC_GETNSWP, 0); /* get number of swapping resources configued */

  if (numberOfResources == 0) return 0;

  // avrei voluto fare: struct swaptable *table = new struct swaptable[...]
  struct swaptable *table = (struct swaptable *)calloc(
      1, sizeof(struct swapent) * numberOfResources + sizeof(int));

  table->swt_n = numberOfResources;
  swapctl(SC_LIST, table); /* list all the swapping resources */

  ULONG virtualFree  = 0;
  ULONG physicalFree = 0;
  for (int i = 0; i < table->swt_n; i++) {
    virtualFree += table->swt_ent[i].ste_free;
  }

  free(table);
  totalFree = virtualFree << 4 + physicalFree;
#else
#ifdef LINUX

  struct sysinfo *sysInfo = (struct sysinfo *)calloc(1, sizeof(struct sysinfo));

  if (!sysinfo(sysInfo)) {
    totalFree = sysInfo->freeram + sysInfo->freeswap;
  } else {
    assert(!"sysinfo function failed");
  }
  free(sysInfo);

#else
  @ @ @ERROR : PLATFORM NOT SUPPORTED
#endif
#endif  //__sgi

#endif  // WIN32

#ifndef WIN32
#else
#endif

  return totalFree;
}

//------------------------------------------------------------

ostream &operator<<(ostream &out, const TTime &t) {
  return out << t.getDate() << " " << t.getTime();
}

//------------------------------------------------------------

#ifdef __sgi
extern "C" long sginap(long ticks);
#else
#ifdef LINUX
extern "C" int usleep(unsigned int);
#endif
#endif

void TSystem::sleep(const TDeltaTime &delay) {
  int ms = delay.getSeconds() * 1000;
  assert(ms >= delay.getSeconds());
  ms += delay.getMilliSeconds();
#ifdef WIN32
  Sleep(ms);
#else
#ifdef __sgi
  sginap(ms * CLK_TCK / 1000);
#else
#ifdef LINUX
  ms *= 1000;
  usleep(ms);
#endif
#endif
#endif
}

//--------------------------------------------------------------

TSystemException::TSystemException(const TFilePath &fname, int err)
    : m_fname(fname.getFullPath())
    , m_err(err)
    , m_msg("")

{}

//--------------------------------------------------------------

TSystemException::TSystemException(const TFilePath &fname, const string &msg)
    : m_fname(fname.getFullPath()), m_err(-1), m_msg(msg) {}

//--------------------------------------------------------------

TSystemException::TSystemException(const string &msg)
    : m_fname(""), m_err(-1), m_msg(msg) {}

//--------------------------------------------------------------

string TSystemException::getMessage() const {
  string msg = m_fname;
  switch (m_err) {
  case -1:
    // nothing
    msg += ": ";
    msg += m_msg;
    CASE EEXIST : msg +=
                  ": Directory was not created because filename is the name of "
                  "an existing file, directory, or device";
    CASE ENOENT : msg +=
                  ": Path was not found, or the named file does not exist or "
                  "is a null pathname.";
    CASE ENOTEMPTY : msg +=
                     ": Given path is not a directory; directory is not empty; "
                     "or directory is either current working directory or root "
                     "directory";
    CASE EACCES : msg +=
                  ": Search permission is denied by a component of the path "
                  "prefix, or write permission on the file named by path is "
                  "denied, or times is NULL, and write access is denied";
    CASE EFAULT : msg +=
                  ": Times is not NULL and, or points outside the process's "
                  "allocated address space.";
    CASE EINTR : msg += ": A signal was caught during the utime system call.";
    CASE ENAMETOOLONG : msg +=
                        ": The length of the path argument exceeds {PATH_MAX}, "
                        "or the length of a path component exceeds {NAME_MAX} "
                        "while _POSIX_NO_TRUNC is in effect.";
    CASE ENOTDIR : msg +=
                   ": A component of the path prefix is not a directory.";
    CASE EPERM : msg +=
                 ": The calling process does not have the super-user "
                 "privilege, the effective user ID is not the owner of the "
                 "file, and times is not NULL, or the file system containing "
                 "the file is mounted read-only";
    CASE EROFS : msg +=
                 ": The current file system level range does not envelop the "
                 "level of the file named by path, and the calling process "
                 "does not have the super-user privilege.";
    CASE ENOSYS : msg +=
                  ": When the named file cannot have its time reset.  The file "
                  "is on a file system that doesn't have this operation.";
    CASE EMFILE
        : msg += ": The maximum number of file descriptors are currently open.";
    CASE ENFILE : msg += ": The system file table is full.";
    CASE EBADF : msg +=
                 ": The file descriptor determined by the DIR stream is no "
                 "longer valid.  This result occurs if the DIR stream has been "
                 "closed.";
    CASE EINVAL
        : msg +=
          ": 64-bit and non-64-bit calls were mixed in a sequence of calls.";
  DEFAULT:
    msg += ": Unknown error";

#ifndef WIN32
    CASE ELOOP
        : msg +=
          ": Too many symbolic links were encountered in translating path.";
    CASE EMULTIHOP : msg +=
                     ": Components of path require hopping to multiple remote "
                     "machines and the file system does not allow it.";
    CASE ENOLINK : msg +=
                   ": Path points to a remote machine and the link to that "
                   "machine is no longer active.";
#ifndef LINUX
    CASE EDIRCORRUPTED : msg += ": The directory is corrupted on disk.";
#endif
    CASE EOVERFLOW : msg +=
                     ": One of the inode number values or offset values did "
                     "not fit in 32 bits, and the 64-bit interfaces were not "
                     "used.";
#endif
  }
  return msg;
}

//--------------------------------------------------------------

static const char *TnzLibMainProcName = "TLibMain";
static std::map<string, const TPluginInfo *> PluginTable;
static std::set<TnzLibMainProcType *> PluginMainTable;

//--------------------------------------------------------------

namespace {

#ifdef WIN32
static std::vector<HINSTANCE> PluginInstanceTable;
#else
static std::vector<void *> PluginInstanceTable;
#endif

extern "C" void unloadPlugins() {
#ifdef WIN32
  for (std::vector<HINSTANCE>::iterator it = PluginInstanceTable.begin();
       it != PluginInstanceTable.end(); ++it)
    FreeLibrary(*it);
#else
  for (std::vector<void *>::iterator it = PluginInstanceTable.begin();
       it != PluginInstanceTable.end(); ++it)
    dlclose(*it);
#endif
  PluginInstanceTable.clear();
}
}

//--------------------------------------------------------------

void TSystem::unLoadPlugins() { unloadPlugins(); }

//--------------------------------------------------------------

void TSystem::loadPlugins(const TFilePath &dir) {
  static bool cbReg = false;
  if (!cbReg) {
    cbReg = true;
    atexit(unloadPlugins);
  }

#ifdef WIN32
  string extension = "dll";
#else
  string extension = "so";
#endif

  TFilePathSet dirContent = TSystem::readDirectory(dir);
  if (dirContent.empty()) return;

  for (TFilePathSet::iterator it = dirContent.begin(); it != dirContent.end();
       it++) {
    TFilePath fp = *it;
    if (fp.getType() != extension) continue;

// cout << "Loading..." << fp << endl;
#ifdef WIN32
    HINSTANCE handle = LoadLibrary(fp.getFullPath().c_str());
#else
    void *handle   = dlopen(fp.getFullPath().c_str(), RTLD_LAZY);
#endif

    if (!handle) {
// non riesce a caricare la libreria;
#ifdef WIN32
      DWORD err = GetLastError();
      string s;
      s = "*Error* unable to load " + fp.getFullPath() + ": " +
          getFormattedMessage(err) + '\0';
      TSystem::outputDebug(s);
#else
      cout << "*ERROR* couldn't load " << fp << ":";
      cout << dlerror() << endl;
#endif
    } else {
      PluginInstanceTable.push_back(handle);
// cout << "loaded" << endl;
#ifdef WIN32
      TnzLibMainProcType *tnzLibMain =
          (TnzLibMainProcType *)GetProcAddress(handle, TnzLibMainProcName);
#else
      TnzLibMainProcType *tnzLibMain =
          (TnzLibMainProcType *)dlsym(handle, TnzLibMainProcName);
#endif
      if (!tnzLibMain) {
        // La libreria non esporta TLibMain;
        // per ora niente messaggi di errore
        // cout<< "Unable to load TLibMain" << endl;
        /*
#ifdef WIN32
FreeLibrary(handle);
#else
dlclose(handle);
#endif
*/
      } else {
        std::set<TnzLibMainProcType *>::iterator it;
        it = PluginMainTable.find(tnzLibMain);
        if (it == PluginMainTable.end()) {
          PluginMainTable.insert(tnzLibMain);
          const TPluginInfo *info = tnzLibMain();
          if (info) {
            PluginTable[info->getName()] = info;
          }
        }
      }  // if(tnzLibMain)
    }    // if(handle)
  }      // for
}
//--------------------------------------------------------------

void TSystem::loadStandardPlugins() {
  static bool alreadyDone = false;
  if (alreadyDone) return;
  alreadyDone          = true;
  TFilePath pluginsDir = TSystem::getBinDir() + "plugins";

  // cout << "loading standard plugins ... " << pluginsDir << endl;

  try {
    TSystem::loadPlugins(pluginsDir + "io");
  } catch (const TException &e) {
#ifdef WIN32
    MessageBox(0, e.getMessage().c_str(), "Error loading plugin", MB_OK);
#else
    cout << e.getMessage() << endl;
#endif
  }

  try {
    TSystem::loadPlugins(pluginsDir + "fx");
  } catch (const TException &e) {
#ifdef WIN32
    MessageBox(0, e.getMessage().c_str(), "Error loading plugin", MB_OK);
#else
    cout << e.getMessage() << endl;
#endif
  }

  // cout << "done ... " << endl;
}

//--------------------------------------------------------------

const TPluginInfo *TSystem::getLoadedPluginInfo(string name) {
  std::map<string, const TPluginInfo *>::iterator it;
  it = PluginTable.find(name);
  if (it == PluginTable.end())
    return 0;
  else
    return it->second;
}

//--------------------------------------------------------------

void TSystem::showDocument(const TFilePath &path) {
#ifdef WIN32
  int ret = (int)ShellExecute(0, "open", path.getFullPath().c_str(), 0, 0,
                              SW_SHOWNORMAL);
  if (ret <= 32) {
    throw TException(path.getFullPath() + " : can't open");
  }
#else
  string cmd = "mediaplayer ";
  cmd        = cmd + path.getFullPath();
  system(cmd.c_str());
#endif
}

//--------------------------------------------------------------

int TSystem::getProcessorCount() {
#ifdef WIN32
  SYSTEM_INFO sysInfo;
  GetSystemInfo(&sysInfo);
  return sysInfo.dwNumberOfProcessors;
#else
#ifdef __sgi
  return sysconf(_SC_NPROC_CONF);
#else
  return sysconf(_SC_NPROCESSORS_CONF);
#endif
#endif
}

//--------------------------------------------------------------

void TSystem::outputDebug(string s) {
#ifdef WIN32
  OutputDebugString(s.c_str());
#else
  cerr << s << endl;
#endif
}
