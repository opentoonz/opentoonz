

#include "toonzqt/fxsettings.h"
#include "toonzqt/gutil.h"
#include "toonzqt/keyframenavigator.h"
#include "toonzqt/dvdialog.h"
#include "toonzqt/fxhistogramrender.h"
#include "toonzqt/histogram.h"

#include "tmacrofx.h"
#include "tstream.h"
#include "tparamcontainer.h"
#include "tspectrumparam.h"
#include "tfxattributes.h"
#include "toutputproperties.h"
#include "pluginhost.h"
#include "tenv.h"

#include "toonz/tcamera.h"
#include "toonz/toonzfolders.h"
#include "toonz/tcolumnfx.h"
#include "toonz/tscenehandle.h"
#include "toonz/txshlevelhandle.h"
#include "toonz/tobjecthandle.h"
#include "toonz/scenefx.h"
#include "toonz/toonzscene.h"
#include "toonz/sceneproperties.h"
#include "toonz/preferences.h"
#include "tw/stringtable.h"

#include <QVBoxLayout>
#include <QToolBar>
#include <QIcon>
#include <QAction>
#include <QStackedWidget>
#include <QLabel>
#include <QMap>
#include <QPainter>
#include <QCheckBox>
#include <QDialog>
#include <QPushButton>

#include <QDesktopServices>
#include <QUrl>

using namespace DVGui;

namespace {

TFxP getCurrentFx(const TFxP &currentFx, std::wstring actualId) {
  if (currentFx->getFxId() == actualId) return currentFx;
  int i;
  for (i = 0; i < currentFx->getInputPortCount(); i++) {
    TFxP fx = getCurrentFx(currentFx->getInputPort(i)->getFx(), actualId);
    if (fx.getPointer()) return fx;
  }
  return 0;
}

bool hasEmptyInputPort(const TFxP &currentFx) {
  if (!currentFx.getPointer()) return true;
  if (currentFx->getInputPortCount() == 0) return false;
  return hasEmptyInputPort(currentFx->getInputPort(0)->getFx());
}
/*
TFxP cloneInputPort(const TFxP &currentFx)
{
        int i;
  for (i=0; i<getInputPortCount(); ++i)
  {
                TFx *inputFx = sceneFx->getInputPort(i)->getFx();
                if(inputFx)
                {
                        if(TLevelColumnFx* affFx = dynamic_cast<TLevelColumnFx*
>(inputFx))
                                currentFx->getInputPort(i)->setFx(inputFx);
                        else
                                currentFx->getInputPort(i)->setFx(cloneInputPort());
                }
                TFxPort *port = getInputPort(i);
                if (port->getFx())
                        fx->connect(getInputPortName(i),
cloneInputPort(port->getFx()));
}
void setLevelFxInputPort(const TFxP &currentFx, const TFxP &sceneFx)
{
        for (int i=0; i<sceneFx->getInputPortCount(); ++i)
        {
                TFx *inputFx = sceneFx->getInputPort(i)->getFx();
                if(inputFx)
                {
                        if(TLevelColumnFx* affFx = dynamic_cast<TLevelColumnFx*
>(inputFx))
                                currentFx->getInputPort(i)->setFx(inputFx);
                        else
                                setLevelFxInputPort(currentFx->getInputPort(i)->getFx(),
inputFx);
                }
        }
}
*/
}

//=============================================================================
// ParamViewer
//-----------------------------------------------------------------------------

ParamsPage::ParamsPage(QWidget *parent, ParamViewer *paramViewer)
    : QFrame(parent)
    , m_paramViewer(paramViewer)
    , m_horizontalLayout(NULL)
    , m_groupLayout(NULL) {
  m_fxHistogramRender = new FxHistogramRender();
  setFrameStyle(QFrame::StyledPanel);

  m_mainLayout = new QGridLayout(this);
  m_mainLayout->setMargin(12);
  m_mainLayout->setVerticalSpacing(10);
  m_mainLayout->setHorizontalSpacing(5);

  m_mainLayout->setColumnStretch(0, 0);
  m_mainLayout->setColumnStretch(1, 1);

  setLayout(m_mainLayout);
}

//-----------------------------------------------------------------------------

ParamsPage::~ParamsPage() {}

//-----------------------------------------------------------------------------

