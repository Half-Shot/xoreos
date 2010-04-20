/* eos - A reimplementation of BioWare's Aurora engine
 * Copyright (c) 2010 Sven Hesse (DrMcCoy), Matthew Hoops (clone2727)
 *
 * The Infinity, Aurora, Odyssey and Eclipse engines, Copyright (c) BioWare corp.
 * The Electron engine, Copyright (c) Obsidian Entertainment and BioWare corp.
 *
 * This file is part of eos and is distributed under the terms of
 * the GNU General Public Licence. See COPYING for more informations.
 */

// Largely based on the ADPCM implementation found in ScummVM.

#include "common/endianness.h"

#include "sound/decoders/adpcm.h"
#include "sound/audiostream.h"

namespace Sound {

class ADPCMStream : public RewindableAudioStream {
protected:
	Common::SeekableReadStream *_stream;
	const DisposeAfterUse::Flag _disposeAfterUse;
	const int32 _startpos;
	const int32 _endpos;
	const int _channels;
	const uint32 _blockAlign;
	uint32 _blockPos[2];
	const int _rate;

	struct {
		// OKI/IMA
		struct {
			int32 last;
			int32 stepIndex;
		} ima_ch[2];
	} _status;

	virtual void reset();
	int16 stepAdjust(byte);

public:
	ADPCMStream(Common::SeekableReadStream *stream, DisposeAfterUse::Flag disposeAfterUse, uint32 size, int rate, int channels, uint32 blockAlign);
	~ADPCMStream();

	virtual bool endOfData() const { return (_stream->eos() || _stream->pos() >= _endpos); }
	virtual bool isStereo() const	{ return _channels == 2; }
	virtual int getRate() const	{ return _rate; }

	virtual bool rewind();
};


// IMA ADPCM support is based on
//   <http://wiki.multimedia.cx/index.php?title=IMA_ADPCM>
//
// In addition, also MS IMA ADPCM is supported. See
//   <http://wiki.multimedia.cx/index.php?title=Microsoft_IMA_ADPCM>.

ADPCMStream::ADPCMStream(Common::SeekableReadStream *stream, DisposeAfterUse::Flag disposeAfterUse, uint32 size, int rate, int channels, uint32 blockAlign)
	: _stream(stream),
		_disposeAfterUse(disposeAfterUse),
		_startpos(stream->pos()),
		_endpos(_startpos + size),
		_channels(channels),
		_blockAlign(blockAlign),
		_rate(rate) {

	reset();
}

ADPCMStream::~ADPCMStream() {
	if (_disposeAfterUse == DisposeAfterUse::YES)
		delete _stream;
}

void ADPCMStream::reset() {
	memset(&_status, 0, sizeof(_status));
	_blockPos[0] = _blockPos[1] = _blockAlign; // To make sure first header is read
}

bool ADPCMStream::rewind() {
	// TODO: Error checking.
	reset();
	_stream->seek(_startpos);
	return true;
}

class Ima_ADPCMStream : public ADPCMStream {
protected:
	int16 decodeIMA(byte code, int channel = 0); // Default to using the left channel/using one channel

public:
	Ima_ADPCMStream(Common::SeekableReadStream *stream, DisposeAfterUse::Flag disposeAfterUse, uint32 size, int rate, int channels, uint32 blockAlign)
		: ADPCMStream(stream, disposeAfterUse, size, rate, channels, blockAlign) {
		memset(&_status, 0, sizeof(_status));
	}

	virtual int readBuffer(int16 *buffer, const int numSamples);
};

int Ima_ADPCMStream::readBuffer(int16 *buffer, const int numSamples) {
	int samples;
	byte data;

	assert(numSamples % 2 == 0);

	for (samples = 0; samples < numSamples && !_stream->eos() && _stream->pos() < _endpos; samples += 2) {
		data = _stream->readByte();
		buffer[samples] = decodeIMA((data >> 4) & 0x0f);
		buffer[samples + 1] = decodeIMA(data & 0x0f, _channels == 2 ? 1 : 0);
	}
	return samples;
}

class MSIma_ADPCMStream : public Ima_ADPCMStream {
public:
	MSIma_ADPCMStream(Common::SeekableReadStream *stream, DisposeAfterUse::Flag disposeAfterUse, uint32 size, int rate, int channels, uint32 blockAlign)
		: Ima_ADPCMStream(stream, disposeAfterUse, size, rate, channels, blockAlign) {
		if (blockAlign == 0)
			error("ADPCMStream(): blockAlign isn't specified for MS IMA ADPCM");
	}

