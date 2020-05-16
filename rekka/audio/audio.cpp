#include "audio.h"
#include "script_core.h"
#include "scheduler.h"
#include "audio/audio_manager.h"

NS_REK_BEGIN

static JSClass audio_class = {
	"Audio", JSCLASS_HAS_PRIVATE,
	nullptr, nullptr, nullptr, nullptr, nullptr, nullptr, nullptr,
	ObjectWrap::destructor
};
static JSPropertySpec audio_properties[] = {
	JS_PROP_DEF(Audio, src),
	JS_PROP_DEF(Audio, loop),
	JS_PROP_DEF(Audio, volume),
	JS_PROP_DEF(Audio, currentTime),
	JS_PROP_GET_DEF(Audio, duration),
	JS_PROP_GET_DEF(Audio, playing),
	JS_PS_END
};
static JSFunctionSpec audio_funcs[] = {
	JS_FUNC_DEF(Audio, canPlayType),
	JS_FUNC_DEF(Audio, play),
	JS_FUNC_DEF(Audio, pause),
	JS_FUNC_DEF(Audio, stop),
	JS_FUNC_DEF(Audio, fadeIn),
	JS_FUNC_DEF(Audio, fadeOut),
	JS_FS_END
};
void Audio::jsb_register(JSContext *ctx, JS::HandleObject parent)
{
	JS_InitClass(ctx, parent, nullptr, &audio_class, Audio::constructor, 0, audio_properties, audio_funcs, 0, 0);
}
bool Audio::is_js_instance(JS::HandleObject obj)
{
	return JS_GetClass(obj) == &audio_class;
}

Audio::Audio(bool isstatic)
: _src(""), _static(isstatic), _chunk(nullptr), _music(nullptr), _channel(-1), 
_loop(false), _volume(128), _currentTime(0)
{
}
Audio::~Audio()
{
	AudioManager::getInstance()->removeAudio(this);
	if (_chunk) AudioManager::getInstance()->releaseChunk(_chunk);
	if (_music) Mix_FreeMusic(_music);
}

bool Audio::constructor(JSContext *ctx, unsigned argc, JS::Value *vp)
{
	JS_BEGIN_ARG;
	bool isstatic = false;
	if (argc > 1) {
		JS_BOOL_ARG(isstatic_, 1);
		isstatic = isstatic_;
	}
	Audio* audio = AudioManager::getInstance()->createAudio(isstatic);
	if (argc > 0) {
		JS_STRING_ARG(url, 0);
		audio->setSrc(url);
	}	
	JS::RootedObject obj(ctx, JS_NewObjectForConstructor(ctx, &audio_class, args));	
	audio->wrap(ctx, obj);	
	JS_RET(obj);
}

void Audio::setSrc(char * src)
{
	decodeURIComponent(src);
	if (_src == src) return;
	if (_chunk) AudioManager::getInstance()->releaseChunk(_chunk);
	if (_music) Mix_FreeMusic(_music);
	_src = src;
	if (_static) {
		_chunk = AudioManager::getInstance()->fetchChunk(_src);
		Mix_VolumeChunk(_chunk, _volume);
	}
	else {
		_music = Mix_LoadMUS_RW(SDL_RWFromFile(_src.c_str(), "rb"), SDL_TRUE);		
		Mix_VolumeMusic(_music, _volume);
	} 	
}

JS_PROPGET_IMPL(Audio, src)
{	
	JS_BEGIN_ARG_THIS(Audio);
	JS_RET(pthis->_src.c_str());
}

JS_PROPSET_IMPL(Audio, src)
{
	JS_BEGIN_ARG_THIS(Audio);
	JS_STRING_ARG(filename, 0);
	pthis->setSrc(filename);
	JS_RETURN;
}

JS_PROPGET_IMPL(Audio, loop)
{
	JS_BEGIN_ARG_THIS(Audio);
	JS_RET(pthis->_loop);
}

JS_PROPSET_IMPL(Audio, loop)
{
	JS_BEGIN_ARG_THIS(Audio);
	JS_BOOL_ARG(loop, 0);
	if (pthis->_loop != loop) {
		pthis->_loop = loop;
		int channel = pthis->_channel;
		if (pthis->_static) {
			if (channel != -1) Mix_SetLoops(channel, loop);
		}
		else {
			if (channel != -1) Mix_SetMusicLoops(pthis->_music, pthis->_channel, loop);							
		}
	}
	JS_RETURN;
}

JS_PROPGET_IMPL(Audio, duration)
{
	JS_BEGIN_ARG_THIS(Audio);
	if (pthis->_static && pthis->_chunk) {
		JS_RET(Mix_GetPlayLength(pthis->_chunk) * 0.001f);
	}
	if (!pthis->_static && pthis->_music) {
		JS_RET(Mix_GetMusicDuration(pthis->_music) * 0.001f);
	}
	JS_RET(0.0f);
}

