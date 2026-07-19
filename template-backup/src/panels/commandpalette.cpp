#include "commandpalette.h"
#include "../framework/menubarcommand.h"
#include <QVBoxLayout>

CommandPalette::CommandPalette(QWidget* parent) : QWidget(parent) {
    auto* layout = new QVBoxLayout(this);
    layout->setContentsMargins(4, 4, 4, 4);
    layout->setSpacing(2);

    m_input = new QLineEdit(this);
    m_input->setPlaceholderText("Type command... (try 'log', 'canvas', 'quit')");
    m_input->setStyleSheet("background-color: #3c3c3c; color: #cccccc; padding: 4px;");
    layout->addWidget(m_input);

    m_list = new QListWidget(this);
    m_list->setAlternatingRowColors(true);
    m_list->setStyleSheet("background-color: #252526; color: #cccccc;");
    layout->addWidget(m_list);

    connect(m_input, &QLineEdit::textChanged, this, &CommandPalette::onTextChanged);
    connect(m_list, &QListWidget::itemActivated, this, &CommandPalette::onItemActivated);
}

void CommandPalette::onTextChanged(const QString& text) {
    m_list->clear();
    if (text.isEmpty()) return;

    struct Cmd { QString id; QString desc; };
    QList<Cmd> commands = {
        {"MI_OpenLogPanel", "Open Log Panel"},
        {"MI_OpenCanvasPanel", "Open Canvas"},
        {"MI_OpenPropertyPanel", "Open Property Inspector"},
        {"MI_OpenCommandPalette", "Open Command Palette"},
        {"MI_OpenWelcomePanel", "Open Welcome Panel"},
        {"MI_SaveLayout", "Save Layout"},
        {"MI_LoadLayout", "Load Layout"},
        {"MI_Quit", "Quit Application"},
    };

    QString lower = text.toLower();
    for (const auto& cmd : commands) {
        if (cmd.desc.toLower().contains(lower) || cmd.id.toLower().contains(lower)) {
            auto* item = new QListWidgetItem(cmd.desc);
            item->setData(Qt::UserRole, cmd.id);
            m_list->addItem(item);
        }
    }
}

void CommandPalette::onItemActivated(QListWidgetItem* item) {
    CommandManager::instance()->execute(item->data(Qt::UserRole).toString().toUtf8().constData());
    m_input->clear();
    m_list->clear();
}
