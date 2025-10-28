#pragma once

#ifndef VIEWERDRAW_INCLUDED
#define VIEWERDRAW_INCLUDED

//
// funzioni per il disegno (OpenGL) di oggetti
// che si vedono nel viewer
//

#include <vector>
#include "tgeometry.h"
#include "tfilepath.h"
#include "tsystem.h"
#include "tenv.h"
#include "toonz/txshsimplelevel.h"
#include <qvariant.h>

// forward declaration
class SceneViewer;
class Ruler;
class TAffine;

//=============================================================================
// ViewerDraw
//-----------------------------------------------------------------------------

namespace ViewerDraw {

enum {  // cfr drawCamera()
  CAMERA_REFERENCE = 0X1,
  CAMERA_3D        = 0X2,
  SAFE_AREA        = 0X4,
  SOLID_LINE       = 0X8,
  SUBCAMERA        = 0X10
};

struct FramesDef {
  QList<double> m_params;  // [width, height, r, g, b]

  QString toString() const {
    QStringList parts;
    for (int i = 0; i < m_params.size(); ++i) {
      if (i < 2)  // Width/Height (Percentage)
        parts << QString::number(m_params[i], 'f', 2);
      else  // R, G, B (Integer 0-255)
        parts << QString::number(m_params[i], 'f', 0);
    }
    return parts.join(", ");
  }

  static FramesDef fromString(const QString& str) {
    FramesDef def;
    QStringList parts = str.split(',');
    for (const QString& part : parts) {
      bool ok;
      double val = part.trimmed().toDouble(&ok);
      if (ok) def.m_params.append(val);
    }
    return def;
  }

  static FramesDef fromVariantList(const QList<QVariant>& list) {
    FramesDef def;
    for (const QVariant& v : list) def.m_params.append(v.toDouble());
    return def;
  }

  QList<QVariant> toVariantList() const {
    QList<QVariant> list;
    for (double p : m_params) list.append(p);
    return list;
  }
};

struct FramesPreset {
  QString m_name         = QString();
  QString m_layoutPath   = QString();
  double m_layoutOffsetX = 0;
  double m_layoutOffsetY = 0;
  QList<FramesDef> m_areas;
};

extern FramesPreset previewFramesPreset;

TRectD getCameraRect();

void drawCameraMask(SceneViewer* viewer);
void drawGridAndGuides(SceneViewer* viewer, double viewerScale, Ruler* vRuler,
                       Ruler* hRuler, bool gridEnabled);

void draw3DCamera(unsigned long flags, double zmin, double phi);
void drawCamera(unsigned long flags, double pixelSize);

void draw3DFrame(double zmin, double phi);
void drawDisk(int& tableDLId);
void drawFieldGuide();
void drawColorcard(UCHAR channel);

void drawFrames(SceneViewer* viewer, bool levelEditing);

unsigned int createDiskDisplayList();

void getFramesPreset(QList<QList<double>>& _sizeList, TXshSimpleLevel** _slP,
                     TPointD* _offsetP);

static inline TFilePath getFramesIniPath() {
  TFilePath fp         = TEnv::getConfigDir();
  std::string fileName = "framespresets.ini";
  TFilePath searchPath = fp;

  while (!TFileStatus(searchPath + fileName).doesExist() &&
         !searchPath.isRoot() && searchPath.getParentDir() != TFilePath()) {
    searchPath = searchPath.getParentDir();
  }

  if (!TFileStatus(searchPath + fileName).doesExist()) {
    fileName   = "safearea.ini";
    searchPath = fp;

    while (!TFileStatus(searchPath + fileName).doesExist() &&
           !searchPath.isRoot() && searchPath.getParentDir() != TFilePath()) {
      searchPath = searchPath.getParentDir();
    }
  }

  if (!TFileStatus(searchPath + fileName).doesExist()) {
    fp = fp + "framespresets.ini";
    TSystem::touchFile(fp);
  } else
    fp = searchPath + fileName;

  return fp;
}

}  // namespace ViewerDraw

#endif
