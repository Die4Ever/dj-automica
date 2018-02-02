#define _CRT_SECURE_NO_WARNINGS
#include <stdio.h>
#include <stdlib.h>
#include <memory>
#include <queue>
#include <math.h>
#include <time.h>
#include <fcntl.h>
#include <iostream>
#include <string>
#include <math.h>
//#include <Windows.h>
#include <fstream>
#include <vector>
#include <queue>
#include <sstream>
#include <unordered_map>
#include <thread>
//#include <mutex>

#ifdef WIN32
#include <io.h>
#endif
using namespace std;

#ifdef WIN32
#include <direct.h>
#else
#include <signal.h>
#include <sys/time.h>
#include <sys/resource.h>
#include <sys/stat.h>
#include <dirent.h>
#endif

//#include "Complex.h"
#include "Fft.h"

#define DEBUGLEVEL 7
#define DEBUGFILELEVEL 7

#define RAYHEADERSMAIN
#include "main.h"
#include "analysis v2.h"

Song RandomSong()
{
	uint s = rand()%Songs.size();
	uint i=0;
	for(auto x=Songs.begin();x!=Songs.end();++x,i++)
	{
		if(i==s && x->second.tempo>1.0 && x->second.length>44100*2*60)//at least 1 minute long
			return x->second;
		else if(i==s)
			s++;
	}
	return Song();//Songs.begin()->second;
}

uint CalcStart(double tempo, float *buff, uint BuffSize)
{
	uint offset = (uint) (60.0 / tempo * 44100.0);
	uint start=0;
	float div = 65536.0 /1024.0;
	uint stop = min(BuffSize, offset*3);
	uint stop2 = BuffSize-offset*8;

	float maxsum=0.0;
	for(uint s=offset;s<stop;s+=50)
	{
		float sum=0.0;
		for(uint i=s;i<s+stop2;i+=offset)
		{
			for(uint x=i;x<i+offset/16;x++)
			{
				float d = buff[x];
				//d /= 16.0;
				sum += d;
			}
		}
		//for(uint i=s;i<s+44100*60;i+=16)
		//sum += (double)(buff[i] * (((i-s)*4/offset) %8)==0 ? 1 : 0)/div;
		if(sum>maxsum)
		{
			maxsum=sum;
			start=s%(offset*2);
		}
	}
	//cout << "start == "<<start<<"\n";

	//stop = min(start+100,BuffSize);
	maxsum=0.0;
	stop = BuffSize - offset*2;
	uint tstart = start+offset;
	stop = min(offset*32*4,BuffSize-offset*4);
	//stop = min(offset*4*4,BuffSize-offset*2);
	for(uint s=max(tstart,2000)-2000;s<tstart+2000;s+=5)
		//for(uint s=0;s<offset+1;s+=5)
	{
		float sum=0.0;
		for(uint i=s;i<stop+s;i+=offset)
		{
			for(uint x=i;x<i+offset/16;x++)
			{
				float d = buff[x];
				//d /= 16.0;
				sum += d;
				//sum = max(sum,d);
			}
		}
		//for(uint i=s;i<stop+s;i+=8)
		//sum += (double)(buff[i] * (((i-s)*4/offset) %8)==0 ? 1 : 0)/div;
		if(sum>maxsum)
		{
			maxsum=sum;
			start=s%(offset*2);
		}
	}
	//cout << "final start == "<<start<<"\n";

	/*if(start<44)
	start+=offset;
	start -= 44;//subtract 1 ms for attack*/
	return start;
}

double CalcTempoStrength(double tempo, float *buff, uint BuffSize)//, vector<double> *vec=NULL)
{
	uint offset = (uint) (60.0 / tempo * 44100.0);
	float ret = 0.0;
	uint end = BuffSize-offset;
	auto pend = &buff[end];

	for(auto p=buff;p<pend;p+=4)
	{
		ret += p[0] * p[offset];
		ret += p[1] * p[offset+1];
		ret += p[2] * p[offset+2];
		ret += p[3] * p[offset+3];
	}

	ret /= (float)(end);//make it an average to make up for the fact that slower tempos has less samples
	return (double)ret;
}

double CalcTempoStrengthBridge(double tempo, float *buff1, uint bufflen, float *buff2, uint buff2offset)
{
	double doffset = (60.0 / tempo * 44100.0);
	float ret = 0.0;
	double count = (double)buff2offset / doffset;
	//double total = count * doffset;
	uint offset = (uint)((1.0 - fmod(count, 1.0)) * doffset);
	uint end=min(bufflen,bufflen-offset);
	auto p1=buff1;
	auto p2=&buff2[offset];
	auto pend=&buff2[end];

	for(;p2<pend;p1++,p2++)
		ret += *p1 * *p2;
	/*for(uint i=0;i<end;i++)
	{
	ret += buff1[i] * buff2[i+offset];
	}*/
	ret /= (float)end;
	return (double)ret;
}

void AnalyzeTempo(Song &song, short *buffs, uint bufflen, uint starts)
{
	uint BuffSize=bufflen/2;
	float *dbuff = new float[BuffSize];
	memset(dbuff, 0, BuffSize*sizeof(dbuff[0]));

	for(uint s=0;s<BuffSize;s++)
		dbuff[s] = (float)buffs[s*2]/2.0f + (float)buffs[s*2+1]/2.0f;

	uint avgcount=1;
	double avgvol = 0.0;
	double prevout=0.0;
	for(uint s=0;s<BuffSize;s++)
	{
		double val = (double)dbuff[s];
		val=abs(val);
		if(val>500)
		{
			avgvol += val;
			avgcount++;
		}
		double prevprevval = prevout;
		prevout=val;
		val -= prevprevval;
		if(val<0.0)
			val=0.0;
		dbuff[s] = (float)(val/128.0);
	}
	avgvol /= (double)avgcount;
	double avgpeak=0.0;
	avgcount=1;
	for(uint s=0;s<BuffSize;s++)
	{
		double val = (double)buffs[s*2]/2.0f + (double)buffs[s*2+1]/2.0f;
		val=abs(val);
		if(val>avgvol)
		{
			avgpeak += val;
			avgcount++;
		}
	}
	avgpeak /= (double)avgcount;
	//avgvol = max(avgvol, avgpeak/2.0);

	uint tickspermeasure = 2;
	double besttempo = 120.0;
	double maxstr = -99999.0;
	for(double t=50.0;t<400.0;t+=0.25)
	{
		double str = CalcTempoStrength(t, dbuff, 44100*20);
		str += CalcTempoStrength(t, dbuff+BuffSize-44100*30, 44100*20);
		/*double str2 = 0.0;
		uint ttickspermeasure=4;
		for(uint m=3;m<=8;m++)//only need 5 to 8?
		{
			double tstr2 = CalcTempoStrength(t/((double)m), dbuff, 44100*20);
			tstr2 += CalcTempoStrength(t/((double)m), dbuff+BuffSize-44100*30, 44100*20);
			str += tstr2/4.0;
			if(tstr2>str2)
			{
				str2=tstr2;
				ttickspermeasure = m;
			}
		}
		str += str2/1.0;*/
		if(str > maxstr)
		{
			maxstr=str;
			besttempo=t;
			tickspermeasure = 4;// ttickspermeasure;
		}
	}

	uint oldtickspermeasure=tickspermeasure;
	double oldbesttempo=besttempo;
	maxstr=-99999.0;

	//for(double t=oldbesttempo-0.5;t<oldbesttempo+0.5;t+=0.0005)
	for(double t=oldbesttempo-0.5;t<oldbesttempo+0.5;t+=0.05)
	{
		double str = CalcTempoStrength(t, dbuff, 44100*20);
		str += CalcTempoStrength(t, dbuff+BuffSize-44100*30, 44100*20);
		/*double str2 = 0.0;
		uint ttickspermeasure=4;
		for(uint m=3;m<=8;m++)//only need 5 to 8?
		{
			double tstr2 = CalcTempoStrength(t/((double)m), dbuff, 44100*20);
			tstr2 += CalcTempoStrength(t/((double)m), dbuff+BuffSize-44100*30, 44100*20);
			str+=tstr2/2.0;
			if(tstr2>str2)
			{
				str2=tstr2;
				ttickspermeasure = m;
			}
		}
		str += str2/1.0;*/
		if(str > maxstr)
		{
			maxstr=str;
			besttempo=t;
			tickspermeasure = 4;// ttickspermeasure;
		}
	}

	//cout << "\tstage 1 tempo == "<<oldbesttempo<<"\n";
	oldbesttempo=besttempo;
	maxstr = -99999.0;

	for(double t=oldbesttempo-0.9;t<oldbesttempo+0.9;t+=0.00025)
	{
		uint buff2start = min((uint)(44100.0*400.0/t),BuffSize-44100*10);
		double str = CalcTempoStrengthBridge(t, dbuff, 44100*10, dbuff+buff2start, buff2start);
		if(str > maxstr)
		{
			maxstr=str;
			besttempo=t;
		}
	}

	oldbesttempo=besttempo;
	maxstr = -99999.0;

	for(double t=oldbesttempo-0.1;t<oldbesttempo+0.1;t+=0.0001)
	{
		uint buff2start = min((uint)(44100.0*120000.0/t),BuffSize-44100*10);
		double str = CalcTempoStrengthBridge(t, dbuff, 44100*10, dbuff+buff2start, buff2start);
		if(str > maxstr)
		{
			maxstr=str;
			besttempo=t;
		}
	}

	oldbesttempo=besttempo;
	maxstr = -99999.0;

	for(double t=oldbesttempo-0.01;t<oldbesttempo+0.01;t+=0.0001)
	{
		uint buff2start = min(44100*60*6,BuffSize-44100*10);
		double str = CalcTempoStrengthBridge(t, dbuff, 44100*10, dbuff+buff2start, buff2start);
		if(str > maxstr)
		{
			maxstr=str;
			besttempo=t;
		}
	}

	assert( abs(oldbesttempo-besttempo)<10.0 );

	uint start = CalcStart(besttempo/((double)tickspermeasure), dbuff, BuffSize);
	uint offset = (uint) (60.0 / besttempo * 44100.0);
	start += starts;
	start %= offset;

	cout << "\tanalyzed song " << PathToDisplay(song.filepath)<<"\n";
	cout << "\ttempo = " << besttempo << " == "<<maxstr<<"\n\ttickspermeasure == "<<tickspermeasure;
	cout << "\tlength == "<<song.length<<"\n";

	song.BeatsPerMeasure = tickspermeasure;
	song.MeasuresPerPhrase = 1;
	song.tempo = besttempo;
	song.offset = start;
	song.avgvol = (uint)avgvol;
	song.peakavgvol = (uint)avgpeak;

	delete[] dbuff;
}

