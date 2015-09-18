/**
 * (c) 2015 WigWag Inc.
 *
 * Author: ed
 */
#include "logger.h"
#include "client_logger.h"

#include <v8.h>
#include <node.h>

#include "nan.h"

#include <stdio.h>
#include <string.h>
#include <errno.h>
#include <stdlib.h>
#include <stddef.h>
#include <sys/types.h>
#include <sys/socket.h>

#include "error-common.h"

using namespace node;
using namespace v8;

using namespace Grease;






NAN_METHOD(ErrorFromErrno) {
	if(info.Length() > 0 && info[0]->Int32Value()) {
		Local<Value> err = _errcmn::errno_to_JS(info[0]->Int32Value(),"netkit: ");
		info.GetReturnValue().Set(err);
	}
}

//NAN_METHOD(NewLogger) {
//	info.GetReturnValue().Set(GreaseLogger::NewInstance(args));
//
//}

//Handle<Value> NewLoggerClient(const Arguments& args) {
//	HandleScope scope;
//
//	return scope.Close(GreaseLoggerClient::NewInstance(args));
//
//}


void InitAll(Handle<Object> exports, Handle<Object> module) {

//	exports->Set(String::NewSymbol("newLogger"), FunctionTemplate::New(NewLogger)->GetFunction());
//	exports->Set(String::NewSymbol("newClient"), FunctionTemplate::New(NewLoggerClient)->GetFunction());

	GreaseLogger::Init(exports);
	GreaseLoggerClient::Init(exports);

	Handle<Object> errconsts = Nan::New<Object>();
	_errcmn::DefineConstants(errconsts);
	Nan::Set(exports,Nan::New<String>("ERR").ToLocalChecked(), errconsts);

}

NODE_MODULE(greaseLog, InitAll)
