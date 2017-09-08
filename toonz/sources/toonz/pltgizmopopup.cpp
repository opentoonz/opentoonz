

#include "pltgizmopopup.h"

// Tnz6 includes
#include "menubarcommandids.h"
#include "tapp.h"

// TnzQt includes
#include "toonzqt/menubarcommand.h"
#include "toonzqt/styleselection.h"
#include "toonzqt/tselectionhandle.h"
#include "historytypes.h"

// TnzLib includes
#include "toonz/tpalettehandle.h"
#include "toonz/palettecontroller.h"

// TnzCore includes
#include "tundo.h"
#include "tcolorstyles.h"
#include "tpixelutils.h"

// Qt includes
#include <QHBoxLayout>
#include <QLabel>
#include <QPushButton>
#include <QAction>
#include <QMainWindow>
#include <QGroupBox>
#include <QMessageBox>

using namespace DVGui;

//-----------------------------------------------------------------------------

static void getStyles(std::vector<TColorStyle *> &styles,
                      const TStyleSelection &selection, TPaletteP palette) {
  styles.clear();
  int pageIndex        = selection.getPageIndex();
  TPalette::Page *page = palette->getPage(pageIndex);
  if (!page) return;
  std::set<int> indices = selection.getIndicesInPage();
  // non si puo' modificare il BG
  if (pageIndex == 0) indices.erase(0);
  styles.reserve(indices.size());
  for (std::set<int>::iterator it = indices.begin(); it != indices.end(); ++it)
    styles.push_back(page->getStyle(*it));
}

//-----------------------------------------------------------------------------

static void getStyleIds(std::vector<int> &styleIds,
                        const TStyleSelection &selection) {
  styleIds.clear();
  int pageIndex        = selection.getPageIndex();
  TPaletteP palette    = selection.getPalette();
  TPalette::Page *page = palette->getPage(pageIndex);
  if (!page) return;
  std::set<int> indices = selection.getIndicesInPage();
  // non si puo' modificare il BG
  if (pageIndex == 0) indices.erase(0);
  styleIds.reserve(indices.size());
  for (std::set<int>::iterator it = indices.begin(); it != indices.end(); ++it)
    styleIds.push_back(page->getStyleId(*it));
}

//=============================================================================
// GizmoUndo
//-----------------------------------------------------------------------------

class GizmoUndo final : public TUndo {
  TStyleSelection m_selection;
  std::vector<TPixel32> m_oldColors, m_newColors;

  std::vector<bool> m_oldEditedFlags, m_newEditedFlags;
  TPaletteP m_palette;

public:
  GizmoUndo(const TStyleSelection &selection)
      : m_selection(selection), m_palette(selection.getPalette()) {
    getColors(m_oldColors, m_oldEditedFlags);
  }

  ~GizmoUndo() {}

  int getSize() const override {
    return sizeof *this +
           (m_oldColors.size() + m_newColors.size()) * sizeof(TPixel32);
  }

  void onAdd() override { getColors(m_newColors, m_newEditedFlags); }

  void getColors(std::vector<TPixel32> &colors,
                 std::vector<bool> &flags) const {
    std::vector<TColorStyle *> styles;
    getStyles(styles, m_selection, m_palette);
    colors.resize(styles.size());
    flags.resize(styles.size());
    for (int i = 0; i < (int)styles.size(); i++) {
      colors[i] = styles[i]->getMainColor();
      flags[i]  = styles[i]->getIsEditedFlag();
    }
  }

  void setColors(const std::vector<TPixel32> &colors,
                 const std::vector<bool> &flags) const {
    std::vector<TColorStyle *> styles;
    getStyles(styles, m_selection, m_palette);
    int n = std::min(styles.size(), colors.size());
    for (int i = 0; i < n; i++) {
      QString gname = QString::fromStdWString(styles[i]->getGlobalName());
      if (!gname.isEmpty() && gname[0] != L'-') continue;
      styles[i]->setMainColor(colors[i]);
      styles[i]->setIsEditedFlag(flags[i]);
      styles[i]->invalidateIcon();
    }
    /*-- m_palette が currentPaletteでない可能性があるため、DirtyFlagは立てない
     * --*/
    TApp::instance()
        ->getPaletteController()
        ->getCurrentPalette()
        ->notifyColorStyleChanged(false, false);
  }

