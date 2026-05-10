#include "../byteptr.h"
#include "../doomstat.h"
#include "../i_sound.h"
#include "../w_wad.h"
#include "../z_zone.h"
#include <filesystem.h>
#include <maxmod9.h>
#include <nds.h>
#include <nds/arm9/dldi.h>

boolean srb2_playsong = false;
boolean srb2_loopsong = false;

uint8_t *wavData;
int wavLen;
int wavPos = 0;

int stream_buffer_in;
int stream_buffer_out;

mm_word streamingCallback(mm_word length,
                          mm_addr dest,
                          mm_stream_formats format)
{
	if (!srb2_playsong)
		return 0;
	
    size_t bytes_until_end = wavLen - stream_buffer_out;

    if (bytes_until_end > length)
    {
        char *src_ = (char *)&wavData[stream_buffer_out];

        M_Memcpy(dest, src_, length);
        stream_buffer_out += length;
    }
    else
    {
        char *src_ = (char *)&wavData[stream_buffer_out];
        char *dst_ = dest;

        M_Memcpy(dst_, src_, bytes_until_end);
        dst_ += bytes_until_end;
		length -= bytes_until_end;

        src_ = (char *)&wavData[0];
        M_Memcpy(dst_, src_, length);
        stream_buffer_out = length;
    }

    return length;
}

// This reads bytes from wavFile into the provided buffer. If the end of the
// file is reached, it starts from the start again.
void readFile(size_t size)
{
	if (!srb2_playsong)
		return;
	
    while (size > 0)
    {
        size--;
		wavPos++;

        if (wavPos >= wavLen-1)
        {
            // Loop back when song ends
			
			if (srb2_loopsong) {
				wavPos = 0;
				size--;
				wavPos++;
			} else
				I_StopDigSong();
        }
    }
}

void streamingFillBuffer(bool force_fill)
{
    if (!force_fill)
    {
        if (stream_buffer_in == stream_buffer_out)
            return;
    }
	
	if (!srb2_playsong)
		return;

    if (stream_buffer_in < stream_buffer_out)
    {
        size_t size = stream_buffer_out - stream_buffer_in;
        readFile(size);
        stream_buffer_in += size;
    }
    else
    {
        size_t size = wavLen - stream_buffer_in;
        readFile(size);
        stream_buffer_in = 0;

        size = stream_buffer_out - stream_buffer_in;
        readFile(size);
        stream_buffer_in += size;
    }

    if (stream_buffer_in >= wavLen-1)
		if (srb2_loopsong) {
			stream_buffer_in = stream_buffer_out = 0;
		} else
			I_StopDigSong();
}

UINT8 sound_started = 0;

void *I_GetSfx(sfxinfo_t *sfx)
{
	(void)sfx;
	return NULL;
}

void I_FreeSfx(sfxinfo_t *sfx)
{
	(void)sfx;
}

void I_StartupSound(void){
}

void I_ShutdownSound(void){}

//
//  SFX I/O
//

INT32 I_StartSound(sfxenum_t id, INT32 vol, INT32 sep, INT32 pitch, INT32 priority)
{
	return -1;
	//return soundPlaySample(S_sfx[id].data, SoundFormat_8Bit, S_sfx[id].length, 8000, 127, 0, false, 0);
}

void I_StopSound(INT32 handle)
{
	(void)handle;
}

INT32 I_SoundIsPlaying(INT32 handle)
{
	(void)handle;
	return false;
}

void I_UpdateSoundParams(INT32 handle, INT32 vol, INT32 sep, INT32 pitch)
{
	(void)handle;
	(void)vol;
	(void)sep;
	(void)pitch;
}

void I_SetSfxVolume(INT32 volume)
{
	(void)volume;
}

//
//  MUSIC I/O
//
UINT8 music_started = 0;
UINT8 digmusic_started = 0;
mm_stream stream;

void I_InitMusic(void){
    mmInitNoSoundbank();
	music_started = 1;
}

void I_ShutdownMusic(void){}

void I_PauseSong(INT32 handle)
{
	(void)handle;
}

void I_ResumeSong(INT32 handle)
{
	(void)handle;
}

//
//  MIDI I/O
//

UINT8 midimusic_started = 0;

void I_InitMIDIMusic(void){}

void I_ShutdownMIDIMusic(void){}

void I_SetMIDIMusicVolume(INT32 volume)
{
	(void)volume;
}

INT32 I_RegisterSong(void *data, size_t len)
{
	(void)data;
	(void)len;
	return -1;
}

boolean I_PlaySong(INT32 handle, INT32 looping)
{
	(void)handle;
	(void)looping;
	return false;
}

void I_StopSong(INT32 handle)
{
	(void)handle;
}

void I_UnRegisterSong(INT32 handle)
{
	(void)handle;
}

//
//  DIGMUSIC I/O
//

void I_InitDigMusic(void){
	soundEnable();
	
	stream.sampling_rate = 8000,
    stream.buffer_length = 2048,
    stream.callback      = streamingCallback,
    stream.format        = MM_STREAM_8BIT_MONO,
    stream.timer         = MM_TIMER3,
    stream.manual        = false,

	streamingFillBuffer(true);

	digmusic_started = 1;
}

void I_ShutdownDigMusic(void){
	mmStreamClose();
	soundDisable();
}

boolean I_StartDigSong(const char *musicname, INT32 looping)
{
	if (nodigimusic) {
		Z_Free(wavData);
		return false;
	}
	
	char filename[9];
	lumpnum_t lumpnum;
	size_t lumplength;
	
	snprintf(filename, sizeof filename, "o_%s\n", musicname);
	strupr(filename);
	if (W_CheckNumForName(filename) == LUMPERROR)
		return false;
	
	wavData = W_CacheLumpName(filename, PU_MUSIC);
	wavLen = W_LumpLength(W_CheckNumForName(filename));
	wavPos = 0;
	
	I_StopDigSong();
	stream.sampling_rate = 8000,
    stream.buffer_length = 2048,
    stream.callback      = streamingCallback,
    stream.format        = MM_STREAM_8BIT_MONO,
    stream.timer         = MM_TIMER3,
    stream.manual        = false,
	mmStreamOpen(&stream);
	
	srb2_playsong = true;
	srb2_loopsong = (looping > 0) ? true : false;
	return true;
}

void I_StopDigSong(void){
	srb2_playsong = false;
	srb2_loopsong = false;
	stream_buffer_in = 0;
	stream_buffer_out = 0;
	mmStreamClose();
}

void I_SetDigMusicVolume(INT32 volume)
{
	if (srb2_playsong)
		mmStreamVolume(volume);
}

boolean I_SetSongSpeed(float speed)
{
	stream.sampling_rate = (int)((float)stream.sampling_rate * speed);
	return true;
}