void AnalyzeSong(Song &song)
{
	SongPlayer sp(song.filepath);
	uint BuffsSize=44100*240*2;
	short *buffs = new short[BuffsSize];
	memset(buffs, 0, BuffsSize*2);

	uint starts = 44100*2*40;
	uint pos=0;
	uint s=0;

	assert(starts<BuffsSize);

	song.length = sp.Read(buffs, starts);
	uint samplesread = sp.Read(buffs, BuffsSize);
	song.length += samplesread;
	if(samplesread<BuffsSize && samplesread >= 44100*60*2)
	{
		BuffsSize=samplesread;
	}
	if(samplesread <BuffsSize && samplesread < 44100*60*2)
	{
		if(song.length<44100*2*10)
		{
			string message = WstringToString(song.filepath) + " is only " + ToString( (int)((double)song.length*1000.0/(double)(44100*2)) ) + " ms long!\n";
			cout << "\t"<<message<<"\n";
			corrupt_files_lock.lock();
			corrupt_files.write(message.c_str(), message.length());
			corrupt_files.flush();
			corrupt_files_lock.unlock();
		}
		song.tempo=-1.0;
		//song.length=0;
		song.version=CURRENT_DJA_VERSION;
		song.Save();
		delete[] buffs;
		return;
	}
	//delete[] buffs;
	//return;

	while(samplesread>=BlockSize)
	{
		short tbuff[BlockSize];
		samplesread = sp.Read(tbuff, BlockSize);
		song.length += samplesread;
	}
	sp.CloseStream();

	//AnalyzeTempo(song, buffs, BuffsSize, starts);
	vector<short> vbuffs;
	vbuffs.resize(BuffsSize);
	for (uint i = 0; i < BuffsSize; i++) vbuffs[i] = buffs[i];
	AnalyzeTempoV2(song, vbuffs, starts);

	delete[] buffs;
	song.version=CURRENT_DJA_VERSION;
	song.Save();
}

void AnalyzeSongsThread(uint start, uint end)
{
	auto s = Songs.begin();
	for(uint i=0;i<start;i++,++s) ;

	for(uint i=start; i<end; i++,++s)
	{
		if(/*abs(s->second.tempo)>0.1 && */s->second.version==CURRENT_DJA_VERSION)//-1.0 is used as an error...
			continue;
		Sleep(1);
		//s = Songs.find(wstring(L"I:\\Music\\Orange Dust\\En Garde\\02 At Large.mp3"));
		//cout << string("start == ")+ToString(start) + ", end == "+ToString(end) + " analyzing song #" + ToString(i) + ": " + WstringToString(s->second.filepath)+"\n";
		uint percent = (uint)((double)(i-start)/(double)(end-start)*100.0);
		cout << "\tanalyzing song #" + ToString(i-start) + "/" + ToString(end-start) + ", " + ToString(percent) + "%:\n\t" + PathToDisplay(s->second.filepath)+"\n";
		uint st = GetTickCount();
		AnalyzeSong(s->second);
		uint mstook = GetTickCount()-st;
		//if(mstook>1000)
		cout << "-Took "<<(mstook)<<" ms\n";
		//exit(0);
		//s->second.tempo = 300.0;
	}

	corrupt_files_lock.lock();
	corrupt_files.flush();
	corrupt_files_lock.unlock();

	if(start!=0)
		return;
	end=(uint)Songs.size();
	cout << "\tcompleted analyzing songs, going back for old ones\n";
	//return;

	/*s = Songs.begin();
	for(uint i=0;i<start;i++,++s) ;

	for(uint i=start; i<end; i++,++s)
	{
		//if(s->second.tempo<0.1)
			//continue;
		Sleep(10);
		//cout << string("start == ")+ToString(start) + ", end == "+ToString(end) + " analyzing song #" + ToString(i) + ": " + WstringToString(s->second.filepath)+"\n";
		cout << "\tre-analyzing song #" + ToString(i) + "/" + ToString(end) + ":\n\t" + PathToDisplay(s->second.filepath)+"\n";
		AnalyzeSong(s->second);
		//s->second.tempo = 300.0;
	}
	cout << "\tcompleted re-analyzing songs!\n";*/
}

void AnalyzeSongs()
{
#ifdef _DEBUG
	const uint threads=8;
#else
	const uint threads=8;
#endif
	uint start[threads+1] = {0};
	for(uint i=1;i<threads+1;i++)
	{
		start[i] = ((uint)Songs.size()/threads)*i;
		start[threads] = (uint)Songs.size();//yea I'm that lazy
		thread( AnalyzeSongsThread, start[i-1], start[i] ).detach();
	}

	//while(1) Sleep(100);
}

class Freq
{
public:
	double freq;
	double vel;
	size_t len;

	Freq()
	{
		vel=0.0;
		freq=0.0;
		len=1;
	}

	bool operator<=(const Freq &b)
	{
		return vel <= b.vel;
	}
};

void SineOsc(double freq, double vel, size_t len,  double &phase, double *dsamples)
{
	if(vel<0.01)
		return;

	vel = log(vel);//+1.0;
	int ivel = (int)(vel*4.0 + 0.5);
	ivel = __max(1, __min(16, ivel) );
	vel = (double)(ivel) / 4.0;

	double tone = freq / divisor;
	double p = phase;

	vel *= 2.0;
	for(size_t s=0;s<len;s++)
	{
		dsamples[s] += (sin( p )) * vel;
		p += tone;
	}
	phase=p;
}

void SquareOsc(double freq, double vel, size_t len,  double &phase, double *dsamples)
{
	if(vel<0.01)
		return;

	vel = log(vel);//+1.0;
	int ivel = (int)(vel*4.0 + 0.5);
	ivel = __max(1, __min(16, ivel) );
	vel = (double)(ivel) / 4.0;

	int crushedfeq = (int)(111875.0 / freq + 0.5);
	//if(crushedfeq==0 || crushedfeq>2048)
	//continue;
	//freq = 111875.0 / (double)crushedfeq;

	//vel = (log(vel)+1.0 + vel)/2.0;
	//vel = log(vel)+1.0;
	double tone = freq / divisor;
	double p = phase;

	double pulsewidth = 0.0;
	switch( (crushedfeq/2)%4 )
	{
	case 0: pulsewidth=-0.75; break;
	case 1: pulsewidth=-0.5; break;
	case 2: pulsewidth=0.0; break;
	case 3: pulsewidth=0.5; break;
	}

	for(size_t s=0;s<len;s++)
	{
		dsamples[s] += (sin( p )>pulsewidth? 1.0 : -1.0) * vel;
		p += tone;
	}
	phase=p;
}

