// Copyright © 1999 Middlesoft, Inc. All rights reserved.
// First Created By Lee Thomason.
// First Created On 09/08/1999.
// Last Modified On 11/09/1999.

/****************************************************************************************

				File Summary: FDTSounds.cpp

  This source file contains the definition for all low-level sound-related functions,
  grouped by classes:

		Class						Member Function

	FDTDefineSound				FDTDefineSound()
								void WriteToSWFStream(FSWFStream*);

	FDTDefineSoundADPCM			FDTDefineSoundADPCM(U8, U8, U8, U32, U8*, U32, int);

	FDTDefineSoundWAV			FDTDefineSoundWAV(FILE*, int);
								bool loadWavFile(FILE*, SSound*, U8**, int*);
	FDTSoundStreamBlock			FDTSoundStreamBlock(U32, U8*);
								void WriteToSWFStream(FSWFStream*);

	FDTSoundStreamHead			FDTSoundStreamHead(U8, U8, U16);
								void WriteToSWFStream(FSWFStream*);

	FDTSoundStreamHead2			FDTSoundStreamHead2(U8, U8, U8, U8, U16);
								void WriteToSWFStream(FSWFStream*);

	FSndEnv						FSndEnv(U32, U16, U16);
								void WriteToSWFStream(FSWFStream*);
	
	FSoundInfo					FSoundInfo(U8, U8, U8, U8, U8, U32, U32, U16, U8, FSndEnv*);
								~FSoundInfo();
								void WriteToSWFStream(FSWFStream*);

****************************************************************************************/

#include "FDTSounds.h"
#include "FSound.h"
#include "FSWFStream.h"

//////////////////////////////////////////////////////////////////////////////////////
//  --------  FDTDefineSound  --------------------------------------------------------

FDTDefineSound::FDTDefineSound()
{
	soundID = FObjCollection::Increment();
	memset(&soundData, 0, sizeof(soundData));
}

void FDTDefineSound::WriteToSWFStream(FSWFStream *_SWFStream)
{

	FSWFStream tempBuffer;

	tempBuffer.WriteWord((U32)soundID);
	tempBuffer.WriteBits((U32)soundData.format, 4);
	tempBuffer.WriteBits((U32)soundData.rate, 2);
	tempBuffer.WriteBits((U32)soundData.size, 1);
	tempBuffer.WriteBits((U32)soundData.type, 1);
	tempBuffer.WriteDWord(soundData.sampleCount);
	tempBuffer.WriteLargeData(soundData.sound, soundData.soundSize);

	_SWFStream->AppendTag(stagDefineSound, tempBuffer.Size(), &tempBuffer);
}

//////////////////////////////////////////////////////////////////////////////////////
//  --------  FDTDefineSoundADPCM  ---------------------------------------------------

FDTDefineSoundADPCM::FDTDefineSoundADPCM(U8 rate,
										 U8 size,
										 U8 type,
										 U32 sampleCount,
										 const U8 *wavData,
										 U32 wavDataSize,
										 int compression)
{
	// clear the vector to write to
	pcmData.clear();

	soundData.format = 1; //a 1 indicates ADPCM
	soundData.rate = rate;
	soundData.size = size;
	soundData.type = type;
	soundData.sampleCount = sampleCount;

	// Construct a FSound object
	FSound sound;
	sound.format = Format();

	sound.nSamples = (S16)soundData.sampleCount;
	sound.samples = const_cast<U8 *>(wavData); // it won't change it - so we cast (lee)
	sound.dataLen = (S16)wavDataSize;
	sound.delay = 0;

	FSoundComp compress(&sound, compression);

	compress.Compress(const_cast<U8 *>(wavData), sound.nSamples, &pcmData); // it won't change it - so we cast (lee)
	compress.Flush(&pcmData);

	// store the compressed data to the sound structure
	soundData.sound = &pcmData[0];
	soundData.soundSize = pcmData.size();
}

//////////////////////////////////////////////////////////////////////////////////////
//  --------  FDTDefineSoundWAV  -----------------------------------------------------

