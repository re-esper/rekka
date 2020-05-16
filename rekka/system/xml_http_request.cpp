#include "xml_http_request.h"
#include "core.h"
#include "scheduler.h"
#include "file_loader.h"
#include <regex>
#include <algorithm>

NS_REK_BEGIN

static JSClass xhr_class = {
	"XMLHttpRequest", JSCLASS_HAS_PRIVATE,
	nullptr, nullptr, nullptr, nullptr, nullptr, nullptr, nullptr,
	ObjectWrap::destructor
};
XMLHttpRequest::XMLHttpRequest()
: _data(nullptr), _curl(nullptr), _responseType(kResponseTypeString)
{
	reset();
}
XMLHttpRequest::~XMLHttpRequest()
{
	SAFE_FREE(_data);	
}

void XMLHttpRequest::jsb_register(JSContext* ctx, JS::HandleObject parent)
{
	static JSPropertySpec xhr_properties[] = {
		JS_PROP_DEF(XMLHttpRequest, responseType),
		JS_PROP_GET_DEF(XMLHttpRequest, readyState),
		JS_PROP_GET_DEF(XMLHttpRequest, status),
		JS_PROP_GET_DEF(XMLHttpRequest, responseText),
		JS_PROP_GET_DEF(XMLHttpRequest, response),
		JS_PS_END
	};
	static JSFunctionSpec xhr_funcs[] = {
		JS_FUNC_DEF(XMLHttpRequest, open),
		JS_FUNC_DEF(XMLHttpRequest, send),
		JS_FUNC_DEF(XMLHttpRequest, setRequestHeader),
		JS_FUNC_DEF(XMLHttpRequest, overrideMimeType),
		JS_FS_END
	};
	JS_InitClass(ctx, parent, nullptr, &xhr_class, XMLHttpRequest::constructor, 0, xhr_properties, xhr_funcs, 0, 0);
}

bool XMLHttpRequest::constructor(JSContext* ctx, unsigned argc, JS::Value * vp)
{
	JS_BEGIN_ARG;
	JS::RootedObject obj(ctx, JS_NewObjectForConstructor(ctx, &xhr_class, args));	
	XMLHttpRequest* xhr = new XMLHttpRequest();
	xhr->wrap(ctx, obj);	
	JS_RET(obj);
}

void XMLHttpRequest::reset()
{
	if (_data) free(_data);
	_readyState = 0;
	_dataLength = 0;
	_dataRead = 0;
	_isHttpTask = false;
	_isAsync = true;
}

bool XMLHttpRequest::_cURLInitialized = false;
void XMLHttpRequest::preparecURL()
{
	if (!_cURLInitialized) {
		_cURLInitialized = true;
		curl_global_init(CURL_GLOBAL_ALL);
	}
	if (!_curl) {
		_curl = curl_easy_init();
		curl_easy_setopt(_curl, CURLOPT_WRITEFUNCTION, XMLHttpRequest::writeFunc);
		curl_easy_setopt(_curl, CURLOPT_WRITEDATA, this);
		curl_easy_setopt(_curl, CURLOPT_HEADERFUNCTION, XMLHttpRequest::headerFunc);
		curl_easy_setopt(_curl, CURLOPT_WRITEHEADER, this);
	}
}

size_t XMLHttpRequest::headerFunc(char * ptr, size_t size, size_t nmemb, void * userdata)
{
	XMLHttpRequest* pthis = (XMLHttpRequest*)userdata;
	std::string header((const char*)ptr, size * nmemb);
	std::regex re("([\\w-]+):\\s+(.+)\r\n");
	std::regex statusRe("HTTP/(1\\.1|1\\.0) (\\d{3}) (.+)\r\n");
	std::smatch md;
	if (std::regex_match(header, md, re)) {
		std::string h = md[1], v = md[2];
		std::transform(h.begin(), h.end(), h.begin(), ::tolower);
		if (h == "content-type") {
			if (v.compare(0, 16, "application/json") == 0) {
				pthis->_responseType = kResponseTypeJSON;
			}
		}
		else if (h == "content-length") {
			pthis->_dataLength = atol(v.c_str());
			pthis->_data = (char*)malloc(pthis->_dataLength + 1);
			pthis->_data[pthis->_dataLength] = 0;
		}
	}
	else if (std::regex_match(header, md, statusRe)) {
		std::string st = md[2];
		pthis->_status = atoi(st.c_str());
	}
	pthis->_readyState = 2;
	return size * nmemb;
}

