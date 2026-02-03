#pragma once

#ifndef VERSION_CONTROL_WIDGET_H
#define VERSION_CONTROL_WIDGET_H

#include <QWidget>
#include <QMap>
#include <QString>

class QLabel;
class QRadioButton;
class QSpinBox;
class QDateTimeEdit;
class QTimeEdit;
class QVBoxLayout;

class DoubleRadioWidget;

//-----------------------------------------------------------------------------

class DateChooserWidget final : public QWidget {
  Q_OBJECT

  int m_selectedRadioIndex;

  QRadioButton *m_timeRadioButton, *m_dayRadioButton, *m_weekRadioButton,
      *m_dateRadioButton;

  QTimeEdit *m_timeEdit;
  QSpinBox *m_dayEdit;
  QSpinBox *m_weekEdit;
  QDateTimeEdit *m_dateTimeEdit;

public:
  DateChooserWidget(QWidget *parent = 0);

  QString getRevisionString() const;

private:
  void disableAllWidgets();

protected slots:
  void onRadioButtonClicked();
};

//-----------------------------------------------------------------------------

class ConflictWidget final : public QWidget {
  Q_OBJECT

  QVBoxLayout *m_mainLayout;
  QMap<DoubleRadioWidget *, int> m_radios;

  QString m_button1Text;
  QString m_button2Text;

public:
  ConflictWidget(QWidget *parent = 0);
  void setFiles(const QStringList &files);

  QStringList getFilesWithOption(int option) const;

  void setButton1Text(const QString &b1text) { m_button1Text = b1text; }
  void setButton2Text(const QString &b2text) { m_button2Text = b2text; }

protected slots:
  void onRadioValueChanged();

signals:
  void allConflictSetted();
};

//-----------------------------------------------------------------------------

class DoubleRadioWidget final : public QWidget {
  Q_OBJECT

  QLabel *m_label;
  QRadioButton *m_firstButton;
  QRadioButton *m_secondButton;

public:
  DoubleRadioWidget(const QString &button1Text, const QString &button2Text,
                    const QString &text, QWidget *parent = 0);

  // -1 No button checked, 0 firstButton, 1 secondButton
  int getValue() const;
  QString getText() const;

signals:
  void valueChanged();
};

#endif  // VERSION_CONTROL_WIDGET_H
