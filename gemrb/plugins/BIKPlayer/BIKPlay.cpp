/* GemRB - Infinity Engine Emulator
 * Copyright (C) 2009 The GemRB Project
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License
 * as published by the Free Software Foundation; either version 2
 * of the License, or (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA 02110-1301, USA.
 *
 * $Id: BIKPlay.cpp 6168 2009-05-28 22:28:33Z mattinm $
 *
 */

/*
 * code derived from Bink Audio decoder
 * Copyright (c) 2007-2009 Peter Ross (pross@xvid.org)
 * Copyright (c) 2009 Daniel Verkamp (daniel@drv.nu)
 *
 * code derived from Bink video decoder
 * Copyright (c) 2009 Konstantin Shishkov
*/

#include <assert.h>
#include <fcntl.h>
#include <sys/stat.h>
#include <cstdio>
#include "../Core/Video.h"
#include "../Core/Audio.h"
#include "../Core/Variables.h"
#include "BIKPlay.h"
#include "../../includes/ie_types.h"
#include "rational.h"
#include "binkdata.h"

static Video *video = NULL;
static unsigned char g_palette[768];
static int g_truecolor;
static ieDword *cbAtFrame = NULL;
static ieDword *strRef = NULL;

static const int ff_wma_critical_freqs[25] = {
	100,   200,  300, 400,   510,  630,  770,    920,
	1080, 1270, 1480, 1720, 2000, 2320, 2700,   3150,
	3700, 4400, 5300, 6400, 7700, 9500, 12000, 15500,
	24500,
};

BIKPlay::BIKPlay(void)
{
	str = NULL;
	autoFree = false;
	video = core->GetVideoDriver();
	inbuff = NULL;
	maxRow = 0;
	rowCount = 0;
	frameCount = 0;
	//force initialisation of static tables
	memset(bink_trees, 0, sizeof(bink_trees));
	memset(table, 0, sizeof(table));
}

BIKPlay::~BIKPlay(void)
{
	if (str && autoFree) {
		delete( str );
	}
	av_freep((void **) &inbuff);
}

void BIKPlay::av_set_pts_info(AVRational &time_base, unsigned int pts_num, unsigned int pts_den)
{
	//pts_wrap_bits, if needed, is always 64
	if(av_reduce(time_base.num, time_base.den, pts_num, pts_den, INT_MAX)) {
		//bla bla, something didn't work
	}

	if(!time_base.num || !time_base.den)
	time_base.num = time_base.den = 0;
}

int BIKPlay::ReadHeader(DataStream *str)
{
	str->Seek(0,GEM_STREAM_START);
	str->Read( header.signature, BIK_SIGNATURE_LEN );
	str->ReadDword(&header.filesize);
	header.filesize += 8;
	str->ReadDword(&header.framecount);

	if (header.framecount > 1000000) {
		return -1;
	}

	str->ReadDword(&header.maxframesize);
	if (header.maxframesize > header.filesize) {
		return -1;
	}

	str->Seek(4,GEM_CURRENT_POS);

	str->ReadDword(&header.width);
	str->ReadDword(&header.height);

	ieDword fps_num, fps_den;

	str->ReadDword(&fps_num);
	str->ReadDword(&fps_den);

	if (fps_num == 0 || fps_den == 0) {
		//av_log(s, AV_LOG_ERROR, "invalid header: invalid fps (%d/%d)\n", fps_num, fps_den);
		return -1;
	}
	//also sets pts_wrap_bits to 64
	av_set_pts_info(v_timebase, fps_den, fps_num);

	str->Seek(4,GEM_CURRENT_POS);
	str->ReadDword(&header.tracks);

	//we handle only single tracks, is this a problem with multi language iwd2?
	if (header.tracks > 1) {
		return -1;
	}

	if (header.tracks) {
		str->Seek(4 * header.tracks,GEM_CURRENT_POS);
		//make sure we use one track, if more needed, rewrite this part
		assert(header.tracks==1);

		str->ReadWord(&header.samplerate);
		//also sets pts_wrap_bits to 64
		//av_set_pts_info(s_timebase, 1, header.samplerate);  //unused, we simply use header.samplerate
		str->ReadWord(&header.audioflag);

		str->Seek(4 * header.tracks,GEM_CURRENT_POS);
	}

	/* build frame index table */
	ieDword pos, next_pos;
	int keyframe;

	str->ReadDword(&pos);
	keyframe = pos & 1;
	pos &= ~1;

	frames.reserve(header.framecount);
	for (unsigned int i = 0; i < header.framecount; i++) {
	if (i == header.framecount - 1) {
		next_pos = header.filesize;
	} else {
		str->ReadDword(&next_pos);
	}
	if (next_pos <= pos) {
		// av_log(s, AV_LOG_ERROR, "invalid frame index table\n");
		return -1;
	}
	//offset, size, keyframe
	binkframe frame;

	//the order of instructions is important here!
	frame.pos=pos;
	frame.keyframe=keyframe;
	pos = next_pos&~1;
	keyframe = next_pos&1;
	frame.size=pos-frame.pos;
	//sanity hack, we might as well just go belly up and refuse playing
	if (frame.size>header.maxframesize) {
		frame.size = header.maxframesize;
	}

	frames.push_back(frame);

	}
	inbuff = (ieByte *) av_malloc(header.maxframesize);
	if (!inbuff) {
		return -2;
	}

	str->Seek(4, GEM_CURRENT_POS);
	return 0;
}

bool BIKPlay::Open(DataStream* stream, bool autoFree)
{
	validVideo = false;
	if (stream == NULL) {
		return false;
	}
	if (str && this->autoFree) {
		delete( str );
	}
	str = stream;
	this->autoFree = autoFree;

	str->Read( &header.signature, BIK_SIGNATURE_LEN );
	if (memcmp( header.signature, BIK_SIGNATURE_DATA, 4 ) == 0) {
		validVideo = ReadHeader(stream)==0;
		return validVideo;
	}
	return false;
}

void BIKPlay::CallBackAtFrames(ieDword cnt, ieDword *arg, ieDword *arg2 )
{
	maxRow = cnt;
	frameCount = 0;
	rowCount = 0;
	cbAtFrame = arg;
	strRef = arg2;
}

