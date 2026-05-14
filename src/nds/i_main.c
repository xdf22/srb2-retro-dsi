#include "../doomdef.h"
#include "../d_main.h"
#include "../m_argv.h"

#include <nds.h>
#include <fat.h>
#include <filesystem.h>

PrintConsole gameConsole;
Keyboard *keyboard;

int main(int argc, char **argv)
{
	// wait for a few frames so we can get NDS firmware data
	swiWaitForVBlank();
    swiWaitForVBlank();
	
	myargc = argc;
	myargv = argv; /// \todo pull out path to exe from this string

	defaultExceptionHandler(); // debug
	
	TIMER0_DATA=0;	// Set up the timer
	TIMER1_DATA=0;//
	TIMER0_CR=TIMER_DIV_1024 | TIMER_ENABLE;
	TIMER1_CR=TIMER_CASCADE | TIMER_ENABLE;

	// start FAT (for assets)
	if (!fatInitDefault())
		I_Error("Failed to initialize FAT!\n");
    
	// start NitroFS (unused)
	/*
	if (!nitroFSInit(NULL))
		I_Error("Failed to initialize NitroFS!\n");
	*/
	
	chdir(D_Home());

    // init bottom screen
	videoSetModeSub(MODE_0_2D);
	vramSetBankC(VRAM_C_SUB_BG);
	
	// init console
	consoleInit(&gameConsole, 0, BgType_Text4bpp, BgSize_T_256x256, 22, 3, false, true);
	consoleSetWindow(&gameConsole, 0, 0, 32, 14);
	consoleSelect(&gameConsole);
	
	// init keyboard
	keyboard = keyboardInit(NULL, 3, BgType_Text4bpp, BgSize_T_256x512, 20, 0, false, true);
	keyboard->scrollSpeed = 0;
	keyboardShow();
	
	// startup SRB2
	CONS_Printf("Setting up SRB2...\n");
	D_SRB2Main();
	CONS_Printf("Entering main game loop...\n");
	
	// never return
	D_SRB2Loop();

	// return to OS
#ifndef __GNUC__
	return 0;
#endif
}
