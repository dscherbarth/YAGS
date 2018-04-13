//
// Level.c
//
// Execute leveling by probing to build a 2d z level array
//
#include <gtk/gtk.h>

#include <wiringPi.h>

#include <errno.h>
#include <termios.h>
#include <unistd.h>
#include <stdio.h>
#include <memory.h>
#include <malloc.h>
#include <fcntl.h>
#include <math.h>
#include <sys/stat.h>

#define XLEVMAX	500
#define YLEVMAX	250

float levarray[XLEVMAX][YLEVMAX];	// uses 20K
float levXpos, levOldXpos = 0.0;				// ongoing pos
float levYpos, levOldYpos = 0.0;				// for z-warp adjustment
float levZpos, levOldZpos = 0.0;				// 

float levXstart, levYstart, levXend, levYend, levStep;
float levUpper, levLower, levFeed;

// levInit initialize the level array and read config array
//
void levConfWrite( float xs, float ys, float xe, float ye)
{
	FILE *config = NULL;

	config = fopen ("level.conf", "w");

	fprintf(config, "Xstart=%f\n", xs);
	fprintf(config, "Ystart=%f\n", ys);
	fprintf(config, "Xend=%f\n", xe);
	fprintf(config, "Yend=%f\n", ye);
	fprintf(config, "Step=5.0\n");
	fprintf(config, "Upper=1.0\n");
	fprintf(config, "Lower=-1.0\n");
	fprintf(config, "Feed=25.0\n");
}

void levInit()
{
	FILE *config = NULL;
	int ix, iy;
	
	// init the array to zeros
	for(ix=0; ix<XLEVMAX; ix++)
	{
		for(iy=0; iy<YLEVMAX; iy++)
		{
			levarray[ix][iy] = 0.0;
		}
	}
	
	// set defaults in case the file is not found
	levXstart = 0.0; levYstart = 0.0;
	levXend  = 25.0; levYend  = 25.0;
	levStep = 5.1;
	levUpper = 0.5;
	levLower = -1.0;
	levFeed = 27.0;
		
	// open and read the config file to get x,y dimensions and step in mm
	char *buf = NULL;
	int len = 0;
	config = fopen ("level.conf", "r");
	if (NULL != config)
	{
		char *xp = NULL;
		char *yp = NULL;

		// read dimensions and step increment
		while (getline(&buf, &len, config) != -1)
		{
			xp = strstr(buf, "Xstart"); if (xp) {xp += 7; sscanf (xp, "%f", &levXstart);}
			yp = strstr(buf, "Ystart"); if (yp) {yp += 7; sscanf (yp, "%f", &levYstart);}
			xp = yp = NULL;
			xp = strstr(buf, "Xend"); if (xp) {xp += 5; sscanf (xp, "%f", &levXend);}
			yp = strstr(buf, "Yend"); if (yp) {yp += 5; sscanf (yp, "%f", &levYend);}
			xp = NULL; xp = strstr(buf, "Step"); if (xp) {xp += 5; sscanf (xp, "%f", &levStep);}
			xp = NULL; xp = strstr(buf, "Upper"); if (xp) {xp += 6; sscanf (xp, "%f", &levUpper);}
			xp = NULL; xp = strstr(buf, "Lower"); if (xp) {xp += 6; sscanf (xp, "%f", &levLower);}
			xp = NULL; xp = strstr(buf, "Feed"); if (xp) {xp += 5; sscanf (xp, "%f", &levFeed);}
		}
		fclose(config);
	}

	// init the array with zeros
	for(ix=0; ix<XLEVMAX; ix++)
	{
		for(iy=0; iy<YLEVMAX; iy++)
		{
			levarray[ix][iy] = 0.0;
		}
	}
	
	// open and read the data file to get x,y dimensions and step in mm
	FILE *levDat = NULL;
	levDat = fopen ("level.dat", "r");
	if (NULL != levDat)
	{
		char *xp = NULL;
		char *yp = NULL;
		int levXsize, levYsize;
		
		// read sizes
		getline(&buf, &len, levDat);
		xp = strstr(buf, "Xsize"); if (xp) {xp += 6; sscanf (xp, "%i", &levXsize);}
		getline(&buf, &len, levDat);
		yp = strstr(buf, "Ysize"); if (yp) {yp += 6; sscanf (yp, "%i", &levYsize);}

		// limit as nec
		if (levXsize > XLEVMAX) levXsize = XLEVMAX;
		if (levYsize > YLEVMAX) levYsize = YLEVMAX;
		
		// read into the array
		int i, j;
		for (i=0; i<levXsize; i++)
		{
			for(j=0; j<levYsize; j++)
			{
				getline(&buf, &len, levDat);
				sscanf(buf, "%f", &levarray[i][j]);
			}
		}
		
		fclose(levDat);
	}
	if (buf != NULL) free(buf);
}

