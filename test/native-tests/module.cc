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

#include "nan.h"

#include "module_err.h"
#include "grease_client.h"
#include <stdio.h>
#include <string.h>
#include <errno.h>
#include <stdlib.h>
#include <unistd.h>

using namespace v8;


Nan::Persistent<v8::Function> TestModule::constructor;





class workReq {
public:
	int number_of_logs;
	uint32_t interval;
	char *s;
	Nan::Callback *completeCB;
	uv_work_t work;

	workReq() : number_of_logs(0), interval(0), s(NULL), completeCB(NULL) {
		work.data = this;
	}
	~workReq() {
		if(s) ::free(s);
		if(completeCB) delete completeCB;
	}
};


/** TestModule(opts)
 * opts {

 * }
 * @param args
 * @return
 **/
NAN_METHOD(TestModule::New){

	TestModule* obj = NULL;

	if (info.IsConstructCall()) {
	    // Invoked as constructor: `new MyObject(...)`
		obj = new TestModule();

		obj->Wrap(info.This());
		info.GetReturnValue().Set(info.This());
	} else {
	    // Invoked as plain function `MyObject(...)`, turn into construct call.
//	    const int argc = 1;
//	    Local<Value> argv[argc] = { info[0] };
	    v8::Local<v8::Function> cons = Nan::New<v8::Function>(TestModule::constructor);
	    info.GetReturnValue().Set(cons->NewInstance(0, 0));
	  }

}



/**
 *
 * @param
 * doSomeLoggin(msg,intervalms,num,completecb)
 */
NAN_METHOD(TestModule::DoSomeLoggin) {
	int cb_index = 1;
	if(info.Length() > 3 && info[0]->IsString() && info[3]->IsFunction()) {

		workReq *req = new workReq();
		v8::String::Utf8Value v8str(info[0]);

		int len = v8str.length();
		if(len > 0) {
			req->s = (char *) malloc(len+1);
			memcpy(req->s,v8str.operator *(),len);
		}

		req->interval = info[1]->Uint32Value();
		req->number_of_logs = (int) info[2]->Int32Value();
		req->completeCB = new Nan::Callback(Local<Function>::Cast(info[3]));

//		if(req->ref) {
//			uv_timer_init(uv_default_loop(),&req->timeoutHandle);
//			req->timeoutHandle.data = req;
//			uv_timer_start(&req->timeoutHandle,timercb_pseudofs,req->timeout*req->retries,0);
//			uv_ref((uv_handle_t *)&req->timeoutHandle);
//		}

		DBG_OUT("Queuing work for doSomeLoggin\n");
		uv_queue_work(uv_default_loop(), &(req->work), TestModule::do_somelogging, TestModule::post_work);
	} else {
		Nan::ThrowTypeError("doSomeLoggin(msg,intervalms,num,completecb)");
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

	if(job->completeCB) {
		job->completeCB->Call(0,0);
	}


	delete job; // should delete Persistent Handles and allow the Buffer object to be GC'ed
}






void InitAll(Handle<Object> exports, Handle<Object> module) {
	INIT_GLOG;  // initialize logger (you should also call this in every new thread at least once)
	Local<FunctionTemplate> tpl = Nan::New<v8::FunctionTemplate>(TestModule::New);
	tpl->SetClassName(Nan::New("testModule").ToLocalChecked());
	tpl->InstanceTemplate()->SetInternalFieldCount(1);

	tpl->PrototypeTemplate()->SetInternalFieldCount(2);
	Nan::SetPrototypeMethod(tpl,"doSomeLoggin",TestModule::DoSomeLoggin);
	TestModule::constructor.Reset(tpl->GetFunction());

//	Nan::Set(exports,Nan::New<String>("testModule"), FunctionTemplate::New(TestModule::New)->GetFunction());
	exports->Set(Nan::New("testModule").ToLocalChecked(), tpl->GetFunction());
//	Nan::Set(exports,Nan::New<String>("doSomeLoggin"), FunctionTemplate::New(TestModule::DoSomeLoggin)->GetFunction());
	Handle<Object> errconsts = Nan::New<Object>();
	_errcmn::DefineConstants(errconsts);
	Nan::Set(exports,Nan::New<String>("ERR").ToLocalChecked(), errconsts);


}

NODE_MODULE(testmodule, InitAll)






