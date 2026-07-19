#include "logpanel.h"

LogPanel::LogPanel(QWidget* parent) : QPlainTextEdit(parent) {
    setReadOnly(true);
    setMaximumBlockCount(1000);
    setStyleSheet("background-color: #1e1e1e; color: #d4d4d4;");
    appendLog("Log panel initialized.");
}

void LogPanel::appendLog(const QString& message) {
    QString timestamp = QDateTime::currentDateTime().toString("hh:mm:ss");
    appendPlainText(QString("[%1] %2").arg(timestamp, message));
}

void LogPanel::clearLog() {
    clear();
}
