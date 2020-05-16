#pragma once

#include "jsapi.h"
#include <assert.h>

class ObjectWrap {
public:
	ObjectWrap() : _refs(0), _holder(nullptr) {		
	}
	virtual ~ObjectWrap() {
		if (_holder) delete _holder;
	}
	inline JS::Heap<JSObject*>& handle() {
		return _handle;
	}
	static void destructor(JSFreeOp* fop, JSObject* obj) {
		ObjectWrap *wrap = (ObjectWrap *)JS_GetPrivate(obj);
		if (wrap != nullptr) { // not a Prototype
			delete wrap;
		}		
	}
protected:
	inline void wrap(JSContext *ctx, JS::HandleObject obj) {
		assert(_refs == 0);
		_handle = obj;
		JS_SetPrivate(_handle, this);
	}
public:	
	virtual void ref(JSContext *ctx) {
		if (!_holder) _holder = new JS::PersistentRootedObject(ctx, _handle);
		_refs++;
	}
	virtual void unref() {
		assert(_refs > 0);
		if (--_refs == 0) {
			if (_holder) delete _holder;			
			_holder = nullptr;			
		}
	}	
private:
	int _refs;  // ro
	JS::Heap<JSObject*> _handle;
	JS::PersistentRootedObject* _holder;
};


