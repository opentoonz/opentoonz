

#ifndef TXSHSOUNDLEVEL_INCLUDED
#define TXSHSOUNDLEVEL_INCLUDED

#include "toonz/txshlevel.h"
#include "tsound.h"

#include <QList>

#include "tpersist.h"

#undef DVAPI
#undef DVVAR
#ifdef TOONZLIB_EXPORTS
#define DVAPI DV_EXPORT_API
#define DVVAR DV_EXPORT_VAR
#else
#define DVAPI DV_IMPORT_API
#define DVVAR DV_IMPORT_VAR
#endif

class DVAPI TXshSoundLevel : public TXshLevel
{
	PERSIST_DECLARATION(TXshSoundLevel)

	TSoundTrackP m_soundTrack;

	double m_duration; // overall soundtrack duration in seconds
	double m_samplePerFrame;
	int m_frameSoundCount;
	double m_fps;
	//!Values is a map of \b Integer and \b DoublePair.
	/*!Integer is horizontal value of row pixel.
     DoublePair is computed according to frameRate, frameCount
     and soundtrack pressure.*/
	std::map<int, DoublePair> m_values;

	TFilePath m_path;

	DECLARE_CLASS_CODE
public:
	TXshSoundLevel(std::wstring name = L"", int startOffset = 0, int endOffset = 0);
	~TXshSoundLevel();

	TXshSoundLevel *clone() const;

	void setScene(ToonzScene *scene);

	//! Overridden from TXshLevel
	TXshSoundLevel *getSoundLevel() { return this; }

	void loadSoundTrack();
	void loadSoundTrack(const TFilePath &fileName);

	void loadData(TIStream &is);
	void saveData(TOStream &os);

	void load();
	void save();
	void save(const TFilePath &path);

	void computeValues(int frameHeight = 20);

	void getValueAtPixel(int pixel, DoublePair &values) const;

	/*! Set frame rate to \b fps. \sa getSamplePerFrame() */
	void setFrameRate(double fps);
	double getFrameRate() const { return m_fps; }

	void setPath(const TFilePath &path) { m_path = path; }
	TFilePath getPath() const { return m_path; }

	void setSoundTrack(TSoundTrackP st)
	{
		m_soundTrack = st;
		computeValues();
	}
	TSoundTrackP getSoundTrack() { return m_soundTrack; }

	//! Pay Attention this is the sound frame !!
	int getFrameSoundCount() const { return m_frameSoundCount; }

	int getSamplePerFrame() const { return m_samplePerFrame; }

	int getFrameCount() const;

	void getFids(std::vector<TFrameId> &fids) const;

private:
	// not implemented
	TXshSoundLevel(const TXshSoundLevel &);
	TXshSoundLevel &operator=(const TXshSoundLevel &);
};

#ifdef _WIN32
template class DV_EXPORT_API TSmartPointerT<TXshSoundLevel>;
#endif
typedef TSmartPointerT<TXshSoundLevel> TXshSoundLevelP;

#endif //TXSHSOUNDLEVEL_INCLUDED
