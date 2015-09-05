/**
 * (c) 2015 WigWag Inc.
 *
 * Author: ed
 */
#include "logger.h"
#include "client_logger.h"

#include <v8.h>
#include <node.h>

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






Handle<Value> ErrorFromErrno(const Arguments& args) {
	HandleScope scope;

	if(args.Length() > 0 && args[0]->Int32Value()) {
		Local<Value> err = _errcmn::errno_to_JS(args[0]->Int32Value(),"netkit: ");
		return scope.Close(err);
	} else {
		return scope.Close(Undefined());
	}

}

Handle<Value> NewLoggerClient(const Arguments& args) {
	HandleScope scope;

	return scope.Close(GreaseLoggerClient::NewInstance(args));

}


void InitAll(Handle<Object> exports, Handle<Object> module) {

	exports->Set(String::NewSymbol("newClient"), FunctionTemplate::New(NewLoggerClient)->GetFunction());

	GreaseLoggerClient::Init();

	Handle<Object> errconsts = Object::New();
	_errcmn::DefineConstants(errconsts);
	exports->Set(String::NewSymbol("ERR"), errconsts);

}

NODE_MODULE(greaseLogClient, InitAll)