FDTDefineSoundWAV::FDTDefineSoundWAV(FILE *fp, int comp)
{
	U8 *wavData;	 // memory where the WAV file is stored
	int wavDataSize; // the number of bytes in the WAV file

	// clear the vector to write to
	pcmData.clear();

	// 	fp = fopen( waveFile, "rb" );
	if (!fp) {
		return;
	}

	if (loadWavFile(fp, &soundData, &wavData, &wavDataSize) == false) {
		// there was an error
		return;
	}

	// Construct a FSound object
	FSound sound;
	sound.format = Format();

	sound.nSamples = (S16)soundData.sampleCount;
	sound.samples = wavData;
	sound.dataLen = wavDataSize;
	sound.delay = 0;

	FSoundComp compress(&sound, comp);

	compress.Compress(wavData, sound.nSamples, &pcmData);
	compress.Flush(&pcmData);

	// 	fclose( fp );

	// store the compressed data to the sound structure
	soundData.sound = &pcmData[0];
	soundData.soundSize = pcmData.size();

	// delete the wav data
	delete[] wavData;
}

//------------------------------------------------------------------------------
bool FDTDefineSoundWAV::loadWavFile(FILE *fp, SSound *soundData, U8 **wavData, int *wavDataSize)
{
	TUINT32 nextseek;
	TUINT32 filesize;
	TUINT32 fileTag;
	TUINT32 typeTag;
	TUINT32 chunkSize;
	TUINT32 chunkId;
	TUINT32 formatTag = 0;

	assert(fp);
	memset(soundData, 0, sizeof(SSound));

	soundData->format = 1; // ADPCM compressed

	fileTag = read32(fp);
	if (fileTag != 0x46464952) {
		FLASHOUTPUT("Not a RIFF format file\n");
		return false;
	}

	filesize = read32(fp);
	filesize += ftell(fp);

	typeTag = read32(fp);
	if (typeTag != 0x45564157) {
		FLASHOUTPUT("Not a WAVE file\n");
		return false;
	}

	nextseek = ftell(fp);
	while (nextseek < filesize) {
		fseek(fp, (TINT32)nextseek, 0);
		chunkId = read32(fp);
		chunkSize = read32(fp);
		nextseek = chunkSize + ftell(fp);

#ifndef __LP64__
		FLASHOUTPUT("\n----- Chunk ID %d, Size %d -----\n", chunkId, chunkSize);
#else
		FLASHOUTPUT("\n----- Chunk ID %d, Size %d -----\n", chunkId, chunkSize);
#endif

		switch (chunkId) {
		case 0x20746D66: //Format Chunk
		{
			FLASHOUTPUT("Format Chunk\n");
			FLASHOUTPUT("Common Fields:\n");

			formatTag = read16(fp);
#ifndef __LP64__
			FLASHOUTPUT("  Format Tag %04lXh:  ", (long unsigned int)formatTag);
#else
			FLASHOUTPUT("  Format Tag %04Xh:  ", formatTag);
#endif
			switch (formatTag) {
			default:
			case 0x0000:
				FLASHOUTPUT("WAVE_FORMAT_UNKNOWN");
				break;
			case 0x0001:
				FLASHOUTPUT("WAVE_FORMAT_PCM\n");
				break;
			case 0x0002:
				FLASHOUTPUT("WAVE_FORMAT_ADPCM\n");
				break;
			case 0x0005:
				FLASHOUTPUT("WAVE_FORMAT_IBM_CVSD\n");
				break;
			case 0x0006:
				FLASHOUTPUT("WAVE_FORMAT_ALAW\n");
				break;
			case 0x0007:
				FLASHOUTPUT("WAVE_FORMAT_MULAW\n");
				break;
			case 0x0010:
				FLASHOUTPUT("WAVE_FORMAT_OKI_ADPCM");
				break;
			case 0x0011:
				FLASHOUTPUT("WAVE_FORMAT_DVI_ADPCM or WAVE_FORMAT_IMA_ADPCM\n");
				break;
			case 0x0015:
				FLASHOUTPUT("WAVE_FORMAT_DIGISTD\n");
				break;
			case 0x0016:
				FLASHOUTPUT("WAVE_FORMAT_DIGIFIX\n");
				break;
			case 0x0020:
				FLASHOUTPUT("WAVE_FORMAT_YAMAHA_ADPCM\n");
				break;
			case 0x0021:
				FLASHOUTPUT("WAVE_FORMAT_SONARC\n");
				break;
			case 0x0022:
				FLASHOUTPUT("WAVE_FORMAT_DSPGROUP_TRUESPEECH\n");
				break;
			case 0x0023:
				FLASHOUTPUT("WAVE_FORMAT_ECHOSC1\n");
				break;
			case 0x0024:
				FLASHOUTPUT("WAVE_FORMAT_AUDIOFILE_AF18\n");
				break;
			case 0x0101:
				FLASHOUTPUT("IBM_FORMAT_MULAW\n");
				break;
			case 0x0102:
				FLASHOUTPUT("IBM_FORMAT_ALAW\n");
				break;
			case 0x0103:
				FLASHOUTPUT("IBM_FORMAT_ADPCM\n");
				break;
			case 0x0200:
				FLASHOUTPUT("WAVE_FORMAT_CREATIVE_ADPCM\n");
				break;
			}
			int channels = (int)read16(fp);
			soundData->type = channels - 1;
			FLASHOUTPUT("  %u Channels\n", channels);

			int samplesPerSec = (int)read32(fp);
			FLASHOUTPUT("  %u Samples Per Second\n", samplesPerSec);
			switch (samplesPerSec / 5000) {
			case 1: // 5k
				soundData->rate = Snd5k;
				break;
			case 2: // 11k
				soundData->rate = Snd11k;
				break;
			case 4: // 22k
				soundData->rate = Snd22k;
				break;
			case 8: // 44k
				soundData->rate = Snd44k;
				break;
			default:
				FLASHOUTPUT("Sample rate %d unsupported\n", samplesPerSec);
				return false;
			}

			int bytesPerSec = (int)read32(fp);
			FLASHOUTPUT("  %u Average Bytes Per Second\n", bytesPerSec);

			int blockAlign = read16(fp);
			FLASHOUTPUT("  Block Align %u\n", blockAlign);

			FLASHOUTPUT("Format Specific Fields:\n");
			int bits = read16(fp);
			FLASHOUTPUT("  %u Bits Per Sample\n", bits);

			//store the size of the chunks of data (8v16 bit)
			if (bits == 8)
				soundData->size = 0;
			else if (bits == 16)
				soundData->size = 1;
			else
				FLASHASSERT(0);

			if (formatTag != 0x0001) {
				int extra = read16(fp);
				FLASHOUTPUT("  %d Bytes of extra information\n", extra);
				FLASHOUTPUT("  NOT YET SUPPORTED\n");
				return false;
			}
		} break;

		case 0x61746164: //Data Chunk
		{
			FLASHOUTPUT("Data Chunk\n");
			if (!formatTag) {
				FLASHOUTPUT("Warning Format Chunk not defined before Data Chunk\n");
				return false;
			}
			*wavDataSize = (int)chunkSize;
			soundData->sampleCount = chunkSize / (soundData->size + 1);
			*wavData = new U8[chunkSize];
			int read = fread(*wavData, 1, *wavDataSize, fp);

			if (read != (*wavDataSize)) {
				FLASHOUTPUT("Warning Read %d of %d bytes\n", read, (*wavDataSize));
				return false;
			}
			return true;
		}

		default:
			FLASHOUTPUT("Unknown Chunk\n");
			return false;
		}
	}
	return false;
}

