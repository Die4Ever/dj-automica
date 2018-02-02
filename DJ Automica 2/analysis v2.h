#include <algorithm>
const int ANALYZE_DIV = 8;//downscale the audio for faster analysis


double CalcTempoStrengthV2(double t, vector<float> &dbuff, uint start, uint end)
{
	uint offset = (uint)(60.0 / t * 44100.0);
	float ret = 0.0;

	if (start+offset >= dbuff.size()) return 0;
	if (end + offset > dbuff.size()) end = dbuff.size() - offset - 1;

	for (uint s = start; s < end; s++)
	{
		ret += dbuff[s] * dbuff[s + offset];
	}

	ret /= (float)(end);//make it an average to make up for the fact that slower tempos has less samples
	return (double)ret;
}

vector<float> PreProc(vector<short> &buffs, uint div)
{
	vector<float> dbuff;
	dbuff.resize(buffs.size() / 2 / ANALYZE_DIV);

	for (uint s = 0; s < dbuff.size(); s++) {
		float f = 0.0f;
		for (uint i = 0; i < ANALYZE_DIV * 2; i++) {
			f += buffs[(s*ANALYZE_DIV * 2) + i] / float(ANALYZE_DIV * 2);
		}
		dbuff[s] = f/30000.0;
	}

	float prevout = 0.0;
	for (uint s = 0; s < dbuff.size(); s++) {
		float f = dbuff[s];
		//f -= prevout;//do I like this line?
		//prevout = dbuff[s];//easy to remove
		dbuff[s] = abs(f);
	}
	return dbuff;
}

void AnalyzeTempoV2(Song &song, vector<short> &buffs, uint starts)
{
	uint avgcount = 1;
	double avgvol = 0.0;
	for (uint s = 0; s < buffs.size(); s+=2) {
		double f = abs(buffs[s]);
		if (f > 500.0f) {
			avgvol += abs(f);
			avgcount++;
		}
	}
	avgvol /= double(avgcount);
	double avgpeak = 0;
	avgcount = 1;
	for (uint s = 0; s < buffs.size(); s += 2) {
		double f = abs(buffs[s]);
		if (f > avgvol) {
			avgpeak += abs(f);
			avgcount++;
		}
	}
	avgpeak /= double(avgcount);

	vector<float> dbuff = PreProc(buffs, ANALYZE_DIV);
	double besttempo = 120.0;
	double maxstr = -99999.0;
	uint tickspermeasure = 4;

	for (double t = 50.0; t < 400.0; t += 0.25)
	{
		double str = CalcTempoStrengthV2(t / double(ANALYZE_DIV), dbuff, 0, (44100*20)/ANALYZE_DIV );
		//str += CalcTempoStrengthV2(t / double(ANALYZE_DIV), dbuff);

		if (str > maxstr)
		{
			maxstr = str;
			besttempo = t;
			tickspermeasure = 4;// ttickspermeasure;
		}
	}

	song.BeatsPerMeasure = tickspermeasure;
	song.MeasuresPerPhrase = 1;
	song.tempo = besttempo;
	song.offset = 0;
	song.avgvol = (uint)avgvol;
	song.peakavgvol = (uint)avgpeak;
}

double MultiplySongs(vector<float> &a, vector<float> &b, double scale, int offset, int len)
{
	double str = 0;
	int start = 0;
	if (offset < 0) start = abs(offset);//a will need to start ahead of b
	int end = (int)min<>(a.size(), b.size());
	end = (int)min<>(end, len);
	if ((end + offset)*scale > end) end = end/scale - offset;
	end-=2;

	while ((end+offset)*scale >= b.size()) end-=10;//fix this....

	double num = double(end - start);
	for (int i = start; i < end; i++) {
		str += double(a[i] * b[(uint)((i+offset)*scale)]) / num;
	}
	return str;
}

std::pair<int,double> GetSongOffsetScale(Song &a, Song &b, double suggestedscale)
{
	while (suggestedscale < 0.49) suggestedscale *= 2.0;
	while (suggestedscale > 2.01) suggestedscale /= 2.0;

	SongPlayer pA(a.filepath);
	SongPlayer pB(b.filepath);
	std::pair<int, double> offsetScale;

	/*offsetScale.first = 0;
	offsetScale.second = suggestedscale;
	return offsetScale;*/

	vector<short> buffA, buffB;
	buffA.resize(44100 * 2 * 200);
	buffB.resize(44100 * 2 * 200);

	pA.Read(&buffA[0], (uint)buffA.size() / 8);//skip intros...
	pB.Read(&buffB[0], (uint)buffB.size() / 8);

	pA.Read(&buffA[0], (uint)buffA.size());
	pB.Read(&buffB[0], (uint)buffB.size());

	uint div = 32;
	auto fbA = PreProc(buffA, div);
	auto fbB = PreProc(buffB, div);

	double beststr = 0;
	double worststr = 999999999999;
	offsetScale.first = 0;
	offsetScale.second = 999.0;


	//maybe try to find some good peaks in both songs and try to line them up for initial offset finding?
	for (double scale = suggestedscale-0.2; scale < suggestedscale+0.2; scale += 0.02) {
		for (int offset = 0; offset < 44100*2/int(div); offset+=32) {
			double str = MultiplySongs(fbA, fbB, scale, offset, fbA.size());
			if (scale == suggestedscale && offset == 0) cout << "str==" << str << "\n";
			if (str > beststr) {
				beststr = str;
				offsetScale.first = offset*div;
				offsetScale.second = scale;
			}
			if (str < worststr) {
				worststr = str;
			}
		}
	}

	cout << "beststr/worststr == " << (beststr / worststr) << "\n";
	/*if (beststr / worststr < 5) {
		offsetScale.first = 0;
		offsetScale.second = 999.0;
		return offsetScale;
	}*/

	/*int bigoffset = offsetScale.first;
	double bigscale = offsetScale.second;
	for (double scale = bigscale - 0.05; scale < bigscale+0.05; scale += 0.002) {
		for (int offset = bigoffset-1000; offset < bigoffset+1000; offset += 2) {
			double str = MultiplySongs(fbA, fbB, scale, offset, fbA.size());
			if (scale == suggestedscale && offset == 0) cout << "str==" << str << "\n";
			if (str > beststr) {
				beststr = str;
				offsetScale.first = offset*div;
				offsetScale.second = scale;
			}
		}
	}*/

	cout << "beststr==" << beststr << "\n";

	return offsetScale;
}
