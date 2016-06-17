

#ifdef MACOSX
#ifndef __LP64__

// Toonz stuff
#include "tvectorimage.h"
#include "tstroke.h"
#include "traster.h"
#include "tfont.h"

// Qt stuff
#include <QString>
#include <QSharedMemory>
#include <QDebug>

// tipc includes
#include "tipc.h"
#include "tipcmsg.h"
#include "tipcsrv.h"

#include "t32fontmsg.h"

//---------------------------------------------------

//  Local namespace stuff

namespace {}

QDataStream &operator<<(QDataStream &ds, const TThickPoint &p) {
  ds << p.x << p.y << p.thick;
  return ds;
}

//---------------------------------------------------

using namespace tipc;

namespace font_io {

void addParsers(tipc::Server *srv) {
  srv->addParser(new LoadFontNamesParser);
  srv->addParser(new GetAllFamiliesParser);
  srv->addParser(new GetAllTypefacesParser);
  srv->addParser(new SetFamilyParser);
  srv->addParser(new SetTypefaceParser);
  srv->addParser(new SetSizeParser);
  srv->addParser(new GetCurrentFamilyParser);
  srv->addParser(new GetCurrentTypefaceParser);
  srv->addParser(new GetDistanceParser);
  srv->addParser(new DrawCharVIParser);
  srv->addParser(new DrawCharGRParser);
  srv->addParser(new DrawCharCMParser);
}

//************************************************************************
//    LoadFontNames Parser
//************************************************************************

void LoadFontNamesParser::operator()(Message &msg) {
  TFontManager::instance()->loadFontNames();
  msg << clr << QString("ok");
}

//************************************************************************
//    GetAllFamilies Parser
//************************************************************************

void GetAllFamiliesParser::operator()(Message &msg) {
  std::vector<std::wstring> families;
  TFontManager::instance()->getAllFamilies(families);

  msg << clr << QString("ok") << families;
}

//************************************************************************
//    GetAllTypefaces Parser
//************************************************************************

void GetAllTypefacesParser::operator()(Message &msg) {
  std::vector<std::wstring> typefaces;
  TFontManager::instance()->getAllTypefaces(typefaces);

  msg << clr << QString("ok") << typefaces;
}

//************************************************************************
//    SetFamily Parser
//************************************************************************

void SetFamilyParser::operator()(Message &msg) {
  std::wstring family;
  msg >> family >> clr;

  TFontManager::instance()->setFamily(family);
  msg << QString("ok");
}

//************************************************************************
//    SetTypeface Parser
//************************************************************************

void SetTypefaceParser::operator()(Message &msg) {
  std::wstring typeface;
  msg >> typeface >> clr;

  TFontManager *fm = TFontManager::instance();
  fm->setTypeface(typeface);
  msg << QString("ok") << fm->getLineAscender() << fm->getLineDescender();
}

//************************************************************************
//    SetSize Parser
//************************************************************************

void SetSizeParser::operator()(Message &msg) {
  int size;
  msg >> size >> clr;

  TFontManager *fm = TFontManager::instance();
  fm->setSize(size);
  msg << QString("ok") << fm->getLineAscender() << fm->getLineDescender();
}

//************************************************************************
//    GetCurrentFamily Parser
//************************************************************************

void GetCurrentFamilyParser::operator()(Message &msg) {
  std::wstring family(TFontManager::instance()->getCurrentFamily());
  msg << clr << QString("ok") << family;
}

//************************************************************************
//    GetCurrentTypeface Parser
//************************************************************************

void GetCurrentTypefaceParser::operator()(Message &msg) {
  std::wstring typeface(TFontManager::instance()->getCurrentTypeface());
  msg << clr << QString("ok") << typeface;
}

//************************************************************************
//    GetDistance Parser
//************************************************************************

void GetDistanceParser::operator()(Message &msg) {
  wchar_t firstChar, secondChar;
  msg >> firstChar >> secondChar >> clr;

  TPoint dist = TFontManager::instance()->getDistance(firstChar, secondChar);
  msg << QString("ok") << dist.x << dist.y;
}

//************************************************************************
//    DrawCharVI Parser
//************************************************************************

void DrawCharVIParser::operator()(Message &msg) {
  wchar_t charCode, nextCode;
  msg >> charCode >> nextCode >> clr;

  TVectorImageP vi(new TVectorImage);

  TPoint p = TFontManager::instance()->drawChar(vi, charCode, nextCode);
  msg << QString("ok") << p.x << p.y;

  // Write the vector image
  std::vector<TThickPoint> cps;

  unsigned int i, strokesCount = vi->getStrokeCount();
  for (i = 0; i < strokesCount; ++i) {
    TStroke *stroke = vi->getStroke(i);

    cps.clear();
    stroke->getControlPoints(cps);

    msg << cps;
  }
}

//************************************************************************
//    DrawCharGR Parser
//************************************************************************

void DrawCharGRParser::operator()(Message &msg) {
  int ink;
  QString shMemId;
  wchar_t charCode, nextCode;
  msg >> ink >> shMemId >> charCode >> nextCode >> clr;

  // Build the character
  TRasterGR8P charRas;
  TPoint unused;
  TPoint p =
      TFontManager::instance()->drawChar(charRas, unused, charCode, nextCode);

  // Retrieve the raster size
  int lx = charRas->getLx(), ly = charRas->getLy();
  int size = lx * ly * sizeof(TPixelGR8);

  // Request a shared memory segment of that size
  {
    tipc::DefaultMessageParser<SHMEM_REQUEST> msgParser;
    Message shMsg;

    shMsg << shMemId << size << reset;
    msgParser(shMsg);

    QString str;
    shMsg >> reset >> str;
    if (str != QString("ok")) {
      msg << QString("err");
      return;
    }
  }

  // Copy charRas to the shared segment
  QSharedMemory shmem(shMemId);
  shmem.attach();
  shmem.lock();

  memcpy(shmem.data(), charRas->getRawData(), size);

  shmem.unlock();
  shmem.detach();

  msg << QString("ok") << lx << ly << p.x << p.y;
}

//************************************************************************
//    DrawCharCM Parser
//************************************************************************

void DrawCharCMParser::operator()(Message &msg) {
  int ink;
  QString shMemId;
  wchar_t charCode, nextCode;
  msg >> ink >> shMemId >> charCode >> nextCode >> clr;

  // Build the character
  TRasterCM32P charRas;
  TPoint unused;
  TPoint p = TFontManager::instance()->drawChar(charRas, unused, ink, charCode,
                                                nextCode);

  // Retrieve the raster size
  int lx = charRas->getLx(), ly = charRas->getLy();
  int size = lx * ly * sizeof(TPixelCM32);

  // Request a shared memory segment of that size
  {
    tipc::DefaultMessageParser<SHMEM_REQUEST> msgParser;
    Message shMsg;

    shMsg << shMemId << size << reset;
    msgParser(shMsg);

    QString str;
    shMsg >> reset >> str;
    if (str != QString("ok")) {
      msg << QString("err");
      return;
    }
  }

  // Copy charRas to the shared segment
  QSharedMemory shmem(shMemId);
  shmem.attach();
  shmem.lock();

  memcpy(shmem.data(), charRas->getRawData(), size);

  shmem.unlock();
  shmem.detach();

  msg << QString("ok") << lx << ly << p.x << p.y;
}

}  // namespace font_io

#endif  // !__LP64
#endif  // MACOSX
