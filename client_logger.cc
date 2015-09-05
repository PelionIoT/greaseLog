/**
 * (c) 2015 WigWag Inc.
 *
 * Author: ed
 */


#include <v8.h>
#include <node.h>
#include <uv.h>
#include <node_buffer.h>

using namespace node;
using namespace v8;

#include <stdio.h>
#include <string.h>
#include <errno.h>
#include <stdlib.h>

#include <TW/tw_fifo.h>
#include <TW/tw_khash.h>

#include "grease_client.h"
#include "client_logger.h"
#include "error-common.h"

using namespace Grease;

Persistent<Function> GreaseLoggerClient::constructor;

Grease::GreaseLoggerClient *Grease::GreaseLoggerClient::CLIENT;

Handle<Value> GreaseLoggerClient::SetGlobalOpts(const Arguments& args) {
	HandleScope scope;

	GreaseLoggerClient *l = GreaseLoggerClient::setupClass();
	Local<Function> cb;

	if(args.Length() < 1 || !args[0]->IsObject()) {
		return ThrowException(Exception::TypeError(String::New("setGlobalOpts: bad parameters")));
	}

	Local<Object> jsObj = args[0]->ToObject();
//	l->levelFilterOutMask
	Local<Value> jsVal = jsObj->Get(String::New("levelFilterOutMask"));

	if(jsVal->Uint32Value()) {
		l->Opts.levelFilterOutMask = jsVal->Uint32Value();
	}

	jsVal = jsObj->Get(String::New("defaultFilterOut"));
	if(jsVal->IsBoolean()) {
		bool v = jsVal->ToBoolean()->BooleanValue();
		l->Opts.defaultFilterOut = v;
	}

	jsVal = jsObj->Get(String::New("show_errors"));
	if(jsVal->IsBoolean()) {
		bool v = jsVal->ToBoolean()->BooleanValue();
		l->Opts.show_errors = v;
	}
	jsVal = jsObj->Get(String::New("callback_errors"));
	if(jsVal->IsBoolean()) {
		bool v = jsVal->ToBoolean()->BooleanValue();
		l->Opts.callback_errors = v;
	}

	return scope.Close(Undefined());
}

/**
 * Returns GREASE_OK (0) if the client successfully connects
 * @param args
 * @return
 */
Handle<Value> GreaseLoggerClient::Start(const Arguments& args) {
	HandleScope scope;
	GreaseLoggerClient *l = GreaseLoggerClient::setupClass();

	int r = grease_initLogger();
	l->greaseConnectMethod = grease_getConnectivityMethod();
	return scope.Close(Integer::New(r));
}

/**
 * logstring and level manadatory
 * all else optional
 * log(message(number), level{number},tag{number},origin{number},extras{object})
 *
 * extras = {
 *    .ignores = {number|array}
 * }
 * @method log
 *
 */
Handle<Value> GreaseLoggerClient::Log(const Arguments& args) {
	static extra_logMeta meta; // static - this call is single threaded from node.
	ZERO_LOGMETA(meta.m);
	HandleScope scope;
//	uint32_t target = GREASE_DEFAULT_TARGET_ID;
	if(args.Length() > 1 && args[0]->IsString() && args[1]->IsInt32()){
		GreaseLoggerClient *l = GreaseLoggerClient::setupClass();
		v8::String::Utf8Value v8str(args[0]->ToString());
		meta.m.level = (uint32_t) args[1]->Int32Value(); // level

		if(args.Length() > 2 && args[2]->IsInt32()) // tag
			meta.m.tag = (uint32_t) args[2]->Int32Value();
		else
			meta.m.tag = 0;

		if(args.Length() > 3 && args[3]->IsInt32()) // origin
			meta.m.origin = (uint32_t) args[3]->Int32Value();
		else
			meta.m.origin = 0;

		if(args.Length() > 4 && args[4]->IsObject()) {
			Local<Object> jsObj = args[4]->ToObject();
			Local<Value> val = jsObj->Get(String::New("ignores"));
			if(val->IsArray()) {
				Local<Array> array = v8::Local<v8::Array>::Cast(val);
				uint32_t i = 0;
				for (i=0 ; i < array->Length() ; ++i) {
				  const Local<Value> value = array->Get(i);
				  if(i >= MAX_IGNORE_LIST) {
					  break;
				  } else {
					  meta.ignore_list[i] = value->Uint32Value();
				  }
				}
				meta.ignore_list[i] = 0;
			} else if(val->IsUint32()) {
				meta.ignore_list[0] = val->Uint32Value();
				meta.ignore_list[1] = 0;
			}
			meta.m.extras = 1;
		}

		if(GreaseLoggerClient::_log(&meta.m,v8str.operator *(),v8str.length()) != GREASE_OK) {
			if(l->greaseConnectMethod == GREASE_VIA_SINK) {
				l->sinkErrorCount++;
				if(l->sinkErrorCount > l->Opts.maxSinkErrors) {
					// make callback
					if(!l->Opts.onSinkFailureCB.IsEmpty()) {
						l->Opts.onSinkFailureCB->Call(Context::GetCurrent()->Global(),0,NULL);
					}
					l->greaseConnectMethod = GREASE_NO_CONNECTION;
				}
			}
		}
	}
	return scope.Close(Undefined());
}



int GreaseLoggerClient::_log( const logMeta *meta, const char *s, int len) { // internal log cmd
	return grease_logToSink(meta, s, len);
}




void GreaseLoggerClient::Init() {
	Local<FunctionTemplate> tpl = FunctionTemplate::New(New);
	tpl->SetClassName(String::NewSymbol("GreaseLoggerClient"));
	tpl->InstanceTemplate()->SetInternalFieldCount(1);

	tpl->PrototypeTemplate()->SetInternalFieldCount(2);



	tpl->InstanceTemplate()->Set(String::NewSymbol("start"), FunctionTemplate::New(Start)->GetFunction());
	tpl->InstanceTemplate()->Set(String::NewSymbol("log"), FunctionTemplate::New(Log)->GetFunction());

	GreaseLoggerClient::constructor = Persistent<Function>::New(tpl->GetFunction());

}


/** TunInterface(opts)
 * opts {
 * 	    ifname: "tun77"
 * }
 * @param args
 * @return
 **/
Handle<Value> GreaseLoggerClient::New(const Arguments& args) {
	HandleScope scope;

	GreaseLoggerClient* obj = NULL;

	if (args.IsConstructCall()) {
	    // Invoked as constructor: `new MyObject(...)`
		if(args.Length() > 0) {
			if(!args[0]->IsObject()) {
				return ThrowException(Exception::TypeError(String::New("Improper first arg to TunInterface cstor. Must be an object.")));
			}

			obj = GreaseLoggerClient::setupClass();
		} else {
			obj = GreaseLoggerClient::setupClass();
		}

		obj->Wrap(args.This());
	    return args.This();
	} else {
	    // Invoked as plain function `MyObject(...)`, turn into construct call.
	    const int argc = 1;
	    Local<Value> argv[argc] = { args[0] };
	    return scope.Close(constructor->NewInstance(argc, argv));
	  }

}

Handle<Value> GreaseLoggerClient::NewInstance(const Arguments& args) {
	HandleScope scope;
	int n = args.Length();
	Local<Object> instance;

	if(args.Length() > 0) {
		Handle<Value> argv[n];
		for(int x=0;x<n;x++)
			argv[x] = args[x];
		instance = GreaseLoggerClient::constructor->NewInstance(n, argv);
	} else {
		instance = GreaseLoggerClient::constructor->NewInstance();
	}

	return scope.Close(instance);
}


