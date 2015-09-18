/**
 * (c) 2015 WigWag Inc.
 *
 * Author: ed
 */


#include <v8.h>
#include <node.h>
#include <uv.h>
#include <node_buffer.h>

#include "nan.h"

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

Nan::Persistent<Function> GreaseLoggerClient::constructor;

Grease::GreaseLoggerClient *Grease::GreaseLoggerClient::CLIENT;

NAN_METHOD(GreaseLoggerClient::SetGlobalOpts) {
	GreaseLoggerClient *l = GreaseLoggerClient::setupClass();
	Local<Function> cb;

	if(info.Length() < 1 || !info[0]->IsObject()) {
		Nan::ThrowTypeError("setGlobalOpts: bad parameters");
		return;
	}

	Local<Object> jsObj = info[0]->ToObject();
//	l->levelFilterOutMask
	Nan::MaybeLocal<Value> jsVal = Nan::Get(jsObj,Nan::New("levelFilterOutMask").ToLocalChecked());
	Local<Value> jsAVal;
	if(jsVal.ToLocal(&jsAVal)) {
		if(!jsAVal.IsEmpty() && jsAVal->Uint32Value()) {
			l->Opts.levelFilterOutMask = jsAVal->Uint32Value();
		}
	}

	jsVal = Nan::Get(jsObj,Nan::New("defaultFilterOut").ToLocalChecked());
	if(jsVal.ToLocal(&jsAVal)) {
		if(jsAVal->IsBoolean()) {
			bool v = jsAVal->ToBoolean()->BooleanValue();
			l->Opts.defaultFilterOut = v;
		}
	}


	//	req->completeCB = Persistent<Function>::New(Local<Function>::Cast(args[0]));
	jsVal = Nan::Get(jsObj,Nan::New("onSinkFailureCB").ToLocalChecked());
	if(jsVal.ToLocal(&jsAVal)) {
		if(jsAVal->IsFunction()) {
			l->Opts.lock();
			l->Opts.onSinkFailureCB = new Nan::Callback(Local<Function>::Cast(jsAVal));
			l->Opts.unlock();
		}
	}


	jsVal = Nan::Get(jsObj,Nan::New("show_errors").ToLocalChecked());
	if(jsVal.ToLocal(&jsAVal)) {
		if(jsAVal->IsBoolean()) {
			bool v = jsAVal->ToBoolean()->BooleanValue();
			l->Opts.show_errors = v;
		}
	}
	jsVal = Nan::Get(jsObj,Nan::New("callback_errors").ToLocalChecked());
	if(jsVal.ToLocal(&jsAVal)) {
		if(jsAVal->IsBoolean()) {
			bool v = jsAVal->ToBoolean()->BooleanValue();
			l->Opts.callback_errors = v;
		}
	}
}

/**
 * Returns GREASE_OK (0) if the client successfully connects
 * @param args
 * @return
 */
