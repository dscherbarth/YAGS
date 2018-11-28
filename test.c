/*
 * Compile me with:
 *   gcc -o tut tut.c $(pkg-config --cflags --libs gtk+-2.0 gmodule-2.0)
 *
 * build using:  gcc `pkg-config gtk+-2.0 --cflags` test.c -o test `pkg-config gtk+-2.0 --libs`
 */

#include <gtk/gtk.h>

#include <wiringPi.h>

#include <errno.h>
#include <termios.h>
#include <unistd.h>
#include <memory.h>
#include <fcntl.h>
#include <sys/stat.h>

//17 pins / 2 pins per encoder = 8 maximum encoders
#define max_encoders 8

struct encoder
{
    int pin_a;
    int pin_b;
    volatile long value;
    volatile int lastEncoded;
};

//Pre-allocate encoder objects on the stack so we don't have to
//worry about freeing them
struct encoder encoders[max_encoders];

/*
  Should be run for every rotary encoder you want to control
  Returns a pointer to the new rotary encoder structer
  The pointer will be NULL is the function failed for any reason
*/
struct encoder *setupencoder(int pin_a, int pin_b); 

int numberofencoders = 0;

void updateEncoders()
{
    struct encoder *encoder = encoders;
    for (; encoder < encoders + numberofencoders; encoder++)
    {
        int MSB = digitalRead(encoder->pin_a);
        int LSB = digitalRead(encoder->pin_b);

        int encoded = (MSB << 1) | LSB;
        int sum = (encoder->lastEncoded << 2) | encoded;

        if(sum == 0b1101 || sum == 0b0100 || sum == 0b0010 || sum == 0b1011) encoder->value++;
        if(sum == 0b1110 || sum == 0b0111 || sum == 0b0001 || sum == 0b1000) encoder->value--;

        encoder->lastEncoded = encoded;
    }
}

struct encoder *setupencoder(int pin_a, int pin_b)
{
    if (numberofencoders > max_encoders)
    {
        printf("Maximum number of encodered exceded: %i\n", max_encoders);
        return NULL;
    }

    struct encoder *newencoder = encoders + numberofencoders++;
    newencoder->pin_a = pin_a;
    newencoder->pin_b = pin_b;
    newencoder->value = 0;
    newencoder->lastEncoded = 0;

    pinMode(pin_a, INPUT);
    pinMode(pin_b, INPUT);
    pullUpDnControl(pin_a, PUD_UP);
    pullUpDnControl(pin_b, PUD_UP);
    wiringPiISR(pin_a,INT_EDGE_BOTH, updateEncoders);
    wiringPiISR(pin_b,INT_EDGE_BOTH, updateEncoders);

    return newencoder;
}

GtkWidget *droX;
GtkWidget *jogX;
GtkWidget *droY;
GtkWidget *jogY;
GtkWidget *droZ;
GtkWidget *jogZ;
GtkWidget *droA;
GtkWidget *jogA;

GtkWidget *jog1;
GtkWidget *jogdot1;
GtkWidget *jogdot01;
GtkWidget *jogdot001;

GtkWidget *zeroX;
GtkWidget *zeroY;
GtkWidget *zeroZ;
GtkWidget *zeroA;

double 	dX = 0.0, dY = 0.0, dZ = 0.0, dA = 0.0;

void zeroX_Clicked(GtkButton * b, gpointer data)
{
    (void)b;
    (void)data;
    dX = 0.0;
    gtk_label_set (GTK_LABEL (droX), "000.000");
}

void zeroY_Clicked(GtkButton * b, gpointer data)
{
    (void)b;
    (void)data;
    dY = 0.0;
    gtk_label_set (GTK_LABEL (droY), "000.000");
}

void zeroZ_Clicked(GtkButton * b, gpointer data)
{
    (void)b;
    (void)data;
    dZ = 0.0;
    gtk_label_set (GTK_LABEL (droZ), "000.000");
}

void zeroA_Clicked(GtkButton * b, gpointer data)
{
    (void)b;
    (void)data;
    dA = 0.0;
    gtk_label_set (GTK_LABEL (droA), "000.000");
}

int 	jogWhat = -1;	// default, no jogging
void jogX_Clicked(GtkButton * b, gpointer data)
{
    (void)b;
    (void)data;
    gtk_button_set_label(GTK_BUTTON(jogX), "(JogX)");
    gtk_button_set_label(GTK_BUTTON(jogY), "JogY");
    gtk_button_set_label(GTK_BUTTON(jogZ), "JogZ");
    gtk_button_set_label(GTK_BUTTON(jogA), "JogA");
    jogWhat = 0;
}