	virtual int readBuffer(int16 *buffer, const int numSamples) {
		if (_channels == 1)
			return readBufferMSIMA1(buffer, numSamples);
		else
			return readBufferMSIMA2(buffer, numSamples);
	}

	int readBufferMSIMA1(int16 *buffer, const int numSamples);
	int readBufferMSIMA2(int16 *buffer, const int numSamples);
};

int MSIma_ADPCMStream::readBufferMSIMA1(int16 *buffer, const int numSamples) {
	int samples = 0;
	byte data;

	assert(numSamples % 2 == 0);

	while (samples < numSamples && !_stream->eos() && _stream->pos() < _endpos) {
		if (_blockPos[0] == _blockAlign) {
			// read block header
			_status.ima_ch[0].last = _stream->readSint16LE();
			_status.ima_ch[0].stepIndex = _stream->readSint16LE();
			_blockPos[0] = 4;
		}

		for (; samples < numSamples && _blockPos[0] < _blockAlign && !_stream->eos() && _stream->pos() < _endpos; samples += 2) {
			data = _stream->readByte();
			_blockPos[0]++;
			buffer[samples] = decodeIMA(data & 0x0f);
			buffer[samples + 1] = decodeIMA((data >> 4) & 0x0f);
		}
	}
	return samples;
}


// Microsoft as usual tries to implement it differently. This method
// is used for stereo data.
int MSIma_ADPCMStream::readBufferMSIMA2(int16 *buffer, const int numSamples) {
	int samples;
	uint32 data;
	int nibble;
	byte k;

	// TODO: Currently this implementation only supports
	// reading a multiple of 16 samples at once. We might
	// consider changing that so it could read an arbitrary
	// sample pair count.
	assert(numSamples % 16 == 0);

	for (samples = 0; samples < numSamples && !_stream->eos() && _stream->pos() < _endpos;) {
		for (int channel = 0; channel < 2; channel++) {
			data = _stream->readUint32LE();

			for (nibble = 0; nibble < 8; nibble++) {
				k = ((data & 0xf0000000) >> 28);
				buffer[samples + channel + nibble * 2] = decodeIMA(k);
				data <<= 4;
			}
		}
		samples += 16;
	}
	return samples;
}

static const int MSADPCMAdaptCoeff1[] = {
	256, 512, 0, 192, 240, 460, 392
};

static const int MSADPCMAdaptCoeff2[] = {
	0, -256, 0, 64, 0, -208, -232
};

static const int MSADPCMAdaptationTable[] = {
	230, 230, 230, 230, 307, 409, 512, 614,
	768, 614, 512, 409, 307, 230, 230, 230
};


class MS_ADPCMStream : public ADPCMStream {
protected:
	struct ADPCMChannelStatus {
		byte predictor;
		int16 delta;
		int16 coeff1;
		int16 coeff2;
		int16 sample1;
		int16 sample2;
	};

	struct {
		// MS ADPCM
		ADPCMChannelStatus ch[2];
	} _status;

	void reset() {
		ADPCMStream::reset();
		memset(&_status, 0, sizeof(_status));
	}

public:
	MS_ADPCMStream(Common::SeekableReadStream *stream, DisposeAfterUse::Flag disposeAfterUse, uint32 size, int rate, int channels, uint32 blockAlign)
		: ADPCMStream(stream, disposeAfterUse, size, rate, channels, blockAlign) {
		if (blockAlign == 0)
			error("MS_ADPCMStream(): blockAlign isn't specified for MS ADPCM");
		memset(&_status, 0, sizeof(_status));
	}

