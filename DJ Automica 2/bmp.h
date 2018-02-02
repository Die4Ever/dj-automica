
//#ifdef LINUX
#define rgb(r,g,b)          ((uint)(((uint)(byte)(r)<<16|((uint)((byte)(g))<<8))|(((uint)(byte)(b)))))
//#endif
#include <math.h>
#include <algorithm>

#pragma pack(1)
struct BMPHeader
{
	//BMP Header
	char BM[2];
	uint size;
	short unused1;
	short unused2;
	uint offset;

	//DIB Header
	uint DIBsize;
	uint width;
	uint height;
	unsigned short colorplanes;
	unsigned short BPP;
	uint compression;
	uint rawdatasize;
	uint hres;
	uint vres;
	uint palettecolors;
	uint importantcolors;

	BMPHeader(int bpp, int Width, int Height)
	{
		BM[0] = 'B';
		BM[1] = 'M';

		uint bytesperpixel = bpp/8;

		size = 54;
		size += Width*Height*bytesperpixel;
		size += (4-(Width*bytesperpixel)%4)*Height;
		//size = htonl(size);

		unused1=unused2=0;

		offset=(54);

		DIBsize=(40);
		width=(Width);
		height=(Height);
		colorplanes=(1);
		BPP=(bpp);
		compression=0;
		rawdatasize = Width*Height*bytesperpixel;
		//rawdatasize += (4-(Width*bytesperpixel)%4)*Height;
		//rawdatasize = htonl(rawdatasize);
		hres=(120);
		vres=(120);
		palettecolors=0;
		importantcolors=0;
	}

};

struct BMP : public BMPHeader
{
	int hwidth;
	int hheight;
	char *data;
	int hBPP;

	uint hbytesperpixel;
	uint hdatasize;
	uint datawidth;

	BMP(int bpp, int Width, int Height) : BMPHeader(bpp, Width, Height)
	{
		hwidth=Width;
		hheight=Height;
		hBPP=bpp;
		hbytesperpixel = hBPP/8;

		hdatasize = (rawdatasize);

		data = new char[hdatasize];
		memset(data, 0, hdatasize);

		//datawidth=(4-(hwidth*hbytesperpixel)%4)+hwidth*hbytesperpixel;
		//datawidth=hwidth*hbytesperpixel;
	}

	~BMP()
	{
		delete[] data;
		data=NULL;
	}

	void SetPix(uint x, uint y, uint color)
	{
		//return;
		//if(y<hwidth/2)
			//return;
		assert(x<(uint)hwidth);
		assert(y<(uint)hheight);

		y=hheight-y;
		y--;
		char *p = &data[ (y*hwidth+x)*hbytesperpixel ];
		for(uint i=0;i<hbytesperpixel;i++)
			p[i] = ((char*)&color)[i];
	}

	void SetPix(uint x, uint y, uint color, double alpha)
	{
		//if(y<hwidth/2)
			//return;
		assert(x<(uint)hwidth);
		assert(y<(uint)hheight);
		assert(alpha<=1.0);
		assert(alpha>=0.0);

		y=hheight-y;
		y--;

		unsigned char *p = (unsigned char*)&data[ (y*hwidth+x)*hbytesperpixel ];
		double oldr = (double)p[0];
		double oldg = (double)p[1];
		double oldb = (double)p[2];

		double newr = (double)((color>>16) & 0xFF);
		double newg = (double)((color>>8) & 0xFF);
		double newb = (double)(color & 0xFF);

		oldr = newr*alpha + oldr*(1.0-alpha);
		oldg = newg*alpha + oldg*(1.0-alpha);
		oldb = newb*alpha + oldb*(1.0-alpha);

		uint newcolor=rgb(std::min(255u,(uint)oldr),std::min(255u,(uint)oldg),std::min(255u,(uint)oldb));
		//newcolor=color;
		for(uint i=0;i<hbytesperpixel;i++)
			p[i] = ((unsigned char*)&newcolor)[i];
	}
};

#pragma pack()
