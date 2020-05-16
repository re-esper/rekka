#include "audio_manager.h"
#include "SDL_mixer.h"

NS_REK_BEGIN

AudioManager* AudioManager::s_sharedAudioManager = nullptr;
AudioManager* AudioManager::getInstance()
{
	if (!s_sharedAudioManager) s_sharedAudioManager = new (std::nothrow) AudioManager();
	return s_sharedAudioManager;
}

bool AudioManager::initialze(int sampleRate, int bufferSize)
{
	if (Mix_OpenAudio(sampleRate, MIX_DEFAULT_FORMAT, 2, bufferSize) < 0) {
		SDL_LogError(0, "Couldn't initialze audio manager: %s", SDL_GetError());
		return false;
	}
	Mix_AllocateChannels(8);
	Mix_ChannelFinished(this, AudioManager::finishCallbackStatic);
	Mix_HookMusicFinishedCh(this, AudioManager::finishCallback);	
	return true;
}

Audio * AudioManager::createAudio(bool isstatic)
{
	Audio* audio = new Audio(isstatic);
	_audios.push_back(audio);
	return audio;
}

void AudioManager::removeAudio(Audio * audio)
{
	for (auto itr = _audios.begin(); itr != _audios.end(); ++itr) {
		if (*itr == audio) {			
			_audios.erase(itr);
			break;
		}
	}
}

Mix_Chunk* AudioManager::fetchChunk(const std::string & path)
{
	auto itr = _chunkCache.find(path);
	if (itr != _chunkCache.end()) {
		itr->second.refcount++;	
		itr->second.chunk->volume = 128; // just hacked
		return itr->second.chunk;
	}
	Mix_Chunk* chunk = Mix_LoadWAV_RW(SDL_RWFromFile(path.c_str(), "rb"), SDL_TRUE);
	_chunkCache[path] = { chunk, 1 };
	return chunk;
}

void AudioManager::releaseChunk(Mix_Chunk* chunk)
{
	for (auto itr = _chunkCache.begin(); itr != _chunkCache.end(); ++itr) {
		if (itr->second.chunk == chunk) {
			itr->second.refcount--;
			if (!itr->second.refcount) {
				Mix_FreeChunk(itr->second.chunk);
				_chunkCache.erase(itr);
				break;
			}
		}
	}
}

void AudioManager::finishCallbackStatic(void * userdata, int channel)
{
	AudioManager* audiomgr = (AudioManager*)userdata;
	for (auto audio : audiomgr->_audios) {
		if (audio->_static && audio->_channel == channel) {
			audio->_channel = -1;
			break;
		}
	}
}

void AudioManager::finishCallback(void * userdata, Mix_Music * music, int channel)
{
	AudioManager* audiomgr = (AudioManager*)userdata;
	for (auto audio : audiomgr->_audios) {
		if (!audio->_static && audio->_channel == channel) {
			audio->_channel = -1;
			break;
		}
	}
}

AudioManager::AudioManager()
{
}

AudioManager::~AudioManager()
{
	Mix_CloseAudio();
}

NS_REK_END