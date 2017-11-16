#pragma once

#ifndef SCENE_PROPERTIES_INCLUDED
#define SCENE_PROPERTIES_INCLUDED

#include "tfilepath.h"
#include "tpixel.h"
#include <QList>

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
class TWriterInfo;
class TPropertyGroup;
class TPalette;
class TSceneProperties;
class CleanupParameters;
class TScannerParameters;
class TCamera;
class TOutputProperties;
class TStageObjectTree;
class VectorizerParameters;
class CaptureParameters;

//=============================================================================
// TSceneProperties
//* This class manages the scene's attributes.
/*!
                The scene is part of the Xsheet and is composed by frames.
                In general is composed by several levels of Xsheets.
         \n	It is the general area where the movie takes place.
                Its features are:
        \li the frame rate that is the movie's speed,
        \li background color of the camera and viewer,  chessboard colors
   (background in the preview without the background color),
        guides etc...
        */
class DVAPI TSceneProperties {
public:
  typedef std::vector<double> Guides;

private:
  Guides m_hGuides, m_vGuides;

  // N.B. questo vettore serve solo durante l'I/O
  // normalmente le camere stanno dentro TStageObjectTree
  std::vector<TCamera *> m_cameras;

  TOutputProperties *m_outputProp, *m_previewProp;

  CleanupParameters *m_cleanupParameters;
  TScannerParameters *m_scanParameters;
  VectorizerParameters *m_vectorizerParameters;
  CaptureParameters *m_captureParameters;

  TPixel32 m_bgColor;

  int m_markerDistance, m_markerOffset;
  int m_fullcolorSubsampling, m_tlvSubsampling;

  int m_fieldGuideSize;
  double m_fieldGuideAspectRatio;

  //! Xsheet Note Color, color number = 7.
  QList<TPixel32> m_notesColor;

  bool m_columnColorFilterOnRender;
  TFilePath m_camCapSaveInPath;

public:
  /*!
          The constructor creates:
  \li a new cleanup TPalette object;
  \li	a new CleanupParameters object that sets to default some basic
attributes
  as basic transformation, rotation angle, scale factor, x-y offsets etc...;
  \li a new TScannerParameters object (parameters to manage the scanner);
\li a new TOutputProperties object for the output and one for the preview ;

  Sets internal attributes to default as background color, guides size and
ratio, level subsampling, etc....
  */
  TSceneProperties();
  /*!
          Deletes object created in the constructor
          and the pointers to the vector of cameras.
  \sa TSceneProperties.
  */
  ~TSceneProperties();
  /*!
          assign attributes of sprop to \e this and assign memory (makes new)
     for the cameras.
  */
  void assign(const TSceneProperties *sprop);
  /*!
          It is called on new scene creation as it updates scanner parameters to
     current scanner.
  */
  void onInitialize();
  /*!
          Returns horizontal Guides; a vector of double values that shows
     y-value of
          horizontal lines.
  */
  Guides &getHGuides() { return m_hGuides; }
  /*!
          Returns vertical Guides; a vector of double values that shows x-value
     of
          vertical lines.
  */
  Guides &getVGuides() { return m_vGuides; }
  /*!
          Returns a vector of pointer to \b TCamera with all camera in scene.
  */
  std::vector<TCamera *> &getCameras() { return m_cameras; }
  /*!
          Returns a constant vector of pointer to \b TCamera with all camera in
     scene.
  */
  const std::vector<TCamera *> &getCameras() const { return m_cameras; }
  /*!
          Returns a \b TOutputProperties with output scene properties.
          Output properties are for example the render settings or output file
     format.
  */
  TOutputProperties *getOutputProperties() const { return m_outputProp; }
  /*!
          Returns \b TOutputProperties with preview scene properties.
          This method is as above, but is called in the updates of the frame or
     of the sheet.
  */
  TOutputProperties *getPreviewProperties() const { return m_previewProp; }

  void saveData(TOStream &os) const;
  void loadData(TIStream &is, bool isLoadingProject);