void ParamsPage::setPageField(TIStream &is, const TFxP &fx, bool isVertical) {
  // m_horizontalLayout dovrebbe essere stato inizializzato prima di entrare nel
  // metodo, per sicurezza verifico.
  if (isVertical == false && !m_horizontalLayout) {
    m_horizontalLayout = new QHBoxLayout();
    m_horizontalLayout->setMargin(0);
    m_horizontalLayout->setSpacing(5);
  }

  /*--
   * HBoxLayoutを挿入するとき、最初のパラメータ名はGridlayoutのColumn0に入れるため
   * --*/
  bool isFirstParamInRow = true;

  while (!is.matchEndTag()) {
    std::string tagName;
    if (!is.matchTag(tagName)) throw TException("expected tag");
    if (tagName == "control") {
      /*--- 設定ファイルからインタフェースの桁数を決める (PairSliderのみ実装。)
       * ---*/
      int decimals            = 0;
      std::string decimalsStr = is.getTagAttribute("decimals");
      if (decimalsStr != "") {
        decimals = QString::fromStdString(decimalsStr).toInt();
      }

      std::string name;
      is >> name;
      is.matchEndTag();
      /*-- Layout設定名とFxParameterの名前が一致するものを取得 --*/
      TParamP param = fx->getParams()->getParam(name);
      if (param) {
        std::string paramName = fx->getFxType() + "." + name;
        QString str =
            QString::fromStdWString(TStringTable::translate(paramName));
        ParamField *field = ParamField::create(this, str, param);
        if (field) {
          if (decimals) field->setPrecision(decimals);
          m_fields.push_back(field);
          /*-- hboxタグに挟まれているとき --*/
          if (isVertical == false) {
            assert(m_horizontalLayout);
            QLabel *label = new QLabel(str, this);
            label->setObjectName("FxSettingsLabel");
            if (isFirstParamInRow) {
              int currentRow = m_mainLayout->rowCount();
              m_mainLayout->addWidget(label, currentRow, 0,
                                      Qt::AlignRight | Qt::AlignVCenter);
              isFirstParamInRow = false;
            } else
              m_horizontalLayout->addWidget(label, 0,
                                            Qt::AlignRight | Qt::AlignVCenter);
            m_horizontalLayout->addWidget(field);
            m_horizontalLayout->addSpacing(10);
          } else {
            int currentRow = m_mainLayout->rowCount();
            QLabel *label  = new QLabel(str, this);
            label->setObjectName("FxSettingsLabel");
            m_mainLayout->addWidget(label, currentRow, 0,
                                    Qt::AlignRight | Qt::AlignVCenter);
            m_mainLayout->addWidget(field, currentRow, 1);
          }
          connect(field, SIGNAL(currentParamChanged()), m_paramViewer,
                  SIGNAL(currentFxParamChanged()));
          connect(field, SIGNAL(actualParamChanged()), m_paramViewer,
                  SIGNAL(actualFxParamChanged()));
          connect(field, SIGNAL(paramKeyToggle()), m_paramViewer,
                  SIGNAL(paramKeyChanged()));
        }
      }
    } else if (tagName == "label") {
      std::string name;
      is >> name;
      is.matchEndTag();
      QString str;
      if (isVertical == false) {
        assert(m_horizontalLayout);
        m_horizontalLayout->addWidget(new QLabel(str.fromStdString(name)));
      } else {
        int currentRow = m_mainLayout->rowCount();
        m_mainLayout->addWidget(new QLabel(str.fromStdString(name)), currentRow,
                                0, 1, 2);
      }
    } else if (tagName == "separator") {
      // <separator/> o <separator label="xxx"/>
      std::string label = is.getTagAttribute("label");
      QString str;
      Separator *sep = new Separator(str.fromStdString(label), this);
      int currentRow = m_mainLayout->rowCount();
      m_mainLayout->addWidget(sep, currentRow, 0, 1, 2);
      m_mainLayout->setRowStretch(currentRow, 0);
    } else if (tagName == "histogram") {
      Histogram *histogram = new Histogram();
      m_fxHistogramRender->setHistograms(histogram->getHistograms());
      if (isVertical == false) {
        assert(m_horizontalLayout);
        m_horizontalLayout->addWidget(histogram);
      } else {
        int currentRow = m_mainLayout->rowCount();
        m_mainLayout->addWidget(histogram, currentRow, 0, 1, 2);
      }
    } else if (tagName == "test") {
      // <test/>
      // box->add(new WidgetBox(new TestSeparator(page)));
    } else if (tagName == "hbox") {
      int currentRow     = m_mainLayout->rowCount();
      m_horizontalLayout = new QHBoxLayout();
      m_horizontalLayout->setMargin(0);
      m_horizontalLayout->setSpacing(5);
      setPageField(is, fx, false);
      QWidget *tmpWidget = new QWidget(this);
      tmpWidget->setLayout(m_horizontalLayout);
      m_mainLayout->addWidget(tmpWidget, currentRow, 1, 1, 2);
    } else if (tagName == "vbox") {
      int shrink            = 0;
      std::string shrinkStr = is.getTagAttribute("shrink");
      if (shrinkStr != "") {
        shrink              = QString::fromStdString(shrinkStr).toInt();
        std::string label   = is.getTagAttribute("label");
        QCheckBox *checkBox = new QCheckBox(this);
        QHBoxLayout *sepLay = new QHBoxLayout();
        sepLay->setMargin(0);
        sepLay->setSpacing(5);
        sepLay->addWidget(checkBox, 0);
        sepLay->addWidget(new Separator(QString::fromStdString(label), this),
                          1);
        int currentRow = m_mainLayout->rowCount();
        m_mainLayout->addLayout(sepLay, currentRow, 0, 1, 2);
        m_mainLayout->setRowStretch(currentRow, 0);
        QGridLayout *keepMainLay = m_mainLayout;
        /*-- レイアウトを一時的に差し替え --*/
        m_mainLayout = new QGridLayout(this);
        m_mainLayout->setMargin(12);
        m_mainLayout->setVerticalSpacing(10);
        m_mainLayout->setHorizontalSpacing(5);
        m_mainLayout->setColumnStretch(0, 0);
        m_mainLayout->setColumnStretch(1, 1);
        setPageField(is, fx, true);

        QWidget *tmpWidget = new QWidget(this);
        tmpWidget->setLayout(m_mainLayout);
        /*-- レイアウト戻し --*/
        m_mainLayout = keepMainLay;
        m_mainLayout->addWidget(tmpWidget, currentRow + 1, 0, 1, 2);
        //--- signal-slot connection
        connect(checkBox, SIGNAL(toggled(bool)), tmpWidget,
                SLOT(setVisible(bool)));
        checkBox->setChecked(shrink == 1);
        tmpWidget->setVisible(shrink == 1);
      } else
        setPageField(is, fx, true);
    }
    /*-- PixelParamFieldが２つあるとき、一方のRGB値を他方にコピーするボタン --*/
    else if (tagName == "rgb_link_button") {
      /*-- リンクさせたいパラメータを２つ得る --*/
      std::string name1, name2;
      is >> name1;
      is >> name2;
      is.matchEndTag();

      /*-- 既に作ってあるGUIを探索し、対応する２つを得て格納 --*/
      PixelParamField *ppf1 = 0;
      PixelParamField *ppf2 = 0;
      for (int r = 0; r < m_mainLayout->rowCount(); r++) {
        QLayoutItem *li = m_mainLayout->itemAtPosition(r, 1);
        if (!li) continue;
        QWidget *w = li->widget();
        if (!w) continue;

        ParamField *pf = dynamic_cast<ParamField *>(w);
        if (pf) {
          PixelParamField *ppf = dynamic_cast<PixelParamField *>(pf);
          if (ppf) {
            if (ppf1 == 0 && ppf->getParamName().toStdString() == name1)
              ppf1 = ppf;

            if (ppf2 == 0 && ppf->getParamName().toStdString() == name2)
              ppf2 = ppf;
          }
        }
      }
      if (ppf1 == 0 || ppf2 == 0) continue;

      /*-- ボタンのラベルのため 翻訳する --*/
      std::string paramName1 = fx->getFxType() + "." + name1;
      std::string paramName2 = fx->getFxType() + "." + name2;
      QString str1 =
          QString::fromStdWString(TStringTable::translate(paramName1));
      QString str2 =
          QString::fromStdWString(TStringTable::translate(paramName2));
      QString buttonStr = QString("Copy RGB : %1 > %2").arg(str1).arg(str2);

      RgbLinkButton *linkBut = new RgbLinkButton(buttonStr, this, ppf1, ppf2);
      linkBut->setFixedHeight(21);
      connect(linkBut, SIGNAL(clicked()), linkBut, SLOT(onButtonClicked()));

      int currentRow = m_mainLayout->rowCount();
      m_mainLayout->addWidget(linkBut, currentRow, 1,
                              Qt::AlignLeft | Qt::AlignVCenter);
    }
    /*-- チェックボックスによって他のインタフェースを表示/非表示させる ---*/
    else if (tagName == "visibleToggle") {
      BoolParamField *controller_bpf = 0;
      QList<QWidget *> on_items;
      QList<QWidget *> off_items;
      while (!is.matchEndTag()) {
        std::string tagName;
        if (!is.matchTag(tagName)) throw TException("expected tag");

        if (tagName ==
                "controller" || /*-- 表示をコントロールするチェックボックス --*/
            tagName == "on" || /*-- ONのとき表示されるインタフェース --*/
            tagName == "off") /*-- OFFのとき表示されるインタフェース --*/
        {
          std::string name;
          is >> name;
          is.matchEndTag();
          for (int r = 0; r < m_mainLayout->rowCount(); r++) {
            QLayoutItem *li = m_mainLayout->itemAtPosition(r, 1);
            if (!li) continue;
            QWidget *w = li->widget();
            if (!w) continue;
            ParamField *pf = dynamic_cast<ParamField *>(w);
            if (pf) {
              if (pf->getParamName().toStdString() == name) {
                if (tagName == "controller")
                  controller_bpf = dynamic_cast<BoolParamField *>(pf);
                else if (tagName == "on") {
                  on_items.push_back(w);
                  on_items.push_back(
                      m_mainLayout->itemAtPosition(r, 0)->widget());
                } else if (tagName == "off") {
                  off_items.push_back(w);
                  off_items.push_back(
                      m_mainLayout->itemAtPosition(r, 0)->widget());
                }
              }
            }
            /*-- 入れ子のLayoutも１段階探す --*/
            else {
              QGridLayout *gridLay = dynamic_cast<QGridLayout *>(w->layout());
              if (!gridLay) continue;
              for (int r_s = 0; r_s < gridLay->rowCount(); r_s++) {
                QLayoutItem *li_s = gridLay->itemAtPosition(r_s, 1);
                if (!li_s) continue;
                ParamField *pf_s = dynamic_cast<ParamField *>(li_s->widget());
                if (pf_s) {
                  if (pf_s->getParamName().toStdString() == name) {
                    if (tagName == "controller")
                      controller_bpf = dynamic_cast<BoolParamField *>(pf_s);
                    else if (tagName == "on") {
                      on_items.push_back(pf_s);
                      on_items.push_back(
                          gridLay->itemAtPosition(r_s, 0)->widget());
                    } else if (tagName == "off") {
                      off_items.push_back(pf_s);
                      off_items.push_back(
                          gridLay->itemAtPosition(r_s, 0)->widget());
                    }
                  }
                }
              }
            }
          }
        } else
          throw TException("unexpected tag " + tagName);
      }
      /*-- 表示コントロールをconnect --*/
      if (controller_bpf) {
        /*-- ラベルとWidgetを両方表示/非表示 --*/
        for (int i = 0; i < on_items.size(); i++) {
          connect(controller_bpf, SIGNAL(toggled(bool)), on_items[i],
                  SLOT(setVisible(bool)));
          on_items[i]->hide();
        }
        for (int i = 0; i < off_items.size(); i++) {
          connect(controller_bpf, SIGNAL(toggled(bool)), off_items[i],
                  SLOT(setHidden(bool)));
          off_items[i]->show();
        }
      } else
        std::cout << "controller_bpf NOT found!" << std::endl;
    } else
      throw TException("unexpected tag " + tagName);
  }

  if (isVertical == false && m_horizontalLayout) {
    m_horizontalLayout->addStretch(1);
  }
}

