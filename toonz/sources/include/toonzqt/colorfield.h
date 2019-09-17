#pragma once

#ifndef COLORFIELD_H
#define COLORFIELD_H

#ifdef _MSC_VER
#pragma warning(disable : 4251)
#endif

#include "tcommon.h"
#include "traster.h"
#include "toonzqt/intfield.h"
#include "tspectrum.h"
#include "tcolorstyles.h"

#include <QWidget>

#undef DVAPI
#undef DVVAR
#ifdef TOONZQT_EXPORTS
#define DVAPI DV_EXPORT_API
#define DVVAR DV_EXPORT_VAR
#else
#define DVAPI DV_IMPORT_API
#define DVVAR DV_IMPORT_VAR
#endif

// forward declaration
class QSlider;
class QImage;
class QPixmap;
class TCleanupStyle;
class TPaletteHandle;

//=============================================================================

namespace DVGui {

//=============================================================================
// StyleSample
//-----------------------------------------------------------------------------

class DVAPI StyleSample final : public QWidget {
  Q_OBJECT
  QImage m_samplePixmap;
  TRaster32P m_bgRas;
  TColorStyle *m_style;  // owner
  bool m_clickEnabled;
  bool m_drawEnable;
  TPixel m_chessColor1;
  TPixel m_chessColor2;

  bool m_isEditing;

public:
  StyleSample(QWidget *parent, int sizeX, int sizeY);
  ~StyleSample();

  void enableClick(bool on) { m_clickEnabled = on; }

  void setStyle(TColorStyle &style);
  TColorStyle *getStyle() const;

  void setColor(const TPixel32 &color);
  void setChessboardColors(const TPixel32 &col1, const TPixel32 &col2);

  void setIsEditing(bool isEditing) {
    m_isEditing = isEditing;
    update();
  }
  bool isEditing() const { return m_isEditing; }

  void setEnable(bool drawEnable) { m_drawEnable = drawEnable; }
  bool isEnable() const { return m_drawEnable; }

protected:
  void paintEvent(QPaintEvent *event) override;
  void mousePressEvent(QMouseEvent *) override;
  void mouseDoubleClickEvent(QMouseEvent *event) override;

signals:
  void clicked(const TColorStyle &style);
};

//=============================================================================
// ChannelField
//-----------------------------------------------------------------------------

class DVAPI ChannelField final : public QWidget {
  Q_OBJECT

  DVGui::IntLineEdit *m_channelEdit;
  QSlider *m_channelSlider;
  int m_maxValue;

public:
  ChannelField(QWidget *parent = 0, const QString &string = "", int value = 0,
               int maxValue = 255, bool horizontal = false, int labelWidth = 13,
               int sliderWidth = -1);

  ~ChannelField() {}

  void setChannel(int value);
  int getChannel();

signals:
  void valueChanged(int value, bool isDragging);

protected slots:
  void onSliderChanged(int value);
  void onSliderReleased();
  void onEditChanged(const QString &str);
};

//=============================================================================
// ColorField
//-----------------------------------------------------------------------------

class DVAPI ColorField final : public QWidget {
  Q_OBJECT

  StyleSample *m_colorSample;
  ChannelField *m_redChannel;
  ChannelField *m_greenChannel;
  ChannelField *m_blueChannel;
  ChannelField *m_alphaChannel;

  TPixel32 m_color;

  //! If it is true editing changed are notified, setIsEditing emit
  //! editingChanged signal.
  bool m_notifyEditingChange;
  bool m_useStyleEditor;

public:
  class ColorFieldEditorController {
  public:
    ColorFieldEditorController() {}
    virtual ~ColorFieldEditorController() {}
    virtual void edit(DVGui::ColorField *colorField){};
    virtual void hide(){};
  };

  static ColorFieldEditorController *m_editorController;

  ColorField(QWidget *parent = 0, bool isAlphaActive = true,
             TPixel32 color = TPixel32(0, 0, 0, 255), int squareSize = 40,
             bool useStyleEditor = true, int sliderWidth = -1);

  ~ColorField() {}

  void setColor(const TPixel32 &color);
  TPixel32 getColor() const { return m_color; }
  void setChessboardColors(const TPixel32 &col1, const TPixel32 &col2);

  static void setEditorController(ColorFieldEditorController *editorController);
  static ColorFieldEditorController *getEditorController();

