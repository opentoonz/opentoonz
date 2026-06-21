#pragma once
#include <QWidget>
#include <QTabWidget>
#include <QLabel>
#include <QLineEdit>
#include <QSpinBox>
#include <QDoubleSpinBox>
#include <QComboBox>
#include <QPushButton>
#include <QGridLayout>
#include <QVBoxLayout>

// Figma-style Properties panel with Design / Prototype tabs
class PropertiesPanel : public QWidget {
    Q_OBJECT
public:
    explicit PropertiesPanel(QWidget* parent = nullptr);
private:
    QWidget* createDesignTab();
    QWidget* createPrototypeTab();
    QLabel* makeLabel(const QString& text);
    QLineEdit* makeField(const QString& value);
};