void jogY_Clicked(GtkButton * b, gpointer data)
{
    (void)b;
    (void)data;
    gtk_button_set_label(GTK_BUTTON(jogX), "JogX");
    gtk_button_set_label(GTK_BUTTON(jogY), "(JogY)");
    gtk_button_set_label(GTK_BUTTON(jogZ), "JogZ");
    gtk_button_set_label(GTK_BUTTON(jogA), "JogA");
    jogWhat = 1;
}

void jogZ_Clicked(GtkButton * b, gpointer data)
{
    (void)b;
    (void)data;
    gtk_button_set_label(GTK_BUTTON(jogX), "JogX");
    gtk_button_set_label(GTK_BUTTON(jogY), "JogY");
    gtk_button_set_label(GTK_BUTTON(jogZ), "(JogZ)");
    gtk_button_set_label(GTK_BUTTON(jogA), "JogA");
    jogWhat = 2;
}

void jogA_Clicked(GtkButton * b, gpointer data)
{
    (void)b;
    (void)data;
    gtk_button_set_label(GTK_BUTTON(jogX), "JogX");
    gtk_button_set_label(GTK_BUTTON(jogY), "JogY");
    gtk_button_set_label(GTK_BUTTON(jogZ), "JogZ");
    gtk_button_set_label(GTK_BUTTON(jogA), "(JogA)");
    jogWhat = 3;
}

double	jogMult = 0.01;

void jog1_Clicked(GtkButton * b, gpointer data)
{
    (void)b;
    (void)data;
    gtk_button_set_label(GTK_BUTTON(jog1), "(Jog 1.0)");
    gtk_button_set_label(GTK_BUTTON(jogdot1), "Jog 0.1");
    gtk_button_set_label(GTK_BUTTON(jogdot01), "Jog 0.01");
    gtk_button_set_label(GTK_BUTTON(jogdot001), "Jog 0.001");
    jogMult = 1.0;
}
void jogdot1_Clicked(GtkButton * b, gpointer data)
{
    (void)b;
    (void)data;
    gtk_button_set_label(GTK_BUTTON(jog1), "Jog 1.0");
    gtk_button_set_label(GTK_BUTTON(jogdot1), "(Jog 0.1)");
    gtk_button_set_label(GTK_BUTTON(jogdot01), "Jog 0.01");
    gtk_button_set_label(GTK_BUTTON(jogdot001), "Jog 0.001");
    jogMult = 0.1;
}
void jogdot01_Clicked(GtkButton * b, gpointer data)
{
    (void)b;
    (void)data;
    gtk_button_set_label(GTK_BUTTON(jog1), "Jog 1.0");
    gtk_button_set_label(GTK_BUTTON(jogdot1), "Jog 0.1");
    gtk_button_set_label(GTK_BUTTON(jogdot01), "(Jog 0.01)");
    gtk_button_set_label(GTK_BUTTON(jogdot001), "Jog 0.001");
    jogMult = 0.01;
}
void jogdot001_Clicked(GtkButton * b, gpointer data)
{
    (void)b;
    (void)data;
    gtk_button_set_label(GTK_BUTTON(jog1), "Jog 1.0");
    gtk_button_set_label(GTK_BUTTON(jogdot1), "Jog 0.1");
    gtk_button_set_label(GTK_BUTTON(jogdot01), "Jog 0.01");
    gtk_button_set_label(GTK_BUTTON(jogdot001), "(Jog 0.001)");
    jogMult = 0.001;
}

struct encoder *encoder;

void *update_window (void *ptr)
{
    int lastval, newval;
    char temp[15];


    lastval = encoder->value / 4;
    while (1)
    {
	usleep(1000);
	
	// if we have new dro values, display them

	// if we are jogging and the jog wheel moved make the update
	newval = encoder->value / 4;
	if (lastval != newval)
	{
	    switch (jogWhat)
	    {
	    case -1:
		break;
	    case 0:
		dX += ((lastval - newval) * jogMult);
		sprintf(temp, "%07.3f", dX);
		gdk_threads_enter();
		gtk_label_set (GTK_LABEL (droX), temp);
		gdk_threads_leave();
		break;

	    case 1:
		dY += ((lastval - newval) * jogMult);
		sprintf(temp, "%07.3f", dY);
		gdk_threads_enter();
		gtk_label_set (GTK_LABEL (droY), temp);
		gdk_threads_leave();
		break;

	    case 2:
		dZ += ((lastval - newval) * jogMult);
		sprintf(temp, "%07.3f", dZ);
		gdk_threads_enter();
		gtk_label_set (GTK_LABEL (droZ), temp);
		gdk_threads_leave();
		break;

	    case 3:
		dA += ((lastval - newval) * jogMult);
		sprintf(temp, "%07.3f", dA);
		gdk_threads_enter();
		gtk_label_set (GTK_LABEL (droA), temp);
		gdk_threads_leave();
		break;
	    }
	    lastval = newval;
	}
        
    }

}