void TriangleOsc(double freq, double vel, size_t len, double &phase, double *dsamples)
{
	if(vel<0.01)
		return;

	vel = 8.0;

	int crushedfeq = (int)(55937.5 / freq + 0.5);
	//if(crushedfeq==0 || crushedfeq>2048)
	//continue;
	//freq = 55937.5 / (double)crushedfeq;

	//vel = log(vel)+1.0;
	double tone = freq / divisor;
	double p = phase;

	vel *= 2.0;//adjusts the scale from (-0.5, 0.5 ) to (-1, 1)
	for(size_t s=0;s<len;s++)
	{
		//dsamples[s] += (sin( p )>0.0? 1.0 : -1.0) * vel;
		double samp = ((1.0 - abs(1.0 - fmod(p / pi, 2.0) ))-0.5);
		dsamples[s] += samp * vel;
		p += tone;
	}
	phase=p;
}


void NoiseOsc(double freq, double vel, size_t len, double &phase, double *dsamples)
{
	if(vel<0.01)
		return;

	//vel = log(vel/2.0);//+1.0; 
	//vel*=64.0;
	int ivel = (int)(vel*4.0 + 0.5);
	//if(ivel==0)
	//return;
	ivel = __max(1, __min(16, ivel) );

	vel = (double)(ivel) / 4.0;
	//vel *= 4.0;

	//int crushedfeq = (int)(111875.0 / freq + 0.5);
	//if(crushedfeq==0 || crushedfeq>2048)
	//continue;
	//freq = 111875.0 / (double)crushedfeq;

	//vel = log(vel)+1.0;
	double tone = freq / divisor;
	double p = phase;

	srand( (int)(p*1000.0) );

	vel = vel / 16384.0 * 5.0;
	for(size_t s=0;s<len;s+=2)
	{
		//dsamples[s] += (sin( p )>0.0? 1.0 : -1.0) * vel;
		dsamples[s] += (double)((rand()%32768)-16384) * vel;
		p += tone;
	}
	for(size_t s=1;s<len;s+=2)
	{
		dsamples[s] = (dsamples[s-1] + dsamples[s+1]) /2.0;
	}

	phase=p;
}

void GroupFreqs(Freq *freqs, int len)
{
	//return;
	double dlen = (double)len;
	dlen /= 512.0;
	for(int i=1;i<len;i++)
	{
		if(freqs[i].freq == freqs[i-1].freq)
		{
			freqs[i].vel += freqs[i-1].vel;
			freqs[i-1].vel=0.0;
			continue;
		}
		if(freqs[i-1].vel < 1.0)
			continue;
		if(freqs[i].vel < 1.0)
		{
			freqs[i] = freqs[i-1];
			freqs[i].vel *= 0.8;
			freqs[i-1].vel=0.0;
			continue;
		}
		if(freqs[i].freq - freqs[i-1].freq > (110.0 / dlen) )
			continue;

		freqs[i].freq = Interpolate<double>(freqs[i].freq, freqs[i-1].freq, freqs[i-1].vel/(freqs[i-1].vel+freqs[i].vel));
		freqs[i].vel += freqs[i-1].vel;
		//freqs[i].freq = (freqs[i].freq*3.0 + freqs[i-1].freq)/4.0;
		freqs[i-1].vel = 0.0;
	}
}

int GetNote(double freq)
{
	int f=1;
	for(;f<127;f++)
		if(NoteFreqs[f]>freq)
			break;
	double beforedist = abs( NoteFreqs[f-1]-freq );
	double afterdist = abs( NoteFreqs[f]-freq );
	int note = beforedist < afterdist ? f-1 : f;
	return note;
}

pair<int,int> GetNotes(double freq)
{
	pair<int,int> ret(0,0);
	int f=1;
	for(;f<127;f++)
		if(NoteFreqs[f]>freq)
			break;
	//double beforedist = abs( NoteFreqs[f-1]-freq );
	//double afterdist = abs( NoteFreqs[f]-freq );
	//int note = beforedist < afterdist ? f-1 : f;
	ret.first = f-1;
	ret.second = f;
	return ret;
}

uint FindNotes(vector<Freq> &notes, Freq dfreqs[NUMHIGHS], double minf, double maxf, uint slot=0)
{
	notes.push_back( Freq() );

	for(int i=slot+1;i<=8;i++)
	{
		double freq = dfreqs[NUMHIGHS-i].freq;
		double vel = dfreqs[NUMHIGHS-i].vel;
		//vel = log(vel);
		if(freq<minf || freq>=maxf || vel<0.5)
			continue;
		notes.back().freq = freq;
		notes.back().len = 1;
		notes.back().vel = vel;
		int midinote = GetNote(freq);
		//cout << NoteNames[midinote] << " " <<vel<<"\n";
		NoteCounts[midinote]++;
		slot=i;
		break;
	}
	return slot;
}

void CombineNotes(vector<Freq> &notes, double strength)
{
	//return;
	Freq old;
	Freq extraold;
	for(uint i=0;i<notes.size();i++)
	{
		notes[i].vel = min(notes[i].vel, 16.0);
		if(notes[i].vel < 0.1)
		{
			notes[i] = old;
			notes[i].len++;
			notes[i].vel *= 0.95;
		}
		else if(notes[i].freq!=old.freq && notes[i].freq==extraold.freq)
		{
			old = extraold;
			old.len++;
			old.vel*=0.95;
			notes[i].len = old.len+1;
			notes[i-1] = old;
		}
		if(notes[i].freq == old.freq)
		{
			if(notes[i].vel < old.vel*1.2)
				notes[i].len = old.len+1;
			else
				notes[i].len = 1;
		}
		/*if(notes[i].vel < old.vel*1.1)
		{
		notes[i]=old;
		notes[i].len++;
		notes[i].vel*=0.9;
		}*/
		extraold=old;
		old = notes[i];
	}
}

void handler (int signum)
{
	cerr << "SIGPIPE!\n";
}


void InitNotes()
{
	NoteFreqs[60] = 261.6255653006;//C0
	NoteFreqs[61] = 277.1826309769;//C0#
	NoteFreqs[62] = 293.6647679174;
	NoteFreqs[63] = 311.1269837221;
	NoteFreqs[64] = 329.6275569129;
	NoteFreqs[65] = 349.2282314330;
	NoteFreqs[66] = 369.9944227116;
	NoteFreqs[67] = 391.9954359817;
	NoteFreqs[68] = 415.3046975799;
	NoteFreqs[69] = 440.0000000000;//A0
	NoteFreqs[70] = 466.1637615181;
	NoteFreqs[71] = 493.8833012561;

	for(int o=-5;o<6;o++)
	{
		int i = (o+5)*12;
		NoteNames[i++] = "C/B# " + ToString(o);
		NoteNames[i++] = "C#/Db " + ToString(o);
		NoteNames[i++] = "D " + ToString(o);
		NoteNames[i++] = "D#/Eb " + ToString(o);
		NoteNames[i++] = "E/Fb " + ToString(o);
		NoteNames[i++] = "F/E# " + ToString(o);
		NoteNames[i++] = "F#/Gb " + ToString(o);
		NoteNames[i++] = "G " + ToString(o);
		if(o<5)
		{
			NoteNames[i++] = "G#/Ab " + ToString(o);
			NoteNames[i++] = "A " + ToString(o);
			NoteNames[i++] = "A#/Bb " + ToString(o);
			NoteNames[i++] = "B/Cb " + ToString(o);
		}
	}

	for(int i=72;i<128;i++)
		NoteFreqs[i] = NoteFreqs[i-12]*2.0;
	for(int i=59;i>=0;i--)
		NoteFreqs[i] = NoteFreqs[i+12]/2.0;

	for(int i=1;i<11;i++)
	{
		for(int a=0;a<7;a++)
		{
			wnotes[a+i*7] = wnotes[a] + i*12;
		}
	}

	for(uint i=0;i<128;i++)
		cout << i << ": " << NoteNames[i] << " - " << NoteFreqs[i] << "hz.\n";
}

void MoveMouse()
{
	LASTINPUTINFO lii;
	lii.cbSize=sizeof(lii);
	GetLastInputInfo(&lii);
	DWORD now = GetTickCount();
	if( now-lii.dwTime > 120000 )
	{
		tagINPUT mi[1];
		mi[0].type=INPUT_MOUSE;
		mi[0].mi.dx=1;
		mi[0].mi.dy=1;
		mi[0].mi.mouseData=0;
		mi[0].mi.time=0;
		mi[0].mi.dwExtraInfo=0;
		mi[0].mi.dwFlags=MOUSEEVENTF_MOVE | MOUSEEVENTF_MOVE_NOCOALESCE;
		cout << "SendInput() == "<<SendInput(1, (LPINPUT)&mi[0], sizeof(mi[0]))<<"================================\n";
		mi[0].type=INPUT_MOUSE;
		mi[0].mi.dx=-1;
		mi[0].mi.dy=-1;
		mi[0].mi.mouseData=0;
		mi[0].mi.time=0;
		mi[0].mi.dwExtraInfo=0;
		mi[0].mi.dwFlags=MOUSEEVENTF_MOVE | MOUSEEVENTF_MOVE_NOCOALESCE;
		cout << "SendInput() == "<<SendInput(1, (LPINPUT)&mi[0], sizeof(mi[0]))<<"================================\n";
		//cout << GetLastError()<<"\n";
	}
}

