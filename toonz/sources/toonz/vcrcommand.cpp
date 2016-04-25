

// Tnz6 includes
#include "menubarcommandids.h"
#include "tapp.h"

// TnzQt includes
#include "toonzqt/menubarcommand.h"
#include "toonzqt/flipconsole.h"

// TnzLib includes
#include "toonz/txsheet.h"
#include "toonz/txshcell.h"
#include "toonz/txsheethandle.h"
#include "toonz/tframehandle.h"
#include "toonz/tcolumnhandle.h"
#include "toonz/preferences.h"

//**********************************************************************************
//    Commands  definition
//**********************************************************************************

class LinkToggleCommand : public MenuItemHandler
{
public:
	LinkToggleCommand() : MenuItemHandler("MI_Link") {}
	void execute()
	{
		FlipConsole::toggleLinked();
	}
} linkToggleCommand;

//-----------------------------------------------------------------------------

class VcrCommand : public MenuItemHandler
{
	FlipConsole::EGadget m_buttonId;

public:
	VcrCommand(const char *cmdId, FlipConsole::EGadget buttonId)
		: MenuItemHandler(cmdId), m_buttonId(buttonId) {}
	void execute()
	{
		FlipConsole *console = FlipConsole::getCurrent();
		if (console)
			console->pressButton(m_buttonId);
	}
};

//-----------------------------------------------------------------------------

class NextDrawingCommand : public MenuItemHandler
{
public:
	NextDrawingCommand() : MenuItemHandler(MI_NextDrawing) {}

	void execute()
	{
		TXsheet *xsh = TApp::instance()->getCurrentXsheet()->getXsheet();

		int row = TApp::instance()->getCurrentFrame()->getFrame();
		int col = TApp::instance()->getCurrentColumn()->getColumnIndex();

		const TXshCell &cell = xsh->getCell(row, col);
		if (cell.isEmpty())
			return;

		for (++row; xsh->getCell(row, col) == cell; ++row)
			;

		if (!xsh->getCell(row, col).isEmpty())
			TApp::instance()->getCurrentFrame()->setFrame(row);
	}
};

//-----------------------------------------------------------------------------

class PrevDrawingCommand : public MenuItemHandler
{
public:
	PrevDrawingCommand() : MenuItemHandler(MI_PrevDrawing) {}

	void execute()
	{
		TXsheet *xsh = TApp::instance()->getCurrentXsheet()->getXsheet();

		int row = TApp::instance()->getCurrentFrame()->getFrame();
		int col = TApp::instance()->getCurrentColumn()->getColumnIndex();

		TXshCell cell = xsh->getCell(row, col);
		if (cell.isEmpty())
			return;

		for (--row; row >= 0 && xsh->getCell(row, col) == cell; --row) // Get *last* cell in previous uniform
			;														   // cell block

		if (row >= 0 && !xsh->getCell(row, col).isEmpty()) {
			cell = xsh->getCell(row, col);
			while (row > 0 && xsh->getCell(row - 1, col) == cell) // Get *first* cell in current uniform
				--row;											  // cell block

			TApp::instance()->getCurrentFrame()->setFrame(row);
		}
	}
};

//-----------------------------------------------------------------------------

class NextStepCommand : public MenuItemHandler
{
public:
	NextStepCommand() : MenuItemHandler(MI_NextStep) {}

	void execute()
	{
		int row = TApp::instance()->getCurrentFrame()->getFrame();
		int step = Preferences::instance()->getXsheetStep();

		TApp::instance()->getCurrentFrame()->setFrame(row + step);
	}
};

//-----------------------------------------------------------------------------

class PrevStepCommand : public MenuItemHandler
{
public:
	PrevStepCommand() : MenuItemHandler(MI_PrevStep) {}

	void execute()
	{
		int row = TApp::instance()->getCurrentFrame()->getFrame();
		int step = Preferences::instance()->getXsheetStep();

		TApp::instance()->getCurrentFrame()->setFrame(std::max(row - step, 0));
	}
};

//**********************************************************************************
//    Commands  instantiation
//**********************************************************************************

VcrCommand
	playCommand(MI_Play, FlipConsole::ePlay),
	pauseCommand(MI_Pause, FlipConsole::ePause),
	loopCommand(MI_Loop, FlipConsole::eLoop),
	firstFrameCommand(MI_FirstFrame, FlipConsole::eFirst),
	lastFrameCommand(MI_LastFrame, FlipConsole::eLast),
	nextFrameCommand(MI_NextFrame, FlipConsole::eNext),
	prevFrameCommand(MI_PrevFrame, FlipConsole::ePrev),

	redChannelCommand(MI_RedChannel, FlipConsole::eRed),
	greenChannelCommand(MI_GreenChannel, FlipConsole::eGreen),
	blueChannelCommand(MI_BlueChannel, FlipConsole::eBlue),
	matteChannelCommand(MI_MatteChannel, FlipConsole::eMatte),

	redChannelGCommand(MI_RedChannelGreyscale, FlipConsole::eGRed),
	greenChannelGComman(MI_GreenChannelGreyscale, FlipConsole::eGGreen),
	blueChannelGCommand(MI_BlueChannelGreyscale, FlipConsole::eGBlue),

	compareCommand(MI_CompareToSnapshot, FlipConsole::eCompare);

NextDrawingCommand nextDrawingCommand;
PrevDrawingCommand prevDrawingCommand;
NextStepCommand nextStepCommand;
PrevStepCommand prevStepCommand;