  void undo() const override { setColors(m_oldColors, m_oldEditedFlags); }
  void redo() const override { setColors(m_newColors, m_newEditedFlags); }

  QString getHistoryString() override {
    QString str =
        QObject::tr("Palette Gizmo  %1")
            .arg(QString::fromStdWString(m_palette->getPaletteName()));

    TPalette::Page *page = m_palette->getPage(m_selection.getPageIndex());
    if (!page) return str;

    str.append("  (");
    std::set<int> indices = m_selection.getIndicesInPage();
    for (std::set<int>::iterator it = indices.begin(); it != indices.end();
         ++it)
      str.append(QString("#%1, ").arg(page->getStyleId(*it)));
    str.chop(2);
    str.append(")");
    return str;
  }
  int getHistoryType() override { return HistoryType::Palette; }
};

//=============================================================================
namespace {
//-----------------------------------------------------------------------------

//=============================================================================
// TransparencyModifier
//-----------------------------------------------------------------------------

class TransparencyModifier {
  int m_delta;

public:
  TransparencyModifier(int delta) : m_delta(delta) {}
  TPixel32 f(TPixel32 color) const {
    int tmpmatte = color.m;
    tmpmatte += (m_delta * tmpmatte) / 100;
    if (tmpmatte < 0)
      tmpmatte = 0;
    else if (tmpmatte > 255)
      tmpmatte = 255;
    color.m    = tmpmatte;
    return color;
  }
};

//=============================================================================
// TransparencyShifter
//-----------------------------------------------------------------------------
class TransparencyShifter {
  int m_delta;

public:
  TransparencyShifter(int delta) : m_delta(delta) {}
  TPixel32 f(TPixel32 color) const {
    int tmpmatte = color.m;
    tmpmatte += m_delta;
    if (tmpmatte < 0)
      tmpmatte = 0;
    else if (tmpmatte > 255)
      tmpmatte = 255;
    color.m    = tmpmatte;
    return color;
  }
};

//=============================================================================
// HueModifier
//-----------------------------------------------------------------------------

class HueModifier {
  int m_delta;

public:
  HueModifier(int delta) : m_delta(delta) {}
  TPixel32 f(TPixel32 color) const {
    int hsv[3];
    rgb2hsv(hsv, color, 65535);
    hsv[0] += (m_delta * hsv[0]) / 100;
    hsv2rgb(color, hsv, 65535);
    return color;
  }
};

//=============================================================================
// HueShifter
//-----------------------------------------------------------------------------

class HueShifter {
  int m_delta;

public:
  HueShifter(int delta) : m_delta(delta) {}
  TPixel32 f(TPixel32 color) const {
    TPixelRGBM32::Channel matte = color.m;
    int hsv[3];
    rgb2hsv(hsv, color, 360);
    hsv[0] += m_delta;
    if (hsv[0] < 0)
      hsv[0] += 360;
    else if (hsv[0] > 359)
      hsv[0] -= 360;
    hsv2rgb(color, hsv, 360);

    color.m = matte;

    return color;
  }
};

//=============================================================================
// LuminanceModifier
//-----------------------------------------------------------------------------

class LuminanceModifier {
  int m_delta;

public:
  LuminanceModifier(int delta) : m_delta(delta) {}
  TPixel32 f(TPixel32 color) const {
    TPixelRGBM32::Channel matte = color.m;
    int hsv[3];
    rgb2hsv(hsv, color, 65535);
    hsv[2] += (m_delta * hsv[2]) / 100;
    hsv2rgb(color, hsv, 65535);

    color.m = matte;

    return color;
  }
};

//=============================================================================
// Luminance(Value)Shifter
//-----------------------------------------------------------------------------

class LuminanceShifter {
  int m_delta;

public:
  LuminanceShifter(int delta) : m_delta(delta) {}
  TPixel32 f(TPixel32 color) const {
    TPixelRGBM32::Channel matte = color.m;

    int hsv[3];
    rgb2hsv(hsv, color, 100);
    hsv[2] += m_delta;
    if (hsv[2] < 0)
      hsv[2] = 0;
    else if (hsv[2] > 100)
      hsv[2] = 100;
    hsv2rgb(color, hsv, 100);

    color.m = matte;

    return color;
  }
};

//=============================================================================
// SaturationModifier
//-----------------------------------------------------------------------------

class SaturationModifier {
  int m_delta;

public:
  SaturationModifier(int delta) : m_delta(delta) {}
  TPixel32 f(TPixel32 color) const {
    TPixelRGBM32::Channel matte = color.m;
    int hsv[3];
    rgb2hsv(hsv, color, 65535);
    hsv[1] += (m_delta * hsv[1]) / 100;
    hsv2rgb(color, hsv, 65535);
    color.m = matte;
    return color;
  }
};

//=============================================================================
// 追加iwasawa SaturationShifter
//-----------------------------------------------------------------------------

class SaturationShifter {
  int m_delta;

public:
  SaturationShifter(int delta) : m_delta(delta) {}
  TPixel32 f(TPixel32 color) const {
    TPixelRGBM32::Channel matte = color.m;

    int hsv[3];
    rgb2hsv(hsv, color, 100);
    hsv[1] += m_delta;
    if (hsv[1] < 0)
      hsv[1] = 0;
    else if (hsv[1] > 100)
      hsv[1] = 100;
    hsv2rgb(color, hsv, 100);

    color.m = matte;

    return color;
  }
};

//=============================================================================
// FadeModifier
//-----------------------------------------------------------------------------

class FadeModifier {
  TPixel32 m_target;
  int m_delta;

public:
  FadeModifier(TPixel32 target, int delta) : m_target(target), m_delta(delta) {}
  TPixel32 f(TPixel32 color) const {
    color.r =
        tcrop((m_target.r * m_delta + color.r * (100 - m_delta)) / 100, 0, 255);
    color.g =
        tcrop((m_target.g * m_delta + color.g * (100 - m_delta)) / 100, 0, 255);
    color.b =
        tcrop((m_target.b * m_delta + color.b * (100 - m_delta)) / 100, 0, 255);
    color.m =
        tcrop((m_target.m * m_delta + color.m * (100 - m_delta)) / 100, 0, 255);
    return color;
  }
};

//=============================================================================

template <class T>
void modifyColor(const T &modifier) {
  TPaletteHandle *paletteHandle =
      TApp::instance()->getPaletteController()->getCurrentLevelPalette();
  TPaletteP palette = paletteHandle->getPalette();
  if (palette->isLocked()) {
    QMessageBox::warning(0, QObject::tr("Warning"),
                         QObject::tr("Palette is locked."));
    return;
  }

  bool areAllStyleLincked = true;
  std::vector<int> styleIds;
  TUndo *undo;
  TStyleSelection *selection = dynamic_cast<TStyleSelection *>(
      TApp::instance()->getCurrentSelection()->getSelection());
  if (selection) {
    undo = new GizmoUndo(*selection);
    getStyleIds(styleIds, *selection);
  }
  /*-- StyleSelectionが有効でない場合はカレントStyleを用いる --*/
  else {
    TStyleSelection tmpSelection;
    tmpSelection.setPaletteHandle(paletteHandle);
    int currentIndex = paletteHandle->getStyleIndex();
    int pageIndex, styleIndexInPage;
    TPalette::Page *page = palette->getStylePage(currentIndex);
    for (int p = 0; p < palette->getPageCount(); p++) {
      if (palette->getPage(p) == page) {
        pageIndex = p;
        break;
      }
    }
    for (int s = 0; s < page->getStyleCount(); s++) {
      if (page->getStyleId(s) == currentIndex) {
        styleIndexInPage = s;
        break;
      }
    }
    tmpSelection.select(pageIndex, styleIndexInPage, true);

    undo = new GizmoUndo(tmpSelection);
    getStyleIds(styleIds, tmpSelection);
  }

  int frame = palette->getFrame();

  for (int i = 0; i < (int)styleIds.size(); i++) {
    int styleId     = styleIds[i];
    TColorStyle *cs = palette->getStyle(styleId);
    if (!cs) continue;
    std::wstring gname = cs->getGlobalName();
    if (gname != L"" && gname[0] != L'-') continue;
    areAllStyleLincked = false;
    for (int j = 0; j < cs->getColorParamCount(); j++) {
      TPixel32 color = cs->getColorParamValue(j);
      color          = modifier.f(color);
      cs->setColorParamValue(j, color);
    }
    cs->invalidateIcon();

    if (palette->isKeyframe(styleId, frame))
      palette->setKeyframe(styleId, frame);

    /*--
     * StudioPaletteStyleからのリンクがあるLevelPaletteStyleの場合、Editedフラグを立てる--*/
    if (gname != L"" && cs->getOriginalName() != L"") cs->setIsEditedFlag(true);
  }
  if (areAllStyleLincked) {
    delete undo;
    return;
  }
  TApp::instance()
      ->getPaletteController()
      ->getCurrentPalette()
      ->notifyColorStyleChanged(false);
  TUndoManager::manager()->add(undo);
}

//-----------------------------------------------------------------------------
}  // namespace
//-----------------------------------------------------------------------------

