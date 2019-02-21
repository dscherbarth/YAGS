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
//	int ticks;

} jog_ele;

void sersendz(char *buf);

int last_time = 0;
//int delta = 0;
int tick = 0;
#define MINPLANNERJOG 4
// jog queue handling thread
void *jog_queue (void *ptr)
{
  struct mq_attr attr;
	jog_ele	je;
	jog_ele	je_tot;
  char temp[65];
	int rc, i;
	static int g91flag = 0;
	float jogfeed = 0.0;


	while (1)
	{
		if (mq)
		{
			// find out how many entries there are right now
			mq_getattr(mq, &attr);
			je_tot.axis[0] = 0; je_tot.magnitude = 0.0;
			for(i=0; i<attr.mq_curmsgs; i++)
			{
        rc = mq_receive(mq, (char *)&je, sizeof(jog_ele), NULL);
				if (0 != rc )
				{
        	je_tot.axis[0] = je.axis[0]; je_tot.axis[1] = je.axis[1]; je_tot.magnitude += je.magnitude;
//					delta = je_tot.ticks - last_time;
//					last_time = je_tot.ticks;
				}
				else
				{
					break;
				}
			}
			// we have a jog entry, wait until the tinyg can take it
			while (1)
			{
				if(getPQ() == 32 && (tick -last_time) > 100 && g91flag)
				{
					sersendz("g90\n");
					g91flag = 0;
				}
				if (je_tot.axis[0] == 0 || je_tot.magnitude == 0.0)
				{
					break;	// nothing to do
				}
				if (getPQ() > MINPLANNERJOG)
				{
					if (tick - last_time > 21)
					{
							jogfeed = 1000;
					}
					else
					{
						jogfeed = (51 - (tick - last_time))*100.0;
						if(jogfeed < 100) jogfeed = 100;
						if(jogfeed > 5000) jogfeed = 5000;
					}
					if(g91flag && jogfeed == 100 )
					{
						sersendz("!%\n");	// stopped
					}
					sprintf(temp, "g91 g1 f%f %s%f\n", jogfeed, je_tot.axis, je_tot.magnitude);
					sersendz(temp);
					last_time = tick;
					g91flag = 1;
					break;
				}
				else
				{
					usleep(5000);	// give tinyg some time to work
				}
			}
			usleep(50000);	// give the queue a chance to fill some
		}
		else
		{
			usleep(5000);	// might be that no entry has been made yet so we are waiting for the queue to be created
		}
	}
}
void setFeedFactor (float ff);
float getFeedFactor (void);

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
//		je.ticks = ticks;
		rc = mq_send (mq, (char *)&je, sizeof(jog_ele), 0);
	}
	else if(isGCPlay() && what[0] == 'f')
	{
		// jogging the feed rate no need to queue, just update the factor and limit the range
		setFeedFactor(getFeedFactor() + howmuch);
		if(getFeedFactor() > 10.0) setFeedFactor(10.0);
		if(getFeedFactor() < 0.1) setFeedFactor(0.1);
	}
}

int value = 0;
//int tick = 0;

// thread to check for quadrature value changes
void *jog_quad (void *ptr)
{
    int lastval, newval;
	float tJ = 0.0;


	lastval = value / 4;
	while (1)
	{
		usleep(10000);	// 100 times/second
		tick++;

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

			case 4:
				// 'jogging' the feed rate
				jog("f", tJ);
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
	int MSB = digitalRead(24);
	int LSB = digitalRead(23);
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
