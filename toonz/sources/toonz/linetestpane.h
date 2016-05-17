#pragma once

#ifndef LINETEST_PANE_INCLUDED
#define LINETEST_PANE_INCLUDED

#ifdef LINETEST

#include "pane.h"
#include "linetestviewer.h"
#include "sceneviewer.h"
#include "toonz/imagepainter.h"
#include "toonzqt/intfield.h"
#include "toonzqt/keyframenavigator.h"
#include "toonzqt/flipconsole.h"

#include <QThread>
#include <QWaitCondition>

class QStackedWidget;

//=============================================================================
// MixAudioThread
//-----------------------------------------------------------------------------

class MixAudioThread : public QThread
{
	Q_OBJECT

	bool m_abort;
	bool m_restart;

	int m_from;
	int m_to;

	QWaitCondition m_condition;

	TSoundTrackP m_computedBuffer;
	TSoundTrackP m_buffer;

public:
	QMutex m_mutex;

	MixAudioThread(QObject *parent = 0);
	~MixAudioThread();

	void computeBuffer(int fromFrame, int toFrame);

	TSoundTrackP getBuffer() { return m_buffer; }

protected:
	void run();

signals:
	void computedBuffer();
};

//=============================================================================
// LineTestPanel
//-----------------------------------------------------------------------------
//Andrebbe messo a fattor comune del codice con ViewerPane
class LineTestPane : public TPanel
{
	Q_OBJECT

	QStackedWidget *m_stackedWidget;
	SceneViewer *m_sceneViewer;
	LineTestViewer *m_lineTestViewer;
	FlipConsole *m_flipConsole;
	ViewerKeyframeNavigator *m_keyFrameButton;

	TSoundTrackP m_mainTrack, m_startTrack; //used in loop
	TSoundTrackP m_buffer;
	int m_bufferSize;	  // in Frames
	int m_nextBufferSize;  // in Frames
	int m_startBufferSize; // in Frames
	MixAudioThread m_mixAudioThread;
	TSoundOutputDevice *m_player;

	TPanelTitleBarButton *m_previewButton;
	TPanelTitleBarButton *m_cameraStandButton;

	// For audio playback
	int m_trackStartFrame;
	int m_startPlayRange, m_endPlayRange;

public:
	LineTestPane(QWidget *parent = 0, Qt::WFlags flags = 0);
	~LineTestPane();

protected:
	void showEvent(QShowEvent *);
	void hideEvent(QHideEvent *);

	int computeBufferSize(int frame);
	void computeBuffer(int frame);
	void initSound();
	void playSound();
	void initializeTitleBar(TPanelTitleBar *titleBar);

public slots:
	void changeWindowTitle();
	void onSceneChanged();
	void onXshLevelSwitched(TXshLevel *);
	void updateFrameRange();
	void updateFrameMarkers();

protected slots:
	void setCurrentViewType(int index);
	void onDrawFrame(int frame, const ImagePainter::VisualSettings &settings);
	void onComputedBuffer();
	void onPlayStateChanged(bool);
	void onFrameSwitched();
	void onSceneSwitched();
	void onFrameTypeChanged();
	void onFlipSliderReleased();
};

#endif //LINETEST

#endif
