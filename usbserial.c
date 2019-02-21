#include <gtk/gtk.h>

#include <errno.h>
#include <termios.h>
#include <unistd.h>
#include <memory.h>
#include <fcntl.h>
#include <sys/stat.h>
#include <string.h>

extern void updatecommstat (int commOn);
extern void updateDro (float tX, float tY, float tZ, float tA, float tF, float tV, int bitmap);
extern void updatePlanq (float pQ);

float currX, currY, currZ, currA;

void getCurr(float *cX, float *cY, float *cZ, float *cA)
{
	*cX = currX;
	*cY = currY;
	*cZ = currZ;
	*cA = currA;
}

int set_interface_attribs (int fd)
{
	struct termios tty;
	memset (&tty, 0, sizeof tty);
	if (tcgetattr (fd, &tty) != 0)
	{
		return -1;
	}

	cfsetospeed (&tty, B115200);
	cfsetispeed (&tty, B115200);

	tty.c_cflag = (tty.c_cflag & ~CSIZE) | CS8;     // 8-bit chars
	// disable IGNBRK for mismatched speed tests; otherwise receive break
	// as \000 chars
	tty.c_iflag &= ~IGNBRK;         // disable break processing
	tty.c_lflag = 0;                // no signaling chars, no echo,
									// no canonical processing
	tty.c_oflag = 0;                // no remapping, no delays
	tty.c_cc[VMIN]  = 1;            // read does block
	tty.c_cc[VTIME] = 5;            // 0.5 seconds read timeout

	tty.c_iflag &= ~(IXON | IXOFF | IXANY); // shut off xon/xoff ctrl

	tty.c_cflag |= (CLOCAL | CREAD);// ignore modem controls,
									// enable reading
	tty.c_cflag &= ~(PARENB | PARODD);      // shut off parity
	tty.c_cflag &= ~CSTOPB;
	tty.c_cflag |= CRTSCTS;			// previously made sure that the tinyg
									// is configured for this

	if (tcsetattr (fd, TCSANOW, &tty) != 0)
	{
		return -1;
	}

    return 0;
}

void set_blocking (int fd, int should_block)
{
	struct termios tty;
	memset (&tty, 0, sizeof tty);
	if (tcgetattr (fd, &tty) != 0)
	{
		return;
	}

	tty.c_cc[VMIN]  = should_block ? 1 : 0;
	tty.c_cc[VTIME] = 0;            // no timeout
//	tty.c_cc[VTIME] = 5;            // 0.5 seconds read timeout

	tcsetattr (fd, TCSANOW, &tty);
}

int readLine (int fd, char *line)
{
	int n, i = 0, rval = 456;
	char temp[200];

	memset (line, 0, 200);

	// collect an entire line reading 1 char at a time..
	while (1)
	{
		n = read (fd, temp, 1);
		if (n <= 0)
		{
			// error out, this probably means we have lost connection
			rval = -1;
			break;
		}
		if (n != 1) usleep(100);
		else
		{
			*line = *temp;
			if (*line == '\n' || *line == '\r')
			{
				// we have an entire line
				rval = i;
				break;
			}
			line++;
			i++;
			if (i > 200)
			{
				// buffer overrun
				rval = 237;
printf("serial line overrun\n");
				break;
			}
		}
	}
	return rval;
}

char templine[200];
int tokenRead(char *line, char *token, float *val, int bump)
{
  char *value, *token2;
	char *vt;
	int rv = 0;		// default is token not found


	// temp copy so we can null the rest of the line for better scanf
	strncpy(templine, line, 199);
	templine[199] = 0;
	token2 = NULL;
  token2 = strstr(templine, token);
  if (NULL != token2)
    {
		value = token2 + bump;	vt = index(value, ',');	if (vt) *vt = 0;
		sscanf(value, "%f", val);
		rv = 1;
    }
	return rv;
}

float qr = 32, qi = 0, qo = 0;
float GCLineno;
void parseLine (char *line, float *X, float *Y, float *Z, float *A, float *F, float *V, float *S, int *bitmap)
{
	*bitmap = 0;
//printf("parse >%s<\n", line);
    // look for posx, y, z, a tags and parse the value
		if (tokenRead (line, "posx", X, 5)) *bitmap |= 1;
		if (tokenRead (line, "posy", Y, 5)) *bitmap |= 2;
		if (tokenRead (line, "posz", Z, 5)) *bitmap |= 4;
		if (tokenRead (line, "posa", A, 5)) *bitmap |= 8;

		if (tokenRead (line, "\"posx\"", X, 7)) *bitmap |= 1;
		if (tokenRead (line, "\"posy\"", Y, 7)) *bitmap |= 2;
		if (tokenRead (line, "\"posz\"", Z, 7)) *bitmap |= 4;
		if (tokenRead (line, "\"posa\"", A, 7)) *bitmap |= 8;

	// also look for line, feed and vel
	if (tokenRead (line, "feed", F, 5)) *bitmap |= 16;
	if (tokenRead (line, "vel", V, 4)) *bitmap |= 32;

	if (tokenRead (line, "\"feed\"", F, 7)) *bitmap |= 16;
	if (tokenRead (line, "\"vel\"", V, 6)) *bitmap |= 32;

	// get the status of the planner queue
	tokenRead (line, "qr", &qr, 3);
	tokenRead (line, "qo", &qo, 3);
	tokenRead (line, "qi", &qi, 3);

	tokenRead (line, "\"qr\"", &qr, 5);
	tokenRead (line, "\"qo\"", &qo, 5);
	tokenRead (line, "\"qi\"", &qi, 5);

//printf("queue >%f<\n", qr);

	// get the line number
	tokenRead (line, "line", &GCLineno, 5);

	// get the status mask
	if (tokenRead (line, "stat", S, 5)) *bitmap |= 64;
}