//-----------------------------------------------------------------------------

void ParamsPage::setPageSpace() {
  if (m_fields.count() != 0) {
    QWidget *spaceWidget = new QWidget();

    int currentRow = m_mainLayout->rowCount();
    m_mainLayout->addWidget(spaceWidget, currentRow, 0, 1, 2);

    for (int i = 0; i < currentRow; i++) m_mainLayout->setRowStretch(i, 0);
    m_mainLayout->setRowStretch(currentRow, 1);
  }
}

//-----------------------------------------------------------------------------
void ParamsPage::beginGroup(const char *name) {
  m_groupLayout = new QGridLayout();

  QGroupBox *group = new QGroupBox(QString::fromUtf8(name), this);
  group->setLayout(m_groupLayout);
  m_mainLayout->addWidget(group, m_mainLayout->rowCount(), 0, 1, 2);
}

void ParamsPage::endGroup() { m_groupLayout = NULL; }

void ParamsPage::addWidget(QWidget *field, bool isVertical) {
  QLabel *label  = NULL;
  ParamField *pf = qobject_cast<ParamField *>(field);
  if (pf) {
    label = new QLabel(pf->getUIName(), this);
    label->setObjectName("FxSettingsLabel");
    if (!pf->getDescription().isEmpty())
      label->setToolTip(pf->getDescription());
  }

  if (isVertical) {
    if (m_groupLayout) {
      int row = m_groupLayout->rowCount();
      if (label)
        m_groupLayout->addWidget(label, row, 0,
                                 Qt::AlignRight | Qt::AlignVCenter);
      m_groupLayout->addWidget(field, row, 1);
    } else {
      int row = m_mainLayout->rowCount();
      if (label)
        m_mainLayout->addWidget(label, row, 0,
                                Qt::AlignRight | Qt::AlignVCenter);
      m_mainLayout->addWidget(field, row, 1);
    }
  } else {
    if (!m_horizontalLayout) {
      m_horizontalLayout = new QHBoxLayout();
      m_horizontalLayout->setMargin(0);
      m_horizontalLayout->setSpacing(5);
    }
    m_horizontalLayout->addWidget(field);
  }
}

#define TOONZ_DEFINE_NEW_COMPONENT(NAME, MAKE)                                 \
  QWidget *ParamsPage::NAME(TFx *fx, const char *name) {                       \
    TParamP param = fx->getParams()->getParam(name);                           \
    if (!param) return NULL;                                                   \
    QString const paramName =                                                  \
        QString::fromStdString(fx->getFxType() + "." + name);                  \
    ParamField *field = MAKE(this, paramName, param);                          \
    if (!field) return NULL;                                                   \
    m_fields.push_back(field);                                                 \
    connect(field, SIGNAL(currentParamChanged()), m_paramViewer,               \
            SIGNAL(currentFxParamChanged()));                                  \
    connect(field, SIGNAL(actualParamChanged()), m_paramViewer,                \
            SIGNAL(actualFxParamChanged()));                                   \
    connect(field, SIGNAL(paramKeyToggle()), m_paramViewer,                    \
            SIGNAL(paramKeyChanged()));                                        \
    return field;                                                              \
  }

TOONZ_DEFINE_NEW_COMPONENT(newParamField, ParamField::create);
TOONZ_DEFINE_NEW_COMPONENT(newLineEdit, make_lineedit);
TOONZ_DEFINE_NEW_COMPONENT(newSlider, make_slider);
TOONZ_DEFINE_NEW_COMPONENT(newSpinBox, make_spinbox);
TOONZ_DEFINE_NEW_COMPONENT(newCheckBox, make_checkbox);
TOONZ_DEFINE_NEW_COMPONENT(newRadioButton, make_radiobutton);
TOONZ_DEFINE_NEW_COMPONENT(newComboBox, make_combobox);

#undef TOONZ_DEFINE_NEW_COMPONENT

//-----------------------------------------------------------------------------

void ParamsPage::setFx(const TFxP &currentFx, const TFxP &actualFx, int frame) {
  assert(currentFx);
  assert(actualFx);
  for (int i = 0; i < (int)m_fields.size(); i++) {
    ParamField *field = m_fields[i];
    QString fieldName = field->getParamName();

    TFxP fx = getCurrentFx(currentFx, actualFx->getFxId());
    assert(fx.getPointer());
    TParamP currentParam =
        currentFx->getParams()->getParam(fieldName.toStdString());
    TParamP actualParam =
        actualFx->getParams()->getParam(fieldName.toStdString());
    assert(currentParam);
    assert(actualParam);
    field->setParam(currentParam, actualParam, frame);
  }
  if (actualFx->getInputPortCount() > 0)
    m_fxHistogramRender->computeHistogram(actualFx->getInputPort(0)->getFx(),
                                          frame);
}

//-----------------------------------------------------------------------------

void ParamsPage::setPointValue(int index, const TPointD &p) {
  if (0 <= index && index < (int)m_fields.size())
    m_fields[index]->setPointValue(p);
}

//-----------------------------------------------------------------------------

void ParamsPage::update(int frame) {
  int i;
  for (i = 0; i < (int)m_fields.size(); i++) {
    ParamField *field = m_fields[i];
    field->update(frame);
  }
}

//-----------------------------------------------------------------------------