//=============================================================================
// ValueAdjuster
//-----------------------------------------------------------------------------

#if QT_VERSION >= 0x050500
ValueAdjuster::ValueAdjuster(QWidget *parent, Qt::WindowFlags flags)
#else
ValueAdjuster::ValueAdjuster(QWidget *parent, Qt::WFlags flags)
#endif
    : QWidget(parent) {
  QPushButton *plusBut  = new QPushButton(QString("+"), this);
  QPushButton *minusBut = new QPushButton(QString("-"), this);
  m_valueLineEdit =
      new DoubleLineEdit(this, 10.00);  // parent - value - min - max
  QLabel *percLabel = new QLabel(QString("%"), this);
  plusBut->setFixedSize(21, 21);
  minusBut->setFixedSize(21, 21);
  plusBut->setObjectName("GizmoButton");
  minusBut->setObjectName("GizmoButton");
  m_valueLineEdit->setRange(0, 1000);
  //----layout
  QHBoxLayout *layout = new QHBoxLayout(this);
  layout->setMargin(0);
  layout->setSpacing(1);
  {
    layout->addWidget(plusBut, 0);
    layout->addWidget(minusBut, 0);
    layout->addWidget(m_valueLineEdit, 1);
    layout->addWidget(percLabel, 0);
  }
  setLayout(layout);

  connect(plusBut, SIGNAL(clicked()), this, SLOT(onClickedPlus()));
  connect(minusBut, SIGNAL(clicked()), this, SLOT(onClickedMinus()));
}

