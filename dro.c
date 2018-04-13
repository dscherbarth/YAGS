#include <gtk/gtk.h>

#include <wiringPi.h>

#include <errno.h>
#include <termios.h>
#include <unistd.h>
#include <memory.h>
#include <fcntl.h>
#include <sys/stat.h>

extern struct encoder *setupencoder(int pin_a, int pin_b); 
extern void setupui (int argc, char **argv);
extern void uiUpdate(void);
extern void serUpdate (void);


struct encoder
{
    int pin_a;
    int pin_b;
    volatile long value;
    volatile int lastEncoded;
};

struct encoder *encoder;

int main (int argc, char **argv)
{

    // init the i/o and setup the jog wheel
	quadInit();

	// setup the ui
	setupui(argc, argv);
	
    // connect the quad interface
    // start an update thread
    uiUpdate();

    // locate and connect the usb-serial interface to the tinyg
	serUpdate ();

    /* Start main loop */
    gdk_threads_enter();
    gtk_main();
    gdk_threads_leave();

    return( 0 );
}