int BIKPlay::Play()
{
	if (!validVideo) {
		return 0;
	}
	//Start Movie Playback
	frameCount = 0;
	int ret = doPlay( );

	EndAudio();
	EndVideo();
	av_freep((void **) &inbuff);
	return ret;
}

//this code could be in the movieplayer parent class
void get_current_time(long &sec, long &usec) {
#ifdef _WIN32
	DWORD time;
	time = GetTickCount();

	sec = time / 1000;
	usec = (time % 1000) * 1000;
#else
	struct timeval tv;
	gettimeofday(&tv, NULL);

	sec = tv.tv_sec;
	usec = tv.tv_usec;
#endif
}

void BIKPlay::timer_start()
{
	get_current_time(timer_last_sec, timer_last_usec);
}

void BIKPlay::timer_wait()
{
	long sec, usec;
	get_current_time(sec, usec);

	while (sec > timer_last_sec) {
		usec += 1000000;
		timer_last_sec++;
	}

	//quick hack, we should rather use the rational time base as ffmpeg
	frame_wait = v_timebase.num*1000000/v_timebase.den;
	while (usec - timer_last_usec > (long)frame_wait) {
		usec -= frame_wait;
		video_frameskip++;
	}

	long to_sleep = frame_wait - (usec - timer_last_usec);
#ifdef _WIN32
	Sleep(to_sleep / 1000);
#else
	usleep(to_sleep);
#endif

	timer_start();
}

bool BIKPlay::next_frame()
{
	if (timer_last_sec) {
		timer_wait();
	}
	if(frameCount>=header.framecount) {
		return false;
	}
	binkframe frame = frames[frameCount++];
	//frame.size = fileRead( frame.pos, inbuff, frame.size);
	//ieDword audframesize = *(ieDword *) inbuff;
	str->Seek(frame.pos, GEM_STREAM_START);
	ieDword audframesize;
	str->ReadDword(&audframesize);
	frame.size = str->Read( inbuff, frame.size - 4 );
	if (DecodeAudioFrame(inbuff, audframesize)) {
		//buggy frame, we stop immediately
		return false;
	}
	if (DecodeVideoFrame(inbuff+audframesize, frame.size-audframesize)) {
		//buggy frame, we stop immediately
		return false;
	}

	if (!timer_last_sec) {
		timer_start();
	}
	return true;
}

int BIKPlay::doPlay()
{
	int done = 0;

	//bink is always truecolor
	g_truecolor = 1;
	//palette is not really needed
	memset( g_palette, 0, 768 );

	frame_wait = 0;
	timer_last_sec = 0;
	video_frameskip = 0;

	if (sound_init( core->GetAudioDrv()->CanPlay())) {
		//sound couldn't be initialized
		return 1;
	}

	video->InitMovieScreen(outputwidth,outputheight);

	if (video_init(outputwidth,outputheight)) {
		return 2;
	}

	while (!done && next_frame()) {
		done = video->PollMovieEvents();
	}

	return 0;
}

unsigned int BIKPlay::fileRead(unsigned int pos, void* buf, unsigned int count)
{
	str->Seek(pos, GEM_STREAM_START);
	return str->Read( buf, count );
}

void BIKPlay::showFrame(unsigned char* buf, unsigned int bufw,
	unsigned int bufh, unsigned int sx, unsigned int sy, unsigned int w,
	unsigned int h, unsigned int dstx, unsigned int dsty)
{
	ieDword titleref = 0;

	if (cbAtFrame && strRef) {
		frameCount ++;
		if ((rowCount<maxRow) && (frameCount >= cbAtFrame[rowCount]) ) {
			rowCount++;
		}
		//draw subtitle here
		if (rowCount) {
			titleref = strRef[rowCount-1];
		}
	}
	video->showFrame(buf,bufw,bufh,sx,sy,w,h,dstx,dsty, g_truecolor, g_palette, titleref);
}

int BIKPlay::setAudioStream()
{
	ieDword volume;
	core->GetDictionary()->Lookup( "Volume Movie", volume) ;
	int source = core->GetAudioDrv()->SetupNewStream(0, 0, 0, volume, false, false);
	return source;
}

void BIKPlay::freeAudioStream(int stream)
{
	if (stream > -1)
		core->GetAudioDrv()->ReleaseStream(stream, true);
}

void BIKPlay::queueBuffer(int stream, unsigned short bits, int channels, short* memory, int size, int samplerate)
{
	if (stream > -1)
		core->GetAudioDrv()->QueueBuffer(stream, bits, channels, memory, size, samplerate);
}


/**
 * @file libavcodec/binkaudio.c
 *
 * Technical details here:
 *  http://wiki.multimedia.cx/index.php?title=Bink_Audio
 */

int BIKPlay::sound_init(bool need_init)
{
	int sample_rate = header.samplerate;
	int sample_rate_half;
	unsigned int i;
	int frame_len_bits;
	int ret;

	if(need_init) {
		s_stream = setAudioStream();
	} else {
		s_stream = -1;
		return 0;
	}

	if(s_stream<0) {
		return 0;
	}

	if(header.audioflag&BINK_AUD_STEREO) {
		header.channels=2;
	}

	/* determine frame length */
	if (sample_rate < 22050) {
		frame_len_bits = 9;
	} else if (sample_rate < 44100) {
		frame_len_bits = 10;
	} else {
		frame_len_bits = 11;
	}
	//audio frame length
	s_frame_len = 1 << frame_len_bits;

	if (header.channels > MAX_CHANNELS) {
		//av_log(s->avctx, AV_LOG_ERROR, "too many channels: %d\n", s->channels);
		return -1;
	}

	if (header.audioflag&BINK_AUD_USEDCT) {
		s_channels = header.channels;
	} else {
		// audio is already interleaved for the RDFT format variant
		sample_rate *= header.channels;
		s_frame_len *= header.channels;
		s_channels = 1;
	if (header.channels == 2)
		frame_len_bits++;
	}

	s_overlap_len   = s_frame_len / 16;
	s_block_size    = (s_frame_len - s_overlap_len) * s_channels;
	sample_rate_half = (sample_rate + 1) / 2;
	s_root          = (float) (2.0 / sqrt(s_frame_len));

	/* calculate number of bands */
	for (s_num_bands = 1; s_num_bands < 25; s_num_bands++) {
		if (sample_rate_half <= ff_wma_critical_freqs[s_num_bands - 1]) {
			break;
		}
	}

	s_bands = (unsigned int *) av_malloc((s_num_bands + 1) * sizeof(*s_bands));
	if (!s_bands) {
		return -2;
	}

	/* populate bands data */
	s_bands[0] = 1;
	for (i = 1; i < s_num_bands; i++)
	s_bands[i] = ff_wma_critical_freqs[i - 1] * (s_frame_len / 2) / sample_rate_half;
	s_bands[s_num_bands] = s_frame_len / 2;

	s_first = 1;

	for (i = 0; i < s_channels; i++)
	s_coeffs_ptr[i] = s_coeffs + i * s_frame_len;

	if (header.audioflag&BINK_AUD_USEDCT)
		ret = ff_dct_init(&s_trans.dct, frame_len_bits, 0);
	else
		ret = ff_rdft_init(&s_trans.rdft, frame_len_bits, IRIDFT);

	return ret;
}

