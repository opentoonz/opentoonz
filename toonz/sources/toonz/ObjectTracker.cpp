

/*******************************************************************
Tracker based on Variable Mean-Shift algorithm and Interpolate Template Matching

Version: MT1

Compiler: Microsoft Visual Studio.net

Luigi Sgaglione

**********************************************************************/
#include <memory>

#include "ObjectTracker.h"
#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <fstream>
#include <iostream>
#include <ios>
#include <algorithm>

using namespace std;

#define MEANSHIFT_ITERATION_NO 15
#define ALPHA 0.98
#define EDGE_DETECT_THRESHOLD 32
#define ITERACTION_THRESHOLD 0.4
#define MAX_FLOAT 3.40282e+38;

//---------------------------------------------------------------------------------------------------------
// Costructor
CObjectTracker::CObjectTracker(int imW, int imH, bool _colorimage,
                               bool _att_background, bool _man_occlusion) {
  att_background = _att_background;
  colorimage     = _colorimage;
  m_nImageWidth  = imW;
  m_nImageHeight = imH;
  track          = true;
  man_occlusion  = _man_occlusion;
  m_initialized  = false;

  for (int i = 0; i < 30; i++) {
    neighbours[i].X     = -1;
    neighbours[i].Y     = -1;
    neighbours[i].dist2 = MAX_FLOAT;
    neighbours[i].X_old = -1;
    neighbours[i].Y_old = -1;
  }

  if (!colorimage)
    HISTOGRAM_LENGTH = 512;
  else
    HISTOGRAM_LENGTH = 8192;

  m_sTrackingObject.initHistogram.reset(new float[HISTOGRAM_LENGTH]);
  if (att_background)
    m_sTrackingObject.weights_background.reset(new float[HISTOGRAM_LENGTH]);
  else
    m_sTrackingObject.weights_background.reset();

  m_sTrackingObject.Status = false;
  for (short j                         = 0; j < HISTOGRAM_LENGTH; j++)
    m_sTrackingObject.initHistogram[j] = 0;
};

//--------------------------------------------------------------------------------------------------------
// Distructor
CObjectTracker::~CObjectTracker() {}

//--------------------------------------------------------------------------------------------------------
// Get pixel value RGB
ValuePixel CObjectTracker::GetPixelValues(TRaster32P *frame, short x, short y) {
  TPixel32 *data;
  ValuePixel pixelValues;

  (*frame)->lock();
  data = x + (*frame)->pixels(abs(y - m_nImageHeight + 1));

  pixelValues.b = data->b;
  pixelValues.g = data->g;
  pixelValues.r = data->r;

  (*frame)->unlock();

  return (pixelValues);
}

//--------------------------------------------------------------------------------------------------------
// Set pixel value RGB
void CObjectTracker::SetPixelValues(TRaster32P *frame, ValuePixel pixelValues,
                                    short x, short y) {
  TPixel32 *data;

  (*frame)->lock();
  data = x + (*frame)->pixels(abs(y - m_nImageHeight + 1));

  data->b = pixelValues.b;
  data->g = pixelValues.g;
  data->r = pixelValues.r;

  (*frame)->unlock();
}

//--------------------------------------------------------------------------------------------------------
// Inizialization parameters object
void CObjectTracker::ObjectTrackerInitObjectParameters(
    short id, short x, short y, short Width, short Height, short _dim,
    short _var_dim, float _dist, float _distB) {
  objID = id;

  m_sTrackingObject.dim_temp             = _dim;
  m_sTrackingObject.var_dim              = _var_dim;
  m_sTrackingObject.threshold_distance   = _dist;
  m_sTrackingObject.threshold_distance_B = _distB;
  m_sTrackingObject.X                    = x;
  m_sTrackingObject.Y                    = y;
  m_sTrackingObject.W                    = Width;
  m_sTrackingObject.H                    = Height;
  m_sTrackingObject.X_old                = x;
  m_sTrackingObject.Y_old                = y;
  m_sTrackingObject.W_old                = Width;
  m_sTrackingObject.H_old                = Height;
  m_sTrackingObject.X_temp               = x;
  m_sTrackingObject.Y_temp               = y;
  m_sTrackingObject.W_temp               = Width;
  m_sTrackingObject.H_temp               = Height;

  m_sTrackingObject.half_pixelx = 0;
  m_sTrackingObject.half_pixely = 0;

  m_sTrackingObject.Status           = true;
  m_sTrackingObject.assignedAnObject = false;
}

//--------------------------------------------------------------------------------------------------------
// Update frame template
void CObjectTracker::updateTemp() {
  m_sTrackingObject.X_temp = m_sTrackingObject.X;
  m_sTrackingObject.Y_temp = m_sTrackingObject.Y;
  m_sTrackingObject.W_temp = m_sTrackingObject.W;
  m_sTrackingObject.H_temp = m_sTrackingObject.H;
}
//--------------------------------------------------------------------------------------------------------
// Object Tracking
void CObjectTracker::ObjeckTrackerHandlerByUser(TRaster32P *frame) {
  if (m_sTrackingObject.Status) {
    if (!m_sTrackingObject.assignedAnObject) {
      FindHistogram(frame, m_sTrackingObject.initHistogram.get(), 1);
      m_sTrackingObject.assignedAnObject = true;
      /*
      ofstream output;
      output.open("output.txt",ios_base::app);
      output<<m_sTrackingObject.X <<" "<<m_sTrackingObject.Y<<endl;
      output<<m_sTrackingObject.W <<" "<<m_sTrackingObject.H<<endl;
      output<<m_sTrackingObject.half_pixelx <<"
      "<<m_sTrackingObject.half_pixely<<endl;
      if (m_visible=="")
      {
              output<<"VISIBLE"<<endl;
      }
      else
      {
              output<<m_visible<<endl;
      }
      output.close();
      output.open("output_center.txt",ios_base::app);
      output<<"Coordinate del punto - X : "<<m_sTrackingObject.X -
      ((m_nImageWidth - 1)/2)<<" - Y : "<<m_sTrackingObject.Y - ((m_nImageHeight
      - 1)/2)<<endl;
      output<<"Larghezza dell'area : "<<m_sTrackingObject.W <<" - Altezza
      dell'aera : "<<m_sTrackingObject.H<<endl;
      output<<"Mezzo pixel : "<<m_sTrackingObject.half_pixelx <<"
      "<<m_sTrackingObject.half_pixely<<endl;
      if (m_visible=="")
      {
              output<<"Visibilità : VISIBLE"<<endl;
      }
      else
      {
              output<<"Visibilità : "<<m_visible<<endl;
      }
      output.close();*/
    } else {
      FindNextLocation(frame);
    }
  }
}

