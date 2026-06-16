#pragma once

#include <QDialog>
#include <QListWidget>
#include <QStackedWidget>
#include <QGridLayout>
#include <QMap>
#include <QString>
#include <QVariant>
#include <QPair>

// Mirrors OpenToonz's PreferencesPopup architecture:
//   QListWidget (left) + QStackedWidget (right)

class QComboBox;
class QGroupBox;

namespace DVGui {
class CheckBox;
class IntLineEdit;
class DoubleLineEdit;
class FileField;
}

class PreferencesPopup : public QDialog {
    Q_OBJECT
public:
    PreferencesPopup(QWidget* parent = nullptr);

private:
    QListWidget* m_categoryList;
    QStackedWidget* m_pagesStack;

    // Key to widget map for unified onChange handling
    QMap<QWidget*, QString> m_widgetKeyMap;

    // Category registration
    void addCategory(const QString& name, QWidget* page);

    // Widget factory (mirrors createUI pattern)
    QWidget* createCheckBox(const QString& key, const QString& label, bool defaultValue);
    QWidget* createIntField(const QString& key, const QString& label,
                            int value, int min, int max);
    QWidget* createDoubleField(const QString& key, const QString& label,
                               double value, double min, double max);
    QWidget* createComboBox(const QString& key, const QString& label,
                            const QList<QPair<QString, QVariant>>& items,
                            const QVariant& currentValue);
    QWidget* createFileField(const QString& key, const QString& label,
                             const QString& path);

    void insertUI(QGridLayout* layout, int row, const QString& label, QWidget* widget);
    void insertFootNote(QGridLayout* layout, int row);

    // 3 demo pages
    QWidget* createGeneralPage();
    QWidget* createAppearancePage();
    QWidget* createAdvancedPage();

    // Save/Load
    void loadPreferences();
    void savePreferences();

private slots:
    void onCategoryChanged(int index);
    void onChange();
};