int levDone = 0;
int levPos = 0;
void levInitPos (void)
{
	// initialize the level probe position and step sizes
	levXpos = levXstart - levStep;
	levYpos = levYstart;
	levDone = 1;
	levPos = 1;
}

// returns 0 when done
int levStepPos (void)
{
	if(levPos)
	{
		// looking to get to probe position
		levPos = 0;

		// step to the next location return 0 when done
		levXpos += levStep;
		if (levXpos > levXend)
		{
			levXpos = levXstart;
			levYpos += levStep;
			if (levYpos > levYend)
			{
				return 0;	// done
			}
		}
	}
	else
	{
		levPos = 1;

	}
	return 1;	// done
}

int waitTimer = 0;
void levDoProbe (int counter)
{
	char temp[120];
	
	// issue the gcode position and probe commands and wait for done
	levDone = 0;	// indicate not done yet
	if(!levPos)
	{
		// just move to probe position
		sprintf (temp, "g0 x%f y%f z%f\n", levXpos, levYpos, levUpper);
		sersend (temp, strlen(temp));
	}
	else
	{
		sprintf (temp, "g38.2 z%f f%f\n", levLower, levFeed);
		sersend (temp, strlen(temp));
	}
	
	// start/restart the timeout timer
	waitTimer = counter;
}

float levZfinal = 0.0;

// updateLevel as the leveling operation proceeds
//
void updateLevel (float tZ, float tV, float tS, int bitmap)
{
printf ("updateLevel tv %f ts %f bm %x levelpos %d\n", tV, tS, bitmap, levPos);
	// until vel goes to zero or state goes to 3 update the leveling z value
	if (tS == 3.0 && (bitmap & 64))
	{
		if (levPos)
		{
			levarray[(int)(levXpos / levStep) - 1][(int)(levYpos / levStep)] = tZ;
			printf ("zval %f loaded to %d %d (%f %f %f\n", tZ, (int)(levXpos / levStep), (int)(levYpos / levStep), levXpos, levYpos, levStep);
		}
		levDone = 1;
	}
}

extern GtkWidget *droZ;
extern GtkWidget *ZProbe;

extern double 	dZ;

void updateZProbe (float tZ, float tV, float tS, int bitmap)
{
	if (tS == 3.0 && (bitmap & 64))
	{
		// end of z probe
		sersendz ("g28.3 z0\n");		// set the zero values
		sersendz ("g10 l2 p1 z0\n");		// set the zero values
		dZ = 0.0;
		gdk_threads_enter();
		gtk_label_set (GTK_LABEL (droZ), "000.000");
		gtk_button_set_label(GTK_BUTTON(ZProbe), "ZProbe");
		gdk_threads_leave();
		
		Tr_ZProbe();

	}
}

#define TIMEOUT	800	// ~5 seconds
int levTestDone(int counter)
{
	int rc = levDone;
	
	// check for timeout
	if(waitTimer != 0 && (counter - waitTimer > TIMEOUT))
	{
		rc = -1;
		waitTimer = 0;
	}
	return rc;
}

// levSave saves the collected dat ain a file
//
int levSave()
{
	int rc = 0;
	
	
	// got all of the data, write the level array data file
	FILE *levDat = NULL;
	levDat = fopen ("level.dat", "w");
	if (NULL != levDat)
	{

		// write sizes
		fprintf(levDat, "Xsize=%i\n", (int)((levXend - levXstart)/levStep) + 1);
		fprintf(levDat, "Ysize=%i\n", (int)((levYend - levYstart)/levStep) + 1);

		// write from the array
		int i, j;
		for (i=0; i<((int)((levXend - levXstart)/levStep) + 1); i++)
		{
			for(j=0; j<((int)((levYend - levYstart)/levStep) + 1); j++)
			{
				fprintf(levDat, "%f\n", levarray[i][j]);
			}
		}
		
		fclose(levDat);
	}
	
	return rc;
}

