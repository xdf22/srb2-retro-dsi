#include <nds.h>
#include <fat.h>

#include "../doomdef.h"
#include "../doomstat.h"
#include "../d_clisrv.h"
#include "../d_main.h"
#include "../d_netcmd.h"
#include "../filesrch.h"
#include "../g_state.h"
#include "../m_misc.h"
#include "../i_system.h"
#include "../i_video.h"
#include "../i_sound.h"
#include "../i_joy.h"
#include "../z_zone.h"

UINT8 keyboard_started = 0;

static volatile tic_t ticcount;

#define timers2ms(tlow,thigh) ((tlow>>5)+(thigh<<11))

// Handy DSdev.org timer functions
u32 GetTicks(void)
{
	return timers2ms(TIMER0_DATA, TIMER1_DATA);
} 

void Pause(u32 ms)
{
	u32 now;
	now=timers2ms(TIMER0_DATA, TIMER1_DATA);
	while((u32)timers2ms(TIMER0_DATA, TIMER1_DATA)<now+ms);
}

void I_Sleep(void)
{
	Pause(cv_sleep.value/1000);
}

int ms_to_next_tick;

tic_t I_GetTime(void)
{
    int t = GetTicks();
    int i = t*(TICRATE/5)/200;
    ms_to_next_tick = (i+1)*200/(TICRATE/5) - t;
    if (ms_to_next_tick > 1000/TICRATE || ms_to_next_tick<1) ms_to_next_tick = 1;
    return i;
}

UINT32 I_GetFreeMem(UINT32 *total)
{
	*total = 12*1024*1024;
	return 12*1024*1024;
}

void I_GetEvent(void)
{
    static touchPosition last_touch_position;
	scanKeys();
	u16 keys = keysDown();
	
	event_t e_w;

	if (keys & KEY_A) {
		event_t event;
		event.type = ev_keydown;
		
		if (menuactive || gamestate == GS_TITLESCREEN)
			event.data1 = KEY_ENTER;
		else
			event.data1 = 'z';
		
		D_PostEvent(&event);
	}
	
	if (keys & KEY_B) {
		event_t event;
		event.type = ev_keydown;
		
		if (menuactive || gamestate == GS_TITLESCREEN)
			event.data1 = KEY_ESCAPE;
		else
			event.data1 = 'x';
		
		D_PostEvent(&event);
	}
	
	if (keys & KEY_START) {
		event_t event;
		event.type = ev_keydown;
		event.data1 = KEY_ESCAPE;
		D_PostEvent(&event);
	}

	if (keys & KEY_UP) {
		event_t event;
		event.type = ev_keydown;
		event.data1 = KEY_UPARROW;
		D_PostEvent(&event);
	}
	
	if (keys & KEY_DOWN) {
		event_t event;
		event.type = ev_keydown;
		event.data1 = KEY_DOWNARROW;
		D_PostEvent(&event);
	}

	if (keys & KEY_LEFT) {
		event_t event;
		event.type = ev_keydown;
		
		if (menuactive || gamestate == GS_TITLESCREEN)
			event.data1 = KEY_LEFTARROW;
		else
			event.data1 = 'a';
		
		D_PostEvent(&event);
	}
	
	if (keys & KEY_RIGHT) {
		event_t event;
		event.type = ev_keydown;
		
		if (menuactive || gamestate == GS_TITLESCREEN)
			event.data1 = KEY_RIGHTARROW;
		else
			event.data1 = 'd';

		D_PostEvent(&event);
	}
	
	if (keys & KEY_L) {
		event_t event;
		event.type = ev_keydown;
		
		if (cv_analog.value)
			event.data1 = '[';
		else
			event.data1 = KEY_LEFTARROW;

		D_PostEvent(&event);
	}
	
	if (keys & KEY_R) {
		event_t event;
		event.type = ev_keydown;
		
		if (cv_analog.value)
			event.data1 = ']';
		else
			event.data1 = KEY_RIGHTARROW;

		D_PostEvent(&event);
	}

    // lowk took this from srb2_3ds
	if(keysHeld() & KEY_TOUCH) {
        event_t event;
		touchPosition current_touch_position;
		touchRead(&current_touch_position);
		if (!(keysDown() & KEY_TOUCH)) {
			event.type = ev_mouse;
			event.data1 = 0;
			event.data2 = (current_touch_position.px - last_touch_position.px);
			event.data3 = -(current_touch_position.py - last_touch_position.py);
			D_PostEvent(&event);
		}
		last_touch_position = current_touch_position;
	}
	
	keys = keysUp();
	
	if (keys & KEY_A) {
		event_t event;
		event.type = ev_keyup;
		
		if (menuactive || gamestate == GS_TITLESCREEN)
			event.data1 = KEY_ENTER;
		else
			event.data1 = 'z';
		
		D_PostEvent(&event);
	}
	
	if (keys & KEY_B) {
		event_t event;
		event.type = ev_keyup;
		
		if (menuactive || gamestate == GS_TITLESCREEN)
			event.data1 = KEY_ESCAPE;
		else
			event.data1 = 'x';
		
		D_PostEvent(&event);
	}
	
	if (keys & KEY_START) {
		event_t event;
		event.type = ev_keyup;
		event.data1 = KEY_ESCAPE;
		D_PostEvent(&event);
	}

	if (keys & KEY_UP) {
		event_t event;
		event.type = ev_keyup;
		event.data1 = KEY_UPARROW;
		D_PostEvent(&event);
	}
	
	if (keys & KEY_DOWN) {
		event_t event;
		event.type = ev_keyup;
		event.data1 = KEY_DOWNARROW;
		D_PostEvent(&event);
	}

	if (keys & KEY_LEFT) {
		event_t event;
		event.type = ev_keyup;
		
		if (menuactive || gamestate == GS_TITLESCREEN)
			event.data1 = KEY_LEFTARROW;
		else
			event.data1 = 'a';
		
		D_PostEvent(&event);
	}
	
	if (keys & KEY_RIGHT) {
		event_t event;
		event.type = ev_keyup;
		
		if (menuactive || gamestate == GS_TITLESCREEN)
			event.data1 = KEY_RIGHTARROW;
		else
			event.data1 = 'd';
		
		D_PostEvent(&event);
	}
	
	if (keys & KEY_L) {
		event_t event;
		event.type = ev_keyup;
		
		if (cv_analog.value)
			event.data1 = '[';
		else
			event.data1 = KEY_LEFTARROW;

		D_PostEvent(&event);
	}
	
	if (keys & KEY_R) {
		event_t event;
		event.type = ev_keyup;
		
		if (cv_analog.value)
			event.data1 = ']';
		else
			event.data1 = KEY_RIGHTARROW;

		D_PostEvent(&event);
	}

	// keyboard
	int16_t c = keyboardUpdate();
	if (c != -1)
	{
		event_t event;

		// backspace
		if (c == '\b')
		{
			event.type = ev_keydown;
			event.data1 = KEY_BACKSPACE;
			D_PostEvent(&event);

			event.type = ev_keyup;
			event.data1 = KEY_BACKSPACE;
			D_PostEvent(&event);
		}
		else if (c >= 32)
		{
			// key down
			event.type = ev_keydown;
			event.data1 = c;
			D_PostEvent(&event);

			// key up
			event.type = ev_keyup;
			event.data1 = c;
			D_PostEvent(&event);
		}
	}
}

