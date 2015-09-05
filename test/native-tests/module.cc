/*
 * ProcFs.cc
 *
 *  Created on: Aug 27, 2014
 *      Author: ed
 * (c) 2014, Framez Inc
 */


#include "module.h"
#define _ERRCMN_ADD_CONSTS 1
#include "error-common.h"

// BUILDTYPE is a node-gyp-dev thing
#ifdef TESTMODULE_DEBUG_BUILD
#warning "*** Debug build."
#endif

#include "module_err.h"
#include "grease_client.h"
#include <stdio.h>
#include <string.h>
#include <errno.h>
#include <stdlib.h>
#include <unistd.h>

using namespace v8;


Persistent<Function> TestModule::constructor;



Handle<Value> TestModule::Init(const Arguments& args) {

	HandleScope scope;


	return scope.Close(Undefined());

}

class workReq {
public:
	int number_of_logs;
	uint32_t interval;
	char *s;
	v8::Persistent<Function> completeCB;
	uv_work_t work;

	workReq() : number_of_logs(0), interval(0), s(NULL), completeCB() {
		work.data = this;
	}
	~workReq() {
		if(s) ::free(s);
	}
};


/** TestModule(opts)
 * opts {

 * }
 * @param args
 * @return
 **/
Handle<Value> TestModule::New(const Arguments& args) {
	HandleScope scope;

	TestModule* obj = NULL;

	if (args.IsConstructCall()) {
	    // Invoked as constructor: `new MyObject(...)`
//	    double value = args[0]->IsUndefined() ? 0 : args[0]->NumberValue();
//		if(!args[0]->IsString()) {
//			return ThrowException(Exception::TypeError(String::New("Improper first arg to ProcFs cstor. Must be a string.")));
//		}

		obj = new TestModule();

		obj->Wrap(args.This());
	    return args.This();
	} else {
	    // Invoked as plain function `MyObject(...)`, turn into construct call.
	    const int argc = 1;
	    Local<Value> argv[argc] = { args[0] };
	    return scope.Close(constructor->NewInstance(argc, argv));
	  }

}



/**
 *
 * @param
 * doSomeLoggin(msg,intervalms,num,completecb)
 */
Handle<Value> TestModule::DoSomeLoggin(const Arguments& args) {
	HandleScope scope;
	int cb_index = 1;
	if(args.Length() > 3 && args[0]->IsString() && args[3]->IsFunction()) {

		workReq *req = new workReq();
		v8::String::Utf8Value v8str(args[0]);

		int len = v8str.length();
		if(len > 0) {
			req->s = (char *) malloc(len+1);
			memcpy(req->s,v8str.operator *(),len);
		}

		req->interval = args[1]->Uint32Value();
		req->number_of_logs = (int) args[2]->Int32Value();
		req->completeCB = Persistent<Function>::New(Local<Function>::Cast(args[3]));

//		if(req->ref) {
//			uv_timer_init(uv_default_loop(),&req->timeoutHandle);
//			req->timeoutHandle.data = req;
//			uv_timer_start(&req->timeoutHandle,timercb_pseudofs,req->timeout*req->retries,0);
//			uv_ref((uv_handle_t *)&req->timeoutHandle);
//		}

		DBG_OUT("Queuing work for doSomeLoggin\n");
		uv_queue_work(uv_default_loop(), &(req->work), TestModule::do_somelogging, TestModule::post_work);

		return scope.Close(Undefined());
	} else {
		return ThrowException(Exception::TypeError(String::New("doSomeLoggin(msg,intervalms,num,completecb)")));
	}
}

void TestModule::do_somelogging(uv_work_t *req) {
	INIT_GLOG;  // initialize logger (for the worker thread - can be called more than once)
	workReq *job = (workReq *) req->data;
	DBG_OUT("do_work()\n");


	for(int n=0;n<20;n++) {
		GLOG("GLOG 1234 %d  ! (test module)",n);
		GLOG_INFO("GLOG 1234 %d  ! (test module)",n);
		GLOG_WARN("GLOG 1234 %d  ! (test module)",n);
		GLOG_ERROR("GLOG 1234 %d  ! (test module)",n);
		GLOG_DEBUG("GLOG 1234 %d  ! (test module)",n);
		GLOG_DEBUG2("GLOG 1234 %d  ! (test module)",n);
		GLOG_DEBUG3("GLOG 1234 %d  ! (test module)",n);
	}










}

void TestModule::post_work(uv_work_t *req, int status) {
	workReq *job = (workReq *) req->data;
	const unsigned argc = 3;
	Local<Value> argv[argc];
	TestModule *hiddenobj = NULL;

	if(!job->completeCB->IsUndefined()) {
		job->completeCB->Call(Context::GetCurrent()->Global(),0,argv);
	}



	delete job; // should delete Persistent Handles and allow the Buffer object to be GC'ed
}






void InitAll(Handle<Object> exports, Handle<Object> module) {
	INIT_GLOG;  // initialize logger (you should also call this in every new thread at least once)

	exports->Set(String::NewSymbol("testModule"), FunctionTemplate::New(TestModule::New)->GetFunction());
	exports->Set(String::NewSymbol("doSomeLoggin"), FunctionTemplate::New(TestModule::DoSomeLoggin)->GetFunction());

	Handle<Object> consts = Object::New();
	_errcmn::DefineConstants(consts);
	exports->Set(String::NewSymbol("CONSTS"), consts);



}

NODE_MODULE(testmodule, InitAll)






