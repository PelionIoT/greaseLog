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
//
//void TestModule::SetReadChunkSize(Local<String> property, Local<Value> val, const AccessorInfo &info) {
//	HandleScope scope;
//	TestModule* obj = ObjectWrap::Unwrap<TestModule>(info.This());
//	if(val->IsInt32()) {
//		obj->read_chunk_size = (int) val->Int32Value();
//	} else {
//		ERROR_OUT("Assignment to ->read_chunk_size with non Int32 type.");
//	}
//
//}

//void TestModule::timercb_pseudofs(uv_timer_t *h, int status) {
//	workReq *req = (workReq *) h->data;
//	DBG_OUT("timeout on eventloop for req.");
//	uv_timer_stop(h);
//	uv_unref((uv_handle_t*) h);
//	uv_close((uv_handle_t*) h, uv_close_handle_cb);
//}
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
	workReq *job = (workReq *) req->data;
	DBG_OUT("do_work()\n");


	for(int n=0;n<20;n++) {
		GLOG("GLOG 1234 %d  !",n);
		GLOG_INFO("GLOG 1234 %d  !",n);
		GLOG_WARN("GLOG 1234 %d  !",n);
		GLOG_ERROR("GLOG 1234 %d  !",n);
		GLOG_DEBUG("GLOG 1234 %d  !",n);
		GLOG_DEBUG2("GLOG 1234 %d  !",n);
		GLOG_DEBUG3("GLOG 1234 %d  !",n);
	}



//	job->_errno = 0;
//	if(job->t & workType::OPEN) {
//		if(job->self->_path) {
//			DBG_OUT("doing OPEN: %s\n", job->self->_path);
//			job->_fs_flags |= job->self->_fs_flags;
//			if((job->t & workType::READ) && !(job->t & workType::WRITE))
//				job->_fs_flags |= O_RDONLY;
//			else if((job->t & workType::WRITE) && !(job->t & workType::READ))
//				job->_fs_flags |= O_WRONLY;
//			else { // if OPEN job only, then put in default flags if none stated
//				if(!job->_fs_flags)
//					job->_fs_flags = O_RDWR;
//			}
//
//			job->_fd = open(job->self->_path, job->_fs_flags);
//
//			if(job->_fd == -1) {
//				job->_errno = errno;
//				DBG_OUT("got error %d\n",job->_errno);
//				return;
//			} else {
//				job->self->_fs_flags = job->_fs_flags; // update flags used in TestModule object
//			}
//		}
//	}
//
//
//	if(!job->_errno && (job->t & workType::READ)) {
//		int numBytes = job->_reqSize;
//		bool eof = false;
//		chunk *nextChunk = NULL;
//		if(!numBytes) numBytes = READ_DEFAULT_CHUNK_SIZE;
//		int total = 0;
//		int r = 0;
//		while(!eof) {
//			if(total < numBytes) { // use the backing buffer for as long as we can... (most pseduo fs use cases)
//				DBG_OUT("doing read() [total=%d]\n",total);
//				r = read(job->_fd,job->_backing+total,numBytes);
//			} else {               // otherwise, use auxiliary buffers. slower.
//				if(nextChunk) {
//					nextChunk->_next = new chunk(READ_DEFAULT_CHUNK_SIZE);
//					nextChunk = nextChunk->_next;
//				} else {
//					job->_extras = new chunk(READ_DEFAULT_CHUNK_SIZE);
//					nextChunk = job->_extras;
//				}
//				r = read(job->_fd,nextChunk->_buf,numBytes);
//				nextChunk->_len = r;
//			}
//			if(r == -1) {
//				job->_errno = errno;
//				break;
//			} else {
//				total += r;
//			}
//			if(r==0 || (job->t & workType::SSHOT)) eof = true; // if no more data, or its a single-shot, then done.
//		}
//		DBG_OUT("total read: %d\n",total);
//		job->len = total;
//	}
//
//	if(!job->_errno && (job->t & workType::WRITE)) {
//		int numBytes = job->len;
//		bool eof = false;
//		chunk *nextChunk = NULL;
//		int total = 0;
//		int r = 0;
//		int retry = 0;
//		DBG_OUT("doing write() loop: [%d] %s \n",job->len, job->_backing);
//		while(job->len && (job->len > total)) {
//			DBG_OUT("doing write()\n");
//			r = write(job->_fd,job->_backing+total,numBytes);
//			if(r == -1) {
//				job->_errno = errno;
//				break;
//			} else {
//				total += r;
//			}
//			if(r == 0) { // if no error, but no bytes written
//				retry++;
//				if(retry > job->retries) {
//					job->_errno = PSEUDO_PARTIAL_WRITE;
//					break;
//				} else
//					usleep(job->timeout*1000); // timeout to try to write() again - bad behavior on a normal FS, but useful for pseudo FS
//			}
//		}
//		IF_DBG( if(total >= r) DBG_OUT("write complete.\n"); );
//	}
//
//	if((job->t & workType::CLOSE) && job->_fd) {  // attempt to close even if previous error
//		int r = close(job->_fd);
//		if(r==-1 && !job->_errno) {
//			job->_errno = errno;
//		}
//	}


}