  void notifyColorChanged(const TPixel32 &color, bool isDragging) {
    emit colorChanged(color, isDragging);
  }

  void setEditingChangeNotified(bool notify) { m_notifyEditingChange = notify; }

  void setIsEditing(bool isEditing) {
    assert(m_colorSample);
    m_colorSample->setIsEditing(isEditing);
    if (m_notifyEditingChange) emit editingChanged(getColor(), isEditing);
  }
  bool isEditing() const {
    assert(m_colorSample);
    return m_colorSample->isEditing();
  }

  void hideChannelsFields(bool hide);
  void setAlphaActive(bool active);

protected:
  void updateChannels();
  void mousePressEvent(QMouseEvent *event) override;
  void mouseDoubleClickEvent(QMouseEvent *event) override;
  void hideEvent(QHideEvent *) override;

protected slots:
  void onRedChannelChanged(int value, bool isDragging);
  void onGreenChannelChanged(int value, bool isDragging);
  void onBlueChannelChanged(int value, bool isDragging);
  void onAlphaChannelChanged(int value, bool isDragging);

signals:
  void editingChanged(const TPixel32 &, bool isEditing);
  void colorChanged(const TPixel32 &, bool isDragging);
};

//=============================================================================
// CleanupColorField
//-----------------------------------------------------------------------------

class DVAPI CleanupColorField final : public QWidget {
  Q_OBJECT

  TPaletteHandle *m_ph;
  StyleSample *m_colorSample;
  ChannelField *m_brightnessChannel;
  ChannelField *m_contrastChannel;
  ChannelField *m_hRangeChannel;
  ChannelField *m_lineWidthChannel;
  ChannelField *m_cThresholdChannel;
  ChannelField *m_wThresholdChannel;

  TColorStyleP m_style;
  TCleanupStyle *m_cleanupStyle;

  bool m_greyMode;

  //! If it is true editing changed are notified, setIsEditing emit
  //! editingChanged signal.
  bool m_notifyEditingChange;

public:
  class CleanupColorFieldEditorController {
  public:
    CleanupColorFieldEditorController() {}
    virtual ~CleanupColorFieldEditorController() {}

    virtual void edit(DVGui::CleanupColorField *colorField){};
    virtual void hide(){};
  };

  static CleanupColorFieldEditorController *m_editorController;

public:
  CleanupColorField(QWidget *parent, TCleanupStyle *cleanupStyle,
                    TPaletteHandle *ph, bool greyMode);
  ~CleanupColorField() { getEditorController()->edit(0); }

  static void setEditorController(
      CleanupColorFieldEditorController *editorController);
  static CleanupColorFieldEditorController *getEditorController();

  void setEditingChangeNotified(bool notify) { m_notifyEditingChange = notify; }

  bool isEditing() const {
    assert(m_colorSample);
    return m_colorSample->isEditing();
  }
  void setIsEditing(bool isEditing) {
    assert(m_colorSample);
    m_colorSample->setIsEditing(isEditing);
    if (m_notifyEditingChange) emit editingChanged(getColor(), isEditing);
  }

  void setColor(const TPixel32 &color);
  TPixel32 getColor() const;
  void updateColor();

  void setOutputColor(const TPixel32 &color);
  TPixel32 getOutputColor() const;

  TColorStyle *getStyle() { return (TColorStyle *)m_cleanupStyle; }
  void setStyle(TColorStyle *style);

  void setContrastEnabled(bool enable);

protected:
  void mousePressEvent(QMouseEvent *event) override;
  void mouseDoubleClickEvent(QMouseEvent *event) override;
  void hideEvent(QHideEvent *) override;

protected slots:

  void onBrightnessChannelChanged(int value, bool dragging);
  void onContrastChannelChanged(int value, bool dragging);
  void onCThresholdChannelChanged(int value, bool dragging);
  void onWThresholdChannelChanged(int value, bool dragging);
  void onHRangeChannelChanged(int value, bool dragging);
  void onLineWidthChannelChanged(int value, bool dragging);

signals:

  void editingChanged(const TPixel32 &, bool isEditing);
  void StyleSelected(TCleanupStyle *);
};

//-----------------------------------------------------------------------------
}  // namespace DVGui
//-----------------------------------------------------------------------------

#endif  // COLORFIELD_H