void ReadConfig(bool reload=false)
{
	MoveMouse();
	{
		char buff[65535];
		ifstream temp("current.txt");
		temp.read(buff, 65534);
		int read = (int)temp.gcount();
		if(read>0)
		{
			buff[read] = '\0';
			htmllock.lock();
			currenttemplate=buff;
			htmllock.unlock();
		}
	}

	ifstream configfile("config.txt");
	//cout << "\t\tloading config\n";
	while(configfile.good())
	{
		string line;
		//wstring wline;
		//std::getline(configfile, wline);
		std::getline(configfile, line);
		const char *var = line.c_str();
		char *value = (char*)strchr(var, '=');
		if(value==NULL)
			continue;
		*value ='\0';
		value++;
		char *comment = (char*)strchr(value, '#');
		if(comment)
			*comment = '\0';

		string svar = var;
		//string svalue = value;
		if(reload==false)
		{
			if(svar=="vlcdir")
				vlcdir=StringToWstring(value);
			else if(svar=="mpg123path")
				mpg123path=StringToWstring(value);
			else if(svar=="dbpath")
				dbpath=StringToWstring(value);
			else if(svar=="musicpath")
				MusicPaths.push_back( StringToWstring(value) );
			else if(svar=="ignore")
				IgnoreList.push_back( WstringToLower(StringToWstring(value)) );
			else if(svar=="maxspeed")
				MaxSpeed = ToDouble(value);
			else if(svar=="minspeed")
				MinSpeed = ToDouble(value);
			else if(svar=="mindfuckchance")
				MindFuckChance = ToDouble(value);
			else if(svar=="numtracks")
				NumTracks = ToDouble(value);
			else if(svar=="cutofflength")
				CutoffLength = ToUInt(value)*44100*2;
			else if(svar=="starttime")
				StartTime = ToUInt(value)*44100*2;
			else if(svar=="setlength")
				SetLength = ToUInt(value);
			else
				cout << "\t\tunknown variable! - ";
		}
		else if(reload==true)
		{
			if(svar=="maxspeed")
				MaxSpeed = ToDouble(value);
			else if(svar=="minspeed")
				MinSpeed = ToDouble(value);
			else if(svar=="mindfuckchance")
				MindFuckChance = ToDouble(value);
			else if(svar=="numtracks")
				NumTracks = ToDouble(value);
			else if(svar=="cutofflength")
				CutoffLength = ToUInt(value)*44100*2;
			else if(svar=="starttime")
				StartTime = ToUInt(value)*44100*2;
			else if(svar=="setlength")
				SetLength = ToUInt(value);
		}
		//cout << "\t\t" << svar << " = " << value<<"\n";
	}
	//cout << "\t\tdone loading config\n\n\n";
}

class Track;

double GetSetInterval(double tempo, uint beatspermeasure)
{
	double SetTempo = tempo;
	SetTempo=(88200.0*60.0)/SetTempo;
	SetTempo *= (double)beatspermeasure;
	return SetTempo;
}

class Effect
{
public:
	string name;

	Effect()
	{
		name="Effect";
	}

	virtual ~Effect()
	{
	}

	virtual uint Play(uint pos, Track *parent, uint slot, double buff[], unsigned int bufflen)=0;

	virtual bool IsDone(Track *parent, uint slot);
	virtual void ToJSON(string &json, vector<shared_ptr<Effect> > &effects, uint slot);
	virtual double SongCount(double currcount, Track *parent, uint slot);
};

class ESongPlayer : public Effect
{
private:
	ESongPlayer(ESongPlayer &old)
	{
		assert(0);
	}

	ESongPlayer()
	{
		assert(0);
	}

public:
	SongPlayer player;
	uint Pos;
	double vol;

	ESongPlayer(wstring path, uint offset) : player(path)
	{
		Song &s = Songs[path+salt];
		name="ESongPlayer";
		if(s.avgvol==0)
			s.avgvol=medvolume*2;

		vol = (double)medvolume / (double)s.avgvol;
		double peakvol = (double)medvolume / (double)s.peakavgvol;
		peakvol *= 1.5;

		cout << PathToDisplay(path) << " with vol == "<<vol<<", peakvol == "<<peakvol<<"\n";

		vol = min(vol,peakvol);
		//vol *= 0.8;
		vol = min(vol, 0.95);
		//vol = pow(vol, 1.5);
		Pos=0;
		double beat = (60.0/s.tempo*44100);
		uint beats = (uint)((StartTime/2)/(60.0/s.tempo*44100));
		if (offset + 1000 < 1000) {
			offset = 0;
		}
		uint start = offset;// +(uint)((double)beats*beat);
		start *= 2;
		for(uint i=0;i<start && player.file;i+=BlockSize)
		{
			short buff[BlockSize];
			player.Read(buff, min(start, BlockSize));
		}
	}

	ESongPlayer(ESongPlayer &&old) : player(std::move(old.player))
	{
		Pos=old.Pos;
		vol=old.vol;
	}

	virtual void ToJSON(string &json, vector<shared_ptr<Effect> > &effects, uint slot)
	{
		//json += ",\"pos\":\"" + ToString(Pos) + "\",\"vol\":\"" + ToString(vol) + "\"";
		if(player.file)
		{
			if(json.length()>0 && json[json.length()-1] == '}')
				json += ", ";
			json += "{ \"name\":\"" + WstringToString(player.filename) + "\" }";
		}
		if(slot>0)
			effects[slot-1]->ToJSON(json, effects, slot-1);
	}

	virtual uint Play(uint pos, Track *parent, uint slot, double buff[], unsigned int bufflen)
	{
		uint played=0;
		uint lastread=0;
		uint read=0;

		while(played<bufflen)
		{
			short sbuff[BlockSize];
			memset(sbuff, 0, sizeof(sbuff[0])*BlockSize);
			lastread = player.Read(sbuff, min(bufflen-played, BlockSize));
			read += lastread;
			Pos += lastread;
			for(uint i=0;played<bufflen && i<BlockSize;i++, played++)
				buff[played] = (((double)sbuff[i])/32768.0) * vol;
		}
		return read;
	}

	virtual double SongCount(double currcount, Track *parent, uint slot)
	{
		if(player.file != NULL)
			currcount += 1.0;
		return currcount;
	}

	virtual bool IsDone(Track *parent, uint slot)
	{
		if(player.file == NULL)
			return true;
		else
			return false;
	}
};

class FadeOut : public Effect
{
public:
	double vol;
	double incr;

	FadeOut(double seconds, double leadinsecs=0)
	{
		name="FadeOut";
		vol=1.0;
		incr=1.0/seconds/44100.0;
		vol += leadinsecs*44100.0 * incr;
	}

	virtual uint Play(uint pos, Track *parent, uint slot, double buff[], unsigned int bufflen);
	bool IsDone(Track *parent, uint slot);
	virtual double SongCount(double currcount, Track *parent, uint slot);
};

class FadeIn : public Effect
{
public:
	double vol;
	double incr;

	FadeIn(double seconds, double leadinsecs=0)
	{
		name="FadeIn";
		vol=0.0;
		incr=1.0/seconds/44100.0;
		vol -= leadinsecs*44100.0 * incr;
	}

	virtual uint Play(uint pos, Track *parent, uint slot, double buff[], unsigned int bufflen);
};

class ScaleSong;

class Track
{
public:
	Song song;
	//SongPlayer player;
	double *settempo;
	uint Pos;

	vector<shared_ptr<Effect> > effects;

	virtual string ToJSON()
	{
		string ret;
		//if(Is
		//ret = "{ \"name\":\"" + PathToDisplay(song.filepath) + "\" }";
		//string ret = "{ \"name\":\"" + WstringToString(song.filepath) + "\", \"tempo\":\"" + ToString(song.tempo) + "\"";
		//ret += ",\"length\":\"" + ToString(song.length) +"\",\"beatspermeasure\":\"" + ToString(song.BeatsPerMeasure) + "\"";
		effects.back()->ToJSON(ret, effects, (uint)effects.size()-1);
		//ret += "}";
		return ret;
	}

	Track(Song s, int offset, double scale, double *SetTempo);

	virtual ~Track()
	{
	}