size_t XMLHttpRequest::writeFunc(char * ptr, size_t size, size_t nmemb, void * userdata)
{
	XMLHttpRequest* pthis = (XMLHttpRequest*)userdata;
	size_t dataBytes = size * nmemb;
	memcpy(pthis->_data + pthis->_dataRead, ptr, dataBytes);	
	pthis->_dataRead += dataBytes;
	if (pthis->_dataRead >= pthis->_dataLength) {
		pthis->_readyState = 4;
	}
	else {
		pthis->_readyState = 3;
	}
	return 0;
}




JS_FUNC_IMPL(XMLHttpRequest, open)
{
	JS_BEGIN_ARG_THIS(XMLHttpRequest);
	if (argc < 2) JS_FAIL("Wrong number of arguments %d", argc);
	JS_STRING_ARG(method, 0);
	JS_STRING_ARG(url, 1);	

	pthis->reset();
	pthis->_readyState = 1;	
	if (argc > 2) {
		JS_BOOL_ARG(isasync, 2);
		pthis->_isAsync = isasync;
	}	
	pthis->_url = url;
	auto& urlstr = pthis->_url;
	if (urlstr.length() > 5 && urlstr.compare(urlstr.length() - 5, 5, ".json") == 0) {
		pthis->_responseType = kResponseTypeJSON;		
	}
	if (strncasecmp(url, "http", 4) == 0) { // match http and https
		pthis->_isHttpTask = true;
		pthis->preparecURL();
		curl_easy_setopt(pthis->_curl, CURLOPT_URL, url);
		if (strcasecmp(method, "POST") == 0) {
			curl_easy_setopt(pthis->_curl, CURLOPT_POST, 1);
		}		
	}
	JS_RETURN;
}

void XMLHttpRequest::fetchFileAsyncCallback(void* data, size_t dataLength)
{
	_data = (char *)data;
	_dataLength = dataLength;
	_status = _data && _dataLength > 0 ? 200 : 404;
	_readyState = 4;
	sendDone(true);
}
void XMLHttpRequest::fetchFile()
{
	_data = fetchFileContent(_url.c_str(), &_dataLength);
	_status = _data && _dataLength > 0 ? 200 : 404;
	_readyState = 4;
	sendDone(false);
}
void XMLHttpRequest::sendHTTPAsyncThread()
{
	curl_easy_perform(_curl);
	_readyState = 4;
	Scheduler::getInstance()->performInMainThread(std::bind(&XMLHttpRequest::sendDone, this, true));
}
void XMLHttpRequest::sendHTTP()
{
	curl_easy_perform(_curl);
	_readyState = 4;
	sendDone(false);
}
void XMLHttpRequest::sendDone(bool async)
{
	if (async) unref();
	JSContext* ctx = ScriptCore::getInstance()->getGlobalContext();
	JS::RootedObject jsthis(ctx, handle());
	JS::RootedValue callbackfunc(ctx);
	if (JS_GetProperty(ctx, jsthis, "onreadystatechange", &callbackfunc)) {
		if (callbackfunc.isObject()) {
			JS::RootedValue rval(ctx);
			JS_CallFunctionValue(ctx, jsthis, callbackfunc, JS::HandleValueArray::empty(), &rval);
		}
	}
	if (JS_GetProperty(ctx, jsthis, "onload", &callbackfunc)) {
		if (callbackfunc.isObject()) {
			JS::RootedValue rval(ctx);
			JS_CallFunctionValue(ctx, jsthis, callbackfunc, JS::HandleValueArray::empty(), &rval);
		}
	}
	if (_status != 200) {
		if (JS_GetProperty(ctx, jsthis, "onerror", &callbackfunc)) {
			if (callbackfunc.isObject()) {
				JS::RootedValue rval(ctx);
				JS_CallFunctionValue(ctx, jsthis, callbackfunc, JS::HandleValueArray::empty(), &rval);
			}
		}
	}
}

