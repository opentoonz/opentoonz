#pragma once

#ifndef DVDIALOG_INCLUDED
#define DVDIALOG_INCLUDED

// TnzCore includes
#include "tcommon.h"
#include "tmsgcore.h"

// Qt includes
#include <QDialog>
#include <QVBoxLayout>
#include <QString>
#include <QProgressBar>
#include <QFrame>
#include <QSettings>

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
class QAbstractButton;
class QHBoxLayout;
class QVBoxLayout;
class QLayout;
class QLabel;
class TXsheetHandle;
class TPalette;

#ifdef _MSC_VER
#pragma warning(disable : 4251)
#endif

//=============================================================================
namespace DVGui {
//=============================================================================

const int WidgetHeight = 20;

class Dialog;
class MessageAndCheckboxDialog;

//=============================================================================

//-----------------------------------------------------------------------------

void DVAPI setDialogTitle(const QString &dialogTitle);

//-----------------------------------------------------------------------------

void DVAPI MsgBoxInPopup(MsgType type, const QString &text);

// ATTENZIONE: Valore di ritorno
// 0 = l'utente ha chiuso la finestra (dovrebbe corrispondere ad un cancel o ad
// un NO) - closed window
// 1 = primo bottone da sx premuto - first button selected
// 2 = secondo bottone da sx premuto - second button
// 3 = terzo bottone da sx premuto - third button
// 4 = fourth button

int DVAPI MsgBox(MsgType type, const QString &text,
                 const std::vector<QString> &buttons,
                 int defaultButtonIndex = 0, QWidget *parent = 0);

// QUESTION: due bottoni user defined
int DVAPI MsgBox(const QString &text, const QString &button1,
                 const QString &button2, int defaultButtonIndex = 0,
                 QWidget *parent = 0);

// QUESTION: tre bottoni user defined
int DVAPI MsgBox(const QString &text, const QString &button1,
                 const QString &button2, const QString &button3,
                 int defaultButtonIndex = 0, QWidget *parent = 0);

// QUESTION: four botton user defined
int DVAPI MsgBox(const QString &text, const QString &button1,
                 const QString &button2, const QString &button3,
                 const QString &button4, int defaultButtonIndex = 0,
                 QWidget *parent = 0);

Dialog DVAPI *createMsgBox(MsgType type, const QString &text,
                           const QStringList &buttons, int defaultButtonIndex,
                           QWidget *parent = 0);

MessageAndCheckboxDialog DVAPI *createMsgandCheckbox(
    MsgType type, const QString &text, const QString &checkBoxText,
    const QStringList &buttons, int defaultButtonIndex, QWidget *parent = 0);

// void DVAPI error(const QString &msg);
// void DVAPI info(const QString &msg);

//-----------------------------------------------------------------------------

QString DVAPI getText(const QString &title, const QString &label,
                      const QString &text = QString(), bool *ok = 0);

//=============================================================================
/*! \brief The Separator class provides a separator.

                Inherits \b QWidget.

                The separator can be text and line or only line. If QString \b
   name, passed to
                constructor, is not empty, separator is composed by text \b name
   and line;
                else separator is a line, this line width is DV dialog width,
   clearly taking care
                DV dialog margin.
                The separator can be horizontal (by default) or vertical,
   isVertical(), you can
                set it using function \b setOrientation().

                To add a separator to DV dialog \b Dialog you must create a new
   Separator
                and recall \b Dialog::addWidget(), or recall \b
   Dialog::addSeparator().

                \b Example: in a DV dialog \b Dialog
                \code
                        Separator* exampleNameAndLine = new
   Separator(QString("Example Name"));
                        addWidget(exampleNameAndLine);
                        Separator* exampleLine = new Separator("");
                        addWidget(exampleLine);
                \endcode
                or:
                \code
                        addSeparator(QString("Example Name"));
                        addSeparator();
                \endcode
                \b Result
                        \image html DialogSeparator.jpg
*/
class DVAPI Separator final : public QFrame {
  Q_OBJECT

  QString m_name;
  bool m_isHorizontal;

public:
  Separator(QString name = "", QWidget *parent = 0);
  ~Separator();

  /*!	Set dialog saparator \b name to name, if name is empty dialog separator
                  is a line. */
  void setName(const QString &name) { m_name = name; }
  QString getName() { return m_name; }

  /*!	Set dialog saparator orientation to horizontal if \b isHorizontal is
     true,
                  otherwise to vertical. */
  void setOrientation(bool isHorizontal) { m_isHorizontal = isHorizontal; }

  /*!	Return true if saparator orientation is horizontal, false otherwise. */
  bool isHorizontal() { return m_isHorizontal; }

protected:
  void paintEvent(QPaintEvent *event) override;
};

//-----------------------------------------------------------------------------

class DVAPI Dialog : public QDialog {
  Q_OBJECT
  static QSettings *m_settings;
  // If the dialog has button then is modal too.
  bool m_hasButton;
  QString m_name;
  int m_currentScreen = -1;
  // gmt. rendo m_buttonLayout protected per ovviare ad un problema
  // sull'addButtonBarWidget(). cfr filebrowserpopup.cpp.
  // Dobbiamo discutere di Dialog.

protected:
  QHBoxLayout *m_buttonLayout;
  QList<QLabel *> m_labelList;
  void resizeEvent(QResizeEvent *e) override;
  void moveEvent(QMoveEvent *e) override;

public:
  QVBoxLayout *m_topLayout;
  QFrame *m_mainFrame, *m_buttonFrame;