	double SongCount()
	{
		double currcount = 0.0;
		if(Done())
			currcount=0.0;
		currcount = effects.back()->SongCount(currcount, this, (uint)effects.size()-1);
		return currcount;
	}

	vector<shared_ptr<Effect> > FindEffects(string name)
	{
		vector<shared_ptr<Effect> > ret;
		for(uint i=0;i<effects.size();i++)
			if(effects[i]->name == name)
				ret.push_back(effects[i]);
		return ret;
	}

	virtual bool Done()
	{
		/*if(Pos>CutoffLength && FindEffects("FadeOut").size())
		{
		effects.push_back( make_shared<FadeOut>(FadeOut(5.0)));
		//song.length = Pos/2;
		//song.Save();
		//Songs[song.filepath].length = song.length;
		//return true;
		}*/
		return effects.back()->IsDone(this, (uint)effects.size()-1);
	}

	virtual void Play(uint pos, double (&buff)[BlockSize])
	{
		//if(Pos>CutoffLength && FindEffects("FadeOut").size()==0)
		//effects.push_back( make_shared<FadeOut>(FadeOut(10.0)));
		for(uint played=0;played<BlockSize;)
			played += effects.back()->Play(pos+played, this, (uint)effects.size()-1, buff+played, BlockSize-played);
		Pos += BlockSize;
	}

};

double Effect::SongCount(double currcount, Track *parent, uint slot)
{
	if(slot==0)
		return currcount;
	return parent->effects[slot-1]->SongCount(currcount, parent, slot-1);
}

bool Effect::IsDone(Track *parent, uint slot)
{
	if(slot==0)
		return true;
	return parent->effects[slot-1]->IsDone(parent, slot-1);
}

void Effect::ToJSON(string &json, vector<shared_ptr<Effect> > &effects, uint slot)
{
	if(slot>0)
		effects[slot-1]->ToJSON(json, effects, slot-1);
}

double FadeOut::SongCount(double currcount, Track *parent, uint slot)
{
	//currcount*=0.5;
	if(slot==0)
		return currcount;
	return parent->effects[slot-1]->SongCount(currcount, parent, slot-1);
}

uint FadeOut::Play(uint pos, Track *parent, uint slot, double buff[], unsigned int bufflen)
{
	uint played=0;
	while(played<bufflen)
	{
		uint amountread = parent->effects[slot-1]->Play(pos, parent, slot-1, &buff[played], bufflen-played);
		uint end = (played+amountread)/2;
		if(vol<1.0)
		{
			for(uint i=played;i<end;i++)
			{
				buff[i*2] *= vol;
				buff[i*2+1] *= vol;
				vol -= incr;
				vol = max(vol,0.0);
			}
		}
		else
			vol -= incr * (double)amountread;

		played+=amountread;
	}
	return played;
}

uint FadeIn::Play(uint pos, Track *parent, uint slot, double buff[], unsigned int bufflen)
{
	uint played=0;
	while(played<bufflen)
	{
		uint amountread = parent->effects[slot-1]->Play(pos, parent, slot-1, &buff[played], bufflen-played);
		uint end = (played+amountread)/2;
		if(vol<0.999)
		{
			for(uint i=played;i<end;i++)
			{
				buff[i*2] *= max(0.0, vol);
				buff[i*2+1] *= max(0.0, vol);
				vol += incr;
				vol = min(vol,1.0);
			}
		}

		played+=amountread;
	}
	return played;
}

bool FadeOut::IsDone(Track *parent, uint slot)
{
	if(vol<0.0001)
		return true;
	if(slot==0)
		return true;
	return parent->effects[slot-1]->IsDone(parent, slot-1);
}


class ScaleSong : public Effect
{
public:
	uint Pos;
	uint tbufflen;
	double tbuff[BlockSize];
	double d;
	double base_scale;
	uint MyCutoffLength;

	ScaleSong()
	{
		base_scale = 1.0;
		name="ScaleSong";
		d=0.0;
		Pos = 0;
		tbufflen=0;
		MyCutoffLength=CutoffLength;
		memset(tbuff, 0, sizeof(tbuff[0])*BlockSize);
	}

	ScaleSong(double scale)
	{
		base_scale = scale;
		name = "ScaleSong";
		d = 0.0;
		Pos = 0;
		tbufflen = 0;
		MyCutoffLength = CutoffLength;
		memset(tbuff, 0, sizeof(tbuff[0])*BlockSize);
	}

	virtual bool IsDone(Track *parent, uint slot)
	{
		//if(tbufflen>
		//if(Pos>CutoffLength)
		//return true;
		if(slot==0)
			return true;
		return parent->effects[slot-1]->IsDone(parent, slot-1);
	}

	virtual double SongCount(double currcount, Track *parent, uint slot)
	{
		//if(Pos>CutoffLength/2)
		//{
		//if(currcount>0.1)
		//}
		if(slot==0)
			return currcount;
		currcount = parent->effects[slot-1]->SongCount(currcount, parent, slot-1);
		//if(Pos<CutoffLength)
		{
			currcount -= pow((double)min(Pos,MyCutoffLength)/(double)MyCutoffLength, 2.0);
			currcount += 0.1;
			//currcount += 1.0;
			//currcount /= 2.0;
		}
		if(currcount<0.1)
		{
			//cout << "WTF! "<<currcount<<", "<<Pos<<"/"<<MyCutoffLength<<"\n";
			currcount=0.1;
		}
		return currcount;
	}

	virtual void ToJSON(string &json, vector<shared_ptr<Effect> > &effects, uint slot)
	{
		//json += ",\"effect\":\"Scaled Song\"";
		if(slot>0 && Pos<CutoffLength)
			effects[slot-1]->ToJSON(json, effects, slot-1);
	}

	virtual uint Play(uint pos, Track *parent, uint slot, double buff[], unsigned int bufflen)
	{
		double scale = base_scale;// *parent->settempo / parent->song.tempo;
		//cout << WstringToString(parent->song.filepath) << " scale == "<<scale<<"\n";
		//double volume = 0.5;
		uint played=0;

		uint out=0;
		if(Pos==0)
		{
			MyCutoffLength=min(MyCutoffLength, (uint)((double)(parent->song.length-(44100*2*15)-StartTime)*scale));
			double SetInterval = GetSetInterval(*parent->settempo, parent->song.BeatsPerMeasure);
			//while((pos+out)%(SetInterval) >= 2 && out<bufflen)
			//out+=2;//wait until the next measure(or beat without the *4)
			while( fmod((double)(pos+out), SetInterval) >= 2 && out<bufflen)
				out+=2;
		}
		played=out;

		if(Pos>MyCutoffLength && (parent->effects.size()==slot+1 || parent->effects[slot+1]->name!="FadeOut") )
		{
			auto old = parent->effects;
			parent->effects.clear();
			for(uint i=0;i<=slot;i++)
				parent->effects.push_back(old[i]);
			parent->effects.push_back( make_shared<FadeOut>(FadeOut(15.0)) );
			for(uint i=slot+1;i<old.size();i++)
				parent->effects.push_back(old[i]);
		}

		if(tbufflen==0 && parent->effects[slot-1]->IsDone(parent,slot-1))
			return bufflen;

		while(played<bufflen)
		{
			uint ret = 1;
			while(tbufflen<BlockSize && ret)
			{
				//ret = song->Read(&tbuff[tbufflen], BlockSize-tbufflen);
				ret = parent->effects[slot-1]->Play(pos, parent, slot-1, &tbuff[tbufflen], BlockSize-tbufflen);
				tbufflen+=ret;
				if(tbufflen==0)
					return bufflen;
				//if(parent->effects[slot-1]->IsDone(parent,slot-1))
					//return bufflen;
				//dpos += (double)(ret/2);
			}

			double e = (double)tbufflen/2.0;
			//double e = dpos;
			uint oldout = played;
			uint i;//=0;

			//d = 0.0;
			double n = 0.0;
			for(;played<bufflen && d<e; played+=2, d+=scale)
			{
				n=d+scale;
				double l = 0.0;
				double r = 0.0;
				uint upto = (uint)(n);
				uint start = (uint)d;
				upto = max(upto, start+1);
				upto = min(upto, tbufflen/2);
				for(i=start; i<upto; i++)
				{
					l += tbuff[i*2];
					r += tbuff[i*2+1];
				}
				l /= (double)(upto-start);
				r /= (double)(upto-start);
				//l*=volume;
				//r*=volume;

				buff[played] = l;
				buff[played+1] = r;
			}
			i*=2;
			memmove(tbuff, &tbuff[i], (tbufflen - i)*sizeof(tbuff[0]) );
			tbufflen -= i;
			d -= (double)(i/2);
			Pos+=i;
		}
		return bufflen;
	}
};

