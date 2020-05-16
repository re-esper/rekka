#pragma once

#include "rekka.h"
#include "rapidjson/document.h"
#include "script_core.h"

NS_REK_BEGIN

class LocalStorage {
public:	
	LocalStorage();
	~LocalStorage();
	void jsb_register(JSContext *ctx, JS::HandleObject parent);
public:
	JS_FUNC_DECL(getItem)
	JS_FUNC_DECL(setItem)
	JS_FUNC_DECL(removeItem)
	JS_FUNC_DECL(clear)
	JS_FUNC_DECL(key)
	JS_PROPGET_DECL(length)	
private:
	void flush();
private:
	std::string _filePath;
	rapidjson::Document _json;
};

NS_REK_END