//////////////////////////////////////////////////////////////////////
//
// ui.c		Handle user interface (touch screen and jog wheel)
//
//////////////////////////////////////////////////////////////////////
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
#define BUFFER_MAX_LENGTH 254

extern GtkWidget *GCFile;

extern int getPQ (void);

extern void measure_setinch(void);
extern void measure_setmm(void);

void getXYZ (char *line, float *x, float *y, float *z)
{
	char *xp = NULL;
	char *yp = NULL;
	char *zp = NULL;
	
	
	xp = strstr(line, "X"); if (xp) {xp++; sscanf (xp, "%f", x);}
	yp = strstr(line, "Y"); if (yp) {yp++; sscanf (yp, "%f", y);}
	zp = strstr(line, "Z"); if (zp) {zp++; sscanf (zp, "%f", z);}
}

void getXYIJ (char *line, float *x, float *y, float *i, float *j)
{
	char *xp = NULL;
	char *yp = NULL;
	char *ip = NULL;
	char *jp = NULL;
	
	
	xp = strstr(line, "X"); if (xp) {xp++; sscanf (xp, "%f", x);}
	yp = strstr(line, "Y"); if (yp) {yp++; sscanf (yp, "%f", y);}
	ip = strstr(line, "I"); if (ip) {ip++; sscanf (ip, "%f", i);}
	jp = strstr(line, "J"); if (jp) {jp++; sscanf (jp, "%f", j);}
}

float getFeed (char *line)
{
	char *fp = NULL;
	float f = 300.0;

	
	fp = strstr(line, "F"); if (fp) {fp++; sscanf (fp, "%f", &f);}

	return f;
}

float lineLen(float iX, float iY, float iZ, float gX, float gY, float gZ)
{
	float sq;
	
	
//	sq = ((gX - iX) * (gX - iX)) + ((gY - iY) * (gY - iY)) + ((gZ - iZ) * (gZ - iZ));
	sq = ((gX - iX) * (gX - iX)) + ((gY - iY) * (gY - iY));

	return sqrtf (sq);
}

static float speed = 2400.0;			// default g0 move speed
static float sX=0.0, sY=0.0, sZ=0.0;	// initial positions
static float minX = 1000.0, minY = 1000.0, maxX = 0.0, maxY = 0.0;

void saveMinMax(float x, float y)
{
	if(x < minX) { minX = x; }
	if(y < minY) { minY = y; }
	if(x > maxX) { maxX = x; }
	if(y > maxY) { maxY = y; }
}

void initMinMax(void)
{
	minX = minY = 1000.0;
	maxX = maxY = 0.0;
}

void getMinMax(float *xMin, float *xMax, float *yMin, float *yMax)
{
	*xMin =  minX;
	*yMin =  minY;
	*xMax =  maxX;
	*yMax =  maxY;
}

int parseGCTime (char *line, int len)
{
	int rv = 0;
	float gX=0.0, gY=0.0, gZ=0.0;	// end positions
	float gI=0.0, gJ=0.0;			// arc values
	float I = 0.0, J = 0.0;			// arc specifiers
	float dist = 0.0;
	float theta = 0.0;
	float radius = 0;
	float cosang;
	
	
	// read the speed if it is present on this line
	if (strstr (line, "F"))
	{
		speed = getFeed(line);
	}
	
	// find g0 and g1 and feed values
	if (strstr (line, "g0") || strstr(line, "G0"))
	{
		// read x,y,z
		gX = sX; gY = sY; gZ = sZ;			// initial start position
		getXYZ (line, &gX, &gY, &gZ);

		// save min/max
		saveMinMax(gX, gY);
		
		// determine length
		dist = lineLen(sX, sY, sZ, gX, gY, gZ);
		sX = gX; sY = gY; sZ = gZ;			// update the positions
		
		// time based on feed
		rv = (int)((dist / 3400.0) * 60.0);
	}
	else if (strstr (line, "g1") || strstr(line, "G1"))
	{
		// read x,y,z
		gX = sX; gY = sY; gZ = sZ;			// initial start position
		getXYZ (line, &gX, &gY, &gZ);
		
		// save min/max
		saveMinMax(gX, gY);
		
		// determine length
		dist = lineLen(sX, sY, sZ, gX, gY, gZ);
		sX = gX; sY = gY; sZ = gZ;			// update the positions
		
		// time based on feed
		rv = (int)((dist / speed) * 60.0);
	}
	else if (strstr (line, "g2") || strstr(line, "G2") || strstr (line, "g3") || strstr(line, "G3"))
	{
		// this is an arc
		// get x, y, i, j parameters
		gX = sX; gY = sY;					// initial start position
		getXYIJ (line, &gX, &gY, &I, &J);
		
		// save min/max
		saveMinMax(gX, gY);
		
		// calc arc length
		//    dist from start to finish sqrt ( (sX-gX)^2 + (sY-gY)^2 )
		//    use law of cosines to get dist from start point to end point
		dist = sqrtf (((sX - gX) * (sX - gX)) + ((sY - gY) * (sY - gY)));
		radius = sqrtf ((I*I) + (J*J));
		cosang = (1.0 - ((dist * dist) / (radius*radius*2)));
		if (cosang < 0) cosang = -cosang;
		cosang = fmodf(cosang, (3.14159/2));
		theta = acosf(cosang);
		
		//    determine arc measure (angle) in radians
		//    arc length is rTheta
		dist = radius * theta;
		rv = (int)((dist / speed) * 60);
		
		sX = gX; sY = gY; sZ = gZ;			// update the positions
		
	}

	return rv;
}