void BIKPlay::ff_init_scantable(uint8_t *permutation, ScanTable *st, const uint8_t *src_scantable){
	int i,j;
	int end;

	st->scantable= src_scantable;

	for(i=0; i<64; i++){
		j = src_scantable[i];
		st->permutated[i] = permutation[j];
	}

	end=-1;
	for(i=0; i<64; i++){
		j = st->permutated[i];
		if(j>end) end=j;
		st->raster_end[i]= end;
	}
}

int BIKPlay::video_init(int w, int h)
{
	int bw, bh, blocks;
	int i;

	if (!bink_trees[15].table) {
		for (i = 0; i < 16; i++) {
			const int maxbits = bink_tree_lens[i][15];
			bink_trees[i].table = table + i*128;
			bink_trees[i].table_allocated = 1 << maxbits;
			bink_trees[i].init_vlc(maxbits, 16, bink_tree_lens[i], 1, 1,
				bink_tree_bits[i], 1, 1, INIT_VLC_LE);
		}
	}

	memset(&c_pic,0, sizeof(AVFrame));
	memset(&c_last,0, sizeof(AVFrame));

	if (w<(signed) header.width || h<(signed) header.height) {
		//movie dimensions are higher than available screen
		return 1;
	}

	//pixel format is PIX_FMT_YUV420P
	//idct permutation is used in various optimisations,
	//we go with the simplest (no permutation)
	for(i=0;i<64;i++) {
		c_idct_permutation[i]=i;
	}

	ff_init_scantable(c_idct_permutation, &c_scantable, bink_scan);

	bw = (header.width  + 7) >> 3;
	bh = (header.height + 7) >> 3;
	blocks = bw * bh;

	for (i = 0; i < BINK_NB_SRC; i++) {
		c_bundle[i].data = (uint8_t *) av_malloc(blocks * 64);
		//not enough memory
		if(!c_bundle[i].data) {
			return 2;
		}
		c_bundle[i].data_end = (uint8_t *) c_bundle[i].data + blocks * 64;
	}

	return 0;
}

int BIKPlay::EndAudio()
{
	freeAudioStream(s_stream);
	av_freep((void **) &s_bands);
	if (header.audioflag&BINK_AUD_USEDCT)
		ff_dct_end(&s_trans.dct);
	else
		ff_rdft_end(&s_trans.rdft);
	return 0;
}

static inline void release_buffer(AVFrame *p)
{
	int i;

	for(i=0;i<4;i++) {
		av_freep((void **) &p->data[i]);
	}
}

static inline void ff_fill_linesize(AVFrame *picture, int width)
{
	memset(picture->linesize, 0, sizeof(picture->linesize));
	int w2 = (width + (1 << 1) - 1) >> 1;
	picture->linesize[0] = width;
	picture->linesize[1] = w2;
	picture->linesize[2] = w2;
}

static inline void get_buffer(AVFrame *p, int width, int height)
{
	ff_fill_linesize(p, width);
	for(int plane=0;plane<3;plane++) {
		p->data[plane] = (uint8_t *) av_malloc(p->linesize[plane]*height);
	}
}

int BIKPlay::EndVideo()
{
	int i;

	release_buffer(&c_pic);
	release_buffer(&c_last);
	for (i = 0; i < BINK_NB_SRC; i++) {
		av_freep((void **) &c_bundle[i].data);
	}
	return 0;
}
static const uint8_t rle_length_tab[16] = {
	2, 3, 4, 5, 6, 8, 9, 10, 11, 12, 13, 14, 15, 16, 32, 64
};

const uint8_t ff_log2_tab[256]={
		0,0,1,1,2,2,2,2,3,3,3,3,3,3,3,3,4,4,4,4,4,4,4,4,4,4,4,4,4,4,4,4,
		5,5,5,5,5,5,5,5,5,5,5,5,5,5,5,5,5,5,5,5,5,5,5,5,5,5,5,5,5,5,5,5,
		6,6,6,6,6,6,6,6,6,6,6,6,6,6,6,6,6,6,6,6,6,6,6,6,6,6,6,6,6,6,6,6,
		6,6,6,6,6,6,6,6,6,6,6,6,6,6,6,6,6,6,6,6,6,6,6,6,6,6,6,6,6,6,6,6,
		7,7,7,7,7,7,7,7,7,7,7,7,7,7,7,7,7,7,7,7,7,7,7,7,7,7,7,7,7,7,7,7,
		7,7,7,7,7,7,7,7,7,7,7,7,7,7,7,7,7,7,7,7,7,7,7,7,7,7,7,7,7,7,7,7,
		7,7,7,7,7,7,7,7,7,7,7,7,7,7,7,7,7,7,7,7,7,7,7,7,7,7,7,7,7,7,7,7,
		7,7,7,7,7,7,7,7,7,7,7,7,7,7,7,7,7,7,7,7,7,7,7,7,7,7,7,7,7,7,7,7
};

