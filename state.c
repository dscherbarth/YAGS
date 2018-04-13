///////////////////////////////////////////////////////////////
//
//	state.c
//		state machine implementation
//
//////////////////////////////////////////////////////////////
#include <gtk/gtk.h>

#include <wiringPi.h>

#include <errno.h>
#include <termios.h>
#include <unistd.h>
#include <memory.h>
#include <fcntl.h>
#include <sys/stat.h>

#define STATEIDLE	1
#define STATEGCPLAY	2
#define STATETOOLCH	3
#define STATEFREEH	4

#define STATELEVEL	6	// auto level (pcb)
#define STATEZPROBE	7	// contact zprobe

int state = 1;

int isZProbe (void)
{
	if (state == STATEZPROBE) return 1;
	else return 0;
}

int isLevel (void)
{
	if (state == STATELEVEL) return 1;
	else return 0;
}

int isIdle(void)
{
	if (state == STATEIDLE) return 1;
	else return 0;
}

int isFreeh(void)
{
	if (state == STATEFREEH) return 1;
	else return 0;
}

int isToolch(void)
{
	if (state == STATETOOLCH) return 1;
	else return 0;
}

int isGCPlay(void)
{
	if (state == STATEGCPLAY) return 1;
	else return 0;
}

extern GtkWidget *Play;
extern GtkWidget *ZProbe;

void Tr_ZProbe(void)
{
	if (!isCommUp())
		return;
		
	if (state == STATEIDLE)	
	{
		// send the probe command
		gtk_button_set_label(GTK_BUTTON(ZProbe), "ZProbing");
		sersendz ("g38.2 f25 z-2\n");
		
		// set the state
		state = STATEZPROBE;
	}
	else if (state == STATEZPROBE)
	{
		state = STATEIDLE;
	}
}

// transition to Play
void Tr_Play(void)
{
	if (!isCommUp())
		return;
		
	if (state == STATEIDLE)	
	{
		// from idle to play a gcode file
		// get the file name and handle and setup to send itoa
		PlayCGFile ();
		
		// state is GCPLAY
		state = STATEGCPLAY;
	}
	else if (state == STATEFREEH)
	{
		// ~ to return from freehold
		sersendNT ("~");		// restart

		// state is GCPLAY
		state = STATEGCPLAY;
	}
	else if (state == STATETOOLCH)
	{
		// restart after a toolchange
		// change the button text
		gtk_button_set_label(GTK_BUTTON(Play), "Play");
		
		// state is GCPLAY
		state = STATEGCPLAY;
	}
}

// transition to Pause
void Tr_Pause(void)
{
	if (state == STATEGCPLAY)
	{
		// ! to enter freehold
		sersendNT ("!");		// pause
		
		// state is FREEHOLD
		state = STATEFREEH;
	}
}

extern int exline;
extern GtkWidget *Level;


// transition to stop
void Tr_Stop(void)
{
	if (state == STATEGCPLAY)	
	{
		sersendNT ("!");		// pause
		
		// player is going to get one more increment..
		exline = -1;
		
		// state is FREEHOLD
		state = STATEFREEH;

		usleep(150000);
		
		// from gcplay to idle
		term_send();

		// % to idle from freehold
		sersendNT ("!%");		// clear queue

		// state is GCPLAY
		state = STATEIDLE;
		usleep(350000);
		updateExLine ((int) 0, 0);

	}
	else if (state == STATEFREEH)
	{
		// % to idle from freehold
		sersendNT ("!%");		// restart
		
		// state is GCPLAY
		state = STATEIDLE;
		usleep(150000);
		updateExLine ((int) 0, 0);
	}
	else if (state == STATELEVEL)
	{
		gtk_button_set_label(GTK_BUTTON(Level), "Level");
		
		// set state back to idle
		state = STATEIDLE;
	}
}

// transition to file complete
void Tr_FileComplete(void)
{
	if (state == STATEGCPLAY)	
	{
		// from gcplay to idle

		// state is GCPLAY
		state = STATEIDLE;
	}
}

// transition to M3
void Tr_M6(void)
{
	if (state == STATEGCPLAY)	
	{
		// from gcplay to tool change
		gdk_threads_enter();
		gtk_button_set_label(GTK_BUTTON(Play), "Change Comp");
		gdk_threads_leave();

		// state is GCPLAY
		state = STATETOOLCH;
	}
}

// transition to level
void Tr_Level(void)
{
	if (state == STATEIDLE)	
	{
		// show that leveling is active
		gtk_button_set_label(GTK_BUTTON(Level), "Leveling");

		// state is GCPLAY
		state = STATELEVEL;

		// init the location
		levInitPos();
	} else if (state == STATELEVEL)
	{
		// change the button
		gdk_threads_enter();
		gtk_button_set_label(GTK_BUTTON(Level), "Level");
		gdk_threads_leave();
		
		// set state back to idle
		state = STATEIDLE;
	}
}

