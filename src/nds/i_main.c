#include "../doomdef.h"
#include "../d_main.h"
#include "../m_argv.h"

#include <nds.h>
#include <fat.h>
#include <filesystem.h>

int main(int argc, char **argv)
{
	myargc = argc;
	myargv = argv; /// \todo pull out path to exe from this string

	defaultExceptionHandler(); // debug
	
	TIMER0_DATA=0;	// Set up the timer
	TIMER1_DATA=0;//
	TIMER0_CR=TIMER_DIV_1024 | TIMER_ENABLE;
	TIMER1_CR=TIMER_CASCADE | TIMER_ENABLE;

	// start NitroFS (for assets)
	if (!nitroFSInit(NULL))
		I_Error("Failed to initialize nitroFS!\n");
    chdir("nitro:/");

	// start FAT (for D_Home)
	//if (!fatInitDefault())
	//	I_Error("Failed to initialize FAT!\n");
	//chdir("sd:/");

    consoleDemoInit(); // init console
	keyboardDemoInit(); // init keyboard

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