//--------------------------------------------------------------------------------------------------------
// histogram object
void CObjectTracker::FindHistogram(TRaster32P *frame, float(*histogram),
                                   float h) {
  short normx = 0, normy = 0;
  float normc = 0.0, normc1 = 0.0;
  short i   = 0;
  short x   = 0;
  short y   = 0;
  UBYTE8 E  = 0;
  UBYTE8 qR = 0, qG = 0, qB = 0;
  ValuePixel pixelValues;
  int indice = 0;

  for (i = 0; i < HISTOGRAM_LENGTH; i++) histogram[i] = 0.0;

  if ((colorimage) && (att_background)) FindWeightsBackground(frame);

  // normalization
  normx = short(m_sTrackingObject.X + m_sTrackingObject.W / 2);
  normy = short(m_sTrackingObject.Y + m_sTrackingObject.H / 2);

  for (y = std::max(m_sTrackingObject.Y - m_sTrackingObject.H / 2, 0);
       y <= std::min(m_sTrackingObject.Y + m_sTrackingObject.H / 2,
                     m_nImageHeight - 1);
       y++)
    for (x = std::max(m_sTrackingObject.X - m_sTrackingObject.W / 2, 0);
         x <= std::min(m_sTrackingObject.X + m_sTrackingObject.W / 2,
                       m_nImageWidth - 1);
         x++) {
      E = CheckEdgeExistance(frame, x, y);

      pixelValues = GetPixelValues(frame, x, y);

      // components RGB "quantizzate"
      if (!colorimage) {
        indice = 256 * E + pixelValues.r;
      } else {
        qR     = pixelValues.r / 16;
        qG     = pixelValues.g / 16;
        qB     = pixelValues.b / 16;
        indice = 4096 * E + 256 * qR + 16 * qG + qB;
      }

      histogram[indice] += 1 -
                           (((m_sTrackingObject.X - x) / normx) *
                                ((m_sTrackingObject.X - x) / normx) +
                            ((m_sTrackingObject.Y - y) / normy) *
                                ((m_sTrackingObject.Y - y) / normy)) /
                               (h * h);
    }

  if ((colorimage) && (att_background)) {
    for (i = 0; i < HISTOGRAM_LENGTH; i++)
      histogram[i] *= m_sTrackingObject.weights_background[i];

    // normalization pdf
    for (i = 0; i < HISTOGRAM_LENGTH; i++) {
      normc += histogram[i];
      normc1 += m_sTrackingObject.weights_background[i];
    }
    // Pdf
    for (i         = 0; i < HISTOGRAM_LENGTH; i++)
      histogram[i] = histogram[i] / (normc * normc1);
  }

  else {
    for (i = 0; i < HISTOGRAM_LENGTH; i++) normc += histogram[i];

    for (i         = 0; i < HISTOGRAM_LENGTH; i++)
      histogram[i] = histogram[i] / (normc);
  }
}

//--------------------------------------------------------------------------------------------------------
// Histogram background
void CObjectTracker::FindHistogramBackground(TRaster32P *frame,
                                             float(*background)) {
  short i   = 0;
  short x   = 0;
  short y   = 0;
  UBYTE8 E  = 0;
  UBYTE8 qR = 0, qG = 0, qB = 0;
  ValuePixel pixelValues;
  UINT32 pix = 0;

  for (i = 0; i < HISTOGRAM_LENGTH; i++) background[i] = 0.0;

  for (y = std::max(m_sTrackingObject.Y - (m_sTrackingObject.H * 1.73) / 2,
                    0.0);
       y <= std::min(m_sTrackingObject.Y + (m_sTrackingObject.H * 1.73) / 2,
                     m_nImageHeight - 1.0);
       y++)
    for (x = std::max(m_sTrackingObject.X - (m_sTrackingObject.W * 1.73) / 2,
                      0.0);
         x <= std::min(m_sTrackingObject.X + (m_sTrackingObject.W * 1.73) / 2,
                       m_nImageWidth - 1.0);
         x++) {
      if (((m_sTrackingObject.Y - m_sTrackingObject.H / 2) <= y) &&
          (y <= (m_sTrackingObject.Y + m_sTrackingObject.H / 2)) &&
          ((m_sTrackingObject.X - m_sTrackingObject.W / 2) <= x) &&
          (x <= (m_sTrackingObject.X + m_sTrackingObject.W / 2)))
        continue;

      E = CheckEdgeExistance(frame, x, y);

      pixelValues = GetPixelValues(frame, x, y);

      // components RGB "quantizzate"
      qR = pixelValues.r / 16;
      qG = pixelValues.g / 16;
      qB = pixelValues.b / 16;

      background[4096 * E + 256 * qR + 16 * qG + qB] += 1;
      pix++;
    }

  // Pdf
  for (i = 0; i < HISTOGRAM_LENGTH; i++) background[i] = background[i] / pix;
}
//--------------------------------------------------------------------------------------------------------
// Weights Background
void CObjectTracker::FindWeightsBackground(TRaster32P *frame) {
  float small1;
  std::unique_ptr<float[]> background(new float[HISTOGRAM_LENGTH]);
  short i;
  for (i                                    = 0; i < HISTOGRAM_LENGTH; i++)
    m_sTrackingObject.weights_background[i] = 0.0;

  // Histogram background
  FindHistogramBackground(frame, background.get());

  // searce min != 0.0
  for (i = 0; background[i] == 0.0; i++)
    ;
  small1 = background[i];
  for (i = 0; i < HISTOGRAM_LENGTH; i++)
    if ((background[i] != 0.0) && (background[i] < small1))
      small1 = background[i];

  // weights
  for (i = 0; i < HISTOGRAM_LENGTH; i++) {
    if (background[i] == 0.0)
      m_sTrackingObject.weights_background[i] = 1;
    else
      m_sTrackingObject.weights_background[i] = small1 / background[i];
  }
}