int
set_interface_attribs (int fd)
{
        struct termios tty;
        memset (&tty, 0, sizeof tty);
        if (tcgetattr (fd, &tty) != 0)
        {
//                error_message ("error %d from tcgetattr", errno);
                return -1;
        }

        cfsetospeed (&tty, 115200);
        cfsetispeed (&tty, 115200);

        tty.c_cflag = (tty.c_cflag & ~CSIZE) | CS8;     // 8-bit chars
        // disable IGNBRK for mismatched speed tests; otherwise receive break
        // as \000 chars
        tty.c_iflag &= ~IGNBRK;         // disable break processing
        tty.c_lflag = 0;                // no signaling chars, no echo,
                                        // no canonical processing
        tty.c_oflag = 0;                // no remapping, no delays
        tty.c_cc[VMIN]  = 0;            // read doesn't block
        tty.c_cc[VTIME] = 5;            // 0.5 seconds read timeout

        tty.c_iflag &= ~(IXON | IXOFF | IXANY); // shut off xon/xoff ctrl

        tty.c_cflag |= (CLOCAL | CREAD);// ignore modem controls,
                                        // enable reading
        tty.c_cflag &= ~(PARENB | PARODD);      // shut off parity
        tty.c_cflag &= ~CSTOPB;
        tty.c_cflag &= ~CRTSCTS;

        if (tcsetattr (fd, TCSANOW, &tty) != 0)
        {
//                error_message ("error %d from tcsetattr", errno);
                return -1;
        }
        return 0;
}

void
set_blocking (int fd, int should_block)
{
        struct termios tty;
        memset (&tty, 0, sizeof tty);
        if (tcgetattr (fd, &tty) != 0)
        {
//                error_message ("error %d from tggetattr", errno);
                return;
        }

        tty.c_cc[VMIN]  = should_block ? 1 : 0;
        tty.c_cc[VTIME] = 5;            // 0.5 seconds read timeout

        if (tcsetattr (fd, TCSANOW, &tty) != 0)
        {
//            error_message ("error %d setting term attributes", errno);
        }
}

void readLine (int fd, char *line)
{

}

void parseLine (char *line, double *X, double *Y, double *Z, double *A)
{
    char *token, *value;


    // look for posx, y, z, a tags and parse the value
    token = strstr(line, "posx");
    if (NULL != token)
    {
	value = token + 5;
	sprintf(value, "%f", X);
    }    
    token = strstr(line, "posy");
    if (NULL != token)
    {
	value = token + 5;
	sprintf(value, "%f", Y);
    }    
    token = strstr(line, "posz");
    if (NULL != token)
    {
	value = token + 5;
	sprintf(value, "%f", Z);
    }    
    token = strstr(line, "posa");
    if (NULL != token)
    {
	value = token + 5;
	sprintf(value, "%f", A);
    }
}


void *update_serial (void *ptr)
{
    char *portname = "/dev/ttyUSB0";
    char templine[200], temp[15];
    double	tX, tY, tZ, tA;


    // outer connect loop
    // open the serial interface and configure

    while (1)
    {
        usleep(100000);
        int fd = open (portname, O_RDWR | O_NOCTTY | O_SYNC);
        if (fd < 0)
        {
            printf ("error %d opening %s: %s", errno, portname, strerror (errno));
            break; // keep trying to connect
        }

        set_interface_attribs (fd);  // set speed to 115,200 bps, 8n1 (no parity)
        set_blocking (fd, 1);                // set blocking

        // inner read loop
        while (1)
        {
	    usleep(1000);
	    
	    // read till end of line
	    readLine(fd, templine);

	    // parse values line:0,posx:0.016,posy:0.000,posz:-7.000,posa:3.000,feed:0.000,vel:61.987,unit:1,coor:1,dist:0,frmo:0,momo:0,stat:5
	    parseLine (templine, &tX, &tY, &tZ, &tA);

	    // update fields 
	    if (tX != dX)
	    {
		dX = tX;
		sprintf(temp, "%07.3f", dX);
		gdk_threads_enter();
		gtk_label_set (GTK_LABEL (droX), temp);
		gdk_threads_leave();
	    }
	}
    }
}


