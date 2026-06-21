#pragma once

#include <QObject>
#include <QMap>
#include <QVariant>
#include <QString>

// Simplified key-value preferences store (replaces OpenToonz's 110+ item enum).
// Keys are dot-separated paths: "app.theme", "app.language", "editor.fontSize", etc.

class PreferenceSystem : public QObject {
    Q_OBJECT
public:
    static PreferenceSystem* instance();

    // Type-safe access
    QVariant value(const QString& key, const QVariant& defaultValue = QVariant()) const;
    bool boolValue(const QString& key, bool defaultValue = false) const;
    int intValue(const QString& key, int defaultValue = 0) const;
    double doubleValue(const QString& key, double defaultValue = 0.0) const;
    QString stringValue(const QString& key, const QString& defaultValue = QString()) const;

    void setValue(const QString& key, const QVariant& val);
    void load(const QString& filePath);
    void save(const QString& filePath);

signals:
    void preferenceChanged(const QString& key);

private:
    PreferenceSystem();
    QMap<QString, QVariant> m_values;
    static PreferenceSystem* m_instance;
};
