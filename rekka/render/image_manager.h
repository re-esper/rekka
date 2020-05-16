#pragma once

#include "rekka.h"
#include "render/image.h"
#include <mutex>
#include <thread>
#include <deque>

NS_REK_BEGIN

class ImageManager {
private:
	static ImageManager* s_sharedImageManager;
public:
	ImageManager();
	~ImageManager();
	static ImageManager* getInstance();	
	void fetchImageAsync(const std::string &filepath, const std::function<void(gpu::Image*)>& callback);
private:
	struct FetchStruct {
		std::string filename;
		std::function<void(gpu::Image*)> callback;
		unsigned char* data;
		int width, height;
		FetchStruct(const std::string& fn, const std::function<void(gpu::Image*)>& f) : filename(fn), callback(f), data(nullptr) {}
	};
	void fetchImageFunc();
	void fetchDoneCallBack();	
	void loadImageData(const std::string& fileName, FetchStruct* fetchStruct);	
	int _asyncRefCount;
	int _fechDoneSchedulerId;
	std::deque<FetchStruct*> _requestQueue;
	std::deque<FetchStruct*> _responseQueue;
	std::mutex _requestMutex;
	std::mutex _responseMutex;	
};

NS_REK_END