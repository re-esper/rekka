#pragma once

#include "rekka.h"
#include <mutex>
#include <thread>
#include <deque>

NS_REK_BEGIN

class FileLoader {
private:
	static FileLoader* s_sharedFileLoader;
public:
	FileLoader();
	~FileLoader();
	static FileLoader* getInstance();
	void loadFileAsync(const std::string &filepath, const std::function<void(void*, size_t)>& callback);
private:
	struct FetchStruct {
		std::string filename;
		std::function<void(void*, size_t)> callback;
		void* data;
		size_t dataLength;
		FetchStruct(const std::string& fn, const std::function<void(void*, size_t)>& f) : filename(fn), callback(f), data(nullptr), dataLength(0) {}
	};
	void fetchFileFunc();
	void fetchDoneCallBack();	
	int _asyncRefCount;
	int _fechDoneSchedulerId;
	std::deque<FetchStruct*> _requestQueue;
	std::deque<FetchStruct*> _responseQueue;
	std::mutex _requestMutex;
	std::mutex _responseMutex;	
};

NS_REK_END