//--------------------------------------------------------------------------------------------------------
// new location
void CObjectTracker::FindWightsAndCOM(TRaster32P *frame, float(*histogram)) {
  short i            = 0;
  short x            = 0;
  short y            = 0;
  UBYTE8 E           = 0;
  float sumOfWeights = 0;
  short ptr          = 0;
  UBYTE8 qR = 0, qG = 0, qB = 0;
  float newX = 0.0;
  float newY = 0.0;
  ValuePixel pixelValues;

  std::unique_ptr<float[]> weights(new float[HISTOGRAM_LENGTH]);

  // weigths
  for (i = 0; i < HISTOGRAM_LENGTH; i++) {
    if (histogram[i] > 0.0)
      weights[i] = sqrt(m_sTrackingObject.initHistogram[i] / histogram[i]);
    else
      weights[i] = 0.0;
  }

  // new location
  for (y = std::max(m_sTrackingObject.Y - m_sTrackingObject.H / 2, 0);
       y <= std::min(m_sTrackingObject.Y + m_sTrackingObject.H / 2,
                     m_nImageHeight - 1);
       y++)
    for (x = std::max(m_sTrackingObject.X - m_sTrackingObject.W / 2, 0);
         x <= std::min(m_sTrackingObject.X + m_sTrackingObject.W / 2,
                       m_nImageWidth - 1);
         x++) {
      E = CheckEdgeExistance(frame, x, y);

      pixelValues = GetPixelValues(frame, x, y);

      if (!colorimage) {
        ptr = 256 * E + pixelValues.r;
      } else {
        qR = pixelValues.r / 16;
        qG = pixelValues.g / 16;
        qB = pixelValues.b / 16;

        ptr = 4096 * E + 256 * qR + 16 * qG + qB;
      }

      newX += (weights[ptr] * x);
      newY += (weights[ptr] * y);

      sumOfWeights += weights[ptr];
    }

  if (sumOfWeights > 0) {
    m_sTrackingObject.X = short((newX / sumOfWeights) + 0.5);
    m_sTrackingObject.Y = short((newY / sumOfWeights) + 0.5);
  }
}

//--------------------------------------------------------------------------------------------------------
// Edge Information of pixel
UBYTE8 CObjectTracker::CheckEdgeExistance(TRaster32P *frame, short _x,
                                          short _y) {
  UBYTE8 E         = 0;
  short GrayCenter = 0;
  short GrayLeft   = 0;
  short GrayRight  = 0;
  short GrayUp     = 0;
  short GrayDown   = 0;
  ValuePixel pixelValues;

  pixelValues = GetPixelValues(frame, _x, _y);

  GrayCenter = short(3 * pixelValues.r + 6 * pixelValues.g + pixelValues.b);
  if (_x > 0) {
    pixelValues = GetPixelValues(frame, _x - 1, _y);

    GrayLeft = short(3 * pixelValues.r + 6 * pixelValues.g + pixelValues.b);
  }

  if (_x < (m_nImageWidth - 1)) {
    pixelValues = GetPixelValues(frame, _x + 1, _y);

    GrayRight = short(3 * pixelValues.r + 6 * pixelValues.g + pixelValues.b);
  }

  if (_y > 0) {
    pixelValues = GetPixelValues(frame, _x, _y - 1);

    GrayUp = short(3 * pixelValues.r + 6 * pixelValues.g + pixelValues.b);
  }

  if (_y < (m_nImageHeight - 1)) {
    pixelValues = GetPixelValues(frame, _x, _y + 1);

    GrayDown = short(3 * pixelValues.r + 6 * pixelValues.g + pixelValues.b);
  }

  if (abs((GrayCenter - GrayLeft) / 10) > EDGE_DETECT_THRESHOLD) E = 1;

  if (abs((GrayCenter - GrayRight) / 10) > EDGE_DETECT_THRESHOLD) E = 1;

  if (abs((GrayCenter - GrayUp) / 10) > EDGE_DETECT_THRESHOLD) E = 1;

  if (abs((GrayCenter - GrayDown) / 10) > EDGE_DETECT_THRESHOLD) E = 1;

  return (E);
}