	virtual int readBuffer(int16 *buffer, const int numSamples);

protected:
	int16 decodeMS(ADPCMChannelStatus *c, byte);
};

int16 MS_ADPCMStream::decodeMS(ADPCMChannelStatus *c, byte code) {
	int32 predictor;

	predictor = (((c->sample1) * (c->coeff1)) + ((c->sample2) * (c->coeff2))) / 256;
	predictor += (signed)((code & 0x08) ? (code - 0x10) : (code)) * c->delta;

	predictor = CLIP<int32>(predictor, -32768, 32767);

	c->sample2 = c->sample1;
	c->sample1 = predictor;
	c->delta = (MSADPCMAdaptationTable[(int)code] * c->delta) >> 8;

	if (c->delta < 16)
		c->delta = 16;

	return (int16)predictor;
}

int MS_ADPCMStream::readBuffer(int16 *buffer, const int numSamples) {
	int samples;
	byte data;
	int i = 0;

	samples = 0;

	while (samples < numSamples && !_stream->eos() && _stream->pos() < _endpos) {
		if (_blockPos[0] == _blockAlign) {
			// read block header
			for (i = 0; i < _channels; i++) {
				_status.ch[i].predictor = CLIP(_stream->readByte(), (byte)0, (byte)6);
				_status.ch[i].coeff1 = MSADPCMAdaptCoeff1[_status.ch[i].predictor];
				_status.ch[i].coeff2 = MSADPCMAdaptCoeff2[_status.ch[i].predictor];
			}

			for (i = 0; i < _channels; i++)
				_status.ch[i].delta = _stream->readSint16LE();

			for (i = 0; i < _channels; i++)
				_status.ch[i].sample1 = _stream->readSint16LE();

			for (i = 0; i < _channels; i++)
				buffer[samples++] = _status.ch[i].sample2 = _stream->readSint16LE();

			for (i = 0; i < _channels; i++)
				buffer[samples++] = _status.ch[i].sample1;

			_blockPos[0] = _channels * 7;
		}

		for (; samples < numSamples && _blockPos[0] < _blockAlign && !_stream->eos() && _stream->pos() < _endpos; samples += 2) {
			data = _stream->readByte();
			_blockPos[0]++;
			buffer[samples] = decodeMS(&_status.ch[0], (data >> 4) & 0x0f);
			buffer[samples + 1] = decodeMS(&_status.ch[_channels - 1], data & 0x0f);
		}
	}

	return samples;
}

// adjust the step for use on the next sample.
int16 ADPCMStream::stepAdjust(byte code) {
	static const int16 adjusts[] = {-1, -1, -1, -1, 2, 4, 6, 8};

	return adjusts[code & 0x07];
}

static const uint16 imaStepTable[89] = {
		7,    8,    9,   10,   11,   12,   13,   14,
	   16,   17,   19,   21,   23,   25,   28,   31,
	   34,   37,   41,   45,   50,   55,   60,   66,
	   73,   80,   88,   97,  107,  118,  130,  143,
	  157,  173,  190,  209,  230,  253,  279,  307,
	  337,  371,  408,  449,  494,  544,  598,  658,
	  724,  796,  876,  963, 1060, 1166, 1282, 1411,
	 1552, 1707, 1878, 2066, 2272, 2499, 2749, 3024,
	 3327, 3660, 4026, 4428, 4871, 5358, 5894, 6484,
	 7132, 7845, 8630, 9493,10442,11487,12635,13899,
	15289,16818,18500,20350,22385,24623,27086,29794,
	32767
};

int16 Ima_ADPCMStream::decodeIMA(byte code, int channel) {
	int32 E = (2 * (code & 0x7) + 1) * imaStepTable[_status.ima_ch[channel].stepIndex] / 8;
	int32 diff = (code & 0x08) ? -E : E;
	int32 samp = CLIP<int32>(_status.ima_ch[channel].last + diff, -32768, 32767);

	_status.ima_ch[channel].last = samp;
	_status.ima_ch[channel].stepIndex += stepAdjust(code);
	_status.ima_ch[channel].stepIndex = CLIP<int32>(_status.ima_ch[channel].stepIndex, 0, ARRAYSIZE(imaStepTable) - 1);

	return samp;
}

RewindableAudioStream *makeADPCMStream(Common::SeekableReadStream *stream, DisposeAfterUse::Flag disposeAfterUse, uint32 size, ADPCMTypes type, int rate, int channels, uint32 blockAlign) {
	switch (type) {
	case kADPCMMSIma:
		return new MSIma_ADPCMStream(stream, disposeAfterUse, size, rate, channels, blockAlign);
	case kADPCMMS:
		return new MS_ADPCMStream(stream, disposeAfterUse, size, rate, channels, blockAlign);
	default:
		error("Unsupported ADPCM encoding");
		break;
	}
}

} // End of namespace Sound