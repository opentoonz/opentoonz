#pragma once

#ifndef OUTPUT_PROPERTIES_INCLUDED
#define OUTPUT_PROPERTIES_INCLUDED

#include "tfilepath.h"

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
class TPropertyGroup;
class TWidget;
class TRenderSettings;
class BoardSettings;

//=============================================================================
//! The TOutputProperties class provides a container for output properties and
//! gives all methods to access to these informations.
/*!The class contains all features necessaries to compute output and provides a
   collection of functions that return the various feature, and enable
   manipulation of these.

   A path getPath() is used to manipulate output name and directory, path can be
   changed using the setPath().

   The getRange() function tells whether is set a range in output, if it's
   so provides first frame, last frame, and step; this element can be changed
   using setRange().

   The getFrameRate() function return output frame rate, it can be set by
   setFrameRate().

   The getWhichLevels() function return a value is used to characterize which
   levels are in output:

   \li If value is TOutputProperties::AllLevels all levels are coputed in
   output.
   \li If value is TOutputProperties::SelectedOnly only selectecd levels are
   coputed in output.
   \li If value is TOutputProperties::AnimatedOnly only animated levels are
   coputed in output.

   It can be set by setWhichLevels().

   The getFileFormatProperties(string ext) function return file of fomat \b ext
   properties \b TPropertyGroup. It's possibile set different format and have
   appropriete properties.

   The getRenderSettings() function return render settings \b TRenderSettings,
   it can be change using setRenderSettings().
*/
//=============================================================================

class DVAPI TOutputProperties {
public:
  /*!
This enum type is used to characterize which levels are in output.
Can set in output all lavel, only selectecd levels or only animated levels.
\sa getWhichLevels(), setWhichLevels()
*/
  enum { AllLevels, SelectedOnly, AnimatedOnly };

  enum MaxTileSizeValues { LargeVal = 50, MediumVal = 10, SmallVal = 2 };

private:
  TFilePath m_path;

  std::map<std::string, TPropertyGroup *>
      m_formatProperties;  //!< [\p owned] Format properties by file extension.

  TRenderSettings *m_renderSettings;

  double m_frameRate;

  int m_from, m_to;
  int m_whichLevels;
  int m_offset, m_step;

  int m_multimediaRendering;

  int m_maxTileSizeIndex;
  int m_threadIndex;

  bool m_subcameraPreview;

  BoardSettings *m_boardSettings;

public:
  /*!
Constructs TOutputProperties with default value.
*/
  TOutputProperties();
  /*!
Destroys the TOutputProperties object.
*/
  ~TOutputProperties();

  /*!
Constructs a TOutputProperties object that is a copy of the TOutputProperties
object \a src.
\sa operator=()
*/
  TOutputProperties(const TOutputProperties &src);

  /*!
Assign the \a src object to this TOutputProperties object.
*/
  TOutputProperties &operator=(const TOutputProperties &src);

  /*!
Return output path, name and directory where output file will be save.
\sa setPath()
*/
  TFilePath getPath() const;
  /*!
Set output path to \b fp.
\sa getPath()
*/
  void setPath(const TFilePath &fp);
  /*!
Set which levels are in output to \b state. State can be:

\li TOutputProperties::AllLevels.
\li TOutputProperties::SelectedOnly.
\li TOutputProperties::AnimatedOnly.

\sa getWhichLevels()
*/
  void setWhichLevels(int state) { m_whichLevels = state; }
  /*!
Return which levels are in output.
\sa setWhichLevels()
*/
  int getWhichLevels() const { return m_whichLevels; }

  /*!
Return output offset.
*/
  int getOffset() const { return m_offset; }
  /*!
Set output offset to \b off.
*/
  void setOffset(int off);

  /*!
Return true if frame start <= than frame end. Set \b step
to output step, \b r0 to first frame, \b r1 to last frame.
\sa setRange()
*/
  bool getRange(int &r0, int &r1, int &step) const;
  /*!
Set first frame to \b r0, last frame to \b r1, step to \b step.
\sa getRange()
*/
  void setRange(int r0, int r1, int step);

  /*!
Set output frame rate.
\sa getFrameRate()
*/
  void setFrameRate(double);
  /*!
Return output frame rate.
\sa setFrameRate()
*/
  double getFrameRate() const { return m_frameRate; }

  /*!
Return const \b TRenderSettings.
\sa setRenderSettings()
*/
  const TRenderSettings &getRenderSettings() const { return *m_renderSettings; }
  /*!
Set render settings to \b renderSettings.
\sa getRenderSettings()
*/
  void setRenderSettings(const TRenderSettings &renderSettings);

  /*!
Return \b TPropertyGroup, file format \b ext (Extension) properties.
If extension there isn't is created.
*/
  TPropertyGroup *getFileFormatProperties(std::string ext);

  /*!
Insert in \b v all extension in format properties of output settings.
*/
  void getFileFormatPropertiesExtensions(std::vector<std::string> &v) const;

  //! Sets the rendering behaviour to 'Multimedia'.
  void setMultimediaRendering(int mode) { m_multimediaRendering = mode; }
  int getMultimediaRendering() const { return m_multimediaRendering; }

  /*! Sets the granularity of raster allocations for rendering processes.
The specified value refers to an index associated with const values,
spanning from 0 (no bound, ie giant rasters are allowed) to 3 (highly
restrictive, only small rasters are allocated). The value should be
high for complex scenes.
*/
  void setMaxTileSizeIndex(int idx) { m_maxTileSizeIndex = idx; }
  int getMaxTileSizeIndex() const { return m_maxTileSizeIndex; }

  /*! Sets index for a combo selection of threads running for rendering
processes.
Possible values are: 0 (1 thread, safe mode), 1 (half), 2 (max - number of
machine's CPU).
*/
  void setThreadIndex(int idx) { m_threadIndex = idx; }
  int getThreadIndex() const { return m_threadIndex; }

  bool isSubcameraPreview() const { return m_subcameraPreview; }
  void setSubcameraPreview(bool enabled) { m_subcameraPreview = enabled; }

  BoardSettings *getBoardSettings() const { return m_boardSettings; }
};

//--------------------------------------------

#endif