void TestModule::post_work(uv_work_t *req, int status) {
	workReq *job = (workReq *) req->data;
	const unsigned argc = 3;
	Local<Value> argv[argc];
	TestModule *hiddenobj = NULL;

	if(!job->completeCB->IsUndefined()) {
		job->completeCB->Call(Context::GetCurrent()->Global(),0,argv);
	}


//	if(job->t & workType::SSHOT) {
//		if(job->t & workType::READ) {
//			if(job->_errno == 0) {
//		//		Buffer* rawbuffer = ObjectWrap<Buffer>(job->buffer);
//				if(!job->completeCB->IsUndefined()) {
//					argv[0] = Local<Primitive>::New(Null());
//					argv[1] = job->buffer->ToObject();
//					argv[2] = Integer::New(job->len);
//					job->completeCB->Call(Context::GetCurrent()->Global(),3,argv);
//					DBG_OUT("SuccessCB (read)\n");
//				}
//			} else { // failure
//				if(!job->completeCB->IsUndefined()) {
//					argv[0] = _errcmn::errno_to_JS(job->_errno,"Error in readPseudofile(): ");
//					job->completeCB->Call(Context::GetCurrent()->Global(),1,argv);
//				}
//			}
//		}
//		if(job->t & workType::WRITE) {
//			if(job->_errno == 0) {
//		//		Buffer* rawbuffer = ObjectWrap<Buffer>(job->buffer);
//				if(!job->completeCB->IsUndefined()) {
//					argv[0] = Local<Primitive>::New(Null());  // changed to null to match node.js behavior
//					job->completeCB->Call(Context::GetCurrent()->Global(),1,argv);
//					DBG_OUT("SuccessCB (write)\n");
//				}
//			} else { // failure
//				if(!job->completeCB->IsUndefined()) {
//					argv[0] = _errcmn::errno_to_JS(job->_errno,"Error in writePseudofile(): ");
//					job->completeCB->Call(Context::GetCurrent()->Global(),1,argv);
//				}
//			}
//		}
//
//		if(job->ref) {
//			uv_unref((uv_handle_t *)&job->timeoutHandle);
//			uv_timer_stop(&job->timeoutHandle);
//		}
//
//		if(job->self) {
//			hiddenobj = job->self;
//		}
//	} else {
//		DBG_OUT("!!! not implemented yet !!!");
//	}

	delete job; // should delete Persistent Handles and allow the Buffer object to be GC'ed
}



///**
// * Writes data to Pseudo file system (procfs, sysfs, devfs)
// * callback = function(error) {}
// * pseudoFS.writeFile(filename, data, [options], callback)
// */
//Handle<Value> TestModule::WritePseudofile(const Arguments& args) {
//	HandleScope scope;
//	int opts_in = 0;
//	if(args.Length() < 3) {
//		return ThrowException(Exception::TypeError(String::New("pseudofs -> Need at least two params: writePseudo(filename, data, [options], callback)")));
//	}
//
//	if(args.Length() == 3) {
//		opts_in = 0;
//	}
//	if(!args[2+opts_in]->IsFunction())
//		return ThrowException(Exception::TypeError(String::New("pseudofs -> Need at least two params: writePseudo(filename, data, [options], callback)")));
//
//	v8::String::Utf8Value v8str(args[0]);
//	TestModule *obj = new TestModule(v8str.operator *());
//	workReq *req = new TestModule::workReq(obj,workType::OPEN|workType::WRITE|workType::CLOSE|workType::SSHOT); // open,read,close - single shot
//
//
//	if(((args[1]->IsObject() && Buffer::HasInstance(args[1])) || args[1]->IsString()) && args[0]->IsString()) {
//		if(args[1]->IsString()) {
//			v8::String::Utf8Value v8dat(args[1]);
//			req->_backing = strdup(v8dat.operator *()); // copy the string to the request
//			req->freeBacking = true; // mark to free() on delete
//			req->len = strlen(req->_backing);
//		} else {
//			req->buffer = Persistent<Object>::New(args[1]->ToObject()); // keep the Buffer persistent until the write is done... (will be removed when req is deleted)
//			req->_backing = node::Buffer::Data(args[1]->ToObject());
//			req->len = node::Buffer::Length(args[1]->ToObject());
//		}
//
//		// TODO process options
//
//		req->completeCB = Persistent<Function>::New(Local<Function>::Cast(args[opts_in+2]));
//		// queue up write job...
//		DBG_OUT("Queuing work for writePseudofile()\n");
//		uv_queue_work(uv_default_loop(), &(req->work), TestModule::do_, TestModule::post_work);
//
//		return scope.Close(Undefined());
//	} else {
//		return ThrowException(Exception::TypeError(String::New("pseudofs -> Need at least two params: writePseudo(filename, data {string|Buffer}, [options], callback)")));
//	}
//}




void InitAll(Handle<Object> exports, Handle<Object> module) {
	INIT_GLOG;  // initialize logger

//	TunInterface::Init();
//	exports->Set(String::NewSymbol("InitPseudoFS"), FunctionTemplate::New(TestModule::Init)->GetFunction());
	exports->Set(String::NewSymbol("testModule"), FunctionTemplate::New(TestModule::New)->GetFunction());
	exports->Set(String::NewSymbol("doSomeLoggin"), FunctionTemplate::New(TestModule::DoSomeLoggin)->GetFunction());

	Handle<Object> consts = Object::New();
	_errcmn::DefineConstants(consts);
	exports->Set(String::NewSymbol("CONSTS"), consts);



}

NODE_MODULE(testmodule, InitAll)






