#pragma once

#include "rekka.h"
#include "curl/curl.h"
#include "script_core.h"

NS_REK_BEGIN

typedef enum {
	kResponseTypeString,
	kResponseTypeArrayBuffer,
	kResponseTypeBlob,
	kResponseTypeDocument,
	kResponseTypeJSON
} XMLHttpRequestResponseType;
static const char *_responseType_enum_names[] = {
	"text",
	"arraybuffer",
	"blob",
	"document",
	"json",
	nullptr
};

// !! 不支持请求过程中重新请求或abort
class XMLHttpRequest : public ObjectWrap {
	static bool _cURLInitialized;
public:
	XMLHttpRequest();
	~XMLHttpRequest();
	static void jsb_register(JSContext *ctx, JS::HandleObject parent);
private:	
	static bool constructor(JSContext *ctx, unsigned argc, JS::Value *vp);
	JS_FUNC_DECL(open)
	JS_FUNC_DECL(send)
	JS_FUNC_DECL(setRequestHeader)
	JS_FUNC_DECL(overrideMimeType)	
	JS_PROPGET_DECL(responseType)
	JS_PROPSET_DECL(responseType)
	JS_PROPGET_DECL(readyState)
	JS_PROPGET_DECL(status)
	JS_PROPGET_DECL(responseText)
	JS_PROPGET_DECL(response)
private:
	void reset();
	void preparecURL();	
	void fetchFileAsyncCallback(void* data, size_t dataLength);	
	void fetchFile();
	void sendHTTPAsyncThread();
	void sendHTTP();
	void sendDone(bool async);
	static size_t headerFunc(char* ptr, size_t size, size_t nmemb, void *userdata);
	static size_t writeFunc(char* ptr, size_t size, size_t nmemb, void *userdata);
private:
	std::string _url;		
	int _readyState;
	int _status;
	int _responseType;	
	char* _data;
	size_t _dataLength;
	size_t _dataRead;
	CURL* _curl;
	bool _isHttpTask;
	bool _isAsync;
};

NS_REK_END