

#include "toonz/txshsoundlevel.h"
#include "tsound_io.h"
#include "toonz/toonzscene.h"
#include "toonz/sceneproperties.h"
#include "toonz/txshleveltypes.h"

#include "tstream.h"
#include "toutputproperties.h"
#include "tconvert.h"

//-----------------------------------------------------------------------------

DEFINE_CLASS_CODE(TXshSoundLevel, 53)

PERSIST_IDENTIFIER(TXshSoundLevel, "soundLevel")

//=============================================================================

TXshSoundLevel::TXshSoundLevel(std::wstring name, int startOffset,
                               int endOffset)
    : TXshLevel(m_classCode, name)
    , m_soundTrack(0)
    , m_duration(0)
    , m_samplePerFrame(0)
    , m_frameSoundCount(0)
    , m_fps(12)
    , m_path() {}

//-----------------------------------------------------------------------------

TXshSoundLevel::~TXshSoundLevel() {}

//-----------------------------------------------------------------------------

TXshSoundLevel *TXshSoundLevel::clone() const {
  TXshSoundLevel *sound = new TXshSoundLevel();
  sound->setSoundTrack(m_soundTrack->clone());
  sound->m_duration        = m_duration;
  sound->m_path            = m_path;
  sound->m_samplePerFrame  = m_samplePerFrame;
  sound->m_frameSoundCount = m_frameSoundCount;
  sound->m_fps             = m_fps;
  return sound;
}

//-----------------------------------------------------------------------------

void TXshSoundLevel::setScene(ToonzScene *scene) {
  assert(scene);
  TXshLevel::setScene(scene);
  TOutputProperties *properties = scene->getProperties()->getOutputProperties();
  assert(properties);
  setFrameRate(properties->getFrameRate());
}

//-----------------------------------------------------------------------------

void TXshSoundLevel::loadSoundTrack() {
  assert(getScene());

  TSceneProperties *properties = getScene()->getProperties();
  if (properties) {
    TOutputProperties *outputProperties = properties->getOutputProperties();
    if (outputProperties) m_fps         = outputProperties->getFrameRate();
  }
  TFilePath path = getScene()->decodeFilePath(m_path);
  try {
    loadSoundTrack(path);
  } catch (TException &e) {
    throw TException(e.getMessage());
  }
}

//-----------------------------------------------------------------------------

void TXshSoundLevel::loadSoundTrack(const TFilePath &fileName) {
  try {
    TSoundTrackP st;
    TFilePath path(fileName);
    bool ret = TSoundTrackReader::load(path, st);
    if (ret) {
      m_duration = st->getDuration();
      setName(fileName.getWideName());
      setSoundTrack(st);
    }
  } catch (TException &) {
    return;
  }
}

//-----------------------------------------------------------------------------

void TXshSoundLevel::load() { loadSoundTrack(); }

//-----------------------------------------------------------------------------

void TXshSoundLevel::save() { save(m_path); }

//-----------------------------------------------------------------------------

void TXshSoundLevel::save(const TFilePath &path) {
  TSoundTrackWriter::save(path, m_soundTrack);
}

//-----------------------------------------------------------------------------

void TXshSoundLevel::loadData(TIStream &is) {
  is >> m_name;
  setName(m_name);

  std::string tagName;
  bool flag = false;

  int type = UNKNOWN_XSHLEVEL;

  for (;;) {
    if (is.matchTag(tagName)) {
      if (tagName == "path") {
        is >> m_path;
        is.matchEndTag();
      } else if (tagName == "type") {
        std::string v;
        is >> v;
        if (v == "sound") type = SND_XSHLEVEL;
        is.matchEndTag();
      } else
        throw TException("unexpected tag " + tagName);
    } else
      break;
  }
  setType(type);
}

//-----------------------------------------------------------------------------

void TXshSoundLevel::saveData(TOStream &os) {
  os << m_name;

  std::map<std::string, std::string> attr;
  os.child("type") << L"sound";
  os.child("path") << m_path;
}