int getPQ (void)
{
	return (int)qr;
}

int getLineno (void)
{
	return (int)GCLineno;
}

void queueState(int *qrp, int *qip, int *qop)
{
	*qrp = qr;
	*qip = qi;
	*qop = qo;
}

int fd;

void sersend(char *buf, int len)
{
	write (fd, buf, len);
}

void sersendz(char *buf)
{
	write (fd, buf, strlen(buf));
}

int sersendNT(char *buf)
{
	int rv;

	// write might fail if the tinyg planning buffer is full
	rv = write (fd, buf, strlen(buf));

	return rv;
}

void updateLevel (float tZ, float tV, float tS, int bitmap);
void updateZProbe (float tZ, float tV, float tS, int bitmap);

int holdOff = 0;
int isHoldoff(void)
{
	return holdOff;
}

void *update_serial (void *ptr)
{
    char templine[200], temp[35];
    static float	tX, tY, tZ, tA, tF, tV, tS;
		int i, rlen, bitmap;


    // outer connect loop
    // open the serial interface and configure

	updatecommstat(0);
    while (1)
    {
		for (i = 0; i<100; i++)
		{
			usleep(100);
			sprintf(temp, "/dev/ttyUSB%d", i);
			fd = open (temp, O_RDWR | O_NOCTTY | O_SYNC);
			if (fd > 0)
			{
				break; // got a connection
			}
		}
		if (i == 100)
		{
			updatecommstat(0);
			usleep(100000);
        }
		else
		{
			updatecommstat(1);

			set_interface_attribs (fd);  // set speed to 115,200 bps, 8n1 (no parity)
			set_blocking (fd, 1);                // set blocking
			usleep(100000);
			write(fd, "$\n", 2);

			// config updates
			sersendNT("$qv=1\n");	// get queue state change reports
			sersendNT("$ex=2\n");	// use rts/cts control

			// safety code
			char *safe1 = "G21 G17\n";	// (G20 inch/G21 Metric, XY plane)
			char *safe2 = "G64 G80\n";	// (normal cutting mode, cancel canned cycles)
			char *safe3 = "G90 G94 G54 M5 M9\n";		// (absolute mode, feed per minute, coord system 1, spindle and coolant off)
			write (fd, "!%\n", 3);			// make sure the queue is empty
			write(fd, safe1, strlen(safe1));
			write(fd, safe2, strlen(safe2));
			write(fd, safe3, strlen(safe3));

			// inner read loop
			int retry = 0;
			while (1)
			{

				// read till end of line (with retries)
				while(1)
				{
					rlen = readLine(fd, templine);
					if(rlen > 0)
					{
						holdOff = 0;
						retry = 0;
						break;
					}
					else
					{
						if(retry++ < 20)
						{
							holdOff = 1;
							usleep(1000);
						}
						else
						{
							break;
						}
					}
				}
				if (-1 == rlen)
				{
					// comm disconnected, probably should kill a gcode play in gtk_progress_set_value
					Tr_Stop();

					updatecommstat(0);
					close (fd);
					break;
				}

				// parse values line:0,posx:0.016,posy:0.000,posz:-7.000,posa:3.000,feed:0.000,vel:61.987,unit:1,coor:1,dist:0,frmo:0,momo:0,stat:5
				tX = currX; tY = currY; tZ = currZ; tA = currA;
				parseLine (templine, &tX, &tY, &tZ, &tA, &tF, &tV, &tS, &bitmap);
				currX = tX; currY = tY; currZ = tZ; currA = tA;

				updateDro (tX, tY, tZ, tA, tF, tV, bitmap);

				updatePlanq (qr);

				if (isLevel())
				{
					// we are calibrating, update the calibration state
					updateLevel (tZ, tV, tS, bitmap);
				}
				if (isZProbe())
				{
					// we are calibrating, update the calibration state
					updateZProbe (tZ, tV, tS, bitmap);
				}
			}
		}
    }
}

void serUpdate (void)
{
    static pthread_t	ser_update;

    pthread_create (&ser_update, NULL, update_serial, NULL);
}
