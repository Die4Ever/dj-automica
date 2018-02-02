#include <array>
#include <thread>
#include <mutex>

#include <util.h>
#include <oswrapper.h>
#include <interpolation.h>
#include <raystringstuff.h>
#include <rayheap.h>
#include <rayheapnew.h>
#include <raycontainers2.h>
#include <sockets4.h>

#include "FFT2.h"
#include "graph.h"

#ifdef _DEBUG
#define DEBUGCOND( A ) if( (A) ) cerr << #A << " is true\n"; else cerr << #A << " is false\n";
#else
#define DEBUGCOND( A )
#endif

/*#ifdef WIN32
typedef wstring path;
#define PATHPRE L
#else
typedef string path;
#define PATHPRE
#endif*/

FILE *out = NULL;
//#define audioout stdout
#define audioout out

const int CURRENT_DJA_VERSION = 4;
const double samplerate = 44100.0;
const double pi = 3.1415926535897932384626433832795;
const double divisor = samplerate / (pi*2.0);
//const int FFTSize = 4096;
//const int FFTMedSize = 1024; 
//const int FFTSmallSize = 256;

double NoteFreqs[128] = { 0.0 };
unsigned char wnotes[7 * 11] = { 0, 2, 4, 5, 7, 9, 11 };
string NoteNames[128];
int NoteCounts[128] = { 0 };

const int BlockSize = 1024 * 2 * 2;//1024*16;
const int FFTSize = 32768 / 8;//16384;
const int FFTSplit = 1;
const int FFTShortSize = BlockSize / FFTSplit;
const int NUMHIGHS = 128;

double MaxSpeed = 1.0;
double MinSpeed = 0.7;
double MindFuckChance = 0.3;
//double TempoScale = 1.0;//probably just make the tempo start as whatever the tempo of the first song is, and probably the tempo should adjust to whatever is playing when there's only 1 song playing
double NumTracks = 1.3;
double TRACKAVGRATE = 100000.0 / BlockSize;
short medvolume = 2000;
uint CutoffLength = 44100 * 60 * 3 * 2;
uint StartTime = 44100 * 40 * 2;
uint SetLength = 20 * 60;

wstring vlcdir = L"C:\\Program Files (x86)\\VideoLAN\\VLC\\vlc.exe";
wstring dbpath = L"D:\\DJ Automica 3 DB\\";
wstring mpg123path = L"mpg123.exe";
//string MUSICPATH = "I:\\Music\\";
vector<wstring> MusicPaths;
vector<wstring> IgnoreList;
wstring salt;

class Song;
unordered_map<wstring, Song> Songs;

string currenttemplate;
string html;
CriticalSection htmllock;

ofstream corrupt_files;
CriticalSection corrupt_files_lock;

wstring WstringToLower(wstring in)
{
	wstring out = in;
	for (uint i = 0; i<out.length(); i++)
	{
		if (out[i] >= L'A' && out[i] <= L'Z')
			out[i] = out[i] - L'A' + L'a';
	}
	return out;
}

wstring StringToWstring(string in)
{
	wstring out;
	int len = MultiByteToWideChar(CP_ACP, 0, in.c_str(), (int)in.length() + 1, 0, 0);
	wchar_t *buf = new wchar_t[len];
	MultiByteToWideChar(CP_ACP, 0, in.c_str(), (int)in.length() + 1, buf, len);
	out = buf;
	delete[] buf;
	return out;
}

string WhitelistCharacters(string in, const char *allowed)
{
	string out;
	for (uint i = 0; i<in.length(); i++) {
		char c = (char)in[i];
		if ((c >= 'a' && c <= 'z') || (c >= 'A' && c <= 'Z') || (c >= '0' && c <= '9') || strchr(allowed, c))
			out += c;
	}
	return out;
}

string BlacklistCharacters(string in, const char *notallowed)
{
	string out;
	for (uint i = 0; i<in.length(); i++) {
		char c = (char)in[i];
		if (strchr(notallowed, c) == NULL)
			out += c;
	}
	return out;
}

string WstringToString(wstring in)
{
	/*string out;
	const char *allowed = "{}[],:- \\\"\'\n.";
	for(uint i=0;i<in.length();i++) {
	//if(! ((char)in[i] & 0x80) )
	char c=(char)in[i];
	if( (c>='a' && c<='z') || (c>='A' && c<='Z') || (c>='0' && c<='9') || strchr(allowed, c) )
	out += c;
	}
	return out;*/

	int len = WideCharToMultiByte(CP_UTF8, 0, in.c_str(), (int)in.length() + 1, NULL, 0, 0, 0);
	char *buf = new char[len];
	WideCharToMultiByte(CP_UTF8, 0, in.c_str(), (int)in.length() + 1, buf, len, 0, 0);
	string out = buf;
	delete[] buf;
	return out;
}

string PathToDisplay(wstring in)
{
	string s = WstringToString(in);
	const char *last = s.c_str();
	for (const char* p = last; *p; p++)
	{
		if (*p == '/' || *p == '\\')
			last = p;
	}
	s = last;
	return s;
}