//-----------------------------------------------------------------------------

void TXshSoundLevel::computeValuesFor(const Orientation *o) {
  int frameHeight = o->dimension(PredefinedDimension::FRAME);  // time axis
  int index       = o->dimension(PredefinedDimension::INDEX);
  map<int, DoublePair> &values = m_values[index];

  if (frameHeight == 0) frameHeight = 1;
  values.clear();
  if (!m_soundTrack) {
    m_frameSoundCount = 0;
    m_samplePerFrame  = 0;
    return;
  }

  m_samplePerFrame      = m_soundTrack->getSampleRate() / m_fps;
  double samplePerPixel = m_samplePerFrame / frameHeight;

  int sampleCount = m_soundTrack->getSampleCount();
  if (sampleCount <= 0)  // This was if(!sampleCount)  :(
    return;              //

  m_frameSoundCount = tceil(sampleCount / m_samplePerFrame);

  double maxPressure = 0.0;
  double minPressure = 0.0;

  m_soundTrack->getMinMaxPressure(TINT32(0), (TINT32)sampleCount, TSound::LEFT,
                                  minPressure, maxPressure);

  double absMaxPressure = std::max(fabs(minPressure), fabs(maxPressure));

  if (absMaxPressure <= 0) return;

  // Adjusting using a fixed scaleFactor
  int desiredAmplitude = o->dimension(PredefinedDimension::SOUND_AMPLITUDE);
  // results will be in range -desiredAmplitude .. +desiredAmplitude
  double weightA = desiredAmplitude / absMaxPressure;

  long i = 0, j;
  long p = 0;  // se p parte da zero notazione per pixel,
  // se parte da 1 notazione per frame
  while (i < m_frameSoundCount) {
    for (j = 0; j < frameHeight - 1; ++j) {
      double min = 0.0;
      double max = 0.0;
      m_soundTrack->getMinMaxPressure(
          (TINT32)(i * m_samplePerFrame + j * samplePerPixel),
          (TINT32)(i * m_samplePerFrame + (j + 1) * samplePerPixel - 1),
          TSound::MONO, min, max);
      values.insert(std::pair<int, std::pair<double, double>>(
          p + j, std::pair<double, double>(min * weightA, max * weightA)));
    }

    double min = 0.0;
    double max = 0.0;
    m_soundTrack->getMinMaxPressure(
        (TINT32)(i * m_samplePerFrame + j * samplePerPixel),
        (TINT32)((i + 1) * m_samplePerFrame - 1), TSound::MONO, min, max);
    values.insert(std::pair<int, std::pair<double, double>>(
        p + j, std::pair<double, double>(min * weightA, max * weightA)));

    ++i;
    p += frameHeight;
  }
}

//-----------------------------------------------------------------------------

void TXshSoundLevel::computeValues() {
  for (auto o : Orientations::all()) computeValuesFor(o);
}

//-----------------------------------------------------------------------------

void TXshSoundLevel::getValueAtPixel(const Orientation *o, int pixel,
                                     DoublePair &values) const {
  int index = o->dimension(PredefinedDimension::INDEX);
  std::map<int, DoublePair>::const_iterator it = m_values[index].find(pixel);
  if (it != m_values[index].end()) values = it->second;
}

//-----------------------------------------------------------------------------

void TXshSoundLevel::setFrameRate(double fps) {
  if (m_fps != fps) {
    m_fps = fps;
    computeValues();
  }
}

//-----------------------------------------------------------------------------

int TXshSoundLevel::getFrameCount() const {
  int frameCount = m_duration * m_fps;
  return (frameCount == 0) ? 1 : frameCount;
}

//-----------------------------------------------------------------------------
// Implementato per utilita'
void TXshSoundLevel::getFids(std::vector<TFrameId> &fids) const {
  int i;
  for (i = 0; i < getFrameCount(); i++) fids.push_back(TFrameId(i));
}
