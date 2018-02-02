#define _CRT_SECURE_NO_WARNINGS
#include <stdio.h>
#include <stdlib.h>
#include <string>
#include <vector>
#include <assert.h>
#include <iostream>
#include <math.h>

using namespace std;
typedef unsigned int uint;
typedef unsigned char byte;

#include "graph.h"
#include "bmp.h"

const uint numcolors=4;
uint colors[] = { rgb(255,0,0), rgb(0,255,0), rgb(0,0,255), rgb(255,255,255)};

void graph(vector<double> *vecs, uint numvecs, uint width, uint height, string filename, bool zoom) 
{
	BMP bmp(24, width, height);

	double maxval = DBL_MIN;//abs(vecs[0][0]);
	double minval = DBL_MAX;//abs(vecs[0][0]);
	uint maxlen=0;

	for (uint v=0; v <numvecs; v++)
		maxlen = max(maxlen, (uint)vecs[v].size()/(zoom ? 16 : 1));
	if(zoom)
		maxlen=(width-1) *32;

	for (uint v=0; v <numvecs; v++)
	{
		//maxlen = max(maxlen, vecs[v].size()/(zoom ? 1 : 16));
		//if(zoom)
			//maxlen/=16;
		for (uint d=0; d<vecs[v].size() && d<maxlen; d++)
		{
			maxval = max(maxval, abs(vecs[v][d]));
			minval = min(minval, abs(vecs[v][d]));
		}
	}
	//if(zoom==true)
		//maxlen=width-1;

	double xscale = (double)(width-1) / (double)maxlen;
	double yscale = (double)(height-1) / (maxval);
	yscale /= 2.025;

	for(uint v=0; v<numvecs; v++)
	{
		uint color = colors[v%numcolors];
		for(uint d=0;d<vecs[v].size() && d<maxlen;d++)
		{
			uint x = (uint)((double)d * xscale);
			double val = vecs[v][d];
			val=val*-1.0;
			val = (val * yscale);
			uint y = (uint)(val + ((double)bmp.hheight/2.0));
			//y += bmp.hheight/2;
			//cout << val << " ("<<x<<", "<<y<<")\n";
			bmp.SetPix(x, y, color);
			/*if(zoom && val<0.0)
			{
				//bmp.SetPix(x, y+1, color);
				for(double alpha=1.0;alpha>0.0 && y<(uint)bmp.hheight && y<height/2;alpha-=0.5,y++)
				{
					bmp.SetPix(x, y, color, alpha);
				}
			}
			else if(zoom && val>0.0)
			{
				//bmp.SetPix(x, y-1, color);
				for(double alpha=1.0;alpha>0.0 && y<(uint)bmp.hheight && y>height/2;alpha-=0.5,y--)
				{
					bmp.SetPix(x, y, color, alpha);
				}
			}*/
		}
	}

	for(uint x=0;x<(uint)bmp.hwidth;x++)
		bmp.SetPix(x, bmp.hheight/2, rgb(127,127,127));

	FILE *outfile = fopen(filename.c_str(), "w");
	fwrite((void*)&bmp, sizeof(BMPHeader), 1, outfile);
	fwrite((void*)bmp.data, bmp.rawdatasize, 1, outfile);
	fclose(outfile);
}
