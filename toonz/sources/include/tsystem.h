#pragma once

#ifndef TSYSTEM_INCLUDED
#define TSYSTEM_INCLUDED

//#include "texception.h"

#ifndef _WIN32
#include <sys/stat.h>
#endif

#undef DVAPI
#undef DVVAR
#ifdef TSYSTEM_EXPORTS
#define DVAPI DV_EXPORT_API
#define DVVAR DV_EXPORT_VAR
#else
#define DVAPI DV_IMPORT_API
#define DVVAR DV_IMPORT_VAR
#endif

//=============================================================================

#include "tfilepath.h"

// tutte le info ritornate da stat e access
// piu' info 'toonzesche' : cos'e': directory, livello, immagine, testo, ecc.

#ifndef TNZCORE_LIGHT

#include <QDateTime>

DVAPI std::ostream &operator<<(std::ostream &out, const QDateTime &t);

#include <QFileInfo>
class DVAPI TFileStatus {
  bool m_exist;
  QFileInfo m_fileInfo;

public:
  TFileStatus(const TFilePath &path);
  QString getGroup() const;
  QString getUser() const;
  TINT64 getSize() const;
  QDateTime getLastAccessTime() const;
  QDateTime getLastModificationTime() const;
  QDateTime getCreationTime() const;
  bool isDirectory() const;
  bool isLink() const;
  QFile::Permissions getPermissions() const;

  bool doesExist() const { return m_exist; }
  bool isWritable() const { return (getPermissions() & QFile::WriteUser) != 0; }
  bool isReadable() const {
    return m_exist && (getPermissions() & QFile::ReadUser) != 0;
  }

  bool isWritableDir() const {
    return doesExist() && isDirectory() && isWritable();
  };
};

#endif

class DVAPI TSystemException final : public TException {
  TFilePath m_fname;
  int m_err;
  TString m_msg;

public:
  TSystemException() {}
  TSystemException(const TFilePath &, int);
  TSystemException(const TFilePath &, const std::string &);
  TSystemException(const TFilePath &, const std::wstring &);
  TSystemException(const std::string &);
  TSystemException(const std::wstring &msg);
  ~TSystemException() {}
  TString getMessage() const override;
};

// DVAPI ostream&operator<<(ostream&out, const TSystemException &e);

