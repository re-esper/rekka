#include "file_loader.h"
#include "scheduler.h"

NS_REK_BEGIN

FileLoader* FileLoader::s_sharedFileLoader = nullptr;
FileLoader* FileLoader::getInstance()
{
	if (!s_sharedFileLoader) s_sharedFileLoader = new (std::nothrow) FileLoader();
	return s_sharedFileLoader;
}

FileLoader::FileLoader()
: _asyncRefCount(0), _fechDoneSchedulerId(0)
{
}

FileLoader::~FileLoader()
{
}

void FileLoader::loadFileAsync(const std::string &filepath, const std::function<void(void*, size_t)>& callback)
{		
	if (0 == _asyncRefCount) {
		_fechDoneSchedulerId = Scheduler::getInstance()->scheduleCallback(std::bind(&FileLoader::fetchDoneCallBack, this), 0, true);
	}	
	++_asyncRefCount;
	FetchStruct* fetchStruct = new (std::nothrow) FetchStruct(filepath, callback);
	_requestMutex.lock();

	bool hasthread = !_requestQueue.empty();
	_requestQueue.push_back(fetchStruct);
	if (!hasthread) std::thread(&FileLoader::fetchFileFunc, this).detach();
	_requestMutex.unlock();
}

void FileLoader::fetchFileFunc()
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
		fetchStruct->data = fetchFileContent(fetchStruct->filename.c_str(), &fetchStruct->dataLength);
		// response
		_responseMutex.lock();
		_responseQueue.push_back(fetchStruct);
		_responseMutex.unlock();
	}	
}

void FileLoader::fetchDoneCallBack()
{
	FetchStruct* fetchStruct = nullptr;
	_responseMutex.lock();
	if (!_responseQueue.empty()) {
		fetchStruct = _responseQueue.front();
		_responseQueue.pop_front();
	}
	_responseMutex.unlock();

	if (fetchStruct) {
		fetchStruct->callback(fetchStruct->data, fetchStruct->dataLength);
		delete fetchStruct;
		--_asyncRefCount;
		if (0 == _asyncRefCount) {
			Scheduler::getInstance()->cancel(_fechDoneSchedulerId);
			_fechDoneSchedulerId = 0;
		}
	}
}

NS_REK_END