//////////////////////////////////////////////////////////////////////////////////////
//  --------  FDTSoundStreamBlock  ---------------------------------------------------

FDTSoundStreamBlock::FDTSoundStreamBlock(U32 _size, U8 *_data)
{
	size = _size;
	data = _data;
}

void FDTSoundStreamBlock::WriteToSWFStream(FSWFStream *_SWFStream)
{

	FSWFStream body;
	body.WriteLargeData(data, size);

	_SWFStream->AppendTag(stagSoundStreamBlock, body.Size(), &body);
}

//////////////////////////////////////////////////////////////////////////////////////
//  --------  FDTSoundStreamHead  ----------------------------------------------------

FDTSoundStreamHead::FDTSoundStreamHead(U8 _mixFormat, U8 _soundType, U16 _count)
{
	mixFormat = _mixFormat;
	soundType = _soundType;
	count = _count;
}

void FDTSoundStreamHead::WriteToSWFStream(FSWFStream *_SWFStream)
{
	FSWFStream body;

	body.WriteByte((U32)mixFormat);
	body.WriteBits(1, 4);
	body.WriteBits(0, 2);
	body.WriteBits(1, 1);
	body.WriteBits((U32)soundType, 1);
	body.WriteWord((U32)count);
	_SWFStream->AppendTag(stagSoundStreamHead, body.Size(), &body);
}

