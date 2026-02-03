#pragma once
#ifndef TVPJSON_IO_H
#define TVPJSON_IO_H

#include "tfilepath.h"

#include <QString>
#include <QSize>
#include <QColor>
#include <QList>
#include <QPair>
#include <QStringList>

#include <iostream>

class ToonzScene;
class TFilePath;
class TXsheet;
class TXshCellColumn;
class QJsonObject;

namespace TvpJsonIo {

// Currently only supports "Color" and "Darken" brend modes
enum BlendMode {
  BlendMode_Color,
  //  BlendMode_Behind,
  //  BlendMode_Erase,
  //  BlendMode_Shade,
  //  BlendMode_Light,
  //  BlendMode_Colorize,
  //  BlendMode_Hue,
  //  BlendMode_Saturation,
  //  BlendMode_Value,
  //  BlendMode_Add,
  //  BlendMode_Sub,
  //  BlendMode_Multiply,
  //  BlendMode_Screen,
  //  BlendMode_Replace,
  //  BlendMode_Copy,
  //  BlendMode_Difference,
  //  BlendMode_Divide,
  //  BlendMode_Overlay,
  //  BlendMode_Overlay2,
  //  BlendMode_Light2,
  //  BlendMode_Shade2,
  //  BlendMode_HardLight,
  //  BlendMode_SoftLight,
  //  BlendMode_GrainExtract,
  //  BlendMode_GrainMerge,
  //  BlendMode_Sub2,
  BlendMode_Darken,
  //  BlendMode_Lighten,
  BlendModeCount
};

// "description": "information of each cell and instance",
class TvpJsonLayerLinkItem {
  // instance-index "description": "first placed frame of instance (the first
  // frame is 0)"
  int m_instance_index;
  // instance-name "description": "drawing number (starting from 1. 0 is used
  // for empty frame)"
  QString m_instance_name;
  // "description": "instance file path, relative to the scene file"
  QString m_file;
  // "description": "each start frame of the image (the first frame is 0)"
  QList<int> m_images;

public:
  void read(const QJsonObject& json);
  void write(QJsonObject& json) const;
  void build(int instance_index, QString instance_name, QString file,
             QList<int> images);
};

class TvpJsonLayer {
  // "description": "layer name (= level name)"
  QString m_name;
  // "description": "visibility"
  bool m_visible;
  // "description": "layer index (stacking order), starting from 0"
  int m_position;
  // "description": "layer opacity 0-255"
  int m_opacity;
  // "description": "start frame of the layer"
  int m_start;
  // "description": "end frame of the layer"
  int m_end;

  // pre_behavior, post_behavior : Unknown. Put 0 for now.

  // "description": "composite mode of the layers. "Color"ÅÅNormal,
  // "Darken"ÅÅDarken"
  BlendMode m_blending_mode;

  // group/red,green,blue  "description": "Unknown. Put 0 for now"
  // QColor m_group

  // "description": "information of each cell and instance",
  QList<TvpJsonLayerLinkItem> m_link;

public:
  void read(const QJsonObject& json);
  void write(QJsonObject& json) const;
  void build(int index, ToonzScene*, TXshCellColumn* column);
  bool isEmpty() { return m_link.isEmpty(); }
};

class TvpJsonClip {
  // "description": "scene name"
  QString m_name;
  // "width": "description": "camera width in pixels"
  // "height": "description": "camera height in pixels"
  QSize m_cameraSize;
  // "description": "frame rate"
  double m_framerate;
  // "description": "pixel aspect ratio"
  double m_pixelaspectratio;
  // "image-count": "description": "scene frame length"
  int m_image_count;
  // bg/red,green,blue  "description": "camera BG colorÅi0-255 for each
  // channelÅj"
  QColor m_bg;

  // camera : camera information. Ignore it for now.

  // markin, markout : Ignore for now.

  //"description": "layer information"
  QList<TvpJsonLayer> m_layers;

  // repeat : Ignore for now.

public:
  void read(const QJsonObject& json);
  void write(QJsonObject& json) const;
  bool isEmpty() { return m_layers.isEmpty(); }
  void build(ToonzScene*, TXsheet*);
};

class TvpJsonProject {
  // format/extension  "description": "Output format extension??"
  QString m_formatExtension;
  //  "description": "scene information"
  TvpJsonClip m_clip;

public:
  void read(const QJsonObject& json);
  void write(QJsonObject& json) const;
  void build(ToonzScene*, TXsheet*);
  bool isEmpty() { return m_clip.isEmpty(); }
};

class TvpJsonVersion {
  // "description": "major version"
  int m_major;
  //  "description": "minor version"
  int m_minor;

public:
  void read(const QJsonObject& json);
  void write(QJsonObject& json) const;
  void setVersion(int, int);
};

class TvpJsonData {
  // "description": "version number of this tvp-json format",
  TvpJsonVersion m_version;
  // "description": "project information"
  TvpJsonProject m_project;

public:
  void read(const QJsonObject& json);
  void write(QJsonObject& json) const;
  void build(ToonzScene*, TXsheet*);
  bool isEmpty() { return m_project.isEmpty(); }
};

}  // namespace TvpJsonIo

#endif