Track::Track(Song s, int offset, double scale, double *SetTempo) : song(s)//, player(s.filepath)
{
	settempo = SetTempo;
	Pos = 0;
	effects.push_back(make_shared<ESongPlayer>(ESongPlayer(s.filepath, offset)));
	effects.push_back(make_shared<ScaleSong>(ScaleSong(scale)));

	cout << "playing " << PathToDisplay(s.filepath) << " with scale==" << scale << ", offset==" << offset << "\n";
}

class MindFuck : public Effect
{
public:
	Track trackb;
	uint Pos;
	//uint pattern[64];
	uint tpattern[64];
	double pattern[64];
	uint pattbase[64];
	uint pattlen;
	bool halfdone;

	double vola;
	double volb;

	virtual void ToJSON(string &json, vector<shared_ptr<Effect> > &effects, uint slot)
	{
		//json += ",\"pos\":\"" + ToString(Pos) + "\",\"vol\":\"" + ToString(vol) + "\"";
		//json += ",\"songb\":\"" + PathToDisplay(trackb.song.filepath) + "\"";
		//json += ",\"songb\":" + trackb.ToJSON();
		if(slot>0)
			effects[slot-1]->ToJSON(json, effects, slot-1);
		if(trackb.Done()==false)
		{
			string json2 = trackb.ToJSON();
			if(json.length()>0 && json[json.length()-1] == '}' && json2.length()>0)
				json += ", ";
			json += json2;
		}
	}

	virtual bool IsDone(Track *parent, uint slot)
	{
		//if(tbufflen>
		if(trackb.Done()==true && (slot==0 || parent->effects[slot-1]->IsDone(parent, slot-1)==true) )
			return true;
		return false;
		/*if(slot==0)
		return true;
		return parent->effects[slot-1]->IsDone(parent, slot-1);*/
	}

	void MakePattBase()
	{
		for(uint i=0;i<64;i+=2)
		{
			pattbase[i]=rand()%2;
			pattbase[i+1]=pattbase[i];
		}

		for(uint i=16;i<24;i++)
			pattbase[i]=(i/2)%2;
		for(uint i=24;i<32;i++)
			pattbase[i]=i%2;

		for(uint i=32;i<36;i++)
			pattbase[i]=(i/2)%2;//1 instead of 0, so it progresses towards the second song?
		for(uint i=36;i<40;i++)
			pattbase[i]=(i/2 +1)%2;//1

		for(uint i=40;i<48;i++)
			pattbase[i]=pattbase[i%8];

		for(uint i=48;i<56;i++)
			pattbase[i]=(i/2 +1)%2;
		for(uint i=56;i<64;i++)
			pattbase[i]=rand()%2;//(i+1)%2;

		for(uint i=8;i<16;i++)
			pattbase[i]=pattbase[(i*2)%16];
		for(uint i=16;i<48;i++)
			pattbase[i]=pattbase[(i)%8+56];
	}

	void MakePattern(double tempo)
	{
		pattlen=16;
		for(uint i=0;i<64;i++)
			pattern[i]=0.2;
		MixupPattern(tempo);
	}

	void MixupPattern(double tempo)
	{
		uint size=4;
		uint i=rand()%(pattlen/size);
		i*=size;
		uint s=rand()%(64/size);
		s*=size;
		if((rand()%4)==0)
		{
			for(uint a=0;a<size;a++,i++)
			{
				pattern[i]=((double)pattbase[s+a])*0.33 + 0.33;
			}
		}
		else
		{
			for(uint a=0;a<size;a++,i++)
			{
				pattern[i]+=(((double)pattbase[s+a]))*0.3 - 0.15;
				pattern[i]=min(pattern[i],0.66);
				pattern[i]=max(pattern[i],0.33);
			}
		}
		PrintPattern();
	}

	void PrintPattern()
	{
		cout << "new pattern == ";
		for(uint i=0;i<pattlen;i++)
			cout << (int)(pattern[i]*10.0+0.5) <<",";
		cout << "\n";
	}

	MindFuck(Song b, double *tempo) : trackb(b, 0, 1.0, tempo)
	{
		volb=vola=1.0;
		halfdone=false;
		name="MindFuck";
		trackb.effects.push_back( make_shared<ScaleSong>(ScaleSong((*tempo)/b.tempo)) );
		//trackb.effects.push_back( make_shared<FadeIn>(FadeIn(10.0,0.0)) );
		Pos=0;
		memset(pattern, 0, sizeof(pattern[0])*64);
		pattlen=64;
		//for(uint i=0;i<pattlen;i++)
			//pattern[i] = (i/2)%2;
		for(uint i=0;i<64;i++)
			tpattern[i]=0;

		MakePattBase();
		MakePattern(*tempo);
	}

	virtual ~MindFuck()
	{
	}

	virtual double SongCount(double currcount, Track *parent, uint slot)
	{
		currcount += trackb.SongCount();
		//currcount *= 0.75;
		if(slot!=0)
			currcount = parent->effects[slot-1]->SongCount(currcount, parent, slot-1);
		//currcount *= 0.75;
		return currcount;
	}

	virtual uint Play(uint pos, Track *parent, uint slot, double buff[], unsigned int bufflen)
	{
		uint played=0;

		uint out=0;
		/*if(Pos==0)
		{
		double SetInterval = GetSetInterval(*parent->settempo, parent->song.BeatsPerMeasure);
		//while((pos+out)%(SetInterval) >= 2 && out<bufflen)
		//out+=2;//wait until the next measure(or beat without the *4)
		while( fmod((double)(pos+out), SetInterval) >= 2 && out<bufflen)
		out+=2;
		}*/
		played=out;

		double bufft[2][BlockSize];
		memset(bufft[0], 0, sizeof(bufft[0][0])*BlockSize);
		parent->effects[slot-1]->Play(pos, parent, slot-1, bufft[0], bufflen);

		memset(bufft[1], 0, sizeof(bufft[1][0])*BlockSize);
		trackb.Play(pos, bufft[1]);

		if(halfdone==false)
		{
			if(trackb.Done() || trackb.effects.back()->name=="FadeOut")
			{
				for(uint i=0;i<64;i++)
					pattern[i]=0.0;
				halfdone=true;
				PrintPattern();
			}
			else if(parent->effects[slot-1]->IsDone(parent, slot-1) || parent->effects[slot-1]->name=="FadeOut")
			{
				for(uint i=0;i<64;i++)
					pattern[i]=1.0;
				halfdone=true;
				PrintPattern();
			}
		}
		if(halfdone==false && (rand()%512)==0)
		{
			cout << "Mixing up MindFuck pattern!\n";
			MixupPattern(*parent->settempo);
		}

		double tempo = *parent->settempo;
		while(tempo>400.0)
			tempo/=2.0;
		while(tempo<200.0)
			tempo*=2.0;

		for(uint s=0;s<bufflen;s++)
		{
			vola -= vola/(44100.0/2.0);
			vola += abs(bufft[0][s]);///(44100.0/4.0);

			volb -= volb/(44100.0/2.0);
			volb += abs(bufft[1][s]);///(44100.0/4.0);
		}

		/*uint tpattern[64];
		memcpy(tpattern,pattern,sizeof(tpattern[0])*64);
		if(vola < volb*0.5)
			for(uint i=0;i<64;i++)
				tpattern[i]=1;
		else if(volb < vola*0.5)
			for(uint i=0;i<64;i++)
				tpattern[i]=0;*/

		double SetInterval = GetSetInterval(tempo, 1);
		for(;played<bufflen;)
		{
			double p = 0.0;
			p = ((double)(pos+played)/SetInterval +0.0025);

			uint start = (uint)((((double)(uint)p)-0.005)*SetInterval);
			uint end=(uint)((((double)(uint)p)+0.005)*SetInterval);
			//uint start = (uint)((((double)(uint)p))*SetInterval);
			//uint end=(uint)((((double)(uint)p)+1.0)*SetInterval);

			uint prevsong = tpattern[((uint)p+pattlen-1)%pattlen];
			uint song = tpattern[((uint)p)%pattlen];

			tpattern[((uint)p+pattlen+1)%pattlen]=1337;

			if(song==1337)
			{
				double minvola = pattern[((uint)p)%pattlen];
				double minvolb = 1.0 - minvola;

				if(vola*minvolb < volb*minvola)
					song=tpattern[((uint)p)%pattlen]=1;
				else
					song=tpattern[((uint)p)%pattlen]=0;
			}

			if(played+pos<end && pos+bufflen>start && pos+played<start)
			{
				end=start-(played+pos);
			}
			else if(played+pos<end && pos+bufflen>start)
			{
				for(;played+pos<end && played<bufflen;played+=2,Pos+=2)
				{
					double vol = ((double)(pos+played)-(double)start)/(double)(end-start);
					vol=min(vol,1.0);
					vol=max(vol,0.0);
					buff[played] = bufft[song][played]*vol;
					buff[played+1] = bufft[song][played+1]*vol;

					vol = 1.0 - ((double)(pos+played)-(double)start)/(double)(end-start);
					vol=min(vol,1.0);
					vol=max(vol,0.0);
					buff[played] += bufft[prevsong][played]*vol;
					buff[played+1] += bufft[prevsong][played+1]*vol;
				}
				continue;
			}
			else
				end=min(end,bufflen);

			for(;played<end;played+=2,Pos+=2)
			{
				buff[played] = bufft[song][played];
				buff[played+1] = bufft[song][played+1];
			}
		}

		/*for(;played<bufflen;played+=2,Pos+=2)
		{
			uint p = 0;
			p = (uint)((double)(pos+played)/SetInterval +0.0025);
			p %= pattlen;
			uint song = tpattern[p];

			buff[played] = bufft[song][played];
			buff[played+1] = bufft[song][played+1];
		}*/
		return bufflen;
	}
};