void levConfWrite( float xs, float ys, float xe, float ye);

int estCGFile (void)
{
	char *filename;
	char temp[35];
	char *buf = NULL;
	int len = 200;
	int lines = 0;
	int est_time = 0;
	FILE *tmpgcode = NULL;
	

	// this is where we open the file and send gcode lines
	filename = gtk_file_chooser_get_filename (GTK_FILE_CHOOSER (GCFile));	
	
	// open the file
	tmpgcode = fopen (filename, "r");
	
	// read the entire file, counting lines
	initMinMax();

	// read the entire file, counting lines
	while (getline(&buf, &len, tmpgcode) != -1)
	{
		est_time += parseGCTime (buf, len);
		lines++;
	}
float x1,y1,x2,y2;
getMinMax(&x1,&x2,&y1,&y2);
printf("minmax results xmin %f ymin %f xmax %f ymax %f\n", x1, y1, x2, y2);
levConfWrite( x1, y1, x2, y2);

	// display the total number of lines
	updateGCLine ((int) lines, 0);
	
	// update the estimated time
	updateGCEstTime ((int) est_time, 0);
	
	// close and re-open the file
	free(buf);
	fclose(tmpgcode);
}

int exline = 0;		// line of executing gcode
FILE *gcode = NULL;
int rem_time = 0;
int PlayCGFile (void)
{
	char *filename;
	char temp[35];
	char *buf = NULL;
	int len = 200;
	int lines = 0;
	int est_time = 0;
	

	// this is where we open the file and send gcode lines
	filename = gtk_file_chooser_get_filename (GTK_FILE_CHOOSER (GCFile));	
	
	// open the file
	gcode = fopen (filename, "r");
	
	while (getline(&buf, &len, gcode) != -1)
	{
		est_time += parseGCTime (buf, len);
		lines++;
	}
	
	// display the total number of lines
	updateGCLine ((int) lines, 0);
	
	// update the estimated time
	updateGCEstTime ((int) est_time, 0);
	rem_time = est_time;
	updateGCRemTime ((int) rem_time, 0);
	
	// close and re-open the file
	free(buf);
	fclose(gcode);
	gcode = fopen (filename, "r");
	
	// queue up sending (executed by the send_gcode thread)
	// set the mode to playing
	exline = 0;

}

char *buf = NULL;
char newbuf[350];

void term_send(void)
{
	if (buf) free (buf);
	buf = NULL;
	if (gcode) fclose (gcode);
	gcode = NULL;
	
}

int levApply(char *code, char *newcode);

int levelTimeout = 0;
#define MINPLANNER 5
void *send_gcode (void *ptr)
{
	int len = 0;
	int rv;
	int lv;
	int subseconds = 0;
	while (1)
	{
		usleep(5000);
		levelTimeout++;
		if (isLevel())
		{
			// if last probe is complete, start the next one
			lv = levTestDone(levelTimeout);
			if (lv == -1)
			{
				// timed out
				Tr_Level();
			}
			else
			{
				if (lv == 1)
				{
					// step for positioning
					if (levStepPos())
					{
						// issue the command
						levDoProbe(levelTimeout);
					}
					else
					{
						// done, save the file and transition to leveling done
						levSave();
						Tr_Level();
					}
					
				}
			}
		}
		if (isGCPlay())
		{
			if (++subseconds >= 220)
			{
				subseconds = 0;
				rem_time--;
				if (rem_time < 0) rem_time = 0;
				updateGCRemTime ((int) rem_time, 1);
			}
				
			// make sure there is room in the planner queue
			if (getPQ() > MINPLANNER)
			{
				// read a line
				usleep(50000);
				rv = getline(&buf, &len, gcode);
				
				// if the line has gcode in it send it
				if (rv == -1)
				{
					// done reading
					term_send();

					// state transition to cgode file complete
					Tr_FileComplete();
					updateExLine ((int) 0, 1);	// reset after done

				}
				else
				{
					// parse special cases (m6..)
					if (buf)
					{
						// update the mm/inch label
						if (strstr(buf, "g20") || strstr(buf, "G20"))
						{
							// label to inch
							measure_setinch();
						}
						if (strstr(buf, "g20") || strstr(buf, "G20"))
						{
							// label to inch
							measure_setmm();
						}
						
						// pause for tool change
						if (strstr(buf, "m6") || strstr(buf, "M6"))
						{
							// change mode to tool change and change 'Play' button to 'Restart'
							Tr_M6();
						}

						// apply leveling
						newbuf[0] = 0;
printf("before levApply\n");
						levApply (buf, newbuf);
printf("lev before >%s< after >%s<\n", buf, &newbuf[0]);
						
						// send the gcode line
						sersendNT(buf);
						
						// update the executing gcode line number
						exline++;
						updateExLine ((int) exline, 1);
					}
				}
			}
		}
	}
}

