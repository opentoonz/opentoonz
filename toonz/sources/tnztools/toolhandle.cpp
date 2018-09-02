

#include "tools/toolhandle.h"
#include "toonz/stage2.h"
#include "tools/tool.h"
#include "tools/toolcommandids.h"
#include "timage.h"
//#include "tapp.h"
#include "toonzqt/menubarcommand.h"
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
  m_oldToolName = m_toolName = name;

  TTool *tool = TTool::getTool(m_toolName.toStdString(),
                               (TTool::ToolTargetType)m_toolTargetType);
  if (tool == m_tool) return;

  if (m_tool) m_tool->onDeactivate();

  // Camera test uses the automaticly activated CameraTestTool
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

void ToolHandle::storeTool() {
  m_storedToolName = m_toolName;
  m_storedToolTime.start();
}

//-----------------------------------------------------------------------------

void ToolHandle::restoreTool() {
  // qDebug() << m_storedToolTime.elapsed();
  if (m_storedToolName != m_toolName && m_storedToolName != "" &&
      m_storedToolTime.elapsed() > 500) {
    setTool(m_storedToolName);
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
  TTool::ToolTargetType targetType = TTool::NoTarget;

  switch (imageType) {
  case TImage::RASTER:
    targetType = TTool::RasterImage;
    break;
  case TImage::TOONZ_RASTER:
    targetType = TTool::ToonzImage;
    break;
  case TImage::VECTOR:
  default:
    targetType = TTool::VectorImage;
    break;
  case TImage::MESH:
    targetType = TTool::MeshImage;
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