//--------------------------------------------------------------------------------------------------------
// Mean-shift
void CObjectTracker::FindNextLocation(TRaster32P *frame) {
  short i         = 0;
  short iteration = 0;
  short xold      = 0;
  short yold      = 0;
  float distanza;
  float Height, Width;
  float h = 1.0;
  float h_opt;
  float DELTA;
  double rho, rho1, rho2;

  std::unique_ptr<float[]> currentHistogram(new float[HISTOGRAM_LENGTH]);

  Height = m_sTrackingObject.H;
  Width  = m_sTrackingObject.W;

  // old
  m_sTrackingObject.W_old = m_sTrackingObject.W;
  m_sTrackingObject.H_old = m_sTrackingObject.H;

  m_sTrackingObject.X_old = m_sTrackingObject.X;
  m_sTrackingObject.Y_old = m_sTrackingObject.Y;

  for (iteration = 0; iteration < MEANSHIFT_ITERATION_NO; iteration++) {
    rho  = 0.0;
    rho1 = 0.0;
    rho2 = 0.0;

    DELTA = 0.1 * h;

    // Prew center
    xold = m_sTrackingObject.X;
    yold = m_sTrackingObject.Y;

    // Histogram with bandwidth h
    FindHistogram(frame, currentHistogram.get(), h);

    // New location
    FindWightsAndCOM(frame, currentHistogram.get());

    // Histogram with new location
    FindHistogram(frame, currentHistogram.get(), h);

    // Battacharyya coefficient
    for (i = 0; i < HISTOGRAM_LENGTH; i++)
      rho += sqrt(m_sTrackingObject.initHistogram[i] * currentHistogram[i]);

    // old center
    m_sTrackingObject.X = xold;
    m_sTrackingObject.Y = yold;

    // Histogram with bandwidth h-DELTA
    m_sTrackingObject.H = Height - m_sTrackingObject.var_dim;
    m_sTrackingObject.W = Width - m_sTrackingObject.var_dim;
    FindHistogram(frame, currentHistogram.get(), h - DELTA);

    // New location
    FindWightsAndCOM(frame, currentHistogram.get());

    // Histogram with new location
    FindHistogram(frame, currentHistogram.get(), h - DELTA);

    // Battacharyya coefficient
    for (i = 0; i < HISTOGRAM_LENGTH; i++)
      rho1 += sqrt(m_sTrackingObject.initHistogram[i] * currentHistogram[i]);

    // old center
    m_sTrackingObject.X = xold;
    m_sTrackingObject.Y = yold;

    // Histogram with bandwidth h+DELTA
    m_sTrackingObject.H = Height + m_sTrackingObject.var_dim;
    m_sTrackingObject.W = Width + m_sTrackingObject.var_dim;
    FindHistogram(frame, currentHistogram.get(), h + DELTA);

    // New location
    FindWightsAndCOM(frame, currentHistogram.get());

    // Histogram with new location
    FindHistogram(frame, currentHistogram.get(), h + DELTA);

    // Battacharyya coefficient
    for (i = 0; i < HISTOGRAM_LENGTH; i++)
      rho2 += sqrt(m_sTrackingObject.initHistogram[i] * currentHistogram[i]);

    // h_optimal
    if ((rho >= rho1) && (rho >= rho2))
      h_opt = h;

    else if (rho1 > rho2) {
      h_opt = h - DELTA;
      Height -= m_sTrackingObject.var_dim;
      Width -= m_sTrackingObject.var_dim;
    } else {
      h_opt = h + DELTA;
      Height += m_sTrackingObject.var_dim;
      Width += m_sTrackingObject.var_dim;
    }

    // oversensitive scale adaptation
    h = 0.1 * h_opt + (1 - 0.1) * h;

    // New dimension Object box
    m_sTrackingObject.H = Height;
    m_sTrackingObject.W = Width;

    // old center
    m_sTrackingObject.X = xold;
    m_sTrackingObject.Y = yold;

    // Current Histogram
    FindHistogram(frame, currentHistogram.get(), h);

    // Definitive new location
    FindWightsAndCOM(frame, currentHistogram.get());

    // threshold
    distanza = sqrt(
        float((xold - m_sTrackingObject.X) * (xold - m_sTrackingObject.X) +
              (yold - m_sTrackingObject.Y) * (yold - m_sTrackingObject.Y)));

    if (distanza <= ITERACTION_THRESHOLD) break;
  }
  // New Histogram
  FindHistogram(frame, currentHistogram.get(), h);
  // Update
  UpdateInitialHistogram(currentHistogram.get());
}

//--------------------------------------------------------------------------------------------------------
// Update Initial histogram
void CObjectTracker::UpdateInitialHistogram(float(*histogram)) {
  short i = 0;

  for (i = 0; i < HISTOGRAM_LENGTH; i++)
    m_sTrackingObject.initHistogram[i] =
        ALPHA * m_sTrackingObject.initHistogram[i] + (1 - ALPHA) * histogram[i];
}