class Set
{
public:
	time_t created;
	string key;
	double currtracks;
	vector<Track> tracks;
	double tempo;
	uint beatspermeasure;

	/*virtual bool IsDone(Track *parent, uint slot)
	{
	if(slot==0)
	return true;
	return parent->effects[slot-1]->IsDone(parent, slot-1);
	}*/

	pair<Song, pair<int,double> > GetSongMatchV2(string key, double tempo, uint beatspermeasure, Song currsong)
	{
		pair<Song, pair<int, double> > ret;
		
		vector<Song> matches;
		vector<std::pair<int, double>> matches_info;
		//return ret;
		double slowerspeed = 0.75;
		if ((rand() % 2) == 0)
			slowerspeed = 1.0;

		for (auto x = Songs.begin(); x != Songs.end(); ++x)
		{
			Song t = x->second;
			if (t.length<44100 * 2 * 60)
				continue;//must be at least 1 minute long
			if (t.filepath == currsong.filepath) continue;

			double scale = tempo / x->second.tempo;
			//if(scale < MinSpeed)
			//scale /= slowerspeed;//just about 45 rpm down to 33 1/3, exactly 5 semitones lower

			if (t.length > 44100 * 60 * 20 * 2)//must be less than 20 minutes long
				scale = 0.0;
			if (t.last_played >= created)
				scale = 0.0;

			if (scale > MinSpeed && scale < MaxSpeed && key == t.key /*&& beatspermeasure == t.BeatsPerMeasure*/) {
				matches.push_back(t);
				matches_info.push_back(pair<int, double>(0, scale));
			}
		}

		/*for (uint i = 0; i < matches.size(); i++) {
			matches_info[i] = GetSongOffsetScale(currsong, matches[i]);
		}*/

		if (matches.size() > 0) {
			auto i = rand() % matches.size();
			ret.first = matches[i];
			double suggestedscale = tempo / matches[i].tempo;
			if (currsong.filepath == matches[i].filepath) suggestedscale *= 2.0;
			matches_info[i] = GetSongOffsetScale(currsong, matches[i], suggestedscale);
			ret.second.first = matches_info[i].first;
			ret.second.second = matches_info[i].second;
		}

		Songs[ret.first.filepath + salt].last_played = created;
		ret.first.last_played = created;
		if(ret.first.tempo>1) cout << "\n\nFound match " << WstringToString(ret.first.filepath) << " tempo == " << ret.first.tempo << " set tempo == " << tempo << "\n\n";
		return ret;
	}

	Song GetSongMatch(string key, double tempo, uint beatspermeasure)
	{
		Song ret;
		vector<Song> matches;
		//return ret;
		double slowerspeed = 0.75;
		if((rand()%2)==0)
			slowerspeed=1.0;

		for(auto x=Songs.begin();x!=Songs.end();++x)
		{
			Song t = x->second;
			if(t.length<44100*2*60)
				continue;//must be at least 1 minute long

			double scale = tempo / x->second.tempo;
			//if(scale < MinSpeed)
			//scale /= slowerspeed;//just about 45 rpm down to 33 1/3, exactly 5 semitones lower

			if(t.length > 44100*60*10*2)//must be less than 10 minutes long
				scale = 0.0;
			if(t.last_played >= created)
				scale = 0.0;

			if(scale > MinSpeed && scale < MaxSpeed && key == t.key /*&& beatspermeasure == t.BeatsPerMeasure*/)
				matches.push_back( t );
		}

		if(matches.size()>0)
			ret = matches[rand()%matches.size()];
		Songs[ret.filepath+salt].last_played=created;
		ret.last_played=created;
		cout << "\n\nFound match "<<WstringToString(ret.filepath)<<" tempo == "<<ret.tempo<<" set tempo == "<<tempo<<"\n\n";
		return ret;
	}

	/*uint GetSetInterval(double tempo, uint beatspermeasure)
	{
	double SetTempo = tempo;
	SetTempo=(88200.0*60.0)/SetTempo;
	SetTempo *= (double)beatspermeasure;
	return (uint)SetTempo;
	}*/

	void UpdateCurrent()
	{
		static time_t lasttime = 0;
		if(lasttime>=time(0))
			return;
		if(currenttemplate.length()==0)
			return;
		lasttime=time(0);

		string thtml = "$sourcedata$";//currenttemplate;
		string first = string(thtml.c_str(), strstr(thtml.c_str(), "$sourcedata$")-thtml.c_str());
		string second = strstr(thtml.c_str(), "$sourcedata$") + strlen("$sourcedata$");

		string json = "{ \"songs\": [ ";
		string songs;
		for(uint i=0;i<tracks.size();i++)
		{
			string song;
			if(songs.length()>0 && songs[songs.length()-1] == '}')
				song += ",";
			song += tracks[i].ToJSON();
			songs += song;
		}
		json += songs;
		json += "], \"tempo\":\"" + ToString(tempo) + "\", \"currtracks\":\"" + ToString(currtracks/TRACKAVGRATE) + "\"";
		json += ", \"SetName\":\"" + ToString(created) + "\"";
		json += ", \"SetLength\":\"" + ToString(SetLength+created) + "\"";
		json += ", \"MindFuckChance\":\"" + ToString((int)(MindFuckChance*100.0)) + "\"";

		json += "}";
		//json = str_replace(json.c_str(), "\\", "\\\\\\\\");
		//json = str_replace(json.c_str(), "\"", "\\\"");

		//json = WhitelistCharacters(json, "{}[],:- \\\"\'\n.");
		json = str_replace(json.c_str(), "\\", "\\\\");
		json = str_replace(json.c_str(), "\n", "\\n");

		thtml = first + json + second;

		htmllock.lock();
		html=thtml;
		htmllock.unlock();
		/*{//make sure the file is cleared and written to asap
			ofstream out( (dbpath + L"current/current.html") );
			out.write(html.c_str(), html.length());
		}*/
	}