namespace {

void updateMaximumPageSize(QGridLayout *layout, int &maxLabelWidth,
                           int &maxWidgetWidth, int &fieldsHeight) {
  /*-- Label側の横幅の最大値を得る --*/
  for (int r = 0; r < layout->rowCount(); r++) {
    /*-- アイテムが無ければ次の行へ --*/
    if (!layout->itemAtPosition(r, 0)) continue;
    /*-- ラベルの横幅を得て、最大値を更新していく --*/
    QLabel *label =
        dynamic_cast<QLabel *>(layout->itemAtPosition(r, 0)->widget());
    QGroupBox *gBox =
        dynamic_cast<QGroupBox *>(layout->itemAtPosition(r, 0)->widget());
    if (label) {
      int tmpWidth = label->fontMetrics().width(label->text());
      if (maxLabelWidth < tmpWidth) maxLabelWidth = tmpWidth;
    }
    /*-- PlugInFxのGroupパラメータのサイズ --*/
    else if (gBox) {
      QGridLayout *gridLay = dynamic_cast<QGridLayout *>(gBox->layout());
      if (gridLay) {
        updateMaximumPageSize(gridLay, maxLabelWidth, maxWidgetWidth,
                              fieldsHeight);
        /*-- GroupBoxのマージン --*/
        maxLabelWidth += 10;
        maxWidgetWidth += 10;
        fieldsHeight += 20;
      }
    }
  }

  /*-- Widget側の最適な縦サイズおよび横幅の最大値を得る --*/
  for (int r = 0; r < layout->rowCount(); r++) {
    /*-- Column1にある可能性のあるもの：ParamField, Histogram, Layout,
     * RgbLinkButton --*/

    QLayoutItem *item = layout->itemAtPosition(r, 1);
    if (!item) continue;
    /*-- ParamFieldの場合 --*/
    ParamField *pF = dynamic_cast<ParamField *>(item->widget());
    if (pF) {
      QSize fieldBestSize = pF->getPreferedSize();
      /*-- 横幅の更新 --*/
      if (maxWidgetWidth < fieldBestSize.width())
        maxWidgetWidth = fieldBestSize.width();
      /*-- 縦サイズの更新 --*/
      fieldsHeight += fieldBestSize.height();
    } else {
      QHBoxLayout *hLay      = dynamic_cast<QHBoxLayout *>(item->layout());
      Histogram *histo       = dynamic_cast<Histogram *>(item->widget());
      Separator *sep         = dynamic_cast<Separator *>(item->widget());
      RgbLinkButton *linkBut = dynamic_cast<RgbLinkButton *>(item->widget());
      /*-- HLayoutの場合 --*/
      if (hLay) {
        int tmpSumWidth  = 0;
        int tmpMaxHeight = 0;
        for (int i = 0; i < hLay->count(); i++) {
          QLabel *hLabel = dynamic_cast<QLabel *>(hLay->itemAt(i)->widget());
          ParamField *hPF =
              dynamic_cast<ParamField *>(hLay->itemAt(i)->widget());
          if (hLabel) {
            int tmpWidth = hLabel->fontMetrics().width(hLabel->text());
            /*-- 横幅を足していく --*/
            tmpSumWidth += tmpWidth;
          } else if (hPF) {
            tmpSumWidth += hPF->getPreferedSize().width();
            if (tmpMaxHeight < hPF->getPreferedSize().height())
              tmpMaxHeight = hPF->getPreferedSize().height();
          } else
            continue;
        }
        /*-- 横幅にSpacing値（=5）の分を足す --*/
        if (hLay->count() > 1) tmpSumWidth += (hLay->count() - 1) * 5;
        /*-- 横幅の更新 --*/
        if (maxWidgetWidth < tmpSumWidth) maxWidgetWidth = tmpSumWidth;
        /*-- 縦サイズの更新 --*/
        fieldsHeight += tmpMaxHeight;
      }
      /*--- ヒストグラムの場合 : 最小サイズは 横278 × 縦162 ---*/
      else if (histo) {
        /*-- 横幅の更新 --*/
        if (maxWidgetWidth < 278) maxWidgetWidth = 278;
        /*-- 縦サイズの更新 --*/
        fieldsHeight += 162;
      }
      /*-- セパレータの場合 : 高さ10 --*/
      else if (sep) {
        fieldsHeight += 10;
      }
      /*-- RgbLinkButtonの場合 : 高さ21 --*/
      else if (linkBut) {
        fieldsHeight += 21;
      } else
        continue;
    }
  }

  if (layout->rowCount() > 1) fieldsHeight += (layout->rowCount() - 1) * 10;
}
};

QSize ParamsPage::getPreferedSize() {
  int maxLabelWidth  = 0;
  int maxWidgetWidth = 0;
  int fieldsHeight   = 0;

  updateMaximumPageSize(m_mainLayout, maxLabelWidth, maxWidgetWidth,
                        fieldsHeight);

  return QSize(
      maxLabelWidth + maxWidgetWidth + 5 /* Spacing */ + 24 /* Margin２つ分 */,
      fieldsHeight + 24 /* Margin２つ分 */ + 20 /* 余白 */);
}

//=============================================================================
// ParamsPageSet
//-----------------------------------------------------------------------------

#if QT_VERSION >= 0x050500
ParamsPageSet::ParamsPageSet(QWidget *parent, Qt::WindowFlags flags)
#else
ParamsPageSet::ParamsPageSet(QWidget *parent, Qt::WFlags flags)
#endif
    : QWidget(parent, flags)
    , m_preferedSize(0, 0)
    , m_helpFilePath("")
    , m_helpCommand("") {
  // TabBar
  m_tabBar = new TabBar(this);
  // This widget is used to set the background color of the tabBar
  // using the styleSheet.
  // It is also used to take 6px on the left before the tabBar
  // and to draw the two lines on the bottom size
  m_tabBarContainer = new TabBarContainter(this);
  m_pagesList       = new QStackedWidget(this);

  m_helpButton = new QPushButton(tr("Fx Help"), this);

  m_parent = dynamic_cast<ParamViewer *>(parent);
  m_pageFxIndexTable.clear();
  m_tabBar->setDrawBase(false);
  m_tabBar->setObjectName("FxSettingsTabBar");
  m_helpButton->setFixedHeight(20);
  m_helpButton->setObjectName("FxSettingsHelpButton");
  m_helpButton->setFocusPolicy(Qt::NoFocus);

  //----layout
  QVBoxLayout *mainLayout = new QVBoxLayout();
  mainLayout->setMargin(1);
  mainLayout->setSpacing(0);
  {
    QHBoxLayout *hLayout = new QHBoxLayout();
    hLayout->setMargin(0);
    hLayout->addSpacing(6);
    {
      hLayout->addWidget(m_tabBar);
      hLayout->addStretch(1);
      hLayout->addWidget(m_helpButton);
    }
    m_tabBarContainer->setLayout(hLayout);

    mainLayout->addWidget(m_tabBarContainer);

    mainLayout->addWidget(m_pagesList);

    mainLayout->addWidget(new Separator("", this), 0);
  }
  setLayout(mainLayout);

  connect(m_tabBar, SIGNAL(currentChanged(int)), this, SLOT(setPage(int)));

  m_helpButton->hide();
}

//-----------------------------------------------------------------------------

ParamsPageSet::~ParamsPageSet() {}

//-----------------------------------------------------------------------------

void ParamsPageSet::setPage(int index) {
  if (m_tabBar->count() == 0 || m_pagesList->count() == 0) return;
  assert(index >= 0 && index < m_pagesList->count());
  m_pagesList->setCurrentIndex(index);
}

//-----------------------------------------------------------------------------

void ParamsPageSet::setFx(const TFxP &currentFx, const TFxP &actualFx,
                          int frame) {
  TMacroFx *currentFxMacro = dynamic_cast<TMacroFx *>(currentFx.getPointer());
  if (currentFxMacro) {
    TMacroFx *actualFxMacro = dynamic_cast<TMacroFx *>(actualFx.getPointer());
    assert(actualFxMacro);
    const std::vector<TFxP> &currentFxMacroFxs = currentFxMacro->getFxs();
    const std::vector<TFxP> &actualFxMacroFxs  = actualFxMacro->getFxs();
    assert(currentFxMacroFxs.size() == actualFxMacroFxs.size());
    for (int i = 0; i < m_pagesList->count(); i++) {
      ParamsPage *page = getParamsPage(i);
      if (!page || !m_pageFxIndexTable.contains(page)) continue;
      int index = m_pageFxIndexTable[page];
      page->setFx(currentFxMacroFxs[index], actualFxMacroFxs[index], frame);
    }
  } else {
    for (int i = 0; i < m_pagesList->count(); i++) {
      ParamsPage *page = getParamsPage(i);
      if (!page) continue;
      page->setFx(currentFx, actualFx, frame);
    }
  }
}