//-------------------------------------------------------------------------------------------------------
// Template Matching
float CObjectTracker::Matching(TRaster32P *frame, TRaster32P *frame_temp) {
  float dist     = 0.0;
  float min_dist = MAX_FLOAT;

  short u, v, x, y;
  short u_sup, v_sup;
  short x_min, y_min;
  short x_max, y_max;
  short dimx, dimy;
  short dimx_int, dimy_int;
  short ok_u = 0;
  short ok_v = 0;

  if (!track) {
    if (m_sTrackingObject.X != -1) {
      m_sTrackingObject.X_old = m_sTrackingObject.X;
      m_sTrackingObject.Y_old = m_sTrackingObject.Y;
    } else {
      m_sTrackingObject.X = m_sTrackingObject.X_old;
      m_sTrackingObject.Y = m_sTrackingObject.Y_old;
    }
  }

  std::unique_ptr<short[]> u_att(new short[2 * m_sTrackingObject.dim_temp + 1]);
  std::unique_ptr<short[]> v_att(new short[2 * m_sTrackingObject.dim_temp + 1]);

  x_min = std::max(m_sTrackingObject.X_temp - m_sTrackingObject.W_temp / 2, 0);
  y_min = std::max(m_sTrackingObject.Y_temp - m_sTrackingObject.H_temp / 2, 0);

  x_max = std::min(m_sTrackingObject.X_temp + m_sTrackingObject.W_temp / 2,
                   m_nImageWidth - 1);
  y_max = std::min(m_sTrackingObject.Y_temp + m_sTrackingObject.H_temp / 2,
                   m_nImageHeight - 1);

  // dimension template
  dimx = x_max - x_min + 1;
  dimy = y_max - y_min + 1;

  // dimension interpolate template
  dimx_int = dimx * 2 - 1;
  dimy_int = dimy * 2 - 1;

  // searce area
  int in = 0;
  for (u = -m_sTrackingObject.dim_temp; u <= m_sTrackingObject.dim_temp; u++) {
    if (((m_sTrackingObject.X + u - dimx / 2) < 0) ||
        ((m_sTrackingObject.X + u + dimx / 2) > m_nImageWidth - 1))
      u_att[in] = 0;
    else {
      u_att[in] = 1;
      ok_u += 1;
    }

    in++;
  }

  in = 0;
  for (v = -m_sTrackingObject.dim_temp; v <= m_sTrackingObject.dim_temp; v++) {
    if (((m_sTrackingObject.Y + v - dimy / 2) < 0) ||
        ((m_sTrackingObject.Y + v + dimy / 2) > m_nImageHeight - 1))
      v_att[in] = 0;
    else {
      v_att[in] = 1;
      ok_v += 1;
    }

    in++;
  }

  if ((ok_u > 0) && (ok_v > 0)) {
    // Interpolate template
    std::unique_ptr<ValuePixel[]> pixel_temp(
        new ValuePixel[dimx_int * dimy_int]);

    // original value
    for (int i = 0; i <= (dimx - 1); i++)
      for (int j = 0; j <= (dimy - 1); j++) {
        pixel_temp[i * dimy_int * 2 + j * 2] =
            GetPixelValues(frame_temp, x_min + i, y_min + j);
      }

    // Interpolate value row
    for (int i = 1; i < (dimx_int * dimy_int); i += 2) {
      pixel_temp[i].r = (pixel_temp[i - 1].r + pixel_temp[i + 1].r) / 2;
      pixel_temp[i].g = (pixel_temp[i - 1].g + pixel_temp[i + 1].g) / 2;
      pixel_temp[i].b = (pixel_temp[i - 1].b + pixel_temp[i + 1].b) / 2;

      if (i % dimy_int == dimy_int - 2) i += dimy_int + 1;
    }

    // Interpolate value column
    for (int i = dimy_int; i <= ((dimx_int - 1) * dimy_int - 1); i += 2) {
      pixel_temp[i].r =
          (pixel_temp[i - dimy_int].r + pixel_temp[i + dimy_int].r) / 2;
      pixel_temp[i].g =
          (pixel_temp[i - dimy_int].g + pixel_temp[i + dimy_int].g) / 2;
      pixel_temp[i].b =
          (pixel_temp[i - dimy_int].b + pixel_temp[i + dimy_int].b) / 2;

      if (i % dimy_int == dimy_int - 1) i += dimy_int - 1;
    }

    // Interpolate value central
    for (int i = dimy_int + 1; i <= ((dimx_int - 1) * dimy_int - 2); i += 2) {
      pixel_temp[i].r = UBYTE8(float((pixel_temp[i - dimy_int - 1].r +
                                      pixel_temp[i - dimy_int + 1].r +
                                      pixel_temp[i + dimy_int - 1].r +
                                      pixel_temp[i + dimy_int + 1].r) /
                                     4) +
                               0.4);

      pixel_temp[i].g = UBYTE8(float((pixel_temp[i - dimy_int - 1].g +
                                      pixel_temp[i - dimy_int + 1].g +
                                      pixel_temp[i + dimy_int - 1].g +
                                      pixel_temp[i + dimy_int + 1].g) /
                                     4) +
                               0.4);

      pixel_temp[i].b = UBYTE8(float((pixel_temp[i - dimy_int - 1].b +
                                      pixel_temp[i - dimy_int + 1].b +
                                      pixel_temp[i + dimy_int - 1].b +
                                      pixel_temp[i + dimy_int + 1].b) /
                                     4) +
                               0.4);

      if (i % dimy_int == dimy_int - 2) i += dimy_int + 1;
    }

    //--------------------------------------------------------
    short dimx_int_ric, dimy_int_ric;
    short x_min_ric, y_min_ric;

    short u_min = -m_sTrackingObject.dim_temp;
    short v_min = -m_sTrackingObject.dim_temp;

    for (int i = 0; i <= 2 * m_sTrackingObject.dim_temp; i++)
      if (u_att[i] == 1)
        break;
      else
        u_min++;

    x_min_ric = m_sTrackingObject.X + u_min - dimx / 2;

    for (int i = 0; i <= 2 * m_sTrackingObject.dim_temp; i++)
      if (v_att[i] == 1)
        break;
      else
        v_min++;

    y_min_ric = m_sTrackingObject.Y + v_min - dimy / 2;

    // Effective Dimension searce interpolate area
    dimx_int_ric = ((dimx + ok_u - 1) * 2 - 1);
    dimy_int_ric = ((dimy + ok_v - 1) * 2 - 1);

    std::unique_ptr<ValuePixel[]> area_ricerca(
        new ValuePixel[dimx_int_ric * dimy_int_ric]);

    // Original value
    for (int i = 0; i <= ((dimx + ok_u - 1) - 1); i++)
      for (int j = 0; j <= ((dimy + ok_v - 1) - 1); j++) {
        area_ricerca[i * dimy_int_ric * 2 + j * 2] =
            GetPixelValues(frame, x_min_ric + i, y_min_ric + j);
      }

    // interpolate value row
    for (int i = 1; i < (dimx_int_ric * dimy_int_ric); i += 2) {
      area_ricerca[i].r = (area_ricerca[i - 1].r + area_ricerca[i + 1].r) / 2;
      area_ricerca[i].g = (area_ricerca[i - 1].g + area_ricerca[i + 1].g) / 2;
      area_ricerca[i].b = (area_ricerca[i - 1].b + area_ricerca[i + 1].b) / 2;

      if (i % dimy_int_ric == dimy_int_ric - 2) i += dimy_int_ric + 1;
    }

    // Interpolate value column
    for (int i = dimy_int_ric; i <= ((dimx_int_ric - 1) * dimy_int_ric - 1);
         i += 2) {
      area_ricerca[i].r = (area_ricerca[i - dimy_int_ric].r +
                           area_ricerca[i + dimy_int_ric].r) /
                          2;
      area_ricerca[i].g = (area_ricerca[i - dimy_int_ric].g +
                           area_ricerca[i + dimy_int_ric].g) /
                          2;
      area_ricerca[i].b = (area_ricerca[i - dimy_int_ric].b +
                           area_ricerca[i + dimy_int_ric].b) /
                          2;

      if (i % dimy_int_ric == dimy_int_ric - 1) i += dimy_int_ric - 1;
    }

    // Interpolate value central
    for (int i = dimy_int_ric + 1; i <= ((dimx_int_ric - 1) * dimy_int_ric - 2);
         i += 2) {
      area_ricerca[i].r = UBYTE8(float((area_ricerca[i - dimy_int_ric - 1].r +
                                        area_ricerca[i - dimy_int_ric + 1].r +
                                        area_ricerca[i + dimy_int_ric - 1].r +
                                        area_ricerca[i + dimy_int_ric + 1].r) /
                                       4) +
                                 0.4);

      area_ricerca[i].g = UBYTE8(float((area_ricerca[i - dimy_int_ric - 1].g +
                                        area_ricerca[i - dimy_int_ric + 1].g +
                                        area_ricerca[i + dimy_int_ric - 1].g +
                                        area_ricerca[i + dimy_int_ric + 1].g) /
                                       4) +
                                 0.4);

      area_ricerca[i].b = UBYTE8(float((area_ricerca[i - dimy_int_ric - 1].b +
                                        area_ricerca[i - dimy_int_ric + 1].b +
                                        area_ricerca[i + dimy_int_ric - 1].b +
                                        area_ricerca[i + dimy_int_ric + 1].b) /
                                       4) +
                                 0.4);

      if (i % dimy_int_ric == dimy_int_ric - 2) i += dimy_int_ric + 1;
    }

    short u_max = m_sTrackingObject.dim_temp;
    short v_max = m_sTrackingObject.dim_temp;

    for (int i = 2 * m_sTrackingObject.dim_temp; i >= 0; i--)
      if (u_att[i] == 1)
        break;
      else
        u_max--;

    for (int i = 2 * m_sTrackingObject.dim_temp; i >= 0; i--)
      if (v_att[i] == 1)
        break;
      else
        v_max--;

    unsigned long indt, indc;

    std::unique_ptr<float[]> mat_dist(
        new float[(2 * ok_u - 1) * (2 * ok_v - 1)]);
    float att_dist_cent = MAX_FLOAT;
    float dist_cent;

    // Distance
    for (u = 2 * u_min; u <= 2 * u_max; u++) {
      for (v = 2 * v_min; v <= 2 * v_max; v++) {
        dist = 0.0;
        //
        for (x = 0; x < dimx_int; x++)
          for (y = 0; y < dimy_int; y++) {
            // T(x,y)

            indt = x * dimy_int + y;

            // L(x+u,y+v)

            indc =
                ((x + (u - 2 * u_min))) * dimy_int_ric + (y + (v - 2 * v_min));

            dist += (pixel_temp[indt].r - area_ricerca[indc].r) *
                        (pixel_temp[indt].r - area_ricerca[indc].r) +
                    (pixel_temp[indt].g - area_ricerca[indc].g) *
                        (pixel_temp[indt].g - area_ricerca[indc].g) +
                    (pixel_temp[indt].b - area_ricerca[indc].b) *
                        (pixel_temp[indt].b - area_ricerca[indc].b);
          }
        dist = sqrt(dist) / (sqrt(float(dimx_int * dimy_int * 3 * 255 * 255)));

        mat_dist[(u - 2 * u_min) * (2 * ok_v - 1) + (v - 2 * v_min)] = dist;

        if (dist < min_dist) {
          min_dist      = dist;
          u_sup         = u;
          v_sup         = v;
          att_dist_cent = sqrt(float(u * u + v * v));
        }

        else if (dist == min_dist) {
          dist_cent = sqrt(float(u * u + v * v));
          if (dist_cent < att_dist_cent) {
            min_dist      = dist;
            u_sup         = u;
            v_sup         = v;
            att_dist_cent = dist_cent;
          }
        }
      }
    }

    if (!man_occlusion) {
      if (min_dist > m_sTrackingObject.threshold_distance) {
        track               = false;
        m_sTrackingObject.X = m_sTrackingObject.X_old;
        m_sTrackingObject.Y = m_sTrackingObject.Y_old;
        m_visible           = "INVISIBLE";
      } else {
        track = true;
        if (min_dist > m_sTrackingObject.threshold_distance_B) {
          m_visible = "WARNING";
          m_K_dist  = floor(
              (double)(min_dist - m_sTrackingObject.threshold_distance_B) /
              (m_sTrackingObject.threshold_distance -
               m_sTrackingObject.threshold_distance_B));
        } else {
          m_visible = "VISIBLE";
        }
      }
    }

    if (track) {
      // half pixel
      m_sTrackingObject.half_pixelx = 0;
      m_sTrackingObject.half_pixely = 0;

      if (u_sup % 2 != 0)
        if (v_sup % 2 != 0) {
          if (mat_dist[(u_sup - 2 * u_min - 1) * (2 * ok_v - 1) +
                       (v_sup - 2 * v_min - 1)] <
              mat_dist[(u_sup - 2 * u_min - 1) * (2 * ok_v - 1) +
                       (v_sup - 2 * v_min + 1)])

            if (mat_dist[(u_sup - 2 * u_min - 1) * (2 * ok_v - 1) +
                         (v_sup - 2 * v_min - 1)] <
                mat_dist[(u_sup - 2 * u_min + 1) * (2 * ok_v - 1) +
                         (v_sup - 2 * v_min - 1)])

              if (mat_dist[(u_sup - 2 * u_min - 1) * (2 * ok_v - 1) +
                           (v_sup - 2 * v_min - 1)] <
                  mat_dist[(u_sup - 2 * u_min + 1) * (2 * ok_v - 1) +
                           (v_sup - 2 * v_min + 1)]) {
                m_sTrackingObject.half_pixelx = -1;
                m_sTrackingObject.half_pixely = -1;
                u_sup -= 1;
                v_sup -= 1;
              } else {
                m_sTrackingObject.half_pixelx = 1;
                m_sTrackingObject.half_pixely = 1;
                u_sup += 1;
                v_sup += 1;
              }
            else if (mat_dist[(u_sup - 2 * u_min + 1) * (2 * ok_v - 1) +
                              (v_sup - 2 * v_min - 1)] <
                     mat_dist[(u_sup - 2 * u_min + 1) * (2 * ok_v - 1) +
                              (v_sup - 2 * v_min + 1)]) {
              m_sTrackingObject.half_pixelx = 1;
              m_sTrackingObject.half_pixely = -1;
              u_sup += 1;
              v_sup -= 1;
            } else {
              m_sTrackingObject.half_pixelx = 1;
              m_sTrackingObject.half_pixely = 1;
              u_sup += 1;
              v_sup += 1;
            }
          else if (mat_dist[(u_sup - 2 * u_min - 1) * (2 * ok_v - 1) +
                            (v_sup - 2 * v_min + 1)] <
                   mat_dist[(u_sup - 2 * u_min + 1) * (2 * ok_v - 1) +
                            (v_sup - 2 * v_min - 1)])

            if (mat_dist[(u_sup - 2 * u_min - 1) * (2 * ok_v - 1) +
                         (v_sup - 2 * v_min + 1)] <
                mat_dist[(u_sup - 2 * u_min + 1) * (2 * ok_v - 1) +
                         (v_sup - 2 * v_min + 1)]) {
              m_sTrackingObject.half_pixelx = -1;
              m_sTrackingObject.half_pixely = 1;
              u_sup -= 1;
              v_sup += 1;
            } else {
              m_sTrackingObject.half_pixelx = 1;
              m_sTrackingObject.half_pixely = 1;
              u_sup += 1;
              v_sup += 1;
            }
          else if (mat_dist[(u_sup - 2 * u_min + 1) * (2 * ok_v - 1) +
                            (v_sup - 2 * v_min - 1)] <
                   mat_dist[(u_sup - 2 * u_min + 1) * (2 * ok_v - 1) +
                            (v_sup - 2 * v_min + 1)]) {
            m_sTrackingObject.half_pixelx = 1;
            m_sTrackingObject.half_pixely = -1;

            u_sup += 1;
            v_sup -= 1;
          } else {
            m_sTrackingObject.half_pixelx = 1;
            m_sTrackingObject.half_pixely = 1;
            u_sup += 1;
            v_sup += 1;
          }
        } else {
          if (mat_dist[(u_sup - 2 * u_min - 1) * (2 * ok_v - 1) +
                       (v_sup - 2 * v_min)] <
              mat_dist[(u_sup - 2 * u_min + 1) * (2 * ok_v - 1) +
                       (v_sup - 2 * v_min)]) {
            m_sTrackingObject.half_pixelx = -1;
            m_sTrackingObject.half_pixely = 0;
            u_sup -= 1;
          } else {
            m_sTrackingObject.half_pixelx = 1;
            m_sTrackingObject.half_pixely = 0;
            u_sup += 1;
          }
        }
      else if (v_sup % 2 != 0) {
        if (mat_dist[(u_sup - 2 * u_min) * (2 * ok_v - 1) +
                     (v_sup - 2 * v_min - 1)] <
            mat_dist[(u_sup - 2 * u_min) * (2 * ok_v - 1) +
                     (v_sup - 2 * v_min + 1)]) {
          m_sTrackingObject.half_pixelx = 0;
          m_sTrackingObject.half_pixely = -1;
          v_sup -= 1;
        } else {
          m_sTrackingObject.half_pixelx = 0;
          m_sTrackingObject.half_pixely = 1;
          v_sup += 1;
        }
      }

      m_sTrackingObject.X += u_sup / 2;
      m_sTrackingObject.Y += v_sup / 2;
    }
  }

  return min_dist;
}

