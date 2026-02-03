// Copyright © 1999 Middlesoft, Inc. All rights reserved.
// First Created By Lee Thomason.
// First Created On 09/08/1999.
// Last Modified On 11/09/1999.

/****************************************************************************************

				File Summary: FDTSounds.h

	This header-file contains the declarations of all low-level sound-related classes. 
	Their parent classes are in the parentheses:

		class FDTDefineSound; (public FDT)
		class FDTDefineSoundADPCM; (public FDTDefineSound)
		class FDTDefineSoundWAV; (public FDTDefineSound)
		class FDTSoundStreamBlock; (public FDT)
		class FDTSoundStreamHead; (public FDT)
		class FDTSoundStreamHead2; (public FDT)
		class FSndEnv;
		class FSoundInfo;

****************************************************************************************/

#ifndef _F_DEFINE_SOUNDS_H_
#define _F_DEFINE_SOUNDS_H_

#include "Macromedia.h"
#include "FDT.h"

#include <vector>

// A flash object that defines a sound

class FDTDefineSound : public FDT
{
public:
	// Compression Type
	enum {
		NO_COMPRESSION,
		PCM
	};

	// Compression Level
	enum {
		Comp2 = 2,
		Comp3,
		Comp4,
		Comp5
	};

	// Sound Rate
	enum {
		Snd5k,
		Snd11k,
		Snd22k,
		Snd44k
	};

	// Sound Size
	enum {
		Snd8Bit,
		Snd16Bit
	};

	// Sound Type
	enum {
		Snd16Mono,
		Snd16Stereo
	};

	FDTDefineSound();
	virtual ~FDTDefineSound() {}
	virtual void WriteToSWFStream(FSWFStream *_SWFStream);

	U16 ID() { return soundID; }
	int NumSamples() { return (U8)soundData.sampleCount; }
	int Format() { return 4 * (soundData.rate) + (soundData.size) * 2 + (soundData.type); }
	int SoundType() { return soundData.type; }

protected:
	U16 soundID;
	SSound soundData;
	std::vector<U8> pcmData;
};

// A flash object that defines a sound

class FDTDefineSoundADPCM : public FDTDefineSound
{
public:
	FDTDefineSoundADPCM(U8 rate, U8 size, U8 type, U32 sampleCount,
						const U8 *data, U32 dataSize, int compression);
};

// A flash object that defines a sound
class FDTDefineSoundWAV : public FDTDefineSound
{

public:
	FDTDefineSoundWAV(FILE *wavFile, int compression);

private:
	bool loadWavFile(FILE *fp,
					 SSound *soundData,
					 U8 **wavData,
					 int *wavDataSize);
	U32 read32(FILE *fp)
	{
		U32 val;
		fread(&val, 1, 4, fp);
		return val;
	}
	U16 read16(FILE *fp)
	{
		U16 val;
		fread(&val, 1, 2, fp);
		return val;
	}
};

// A Flash object that defines sound data that is interleaved with frame data
//enables sound streaming

class FDTSoundStreamBlock : public FDT
{

public:
	FDTSoundStreamBlock(U32 _size, U8 *_data);
	virtual void WriteToSWFStream(FSWFStream *_SWFStream);

private:
	U32 size;
	U8 *data;
};

// A flash object that defines the start of sound data that will be interleaved within flash frames
// This is how sound streaming of networks is possible.
//	Doesn't support compressed sound (flash 2.0)

class FDTSoundStreamHead : public FDT
{

public:
	FDTSoundStreamHead(U8 _mixFormat, // mixer format (?) set to 6
					   U8 _soundType, // 0 mono, 1 stereo
					   U16 _count	 // number of sound samples
					   );
	virtual void WriteToSWFStream(FSWFStream *_SWFStream);

private:
	U8 mixFormat;
	U8 soundType;
	U16 count;
};

class FDTSoundStreamHead2 : public FDT
{

public:
	FDTSoundStreamHead2(U8 _mixFormat, U8 _compression, U8 _size, U8 _soundType, U16 _count);
	FDTSoundStreamHead2(U8 _mixFormat, U8 _compression, U8 _rate, U8 _size, U8 _soundType, U16 _count);
	virtual void WriteToSWFStream(FSWFStream *_SWFStream);

private:
	U8 mixFormat;
	U8 compression;
	U8 rate;
	U8 size;
	U8 soundType;
	U16 count;
};

class FSndEnv
{

public:
	FSndEnv(U32 mark44, U16 level0, U16 level1);
	void WriteToSWFStream(FSWFStream *_SWFStream);

private:
	U32 mark44;
	U16 level0;
	U16 level1;
};

// specifies how a sound character is player

class FSoundInfo
{

public:
	FSoundInfo(U8 _syncFlags, U8 _hasEnvelope, U8 _hasLoops, U8 _hasOutPoint,
			   U8 _hasInPoint, U32 _inPoint, U32 _outPoint, U16 _loopCount,
			   U8 _nPoints, FSndEnv *_soundEnvelope);
	~FSoundInfo();
	void WriteToSWFStream(FSWFStream *_SWFStream);

	enum {
		SyncNoMultiple = 0x01,
		SyncStor = 0x02
	};

private:
	U8 syncFlags;
	U8 hasEnvelope;
	U8 hasLoops;
	U8 hasOutPoint;
	U8 hasInPoint;
	U32 inPoint;
	U32 outPoint;
	U16 loopCount;
	U8 nPoints;
	FSndEnv *soundEnvelope;
};

#endif
