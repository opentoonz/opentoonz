#pragma once
#include <QWidget>

class WelcomePanel : public QWidget {
    Q_OBJECT
public:
    explicit WelcomePanel(QWidget* parent = nullptr);
protected:
    void paintEvent(QPaintEvent*) override;
};