static inline int float_to_int16_one(const float *src){
	float f = *src;
	// clamp the values to the range of an int16.
	if (f > 32767)
		f = 32767;
	else if (f < -32768)
		f = -32768;
	return (int32_t) (f);
}

static void ff_float_to_int16_interleave_c(int16_t *dst, const float **src, long len, int channels){
	int i;
	if(channels==2) {
		for(i=0; i<len; i++) {
			dst[2*i]   = float_to_int16_one(src[0]+i);
			dst[2*i+1] = float_to_int16_one(src[1]+i);
		}
		return;
	}
	//one channel
	for(i=0; i<len; i++) {
		dst[i] = float_to_int16_one(src[0]+i);
	}
}

/**
 * Decode Bink Audio block
 * @param[out] out Output buffer (must contain s->block_size elements)
 */
void BIKPlay::DecodeBlock(short *out)
{
	unsigned int ch, i, j, k;
	float q, quant[25];
	int width, coeff;

	if (header.audioflag&BINK_AUD_USEDCT) {
		s_gb.skip_bits(2);
	}

	for (ch = 0; ch < s_channels; ch++) {
		FFTSample *coeffs = s_coeffs_ptr[ch];
		q = 0.0;
		coeffs[0] = s_gb.get_float();
		coeffs[1] = s_gb.get_float();

		for (i = 0; i < s_num_bands; i++) {
			int value = s_gb.get_bits(8);
			quant[i] = (float) pow(10.0, FFMIN(value, 95) * 0.066399999);
		}

		// find band (k)
		for (k = 0; s_bands[k] * 2 < 2; k++) {
			q = quant[k];
		}

		// parse coefficients
		i = 2;
		while (i < s_frame_len) {
			if (s_gb.get_bits(1)) {
				j = i + rle_length_tab[s_gb.get_bits(4)] * 8;
			} else {
				j = i + 8;
			}

			if (j > s_frame_len)
				j = s_frame_len;

			width = s_gb.get_bits(4);
			if (width == 0) {
				memset(coeffs + i, 0, (j - i) * sizeof(*coeffs));
				i = j;
				while (s_bands[k] * 2 < i)
					q = quant[k++];
			} else {
				while (i < j) {
					if (s_bands[k] * 2 == i)
						q = quant[k++];
					coeff = s_gb.get_bits(width);
					if (coeff) {
						if (s_gb.get_bits(1))
							coeffs[i] = -q * coeff;
						else
							coeffs[i] =  q * coeff;
					} else {
						coeffs[i] = 0.0;
					}
					i++;
				}
			}
		}

		if (header.audioflag&BINK_AUD_USEDCT)
			ff_dct_calc (&s_trans.dct,  coeffs);
		else
			ff_rdft_calc(&s_trans.rdft, coeffs);

		for (i = 0; i < s_frame_len; i++)
			coeffs[i] *= s_root;
	}

	ff_float_to_int16_interleave_c(out, (const float **)s_coeffs_ptr, s_frame_len, s_channels);

	if (!s_first) {
		unsigned int count = s_overlap_len * s_channels;
		int shift = av_log2(count);
		for (i = 0; i < count; i++) {
			out[i] = (s_previous[i] * (count - i) + out[i] * i) >> shift;
		}
	}

	memcpy(s_previous, out + s_block_size, s_overlap_len * s_channels * sizeof(*out));

	s_first = 0;
}

//audio samples
int BIKPlay::DecodeAudioFrame(void *data, int data_size)
{
	int bits = data_size*8;
	s_gb.init_get_bits((uint8_t *) data, bits);

	unsigned int reported_size = s_gb.get_bits_long(32);
	ieWordSigned *samples = (ieWordSigned *) calloc(reported_size+s_block_size,1);
	if (!samples) {
		return -1;
	}

	ieWordSigned *outbuf = samples;
	ieWordSigned *samples_end  = samples+reported_size/sizeof(ieWordSigned);

	//s_block_size is in sample units
	while (s_gb.get_bits_count() < bits && outbuf + s_block_size <= samples_end) {
		DecodeBlock(outbuf);
		outbuf += s_block_size;
		s_gb.get_bits_align32();
	}

	unsigned int ret = (unsigned int) ((uint8_t*)outbuf - (uint8_t*)samples);

	//sample format is signed 16 bit integers
	queueBuffer(s_stream, 16, s_channels, samples, ret, header.samplerate);

	free(samples);
	return reported_size!=ret;
}

/**
 * Reads 8x8 block of DCT coefficients.
 *
 * @param gb    context for reading bits
 * @param block place for storing coefficients
 * @param scan  scan order table
 * @return 0 for success, negative value in other cases
 */
int BIKPlay::read_dct_coeffs(DCTELEM block[64], const uint8_t *scan)
{
	int mode_list[128];
	int i, t, mask, bits, ccoef, mode;
	int list_start = 64, list_end = 64, list_pos;

	mode_list[list_end++] = ( 4 << 2) | 0;
	mode_list[list_end++] = (24 << 2) | 0;
	mode_list[list_end++] = (44 << 2) | 0;
	mode_list[list_end++] = ( 1 << 2) | 3;
	mode_list[list_end++] = ( 2 << 2) | 3;
	mode_list[list_end++] = ( 3 << 2) | 3;

	bits = v_gb.get_bits(4) - 1;
	for (mask = 1 << bits; bits >= 0; mask >>= 1, bits--) {
		list_pos = list_start;
		while (list_pos < list_end) {
			if (!mode_list[list_pos] || !v_gb.get_bits(1)) {
				list_pos++;
				continue;
			}
			ccoef = mode_list[list_pos] >> 2;
			mode  = mode_list[list_pos] & 3;
			switch (mode) {
			case 0:
			case 2:
				if (!mode) {
					mode_list[list_pos] = ((ccoef + 4) << 2) | 1;
				} else {
					mode_list[list_pos++] = 0;
				}
				for (i = 0; i < 4; i++, ccoef++) {
					if (v_gb.get_bits(1)) {
						mode_list[--list_start] = (ccoef << 2) | 3;
					} else {
						int t;
						if (!bits) {
							t = v_gb.get_bits(1) ? -1 : 1;
						} else {
							t = v_gb.get_bits(bits) | mask;
							if (v_gb.get_bits(1)) {
								t = -t;
							}
						}
						block[scan[ccoef]] = t;
					}
				}
				break;
			case 1:
				mode_list[list_pos] = (ccoef << 2) | 2;
				for (i = 0; i < 3; i++) {
					ccoef += 4;
					mode_list[list_end++] = (ccoef << 2) | 2;
				}
				break;
			case 3:
				if (!bits) {
					t = v_gb.get_bits(1) ? -1 : 1;
				} else {
					t = v_gb.get_bits(bits) | mask;
					if (v_gb.get_bits(1))
						t = -t;
				}
				block[scan[ccoef]] = t;
				mode_list[list_pos++] = 0;
				break;
			}
		}
	}

	return 0;
}

