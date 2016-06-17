#pragma once

#ifndef CAPTUREPARAMETERS_H
#define CAPTUREPARAMETERS_H

#include "tfilepath.h"
#include "tgeometry.h"

#undef DVAPI
#undef DVVAR
#ifdef TOONZLIB_EXPORTS
#define DVAPI DV_EXPORT_API
#define DVVAR DV_EXPORT_VAR
#else
#define DVAPI DV_IMPORT_API
#define DVVAR DV_IMPORT_VAR
#endif

//=============================================================================
// forward declarations
class TIStream;
class TOStream;
class TPropertyGroup;

class DVAPI CaptureParameters {
  std::wstring m_deviceName;
  TDimension m_resolution;
  int m_brightness;
  int m_contranst;
  bool m_useWhiteImage;
  bool m_upsideDown;
  int m_increment;
  int m_step;

  TFilePath m_filePath;
  std::string m_format;
  std::map<std::string, TPropertyGroup *> m_formatProperties;

public:
  CaptureParameters();
  ~CaptureParameters() {}

  std::wstring getDeviceName() const { return m_deviceName; }
  void setDeviceName(std::wstring name) { m_deviceName = name; }

  TDimension getResolution() const { return m_resolution; }
  void setResolution(TDimension res) { m_resolution = res; }

  int getBrightness() const { return m_brightness; }
  void setBrightness(int value) { m_brightness = value; }

  int getContranst() const { return m_contranst; }
  void setContranst(int value) { m_contranst = value; }

  bool isUseWhiteImage() const { return m_useWhiteImage; }
  void useWhiteImage(bool value) { m_useWhiteImage = value; }

  bool isUpsideDown() const { return m_upsideDown; }
  void upsideDown(bool value) { m_upsideDown = value; }

  TFilePath getFilePath() const { return m_filePath; }
  void setFilePath(TFilePath value) { m_filePath = value; }

  int getIncrement() const { return m_increment; }
  void setIncrement(int value) { m_increment = value; }

  int getStep() const { return m_step; }
  void setStep(int value) { m_step = value; }

  std::string getFileFormat() const { return m_format; }
  void setFileFormat(std::string value) { m_format = value; }

  TPropertyGroup *getFileFormatProperties(std::string ext);

  void assign(const CaptureParameters *params);
  void saveData(TOStream &os);
  void loadData(TIStream &is);

protected:
  void getFileFormatPropertiesExtensions(std::vector<std::string> &v) const;
};
#endif  // CAPTUREPARAMETERS_H