JS_PROPGET_IMPL(Audio, currentTime)
{
	JS_BEGIN_ARG_THIS(Audio);
	double position = 0;
	int channel = pthis->_channel;
	if (!pthis->_static && channel != -1) {
		position = Mix_GetMusicPositionCh(pthis->_channel);		
	}
	JS_RET(position);
}

JS_PROPSET_IMPL(Audio, currentTime)
{
	JS_BEGIN_ARG_THIS(Audio);
	JS_DOUBLE_ARG(position, 0);
	if (!pthis->_static) {
		if (pthis->_channel != -1)
			Mix_SetMusicPositionCh(position, pthis->_channel);
		else
			pthis->_currentTime = position;
	}
	JS_RETURN;
}

JS_PROPGET_IMPL(Audio, volume)
{
	JS_BEGIN_ARG_THIS(Audio);
	JS_RET(pthis->_volume / 128.0);
}

JS_PROPSET_IMPL(Audio, volume)
{
	JS_BEGIN_ARG_THIS(Audio);
	JS_DOUBLE_ARG(volume, 0);	
	volume = std::max<double>(std::min<double>(volume, 1.0), 0.0) * 128;
	pthis->_volume = volume;	
	if (pthis->_static && pthis->_chunk) {
		Mix_VolumeChunk(pthis->_chunk, volume);
	}
	if (!pthis->_static && pthis->_music) {
		Mix_VolumeMusic(pthis->_music, volume);
	}
	JS_RETURN;
}

JS_PROPGET_IMPL(Audio, playing)
{
	JS_BEGIN_ARG_THIS(Audio);	
	JS_RET(pthis->_channel != -1 && Mix_Playing(pthis->_channel));
}

JS_FUNC_IMPL(Audio, canPlayType)
{
	JS_BEGIN_ARG_THIS(Audio);
	JS_STRING_ARG(playType, 0);
	if (strstr(playType, "audio/ogg")) JS_RET(true);
	if (strstr(playType, "audio/wav")) JS_RET(true);
	JS_RET(false);
}

JS_FUNC_IMPL(Audio, play)
{
	JS_BEGIN_ARG_THIS(Audio);
	int channel = pthis->_channel;
	int loops = pthis->_loop ? -1 : 0;
	if (channel != -1) {
		if (Mix_Paused(channel)) Mix_Resume(channel);
	}
	else if (pthis->_static) {
		pthis->_channel = Mix_PlayChannel(-1, pthis->_chunk, loops);
	}
	else {
		auto mgr = AudioManager::getInstance();		
		pthis->_channel = Mix_PlayMusicCh(pthis->_music, loops, -1);
		if (pthis->_currentTime > 0) Mix_SetMusicPositionCh(pthis->_currentTime, pthis->_channel);		
		pthis->_currentTime = 0;		
	}
	JS_RETURN;
}

JS_FUNC_IMPL(Audio, pause)
{
	JS_BEGIN_ARG_THIS(Audio);
	if (pthis->_channel != -1) Mix_Pause(pthis->_channel);
	JS_RETURN; 
}

JS_FUNC_IMPL(Audio, stop)
{
	JS_BEGIN_ARG_THIS(Audio);
	if (pthis->_channel != -1) Mix_HaltChannel(pthis->_channel);
	JS_RETURN;
}

JS_FUNC_IMPL(Audio, fadeIn)
{
	JS_BEGIN_ARG_THIS(Audio);
	JS_DOUBLE_ARG(duration, 0);
	double position = 0;
	if (argc > 1) {
		JS_DOUBLE_ARG(p, 1);
		position = p;
	}
	if (pthis->_channel != -1) Mix_HaltChannel(pthis->_channel); // SDL_Mixer bug: 若不先halt则没有fade效果
	int loops = pthis->_loop ? -1 : 0;
	if (pthis->_static) {
		pthis->_channel = Mix_FadeInChannelTimed(-1, pthis->_chunk, loops, duration * 1000, -1, position * 1000);
	}
	else {				
		pthis->_channel = Mix_FadeInMusicPosCh(pthis->_music, loops, duration * 1000, -1, position);
		pthis->_currentTime = 0;		
	}
	JS_RETURN;
}

JS_FUNC_IMPL(Audio, fadeOut)
{
	JS_BEGIN_ARG_THIS(Audio);
	JS_DOUBLE_ARG(duration, 0);	
	int channel = pthis->_channel;
	if (pthis->_static) {
		if (channel != -1) Mix_FadeOutChannel(channel, duration * 1000);
	}
	else {
		if (channel != -1) Mix_FadeOutMusicCh(duration * 1000, channel);
	}
	JS_RETURN;
}

NS_REK_END