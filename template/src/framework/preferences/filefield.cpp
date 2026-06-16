#include "filefield.h"
#include "lineedit.h"
#include <QHBoxLayout>
#include <QPushButton>
#include <QFileDialog>

using namespace DVGui;

FileField::FileField(QWidget* parent, QString path, bool readOnly)
    : QWidget(parent)
    , m_fileMode(QFileDialog::Directory) {
    auto* layout = new QHBoxLayout(this);
    layout->setContentsMargins(0, 0, 0, 0);
    layout->setSpacing(2);

    m_field = new LineEdit(this);
    m_field->setReadOnly(readOnly);
    m_field->setText(path);
    layout->addWidget(m_field);

    m_fileBrowseButton = new QPushButton("...", this);
    m_fileBrowseButton->setFixedWidth(30);
    connect(m_fileBrowseButton, &QPushButton::clicked, this, &FileField::browseDirectory);
    layout->addWidget(m_fileBrowseButton);

    connect(m_field, &QLineEdit::textChanged, this, &FileField::pathChanged);
}

void FileField::setFileMode(QFileDialog::FileMode mode) { m_fileMode = mode; }
void FileField::setFilters(const QStringList& filters) { m_filters = filters; }
QString FileField::getPath() const { return m_field->text(); }
void FileField::setPath(const QString& path) { m_field->setText(path); }

void FileField::browseDirectory() {
    QString path;
    if (m_fileMode == QFileDialog::Directory) {
        path = QFileDialog::getExistingDirectory(this, QString(), getPath());
    } else {
        path = QFileDialog::getOpenFileName(this, QString(), getPath(),
            m_filters.join(";;"));
    }
    if (!path.isEmpty()) {
        setPath(path);
        emit pathChanged();
    }
}