//-----------------------------------------------------------------------------

void ParamsPageSet::updatePage(int frame, bool onlyParam) {
  if (!m_pagesList) return;
  int i;
  for (i = 0; i < m_pagesList->count(); i++) {
    ParamsPage *page = getParamsPage(i);
    if (!page) continue;
    page->update(frame);
    if (!onlyParam) page->getFxHistogramRender()->invalidateFrame(frame);
  }
}

//-----------------------------------------------------------------------------

void ParamsPageSet::setScene(ToonzScene *scene) {
  if (!m_pagesList) return;
  int i;
  for (i = 0; i < m_pagesList->count(); i++) {
    ParamsPage *page = getParamsPage(i);
    if (!page) continue;
    page->getFxHistogramRender()->setScene(scene);
  }
}

//-----------------------------------------------------------------------------

void ParamsPageSet::setIsCameraViewMode(bool isCameraViewMode) {
  if (!m_pagesList) return;
  int i;
  for (i = 0; i < m_pagesList->count(); i++) {
    ParamsPage *page = getParamsPage(i);
    if (!page) continue;
    page->getFxHistogramRender()->setIsCameraViewMode(isCameraViewMode);
  }
}

//-----------------------------------------------------------------------------

ParamsPage *ParamsPageSet::createParamsPage() {
  return new ParamsPage(this, m_parent);
}

void ParamsPageSet::addParamsPage(ParamsPage *page, const char *name) {
  /*-- このFxで最大サイズのページに合わせてダイアログをリサイズ --*/
  QSize pagePreferedSize = page->getPreferedSize();
  m_preferedSize         = m_preferedSize.expandedTo(
      pagePreferedSize + QSize(m_tabBarContainer->height() + 2,
                               2)); /*-- 2は上下左右のマージン --*/

  QScrollArea *pane = new QScrollArea(this);
  pane->setWidgetResizable(true);
  pane->setHorizontalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
  pane->setVerticalScrollBarPolicy(Qt::ScrollBarAsNeeded);
  pane->setWidget(page);

  m_tabBar->addSimpleTab(QString::fromUtf8(name));
  m_pagesList->addWidget(pane);
}

//-----------------------------------------------------------------------------

void ParamsPageSet::createControls(const TFxP &fx, int index) {
  if (TMacroFx *macroFx = dynamic_cast<TMacroFx *>(fx.getPointer())) {
    const std::vector<TFxP> &fxs = macroFx->getFxs();
    for (int i = 0; i < (int)fxs.size(); i++) createControls(fxs[i], i);
    return;
  }
  if (RasterFxPluginHost *plugin =
          dynamic_cast<RasterFxPluginHost *>(fx.getPointer())) {
    plugin->build(this);
    std::string url = plugin->getUrl();
    if (!url.empty()) {
      connect(m_helpButton, SIGNAL(pressed()), this, SLOT(openHelpUrl()));
      m_helpButton->show();
      m_helpUrl = url;
    }
    return;
  }

  TFilePath fp = ToonzFolder::getProfileFolder() + "layouts" + "fxs" +
                 (fx->getFxType() + ".xml");
  TIStream is(fp);
  if (!is) return;

  if (fx->getParams()->getParamCount()) {
    try {
      std::string tagName;
      if (!is.matchTag(tagName) || tagName != "fxlayout")
        throw TException("expected <fxlayout>");

      m_helpFilePath = is.getTagAttribute("help_file");
      if (m_helpFilePath != "") {
        connect(m_helpButton, SIGNAL(pressed()), this, SLOT(openHelpFile()));
        m_helpButton->show();
        /*-- pdfファイルのページ指定など、引数が必要な場合の追加fragmentを取得
         * --*/
        m_helpCommand = is.getTagAttribute("help_command");
      }

      while (!is.matchEndTag()) createPage(is, fx, index);
    } catch (TException const &) {
    }
  }
  // else createEmptyPage();
}

//-----------------------------------------------------------------------------

ParamsPage *ParamsPageSet::getCurrentParamsPage() const {
  QScrollArea *scrollAreaPage =
      dynamic_cast<QScrollArea *>(m_pagesList->currentWidget());
  assert(scrollAreaPage);
  return dynamic_cast<ParamsPage *>(scrollAreaPage->widget());
}

//-----------------------------------------------------------------------------

ParamsPage *ParamsPageSet::getParamsPage(int index) const {
  QScrollArea *scrollAreaPage =
      dynamic_cast<QScrollArea *>(m_pagesList->widget(index));
  assert(scrollAreaPage);
  return dynamic_cast<ParamsPage *>(scrollAreaPage->widget());
}

//-----------------------------------------------------------------------------

void ParamsPageSet::createPage(TIStream &is, const TFxP &fx, int index) {
  std::string tagName;
  if (!is.matchTag(tagName) || tagName != "page")
    throw TException("expected <page>");
  std::string pageName         = is.getTagAttribute("name");
  if (pageName == "") pageName = "page";

  ParamsPage *paramsPage = new ParamsPage(this, m_parent);
  paramsPage->setPage(is, fx);

  /*-- このFxで最大サイズのページに合わせてダイアログをリサイズ --*/
  QSize pagePreferedSize = paramsPage->getPreferedSize();
  m_preferedSize         = m_preferedSize.expandedTo(
      pagePreferedSize + QSize(m_tabBarContainer->height() + 2,
                               2)); /*-- 2は上下左右のマージン --*/

  QScrollArea *scrollAreaPage = new QScrollArea(this);
  scrollAreaPage->setWidgetResizable(true);
  scrollAreaPage->setHorizontalScrollBarPolicy(Qt::ScrollBarAsNeeded);
  scrollAreaPage->setVerticalScrollBarPolicy(Qt::ScrollBarAsNeeded);
  scrollAreaPage->setWidget(paramsPage);

  QString str;
  m_tabBar->addSimpleTab(str.fromStdString(pageName));
  m_pagesList->addWidget(scrollAreaPage);
  if (index >= 0) m_pageFxIndexTable[paramsPage] = index;
}

//-----------------------------------------------------------------------------

/* TODO: Webサイト内のヘルプに対応すべきか検討 2016.02.01 shun_iwasawa */
void ParamsPageSet::openHelpFile() {
  if (m_helpFilePath == "") return;

  // if (m_helpCommand != "")
  //	commandString += m_helpCommand + " ";

  TFilePath helpFp =
      TEnv::getStuffDir() + TFilePath("doc") + TFilePath(m_helpFilePath);
  // commandString +=
  // QString::fromStdWString(helpFp.getWideString()).toStdString();
  // QString command = QString::fromStdString(m_helpFilePath);
  // system(commandString.c_str());
  // QProcess process;
  // process.start(command);
  QDesktopServices::openUrl(
      QUrl::fromLocalFile(QString::fromStdWString(helpFp.getWideString())));
}

void ParamsPageSet::openHelpUrl() {
  QDesktopServices::openUrl(QUrl(QString(m_helpUrl.c_str())));
}

//=============================================================================
// ParamViewer
//-----------------------------------------------------------------------------

