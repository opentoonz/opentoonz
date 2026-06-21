#include "prefsystem.h"
#include <QFile>
#include <QXmlStreamReader>
#include <QXmlStreamWriter>
#include <QDebug>

PreferenceSystem* PreferenceSystem::m_instance = nullptr;

PreferenceSystem* PreferenceSystem::instance() {
    if (!m_instance) m_instance = new PreferenceSystem();
    return m_instance;
}

PreferenceSystem::PreferenceSystem() {}

QVariant PreferenceSystem::value(const QString& key, const QVariant& defaultValue) const {
    return m_values.value(key, defaultValue);
}

bool PreferenceSystem::boolValue(const QString& key, bool defaultValue) const {
    return value(key, defaultValue).toBool();
}

int PreferenceSystem::intValue(const QString& key, int defaultValue) const {
    return value(key, defaultValue).toInt();
}

double PreferenceSystem::doubleValue(const QString& key, double defaultValue) const {
    return value(key, defaultValue).toDouble();
}

QString PreferenceSystem::stringValue(const QString& key, const QString& defaultValue) const {
    return value(key, defaultValue).toString();
}

void PreferenceSystem::setValue(const QString& key, const QVariant& val) {
    m_values[key] = val;
}

void PreferenceSystem::load(const QString& filePath) {
    QFile file(filePath);
    if (!file.open(QIODevice::ReadOnly)) return;
    QXmlStreamReader xml(&file);
    while (!xml.atEnd() && !xml.hasError()) {
        xml.readNext();
        if (xml.isStartElement() && xml.name() == QStringLiteral("pref")) {
            QString key = xml.attributes().value("key").toString();
            QString val = xml.attributes().value("value").toString();
            if (!key.isEmpty()) m_values[key] = val;
        }
    }
    file.close();
}

void PreferenceSystem::save(const QString& filePath) {
    QFile file(filePath);
    if (!file.open(QIODevice::WriteOnly | QIODevice::Truncate)) return;
    QXmlStreamWriter xml(&file);
    xml.setAutoFormatting(true);
    xml.writeStartDocument();
    xml.writeStartElement("preferences");
    for (auto it = m_values.begin(); it != m_values.end(); ++it) {
        xml.writeStartElement("pref");
        xml.writeAttribute("key", it.key());
        xml.writeAttribute("value", it.value().toString());
        xml.writeEndElement();
    }
    xml.writeEndElement();
    xml.writeEndDocument();
    file.close();
}