  /*!
Returns cleanup parameters \b CleanupParameters, i.e. basic colors.
  */
  CleanupParameters *getCleanupParameters() const {
    return m_cleanupParameters;
  }
  /*!
           Return scanner parameters as Black and white scanner , graytones
     scanner or a color scanner.
  */
  TScannerParameters *getScanParameters() const { return m_scanParameters; }
  /*!
           Return vectorizer parameters.
  */
  VectorizerParameters *getVectorizerParameters() const {
    return m_vectorizerParameters;
  }
  /*!
Return device capture parameters.
*/
  CaptureParameters *getCaptureParameters() const {
    return m_captureParameters;
  }

  /*!
          Sets the scene's background color to \b color.
\sa getBgColor()
  */
  void setBgColor(const TPixel32 &color);
  /*!
          Returns the scene's background color.
\sa setBgColor()
  */
  TPixel getBgColor() const { return m_bgColor; }

  /*!
          Provides information about xsheet markers, xsheet horizontal line.
Set the distance between two markers to \p distance and \b offset to markers
offset,
\sa setMarkers()
  */
  void getMarkers(int &distance, int &offset) const {
    distance = m_markerDistance;
    offset   = m_markerOffset;
  }
  /*!
          Sets information about xsheet markers, xsheet horizontal line.
          Sets the distance between two markers to \p distance and \b offset,row
of first
          marker, to markers \b offset.
\sa getMarkers()
  */
  void setMarkers(int distance, int offset);
  /*!
          Returns full-color images subsampling in scene. Subsampling value is
the simplifying
          factor to be applied to animation levels, images when displayed in the
work area
          in order to have a faster visualization and playback; for example if
it is 2,
          one pixel each two is displayed.
\sa setFullcolorSubsampling()
  */
  int getFullcolorSubsampling() const { return m_fullcolorSubsampling; }
  /*!
          Sets the  full-color images subsampling in scene to \p s.
\sa getFullcolorSubsampling()
  */
  void setFullcolorSubsampling(int s);
  /*!
          Returns the level images subsampling in scene.
          \sa setTlvSubsampling() and getFullcolorSubsampling().
  */
  int getTlvSubsampling() const { return m_tlvSubsampling; }
  /*!
          Sets the level images subsampling in scene to \b s.
          \sa getTlvSubsampling()
  */
  void setTlvSubsampling(int s);
  /*!
          Returns field guide size.
          Field guide size is the number of fields the field guide is wide.
\sa setFieldGuideSize().
  */
  int getFieldGuideSize() const { return m_fieldGuideSize; }
  /*!
          Set field guide size to \b size. Field guide size is the number of
fields
          the field guide is wide.
\sa getFieldGuideSize()
  */
  void setFieldGuideSize(int size);
  /*!
          Returns field guide aspect ratio.
          Field guide aspect ratio is the ratio between the field guide width
and height.
\sa setFieldGuideAspectRatio()
  */
  double getFieldGuideAspectRatio() const { return m_fieldGuideAspectRatio; }
  /*!
          Sets field guide aspect ratio to \p ar.
\sa getFieldGuideAspectRatio()
  */
  void setFieldGuideAspectRatio(double ar);

  /* Returns whether the column color filter and transparency is available also
   * in render */
  bool isColumnColorFilterOnRenderEnabled() const {
    return m_columnColorFilterOnRender;
  }

  /* Activate / deactivate the column color filter in render */
  void enableColumnColorFilterOnRender(bool on) {
    m_columnColorFilterOnRender = on;
  }

  /* Returns initial save in path for the camera capture feature */
  TFilePath cameraCaptureSaveInPath() const { return m_camCapSaveInPath; }

  /* Set the initial save in path for the camera capture feature */
  void setCameraCaptureSaveInPath(const TFilePath &fp) {
    m_camCapSaveInPath = fp;
  }

  //! Substitutes current cameras with those stored in the specified stage tree.
  void cloneCamerasFrom(TStageObjectTree *stageObjects);

  //! Copies current cameras to passed stage tree. If no corresponding cameras
  //! in the stage are present, they are created. Outnumbering cameras in the
  //! stage are left untouched.
  void cloneCamerasTo(TStageObjectTree *stageObjects) const;

  QList<TPixel32> getNoteColors() const;
  TPixel32 getNoteColor(int colorIndex) const;
  void setNoteColor(TPixel32 color, int colorIndex);

private:
  // not implemented
  TSceneProperties(const TSceneProperties &);
  const TSceneProperties &operator=(const TSceneProperties &);
};

#endif