/**
 * Reads 8x8 block with residue after motion compensation.
 *
 * @param gb          context for reading bits
 * @param block       place to store read data
 * @param masks_count number of masks to decode
 * @return 0 on success, negative value in other cases
 */
int BIKPlay::read_residue(DCTELEM block[64], int masks_count)
{
	int mode_list[128];
	int i, mask, ccoef, mode;
	int list_start = 64, list_end = 64, list_pos;
	int nz_coeff[64];
	int nz_coeff_count = 0;

	mode_list[list_end++] = ( 4 << 2) | 0;
	mode_list[list_end++] = (24 << 2) | 0;
	mode_list[list_end++] = (44 << 2) | 0;
	mode_list[list_end++] = ( 0 << 2) | 2;

	for (mask = 1 << v_gb.get_bits(3); mask; mask >>= 1) {
		for (i = 0; i < nz_coeff_count; i++) {
			if (!v_gb.get_bits(1))
				continue;
			if (block[nz_coeff[i]] < 0)
				block[nz_coeff[i]] -= mask;
			else
				block[nz_coeff[i]] += mask;
			masks_count--;
			if (masks_count < 0)
				return 0;
		}
		list_pos = list_start;
		while (list_pos < list_end) {
			if (!mode_list[list_pos] || !v_gb.get_bits(1)) {
				list_pos++;
				continue;
			}
			ccoef = mode_list[list_pos] >> 2;
			mode  = mode_list[list_pos] & 3;
			switch (mode) {
			case 0:
			case 2:
				if (!mode) {
					mode_list[list_pos] = ((ccoef + 4) << 2) | 1;
				} else {
					mode_list[list_pos++] = 0;
				}
				for (i = 0; i < 4; i++, ccoef++) {
					if (v_gb.get_bits(1)) {
						mode_list[--list_start] = (ccoef << 2) | 3;
					} else {
						nz_coeff[nz_coeff_count++] = bink_scan[ccoef];
						block[bink_scan[ccoef]] = v_gb.get_bits(1) ? -mask : mask;
						masks_count--;
						if (masks_count < 0) {
							return 0;
						}
					}
				}
				break;
			case 1:
				mode_list[list_pos] = (ccoef << 2) | 2;
				for (i = 0; i < 3; i++) {
					ccoef += 4;
					mode_list[list_end++] = (ccoef << 2) | 2;
				}
				break;
			case 3:
				nz_coeff[nz_coeff_count++] = bink_scan[ccoef];
				block[bink_scan[ccoef]] = v_gb.get_bits(1) ? -mask : mask;
				mode_list[list_pos++] = 0;
				masks_count--;
				if (masks_count < 0)
					return 0;
				break;
			}
		}
	}

	return 0;
}

/**
 * Prepares bundle for decoding data.
 *
 * @param gb          context for reading bits
 * @param c           decoder context
 * @param bundle_num  number of the bundle to initialize
 */
void BIKPlay::read_bundle(int bundle_num)
{
	int i;

	if (bundle_num == BINK_SRC_COLORS) {
		for (i = 0; i < 16; i++)
			v_gb.read_tree(&c_col_high[i]);
		c_col_lastval = 0;
	}
	if (bundle_num != BINK_SRC_INTRA_DC && bundle_num != BINK_SRC_INTER_DC)
		v_gb.read_tree(&c_bundle[bundle_num].tree);
	c_bundle[bundle_num].cur_dec =
	c_bundle[bundle_num].cur_ptr = c_bundle[bundle_num].data;
}

/**
 * Initializes length length in all bundles.
 *
 * @param c     decoder context
 * @param width plane width
 * @param bw    plane width in 8x8 blocks
 */
void BIKPlay::init_lengths(int width, int bw)
{
	c_bundle[BINK_SRC_BLOCK_TYPES].len = av_log2((width >> 3) + 511) + 1;

	c_bundle[BINK_SRC_SUB_BLOCK_TYPES].len = av_log2((width >> 4) + 511) + 1;

	c_bundle[BINK_SRC_COLORS].len = av_log2((width >> 3)*64 + 511) + 1;

	c_bundle[BINK_SRC_INTRA_DC].len =
	c_bundle[BINK_SRC_INTER_DC].len =
	c_bundle[BINK_SRC_X_OFF].len =
	c_bundle[BINK_SRC_Y_OFF].len = av_log2((width >> 3) + 511) + 1;

	c_bundle[BINK_SRC_PATTERN].len = av_log2((bw << 3) + 511) + 1;

	c_bundle[BINK_SRC_RUN].len = av_log2((width >> 3)*48 + 511) + 1;
}

#define CHECK_READ_VAL(gb, b, t) \
	if (!b->cur_dec || (b->cur_dec > b->cur_ptr)) \
		return 0; \
	t = gb.get_bits(b->len); \
	if (!t) { \
		b->cur_dec = NULL; \
		return 0; \
	}