//-----------------------------------------------------------------------------

ValueAdjuster::~ValueAdjuster() {}
//-----------------------------------------------------------------------------

void ValueAdjuster::onClickedPlus() {
  double value = m_valueLineEdit->text().toDouble();
  emit adjust(value);
}

//-----------------------------------------------------------------------------

void ValueAdjuster::onClickedMinus() {
  double value = m_valueLineEdit->text().toDouble();
  emit adjust(-value);
}

//=============================================================================
// ValueShifter
//-----------------------------------------------------------------------------

#if QT_VERSION >= 0x050500
ValueShifter::ValueShifter(bool isHue, QWidget *parent, Qt::WindowFlags flags)
#else
ValueShifter::ValueShifter(bool isHue, QWidget *parent, Qt::WFlags flags)
#endif
    : QWidget(parent) {
  QPushButton *plusBut  = new QPushButton(QString("+"), this);
  QPushButton *minusBut = new QPushButton(QString("-"), this);
  int maxValue          = (isHue) ? 360 : 100;
  m_valueLineEdit       = new DoubleLineEdit(this, 10.00);  // parent - value
  plusBut->setFixedSize(21, 21);
  minusBut->setFixedSize(21, 21);
  plusBut->setObjectName("GizmoButton");
  minusBut->setObjectName("GizmoButton");
  m_valueLineEdit->setRange(0, maxValue);

  //----layout
  QHBoxLayout *layout = new QHBoxLayout(this);
  layout->setMargin(0);
  layout->setSpacing(1);
  {
    layout->addWidget(plusBut, 0);
    layout->addWidget(minusBut, 0);
    layout->addWidget(m_valueLineEdit, 1);
  }
  setLayout(layout);

  connect(plusBut, SIGNAL(clicked()), this, SLOT(onClickedPlus()));
  connect(minusBut, SIGNAL(clicked()), this, SLOT(onClickedMinus()));
}

//-----------------------------------------------------------------------------

ValueShifter::~ValueShifter() {}
//-----------------------------------------------------------------------------

void ValueShifter::onClickedPlus() {
  double value = m_valueLineEdit->text().toDouble();
  emit adjust(value);
}

//-----------------------------------------------------------------------------

