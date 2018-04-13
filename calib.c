///////////////////////////////////////////////////////////////
//
//	calib.c
//		calibration sequence execution
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

// sub-states to complete the calibration sequence
#define SSTATECALIB0	0	// auto calibrating idle
#define SSTATECALIB1	1	// auto calibrating level upper right
#define SSTATECALIB2	2	// auto calibrating level upper right
#define SSTATECALIB3	3	// auto calibrating level lower left
#define SSTATECALIB4	4	// auto calibrating x left
#define SSTATECALIB5	5	// auto calibrating x right
#define SSTATECALIB6	6	// auto calibrating y upper
#define SSTATECALIB7	7	// auto calibrating y lower
#define SSTATECALIB8	8	// auto calibrating y right
#define SSTATECALIB9	9	// auto calibrating x lower

int calib_substate = 0;

float cX, cY, cZ, cV, cS;
float zCalUL;

void updateCalib (float X, float Y, float Z, float V, float S, int bitmap)
{
	cX = X;
	cY = Y;
	cZ = Z;
	cV = V;
	cS = S;
	
}

int  wait_and_fetch_calib (float *x, float *y, float *z)
{
	int rv = -1;
	int i;
	
	
	// probe is under way, update dro display as we wait for probe to stop
	// stop is indicated by seeing "stat:3" and-or "vel:0.0" or probe distance reached..
	cV = -1.0; cS = 0;
	cX = cY = cZ = 0.0;
	for(i=0; i<3000; i++)	// time out after 30 seconds
	{
		usleep(10000);
		
		// wait for completion
		if (cV == 0.0 || cS == 3)
		{
			// done
			*x = cX; *y = cY; *z = cZ;
			rv = 0;
			break;
		}
	}
	return rv;
}

/////////////////////////////////////////////////////////////////////
//
// issue_next_calib_command(void)
//
//	based on the current sub-state of the calibration sequence issue the next calibration command
//
int issue_next_calib_command(void)
{
	int rv = -1;
	float x, y, z;
	
	
	switch (calib_substate)
	{
	case SSTATECALIB0:	
		// not started yet assume we have been positioned over the top of the upper left corner of the 123 block
		// z down to contact or fail
		// G21 G91 
		// G38.2 F100 Z-70
		// read z value and store in calib variable
		rv = wait_and_fetch_calib (&x, &y, &z);
		if (rv == 0 && z != -70)
		{
			// got a good value
			// set to next sub-state
			zCalUL = z;
			rv = 0;		// command succeeded
			calib_substate = SSTATECALIB1;
		}
		else
		{
			// cal failed
			zCalUL = 0.0;
			calib_substate = SSTATECALIB0;
		}
		break;
		
	case SSTATECALIB1:	
		// go to upper right and probe z
		// z down to contact or fail
		// G21 G91 
		// G0 Z0 Y70
		// G38.2 F100 Z-70
		// read z value and store in calib variable
		// set to next sub-state
		rv = 0;		// command succeeded
		calib_substate = SSTATECALIB2;
		break;
		
	case SSTATECALIB2:
		// go to lower right and probe z
		// z down to contact or fail
		// G21 G91 
		// G0 Z0 x44.45
		// G38.2 F100 Z-70
		// read z value and store in calib variable
		// set to next sub-state
		rv = 0;		// command succeeded
		calib_substate = SSTATECALIB3;
		break;
		
	case SSTATECALIB3:
		// go to upper left lower z and probe x
		// G21 G91 
		// G0 Z0 
		// G0 X-10
		// G0 Y0
		// G0 Z-10
		// G38.2 F100 X70
		// read x value and store in calib variable
		// set to next sub-state
		rv = 0;		// command succeeded
		calib_substate = SSTATECALIB4;
		break;
		
	case SSTATECALIB4:
		// go to upper right and probe x
		// G21 G91 
		// G0 X-10
		// G0 Y70
		// G38.2 F100 X70
		// read x value and store in calib variable
		// set to next sub-state
		rv = 0;		// command succeeded
		calib_substate = SSTATECALIB5;
		break;
		
	case SSTATECALIB5:
		// go to upper left and probe y
		// G21 G91 
		// G0 Z0
		// G0 X10
		// G0 Y10
		// G0 Z-10
		// G38.2 F100 Y70
		// read y value and store in calib variable
		// set to next sub-state
		rv = 0;		// command succeeded
		calib_substate = SSTATECALIB6;
		break;
		
	case SSTATECALIB6:
		// go to upper left and probe y
		// G21 G91 
		// G0 Y-10
		// G0 X44
		// G38.2 F100 Y70
		// read y value and store in calib variable
		// set to next sub-state
		rv = 0;		// command succeeded
		calib_substate = SSTATECALIB7;
		break;
		
	case SSTATECALIB7:
		// go to lower left and probe y
		// G21 G91 
		// G0 Y-10
		// G0 X44
		// G38.2 F100 Y70
		// read y value and store in calib variable
		// set to next sub-state
		rv = 0;		// command succeeded
		calib_substate = SSTATECALIB8;
		break;
		
	case SSTATECALIB8:
		// go to lower left and probe x backward
		// G21 G91 
		// G0 Y-10
		// G0 X70
		// G0 Y0
		// G38.2 F100 X40
		// read y value and store in calib variable
		// set to next sub-state
		rv = 0;		// command succeeded
		calib_substate = SSTATECALIB9;
		break;
		
	case SSTATECALIB9:
		// go to lower left and probe x backward
		// G21 G91 
		// G0 Z-10
		// G0 Y75
		// G0 X0
		// G38.2 F100 Y50
		// read y value and store in calib variable
		// set to next sub-state
		rv = 0;		// command succeeded
		calib_substate = SSTATECALIB0;	// done
		break;
	}
}
	