	void Play()
	{
		created = time(0);
		currtracks = 200.0 * NumTracks * TRACKAVGRATE;
		//if currtracks is ever equal to 0 then maybe the set should kill itself and start a new one?

		key = "C";
		tempo = 0.0;
		beatspermeasure = 4;

		std::mutex songsearchmutex;
		Song currentsong;
		vector<Song> upcomingsongs;
		vector<std::pair<int, double>> upcomingsongs_info;
		while(tempo<1.0)
		{
			songsearchmutex.lock();
			Song s = RandomSong();
			currentsong = s;
			tempo = s.tempo;
			key = s.key;
			beatspermeasure = s.BeatsPerMeasure;
			upcomingsongs.push_back(s);
			upcomingsongs_info.push_back(pair<int, double>(0, 1.0));
			songsearchmutex.unlock();
		}

		uint pos=0;
		short buff[BlockSize];
		double dbuff[BlockSize];

		std::thread songsearches[3];
		uint seed = 323;
		for (auto &t : songsearches) {
			t = std::thread([this, &songsearchmutex, &currentsong, &upcomingsongs, &upcomingsongs_info, &seed](){
				srand(seed++);
				uint upcomingsongscount = 0;
				while (upcomingsongscount <2) {
					songsearchmutex.lock();
					auto currsong = currentsong;
					upcomingsongscount = upcomingsongs.size();
					songsearchmutex.unlock();
					if (upcomingsongscount <2) {
						auto r = GetSongMatchV2(key, tempo, beatspermeasure, currsong);
						if (r.first.tempo>1 && r.second.second > 0.0 && r.second.second < 10.0) {
							songsearchmutex.lock();
							bool found = false;
							for (auto &s : upcomingsongs) {
								if (s.filepath == r.first.filepath) found = true;
							}
							if (found == false) {
								upcomingsongs.push_back(r.first);
								upcomingsongs_info.push_back(r.second);
							}
							songsearchmutex.unlock();
						}
					}
					RaySleep(1000 * 1000);
				}
			});
		}

		//songsearch.join();
		for (auto &t : songsearches) t.join();
		created = time(0);

		while(1)
		{
			//memset(buff, 0, sizeof(buff[0])*BlockSize);
			memset(dbuff, 0, sizeof(dbuff[0])*BlockSize);
			//for(uint i=0;i<BlockSize;i++)
			//dbuff[i] = 0.0;

			//assert(currtracks>-0.01);
			if( currtracks/TRACKAVGRATE < NumTracks && (double)tracks.size() < NumTracks+1.5 && time(0) - created < SetLength)
			{
				//cout << "\n\n====================\n";
				ReadConfig(true);
				if(tracks.size()>0 && tracks.back().effects.size()<10 && (double)(rand()%10001)/10000.0 < MindFuckChance)
				{
					tracks.back().effects.push_back( make_shared<MindFuck>(MindFuck(GetSongMatch(key, tempo, beatspermeasure), &tempo)) );

					double numtracks = 0;
					if (tracks.size()) numtracks = tracks.back().SongCount();
					currtracks += min(TRACKAVGRATE*0.5, (TRACKAVGRATE*numtracks) * 0.8);
				}
				else
				{
					//auto r = GetSongMatchV2(key, tempo, beatspermeasure);
					songsearchmutex.lock();
					/*while (upcomingsongs.size() > 0 && upcomingsongs.back().tempo < 3) {
						upcomingsongs.pop_back();
					}*/
					if (upcomingsongs.size() > 0) {
						tracks.push_back(Track(upcomingsongs.back(), upcomingsongs_info.back().first, upcomingsongs_info.back().second, &tempo));
						currentsong = upcomingsongs.back();
						//tracks.back().effects.push_back( make_shared<ScaleSong>(ScaleSong()) );
						double leadin = 15.0;
						if (tracks.size() == 1)
							leadin = 0.0;//give me some lead in to get the stream started?
						tracks.back().effects.push_back(make_shared<FadeIn>(FadeIn(5.0, leadin)));//15 second extra lead in is to make it equivalent to how mindfucks lead in
						upcomingsongs.pop_back();
						//while (upcomingsongs.size() > 1) upcomingsongs.pop_back();

						double numtracks = 0;
						if (tracks.size()) numtracks = tracks.back().SongCount();
						currtracks += min(TRACKAVGRATE*0.5, (TRACKAVGRATE*numtracks) * 0.8);
					}

					if (upcomingsongs.size() > 0) {
						tracks.push_back(Track(upcomingsongs.front(), upcomingsongs_info.front().first, upcomingsongs_info.front().second, &tempo));
						currentsong = upcomingsongs.front();
						//tracks.back().effects.push_back( make_shared<ScaleSong>(ScaleSong()) );
						double leadin = 15.0;
						if (tracks.size() == 1)
							leadin = 0.0;//give me some lead in to get the stream started?
						tracks.back().effects.push_back(make_shared<FadeIn>(FadeIn(5.0, leadin)));//15 second extra lead in is to make it equivalent to how mindfucks lead in
						upcomingsongs.pop_back();
						//while (upcomingsongs.size() > 1) upcomingsongs.pop_back();
						upcomingsongs.clear();

						double numtracks = 0;
						if (tracks.size()) numtracks = tracks.back().SongCount();
						currtracks += min(TRACKAVGRATE*0.5, (TRACKAVGRATE*numtracks) * 0.8);
					}
					songsearchmutex.unlock();
				}
			}

			for(uint i=0;i<tracks.size();i++)
			{
				if(tracks[i].Done())
				{
					cout << "\nClosing track "<<WstringToString(tracks[i].song.filepath)<<"\n";
					tracks[i] = tracks.back();
					tracks.pop_back();
					i--;
				}
			}

			currtracks -= currtracks/TRACKAVGRATE;
			for(uint i=0;i<tracks.size();i++)
				currtracks += tracks[i].SongCount();

			if(tracks.size()==0 && currtracks/TRACKAVGRATE <= 0.1)
			{
				//songsearch.join();
				return;
			}
			else if(tracks.size()==0 && (currtracks/TRACKAVGRATE > 10000.0 || currtracks/TRACKAVGRATE < -10000.0 || !(currtracks==currtracks)) )
				currtracks=0.0;

			/*if(tracks.size()==1)
			{
			double stempo = tracks[0].song.tempo;
			if(stempo > 185.0)
			stempo *= (100.0/3.0)/45.0;
			tempo = tempo*0.9995 + stempo*0.0005;
			//tempo = tempo*0.999 + stempo*0.001;
			pos = tracks[0].Pos;
			if((rand()%512)==0)
			cout << "new set tempo == "<<tempo<<"\n";
			}*/

			for(uint i=0;i<tracks.size();i++)
			{
				double buff2[BlockSize];
				memset(buff2, 0, sizeof(buff2[0])*BlockSize);
				tracks[i].Play(pos, buff2);
				for(uint s=0;s<BlockSize;s++)
					dbuff[s] += buff2[s];
			}
			double maxvol=0.0;
			for(uint s=0;s<BlockSize;s++)
			{
				maxvol=max (maxvol, abs (dbuff[s]));
				buff[s] = (short)(dbuff[s]*32768.0);
			}
			if (maxvol>1.0)
				cout << "clip!\n";

			pos += BlockSize;
			fwrite(buff, sizeof(buff[0]), BlockSize, out);
			UpdateCurrent();
		}
		//songsearch.join();
	}
};

void HTMLServer()
{
	BasicSocket sock;
	sock.SetSync();
	if(sock.Bind(80)!=1)
	{
		cout << "Failed to bind to port!\n";
		exit(1);
		return;
	}
	if(sock.Listen()!=0)
	{
		cout << "Failed to listen!\n";
		exit(1);
		return;
	}
	cout << "Listening on port!\n";

	while(1)
	{
		BasicSocket acceptsock;
		acceptsock.SetSync();
		if(sock.Accept(acceptsock))
		{
			acceptsock.SetSync();
			char buff[65536];
			acceptsock.Recv(buff, 65536);

			string thtml;
			string contenttype="application/json; charset=utf-8";
			if(strstr(buff, "current.txt")==NULL)
			{
				//cout << "connection! current.html\n";
				htmllock.lock();
				thtml=currenttemplate;
				htmllock.unlock();
				contenttype="text/html; charset=utf-8";
			}
			else
			{
				//cout << "connection! current.txt\n";
				htmllock.lock();
				thtml=html;
				htmllock.unlock();
			}

			string response="HTTP/1.1 200 OK\r\nConnection: close\r\nContent-Type: " + contenttype + "\r\nCache-Control: no-cache\r\n\r\n";// + thtml;
			acceptsock.Send(response.c_str(), (int)response.length());
			thtml = "\x0ef\x0bb\x0bf" + thtml;
			acceptsock.Send((const char*)thtml.c_str(), (int)thtml.length());
			RaySleep(1000);
			acceptsock.Disconnect();
		}
		else
			RaySleep(1000);
	}
}

int main(int argc, char **argv)
{
	srand( (uint)time(0) );
	salt = StringToWstring(ToString((uint)time(0)));

	cout << "salt == "<<WstringToString(salt)<<"\n";
	cout << "CURRENT_DJA_VERSION == "<<CURRENT_DJA_VERSION<<"\n\n";
	ReadConfig();
	{
		Set().UpdateCurrent();//just clear the current.html file before scanning libraries
	}
	thread( HTMLServer ).detach();
	RaySleep(1000*1000);
	
	wstring command;
	command = L"\"\"" + vlcdir + L"\" --meta-title=\"DJ Automica 2\" --play-and-exit --demux=rawaud --rawaud-channels 2 --rawaud-samplerate 44100 -\"";
	
#ifdef WIN32
	out=popen(command.c_str(), L"wb");
	_setmode( _fileno(audioout), _O_BINARY);
#else
	out = popen(command.c_str(), "w");
	//sighandler_t sighand = signal(SIGPIPE, handler);
	//out=stdout;
#endif

	InitNotes();

	//corrupt_files.open( (dbpath + L"corrupt.txt").c_str(), std::ios::app | std::ios::ate);
	corrupt_files.open( (dbpath + L"corrupt.txt").c_str());

	UpdateSongList();
	AnalyzeSongs();
	RaySleep(5*1000*1000);

	while(1)
	{
		//RaySleep(1000 * 1000);
		cout << "\n\n================\nStarting new set!\n\n";
		Set().Play();
		cout << "\n\n================\nSet over, taking a break....\n\n";
		//RaySleep(3000 * 1000);//don't need to sleep because the set starts with currtracks = NumTracks * MAXTRACKAVG
	}

	return 0;
}