int BIKPlay::get_vlc2(int16_t (*table)[2], int bits, int max_depth)
{
	int code;
	int n, index, nb_bits;

	index= v_gb.peek_bits(bits);
	code = table[index][0];
	n    = table[index][1];

	if(max_depth > 1 && n < 0){
		v_gb.skip_bits(bits);

		nb_bits = -n;

		index= v_gb.peek_bits(nb_bits) + code;
		code = table[index][0];
		n    = table[index][1];
		if(max_depth > 2 && n < 0){
			v_gb.skip_bits(nb_bits);

			nb_bits = -n;

			index= v_gb.get_bits(nb_bits) + code;
			code = table[index][0];
			n    = table[index][1];
		}
	}
	v_gb.skip_bits(n);
	return code;
}

#define GET_HUFF(tree)  \
	(tree).syms[get_vlc2(bink_trees[(tree).vlc_num].table, \
		bink_trees[(tree).vlc_num].bits, 1)]

int BIKPlay::read_runs(Bundle *b)
{
	int i, t, v;

	CHECK_READ_VAL(v_gb, b, t);
	if (v_gb.get_bits(1)) {
		v = v_gb.get_bits(4);
		if (b->cur_dec + t > b->data_end) {
			return -1;
		}
		memset(b->cur_dec, v, t);
		b->cur_dec += t;
	} else {
		for (i = 0; i < t; i++) {
			v = GET_HUFF(b->tree);
			*b->cur_dec++ = v;
		}
	}
	return 0;
}

int BIKPlay::read_motion_values(Bundle *b)
{
	int i, t, v;

	CHECK_READ_VAL(v_gb, b, t);
	if (v_gb.get_bits(1)) {
		v = v_gb.get_bits(4);
		if (v && v_gb.get_bits(1)) {
			v = -v;
		}
		if (b->cur_dec + t > b->data_end) {
			return -1;
		}
		memset(b->cur_dec, v, t);
		b->cur_dec += t;
	} else {
		for (i = 0; i < t; i++) {
			v = GET_HUFF(b->tree);
			if (v && v_gb.get_bits(1)) {
				v = -v;
			}
			*b->cur_dec++ = v;
		}
	}
	return 0;
}

const uint8_t bink_rlelens[4] = { 4, 8, 12, 32 };

int BIKPlay::read_block_types(Bundle *b)
{
	int i, t, v;
	int last = 0;

	CHECK_READ_VAL(v_gb, b, t);
	if (v_gb.get_bits(1)) {
		v = v_gb.get_bits(4);
		memset(b->cur_dec, v, t);
		b->cur_dec += t;
	} else {
		for (i = 0; i < t; i++) {
			v = GET_HUFF(b->tree);
			if (v < 12) {
				last = v;
				*b->cur_dec++ = v;
			} else {
				int run = bink_rlelens[v - 12];

				memset(b->cur_dec, last, run);
				b->cur_dec += run;
				i += run - 1;
			}
		}
	}
	return 0;
}

int BIKPlay::read_patterns(Bundle *b)
{
	int i, t, v;

	CHECK_READ_VAL(v_gb, b, t);
	for (i = 0; i < t; i++) {
		v  = GET_HUFF(b->tree);
		v |= GET_HUFF(b->tree) << 4;
		*b->cur_dec++ = v;
	}

	return 0;
}

int BIKPlay::read_colors(Bundle *b)
{
	int i, t, v, v2;

	CHECK_READ_VAL(v_gb, b, t);
	if (v_gb.get_bits(1)) {
		v2 = GET_HUFF(c_col_high[c_col_lastval]);
		c_col_lastval = v2;
		v = GET_HUFF(b->tree);
		v = (v2 << 4) | v;
		memset(b->cur_dec, v, t);
		b->cur_dec += t;
	} else {
		for (i = 0; i < t; i++) {
			v2 = GET_HUFF(c_col_high[c_col_lastval]);
			c_col_lastval = v2;
			v = GET_HUFF(b->tree);
			v = (v2 << 4) | v;
			*b->cur_dec++ = v;
		}
	}
	return 0;
}

/** number of bits used to store first DC value in bundle */
#define DC_START_BITS 11

int BIKPlay::read_dcs(Bundle *b, int start_bits, int has_sign)
{
	int i, j, len, len2, bsize, v, v2;
	int16_t *dst = (int16_t*)b->cur_dec;

	CHECK_READ_VAL(v_gb, b, len);
	if (has_sign) {
		v = v_gb.get_bits(start_bits - 1);
		if (v && v_gb.get_bits(1))
			v = -v;
	} else {
		v = v_gb.get_bits(start_bits);
	}
	*dst++ = v;
	len--;
	for (i = 0; i < len; i += 8) {
		len2 = FFMIN(len - i, 8);
		bsize = v_gb.get_bits(4);
		if (bsize) {
			for (j = 0; j < len2; j++) {
				v2 = v_gb.get_bits(bsize);
				if (v2 && v_gb.get_bits(1)) {
					v2 = -v2;
				}
				v += v2;
				*dst++ = v;
				if (v < -32768 || v > 32767) {
					return -1;
				}
			}
		} else {
			for (j = 0; j < len2; j++) {
				*dst++ = v;
			}
		}
	}

	b->cur_dec = (uint8_t*)dst;
	return 0;
}

inline int BIKPlay::get_value(int bundle)
{
	int16_t ret;

	if (bundle < BINK_SRC_X_OFF || bundle == BINK_SRC_RUN) {
		return *c_bundle[bundle].cur_ptr++;
	}
	if (bundle == BINK_SRC_X_OFF || bundle == BINK_SRC_Y_OFF) {
		return (int8_t)*c_bundle[bundle].cur_ptr++;
	}
	ret = *(int16_t*)c_bundle[bundle].cur_ptr;
	c_bundle[bundle].cur_ptr += 2;
	return ret;
}

#define PUT2x2(dst, stride, x, y, pix) \
	dst[(x)*2 +     (y)*2       * stride] = \
	dst[(x)*2 + 1 + (y)*2       * stride] = \
	dst[(x)*2 +     ((y)*2 + 1) * stride] = \
	dst[(x)*2 + 1 + ((y)*2 + 1) * stride] = pix;

