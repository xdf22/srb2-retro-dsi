#include "../i_sound.h"
#include <filesystem.h>
#include <maxmod9.h>
#include <nds.h>
#include <nds/arm9/dldi.h>

boolean srb2_playsong = false;
boolean srb2_loopsong = false;

// streaming example thingy

#define DATA_ID 0x61746164
#define FMT_ID  0x20746d66
#define RIFF_ID 0x46464952
#define WAVE_ID 0x45564157

typedef struct WAVHeader
{
    // "RIFF" chunk descriptor
    uint32_t chunkID;
    uint32_t chunkSize;
    uint32_t format;
    // "fmt" subchunk
    uint32_t subchunk1ID;
    uint32_t subchunk1Size;
    uint16_t audioFormat;
    uint16_t numChannels;
    uint32_t sampleRate;
    uint32_t byteRate;
    uint16_t blockAlign;
    uint16_t bitsPerSample;
    // "data" subchunk
    uint32_t subchunk2ID;
    uint32_t subchunk2Size;
}
WAVHeader_t;

#define BUFFER_LENGTH 1024

FILE *wavFile = NULL;

uint8_t stream_buffer[BUFFER_LENGTH];
uint32_t stream_buffer_in;
uint32_t stream_buffer_out;

mm_word streamingCallback(mm_word length,
                          mm_addr dest,
                          mm_stream_formats format)
{
    size_t bytes_until_end = BUFFER_LENGTH - stream_buffer_out;
	
	if (srb2_playsong)
		if (bytes_until_end > length)
		{
			uint8_t *src_ = &stream_buffer[stream_buffer_out];

			memcpy(dest, src_, length);
			stream_buffer_out += length;
		}
		else
		{
			uint8_t *src_ = &stream_buffer[stream_buffer_out];
			uint8_t *dst_ = dest;

			memcpy(dst_, src_, bytes_until_end);
			dst_ += bytes_until_end;
			length -= bytes_until_end;

			src_ = &stream_buffer[0];
			memcpy(dst_, src_, length);
			stream_buffer_out = length;
		}

    return length;
}

void readFile(uint8_t *buffer, size_t size)
{
	if (!srb2_playsong)
		return;
	
    while (size > 0)
    {
        int res = fread(buffer, 1, size, wavFile);
        size -= res;
        buffer += res;

        if (feof(wavFile))
        {
            // Loop back when song ends
			if (srb2_loopsong) {
				fseek(wavFile, sizeof(WAVHeader_t), SEEK_SET);
				res = fread(buffer, 1, size, wavFile);
				size -= res;
				buffer += res;
			} else {
				I_StopDigSong(); 
				break;
			}
		}
    }
}

void streamingFillBuffer(void)
{
    if (stream_buffer_in < stream_buffer_out)
    {
        size_t size = stream_buffer_out - stream_buffer_in;
        readFile(&stream_buffer[stream_buffer_in], size);
        stream_buffer_in += size;
    }
    else
    {
        size_t size = BUFFER_LENGTH - stream_buffer_in;
        readFile(&stream_buffer[stream_buffer_in], size);
        stream_buffer_in = 0;

        size = stream_buffer_out - stream_buffer_in;
        readFile(&stream_buffer[stream_buffer_in], size);
        stream_buffer_in += size;
    }

    if (stream_buffer_in >= BUFFER_LENGTH)
        stream_buffer_in -= BUFFER_LENGTH;
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
	soundEnable();
	sound_started = 0;
}

void I_ShutdownSound(void){}

//
//  SFX I/O
//

INT32 I_StartSound(sfxenum_t id, INT32 vol, INT32 sep, INT32 pitch, INT32 priority)
{
	return soundPlaySample(S_sfx[id].data, SoundFormat_8Bit, S_sfx[id].length, 8000, 127, 0, false, 0);
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
WAVHeader_t wavHeader = { 0 };

void I_InitMusic(void){
    mmInitNoSoundbank();
	
	stream.sampling_rate = wavHeader.sampleRate,
    stream.buffer_length = 128,
    stream.callback      = streamingCallback,
    stream.format        = MM_STREAM_8BIT_MONO,
    stream.timer         = MM_TIMER3,
    stream.manual        = false,

	streamingFillBuffer();

	digmusic_started = 1;
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
}

void I_ShutdownDigMusic(void){
}

boolean I_StartDigSong(const char *musicname, INT32 looping)
{
	wavFile = fopen(va("nitro:/music/O_%s.wav", strupr(musicname)), "rb");

	if (fread(&wavHeader, 1, sizeof(WAVHeader_t), wavFile) != sizeof(WAVHeader_t))
        return false;

	mmStreamClose();
	stream.sampling_rate = wavHeader.sampleRate,
    stream.buffer_length = 128,
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
	mmStreamClose();
}

void I_SetDigMusicVolume(INT32 volume)
{
	if (srb2_playsong)
		mmStreamVolume(volume);
}

boolean I_SetSongSpeed(float speed)
{
	(void)speed;
	return false;
}
