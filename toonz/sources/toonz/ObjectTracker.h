#pragma once

#if !defined(OBEJCTTRACKER_H_INCLUDED_)
#define OBEJCTTRACKER_H_INCLUDED_

#include <memory>

#include "traster.h"
#include "predict3d.h"

typedef unsigned char UBYTE8;
typedef unsigned int UINT32;

// structure pixel RGB
struct ValuePixel {
  UBYTE8 r;
  UBYTE8 g;
  UBYTE8 b;
};

// structure position (old and new) and distance^2 of neighbours,
struct NEIGHBOUR {
  short X;
  short Y;
  float dist2;
  short X_old, Y_old;
};

class CObjectTracker {
public:
  short objID;
  bool track;

private:
  struct OBJECT_SPECS {
    bool Status;
    bool assignedAnObject;

    short dim_temp;  // dimension searce template area (real area =
                     // (2*dim_temp+1)*(2*dim_temp+1))
    short var_dim;   // variation dimension window (dimension in pixel)
    float threshold_distance;    // threshold of lose object
    float threshold_distance_B;  // threshold of lose object

    // characterize
    short X;
    short Y;
    short W;
    short H;
    short half_pixelx;  // precision half pixel x
    short half_pixely;  // precision half pixel y

    // old characterize
    short X_old;
    short Y_old;
    short W_old;
    short H_old;

    // histogram
    std::unique_ptr<float[]> initHistogram;
    std::unique_ptr<float[]> weights_background;

    // template characterize
    short X_temp;
    short Y_temp;
    short W_temp;
    short H_temp;
  };

  OBJECT_SPECS m_sTrackingObject;
  int m_nImageWidth;
  int m_nImageHeight;
  bool colorimage;
  bool att_background;
  int HISTOGRAM_LENGTH;
  bool man_occlusion;
  NEIGHBOUR neighbours[30];
  Predict3D::Point initial;
  bool m_initialized;
  std::string m_visible;
  int m_K_dist;

  //--------------------------------------------------------------------------------------------------------

  void FindHistogram(TRaster32P *frame, float(*histogram), float h);
  //--------------------------------------------------------------------------------------------------------

  void FindHistogramBackground(TRaster32P *frame, float(*background));
  //--------------------------------------------------------------------------------------------------------

  void FindWeightsBackground(TRaster32P *frame);
  //--------------------------------------------------------------------------------------------------------

  void FindWightsAndCOM(TRaster32P *frame, float(*histogram));
  //--------------------------------------------------------------------------------------------------------

  UBYTE8 CheckEdgeExistance(TRaster32P *frame, short _x, short _y);
  //--------------------------------------------------------------------------------------------------------

  void UpdateInitialHistogram(float(*histogram));
  //--------------------------------------------------------------------------------------------------------

  ValuePixel GetPixelValues(TRaster32P *frame, short x, short y);
  //--------------------------------------------------------------------------------------------------------

  void SetPixelValues(TRaster32P *frame, ValuePixel pixelValues, short x,
                      short y);

public:
  CObjectTracker(int imW, int imH, bool _colorimage, bool _att_background,
                 bool _man_occlusion);
  //--------------------------------------------------------------------------------------------------------

  virtual ~CObjectTracker();
  //--------------------------------------------------------------------------------------------------------

  void FindNextLocation(TRaster32P *frame);
  //--------------------------------------------------------------------------------------------------------

  void ObjeckTrackerHandlerByUser(TRaster32P *frame);
  //--------------------------------------------------------------------------------------------------------

  void ObjectTrackerInitObjectParameters(short id, short x, short y,
                                         short Width, short Height, short _dim,
                                         short _var_dim, float _dist,
                                         float _distB);
  //--------------------------------------------------------------------------------------------------------

  void updateTemp();

  //--------------------------------------------------------------------------------------------------------

  float Matching(TRaster32P *frame, TRaster32P *frame_temp);
  //--------------------------------------------------------------------------------------------------------

  void DistanceAndUpdate(NEIGHBOUR position);
  //--------------------------------------------------------------------------------------------------------

  void SetPositionByNeighbours(void);
  //--------------------------------------------------------------------------------------------------------

  void WriteOnOutputFile(char *filename);
  //--------------------------------------------------------------------------------------------------------

  void DistanceReset(void);
  //--------------------------------------------------------------------------------------------------------

  NEIGHBOUR GetPosition(void);
  //--------------------------------------------------------------------------------------------------------

  void SetInit(bool status);
  //--------------------------------------------------------------------------------------------------------

  bool GetInit();
  //--------------------------------------------------------------------------------------------------------

  std::string GetVisibility();
  //--------------------------------------------------------------------------------------------------------

  void SetVisibility(std::string visibily);
  //--------------------------------------------------------------------------------------------------------

  Predict3D::Point GetInitials();
  //--------------------------------------------------------------------------------------------------------

  void SetInitials(NEIGHBOUR position);
  //--------------------------------------------------------------------------------------------------------

  void SetPosition(short x, short y);
  //--------------------------------------------------------------------------------------------------------

  int GetKDist();
  //--------------------------------------------------------------------------------------------------------
  short getId() { return objID; }

};  // end of trackobject class
//---------------------------------------------------------------------------
#endif