void I_OsPolling(void)
{
	I_GetEvent();
}

ticcmd_t *I_BaseTiccmd(void)
{
	static ticcmd_t emptyticcmd;
	return &emptyticcmd;
}

ticcmd_t *I_BaseTiccmd2(void)
{
	static ticcmd_t emptyticcmd2;
	return &emptyticcmd2;
}

void I_Quit(void)
{
	exit(0);
}

FUNCIERROR void I_Error(const char *error, ...)
{
    // Format the error string
    va_list args;
    va_start(args, error);

    int len = vsnprintf(NULL, 0, error, args);
    va_end(args);

    char buffer[512];
    vsnprintf(buffer, sizeof(buffer), error, args);
    printf("SRB2 Error:\n%s\n", buffer);


    M_SaveConfig(NULL);
    D_QuitNetGame();
    I_ShutdownGraphics();
    I_ShutdownSound();
    I_ShutdownMusic();
    I_ShutdownSystem();

    while(1) 
	{
        swiWaitForVBlank();
    }
}


void I_Tactile(FFType Type, const JoyFF_t *Effect)
{
	(void)Type;
	(void)Effect;
}

void I_Tactile2(FFType Type, const JoyFF_t *Effect)
{
	(void)Type;
	(void)Effect;
}

void I_JoyScale(void){}

void I_JoyScale2(void){}

void I_InitJoystick(void)
{
	Joystick.bGamepadStyle = true;
}

void I_InitJoystick2(void){}

INT32 I_NumJoys(void)
{
	return 0;
}

const char *I_GetJoyName(INT32 joyindex)
{
	(void)joyindex;
	return NULL;
}

void I_SetupMumble(void)
{
}

void I_UpdateMumble(const MumblePos_t *MPos)
{
	(void)MPos;
}

FUNCPRINTF void I_OutputMsg(const char *error, ...)
{
	va_list args;
	va_start(args, error);

	int len = vsnprintf(NULL, 0, error, args);
	va_end(args);

    char buffer[512];
    vsnprintf(buffer, sizeof(buffer), error, args);
}

void I_StartupMouse(void){}

void I_StartupMouse2(void){}

void I_StartupKeyboard(void){}

INT32 I_GetKey(void)
{
	return 0;
}

static void NDS_VBlankHandler(void)
{
	ticcount++;
}

void I_StartupTimer(void)
{
	irqSet(IRQ_VBLANK, NDS_VBlankHandler);
}

void I_AddExitFunc(void (*func)())
{
	(void)func;
}

void I_RemoveExitFunc(void (*func)())
{
	(void)func;
}

INT32 I_StartupSystem(void)
{
	return 0;
}

void I_ShutdownSystem(void){}

void I_GetDiskFreeSpace(INT64* freespace)
{
	*freespace = 0;
}

char *I_GetUserName(void)
{
	return NULL;
}

INT32 I_mkdir(const char *dirname, INT32 unixright)
{
	return mkdir(dirname, unixright);
}

const CPUInfoFlags *I_CPUInfo(void)
{
	return NULL;
}

const char *I_LocateWad(void)
{
	/*
	chdir("nitro:");
	return "nitro:";
	*/
    chdir(va("%s%s", D_Home(), DEFAULTDIR));
	return va("%s%s", D_Home(), DEFAULTDIR);
}

void I_GetJoystickEvents(void){}

void I_GetJoystick2Events(void){}

void I_GetMouseEvents(void){}

char *I_GetEnv(const char *name)
{
	(void)name;
	return NULL;
}

INT32 I_PutEnv(char *variable)
{
	(void)variable;
	return -1;
}

void I_RegisterSysCommands(void) {}

#include "../sdl/dosstr.c"
