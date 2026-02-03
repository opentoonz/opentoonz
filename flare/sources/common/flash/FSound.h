#ifndef SNDMIX_INCLUDED
#define SNDMIX_INCLUDED

// #include <windows.h>
// #include <mmreg.h>
#include <assert.h>
#include <stdio.h>
#include <vector>

#include "Macromedia.h"
#include "FFixed.h"

struct WaveFormat {
	U16 wFormatTag;		 /* format type */
	U16 nChannels;		 /* number of channels (i.e. mono, stereo...) */
	U32 nSamplesPerSec;  /* sample rate */
	U32 nAvgBytesPerSec; /* for buffer estimation */
	U16 nBlockAlign;	 /* block size of data */
	U16 wBitsPerSample;  /* number of bits per sample of mono data */
	U16 cbSize;			 /* the count in bytes of the size of */
};

// Our supported sample rates
// Note that sndRate5K is really 5512.5khz
// Use defines instead of an enum since these must be 32 bits on all platforms
#define sndRate5K 5512L
#define sndRate11K 11025L
#define sndRate22K 22050L
#define sndRate44K 44100L

const int sndMono = 0x0;
const int sndStereo = 0x1;

const int snd8Bit = 0x0;
const int snd16Bit = 0x2;

const int snd5K = 0 << 2;
const int snd11K = 1 << 2;
const int snd22K = 2 << 2;
const int snd44K = 3 << 2;

const int sndCompressNone = 0x00; // we could add 14 more compression types here...
const int sndCompressADPCM = 0x10;
const int sndCompressMP3 = 0x20;
const int sndCompressNoneI = 0x30; // save out in intel byte order
const int sndRateMask = 0x3 << 2;
const int sndCompressMask = 0xF0;

enum { // Sound format types
	snd5K8Mono = 0,
	snd5K8Stereo,
	snd5K16Mono,
	snd5K16Stereo,
	snd11K8Mono,
	snd11K8Stereo,
	snd11K16Mono,
	snd11K16Stereo,
	snd22K8Mono,
	snd22K8Stereo,
	snd22K16Mono,
	snd22K16Stereo,
	snd44K8Mono,
	snd44K8Stereo,
	snd44K16Mono,
	snd44K16Stereo
};

// This object defines a sound sample
class FSound
{
public:
	int format;
	S32 nSamples;  // the number of samples - duration = nSamples/Rate()
	void *samples; // this should probably be a handle on Mac
	S32 dataLen;   // length in bytes of samples - set only if needed
	S32 delay;	 // MP3 compression has a delay before the real sound data

	static const S32 kRateTable[4];
	static const int kRateShiftTable[4];

public:
	void Init();

	S32 Rate() { return kRateTable[(format >> 2) & 0x3]; }
	int RateShift() { return kRateShiftTable[(format >> 2) & 0x3]; }
	bool Stereo() { return (format & sndStereo) != 0; }
	int NChannels() { return (format & sndStereo) ? 2 : 1; }
	bool Is8Bit() { return (format & snd16Bit) == 0; }
	int BitsPerSample() { return (format & snd16Bit) ? 16 : 8; }
	int BytesPerSample() { return (format & snd16Bit) ? 2 : 1; }
	int CompressFormat() { return format & sndCompressMask; }
	bool Compressed() { return (format & sndCompressMask) != 0; }

	// Manage the duration in 44kHz units
	S32 GetDuration44() { return nSamples << RateShift(); }
	void SetDuration44(S32 d) { nSamples = d >> RateShift(); }

	int BytesPerBlock() { return BytesPerSample() * NChannels(); }
	S32 SizeBytes() { return nSamples * BytesPerBlock(); }

	void Set(WaveFormat *);
};

class FSoundComp
{
private:
	BOOL isStereo;
	BOOL is8Bit;
	int nBits; // number of bits in each sample

	S32 nSamples; // samples compressed so far

	S32 valpred[2]; // ADPCM state
	int index[2];

	// The Destination
	S32 len; // default is to just calculate the size

	// State for writing bits
	unsigned int bitBuf;
	int bitPos;

public:
	FSoundComp(FSound *snd, S32 numberBits); // numberBits from 2-5
	~FSoundComp() { assert(bitPos == 0); }

	void Compress(void *src, S32 n, std::vector<U8> *stream);
	void Flush(std::vector<U8> *stream);

private:
	void Compress16(S16 *src, S32 n, std::vector<U8> *stream);

	// Write variable width bit fields
	void WriteBits(std::vector<U8> *stream); // empty the buffer of whole bytes

	void PutBits(S32 v, int n, std::vector<U8> *stream)
	{
		assert(n <= 16);
		if (bitPos + n > 32)
			WriteBits(stream);
		bitBuf = (bitBuf << n) | (v & ~(0xFFFFFFFFL << n));
		bitPos += n;
	}
};

#endif
