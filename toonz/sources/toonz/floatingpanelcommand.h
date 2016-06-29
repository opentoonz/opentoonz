#pragma once

#ifndef FLOATING_PANEL_COMMAND_INCLUDED
#define FLOATING_PANEL_COMMAND_INCLUDED

// #include "pane.h"
#include "toonzqt/menubarcommand.h"

class TPanel;

// usage: OpenFloatingPanel openMyWidget(MI_OpenMyWidget, "paneName" , tr("My
// Widget"));
// the "paneName" create the right widget and set it to the pane

class OpenFloatingPanel final : public MenuItemHandler {
  QString m_title;
  std::string m_panelType;

public:
  OpenFloatingPanel(CommandId id, const std::string &panelType, QString title);
  void execute() override;

  static TPanel *getOrOpenFloatingPanel(const std::string &panelType);
};

#endif
