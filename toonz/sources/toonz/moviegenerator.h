#pragma once

#ifndef MOVIEGENERATOR_INCLUDED
#define MOVIEGENERATOR_INCLUDED

#include <memory>

#include "tfilepath.h"
#include "tpixel.h"
#include "tgeometry.h"

// forward declaration
class ProgressMeter;
class TSceneProperties;
class TXsheet;
class ToonzScene;
class OnionSkinMask;
class TWidget;
class TOutputProperties;

//=============================================================================
// MovieGenerator
//-----------------------------------------------------------------------------

class MovieGenerator {
public:
  class Imp;

private:
  std::unique_ptr<Imp> m_imp;

public:
  class Listener {
  public:
    //! if listener returns false then the movie generation aborts
    virtual bool onFrameCompleted(int frameCount) = 0;
    virtual ~Listener() {}
  };

  MovieGenerator(const TFilePath &outputPath,
                 const TDimension &cameraResolution,
                 TOutputProperties &outputProperties, bool useMarkers);

  ~MovieGenerator();

  void setListener(Listener *listener);

  //! set how many scenes will be generated. call it once, at the beginning
  void setSceneCount(int sceneCount);

  //! Note: add soundtrack BEFORE scene.
  bool addSoundtrack(const ToonzScene &scene, int frameOffset,
                     int sceneFramesCount);

  bool addScene(ToonzScene &scene, int r0, int r1);

  void setBackgroundColor(TPixel32 color);
  TPixel32 getBackgroundColor();

  void setOnionSkin(int columnIndex, const OnionSkinMask &mask);

  TFilePath getOutputPath();

  void close();

private:
  // not implemented
  MovieGenerator(const MovieGenerator &);
  MovieGenerator &operator=(const MovieGenerator &);
};

#endif
