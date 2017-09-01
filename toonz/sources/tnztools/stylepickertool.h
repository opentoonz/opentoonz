#pragma once

#ifndef STYLEPICKERTOOL_H
#define STYLEPICKERTOOL_H

#include "tools/tool.h"
#include "tproperty.h"
#include <QObject>
// For Qt translation support
#include <QCoreApplication>

class StylePickerTool final : public TTool, public QObject {
  Q_DECLARE_TR_FUNCTIONS(StylePickerTool)

  int m_oldStyleId, m_currentStyleId;

  TEnumProperty m_colorType;
  TPropertyGroup m_prop;
  TBoolProperty m_passivePick;

  TBoolProperty m_organizePalette;
  TPalette *m_paletteToBeOrganized;

  bool startOrganizePalette();

public:
  TPropertyGroup *getProperties(int targetType) override { return &m_prop; }

  StylePickerTool();

  ToolType getToolType() const override { return TTool::LevelReadTool; }

  void draw() override {}

  void leftButtonDown(const TPointD &pos, const TMouseEvent &e) override;

  void leftButtonDrag(const TPointD &pos, const TMouseEvent &e) override;

  void pick(const TPointD &pos, const TMouseEvent &e);

  void mouseMove(const TPointD &pos, const TMouseEvent &e) override;

  int getCursorId() const override;

  bool onPropertyChanged(std::string propertyName);

  bool isOrganizePaletteActive() { return m_organizePalette.getValue(); }

  /*
     This function is called when either frame/column/level/scene is switched or
     either scene/level/object is changed (see tapp.cpp).
     If the working palette is changed, then deactivate the "organize palette"
     toggle.
  */
  void onImageChanged() override;

  void updateTranslation() override;
};

#endif