//////////////////////////////////////////////////////////////////////////////////////
//  --------  FDTSoundStreamHead2  ---------------------------------------------------

FDTSoundStreamHead2::FDTSoundStreamHead2(U8 _mixFormat, U8 _compression, U8 _size, U8 _soundType, U16 _count)
{
	mixFormat = _mixFormat;
	compression = _compression;
	rate = 0;
	size = _size;
	soundType = _soundType;
	count = _count;
}

FDTSoundStreamHead2::FDTSoundStreamHead2(U8 _mixFormat, U8 _compression, U8 _rate, U8 _size, U8 _soundType, U16 _count)
{
	mixFormat = _mixFormat;
	compression = _compression;
	rate = _rate;
	size = _size;
	soundType = _soundType;
	count = _count;
}

void FDTSoundStreamHead2::WriteToSWFStream(FSWFStream *_SWFStream)
{

	FSWFStream body;

	body.WriteByte((U32)mixFormat);
	body.WriteBits((U32)compression, 4);
	body.WriteBits((U32)rate, 2);
	body.WriteBits((U32)size, 1);
	body.WriteBits((U32)soundType, 1);
	body.WriteWord((U32)count);

	_SWFStream->AppendTag(stagSoundStreamHead2, body.Size(), &body);
}

//////////////////////////////////////////////////////////////////////////////////////
//  --------  FSndEnv  ---------------------------------------------------------------
FSndEnv::FSndEnv(U32 mark44, U16 level0, U16 level1)
{
	mark44 = mark44;
	level0 = level0;
	level1 = level1;
}

void FSndEnv::WriteToSWFStream(FSWFStream *_SWFStream)
{
	_SWFStream->WriteDWord(mark44);
	_SWFStream->WriteWord((U32)level0);
	_SWFStream->WriteWord((U32)level1);
}

//////////////////////////////////////////////////////////////////////////////////////
//  --------  FSoundInfo  ------------------------------------------------------------
FSoundInfo::FSoundInfo(U8 _syncFlags, U8 _hasEnvelope, U8 _hasLoops, U8 _hasOutPoint,
					   U8 _hasInPoint, U32 _inPoint, U32 _outPoint, U16 _loopCount,
					   U8 _nPoints, FSndEnv *_soundEnvelope)
{

	syncFlags = _syncFlags;
	hasEnvelope = _hasEnvelope;
	hasLoops = _hasLoops;
	hasOutPoint = _hasOutPoint;
	hasInPoint = _hasInPoint;
	inPoint = _inPoint;
	outPoint = _outPoint;
	loopCount = _loopCount;
	nPoints = _nPoints;
	soundEnvelope = _soundEnvelope;
}

FSoundInfo::~FSoundInfo()
{

	delete soundEnvelope;
}

void FSoundInfo::WriteToSWFStream(FSWFStream *_SWFStream)
{
	_SWFStream->WriteBits((U32)syncFlags, 4);
	_SWFStream->WriteBits((U32)hasEnvelope, 1);
	_SWFStream->WriteBits((U32)hasLoops, 1);
	_SWFStream->WriteBits((U32)hasOutPoint, 1);
	_SWFStream->WriteBits((U32)hasInPoint, 1);
	if (hasInPoint)
		_SWFStream->WriteDWord(inPoint);
	if (hasOutPoint)
		_SWFStream->WriteDWord(outPoint);
	if (hasLoops)
		_SWFStream->WriteWord(loopCount);
	if (hasEnvelope) {
		_SWFStream->WriteByte(nPoints);
		soundEnvelope->WriteToSWFStream(_SWFStream);
	}
}