static void get_pixels(DCTELEM *block, const uint8_t *pixels, int line_size)
{
	int i;

	/* read the pixels */
	for(i=0;i<8;i++) {
		block[0] = pixels[0];
		block[1] = pixels[1];
		block[2] = pixels[2];
		block[3] = pixels[3];
		block[4] = pixels[4];
		block[5] = pixels[5];
		block[6] = pixels[6];
		block[7] = pixels[7];
		pixels += line_size;
		block += 8;
	}
}

uint8_t ff_cropTbl[256 + 2 * MAX_NEG_CROP] = {0, };

static void put_pixels_clamped(const DCTELEM *block, uint8_t *pixels, int line_size)
{
	int i;
	uint8_t *cm = ff_cropTbl + MAX_NEG_CROP;

	/* read the pixels */
	for(i=0;i<8;i++) {
		pixels[0] = cm[block[0]];
		pixels[1] = cm[block[1]];
		pixels[2] = cm[block[2]];
		pixels[3] = cm[block[3]];
		pixels[4] = cm[block[4]];
		pixels[5] = cm[block[5]];
		pixels[6] = cm[block[6]];
		pixels[7] = cm[block[7]];

		pixels += line_size;
		block += 8;
	}
}

static void add_pixels_clamped(const DCTELEM *block, uint8_t *pixels, int line_size)
{
	int i;
	uint8_t *cm = ff_cropTbl + MAX_NEG_CROP;

	/* read the pixels */
	for(i=0;i<8;i++) {
		pixels[0] = cm[pixels[0] + block[0]];
		pixels[1] = cm[pixels[1] + block[1]];
		pixels[2] = cm[pixels[2] + block[2]];
		pixels[3] = cm[pixels[3] + block[3]];
		pixels[4] = cm[pixels[4] + block[4]];
		pixels[5] = cm[pixels[5] + block[5]];
		pixels[6] = cm[pixels[6] + block[6]];
		pixels[7] = cm[pixels[7] + block[7]];
		pixels += line_size;
		block += 8;
	}
}

static inline void copy_block(DCTELEM block[64], const uint8_t *src, uint8_t *dst, int stride)
{
	get_pixels(block, src, stride);
	put_pixels_clamped(block, dst, stride);
}

#define clear_block(block) memset( (block), 0, sizeof(DCTELEM)*64);

static void idct_put(uint8_t *dest, int line_size, DCTELEM *block)
{
	j_rev_dct(block);
	put_pixels_clamped(block, dest, line_size);
}
static void idct_add(uint8_t *dest, int line_size, DCTELEM *block)
{
	j_rev_dct(block);
	add_pixels_clamped(block, dest, line_size);
}