void ValueShifter::onClickedMinus() {
  double value = m_valueLineEdit->text().toDouble();
  emit adjust(-value);
}

//=============================================================================
// ColorFader
//-----------------------------------------------------------------------------

#if QT_VERSION >= 0x050500
ColorFader::ColorFader(QString name, QWidget *parent, Qt::WindowFlags flags)
#else
ColorFader::ColorFader(QString name, QWidget *parent, Qt::WFlags flags)
#endif
    : QWidget(parent) {
  QHBoxLayout *layout = new QHBoxLayout(this);
  layout->setMargin(0);
  layout->setSpacing(6);
  layout->setSizeConstraint(QLayout::SetFixedSize);

  QPushButton *button = new QPushButton(name, this);
  button->setFixedSize(50, WidgetHeight);
  button->setObjectName("PushButton_NoPadding");
  connect(button, SIGNAL(clicked()), this, SLOT(onClicked()));

  m_valueLineEdit = new DoubleLineEdit(this, 10.00);

  QLabel *percLabel = new QLabel(QString("%"), this);
  percLabel->setFixedWidth(15);

  layout->addWidget(button);
  layout->addWidget(m_valueLineEdit);
  layout->addWidget(percLabel);

  setLayout(layout);

  setFixedHeight(WidgetHeight);
}

//-----------------------------------------------------------------------------

ColorFader::~ColorFader() {}

//-----------------------------------------------------------------------------

void ColorFader::onClicked() {
  double value = m_valueLineEdit->text().toDouble();
  emit valueChanged(value);
}

//=============================================================================
// PltGizmoPopup
//-----------------------------------------------------------------------------

PltGizmoPopup::PltGizmoPopup()
    : Dialog(TApp::instance()->getMainWindow(), false, true, "PltGizmo") {
  setWindowTitle(tr("Palette Gizmo"));

  ValueAdjuster *luminanceValue   = new ValueAdjuster(this);
  ValueAdjuster *saturationValue  = new ValueAdjuster(this);
  ValueAdjuster *trasparancyValue = new ValueAdjuster(this);
  QPushButton *blendButton        = new QPushButton(tr("Blend"), this);
  m_colorFld = new ColorField(this, true, TPixel32(0, 0, 0, 255), 50);
  ColorFader *colorFader = new ColorFader(tr("Fade"), this);

  ValueShifter *luminanceShift   = new ValueShifter(false, this);
  ValueShifter *saturationShift  = new ValueShifter(false, this);
  ValueShifter *hueShift         = new ValueShifter(true, this);
  ValueShifter *trasparancyShift = new ValueShifter(false, this);

  QPushButton *fullMatteButton = new QPushButton(tr("Full Alpha"), this);
  QPushButton *zeroMatteButton = new QPushButton(tr("Zero Alpha"), this);

  //----layout
  m_topLayout->setMargin(5);
  m_topLayout->setSpacing(3);
  {
    QGridLayout *upperLay = new QGridLayout();
    upperLay->setMargin(0);
    upperLay->setSpacing(3);
    {
      upperLay->addWidget(new QLabel(tr("Scale (%)"), this), 0, 1);
      upperLay->addWidget(new QLabel(tr("Shift (value)"), this), 0, 2, 1, 2);

      upperLay->addWidget(new QLabel(tr("Value"), this), 1, 0);
      upperLay->addWidget(luminanceValue, 1, 1);
      upperLay->addWidget(luminanceShift, 1, 2, 1, 2);

      upperLay->addWidget(new QLabel(tr("Saturation"), this), 2, 0);
      upperLay->addWidget(saturationValue, 2, 1);
      upperLay->addWidget(saturationShift, 2, 2, 1, 2);

      upperLay->addWidget(new QLabel(tr("Hue"), this), 3, 0);
      // upperLay->addWidget(hueValue,3,1);
      upperLay->addWidget(hueShift, 3, 2, 1, 2);

      upperLay->addWidget(new QLabel(tr("Alpha"), this), 4, 0);
      upperLay->addWidget(trasparancyValue, 4, 1);
      upperLay->addWidget(trasparancyShift, 4, 2, 1, 2);

      upperLay->addWidget(blendButton, 5, 1);
      upperLay->addWidget(fullMatteButton, 5, 2);
      upperLay->addWidget(zeroMatteButton, 5, 3);
    }
    upperLay->setColumnStretch(0, 0);
    upperLay->setColumnStretch(1, 2);
    upperLay->setColumnStretch(2, 1);
    upperLay->setColumnStretch(3, 1);
    m_topLayout->addLayout(upperLay);

    QGroupBox *fadeBox   = new QGroupBox(tr("Fade to Color"), this);
    QVBoxLayout *fadeLay = new QVBoxLayout();
    fadeLay->setMargin(3);
    fadeLay->setSpacing(3);
    {
      QHBoxLayout *colorLay = new QHBoxLayout();
      colorLay->setMargin(0);
      colorLay->setSpacing(4);
      {
        colorLay->addWidget(new QLabel(tr("Color"), this), 0);
        colorLay->addWidget(m_colorFld);
        // colorLay->addStretch();
      }
      fadeLay->addLayout(colorLay);

      fadeLay->addWidget(colorFader);
    }
    fadeBox->setLayout(fadeLay);
    m_topLayout->addWidget(fadeBox);

    m_topLayout->addStretch();
  }

  resize(460, 220);

  //----signal-slot connections
  /*-- Scale の差が渡される。+10%なら0.1が渡される --*/
  connect(luminanceValue, SIGNAL(adjust(double)), SLOT(adjustV(double)));
  connect(saturationValue, SIGNAL(adjust(double)), SLOT(adjustS(double)));
  connect(trasparancyValue, SIGNAL(adjust(double)), SLOT(adjustT(double)));
  /*-- シフト用 移動値が渡される --*/
  connect(luminanceShift, SIGNAL(adjust(double)), SLOT(shiftV(double)));
  connect(saturationShift, SIGNAL(adjust(double)), SLOT(shiftS(double)));
  connect(hueShift, SIGNAL(adjust(double)), SLOT(shiftH(double)));
  connect(trasparancyShift, SIGNAL(adjust(double)), SLOT(shiftT(double)));

  connect(fullMatteButton, SIGNAL(pressed()), SLOT(fullMatte()));
  connect(zeroMatteButton, SIGNAL(pressed()), SLOT(zeroMatte()));

  connect(blendButton, SIGNAL(clicked()), this, SLOT(onBlend()));
  connect(colorFader, SIGNAL(valueChanged(double)), SLOT(onFade(double)));
}

