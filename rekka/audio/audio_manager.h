#pragma once

#include "rekka.h"
#include "audio/audio.h"
#include <list>
#include <map>

NS_REK_BEGIN

class AudioManager {
private:
	static AudioManager* s_sharedAudioManager;
public:
	AudioManager();
	~AudioManager();
	static AudioManager* getInstance();
	bool initialze(int sampleRate = 44100, int bufferSize = 4096);
	Audio* createAudio(bool isstatic = false);
	void removeAudio(Audio* audio);
	Mix_Chunk* fetchChunk(const std::string& path);
	void releaseChunk(Mix_Chunk* chunk);
private:
	static void finishCallbackStatic(void* userdata, int channel);
	static void finishCallback(void* userdata, Mix_Music *music, int channel);	
private:
	std::list<Audio*> _audios;
	struct ChunkItem {
		Mix_Chunk* chunk;
		int refcount;
	};
	std::map<std::string, ChunkItem> _chunkCache;
};

NS_REK_END