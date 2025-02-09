

#include "tools/toolhandle.h"
#include "toonz/stage2.h"
#include "tools/tool.h"
#include "tools/toolcommandids.h"
#include "timage.h"
// #include "tapp.h"
#include "toonzqt/menubarcommand.h"
#include "toonz/preferences.h"
#include <QAction>
#include <QMap>
#include <QDebug>

//=============================================================================
// ToolHandle
//-----------------------------------------------------------------------------

ToolHandle::ToolHandle()
    : m_tool(0)
    , m_toolName("")
    , m_toolTargetType(TTool::NoTarget)
    , m_storedToolName("")
    , m_toolIsBusy(false) {}

//-----------------------------------------------------------------------------

ToolHandle::~ToolHandle() {}

//-----------------------------------------------------------------------------

TTool *ToolHandle::getTool() const { return m_tool; }

//-----------------------------------------------------------------------------

void ToolHandle::setTool(QString name) {
  // If we're in navigation mode, only allow navigation-related tool changes
  if (m_inNavigationMode && name != "T_Hand" && name != "T_Rotate" &&
      name != "T_Zoom") {
    return;
  }

  m_oldToolName = m_toolName = name;

  TTool *tool = TTool::getTool(m_toolName.toStdString(),
                               (TTool::ToolTargetType)m_toolTargetType);
  if (tool == m_tool) return;

  if (m_tool) m_tool->onDeactivate();

  // Camera test uses the automatically activated CameraTestTool
  if (name != "T_CameraTest" && CameraTestCheck::instance()->isEnabled())
    CameraTestCheck::instance()->setIsEnabled(false);

  m_tool = tool;

  if (name != T_Hand && CleanupPreviewCheck::instance()->isEnabled()) {
    // When using a tool, you have to exit from cleanup preview mode
    QAction *act = CommandManager::instance()->getAction("MI_CleanupPreview");
    if (act) CommandManager::instance()->execute(act);
  }

  if (m_tool)  // Should always enter
  {
    m_tool->onActivate();
    emit toolSwitched();
  }
}

//-----------------------------------------------------------------------------

void ToolHandle::setSpacePressed(bool pressed) {
  if (m_spacePressed == pressed) return;

  m_spacePressed = pressed;
  if (pressed) {
    // Store current tool when entering navigation mode
    m_originalTool     = m_toolName;
    m_inNavigationMode = true;
    setTool("T_Hand");
  } else {
    // Restore original tool when exiting navigation mode
    m_inNavigationMode = false;
    // Reset all modifier states when exiting navigation mode
    m_ctrlPressed  = false;
    m_shiftPressed = false;
    if (!m_originalTool.isEmpty()) {
      setTool(m_originalTool);
      m_originalTool.clear();
    }
  }
  updateNavigationState();
}

void ToolHandle::setShiftPressed(bool pressed) {
  if (m_shiftPressed == pressed) return;

  m_shiftPressed = pressed;
  updateNavigationState();
}

void ToolHandle::setCtrlPressed(bool pressed) {
  if (m_ctrlPressed == pressed) return;

  m_ctrlPressed = pressed;
  updateNavigationState();
}

void ToolHandle::updateNavigationState() {
  // Only handle state changes when in navigation mode
  if (!m_inNavigationMode) return;

  if (m_spacePressed) {
    if (m_ctrlPressed) {
      setTool("T_Zoom");
    } else if (m_shiftPressed) {
      setTool("T_Rotate");
    } else {
      setTool("T_Hand");
    }
  }
}

//-----------------------------------------------------------------------------

void ToolHandle::storeTool() {
  m_storedToolName = m_toolName;
  m_storedToolTime.start();
}

//-----------------------------------------------------------------------------

void ToolHandle::restoreTool() {
  if (m_storedToolName != m_toolName && m_storedToolName != "") {
    // Only bypass timer when in navigation mode
    if (m_inNavigationMode) {
      setTool(m_storedToolName);
    }
    // For all other cases, use the timer check
    else if (m_storedToolTime.elapsed() >
             Preferences::instance()->getTempToolSwitchTimer()) {
      setTool(m_storedToolName);
    }
  }
}

//-----------------------------------------------------------------------------

void ToolHandle::setPseudoTool(QString name) {
  QString oldToolName = m_oldToolName;
  setTool(name);
  m_oldToolName = oldToolName;
}

//-----------------------------------------------------------------------------

void ToolHandle::unsetPseudoTool() {
  if (m_toolName != m_oldToolName) setTool(m_oldToolName);
}

//-----------------------------------------------------------------------------

void ToolHandle::setToolBusy(bool value) {
  m_toolIsBusy = value;
  if (!m_toolIsBusy) emit toolEditingFinished();
}

//-----------------------------------------------------------------------------

QIcon currentIcon;

static QIcon getCurrentIcon() { return currentIcon; }

//-----------------------------------------------------------------------------

/*
void ToolHandle::changeTool(QAction* action)
{
}
*/

//-----------------------------------------------------------------------------

void ToolHandle::onImageChanged(TImage::Type imageType) {
  TTool::ToolTargetType targetType = TTool::EmptyTarget;

  switch (imageType) {
  case TImage::RASTER:
    targetType = TTool::RasterImage;
    break;
  case TImage::TOONZ_RASTER:
    targetType = TTool::ToonzImage;
    break;
  case TImage::MESH:
    targetType = TTool::MeshImage;
    break;
  case TImage::META:
    targetType = TTool::MetaImage;
    break;
  case TImage::VECTOR:
    targetType = TTool::VectorImage;
    break;
  default:
    targetType = TTool::EmptyTarget;
    break;
  }

  if (targetType != m_toolTargetType) {
    m_toolTargetType = targetType;
    setTool(m_toolName);
  }

  if (m_tool) {
    m_tool->updateMatrix();
    m_tool->onImageChanged();
  }
}

//-----------------------------------------------------------------------------

void ToolHandle::updateMatrix() {
  if (m_tool) m_tool->updateMatrix();
}