namespace TSystem {
// cambiare i nomi. Niente T (c'e' gia' in TSystem). Usiamo la convenzione
// solita per
// i nomi composti (maiuscole e non '_'). Facciamo tutto minuscolo con
// l'iniziale maiuscola
// es. MaxPathLen;

extern const int MaxPathLen;      // Maximum length of full path
extern const int MaxFNameLen;     // Maximum length of filename component
extern const int MaxHostNameLen;  // Maximum length of the host name  ==64

DVAPI bool doHaveMainLoop();
DVAPI void hasMainLoop(bool state);
// DVAPI void enableFrameGrouping(bool on);
// DVAPI bool isFrameGroupingEnabled();

// aggiungono (in coda) a set
DVAPI void readDirectory(TFilePathSet &fpset, const QDir &dir,
                         bool groupFrames = true);

DVAPI void readDirectory(TFilePathSet &fpset, const TFilePath &path,
                         bool groupFrames = true, bool onlyFiles = false,
                         bool getHiddenFiles = false);
DVAPI void readDirectory(TFilePathSet &fpset, const TFilePathSet &pathSet,
                         bool groupFrames = true, bool onlyFiles = false,
                         bool getHiddenFiles = false);
DVAPI void readDirectoryTree(TFilePathSet &fpset, const TFilePath &path,
                             bool groupFrames = true, bool onlyFiles = false);
DVAPI void readDirectoryTree(TFilePathSet &fpset, const TFilePathSet &pathSet,
                             bool groupFrames = true, bool onlyFiles = false);

// to retrieve the both lists with groupFrames option = on and off.
DVAPI void readDirectory(TFilePathSet &groupFpSet, TFilePathSet &allFpSet,
                         const TFilePath &path);

// return the folder path list which is readable and executable
DVAPI void readDirectory_Dir_ReadExe(TFilePathSet &dst, const TFilePath &path);

// return the folder item list which is readable and executable (only names)
DVAPI void readDirectory_DirItems(QStringList &dst, const TFilePath &path);

// creano un set nuovo
DVAPI TFilePathSet readDirectory(const TFilePath &path, bool groupFrames = true,
                                 bool onlyFiles      = false,
                                 bool getHiddenFiles = false);
DVAPI TFilePathSet readDirectory(const TFilePathSet &pathSet,
                                 bool groupFrames    = true,
                                 bool onlyFiles      = false,
                                 bool getHiddenFiles = false);
DVAPI TFilePathSet readDirectoryTree(const TFilePath &path,
                                     bool groupFrames = true,
                                     bool onlyFiles   = false);
DVAPI TFilePathSet readDirectoryTree(const TFilePathSet &pathSet,
                                     bool groupFrames = true,
                                     bool onlyFiles   = false);

//
DVAPI TFilePathSet packLevelNames(const TFilePathSet &);

DVAPI TFilePath getHomeDirectory();
DVAPI TFilePath getTempDir();
DVAPI TFilePath getTestDir(std::string name = "verify_tnzcore");
DVAPI TFilePath getBinDir();
DVAPI TFilePath getDllDir();

/*!Returns the process id of the current process*/
DVAPI int getProcessId();

// n.b. queste possono buttare eccezioni!
// mkdir crea anche il parent se non esiste
DVAPI void mkDir(const TFilePath &path);
DVAPI void rmDir(const TFilePath &path);
DVAPI void rmDirTree(const TFilePath &path);
DVAPI void copyDir(const TFilePath &dst, const TFilePath &src);
DVAPI bool touchParentDir(const TFilePath &fp);

DVAPI void touchFile(const TFilePath &path);
DVAPI void copyFile(const TFilePath &dst, const TFilePath &src,
                    bool overwrite = true);
DVAPI void renameFile(const TFilePath &dst, const TFilePath &src,
                      bool overwrite = true);
DVAPI void deleteFile(const TFilePath &dst);
DVAPI void hideFile(const TFilePath &dst);
DVAPI void moveFileToRecycleBin(const TFilePath &fp);

DVAPI void copyFileOrLevel_throw(const TFilePath &dst, const TFilePath &src);
DVAPI void renameFileOrLevel_throw(const TFilePath &dst, const TFilePath &src,
                                   bool renamePalette = false);
DVAPI void removeFileOrLevel_throw(const TFilePath &fp);
DVAPI void hideFileOrLevel_throw(const TFilePath &fp);
DVAPI void moveFileOrLevelToRecycleBin_throw(const TFilePath &fp);

DVAPI bool doesExistFileOrLevel(const TFilePath &fp);
DVAPI bool copyFileOrLevel(const TFilePath &dst, const TFilePath &src);
DVAPI bool renameFileOrLevel(const TFilePath &dst, const TFilePath &src,
                             bool renamePalette = false);
DVAPI bool removeFileOrLevel(const TFilePath &fp);
DVAPI bool hideFileOrLevel(const TFilePath &fp);
DVAPI bool moveFileOrLevelToRecycleBin(const TFilePath &fp);

DVAPI void sleep(TINT64 delay);
inline void sleep(int ms) { sleep((TINT64)ms); }

DVAPI TFilePathSet getDisks();

/*! returns disk size in Kbytes */
DVAPI TINT64 getDiskSize(const TFilePath &);

/* returns free disk size in Kbytes */
DVAPI TINT64 getFreeDiskSize(const TFilePath &);

/*! returns available physical (+ virtual mem if boolean=true)  memory in KBytes
 */
DVAPI TINT64 getFreeMemorySize(bool onlyPhisicalMemory);

/*! return total physical (+ virtual mem if boolean=true) memory in kbytes */
DVAPI TINT64 getMemorySize(bool onlyPhisicalMemory);

/*! return true if not enough memory. It can happen for 2 reasons:
      1) free phisical memory is close to 0;
      2) the calling process has allocated the maximum amount of memory  allowed
   for a single process(tipically, for a 32 bits machine, 2GB)*/

DVAPI bool memoryShortage();

/*! run di un extern viewer */
// DVAPI void showDocument(const TFilePath &dst);

DVAPI int getProcessorCount();

enum CPUExtensions {
  CPUExtensionsNone = 0x00000000L,
  // CpuSupportsCpuId      = 0x00000001L,
  // CpuSupportsFpu        = 0x00000002L,
  // CpuSupportsMmx        = 0x00000004L,
  // CpuSupportsIntegerSse = 0x00000008L,
  CpuSupportsSse  = 0x00000010L,
  CpuSupportsSse2 = 0x00000020L,
  // CpuSupports3DNow      = 0x00000040L,
  // CpuSupports3DNowExt   = 0x00000080L
};

/*! returns a bit mask containing the CPU extensions supported */
DVAPI long getCPUExtensions();

/*! enables/disables the CPU extensions, if available*/
// DVAPI void enableCPUExtensions(bool on);

// cosette da fare:

// un po' di studio sui file memory mapped

// indagare
DVAPI std::iostream openTemporaryFile();
// un'altra idea e' avere un TQualcosaP che ritorna un UniqueFileName
// e che sul distruttore cancella il file

// pensare a:
//  DVAPI void setFileProtection(const TFilePath &path, ???);

void DVAPI outputDebug(std::string s);

DVAPI bool isUNC(const TFilePath &fp);

//! Returns the filepath in UNC format
// ex.: TFilePath("O:\\temp").toUNC() == TFilePath("\\\\vega\\PERSONALI\\temp")
// if "\\\\vega\\PERSONALI\\" is mounted as "O:\\" on the local host
// A TException is thrown if the the conversion is not possible

DVAPI TFilePath toUNC(const TFilePath &fp);

/*!Returns the filepath in a format that refers to the local host
            ex.: TFilePath("\\\\dell530\\discoD\\temp").toLocalPath() ==
   TFilePath("C:\\temp")
      if the local host is "dell530"
     No conversion is done if the filepath already refers to the local host
     A TException is thrown if the the conversion is not possible
   */
DVAPI TFilePath toLocalPath(const TFilePath &fp);

DVAPI bool showDocument(const TFilePath &fp);

#ifndef TNZCORE_LIGHT
DVAPI QDateTime getFileTime(const TFilePath &path);
DVAPI QString getHostName();
DVAPI QString getUserName();
DVAPI QString getSystemValue(const TFilePath &name);
DVAPI TFilePath getUniqueFile(QString field = "");
#endif
}

//
// Esempio di lettura di una directory:
// TFilePathSet dirContent = TSystem::readDirectory("C:\\temp");
// for(TFilePathSet::Iterator it = dirContent.begin(); it != dirContent.end();
// it++)
//    {
//     TFilePath fp = *it;
//     cout << fp << endl;
//    }
//

#endif