#if QT_VERSION >= 0x050500
ParamViewer::ParamViewer(QWidget *parent, Qt::WindowFlags flags)
#else
ParamViewer::ParamViewer(QWidget *parent, Qt::WFlags flags)
#endif
    : QFrame(parent, flags), m_fx(0) {
  m_tablePageSet = new QStackedWidget(this);
  m_tablePageSet->addWidget(new QWidget());

  /*-- SwatchViewerを表示/非表示するボタン --*/
  QPushButton *showSwatchButton = new QPushButton("", this);
  QLabel *swatchLabel           = new QLabel(tr("Swatch Viewer"), this);

  swatchLabel->setObjectName("TitleTxtLabel");
  showSwatchButton->setObjectName("FxSettingsPreviewShowButton");
  showSwatchButton->setFixedSize(15, 15);
  showSwatchButton->setCheckable(true);
  showSwatchButton->setChecked(false);
  showSwatchButton->setFocusPolicy(Qt::NoFocus);

  QVBoxLayout *mainLayout = new QVBoxLayout(this);
  mainLayout->setMargin(1);
  mainLayout->setSpacing(0);
  {
    mainLayout->addWidget(m_tablePageSet, 1);

    QHBoxLayout *showPreviewButtonLayout = new QHBoxLayout(this);
    showPreviewButtonLayout->setMargin(3);
    showPreviewButtonLayout->setSpacing(3);
    {
      showPreviewButtonLayout->addWidget(showSwatchButton, 0);
      showPreviewButtonLayout->addWidget(swatchLabel, 0);
      showPreviewButtonLayout->addStretch(1);
    }
    mainLayout->addLayout(showPreviewButtonLayout, 0);
  }
  setLayout(mainLayout);

  connect(showSwatchButton, SIGNAL(toggled(bool)), this,
          SIGNAL(showSwatchButtonToggled(bool)));
}

//-----------------------------------------------------------------------------

ParamViewer::~ParamViewer() {}

//-----------------------------------------------------------------------------

void ParamViewer::setFx(const TFxP &currentFx, const TFxP &actualFx, int frame,
                        ToonzScene *scene) {
  if (!actualFx) {
    m_tablePageSet->setCurrentIndex(0);
    return;
  }
  std::string name = actualFx->getFxType();
  if (name == "macroFx") {
    TMacroFx *macroFx = dynamic_cast<TMacroFx *>(currentFx.getPointer());
    if (macroFx) name = macroFx->getMacroFxType();
  }

  int currentIndex = -1;

  QMap<std::string, int>::iterator it;
  it = m_tableFxIndex.find(name);
  if (it == m_tableFxIndex.end()) {
    ParamsPageSet *pageSet = new ParamsPageSet(this);
    currentIndex           = m_tablePageSet->addWidget(pageSet);
    m_tableFxIndex[name]   = currentIndex;
    pageSet->createControls(actualFx);
  } else
    currentIndex = it.value();

  assert(currentIndex >= 0);

  m_tablePageSet->setCurrentIndex(currentIndex);

  getCurrentPageSet()->setScene(scene);

  if (m_fx != currentFx) {
    getCurrentPageSet()->setFx(currentFx, actualFx, frame);
    QSize pageViewerPreferedSize =
        getCurrentPageSet()->getPreferedSize() + QSize(2, 20);
    emit preferedSizeChanged(pageViewerPreferedSize);
  }
}

//-----------------------------------------------------------------------------

void ParamViewer::setScene(ToonzScene *scene) {
  ParamsPageSet *paramsPageSet = getCurrentPageSet();
  if (!paramsPageSet) return;
  paramsPageSet->setScene(scene);
}

//-----------------------------------------------------------------------------

void ParamViewer::setIsCameraViewMode(bool isCameraViewMode) {
  ParamsPageSet *paramsPageSet = getCurrentPageSet();
  if (!paramsPageSet) return;
  paramsPageSet->setIsCameraViewMode(isCameraViewMode);
}

//-----------------------------------------------------------------------------

void ParamViewer::update(int frame, bool onlyParam) {
  ParamsPageSet *paramsPageSet = getCurrentPageSet();
  if (!paramsPageSet) return;
  paramsPageSet->updatePage(frame, onlyParam);
}

//-----------------------------------------------------------------------------

void ParamViewer::setPointValue(int index, const TPointD &p) {
  // Search the index-th param among all pages
  ParamsPageSet *pageSet = getCurrentPageSet();
  ParamsPage *page       = 0;

  for (int i = 0; i < pageSet->getParamsPageCount(); ++i) {
    page            = pageSet->getParamsPage(i);
    int paramsCount = page->m_fields.count();

    if (index <= paramsCount) break;
    index -= paramsCount;
  }

  if (page) page->setPointValue(index, p);
}

//-----------------------------------------------------------------------------

ParamsPageSet *ParamViewer::getCurrentPageSet() const {
  return dynamic_cast<ParamsPageSet *>(m_tablePageSet->currentWidget());
}

//=============================================================================
// FxSettings
//-----------------------------------------------------------------------------

FxSettings::FxSettings(QWidget *parent, const TPixel32 &checkCol1,
                       const TPixel32 &checkCol2)
    : QSplitter(Qt::Vertical, parent)
    , m_frameHandle(0)
    , m_fxHandle(0)
    , m_xsheetHandle(0)
    , m_sceneHandle(0)
    , m_levelHandle(0)
    , m_objectHandle(0)
    , m_checkCol1(checkCol1)
    , m_checkCol2(checkCol2)
    , m_isCameraModeView(false)
    , m_container_height(177) {
  // param viewer
  m_paramViewer = new ParamViewer(this);
  // swatch
  QWidget *swatchContainer = new QWidget(this);
  m_viewer                 = new SwatchViewer(swatchContainer);
  setWhiteBg();
  createToolBar();

  m_paramViewer->setMinimumHeight(50);
  swatchContainer->setSizePolicy(QSizePolicy::MinimumExpanding,
                                 QSizePolicy::MinimumExpanding);

  //---layout
  addWidget(m_paramViewer);

  QVBoxLayout *swatchLayout = new QVBoxLayout(swatchContainer);
  swatchLayout->setMargin(0);
  swatchLayout->setSpacing(0);
  {
    swatchLayout->addWidget(m_viewer, 0, Qt::AlignHCenter);

    QHBoxLayout *toolBarLayout = new QHBoxLayout(swatchContainer);
    {
      toolBarLayout->addWidget(m_toolBar, 0,
                               Qt::AlignHCenter | Qt::AlignBottom);
    }
    swatchLayout->addLayout(toolBarLayout);
  }
  swatchContainer->setLayout(swatchLayout);

  addWidget(swatchContainer);

  //---signal-slot connections
  bool ret = true;
  ret      = ret && connect(m_paramViewer, SIGNAL(currentFxParamChanged()),
                       SLOT(updateViewer()));
  ret = ret &&
        connect(m_viewer, SIGNAL(pointPositionChanged(int, const TPointD &)),
                SLOT(onPointChanged(int, const TPointD &)));
  ret = ret && connect(m_paramViewer, SIGNAL(preferedSizeChanged(QSize)), this,
                       SLOT(onPreferedSizeChanged(QSize)));
  ret = ret && connect(m_paramViewer, SIGNAL(showSwatchButtonToggled(bool)),
                       this, SLOT(onShowSwatchButtonToggled(bool)));
  assert(ret);

  swatchContainer->hide();

  // Swatch updates should happen only at the end of a separator resize op.
  setStretchFactor(0, 1);
  setStretchFactor(1, 0);
  setOpaqueResize(false);
}

//-----------------------------------------------------------------------------

FxSettings::~FxSettings() {}

//-----------------------------------------------------------------------------