//-------------------------------------------------------------------------------------------------------
// Update characteristic neighbours
void CObjectTracker::DistanceAndUpdate(NEIGHBOUR position) {
  float x0 = m_sTrackingObject.X;
  float y0 = m_sTrackingObject.Y;
  float x1 = position.X_old, y1 = position.Y_old;
  float dist2;
  dist2 = (x0 - x1) * (x0 - x1) + (y0 - y1) * (y0 - y1);

  // searce free position
  for (int i = 0; i < 30; i++) {
    if ((neighbours[i].X == -1) && (neighbours[i].Y == -1)) {
      neighbours[i].X     = position.X;
      neighbours[i].Y     = position.Y;
      neighbours[i].dist2 = dist2;
      neighbours[i].X_old = position.X_old;
      neighbours[i].Y_old = position.Y_old;
      return;
    }
  }

  // sostitution based on distance
  int i = 0;
  for (i = 0; i < 30; i++) {
    if (dist2 < neighbours[i].dist2) {
      neighbours[i].X     = position.X;
      neighbours[i].Y     = position.Y;
      neighbours[i].dist2 = dist2;
      neighbours[i].X_old = position.X_old;
      neighbours[i].Y_old = position.Y_old;
      return;
    }
  }
}

//--------------------------------------------------------------------------------------------------------
// Set position by neighbours
void CObjectTracker::SetPositionByNeighbours(void) {
  if (!track) {
    float x[3], y[3], d2[3];
    for (int i = 0; i < 30; i++) {
      //((X,Y)=(-1,-1) => neighbours not valid)
      if ((neighbours[i].X == -1) && (neighbours[i].Y == -1)) {
        m_sTrackingObject.X = -1;
        m_sTrackingObject.Y = -1;
        return;
      } else {
        x[i]  = neighbours[i].X;
        y[i]  = neighbours[i].Y;
        d2[i] = neighbours[i].dist2;
      }
    }

    // error: neighbours aligns
    if ((y[1] - y[0]) * (x[2] - x[0]) == (y[2] - y[0]) * (x[1] - x[0])) {
      m_sTrackingObject.X = -1;
      m_sTrackingObject.Y = -1;
      return;
    }

    // old neighbours coordinate
    float x_old[3], y_old[3];
    int i = 0;
    for (i = 0; i < 3; i++) {
      x_old[i] = neighbours[i].X_old;
      y_old[i] = neighbours[i].Y_old;
    }

    //
    float dn2[3], dn_old2[3];

    // distance
    for (i = 0; i < 2; i++)
      for (int j = i + 1; j < 3; j++) {
        dn_old2[i + j - 1] = (x_old[i] - x_old[j]) * (x_old[i] - x_old[j]) +
                             (y_old[i] - y_old[j]) * (y_old[i] - y_old[j]);
        dn2[i + j - 1] =
            (x[i] - x[j]) * (x[i] - x[j]) + (y[i] - y[j]) * (y[i] - y[j]);
      }

    // scale factor (^2)
    float scale = ((dn2[0] / dn_old2[0]) + (dn2[1] / dn_old2[1]) +
                   (dn2[2] / dn_old2[2])) /
                  3.0;

    // update distance
    for (i = 0; i < 3; i++) d2[i] *= scale;

    // new location
    float A[2][2], b[2];
    A[0][0] = 2 * (x[1] - x[0]);
    A[0][1] = 2 * (y[1] - y[0]);
    A[1][0] = 2 * (x[2] - x[0]);
    A[1][1] = 2 * (y[2] - y[0]);
    b[0] =
        d2[0] - d2[1] + x[1] * x[1] - x[0] * x[0] + y[1] * y[1] - y[0] * y[0];
    b[1] =
        d2[0] - d2[2] + x[2] * x[2] - x[0] * x[0] + y[2] * y[2] - y[0] * y[0];
    float detA, invA[2][2];
    detA       = A[0][0] * A[1][1] - A[0][1] * A[1][0];
    invA[0][0] = A[1][1] / detA;
    invA[0][1] = -A[0][1] / detA;
    invA[1][0] = -A[1][0] / detA;
    invA[1][1] = A[0][0] / detA;
    float X, Y;
    X = invA[0][0] * b[0] + invA[0][1] * b[1];
    Y = invA[1][0] * b[0] + invA[1][1] * b[1];

    m_sTrackingObject.X = short(X + 0.49999);
    m_sTrackingObject.Y = short(Y + 0.49999);

    m_sTrackingObject.half_pixelx = 0;
    m_sTrackingObject.half_pixely = 0;

    // error
    if (m_sTrackingObject.X < 0) {
      //			m_sTrackingObject.X = 0;
      m_visible = "INVISIBLE";
    }
    if (m_sTrackingObject.X > (m_nImageWidth - 1)) {
      //			m_sTrackingObject.X = m_nImageWidth - 1;
      m_visible = "INVISIBLE";
    }
    if (m_sTrackingObject.Y < 0) {
      //			m_sTrackingObject.Y = 0;
      m_visible = "INVISIBLE";
    }
    if (m_sTrackingObject.Y > (m_nImageHeight - 1)) {
      //			m_sTrackingObject.Y = m_nImageHeight - 1;
      m_visible = "INVISIBLE";
    }
  }
}

