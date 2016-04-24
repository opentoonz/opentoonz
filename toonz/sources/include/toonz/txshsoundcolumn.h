

#ifndef TXSHSOUNDCOLUMN_INCLUDED
#define TXSHSOUNDCOLUMN_INCLUDED

#include "toonz/txshcolumn.h"
#include "toonz/txshcell.h"
#include "tsound.h"

#include <QList>
#include <QTimer>

#undef DVAPI
#undef DVVAR
#ifdef TOONZLIB_EXPORTS
#define DVAPI DV_EXPORT_API
#define DVVAR DV_EXPORT_VAR
#else
#define DVAPI DV_IMPORT_API
#define DVVAR DV_IMPORT_VAR
#endif

//=============================================================================
//forward declarations
class TFilePath;
class ColumnLevel;

//=============================================================================
//!The TXshSoundColumn class provides a sound column in xsheet and allows its management through cell concept.
/*!Inherits \b TXshCellColumn. */
//=============================================================================

class DVAPI TXshSoundColumn : public QObject, public TXshCellColumn
{
	Q_OBJECT

	PERSIST_DECLARATION(TXshSoundColumn)
	TSoundOutputDevice *m_player;

	QList<ColumnLevel *> m_levels; // owner

	/*!Used to menage current playback.*/
	TSoundTrackP m_currentPlaySoundTrack;

	//! From 0.0 to 1.0
	double m_volume;
	bool m_isOldVersion;

	QTimer m_timer;

public:
	TXshSoundColumn();
	~TXshSoundColumn();

	TXshColumn::ColumnType getColumnType() const;
	TXshSoundColumn *getSoundColumn() { return this; }

	bool canSetCell(const TXshCell &cell) const;

	TXshColumn *clone() const;

	/*! Clear column and set src level. */
	void assignLevels(const TXshSoundColumn *src);

	void loadData(TIStream &is);
	void saveData(TOStream &os);

	/*! r0 : min row not empty, r1 : max row not empty. Return row count.*/
	int getRange(int &r0, int &r1) const;
	/*! Last not empty row - first not empty row. */
	int getRowCount() const;
	/*! Return max row not empty. */
	int getMaxFrame() const;
	/*! Return min row not empty.*/
	int getFirstRow() const;

	const TXshCell &getCell(int row) const;
	void getCells(int row, int rowCount, TXshCell cells[]);

	bool setCell(int row, const TXshCell &cell);
	/*! Return false if cannot set cells.*/
	bool setCells(int row, int rowCount, const TXshCell cells[]);

	void insertEmptyCells(int row, int rowCount);

	void clearCells(int row, int rowCount);
	void removeCells(int row, int rowCount);

	/*! Check if frames from \b row to \b row+rowCount are in sequence and collapse level if it is true. */
	void updateCells(int row, int rowCount);

	/*! Modify range of level sound in row. Return new range value of the level in row.
      N.B. Row must be the first or last cell of a sound level. */
	int modifyCellRange(int row, int delta, bool modifyStartValue);

	bool isCellEmpty(int row) const;
	/*! r0 : min row not empty of level in row, r1 : max row not empty of level in row.
      Return true if level range is not empty.*/
	bool getLevelRange(int row, int &r0, int &r1) const;
	/*! r0 : min possible (without offset) row of level in row, r1 : max possible (without offset) row of level in row.
      Return true if level range is not empty.*/
	bool getLevelRangeWithoutOffset(int row, int &r0, int &r1) const;

	/*! Only debug. */
	void checkColumn() const;

	void setXsheet(TXsheet *xsheet);
	void setFrameRate(double fps);
	void updateFrameRate(double fps);

	//! From 0.0 to 1.0
	void setVolume(double value);
	double getVolume() const;

	TSoundTrackP getCurrentPlaySoundTruck() { return m_currentPlaySoundTrack; }

	//! s0 and s1 are samples
	void play(TSoundTrackP soundtrack, int s0, int s1, bool loop);
	/*! Play the whole soundSequence, currentFrame it is used to compute an offset
      when the user play a single level and hence the audio behind..*/
	void play(ColumnLevel *ss, int currentFrame);
	void play(int currentFrame = 0);
	void stop();

	bool isPlaying() const;

	void scrub(int fromFrame, int toFrame);

	TSoundTrackP getOverallSoundTrack(int fromFrame = -1, int toFram = -1, double fps = -1, TSoundTrackFormat format = TSoundTrackFormat());

	TSoundTrackP mixingTogether(const std::vector<TXshSoundColumn *> &vect, int fromFrame = -1, int toFram = -1, double fps = -1);

protected:
	bool setCell(int row, const TXshCell &cell, bool updateSequence);
	void removeCells(int row, int rowCount, bool shift);

	void setCellInEmptyFrame(int row, const TXshCell &cell);

	/*! If index == -1 insert soundColumnLevel at last and than order soundColumnLevel by startFrame. */
	void insertColumnLevel(ColumnLevel *columnLevel, int index = -1);
	void removeColumnLevel(ColumnLevel *columnLevel);

	ColumnLevel *getColumnLevelByFrame(int frame) const;
	ColumnLevel *getColumnLevel(int index);
	int getColumnLevelIndex(ColumnLevel *ss) const;
	void clear();

protected slots:
	void onTimerOut();
};

#ifdef _WIN32
template class DV_EXPORT_API TSmartPointerT<TXshSoundColumn>;
#endif
typedef TSmartPointerT<TXshSoundColumn> TXshSoundColumnP;

#endif
