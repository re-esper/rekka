#include "local_storage.h"
#include "script_core.h"
#include "core.h"
#include "rapidjson/writer.h"
#include "rapidjson/stringbuffer.h"

NS_REK_BEGIN

static JSClass localstorage_class = {
	"Storage", JSCLASS_HAS_PRIVATE,
	nullptr, nullptr, nullptr, nullptr, nullptr, nullptr, nullptr, nullptr
};
static JSPropertySpec localstorage_properties[] = {
	JS_PROP_GET_DEF(LocalStorage, length),
	JS_PS_END
};
static JSFunctionSpec localstorage_funcs[] = {
	JS_FUNC_DEF(LocalStorage, getItem),
	JS_FUNC_DEF(LocalStorage, setItem),
	JS_FUNC_DEF(LocalStorage, removeItem),
	JS_FUNC_DEF(LocalStorage, clear),
	JS_FUNC_DEF(LocalStorage, key),
	JS_FS_END
};
LocalStorage::LocalStorage()
{
}

LocalStorage::~LocalStorage()
{
}

void LocalStorage::jsb_register(JSContext* ctx, JS::HandleObject parent)
{	
	JS::RootedObject robj(ctx, JS_DefineObject(ctx, parent, "localStorage", &localstorage_class, JSPROP_PERMANENT | JSPROP_READONLY | JSPROP_ENUMERATE));
	JS_DefineFunctions(ctx, robj, localstorage_funcs);
	JS_DefineProperties(ctx, robj, localstorage_properties);
	JS_SetPrivate(robj, this);
	_filePath = Core::getInstance()->_prefPath + "LocalStorage";
	size_t dataLength = 0;
	char* data = fetchFileContent(_filePath.c_str(), &dataLength);
	if (data == nullptr) { // 尝试重新创建
		_json.SetObject();
		flush();
	}
	else { // 读取并解析
		_json.Parse(data);
		if (_json.HasParseError()) {
			SDL_LogError(0, "Broken local storage file, error %d", _json.GetParseError());
			_json.SetObject();			
		}
		free(data);
	}
}

void LocalStorage::flush()
{
	rapidjson::StringBuffer buffer;
	rapidjson::Writer<rapidjson::StringBuffer> writer(buffer);
	_json.Accept(writer);
	writeFileContent(_filePath.c_str(), buffer.GetString(), buffer.GetSize());
}

JS_PROPGET_IMPL(LocalStorage, length)
{
	JS_BEGIN_ARG_THIS(LocalStorage);
	JS_RET(pthis->_json.MemberCount());
}

JS_FUNC_IMPL(LocalStorage, getItem)
{
	JS_BEGIN_ARG_THIS(LocalStorage);
	JS_STRING_ARG(key, 0);
	auto& d = pthis->_json;
	if (d.HasMember(key) && d[key].IsString()) {		
		JS_RET(d[key].GetString());
	}
	JS_RET("");
}

JS_FUNC_IMPL(LocalStorage, setItem)
{
	JS_BEGIN_ARG_THIS(LocalStorage);
	JS_STRING_ARG(key, 0);
	JS_STRING_ARG(value, 1);
	auto& d = pthis->_json;
	if (d.HasMember(key)) {		
		d[key].SetString(value, d.GetAllocator());
	}
	else {
		rapidjson::Value vkey(key, d.GetAllocator());
		rapidjson::Value vvalue(value, d.GetAllocator());
		d.AddMember(vkey, vvalue, d.GetAllocator());
	}
	pthis->flush();
	JS_RETURN;
}

JS_FUNC_IMPL(LocalStorage, removeItem)
{
	JS_BEGIN_ARG_THIS(LocalStorage);
	JS_STRING_ARG(key, 0);
	auto& d = pthis->_json;
	if (d.HasMember(key)) {
		if (d.RemoveMember(key)) pthis->flush();
	}
	JS_RETURN;
}

JS_FUNC_IMPL(LocalStorage, clear)
{
	JS_BEGIN_ARG_THIS(LocalStorage);
	pthis->_json.Clear();
	pthis->flush();
	JS_RETURN;
}

JS_FUNC_IMPL(LocalStorage, key)
{
	JS_BEGIN_ARG_THIS(LocalStorage);
	JS_INT_ARG(index, 0);
	auto& d = pthis->_json;
	int i = 0;
	for (auto itr = d.MemberBegin(); itr != d.MemberEnd(); ++itr, ++i) {
		if (i == index) {
			JS_RET(itr->name.GetString());
		}
	}
	JS_RETURN;
}

NS_REK_END