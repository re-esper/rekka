#include "image_manager.h"
#include "scheduler.h"
#include "stb_image.h"

NS_REK_BEGIN

ImageManager* ImageManager::s_sharedImageManager = nullptr;
ImageManager* ImageManager::getInstance()
{
	if (!s_sharedImageManager) s_sharedImageManager = new (std::nothrow) ImageManager();
	return s_sharedImageManager;
}

ImageManager::ImageManager()
: _asyncRefCount(0), _fechDoneSchedulerId(0)
{
}

ImageManager::~ImageManager()
{
}

void ImageManager::fetchImageAsync(const std::string & filepath, const std::function<void(gpu::Image*)>& callback)
{	
	if (0 == _asyncRefCount) {
		_fechDoneSchedulerId = Scheduler::getInstance()->scheduleCallback(std::bind(&ImageManager::fetchDoneCallBack, this), 0, true);
	}	
	++_asyncRefCount;
	FetchStruct* fetchStruct = new (std::nothrow) FetchStruct(filepath, callback);
	_requestMutex.lock();
	bool hasthread = !_requestQueue.empty();		
	_requestQueue.push_back(fetchStruct);
	if (!hasthread) std::thread(&ImageManager::fetchImageFunc, this).detach();
	_requestMutex.unlock();
}

void ImageManager::fetchImageFunc()
{	 
	bool firstloop = true;	
	while (true) {
		// get any request
		FetchStruct* fetchStruct = nullptr;
		_requestMutex.lock();
		if (firstloop) firstloop = false;
		else _requestQueue.pop_front();		
		if (!_requestQueue.empty()) fetchStruct = _requestQueue.front();		
		_requestMutex.unlock();
		// exit thread when request queue is empty
		if (!fetchStruct) break;
		// handle request, load image
		loadImageData(fetchStruct->filename, fetchStruct);	
		// response
		_responseMutex.lock();
		_responseQueue.push_back(fetchStruct);
		_responseMutex.unlock();
	}	
}

void ImageManager::fetchDoneCallBack()
{
	FetchStruct* fetchStruct = nullptr;
	_responseMutex.lock();
	if (!_responseQueue.empty()) {
		fetchStruct = _responseQueue.front();
		_responseQueue.pop_front();
	}
	_responseMutex.unlock();

	if (fetchStruct) {
		gpu::Image* image = nullptr;
		if (fetchStruct->data) {
			image = gpu::CreateImage(fetchStruct->width, fetchStruct->height, gpu::FORMAT_RGBA);
			gpu::UpdateImageBytes(image, NULL, fetchStruct->data, 4 * fetchStruct->width);
			stbi_image_free(fetchStruct->data);
		}
		if (fetchStruct->callback) fetchStruct->callback(image);
		delete fetchStruct;
		--_asyncRefCount;
		if (0 == _asyncRefCount) {
			Scheduler::getInstance()->cancel(_fechDoneSchedulerId);
			_fechDoneSchedulerId = 0;
		}
	}
}

#define RGB_PREMULTIPLY_ALPHA(vr, vg, vb, va) \
    (unsigned)(((unsigned)((unsigned char)(vr) * ((unsigned char)(va) + 1)) >> 8) | \
    ((unsigned)((unsigned char)(vg) * ((unsigned char)(va) + 1) >> 8) << 8) | \
    ((unsigned)((unsigned char)(vb) * ((unsigned char)(va) + 1) >> 8) << 16) | \
    ((unsigned)(unsigned char)(va) << 24))
void ImageManager::loadImageData(const std::string& fileName, FetchStruct* fetchStruct)
{
	SDL_RWops* rwops = SDL_RWFromFile(fileName.c_str(), "rb");
	fetchStruct->data = nullptr;
	fetchStruct->width = 0;
	fetchStruct->height = 0;
	if (!rwops) {
		SDL_LogError(0, "Failed to open file: %s", SDL_GetError());
		return;
	}

	SDL_RWseek(rwops, 0, SEEK_SET);
	int data_bytes = SDL_RWseek(rwops, 0, SEEK_END);
	SDL_RWseek(rwops, 0, SEEK_SET);
	// Read in the rwops data
	unsigned char* c_data = (unsigned char*)SDL_malloc(data_bytes);
	SDL_RWread(rwops, c_data, 1, data_bytes);
	SDL_RWclose(rwops);
	int width, height, channels;
	unsigned char* bitmap = stbi_load_from_memory(c_data, data_bytes, &width, &height, &channels, 4);
	if (!bitmap) {
		SDL_LogError(0, "Failed to load image : %s", stbi_failure_reason());	
	}
	else {
		stbi_info_from_memory(c_data, data_bytes, &width, &height, &channels);
		// ! Premultiplied alpha 
		if (channels == 4) {
			uint32_t* data32 = (uint32_t*)bitmap;
			for (int i = 0; i < width * height; ++i) {
				unsigned char* p = bitmap + i * 4;
				data32[i] = RGB_PREMULTIPLY_ALPHA(p[0], p[1], p[2], p[3]);
			}
		}
		fetchStruct->width = width;
		fetchStruct->height = height;
	}
	fetchStruct->data = bitmap;
	SDL_free(c_data);
}

NS_REK_END