//--------------------------------------------------------------------------------------------------------
// Write on output file
void CObjectTracker::WriteOnOutputFile(char *filename) {
  ofstream output;
  output.open(filename, ios_base::app);
  output << m_sTrackingObject.X << " " << m_sTrackingObject.Y << endl;
  output << m_sTrackingObject.W << " " << m_sTrackingObject.H << endl;
  output << m_sTrackingObject.half_pixelx << " "
         << m_sTrackingObject.half_pixely << endl;
  output << m_visible << endl;
  output.close();
  output.open("output_center.txt", ios_base::app);
  output << "Coordinate del punto - X : "
         << m_sTrackingObject.X - ((m_nImageWidth - 1) / 2)
         << " - Y : " << m_sTrackingObject.Y - ((m_nImageHeight - 1) / 2)
         << endl;
  output << "Larghezza dell'area : " << m_sTrackingObject.W
         << " - Altezza dell'aera : " << m_sTrackingObject.H << endl;
  output << "Mezzo pixel : " << m_sTrackingObject.half_pixelx << " "
         << m_sTrackingObject.half_pixely << endl;
  output << "Visibilità : " << m_visible << endl;
  output.close();
}

//--------------------------------------------------------------------------------------------------------
// Reset distance
void CObjectTracker::DistanceReset(void) {
  for (int i = 0; i < 30; i++) {
    neighbours[i].X     = -1;
    neighbours[i].Y     = -1;
    neighbours[i].dist2 = MAX_FLOAT;
  }
}