void FxSettings::setFxHandle(TFxHandle *fxHandle) {
  m_fxHandle = fxHandle;
  m_keyframeNavigator->setFxHandle(m_fxHandle);

  ParamField::setFxHandle(m_fxHandle);
}

//-----------------------------------------------------------------------------

void FxSettings::setFrameHandle(TFrameHandle *frameHandle) {
  m_frameHandle = frameHandle;
  m_keyframeNavigator->setFrameHandle(m_frameHandle);
  m_frameNavigator->setFrameHandle(m_frameHandle);
}

//-----------------------------------------------------------------------------

void FxSettings::setXsheetHandle(TXsheetHandle *xsheetHandle) {
  m_xsheetHandle = xsheetHandle;
}

//-----------------------------------------------------------------------------

void FxSettings::setSceneHandle(TSceneHandle *sceneHandle) {
  m_sceneHandle = sceneHandle;
  setCurrentScene();
}

//-----------------------------------------------------------------------------

void FxSettings::setLevelHandle(TXshLevelHandle *levelHandle) {
  m_levelHandle = levelHandle;
}

//-----------------------------------------------------------------------------

void FxSettings::setObjectHandle(TObjectHandle *objectHandle) {
  m_objectHandle = objectHandle;
}

//-----------------------------------------------------------------------------

void FxSettings::createToolBar() {
  m_toolBar = new QToolBar(this);
  m_toolBar->setMovable(false);
  m_toolBar->setFixedHeight(23);
  m_toolBar->setIconSize(QSize(17, 17));
  m_toolBar->setObjectName("MediumPaddingToolBar");
  // m_toolBar->setIconSize(QSize(23, 21));
  // m_toolBar->setSizePolicy(QSizePolicy::MinimumExpanding,
  // QSizePolicy::MinimumExpanding);

  // View mode
  QActionGroup *viewModeActGroup = new QActionGroup(m_toolBar);
  viewModeActGroup->setExclusive(false);
  // camera
  QIcon camera       = createQIconOnOff("viewcamera");
  QAction *cameraAct = new QAction(camera, tr("&Camera Preview"), m_toolBar);
  cameraAct->setCheckable(true);
  viewModeActGroup->addAction(cameraAct);
  m_toolBar->addAction(cameraAct);
  // preview
  QIcon preview       = createQIconOnOff("preview");
  QAction *previewAct = new QAction(preview, tr("&Preview"), m_toolBar);
  previewAct->setCheckable(true);
  viewModeActGroup->addAction(previewAct);
  m_toolBar->addAction(previewAct);
  connect(viewModeActGroup, SIGNAL(triggered(QAction *)),
          SLOT(onViewModeChanged(QAction *)));

  m_toolBar->addSeparator();

  QActionGroup *viewModeGroup = new QActionGroup(m_toolBar);
  viewModeGroup->setExclusive(true);

  QAction *whiteBg = new QAction(createQIconOnOff("preview_white"),
                                 tr("&White Background"), m_toolBar);
  whiteBg->setCheckable(true);
  whiteBg->setChecked(true);
  viewModeGroup->addAction(whiteBg);
  connect(whiteBg, SIGNAL(triggered()), this, SLOT(setWhiteBg()));
  m_toolBar->addAction(whiteBg);

  QAction *blackBg = new QAction(createQIconOnOff("preview_black"),
                                 tr("&Black Background"), m_toolBar);
  blackBg->setCheckable(true);
  viewModeGroup->addAction(blackBg);
  connect(blackBg, SIGNAL(triggered()), this, SLOT(setBlackBg()));
  m_toolBar->addAction(blackBg);

  m_checkboardBg = new QAction(createQIconOnOff("preview_checkboard"),
                               tr("&Checkered Background"), m_toolBar);
  m_checkboardBg->setCheckable(true);
  viewModeGroup->addAction(m_checkboardBg);
  connect(m_checkboardBg, SIGNAL(triggered()), this, SLOT(setCheckboardBg()));
  m_toolBar->addAction(m_checkboardBg);

  m_toolBar->addSeparator();

  m_keyframeNavigator = new FxKeyframeNavigator(m_toolBar);
  m_toolBar->addWidget(m_keyframeNavigator);

  m_toolBar->addSeparator();

  m_frameNavigator = new FrameNavigator(m_toolBar);
  m_frameNavigator->setFrameHandle(m_frameHandle);
  m_toolBar->addWidget(m_frameNavigator);
}

//-----------------------------------------------------------------------------

void FxSettings::setFx(const TFxP &currentFx, const TFxP &actualFx) {
  // disconnecting from the fxChanged() signals avoid useless and dangerous
  // updates!!!
  if (m_fxHandle)
    disconnect(m_fxHandle, SIGNAL(fxChanged()), this,
               SLOT(updateParamViewer()));

  TFxP currentFxWithoutCamera = 0;
  if (currentFx && actualFx)
    currentFxWithoutCamera = getCurrentFx(currentFx, actualFx->getFxId());

  if (currentFxWithoutCamera)
    TFxUtil::setKeyframe(currentFxWithoutCamera, m_frameHandle->getFrameIndex(),
                         actualFx, m_frameHandle->getFrameIndex());

  ToonzScene *scene        = 0;
  if (m_sceneHandle) scene = m_sceneHandle->getScene();

  int frameIndex                = 0;
  if (m_frameHandle) frameIndex = m_frameHandle->getFrameIndex();

  m_paramViewer->setFx(currentFxWithoutCamera, actualFx, frameIndex, scene);
  m_paramViewer->setIsCameraViewMode(m_isCameraModeView);
  m_viewer->setCameraMode(m_isCameraModeView);

  TDimension cameraSize = TDimension(-1, -1);
  if (scene) cameraSize = scene->getCurrentCamera()->getRes();
  m_viewer->setCameraSize(cameraSize);

  m_viewer->setFx(currentFx, actualFx, frameIndex);

  if (m_fxHandle)
    connect(m_fxHandle, SIGNAL(fxChanged()), this, SLOT(updateParamViewer()));
}

//-----------------------------------------------------------------------------

void FxSettings::setCurrentFrame() {
  int frame = m_frameHandle->getFrameIndex();
  m_paramViewer->update(frame, false);

  // if(m_isCameraModeView)
  setCurrentFx();
  m_viewer->updateFrame(frame);
}

//-----------------------------------------------------------------------------

void FxSettings::changeTitleBar(TFx *fx) {
  QDialog *popup = dynamic_cast<QDialog *>(parentWidget());
  if (!popup) return;

  QString titleText("Fx Settings");
  if (fx) {
    titleText += tr(" : ");
    titleText += QString::fromStdWString(fx->getName());
  }

  popup->setWindowTitle(titleText);
}

//-----------------------------------------------------------------------------

void FxSettings::setCurrentFx() {
  TFx *currFx = m_fxHandle->getFx();

  TFxP actualFx, currentFx;
  if (!currFx || 0 != dynamic_cast<TXsheetFx *>(currFx)) {
    actualFx = currentFx = TFxP();
    setFx(actualFx, currentFx);
    changeTitleBar(currentFx.getPointer());
    return;
  }
  TFxP fx(currFx);
  bool hasEmptyInput = false;
  if (TZeraryColumnFx *zfx = dynamic_cast<TZeraryColumnFx *>(fx.getPointer()))
    fx = zfx->getZeraryFx();
  else
    hasEmptyInput   = hasEmptyInputPort(fx);
  int frame         = m_frameHandle->getFrame();
  ToonzScene *scene = m_sceneHandle->getScene();
  actualFx          = fx;
  bool isEnabled    = actualFx->getAttributes()->isEnabled();
  actualFx->getAttributes()->enable(true);
  if (hasEmptyInput)
    currentFx = actualFx;
  else {
    if (!m_isCameraModeView)
      currentFx = buildSceneFx(scene, frame, actualFx, false);
    else {
      const TRenderSettings rs =
          scene->getProperties()->getPreviewProperties()->getRenderSettings();
      currentFx = buildPartialSceneFx(scene, (double)frame, actualFx, 1, false);
    }
  }

  if (currentFx) currentFx = currentFx->clone(true);

  // se al frame corrente non c'e' il livello a cui e' applicato l'effetto:
  // current=0, actual!=0
  if (!currentFx) currentFx = actualFx->clone(false);

  actualFx->getAttributes()->enable(isEnabled);
  setFx(currentFx, actualFx);
  changeTitleBar(currentFx.getPointer());
}