int main (int argc, char **argv)
{
    GtkBuilder *builder;
    GtkWidget  *window;
    GError     *error = NULL;
    static pthread_t	win_update;
    static pthread_t	ser_update;

    PangoFontDescription *dfl;
	

    // create font specifiers
    dfl = pango_font_description_from_string ("Monospace");
    pango_font_description_set_size (dfl, 25*PANGO_SCALE);

    /* Init GTK+ */
    gdk_threads_init();
    gtk_init( &argc, &argv );

    // init the i/o and setup the jog wheel
    int res = wiringPiSetupGpio(); // Initialize wiringPi -- using Broadcom pin numbers
    encoder = setupencoder(23, 24);

    /* Create new GtkBuilder object */
    builder = gtk_builder_new();
    /* Load UI from file. If error occurs, report it and quit application.
     * Replace "tut.glade" with your saved project. */
    if( ! gtk_builder_add_from_file( builder, "cnc-head.glade", &error ) )
    {
        g_warning( "%s", error->message );
        g_free( error );
        return( 1 );
    }

    /* Get main window pointer from UI */
    window = GTK_WIDGET( gtk_builder_get_object( builder, "window1" ) );

    // get dro label handles
    droX = GTK_WIDGET( gtk_builder_get_object( builder, "droX" ) );
    gtk_widget_modify_font(droX, dfl);  
    jogX = GTK_WIDGET( gtk_builder_get_object( builder, "JogX" ) );
    droY = GTK_WIDGET( gtk_builder_get_object( builder, "droY" ) );
    gtk_widget_modify_font(droY, dfl);  
    jogY = GTK_WIDGET( gtk_builder_get_object( builder, "JogY" ) );
    droZ = GTK_WIDGET( gtk_builder_get_object( builder, "droZ" ) );
    gtk_widget_modify_font(droZ, dfl);  
    jogZ = GTK_WIDGET( gtk_builder_get_object( builder, "JogZ" ) );
    droA = GTK_WIDGET( gtk_builder_get_object( builder, "droA" ) );
    gtk_widget_modify_font(droA, dfl);  
    jogA = GTK_WIDGET( gtk_builder_get_object( builder, "JogA" ) );
    
    // get jog size handles
    jog1 = GTK_WIDGET( gtk_builder_get_object( builder, "jog1" ) );
    jogdot1 = GTK_WIDGET( gtk_builder_get_object( builder, "jogdot1" ) );
    jogdot01 = GTK_WIDGET( gtk_builder_get_object( builder, "jogdot01" ) );
    jogdot001 = GTK_WIDGET( gtk_builder_get_object( builder, "jogdot001" ) );

    // get zero command handles
    zeroX = GTK_WIDGET( gtk_builder_get_object( builder, "ZeroX" ) );
    zeroY = GTK_WIDGET( gtk_builder_get_object( builder, "ZeroY" ) );
    zeroZ = GTK_WIDGET( gtk_builder_get_object( builder, "ZeroZ" ) );
    zeroA = GTK_WIDGET( gtk_builder_get_object( builder, "ZeroA" ) );

    /* Connect signals */
    gtk_signal_connect (GTK_OBJECT(jogX), "clicked", GTK_SIGNAL_FUNC(jogX_Clicked), NULL);
    gtk_signal_connect (GTK_OBJECT(jogY), "clicked", GTK_SIGNAL_FUNC(jogY_Clicked), NULL);
    gtk_signal_connect (GTK_OBJECT(jogZ), "clicked", GTK_SIGNAL_FUNC(jogZ_Clicked), NULL);
    gtk_signal_connect (GTK_OBJECT(jogA), "clicked", GTK_SIGNAL_FUNC(jogA_Clicked), NULL);

    gtk_signal_connect (GTK_OBJECT(zeroX), "clicked", GTK_SIGNAL_FUNC(zeroX_Clicked), NULL);
    gtk_signal_connect (GTK_OBJECT(zeroY), "clicked", GTK_SIGNAL_FUNC(zeroY_Clicked), NULL);
    gtk_signal_connect (GTK_OBJECT(zeroZ), "clicked", GTK_SIGNAL_FUNC(zeroZ_Clicked), NULL);
    gtk_signal_connect (GTK_OBJECT(zeroA), "clicked", GTK_SIGNAL_FUNC(zeroA_Clicked), NULL);

    gtk_signal_connect (GTK_OBJECT(jog1), "clicked", GTK_SIGNAL_FUNC(jog1_Clicked), NULL);
    gtk_signal_connect (GTK_OBJECT(jogdot1), "clicked", GTK_SIGNAL_FUNC(jogdot1_Clicked), NULL);
    gtk_signal_connect (GTK_OBJECT(jogdot01), "clicked", GTK_SIGNAL_FUNC(jogdot01_Clicked), NULL);
    gtk_signal_connect (GTK_OBJECT(jogdot001), "clicked", GTK_SIGNAL_FUNC(jogdot001_Clicked), NULL);

    /* Destroy builder, since we don't need it anymore */
    g_object_unref( G_OBJECT( builder ) );

    /* Show window. All other widgets are automatically shown by GtkBuilder */
    gtk_widget_show( window );

    // connect the quad interface
    // start an update thread
    pthread_create (&win_update, NULL, update_window, NULL);

    // locate and connect the usb-serial interface to the tinyg
    pthread_create (&ser_update, NULL, update_serial, NULL);

    /* Start main loop */
    gdk_threads_enter();
    gtk_main();
    gdk_threads_leave();

    return( 0 );
}
