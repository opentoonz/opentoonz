

#include "tsio_raw.h"
#include "tsound_t.h"
#include "tsystem.h"
#include "tfilepath_io.h"
using namespace std;

//==============================================================================

TSoundTrackReaderRaw::TSoundTrackReaderRaw(const TFilePath &fp)
    : TSoundTrackReader(fp) {}

//------------------------------------------------------------------------------

TSoundTrackP TSoundTrackReaderRaw::load() {
  Tifstream is(m_path);

  if (!is)
    throw TException(L"Unable to load the RAW file " + m_path.getWideString() +
                     L" : doesn't exist");

  is.seekg(0, ios_base::end);
  long sampleCount = is.tellg() / 2;
  is.seekg(0, ios_base::beg);

  TSoundTrack *track = new TSoundTrackMono16(22050, 1, sampleCount);
  is.read((char *)track->getRawData(), sampleCount * 2);
  return track;
}

//==============================================================================

TSoundTrackWriterRaw::TSoundTrackWriterRaw(const TFilePath &fp)
    : TSoundTrackWriter(fp) {}

//------------------------------------------------------------------------------

bool TSoundTrackWriterRaw::save(const TSoundTrackP &track) {
  TFileStatus fs(m_path);
  if (fs.doesExist() && !fs.isWritable())
    throw TException(L"Unable to save the soundtrack: " +
                     m_path.getWideString() + L" is read-only");

  Tofstream os(m_path);

  // inserisco i dati nell'output stream

  // os << (ULONG) (track->getSampleCount()) << " ";

  int sampleCount = track->getSampleCount();
  int i;

  switch (track->getChannelCount()) {
  case 1:  // mono
    switch (track->getSampleSize()) {
    case 1:  // un byte per campione
    {
      const char *srcCharSamples = (char *)track->getRawData();
      for (i = 0; i < sampleCount; i++) {
        short sample = *(srcCharSamples + i);
        os.write((char *)&sample, sizeof(short));
      }

      break;
    }
    case 2:  // due byte per campione
    {
      const short *srcShortSamples = (short *)track->getRawData();
      for (i = 0; i < sampleCount; i++)
        os.write((char *)(srcShortSamples + i), sizeof(short));

      break;
    }
    }
    break;

  case 2:  // stereo
    switch (track->getSampleSize()) {
    case 2:  // due byte per campione, un byte per canale
    {
      // considera solo il canale sinistro
      const char *srcCharSamples = (char *)track->getRawData();
      for (i = 0; i < sampleCount; i += 2) {
        short sample = *(srcCharSamples + i);
        os.write((char *)&sample, sizeof(short));
      }

      break;
    }
    case 4:  // quattro byte per campione, due byte per canale
    {
      // considera solo il canale sinistro
      const short *srcShortSamples = (short *)track->getRawData();
      for (i = 0; i < sampleCount; i += 2)
        os.write((char *)(srcShortSamples + i), sizeof(short));

      break;
    }
    }

    break;
  default:
    break;
  }
  return true;
}