//--------------------------------------------------------------------------------------------------------
// Get position neighbours
NEIGHBOUR CObjectTracker::GetPosition(void) {
  NEIGHBOUR _neighbour;

  _neighbour.X     = m_sTrackingObject.X;
  _neighbour.Y     = m_sTrackingObject.Y;
  _neighbour.dist2 = MAX_FLOAT;
  _neighbour.X_old = m_sTrackingObject.X_old;
  _neighbour.Y_old = m_sTrackingObject.Y_old;

  return _neighbour;
}

// Set position of the object
void CObjectTracker::SetPosition(short x, short y) {
  if (x < 0) {
    //		x = 0;
    m_visible = "INVISIBLE";
  } else if (x > (m_nImageWidth - 1)) {
    //		x = m_nImageWidth - 1;
    m_visible = "INVISIBLE";
  }
  if (y < 0) {
    //		y = 0;
    m_visible = "INVISIBLE";
  } else if (y > (m_nImageHeight - 1)) {
    //		y = m_nImageHeight - 1;
    m_visible = "INVISIBLE";
  }

  m_sTrackingObject.X = x;
  m_sTrackingObject.Y = y;

  m_sTrackingObject.half_pixelx = 0;
  m_sTrackingObject.half_pixely = 0;
}

// Set the object as initialized
void CObjectTracker::SetInit(bool status) { m_initialized = status; }

// Get if the object is initialized
bool CObjectTracker::GetInit() { return m_initialized; }

std::string CObjectTracker::GetVisibility() { return m_visible; }

void CObjectTracker::SetVisibility(string visibility) {
  m_visible = visibility;
}

// Set initials position of neighbours
void CObjectTracker::SetInitials(NEIGHBOUR position) {
  initial.x = position.X;
  initial.y = position.Y;
}

// Get initials position of neighbours
Predict3D::Point CObjectTracker::GetInitials() { return initial; }

// Get the K_Dist
int CObjectTracker::GetKDist() { return m_K_dist; }

// End
