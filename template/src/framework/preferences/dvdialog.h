#pragma once

#include <QDialog>
#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QFrame>
#include <QLabel>
#include <QList>

namespace DVGui {

const int WidgetHeight = 20;

class Separator : public QFrame {
    Q_OBJECT
    QString m_name;
    bool m_isHorizontal;
public:
    Separator(QString name = "", QWidget* parent = nullptr, bool isHorizontal = true);
    ~Separator() {}
protected:
    void paintEvent(QPaintEvent* event) override;
};

class Dialog : public QDialog {
    Q_OBJECT
public:
    QVBoxLayout* m_topLayout;
    QFrame* m_mainFrame;
    QFrame* m_buttonFrame;
    QHBoxLayout* m_buttonLayout;

    Dialog(QWidget* parent = nullptr, bool hasButton = false, bool hasFixedSize = true,
           const QString& name = QString());
    ~Dialog();

    void addWidget(QWidget* widget);
    void addWidget(const QString& labelName, QWidget* widget);
    void addSeparator(const QString& name = QString());
    void setTopMargin(int margin);
    void setTopSpacing(int spacing);
    int getLabelWidth() const;
    void setLabelWidth(int width);
signals:
    void dialogClosed();
};

} // namespace DVGui
