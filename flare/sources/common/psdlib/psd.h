#pragma once

#ifndef PSD_INCLUDED
#define PSD_INCLUDED

#include "psdutils.h"
#include "timage_io.h"

#define REF_LAYER_BY_NAME

class TRasterImageP;

#undef DVAPI
#undef DVVAR

#ifdef TNZCORE_EXPORTS
#define DVAPI DV_EXPORT_API
#define DVVAR DV_EXPORT_VAR
#else
#define DVAPI DV_IMPORT_API
#define DVVAR DV_IMPORT_VAR
#endif

// Photoshop's mode
#define ModeBitmap 0
#define ModeGrayScale 1
#define ModeIndexedColor 2
#define ModeRGBColor 3
#define ModeCMYKColor 4
#define ModeHSLColor 5
#define ModeHSBColor 6
#define ModeMultichannel 7
#define ModeDuotone 8
#define ModeLabColor 9
#define ModeGray16 10
#define ModeRGB48 11
#define ModeLab48 12
#define ModeCMYK64 13
#define ModeDeepMultichannel 14
#define ModeDuotone16 15

struct TPSDLayerMaskInfo {
  psdByte size;
  long top;
  long left;
  long bottom;
  long right;
  char default_colour;
  char flags;

  psdPixel rows, cols;
};

struct TPSDBlendModeInfo {
  char sig[4];
  char key[4];
  unsigned char opacity;
  unsigned char clipping;
  unsigned char flags;
};

struct TPSDLayerInfo {
  long top;
  long left;
  long bottom;
  long right;
  short channels;

  TPSDChannelInfo *chan;
  int *chindex;

  unsigned long layerId;
  unsigned long protect;
  unsigned long section;
  unsigned long foreignEffectID;
  unsigned long layerVersion;

  int blendClipping;
  int blendInterior;
  int knockout;
  int transparencyShapes;
  int layerMaskAsGlobalMask;
  int vectorMaskAsGlobalMask;

  TPSDBlendModeInfo blend;
  TPSDLayerMaskInfo mask;

  double referencePointX;
  double referencePointY;

  char *name;
  char *nameno;  // "layerNr"
  char *unicodeName;
  char *layerNameSource;
  psdByte additionalpos;
  psdByte additionallen;
  psdByte filepos;  //

  psdByte startDataPos;  // Posizione di inizio dati all'interno del file
  psdByte dataLength;    // lunghezza dati

  // LAYER EFFECTS
  unsigned long int *fxCommonStateVersion;
  int fxCommonStateVisible;
  unsigned long int fxShadowVersion;
  float fxShadowBlur;
  float fxShadowIntensity;
  float fxShadowAngle;
  float fxShadowDistance;

  // my tags
  bool isFolder;
};

struct TPSDHeaderInfo {
  char sig[4];
  short version;
  char reserved[6];
  short channels;
  long rows;
  long cols;
  short depth;
  short mode;
  double vres;  // image resource. Vertical resolution
  double hres;  // image resource. Horizontal resolution

  psdByte colormodepos;
  int layersCount;
  int mergedalpha;
  bool linfoBlockEmpty;
  TPSDLayerInfo *linfo;      // array of layer info
  psdByte lmistart, lmilen;  // set by doLayerAndMaskInfo()
  psdByte layerDataPos;      // set by doInfo
};

struct dictentry {
  int id;
  const char *key, *tag, *desc;
  void (*func)(FILE *f, struct dictentry *dict, TPSDLayerInfo *li);
};

// PSD LIB
// Restituisce eccezioni
class DVAPI TPSDReader {
  TFilePath m_path;
  FILE *m_file;
  int m_lx, m_ly;
  TPSDHeaderInfo m_headerInfo;
  int m_layerId;
  int m_shrinkX;
  int m_shrinkY;
  TRect m_region;

public:
  TPSDReader(const TFilePath &path);
  ~TPSDReader();
  TImageReaderP getFrameReader(TFilePath path);
  TPSDHeaderInfo getPSDHeaderInfo();

  void load(TRasterImageP &rasP, int layerId);
  TDimension getSize() const { return TDimension(m_lx, m_ly); }
  TPSDLayerInfo *getLayerInfo(int index);
  int getLayerInfoIndexById(int layerId);
  void setShrink(int shrink) {
    m_shrinkX = shrink;
    m_shrinkY = shrink;
  }
  void setShrink(int shrinkX, int shrinkY) {
    m_shrinkX = shrinkX;
    m_shrinkY = shrinkY;
  }
  void setRegion(TRect region) { m_region = region; }