int BIKPlay::DecodeVideoFrame(void *data, int data_size)
{
	int blk, bw, bh;
	int i, j, plane, bx, by;
	uint8_t *dst, *prev;
	int v, c1, c2;
	const uint8_t *scan;
	const uint32_t *quant;
	int xoff, yoff;
#pragma pack(push,16)
	DCTELEM block[64];
#pragma pack(pop)

		int bits = data_size*8;
	v_gb.init_get_bits((uint8_t *) data, bits);
	//this is compatible only with the BIKi version
	v_gb.skip_bits(32);

	get_buffer(&c_pic, header.width, header.height);
	//plane order is YUV
	for (plane = 0; plane < 3; plane++) {
		const int stride = c_pic.linesize[plane];

		bw = plane ? (header.width  + 15) >> 4 : (header.width  + 7) >> 3;
		bh = plane ? (header.height + 15) >> 4 : (header.height + 7) >> 3;
		if(plane) {
			init_lengths(header.width >> 1, bw);
		} else {
			init_lengths(header.width, bw);
		}

		for (i = 0; i < BINK_NB_SRC; i++) {
			read_bundle(i);
		}

		for (by = 0; by < bh; by++) {
			v_gb.debug("rbt");
			if (read_block_types(&c_bundle[BINK_SRC_BLOCK_TYPES]) < 0)
				return -1;
			v_gb.debug("rbts");
			if (read_block_types(&c_bundle[BINK_SRC_SUB_BLOCK_TYPES]) < 0)
				return -1;
			v_gb.debug("rc");
			if (read_colors(&c_bundle[BINK_SRC_COLORS]) < 0)
				return -1;
			v_gb.debug("rp");
			if (read_patterns(&c_bundle[BINK_SRC_PATTERN]) < 0)
				return -1;
			v_gb.debug("rmvx");
			if (read_motion_values(&c_bundle[BINK_SRC_X_OFF]) < 0)
				return -1;
			v_gb.debug("rmvy");
			if (read_motion_values(&c_bundle[BINK_SRC_Y_OFF]) < 0)
				return -1;
			v_gb.debug("rdcs0");
			if (read_dcs(&c_bundle[BINK_SRC_INTRA_DC], DC_START_BITS, 0) < 0)
				return -1;
			v_gb.debug("rdcs1");
			if (read_dcs(&c_bundle[BINK_SRC_INTER_DC], DC_START_BITS, 1) < 0)
				return -1;
			v_gb.debug("rr");
			if (read_runs(&c_bundle[BINK_SRC_RUN]) < 0)
				return -1;
			v_gb.debug("end");

			//why is this here?
			if (by == bh)
				break;

			dst = c_pic.data[plane] + 8*by*stride;
			prev = c_last.data[plane] + 8*by*stride;
			for (bx = 0; bx < bw; bx++, dst += 8, prev += 8) {
				blk = get_value(BINK_SRC_BLOCK_TYPES);
				if ((by & 1) && blk == 1) {
					bx++;
					dst  += 8;
					prev += 8;
					continue;
				}
				switch (blk) {
				case SKIP_BLOCK:
					copy_block(block, prev, dst, stride);
					break;
				case SCALED_BLOCK:
					blk = get_value(BINK_SRC_SUB_BLOCK_TYPES);
					switch (blk) {
					case RUN_BLOCK:
						scan = bink_patterns[v_gb.get_bits(4)];
						i = 0;
						do {
							int run = get_value(BINK_SRC_RUN) + 1;
							if (v_gb.get_bits(1)) {
								v = get_value(BINK_SRC_COLORS);
								for (j = 0; j < run; j++) {
									int pos = *scan++;
									PUT2x2(dst, stride, pos & 7, pos >> 3, v);
								}
							} else {
								for (j = 0; j < run; j++) {
									int pos = *scan++;
									PUT2x2(dst, stride, pos & 7, pos >> 3, get_value(BINK_SRC_COLORS));
								}
							}
							i += run;
						} while (i < 63);
							if (i == 63) {
								int pos = *scan++;
								PUT2x2(dst, stride, pos & 7, pos >> 3, get_value(BINK_SRC_COLORS));
							}
							break;
					case INTRA_BLOCK:
						clear_block(block);
						block[0] = get_value(BINK_SRC_INTRA_DC);
						read_dct_coeffs(block, c_scantable.permutated);
						quant = bink_intra_quant[v_gb.get_bits(4)];
						for (i = 0; i < 64; i++) {
							block[i] = MUL64(block[i], quant[i]) >> 16;
						}
						j_rev_dct(block);
						for (j = 0; j < 8; j++) {
							for (i = 0; i < 8; i++) {
								PUT2x2(dst, stride, i, j, block[i + j*8]);
							}
						}
						break;
					case FILL_BLOCK:
						v = get_value(BINK_SRC_COLORS);
							for (j = 0; j < 16; j++) {
							memset(dst + j*stride, v, 16);
						}
							break;
					case PATTERN_BLOCK:
						c1 = get_value(BINK_SRC_COLORS);
						c2 = get_value(BINK_SRC_COLORS);
						for (i = 0; i < 8; i++) {
							v = get_value(BINK_SRC_PATTERN);
							for (j = 0; j < 8; j++, v >>= 1) {
								PUT2x2(dst, stride, i, j, (v & 1) ? c2 : c1);
							}
						}
							break;
					case RAW_BLOCK:
						for (j = 0; j < 8; j++) {
							for (i = 0; i < 8; i++) {
								PUT2x2(dst, stride, i, j, get_value(BINK_SRC_COLORS));
							}
						}
						break;
					default:
						//av_log(avctx, AV_LOG_ERROR, "Incorrect 16x16 block type %d\n", blk);
						return -1;
					}
					bx++;
					dst += 8;
					prev += 8;
					break;
				case MOTION_BLOCK:
					xoff = get_value(BINK_SRC_X_OFF);
					yoff = get_value(BINK_SRC_Y_OFF);
					copy_block(block, prev + xoff + yoff*stride, dst, stride);
					break;
				case RUN_BLOCK:
					scan = bink_patterns[v_gb.get_bits(4)];
					i = 0;
					do {
						int run = get_value(BINK_SRC_RUN) + 1;
						if (v_gb.get_bits(1)) {
							v = get_value(BINK_SRC_COLORS);
							for (j = 0; j < run; j++) {
								int pos = *scan++;
								dst[(pos & 7) + (pos >> 3) * stride] = v;
							}
						} else {
							for (j = 0; j < run; j++) {
								int pos = *scan++;
								dst[(pos & 7) + (pos >> 3) * stride] = get_value(BINK_SRC_COLORS);
							}
						}
						i += run;
					} while (i < 63);
					if (i == 63) {
						int pos = *scan++;
						dst[(pos & 7) + (pos >> 3)*stride] = get_value(BINK_SRC_COLORS);
					}
					break;
				case RESIDUE_BLOCK:
					xoff = get_value(BINK_SRC_X_OFF);
					yoff = get_value(BINK_SRC_Y_OFF);
					copy_block(block, prev + xoff + yoff*stride, dst, stride);
					clear_block(block);
					v = v_gb.get_bits(7);
					read_residue(block, v);
					add_pixels_clamped(block, dst, stride);
					break;
				case INTRA_BLOCK:
					clear_block(block);
					block[0] = get_value(BINK_SRC_INTRA_DC);
					read_dct_coeffs(block, c_scantable.permutated);
					quant = bink_intra_quant[v_gb.get_bits(4)];
					for (i = 0; i < 64; i++)
						block[i] = MUL64(block[i], quant[i]) >> 16;
					idct_put(dst, stride, block);
					break;
				case FILL_BLOCK:
					v = get_value(BINK_SRC_COLORS);
					for (i = 0; i < 8; i++) {
						memset(dst + i*stride, v, 8);
					}
					break;
				case INTER_BLOCK:
					xoff = get_value(BINK_SRC_X_OFF);
					yoff = get_value(BINK_SRC_Y_OFF);
					copy_block(block, prev + xoff + yoff*stride, dst, stride);
					clear_block(block);
					block[0] = get_value(BINK_SRC_INTER_DC);
					read_dct_coeffs(block, c_scantable.permutated);
					quant = bink_inter_quant[v_gb.get_bits(4)];
					for (i = 0; i < 64; i++) {
						block[i] = MUL64(block[i], quant[i]) >> 16;
					}
					idct_add(dst, stride, block);
					break;
				case PATTERN_BLOCK:
					c1 = get_value(BINK_SRC_COLORS);
					c2 = get_value(BINK_SRC_COLORS);
					for (i = 0; i < 8; i++) {
						v = get_value(BINK_SRC_PATTERN);
						for (j = 0; j < 8; j++, v >>= 1) {
							dst[i + j*stride] = (v & 1) ? c2 : c1;
						}
					}
					break;
				case RAW_BLOCK:
					for (i = 0; i < 8; i++) {
						memcpy(dst + i*stride, c_bundle[BINK_SRC_COLORS].cur_ptr + i*8, 8);
					}
					c_bundle[BINK_SRC_COLORS].cur_ptr += 64;
					break;
				default:
					//av_log(avctx, AV_LOG_ERROR, "Unknown block type %d\n", blk);
					return -1;
				}
			}
		}
		v_gb.get_bits_align32();
	}

	if (video_frameskip) {
		video_frameskip--;
		video_skippedframes++;
	} else {
		unsigned int dest_x = (outputwidth - header.width) >> 1;
		unsigned int dest_y = (outputheight - header.height) >> 1;
		showFrame((ieByte *) c_pic.data, header.width, header.height, 0, 0, header.width, header.height, dest_x, dest_y);
		release_buffer(&c_last);
		c_last=c_pic;
	}
	return 0;
}