FILE * OpenSong(wstring filename)
{
	FILE *file = NULL;
	wstring command;

	//command = L"\"\"" + vlcdir + L"\" \"" + filename + L"\" -A file q --play-and-exit --audiofile-channels=2 --audiofile-format=s16 --aout-rate=44100 --no-audiofile-wav --audiofile-file - --dummy-quiet --intf dummy\"";
	//command = "\"\"" + vlcdir + "\" \"" + filename + "\" -A file q --play-and-exit --audiofile-channels=2 --audiofile-format=s16 --aout-rate=44100 --no-audiofile-wav --intf dummy --dummy-quiet --audiofile-file=\"D:\\test.txt\"\"";
	//command = "\"mpg123.exe  --stdout --stereo -r 44100 -q \"" + filename + "\"\"";
	command = L"\"" + mpg123path + L" -q --stdout --stereo -r 44100 \"" + filename + L"\"\"";

#ifdef WIN32
	file = popen(command.c_str(), L"rb");
	_setmode(_fileno(file), _O_BINARY);
#else
	file = popen(WstringToString(command).c_str(), "r");
#endif
	//Sleep(100);
	if (file == NULL)
	{
		cerr << WstringToString(command) << " not found!\n";
		return NULL;
	}
	//cerr << "Opening "<<WstringToString(command)<<"\n\n";
	return file;
}

#pragma pack(1)
class SongInfov1
{
public:
	int version;
	char key[4];
	int pad;
	double tempo;
	uint BeatsPerMeasure;
	uint MeasuresPerPhrase;
	uint offset;
	uint avgvol;
	uint length;
};
#pragma pack()

class SongInfov3
{
public:
	int version;
	char key[4];
	double tempo;
	uint BeatsPerMeasure;
	uint MeasuresPerPhrase;
	uint offset;
	uint avgvol;
	uint length;

	SongInfov3()
	{
		strcpy(key, "C");
		tempo = 0.0;
		BeatsPerMeasure = 4;
		MeasuresPerPhrase = 1;
		offset = 0;
		avgvol = medvolume;
		length = 0;
		version = 0;//CURRENT_DJA_VERSION;
	}

};

class SongInfov4
{
public:
	int version;
	char key[4];
	double tempo;
	uint BeatsPerMeasure;
	uint MeasuresPerPhrase;
	uint offset;
	uint avgvol;
	uint peakavgvol;
	uint length;

	SongInfov4()
	{
		strcpy(key, "C");
		tempo = 0.0;
		BeatsPerMeasure = 4;
		MeasuresPerPhrase = 1;
		offset = 0;
		avgvol = medvolume;
		peakavgvol = medvolume * 2;
		length = 0;
		version = 0;//CURRENT_DJA_VERSION;
	}

};

wstring PathToDBFile(wstring filepath)
{
	wstring ret;
	//ret += dbpath;
	for (unsigned int i = 0; i<filepath.length(); i++)
	{
		wchar_t c = filepath[i];
		if (c == L'\\' || c == L'/' || c == L':' || c == L'<' || c == L'>' || c == L'?' || c == L'\'' || c == L'\"' || c == L'*' || c == L'*')
			ret += L'-';
		else
			ret += c;
	}
	ret += L".dja";

	if (ret.length()>150)
		ret = ret.substr(ret.length() - 150);
	ret = dbpath + ret;
	return ret;
}

class Song : public SongInfov4
{
public:
	wstring filepath;
	time_t last_played;

	//what else could I store? probably the notes list

	Song()
	{
		last_played = 0;
	}

	Song(wstring Filepath) : filepath(Filepath)
	{
		last_played = 0;
	}

	void Load()
	{
		wstring dbpath = PathToDBFile(filepath);
		//cout << "Loading SongInfo " << WstringToString(dbpath) << "\n";
		//SongInfov1 t;
		ifstream file(dbpath, ios::binary);
		if (!file.good())
			return;
		int tversion = 0;
		file.read((char*)&tversion, sizeof(tversion));
		file.seekg(0);
		if (tversion == 4)
			file.read((char*)(SongInfov4*)this, sizeof(SongInfov4));
		else if (tversion == 3)
		{
			SongInfov3 tsi;
			file.read((char*)&tsi, sizeof(tsi));
			avgvol = tsi.avgvol;
			peakavgvol = avgvol * 2;
			BeatsPerMeasure = tsi.BeatsPerMeasure;
			memcpy(key, tsi.key, sizeof(key));
			length = tsi.length;
			MeasuresPerPhrase = tsi.MeasuresPerPhrase;
			offset = tsi.offset;
			tempo = tsi.tempo;
			version = tsi.version;
		}

		if (length<44100 * 2 * 2)
		{
			string message = WstringToString(filepath) + " is only " + ToString((int)((double)length*1000.0 / (double)(44100 * 2))) + " ms long!\n";
			cout << "\t" << message << "\n";
			corrupt_files_lock.lock();
			corrupt_files.write(message.c_str(), message.length());
			corrupt_files.flush();
			corrupt_files_lock.unlock();
		}
	}

