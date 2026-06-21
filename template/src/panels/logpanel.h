#pragma once
#include <QPlainTextEdit>
#include <QDateTime>

class LogPanel : public QPlainTextEdit {
    Q_OBJECT
public:
    explicit LogPanel(QWidget* parent = nullptr);
public slots:
    void appendLog(const QString& message);
    void clearLog();
};
