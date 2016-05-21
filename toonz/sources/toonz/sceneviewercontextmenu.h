#pragma once

#ifndef SCENE_VIEWER_CONTEXT_MENU
#define SCENE_VIEWER_CONTEXT_MENU

#include <QMenu>
#include "tgeometry.h"

class TStageObjectId;
class SceneViewer;
class TXshColumn;

class SceneViewerContextMenu : public QMenu
{
	Q_OBJECT
	SceneViewer *m_viewer;
	int m_groupIndexToBeEntered;

	void addShowHideCommand(QMenu *menu, TXshColumn *column);
	void addSelectCommand(QMenu *menu, const TStageObjectId &id);

public:
	SceneViewerContextMenu(SceneViewer *parent);
	~SceneViewerContextMenu();

	void addEnterGroupCommands(const TPointD &pos);
	void addLevelCommands(std::vector<int> &indices);

public slots:

	void savePreviewedFrames();

	void enterVectorImageGroup();
	void exitVectorImageGroup();

	void onShowHide();
	void onSetCurrent();
};

#endif
