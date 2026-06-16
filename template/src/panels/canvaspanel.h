#pragma once
#include <QWidget>

class CanvasPanel : public QWidget {
    Q_OBJECT
public:
    explicit CanvasPanel(QWidget* parent = nullptr);
protected:
    void paintEvent(QPaintEvent*) override;
    void resizeEvent(QResizeEvent*) override;
private:
    QPixmap m_checker;
    void rebuildChecker();
};