int Xsize, Ysize;
// interpolate based on x,y pos and return z delta
//   returns z adjustment for levelling
//
float interpolate(float x, float y)
{
	float rc = 0.0;
	int xindex, yindex;

	// simple first pass is to adjust based on just passed index
	// make sure that the x and y are within level adjusted space
	if((x <= levXend && x >= levXstart) && (y <= levYend && y >= levYstart))
	{
		// in range, find correct grid
		xindex = (int)((x - levXstart)/levStep);
		yindex = (int)((y - levYstart)/levStep);
		
		rc = levarray[xindex][yindex];
	}
	return rc;
}

// levApply take incoming g0 and g1,2,3 x,y,z and adjust
//
int levApply(char *code, char *newcode)
{
	int rc = 0;
	static float cmdX = 0.0;	// commanded x
	static float cmdY = 0.0;	// commanded y
	static float cmdZ = 0.0;	// commanded z
	static float cmdI = 0.0;	// commanded z
	static float cmdJ = 0.0;	// commanded z
	static float cmdK = 0.0;	// commanded z
	static float cmdR = 0.0;	// commanded z
	static float cmdF = 0.0;	// commanded feed
	static float adjZ = 0.0;
	char *gp = NULL;
	char *xp = NULL;
	char *yp = NULL;
	char *zp = NULL;
	char *ip = NULL;
	char *jp = NULL;
	char *kp = NULL;
	char *rp = NULL;
	char *fp = NULL;
	int gWhat = 0;

	
	// parse the gcode for g1, g2, g3
	if (strstr (code, "g1 ") || strstr (code, "G1 ")) {gWhat = 1; gp = "G1";}
 	if (strstr (code, "g2 ") || strstr (code, "G2 ")) {gWhat = 2; gp = "G2";}
	if (strstr (code, "g3 ") || strstr (code, "G3 ")) {gWhat = 3; gp = "G3";}

	// g0,1,2,3 get the xyz values
	xp = strstr(code, "X"); if (xp) {xp++; sscanf (xp, "%f", &cmdX);}
	yp = strstr(code, "Y"); if (yp) {yp++; sscanf (yp, "%f", &cmdY);}
	zp = strstr(code, "Z"); if (zp) {zp++; sscanf (zp, "%f", &cmdZ);}

	if (gWhat)
	{
		ip = strstr(code, "I"); if (ip) {ip++; sscanf (ip, "%f", &cmdI);}
		jp = strstr(code, "J"); if (jp) {jp++; sscanf (jp, "%f", &cmdJ);}
		kp = strstr(code, "K"); if (kp) {kp++; sscanf (kp, "%f", &cmdK);}
		rp = strstr(code, "R"); if (rp) {rp++; sscanf (rp, "%f", &cmdR);}
		fp = strstr(code, "F"); if (fp) {fp++; sscanf (fp, "%f", &cmdF);}
		
		// get the z adjustment value
		adjZ = interpolate (cmdX, cmdY);
		
		// create a new gcode line
		sprintf (newcode, "%s ", gp);
		if (fp) sprintf (&newcode[strlen (newcode)], "F%f ", cmdF);
		if (xp) sprintf (&newcode[strlen (newcode)], "X%f ", cmdX);
		if (yp) sprintf (&newcode[strlen (newcode)], "Y%f ", cmdY);
		sprintf (&newcode[strlen (newcode)], "Z%f ", cmdZ + adjZ);
		if (ip) sprintf (&newcode[strlen (newcode)], "I%f ", cmdI);
		if (jp) sprintf (&newcode[strlen (newcode)], "J%f ", cmdJ);
		if (kp) sprintf (&newcode[strlen (newcode)], "K%f ", cmdK);
		if (rp) sprintf (&newcode[strlen (newcode)], "R%f ", cmdR);
		sprintf (&newcode[strlen (newcode)], "\n");
	}
	else
	{
		strcpy (newcode, code);
	}
	
	return rc;
}
