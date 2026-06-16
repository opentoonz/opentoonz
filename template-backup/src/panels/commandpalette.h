#pragma once
#include <QWidget>
#include <QLineEdit>
#include <QListWidget>

class CommandPalette : public QWidget {
    Q_OBJECT
public:
    explicit CommandPalette(QWidget* parent = nullptr);
private slots:
    void onTextChanged(const QString& text);
    void onItemActivated(QListWidgetItem* item);
private:
    QLineEdit* m_input;
    QListWidget* m_list;
};
