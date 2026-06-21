#pragma once

#include <QSqlDatabase>
#include <QSqlQuery>
#include <QSqlError>
#include <QStandardPaths>
#include <QDir>
#include <QString>
#include <QDebug>
#include <functional>

// Lightweight SQLite database helper.
// Usage:
//   Database db("mydata.db");
//   db.execute("CREATE TABLE IF NOT EXISTS items (id INTEGER PRIMARY KEY, name TEXT)");
//   db.query("SELECT * FROM items", [](QSqlQuery& q) { while(q.next()) { ... } });

class Database {
public:
    explicit Database(const QString& name, const QString& connectionName = "default") {
        QString dir = QStandardPaths::writableLocation(QStandardPaths::AppDataLocation);
        QDir().mkpath(dir);
        QString path = dir + "/" + name;

        m_db = QSqlDatabase::addDatabase("QSQLITE", connectionName);
        m_db.setDatabaseName(path);

        if (!m_db.open()) {
            qCritical() << "Failed to open database:" << m_db.lastError().text();
        }
    }

    ~Database() {
        if (m_db.isOpen()) m_db.close();
    }

    bool execute(const QString& sql) {
        QSqlQuery q(m_db);
        if (!q.exec(sql)) {
            qWarning() << "SQL error:" << q.lastError().text() << "|" << sql;
            return false;
        }
        return true;
    }

    void query(const QString& sql, std::function<void(QSqlQuery&)> callback) {
        QSqlQuery q(m_db);
        if (q.exec(sql)) {
            callback(q);
        } else {
            qWarning() << "Query error:" << q.lastError().text() << "|" << sql;
        }
    }

    QSqlDatabase& handle() { return m_db; }

private:
    QSqlDatabase m_db;
};