NAN_METHOD(GreaseLoggerClient::Start) {
	GreaseLoggerClient *l = GreaseLoggerClient::setupClass();

	int r = grease_initLogger();
	if(r != GREASE_OK) {
		l->sinkFailureNeedsCall = true;
	}
	l->greaseConnectMethod = grease_getConnectivityMethod();
	info.GetReturnValue().Set(Nan::New((int32_t) r));
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
NAN_METHOD(GreaseLoggerClient::Log) {
	static extra_logMeta meta; // static - this call is single threaded from node.
	ZERO_LOGMETA(meta.m);
//	uint32_t target = GREASE_DEFAULT_TARGET_ID;
	if(info.Length() > 1 && info[0]->IsString() && info[1]->IsInt32()){
		GreaseLoggerClient *l = GreaseLoggerClient::setupClass();
		Nan::Utf8String v8str(info[0]->ToString());
		meta.m.level = (uint32_t) info[1]->Int32Value(); // level

		if(info.Length() > 2 && info[2]->IsInt32()) // tag
			meta.m.tag = (uint32_t) info[2]->Int32Value();
		else
			meta.m.tag = 0;

		if(info.Length() > 3 && info[3]->IsInt32()) // origin
			meta.m.origin = (uint32_t) info[3]->Int32Value();
		else
			meta.m.origin = 0;

		if(info.Length() > 4 && info[4]->IsObject()) {
			Local<Object> jsObj = info[4]->ToObject();
			Local<Value> val; Nan::MaybeLocal<Value> Mval = Nan::Get(jsObj,Nan::New("ignores").ToLocalChecked());
			if(Mval.ToLocal<Value>(&val)) {
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
			}

			meta.m.extras = 1;
		}

		if(GreaseLoggerClient::_log(&meta.m,v8str.operator *(),v8str.length()) != GREASE_OK) {
			if(l->sinkFailureNeedsCall) {
				if(l->Opts.onSinkFailureCB) {
					l->Opts.onSinkFailureCB->Call(Nan::GetCurrentContext()->Global(),0,NULL);
					l->sinkFailureNeedsCall = false;
				}
			}
			if(l->greaseConnectMethod == GREASE_VIA_SINK) {
				l->sinkErrorCount++;
				if(l->sinkErrorCount > l->Opts.maxSinkErrors) {
					// make callback
					if(l->Opts.onSinkFailureCB) {
						l->Opts.onSinkFailureCB->Call(Nan::GetCurrentContext()->Global(),0,NULL);
					} else {
						l->sinkFailureNeedsCall = true;
					}
					l->greaseConnectMethod = GREASE_NO_CONNECTION;
				}
			}
		}
	}
}



int GreaseLoggerClient::_log( const logMeta *meta, const char *s, int len) { // internal log cmd
	int ret = GREASE_FAILED;
	if(grease_log) {
		ret = grease_log(meta, s, len);
	}

	return ret;
}




void GreaseLoggerClient::Init(v8::Local<v8::Object> exports) {
	Nan::HandleScope scope;

	Local<FunctionTemplate> tpl = Nan::New<v8::FunctionTemplate>(New);
	tpl->SetClassName(Nan::New("GreaseLoggerClient").ToLocalChecked());
	tpl->InstanceTemplate()->SetInternalFieldCount(1);

	tpl->PrototypeTemplate()->SetInternalFieldCount(2);

	Nan::SetPrototypeMethod(tpl,"start",Start);
	Nan::SetPrototypeMethod(tpl,"log",Log);
	Nan::SetPrototypeMethod(tpl,"setGlobalOpts",SetGlobalOpts);
//	tpl->InstanceTemplate()->Set(String::NewSymbol("start"), FunctionTemplate::New(Start)->GetFunction());
//	tpl->InstanceTemplate()->Set(String::NewSymbol("log"), FunctionTemplate::New(Log)->GetFunction());
//	tpl->InstanceTemplate()->Set(String::NewSymbol("setGlobalOpts"), FunctionTemplate::New(SetGlobalOpts)->GetFunction());


	GreaseLoggerClient::constructor.Reset(tpl->GetFunction());
	exports->Set(Nan::New<String>("ClientLogger").ToLocalChecked(), tpl->GetFunction());
}


/** TunInterface(opts)
 * opts {
 * 	    ifname: "tun77"
 * }
 * @param args
 * @return
 **/
NAN_METHOD(GreaseLoggerClient::New) {
	GreaseLoggerClient* obj = NULL;

	if (info.IsConstructCall()) {
	    // Invoked as constructor: `new MyObject(...)`
		if(info.Length() > 0) {
			if(!info[0]->IsObject()) {
				Nan::ThrowTypeError("Improper first arg to TunInterface cstor. Must be an object.");
				return;
			}

			obj = GreaseLoggerClient::setupClass();
		} else {
			obj = GreaseLoggerClient::setupClass();
		}

		obj->Wrap(info.This());
	    info.GetReturnValue().Set(info.This());
	} else {
	    // Invoked as plain function `MyObject(...)`, turn into construct call.
	    const int argc = 1;
	    Local<Value> argv[argc] = { info[0] };
	    v8::Local<v8::Function> cons = Nan::New<v8::Function>(constructor);
	    info.GetReturnValue().Set(cons->NewInstance(argc, argv));
	  }

}

//Handle<Value> GreaseLoggerClient::NewInstance(const Arguments& args) {
//	HandleScope scope;
//	int n = args.Length();
//	Local<Object> instance;
//
//	if(args.Length() > 0) {
//		Handle<Value> argv[n];
//		for(int x=0;x<n;x++)
//			argv[x] = args[x];
//		instance = GreaseLoggerClient::constructor->NewInstance(n, argv);
//	} else {
//		instance = GreaseLoggerClient::constructor->NewInstance();
//	}
//
//	return scope.Close(instance);
//}