//-----------------------------------------------------------------------------

void FxSettings::setCurrentScene() {
  ToonzScene *scene = m_sceneHandle->getScene();
  m_paramViewer->setScene(scene);
}

//-----------------------------------------------------------------------------

void FxSettings::notifySceneChanged() {
  TPixel32 col1, col2;
  Preferences::instance()->getChessboardColors(col1, col2);
  setCheckboardColors(col1, col2);
}

//-----------------------------------------------------------------------------

void FxSettings::showEvent(QShowEvent *event) {
  setCurrentFx();
  setCurrentFrame();
  connect(m_frameHandle, SIGNAL(frameSwitched()), SLOT(setCurrentFrame()));
  if (m_fxHandle) {
    connect(m_paramViewer, SIGNAL(actualFxParamChanged()), m_fxHandle,
            SIGNAL(fxChanged()));
    connect(m_fxHandle, SIGNAL(fxChanged()), SLOT(updateParamViewer()));
    connect(m_fxHandle, SIGNAL(fxSettingsShouldBeSwitched()),
            SLOT(setCurrentFx()));
  }
  if (m_sceneHandle) {
    connect(m_sceneHandle, SIGNAL(sceneChanged()), this,
            SLOT(notifySceneChanged()));
    connect(m_sceneHandle, SIGNAL(sceneSwitched()), this,
            SLOT(setCurrentScene()));
  }
  if (m_xsheetHandle)
    connect(m_xsheetHandle, SIGNAL(xsheetChanged()), SLOT(setCurrentFx()));
  if (m_levelHandle)
    connect(m_levelHandle, SIGNAL(xshLevelChanged()), SLOT(setCurrentFx()));
  if (m_objectHandle)
    connect(m_objectHandle, SIGNAL(objectChanged(bool)), SLOT(setCurrentFx()));
}

//-----------------------------------------------------------------------------

void FxSettings::hideEvent(QHideEvent *) {
  setFx(0, 0);
  disconnect(m_frameHandle, SIGNAL(frameSwitched()), this,
             SLOT(setCurrentFrame()));
  if (m_fxHandle) {
    disconnect(m_fxHandle, SIGNAL(fxChanged()), this, SLOT(setCurrentFx()));
    disconnect(m_fxHandle, SIGNAL(fxChanged()), this,
               SLOT(updateParamViewer()));
    disconnect(m_fxHandle, SIGNAL(fxSettingsShouldBeSwitched()), this,
               SLOT(setCurrentFx()));
  }
  if (m_sceneHandle) {
    disconnect(m_sceneHandle, SIGNAL(sceneChanged()), this,
               SLOT(notifySceneChanged()));
    disconnect(m_sceneHandle, SIGNAL(sceneSwitched()), this,
               SLOT(setCurrentScene()));
  }
  if (m_xsheetHandle)
    disconnect(m_xsheetHandle, SIGNAL(xsheetChanged()), this,
               SLOT(setCurrentFx()));
  if (m_levelHandle)
    disconnect(m_levelHandle, SIGNAL(xshLevelChanged()), this,
               SLOT(setCurrentFx()));
  if (m_objectHandle)
    disconnect(m_objectHandle, SIGNAL(objectChanged(bool)), this,
               SLOT(setCurrentFx()));
}

//-----------------------------------------------------------------------------

void FxSettings::setCheckboardColors(const TPixel32 &col1,
                                     const TPixel32 &col2) {
  m_checkCol1 = col1;
  m_checkCol2 = col2;
  if (m_checkboardBg->isChecked())
    m_viewer->setBgPainter(m_checkCol1, m_checkCol2);
}

//-----------------------------------------------------------------------------

void FxSettings::setWhiteBg() { m_viewer->setBgPainter(TPixel32::White); }

//-----------------------------------------------------------------------------

void FxSettings::setBlackBg() { m_viewer->setBgPainter(TPixel32::Black); }

//-----------------------------------------------------------------------------

void FxSettings::setCheckboardBg() {
  m_viewer->setBgPainter(m_checkCol1, m_checkCol2);
}

//-----------------------------------------------------------------------------

void FxSettings::updateViewer() {
  if (m_viewer->isEnabled())
    m_viewer->updateFrame(m_frameHandle->getFrameIndex());
}

//-----------------------------------------------------------------------------

void FxSettings::updateParamViewer() {
  if (!m_paramViewer || !m_frameHandle) return;
  m_paramViewer->update(m_frameHandle->getFrameIndex(), true);
}

//-----------------------------------------------------------------------------

void FxSettings::onPointChanged(int index, const TPointD &p) {
  m_paramViewer->setPointValue(index, p);
}

//-----------------------------------------------------------------------------

void FxSettings::onViewModeChanged(QAction *triggeredAct) {
  setFocus();
  QString name             = triggeredAct->text();
  bool actIsChecked        = triggeredAct->isChecked();
  QList<QAction *> actions = m_toolBar->actions();
  QAction *cameraAct       = actions[0];
  QAction *previewAct      = actions[1];
  if (name == previewAct->text()) {
    if (cameraAct->isChecked()) cameraAct->setChecked(false);
    if (actIsChecked) {
      m_isCameraModeView = false;
      m_paramViewer->setIsCameraViewMode(false);
      setCurrentFx();
    }
    m_viewer->setEnable(actIsChecked);
  } else if (name == cameraAct->text()) {
    if (previewAct->isChecked()) previewAct->setChecked(false);
    if (actIsChecked) {
      m_isCameraModeView = true;
      m_paramViewer->setIsCameraViewMode(true);
      setCurrentFx();
    }
    m_viewer->setEnable(actIsChecked);
  }
}

//-----------------------------------------------------------------------------

void FxSettings::onPreferedSizeChanged(QSize pvBestSize) {
  QSize popupBestSize = pvBestSize;
  if (m_toolBar->isVisible()) {
    popupBestSize += QSize(0, m_viewer->height() + m_toolBar->height());
  }

  QDialog *popup = dynamic_cast<QDialog *>(parentWidget());
  if (popup) {
    QRect geom = popup->geometry();
    geom.setSize(popupBestSize);
    popup->setGeometry(geom);
    popup->update();
  }
}

//-----------------------------------------------------------------------------

void FxSettings::onShowSwatchButtonToggled(bool on) {
  QWidget *bottomContainer = widget(1);

  if (!on)
    m_container_height =
        bottomContainer->height() + handleWidth() /* ハンドル幅 */;

  bottomContainer->setVisible(on);

  QDialog *popup = dynamic_cast<QDialog *>(parentWidget());
  if (popup) {
    QRect geom = popup->geometry();

    int height_change = (on) ? m_container_height : -m_container_height;

    geom.setSize(geom.size() + QSize(0, height_change));
    popup->setGeometry(geom);
    popup->update();
  }
}

//-----------------------------------------------------------------------------