  QHBoxLayout *m_mainHLayout;
  bool m_isMainHLayout;

  QVBoxLayout *m_leftVLayout, *m_rightVLayout;
  bool m_isMainVLayout;

  int m_layoutSpacing;
  int m_layoutMargin;
  int m_labelWidth;

  std::vector<QWidget *> m_buttonBarWidgets;

public:
  // if 'name' is not empty, the dialog will remember its geometry between Toonz
  // sessions
  Dialog(QWidget *parent = 0, bool hasButton = false, bool hasFixedSize = true,
         const QString &name = QString());
  ~Dialog();

  void beginVLayout();
  void endVLayout();

  void beginHLayout();
  void endHLayout();

  void addWidget(QWidget *widget, bool isRight = true);
  void addWidgets(QWidget *firstW, QWidget *secondW);
  void addWidget(QString labelName, QWidget *widget);

  void addLayout(QLayout *layout, bool isRight = true);
  void addWidgetLayout(QWidget *widget, QLayout *layout);
  void addLayout(QString labelName, QLayout *layout);
  void addLayouts(QLayout *firstL, QLayout *secondL);

  void addSpacing(int spacing);
  void addSeparator(QString name = QString());

  void setAlignment(Qt::Alignment alignment);

  void setTopMargin(int margin);
  void setTopSpacing(int spacing);

  void setLabelWidth(int labelWidth);
  int getLabelWidth() const { return m_labelWidth; };

  void setLayoutInsertedSpacing(int spacing);
  int getLayoutInsertedSpacing();

  void setButtonBarMargin(int margin);
  void setButtonBarSpacing(int spacing);

  void addButtonBarWidget(QWidget *widget);
  void addButtonBarWidget(QWidget *first, QWidget *second);
  void addButtonBarWidget(QWidget *first, QWidget *second, QWidget *third);
  void addButtonBarWidget(QWidget *first, QWidget *second, QWidget *third,
                          QWidget *fourth);

  void hideEvent(QHideEvent *event) override;

  void clearButtonBar();
signals:
  void dialogClosed();
};

//-----------------------------------------------------------------------------

class DVAPI MessageAndCheckboxDialog final : public DVGui::Dialog {
  Q_OBJECT

  int m_checked = 0;

public:
  MessageAndCheckboxDialog(QWidget *parent = 0, bool hasButton = false,
                           bool hasFixedSize   = true,
                           const QString &name = QString());
  int getChecked() { return m_checked; }

public slots:
  void onCheckboxChanged(int checked);
  void onButtonPressed(int id);
};

//-----------------------------------------------------------------------------
/*! Is a modal dialog with exclusive list of radio button.
    Exec value depend to checked button.
    0 -> Cancel or Close Popup,
    1,2,3,... -> checkbox clicked.
*/
class DVAPI RadioButtonDialog final : public DVGui::Dialog {
  Q_OBJECT

  int m_result;

public:
  RadioButtonDialog(const QString &labelText,
                    const QList<QString> &radioButtonList, QWidget *parent = 0,
                    Qt::WindowFlags f = 0);

public Q_SLOTS:
  void onButtonClicked(int id);
  void onCancel();
  void onApply();
};

//-----------------------------------------------------------------------------

int DVAPI RadioButtonMsgBox(MsgType type, const QString &labelText,
                            const QList<QString> &buttons, QWidget *parent = 0);

//-----------------------------------------------------------------------------

class DVAPI ProgressDialog : public DVGui::Dialog {
  Q_OBJECT

  QLabel *m_label;
  QProgressBar *m_progressBar;
  QPushButton *m_cancelButton;

protected:
  bool m_isCanceled;

public:
  ProgressDialog(const QString &labelText, const QString &cancelButtonText,
                 int minimum, int maximum, QWidget *parent = 0,
                 Qt::WindowFlags f = 0);

  void setLabelText(const QString &text);
  void setCancelButton(QPushButton *cancelButton);

  int maximum();
  void setMaximum(int maximum);

  int minimum();
  void setMinimum(int minimum);

  void reset();
  int value();

  bool wasCanceled() const;

public Q_SLOTS:
  void setValue(int progress);
  virtual void onCancel();

Q_SIGNALS:
  void canceled();
};

//-----------------------------------------------------------------------------
/*! Return 2 if erase style,
                                         1 if don't erase style,
                                         0 if press cancel or close popup.
          If newPalette != 0 verify if styles to erase are in new palette before
   send question.
*/
int eraseStylesInDemand(TPalette *palette, const TXsheetHandle *xsheetHandle,
                        TPalette *newPalette = 0);

int eraseStylesInDemand(TPalette *palette, std::vector<int> styleIds,
                        const TXsheetHandle *xsheetHandle);

//-----------------------------------------------------------------------------

//-----------------------------------------------------------------------------
}  // namespace DVGui
//-----------------------------------------------------------------------------

#endif  // DVDIALOG_INCLUDED