//-----------------------------------------------------------------------------

PltGizmoPopup::~PltGizmoPopup() {}

//-----------------------------------------------------------------------------

void PltGizmoPopup::adjustV(double p) {
  modifyColor(LuminanceModifier((int)(p)));
}

//-----------------------------------------------------------------------------

void PltGizmoPopup::adjustS(double p) {
  modifyColor(SaturationModifier((int)(p)));
}

//-----------------------------------------------------------------------------

void PltGizmoPopup::adjustH(double p) { modifyColor(HueModifier((int)(p))); }

//-----------------------------------------------------------------------------

void PltGizmoPopup::adjustT(double p) {
  modifyColor(TransparencyModifier((int)(p)));
}

//-----------------------------------------------------------------------------

void PltGizmoPopup::shiftV(double p) {
  modifyColor(LuminanceShifter((int)(p)));
}
void PltGizmoPopup::shiftS(double p) {
  modifyColor(SaturationShifter((int)(p)));
}
void PltGizmoPopup::shiftH(double p) { modifyColor(HueShifter((int)(p))); }
void PltGizmoPopup::shiftT(double p) {
  modifyColor(TransparencyShifter((int)(p)));
}

//-----------------------------------------------------------------------------

void PltGizmoPopup::zeroMatte() { modifyColor(TransparencyShifter(-255)); }
void PltGizmoPopup::fullMatte() { modifyColor(TransparencyShifter(255)); }

//-----------------------------------------------------------------------------

void PltGizmoPopup::onBlend() {
  CommandManager *cmd  = CommandManager::instance();
  QAction *blendAction = cmd->getAction(MI_BlendColors);
  blendAction->trigger();
}

//-----------------------------------------------------------------------------

void PltGizmoPopup::onFade(double p) {
  TPixel32 color = m_colorFld->getColor();
  modifyColor(FadeModifier(color, (int)p));
}

//-----------------------------------------------------------------------------

OpenPopupCommandHandler<PltGizmoPopup> openPltGizmoPopup(MI_OpenPltGizmo);