	void Save()
	{
		wstring dbpath = PathToDBFile(filepath);
		cout << "\tSaving SongInfo " << PathToDisplay(dbpath) << " with tempo=="<<tempo<<"\n";
		ofstream file(dbpath, ios::binary);
		file.write((char*)(SongInfov4*)this, sizeof(SongInfov4));
	}
};

class SongPlayer
{
private:
	SongPlayer(SongPlayer &old)
	{
		assert(0);
	}
public:
	FILE *file;
	wstring filename;

	SongPlayer()
	{
		file = NULL;
	}

	SongPlayer(wstring Filename) : filename(Filename)
	{
		if (filename.length() == 0)
			file = NULL;
		else
			file = OpenSong(filename);
	}

	SongPlayer(SongPlayer &&old) : filename(old.filename)
	{
		file = old.file;
		old.file = NULL;
	}

	FILE* OpenStream()
	{
		CloseStream();
		if (filename.length()>0)
			file = OpenSong(filename);
	}

	void CloseStream()
	{
		if (file)
		{
			cerr << "\t-";
			//fclose(file);
			pclose(file);
		}
		file = NULL;
	}

	virtual ~SongPlayer()
	{
		CloseStream();
	}

	uint Read(short *buff, uint length)
	{
		if (file == NULL)
		{
			memset(buff, 0, sizeof(buff[0])*length);
			return 0;
		}

		size_t ret;

		ret = fread(buff, 4, length / 2, file);

		if (ret != length / 2 || ferror(file))
		{
			clearerr(file);
			//fclose(file);
			pclose(file);
			file = NULL;
			//cerr << "Done playing "<< WstringToString(filename)<<"\n";
		}
		return (uint)ret * 2;
	}
};

void FoundSong(wstring filepath)
{
	//cout << filepath+"\n";
	wstring lower = WstringToLower(filepath);
	for (uint i = 0; i<IgnoreList.size(); i++)
	{
		if (wcsstr(lower.c_str(), IgnoreList[i].c_str()))
		{
			cout << "-";
			return;
		}
	}
	cout << "+";
	size_t count = Songs.count(filepath + salt);
	if (count == 0)
		Songs.insert(pair<wstring, Song>(filepath + salt, Song(filepath)));

	auto &s = Songs.at(filepath + salt);
	s.Load();
}

#ifdef WIN32
void CrawlFolder(wstring path, int level = 0)
{
	if (level<3)
		cout << string("\n") + (WstringToString(path) + "\n");

	wchar_t buff[4096];
	swprintf(buff, 4096, L"%ls*", path.c_str());

	WIN32_FIND_DATA FindFileData;
	HANDLE hFind;
	hFind = FindFirstFile(buff, &FindFileData);

	if (hFind == INVALID_HANDLE_VALUE)
	{
		cerr << "CrawlFolder() failed!\n";
		return;
	}

	do
	{
		if (FindFileData.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY)
		{
			if (wcscmp(FindFileData.cFileName, L".") != 0 && wcscmp(FindFileData.cFileName, L"..") != 0)
			{
				CrawlFolder(path + FindFileData.cFileName + L"\\", level + 1);
			}
		}
		else
		{
			wstring filepath = path + wstring(FindFileData.cFileName);
			//if( stricmp(".mp3", filepath.c_str()+filepath.length()-4)==0 )
			if (_wcsicmp(L".mp3", filepath.c_str() + filepath.length() - 4) == 0)
			{
				FoundSong(filepath);
			}
		}
	} while (FindNextFile(hFind, &FindFileData));
}
#else
void CrawlFolder(DIR *dp, string path, int level = 0)
{
	if (level<3)
		cout << (path + "\n");

	dirent *ep;
	while (ep = readdir(dp))
	{
		if (strlen(ep->d_name)>2)
		{
			string filepath = path + "/" + ep->d_name;
			//cerr << filepath << " is type "<<(int)ep->d_type<<"\n";
			//if(songinfos.Get(buff) == NULL)
			if (ep->d_type == 4)
			{
				DIR *sdp = opendir(filepath.c_str());
				if (sdp != NULL)
				{
					CrawlFolder(sdp, filepath, level + 1);
					closedir(sdp);
				}
			}
			else if ((ep->d_type == 0 || ep->d_type == 8) && stricmp(".mp3", filepath.c_str() + filepath.length() - 4) == 0)
			{
				//if( (!SONICMODE) || strstr(filepath.c_str(), "Sonic") )
				//Songs.Create( Song(filepath) );//->Play();
				//cerr << "Created song "<<filepath<<"\n";
				FoundSong(StringToWstring(filepath));
			}
		}
	}
}
#endif

void UpdateSongList()
{
	for (uint i = 0; i<MusicPaths.size(); i++)
	{
#ifdef WIN32
		wstring path = MusicPaths[i];
		CrawlFolder(path);
#else
		string path = WstringToString(MusicPaths[i]);
		DIR *dp;
		dp = opendir(path.c_str());
		if (dp != NULL)
		{
			CrawlFolder(dp, path);
			closedir(dp);
		}
#endif
	}

	cout << "\nDone loading libraries! Found " << Songs.size() << " songs!\n\n";
}

