#pragma once

#include <QWidget>
#include <QFileDialog>
#include <QStringList>

class QPushButton;

namespace DVGui {

class LineEdit;

class FileField : public QWidget {
    Q_OBJECT
public:
    FileField(QWidget* parent = nullptr, QString path = QString(), bool readOnly = false);
    ~FileField() {}

    void setFileMode(QFileDialog::FileMode mode);
    void setFilters(const QStringList& filters);
    QString getPath() const;
    void setPath(const QString& path);

signals:
    void pathChanged();

protected:
    QPushButton* m_fileBrowseButton;
    LineEdit* m_field;
    QStringList m_filters;
    QFileDialog::FileMode m_fileMode;

protected slots:
    virtual void browseDirectory();
};

} // namespace DVGui
