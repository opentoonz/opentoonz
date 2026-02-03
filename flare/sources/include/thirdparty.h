#pragma once

#ifndef THIRDPARTY_INCLUDED
#define THIRDPARTY_INCLUDED

#include "tcommon.h"

#include <QProcess>
#include <QString>
#include <QStringList>

#undef DVAPI
#ifdef TOONZLIB_EXPORTS
#define DVAPI DV_EXPORT_API
#else
#define DVAPI DV_IMPORT_API
#endif

namespace ThirdParty {

//-----------------------------------------------------------------------------

DVAPI void initialize();

//-----------------------------------------------------------------------------

DVAPI void getFFmpegVideoSupported(QStringList &exts);
DVAPI void getFFmpegAudioSupported(QStringList &exts);

DVAPI bool findFFmpeg(QString dir);
DVAPI bool checkFFmpeg();
DVAPI QString autodetectFFmpeg();

DVAPI QString getFFmpegDir();
DVAPI void setFFmpegDir(const QString &dir);
DVAPI int getFFmpegTimeout();
DVAPI void setFFmpegTimeout(int secs);

DVAPI void runFFmpeg(QProcess &process, const QStringList &arguments);
DVAPI void runFFprobe(QProcess &process, const QStringList &arguments);

DVAPI void runFFmpegAudio(QProcess &process, QString srcPath, QString dstPath,
                          int samplerate = 44100, int bpp = 16,
                          int channels = 2);
DVAPI bool readFFmpegAudio(QProcess &process, QByteArray &rawData);

//-----------------------------------------------------------------------------

DVAPI bool findRhubarb(QString dir);
DVAPI bool checkRhubarb();
DVAPI QString autodetectRhubarb();

DVAPI QString getRhubarbDir();
DVAPI void setRhubarbDir(const QString &dir);
DVAPI int getRhubarbTimeout();
DVAPI void setRhubarbTimeout(int secs);

DVAPI void runRhubarb(QProcess &process, const QStringList &arguments);

//-----------------------------------------------------------------------------

// return  0 = No error
// return -1 = error code
// return -2 = timed out
DVAPI int waitAsyncProcess(const QProcess &process, int timeout);

//-----------------------------------------------------------------------------

}  // namespace ThirdParty

#endif
