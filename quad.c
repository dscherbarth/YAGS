#include <wiringPi.h>

#include <errno.h>
#include <termios.h>
#include <unistd.h>
#include <memory.h>

#include <stdio.h>
#include <fcntl.h>
#include <sys/stat.h>
#include <mqueue.h>

#include <time.h>

/*
  Should be run for every rotary encoder you want to control
  Returns a pointer to the new rotary encoder structer
  The pointer will be NULL is the function failed for any reason
*/
void setupencoder(void);
double getJM(void);
int getJW(void);

mqd_t	mq = 0;
typedef struct je
{
	char axis[2];
	float magnitude;
	
} jog_ele;

void sersendz(char *buf);

#define MINPLANNERJOG 4
// jog queue handling thread
void *jog_queue (void *ptr)
{
	jog_ele	je;
    char temp[65];
	int rc;
	
	while (1)
	{
		if (mq)
		{
			
			rc = mq_receive(mq, (char *)&je, sizeof(jog_ele), NULL);
			if (0 != rc )
			{
				// we have a jog entry, wait until the tinyg can take it
				while (1)
				{
					if (getPQ() > MINPLANNERJOG)
					{
						sprintf(temp, "g91 g0 %s%f\n", je.axis, je.magnitude);
						sersendz(temp);
						sersendz("g90\n");
						break;
					}
					else
					{
						usleep(5000);	// hope this doesn't get stuck..
					}
				}
			}
			else
			{
				usleep(5000);	// really shouldn't happen
			}
		}
		else
		{
			usleep(5000);	// might be that no entry has been made yet so we are waiting for the queue to be created
		}
	}
}

void jog(char *what, float howmuch)
{
    char temp[65];
	jog_ele	je;
	int rc;
	

	// create and open the queue if it doesn't exist
	if (mq == 0)
	{
		// create the queue for jogging
		struct mq_attr	attr;
		
		attr.mq_flags = 0;
		attr.mq_maxmsg = 500;
		attr.mq_msgsize = sizeof(jog_ele);
		attr.mq_curmsgs = 0;
		
		mq = mq_open("/jogqueue", O_CREAT | O_RDWR, 0777, &attr);
	}

	// make an entry in the jog queue
	if(isIdle() || isFreeh() || isToolch())
	{
		strcpy (je.axis, what);
		je.magnitude = howmuch;
		rc = mq_send (mq, (char *)&je, sizeof(jog_ele), 0);
	}
}

int value = 0;

// thread to check for quadrature value changes
void *jog_quad (void *ptr)
{
    int lastval, newval;
	float tJ = 0.0;
	

	lastval = value / 4;
	while (1)
	{
		usleep(10000);
		
		// if we are jogging and the jog wheel moved make the update
		newval = value / 4;
		if (lastval != newval)
		{
			// make a delta 'jog' move value
			tJ = ((float)(lastval - newval) * getJM());
			lastval = newval;
			switch (getJW())
			{
			case -1:
				break;
			case 0:
				// send it as a relative move
				jog("x", tJ);
				break;

			case 1:
				// send it as a relative move
				jog("y", tJ);
				break;

			case 2:
				// send it as a relative move
				jog("z", tJ);
				break;

			case 3:
				// send it as a relative move
				jog("a", tJ);
				break;
			}
		}
    }
}

void quadInit(void)
{
    static pthread_t	win_update;


	wiringPiSetupGpio(); // Initialize wiringPi -- using Broadcom pin numbers
    setupencoder();

	// kick off the jog handling thread
	// create threads to handle the jog wheel, it's queue processor and the sending of gcode files
	pthread_create (&win_update, NULL, jog_quad, NULL);
	pthread_create (&win_update, NULL, jog_queue, NULL);
}

const signed short table[] = {0,-1,1,0,1,0,0,-1,-1,0,0,1,0,1,-1,0};
//const signed short table[] = {-2,-1,1,2,1,-2,2,-1,-1,2,-2,1,2,1,-1,-2};
int lastEncoded = 0;


// isr for pin value changes
void updateEncoders()
{
	int MSB = digitalRead(23);
	int LSB = digitalRead(24);
	int encoded = (MSB << 1) | LSB;
	int sum = (lastEncoded << 2) | encoded;
	lastEncoded = encoded;

	value += table[sum];
}

void setupencoder(void)
{

    pinMode(23, INPUT);
    pinMode(24, INPUT);
    pullUpDnControl(23, PUD_UP);
    pullUpDnControl(24, PUD_UP);
    wiringPiISR(23,INT_EDGE_BOTH, updateEncoders);
    wiringPiISR(24,INT_EDGE_BOTH, updateEncoders);
}

