#pragma once

#ifndef TNONGRAPHICTEST_INCLUDED
#define TNONGRAPHICTEST_INCLUDED

#include "tcommon.h"
#include "tlevel_io.h"
#include "timage_io.h"
#include "tvectorimage.h"

#undef DVAPI
#ifdef TTEST_EXPORTS
#define DVAPI DV_EXPORT_API
#else
#define DVAPI DV_IMPORT_API
#endif

class DVAPI TTest {
  int m_levelInstanceCount;
  int m_imageInstanceCount;
  int m_rasterInstanceCount;
  int m_imageReaderInstanceCount;
  int m_imageWriterInstanceCount;
  int m_levelReaderInstanceCount;
  int m_levelWriterInstanceCount;
  int m_paramInstanceCount;
  int m_fxInstanceCount;

public:
  TTest(const std::string &testName);
  virtual ~TTest();

  void setInstanceCount();
  void verifyInstanceCount();

  virtual void test() = 0;
  virtual void before() { setInstanceCount(); };
  virtual void after(){};

  static void runTests(std::string filename);
};

// Utility

DVAPI TFilePath getTestFile(std::string name);

DVAPI int areEqual(TRasterP ra, TRasterP rb, double err = 1e-8);
DVAPI int areEqual(TVectorImageP va, TVectorImageP vb, double err = 1e-8);
DVAPI int areEqual(TImageP a, TImageP b, double err = 1e-8);
DVAPI bool areEqual(const TPalette *paletteA, const TPalette *paletteB);
DVAPI bool areEqual(TLevelP la, TLevelP lb);

#endif
