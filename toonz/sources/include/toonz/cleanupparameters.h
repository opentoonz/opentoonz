#pragma once

#ifndef CLEANUPPARAMETERS_INCLUDED
#define CLEANUPPARAMETERS_INCLUDED

// Qt includes
#include <QString>

// Toonz includes
#include "tfilepath.h"
#include "toonz/targetcolors.h"
#include "tcamera.h"
#include "tpalette.h"

#undef DVAPI
#undef DVVAR
#ifdef TOONZLIB_EXPORTS
#define DVAPI DV_EXPORT_API
#define DVVAR DV_EXPORT_VAR
#else
#define DVAPI DV_IMPORT_API
#define DVVAR DV_IMPORT_VAR
#endif

//---------------------------------------------------------------

//  Forward declarations

class TIStream;
class TOStream;
class ToonzScene;

//---------------------------------------------------------------

//*******************************************************************************
//    CleanupTypes namespace
//*******************************************************************************

namespace CleanupTypes {

// #define LINESHAPE_ENABLED

enum PEGS_SIDE {
  PEGS_NONE,

  PEGS_BOTTOM,
  PEGS_TOP,
  PEGS_LEFT,
  PEGS_RIGHT,

  PEGS_SIDE_HOW_MANY
};

enum AUTO_ADJ_MODE {
  AUTO_ADJ_NONE,

  AUTO_ADJ_BLACK_EQ,
  AUTO_ADJ_HISTOGRAM,
  AUTO_ADJ_HISTO_L,

#ifdef LINESHAPE_ENABLED
  AUTO_ADJ_LINESHAPE,
#endif

  AUTO_ADJ_HOW_MANY

};

enum OVERLAP_ALGO { OVER_NO_BLEND, OVER_BLEND, OVER_MERGE, OVER_HOW_MANY };

enum AUTOCENTER_TYPE {
  AUTOCENTER_NONE,
  AUTOCENTER_FDG,
  AUTOCENTER_CTR,
  /* c'era una volta AUTOALIGN */

  AUTOCENTER_HOW_MANY
};

struct DOT {
  float x, y; /* baricentro del dot */
  int x1, y1, x2,
      y2;     /* estremi, in pixel, del rettangolo che contiene il dot */
  int area;   /* espressa in numero di pixel */
  int lx, ly; /* espresse in pixel */
};

class FDG_INFO {
public:
  std::string m_name;
  int ctr_type;
  /* ctr_type == TRUE:  */
  double ctr_x, ctr_y; /* in mm */
  double ctr_angle;    /* in deg */
  double ctr_skew;     /* in deg */
  /* ctr_type == FALSE: */
  std::vector<DOT> dots;

  double dist_ctr_to_ctr_hole;  /* Distanza centro fg centro foro centrale */
  double dist_ctr_hole_to_edge; /* in mm */

  FDG_INFO()
      : ctr_type(0)
      , ctr_x(0)
      , ctr_y(0)
      , ctr_angle(0)
      , ctr_skew(0)
      , dist_ctr_to_ctr_hole(0)
      , dist_ctr_hole_to_edge(0) {}

  bool operator==(const FDG_INFO &rhs) const {
    return (ctr_type == rhs.ctr_type && ctr_x == rhs.ctr_x &&
            ctr_y == rhs.ctr_y && ctr_angle == rhs.ctr_angle &&
            ctr_skew == rhs.ctr_skew &&
            dist_ctr_to_ctr_hole == rhs.dist_ctr_to_ctr_hole &&
            dist_ctr_hole_to_edge == rhs.dist_ctr_hole_to_edge);
  }
};

}  // namespace CleanupTypes

//*******************************************************************************
//    CleanupParameters declaration
//*******************************************************************************

enum { lpNone = 0, lpGrey, lpColor };

class DVAPI CleanupParameters {
  CleanupTypes::FDG_INFO m_fdgInfo;
  bool m_dirtyFlag;

public:
  static CleanupParameters GlobalParameters;
  static CleanupParameters LastSavedParameters;

public:
  CleanupTypes::AUTOCENTER_TYPE m_autocenterType;
  CleanupTypes::PEGS_SIDE m_pegSide;

  TCamera m_camera;
  int m_rotate;
  bool m_flipx;
  bool m_flipy;
  double m_offx, m_offy;

  int m_lineProcessingMode;

  double m_closestField;
  CleanupTypes::AUTO_ADJ_MODE m_autoAdjustMode;
  double m_sharpness;
  bool m_transparencyCheckEnabled;
  bool m_noAntialias;
  bool m_postAntialias;
  int m_despeckling;
  int m_aaValue;
  TargetColors m_colors;
  TPaletteP m_cleanupPalette;
  TFilePath m_path;

  /*--- オフセットを軸ごとにロックする ---*/
  bool m_offx_lock, m_offy_lock;

public:
  CleanupParameters();
  CleanupParameters(const CleanupParameters &p) { assign(&p); }

  const CleanupParameters &operator=(const CleanupParameters &p) {
    assign(&p);
    return *this;
  }

  const CleanupTypes::FDG_INFO &getFdgInfo();

  std::string getFdgName() const { return m_fdgInfo.m_name; }
  bool setFdgByName(std::string name);

  // la scena serve per gestire i default:
  // internamente il path puo' essere vuoto
  // oppure della forma "+pippo/$savepath"
  // nota: setPath() accetta un path "espanso"
  // (es. "+drawings/scene1") e lo trasforma
  // nella forma compressa (es. "")
  TFilePath getPath(ToonzScene *scene) const;
  void setPath(ToonzScene *scene, TFilePath fp);

  static void getFdgNames(std::vector<std::string> &names);

  void getOutputImageInfo(TDimension &resolution, double &dpix,
                          double &dpiy) const;

  void assign(const CleanupParameters *params, bool clonePalette = true);

  bool getDirtyFlag() { return m_dirtyFlag; }
  void setDirtyFlag(bool value) { m_dirtyFlag = value; }

  void saveData(TOStream &os) const;
  void loadData(TIStream &is, bool globalParams);
};

#endif