  int getShrinkX() { return m_shrinkX; }
  int getShrinkY() { return m_shrinkY; }
  TRect getRegion() { return m_region; }

private:
  std::map<int, TRect> m_layersSavebox;

  bool doInfo();
  bool doHeaderInfo();
  bool doColorModeData();
  bool doImageResources();
  bool doLayerAndMaskInfo();
  bool doLayersInfo();
  bool readLayerInfo(int index);

  // void doImage(unsigned char *rasP, TPSDLayerInfo *li);
  void doImage(TRasterP &rasP, int layerId);

  void readImageData(TRasterP &rasP, TPSDLayerInfo *li, TPSDChannelInfo *chan,
                     int chancount, psdPixel rows, psdPixel cols);
  int m_error;
  TThread::Mutex m_mutex;
  int openFile();

  void doExtraData(TPSDLayerInfo *li, psdByte length);
  int sigkeyblock(FILE *f, struct dictentry *dict, TPSDLayerInfo *li);
  struct dictentry *findbykey(FILE *f, struct dictentry *parent, char *key,
                              TPSDLayerInfo *li);
};

// converts psd layers structure into toonz level structure	according to
// path
class DVAPI TPSDParser {
  class Level {
  public:
    Level(std::string nm = "Unknown", int lid = 0, bool is_folder = false)
        : name(nm), layerId(lid), folder(is_folder) {}
    void addFrame(int layerId, bool isFolder = false) {
      framesId.push_back(Frame(layerId, isFolder));
    }
    int getFrameCount() { return (int)framesId.size(); }
    std::string getName() { return name; }
    int getLayerId() { return layerId; }
    void setName(std::string nname) { name = nname; }
    void setLayerId(int nlayerId) { layerId = nlayerId; }
    int getFrameId(int index) {
      assert(index >= 0 && index < (int)framesId.size());
      return framesId[index].layerId;
    }
    // return true if frame is a subfolder
    bool isSubFolder(int frameIndex) {
      assert(frameIndex >= 0 && frameIndex < (int)framesId.size());
      return framesId[frameIndex].isFolder;
    }
    // return true if the level is built from a psd folder
    bool isFolder() { return folder; }

  private:
    struct Frame {
      int layerId;  // psd layerId
      bool isFolder;
      Frame(int layId, bool folder) : layerId(layId), isFolder(folder) {}
    };

    std::string name;             // psd name
    int layerId;                  // psd layer id
    std::vector<Frame> framesId;  // array of layer ID as frame
    bool folder;
  };

  TFilePath m_path;
  std::vector<Level> m_levels;  // layers id
  TPSDReader *m_psdreader;      // lib
public:
  // path define levels construction method
  // if path = :
  //  filename.psd                    flat image so LevelsCount = 1;
  //  filename#LAYERID.psd            each psd layer as a tlevel
  //  filename#LAYERID#frames.psd     each psd layer as a frame so there is only
  //  one tlevel with 1 or more frames;
  //  filename#LAYERID#folder.psd     each psd layer is a tlevel and
  //                                  each folder is a tlevel such as the psd
  //                                  layers
  //                                  contained into folder are frames of tlevel
  // LAYERID(Integer) is psd layerId
  TPSDParser(const TFilePath &path);
  ~TPSDParser();
  int getLevelsCount() { return (int)m_levels.size(); }
  // load a psd layer
  // if layerId == 0 load flat image
  void load(TRasterImageP &rasP, int layerId = 0);

  // Returns psd layerID
  int getLevelId(int index) {
    assert(index >= 0 && index < (int)m_levels.size());
    return m_levels[index].getLayerId();
  }
  bool isFolder(int levelIndex) {
    assert(levelIndex >= 0 && levelIndex < (int)m_levels.size());
    return m_levels[levelIndex].isFolder();
  }
  int getLevelIndexById(int levelId);
  // Returns layerID by name. Note that the layer name is not unique, so it
  // return the first layer id found.
  int getLevelIdByName(std::string levelName);
  int getFrameId(int layerId, int frameIndex) {
    return m_levels[getLevelIndexById(layerId)].getFrameId(frameIndex);
  }
  int getFramesCount(int levelId);
  bool isSubFolder(int levelIndex, int frameIndex) {
    assert(levelIndex >= 0 && levelIndex < (int)m_levels.size());
    return m_levels[levelIndex].isSubFolder(frameIndex);
  }
  std::string getLevelName(int levelId);
  // Returns level name.
  // If there are 2 or more level with the same name then
  // returns levelname, levelname__2, etc
  std::string getLevelNameWithCounter(int levelId);

private:
  void doLevels();  // do m_levels
};

#endif  // TIIO_PSD_H