JS_FUNC_IMPL(XMLHttpRequest, send)
{
	JS_BEGIN_ARG_THIS(XMLHttpRequest);
	if (pthis->_readyState != 1) JS_FAIL("Must perform \"open\" first");
	SAFE_FREE(pthis->_data);
	if (pthis->_isHttpTask) {
		if (argc > 0) {
			// 暂只支持string类型的form-data			
			if (args[0].isString()) {
				JS_STRING_ARG(data, 0);
				curl_easy_setopt(pthis->_curl, CURLOPT_COPYPOSTFIELDS, data);
			}			
		}
		if (pthis->_isAsync) {
			pthis->ref(ctx);
			std::thread(std::bind(&XMLHttpRequest::sendHTTPAsyncThread, pthis)).detach();
		}
		else {
			pthis->sendHTTP();
		}
	}
	else {
		if (pthis->_isAsync) {
			pthis->ref(ctx);
			FileLoader::getInstance()->loadFileAsync(pthis->_url.c_str(),
				std::bind(&XMLHttpRequest::fetchFileAsyncCallback, pthis, std::placeholders::_1, std::placeholders::_2));
		}
		else {
			pthis->fetchFile();
		}
	}
	JS_RETURN;
}

JS_FUNC_IMPL(XMLHttpRequest, setRequestHeader)
{
	JS_BEGIN_ARG_THIS(XMLHttpRequest);
	JS_STRING_ARG(header, 0);
	JS_STRING_ARG(value, 1);
	pthis->preparecURL();
	std::string theheader = header;
	theheader += ": ";
	theheader += value;	
	struct curl_slist *slist = nullptr;
	slist = curl_slist_append(slist, theheader.c_str());
	curl_easy_setopt(pthis->_curl, CURLOPT_HTTPHEADER, slist);
	JS_RETURN;
}

JS_FUNC_IMPL(XMLHttpRequest, overrideMimeType)
{
	JS_BEGIN_ARG_THIS(XMLHttpRequest);
	JS_RETURN;
}

JS_PROPGET_IMPL(XMLHttpRequest, responseType)
{
	JS_BEGIN_ARG_THIS(XMLHttpRequest);
	JS_RET(_responseType_enum_names[pthis->_responseType]);
}

JS_PROPSET_IMPL(XMLHttpRequest, responseType)
{
	JS_BEGIN_ARG_THIS(XMLHttpRequest);
	JS_STRING_ARG(responseType, 0);
	if (strcmp(responseType, "text") == 0) {
		pthis->_responseType = kResponseTypeString;
	}	
	else if (strcmp(responseType, "arraybuffer") == 0) {
		pthis->_responseType = kResponseTypeArrayBuffer;
	}
	else if (strcmp(responseType, "json") == 0) {
		pthis->_responseType = kResponseTypeJSON;
	}	
	JS_RETURN;
}

JS_PROPGET_IMPL(XMLHttpRequest, readyState)
{
	JS_BEGIN_ARG_THIS(XMLHttpRequest);
	JS_RET(pthis->_readyState);
}

JS_PROPGET_IMPL(XMLHttpRequest, status)
{
	JS_BEGIN_ARG_THIS(XMLHttpRequest);
	JS_RET(pthis->_status);
}

JS_PROPGET_IMPL(XMLHttpRequest, responseText)
{
	JS_BEGIN_ARG_THIS(XMLHttpRequest);
	if (pthis->_data && pthis->_dataLength > 0) {
		JS_RET((const char *)pthis->_data);
	}
	JS_RETURN;
}

JS_PROPGET_IMPL(XMLHttpRequest, response)
{
	JS_BEGIN_ARG_THIS(XMLHttpRequest);
	if (pthis->_data && pthis->_dataLength > 0) {
		if (pthis->_responseType == kResponseTypeJSON) {
			JS::RootedValue jsonval(ctx);
			JS::RootedString str(ctx, JS_NewStringCopyZ(ctx, (const char *)pthis->_data));			
			if (JS_ParseJSON(ctx, str, &jsonval)) {
				JS_RET(jsonval);
			}
			else {
				JS_FAIL_EXCEPTOPN;				
			}
		}
		else if (pthis->_responseType == kResponseTypeArrayBuffer) {
			JSObject* arraybuffer = JS_NewMappedArrayBufferWithContents(ctx, pthis->_dataLength, pthis->_data);
			pthis->_data = nullptr;
			pthis->_dataLength = 0;
			JS_RET(arraybuffer);
		}
		else {
			// by default, return text
			JS_RET((const char *)pthis->_data);
		}		
	}
	JS_RETURN;
}

NS_REK_END