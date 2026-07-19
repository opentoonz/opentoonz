#pragma once

#include "menubarcommand.h"

class TPanel;

// usage: OpenFloatingPanel openMyWidget(MI_OpenMyWidget, "paneName", tr("My Widget"));
// the "paneName" matches a TPanelFactory key

class OpenFloatingPanel final : public MenuItemHandler {
    QString m_title;
    std::string m_panelType;

public:
    OpenFloatingPanel(CommandId id, const std::string& panelType, QString title);
    void execute() override;

    static TPanel* getOrOpenFloatingPanel(const std::string& panelType);
};
