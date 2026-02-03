#include "Macromedia.h"
#include "FSound.h"

//
// The Low Level sound object
//

const S32 FSound::kRateTable[4] = {sndRate5K, sndRate11K, sndRate22K, sndRate44K};
const int FSound::kRateShiftTable[4] = {3, 2, 1, 0};

void FSound::Init()
{
	format = 0;
	nSamples = 0;
	samples = 0;
	dataLen = 0;
	delay = 0;
}

void FSound::Set(WaveFormat *wfmt)
{
	wfmt->wFormatTag = 1; //WAVE_FORMAT_PCM;
	wfmt->nSamplesPerSec = Rate();
	wfmt->nChannels = NChannels();
	wfmt->wBitsPerSample = BitsPerSample();
	wfmt->nBlockAlign = (wfmt->wBitsPerSample * wfmt->nChannels) / 8;
	wfmt->nAvgBytesPerSec = wfmt->nBlockAlign * wfmt->nSamplesPerSec;

	//wfmt->cbSize = 0;
}

//
// ADPCM tables
//

static const int indexTable2[2] = {
	-1, 2,
};

// Is this ok?
static const int indexTable3[4] = {
	-1, -1, 2, 4,
};

static const int indexTable4[8] = {
	-1, -1, -1, -1, 2, 4, 6, 8,
};

static const int indexTable5[16] = {
	-1, -1, -1, -1, -1, -1, -1, -1, 1, 2, 4, 6, 8, 10, 13, 16,
};

static const int *indexTables[] = {
	indexTable2,
	indexTable3,
	indexTable4,
	indexTable5};

static const int stepsizeTable[89] = {
	7, 8, 9, 10, 11, 12, 13, 14, 16, 17,
	19, 21, 23, 25, 28, 31, 34, 37, 41, 45,
	50, 55, 60, 66, 73, 80, 88, 97, 107, 118,
	130, 143, 157, 173, 190, 209, 230, 253, 279, 307,
	337, 371, 408, 449, 494, 544, 598, 658, 724, 796,
	876, 963, 1060, 1166, 1282, 1411, 1552, 1707, 1878, 2066,
	2272, 2499, 2749, 3024, 3327, 3660, 4026, 4428, 4871, 5358,
	5894, 6484, 7132, 7845, 8630, 9493, 10442, 11487, 12635, 13899,
	15289, 16818, 18500, 20350, 22385, 24623, 27086, 29794, 32767};

//
// The Compressor
//

FSoundComp::FSoundComp(FSound *snd, S32 nb)
{
	// 	assert(snd->CompressFormat() == sndCompressADPCM);
	isStereo = snd->Stereo();
	is8Bit = snd->Is8Bit();

	nBits = nb;
	nSamples = 0;

	len = 0;

	bitBuf = 0;
	bitPos = 0;
}

void FSoundComp::WriteBits(std::vector<U8> *stream)
{
	if (stream) {
		// Actually write the bits...
		while (bitPos >= 8) {
			//			May need to rewrite this line
			// 			recorder->PutByte((U8)(bitBuf >> (bitPos-8)));
			stream->push_back((U8)(bitBuf >> (bitPos - 8)));

			bitPos -= 8;
			len++;
		}
	} else {
		// Just counting...
		len += bitPos / 8;
		bitPos &= 0x7;
	}
}

void FSoundComp::Flush(std::vector<U8> *stream)
{
	WriteBits(stream);
	if (bitPos > 0) {
		PutBits(0, 8 - bitPos, stream);
		WriteBits(stream);
	}
}

void FSoundComp::Compress16(S16 *src, S32 n, std::vector<U8> *stream)
{
	if (nSamples == 0) {
		// Emit the compression settings
		PutBits(nBits - 2, 2, stream);
	}

	int sn = isStereo ? 2 : 1;
	const int *indexTable = indexTables[nBits - 2];
	while (n-- > 0) {
		nSamples++;
		if ((nSamples & 0xfff) == 1) {
			// We emit a header every 4096 samples so we can seek quickly
			for (int i = 0; i < sn; i++) {
				// Pick an initial index value
				S32 d = abs(src[sn] - src[0]);
				int k = 0;
				while (k < 63 && stepsizeTable[k] < d)
					k++;

				PutBits(valpred[i] = *src++, 16, stream);
				PutBits(index[i] = k, 6, stream);
			}

		} else {
			// Generate a delta value
			for (int i = 0; i < sn; i++) {
				/* Step 1 - compute difference with previous value */
				S32 diff = *src++ - valpred[i]; /* Difference between val and valprev */
				int sign;
				if (diff < 0) {
					sign = 1 << (nBits - 1);
					diff = -diff;
				} else {
					sign = 0;
				}

				/* Step 2 - Divide and clamp */
				/* Note:
				** This code *approximately* computes:
				**    delta = diff*4/step;
				**    vpdiff = (delta+0.5)*step/4;
				** but in shift step bits are dropped. The net result of this is
				** that even if you have fast mul/div hardware you cannot put it to
				** good use since the fixup would be too expensive.
				*/
				int step = stepsizeTable[index[i]]; /* Stepsize */
				S32 delta = 0;						/* Current adpcm output value */
				S32 vpdiff = 0;						/* Current change to valpred */

				int k = 1 << (nBits - 2);
				do {
					if (diff >= step) {
						delta |= k;
						diff -= step;
						vpdiff += step;
					}
					step >>= 1;
					k >>= 1;
				} while (k);
				vpdiff += step; // add the 0.5

				/* Step 3 - Update previous value */
				if (sign)
					valpred[i] -= vpdiff;
				else
					valpred[i] += vpdiff;

				/* Step 4 - Clamp previous value to 16 bits */
				if (valpred[i] != (S16)valpred[i])
					valpred[i] = valpred[i] < 0 ? -32768 : 32767;
				assert(valpred[i] <= 32767 && valpred[i] >= -32768);

				/* Step 5 - Assemble value, update index and step values */
				index[i] += indexTable[delta];
				if (index[i] < 0)
					index[i] = 0;
				else if (index[i] > 88)
					index[i] = 88;

				delta |= sign;

				/* Step 6 - Output value */
				PutBits(delta, nBits, stream);
			}
		}
	}
}

inline void Filter8to16(U8 *src, S16 *dst, S32 n)
// Can work in place
{
	src += n;
	dst += n;
	while (n--)
		*(--dst) = ((S16) * (--src) - 128) << 8;
}

void FSoundComp::Compress(void *src, S32 n, std::vector<U8> *stream)
{
	if (is8Bit) {
		S16 buf[4096];
		U8 *s = (U8 *)src;
		while (n > 0) {
			// Expand to 16 bit and compress
			S32 nb = std::min((S32)4096, n);
			Filter8to16(s, buf, nb);
			Compress16(buf, nb, stream);
			n -= nb;
			s += nb;
		}

	} else {
		Compress16((S16 *)src, n, stream);
	}
}
