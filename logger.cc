/*
 * tuninterface.cc
 *
 *  Created on: Aug 27, 2014
 *      Author: ed
 * (c) 2014, WigWag Inc.
 */


#include "logger.h"

#include <stdio.h>
#include <string.h>
#include <errno.h>
#include <stdlib.h>

#include <TW/tw_fifo.h>
#include <TW/tw_khash.h>

#include "grease_client.h"
#include "error-common.h"

using namespace Grease;

Persistent<Function> GreaseLogger::constructor;


//filterHash tagFilter;
//filterHash levelFilter;
//filterHash originFilter;

//bool GreaseLogger::sift(logMeta &f) { // returns true, then the logger should log it
//	bool ret = true;
//	uint32_t n = 0;
//	uv_mutex_lock(&modifyFilters);
//
//	if(levelFilterMask & f.level)
//		ret = false;
//
//	if(ret && f.tag) {
//		if(!tagFilter.find(f.tag,n)) {
//			ret = false;
//		} else {
//			ret = showNoTag;
//		}
//	}
//
//	if(ret && f.origin) {
//		if(!originFilter.find(f.origin,n)) {
//			ret = false;
//		} else {
//			ret = showNoOrigin;
//		}
//	}
//	uv_mutex_unlock(&modifyFilters);
//	return ret;
//}

bool GreaseLogger::sift(const logMeta &f, FilterList *&list) { // returns true, then the logger should log it
	bool ret = true;

	if(levelFilterOutMask & f.level)
		ret = false;

	FilterHash hash = filterhash(f.tag,f.origin);

	uv_mutex_lock(&modifyFilters);
	if(!filterHashTable.find(hash,list)) {
		if(defaultFilterOut)
			ret = false;
		list = NULL;
	}
	uv_mutex_unlock(&modifyFilters);
	return ret;
}


GreaseLogger *GreaseLogger::LOGGER = NULL;

void GreaseLogger::handleInternalCmd(uv_async_t *handle, int status /*UNUSED*/) {
	GreaseLogger::internalCmdReq req;
	while(LOGGER->internalCmdQueue.removeMv(req)) {
		logTarget *t = NULL;
    	if(req.d > 0)
    		t = GreaseLogger::LOGGER->getTarget(req.d);
    	else
    		t = GreaseLogger::LOGGER->defaultTarget;
    	assert(t);
    	switch(req.c) {
		case TARGET_ROTATE_BUFFER:
		    {
				DBG_OUT("TARGET_ROTATE_BUFFER [%d]", req.auxInt);
				t->flushAll(false);
//		    	t->flush(req.auxInt);
		    }
			break;

		case WRITE_TARGET_OVERFLOW:
			{
				DBG_OUT("WRITE_TARGET_OVERFLOW");
				t->flushAll();
				heapBuf *big = (heapBuf *) req.aux;
				t->writeAsync(big);
//				delete big; // handled by callback

	//			t->writeAsync() //overflowBuf
			}

			break;
    	case INTERNAL_SHUTDOWN:
    	{

    		// disable timer
    		uv_timer_stop(&LOGGER->flushTimer);
    		// disable all queues
    		uv_close((uv_handle_t *)&LOGGER->asyncExternalCommand, NULL);
    		uv_close((uv_handle_t *)&LOGGER->asyncInternalCommand, NULL);
    		// flush all queues
    		flushAllSync();
    		// kill thread

    	}
    	}
	}
}

void GreaseLogger::handleExternalCmd(uv_async_t *handle, int status /*UNUSED*/) {
	GreaseLogger::nodeCmdReq req;
	while(LOGGER->nodeCmdQueue.removeMv(req)) {
		logTarget *t = NULL;
//		switch(req->c) {
//		case TARGET_ROTATE_BUFFER:
//		    {
//		    	t = GreaseLogger::LOGGER->getTarget(req->d);
//		    	t->flush(req->auxInt);
//		    }
//			break;
//
//		case WRITE_TARGET_OVERFLOW:
//			{
//				t = GreaseLogger::LOGGER->getTarget(req->d);
//				t->flushAll();
//	//			t->writeAsync() //overflowBuf
//			}
//
//			break;
//		}
	}

}


void GreaseLogger::flushTimer_cb(uv_timer_t* handle, int status) {
	DBG_OUT("!! FLUSH TIMER !!");
	flushAll();
}



void GreaseLogger::mainThread(void *p) {
	GreaseLogger *SELF = (GreaseLogger *) p;



	uv_timer_init(SELF->loggerLoop, &SELF->flushTimer);
	uv_unref((uv_handle_t *)&LOGGER->flushTimer);
	uv_timer_start(&SELF->flushTimer, flushTimer_cb, 2000, 500);
	uv_async_init(SELF->loggerLoop, &SELF->asyncInternalCommand, handleInternalCmd);
	uv_async_init(SELF->loggerLoop, &SELF->asyncExternalCommand, handleExternalCmd);

    uv_run(SELF->loggerLoop, UV_RUN_DEFAULT);

}

int GreaseLogger::log(const logMeta &f, const char *s, int len) { // does the work of logging
	FilterList *list = NULL;
	if(sift(f,list)) {
		return _log(list,f,s,len);
	} else
		return GREASE_OK;
}


/**
 * create a log entry for use across the network to Grease.
 * Memory layout: [PREAMBLE][Length (type RawLogLen)][logMeta][logdata - string - no null termination]
 * @param f a pointer to a meta data for logging. If NULL, it will be empty meta data
 * @param s string to log
 * @param len length of the passed in string
 * @param tobuf A raw buffer to store the log output ready for processing
 * @param len A pointer to an int. This will be read to know the existing length of the buffer, and then set
 * the size of the buffer that should be sent
 * @return returns GREASE_OK if successful, or GREASE_NO_BUFFER if the buffer is too small. If parameters are
 * invalid returns GREASE_INVALID_PARAMS
 */
int logToRaw(logMeta *f, char *s, RawLogLen len, char *tobuf, int *buflen) {
	if(!tobuf || *buflen < (GREASE_RAWBUF_MIN_SIZE + len))  // sanity check
		return GREASE_NO_BUFFER;
	int w = 0;

	memcpy(tobuf,&__grease_preamble,SIZEOF_SINK_LOG_PREAMBLE);
	w += SIZEOF_SINK_LOG_PREAMBLE;
	RawLogLen _len =sizeof(logMeta) + sizeof(RawLogLen) + w;
	memcpy(tobuf+w,&_len,sizeof(RawLogLen));
	w += sizeof(RawLogLen);
	if(f)
		memcpy(tobuf+w,f,sizeof(logMeta));
	else
		memcpy(tobuf+w,&__noMetaData,sizeof(logMeta));
	w += sizeof(logMeta);
	if(s && len > 0) {
		memcpy(tobuf+w,s,len);
	}
	*buflen = len;
	return GREASE_OK;
}



int GreaseLogger::logFromRaw(char *base, int len) {
	logMeta m;
	RawLogLen l;
	if(len >= GREASE_RAWBUF_MIN_SIZE) {
		memcpy(&l,base+SIZEOF_SINK_LOG_PREAMBLE,sizeof(RawLogLen));
		if(l > GREASE_MAX_MESSAGE_SIZE)  // don't let crazy memory blocks through.
			return GREASE_OVERFLOW;
		memcpy(&m,base+GREASE_CLIENT_HEADER_SIZE,sizeof(logMeta));
		if(l > 0 && l < GREASE_MAX_MESSAGE_SIZE) {
			return log(m,base+GREASE_CLIENT_HEADER_SIZE+sizeof(logMeta),l);
		} else {
			ERROR_OUT("logFromRaw: message size out of range: %d\n",l);
		}
		return GREASE_OK;
	} else
		return GREASE_NO_BUFFER;
}


int GreaseLogger::logSync(const logMeta &f, const char *s, int len) { // does the work of logging. now. will empty any buffers first.
	FilterList *list = NULL;
	if(sift(f,list)) {
		return _logSync(list,f,s,len);
	} else
		return GREASE_OK;
}
void GreaseLogger::flushAll() { // flushes buffers. Synchronous
	if(LOGGER->defaultTarget) {
		LOGGER->defaultTarget->flushAll();
	} else
		ERROR_OUT("No default target!");

	logTarget **t; // shut down other targets.
	GreaseLogger::TargetTable::HashIterator iter(LOGGER->targets);
	while(!iter.atEnd()) {
		t = iter.data();
		(*t)->flushAll();
		iter.getNext();
	}
}
void GreaseLogger::flushAllSync() { // flushes buffers. Synchronous
	if(LOGGER->defaultTarget) {
		LOGGER->defaultTarget->flushAllSync();
	} else
		ERROR_OUT("No default target!");

	logTarget **t; // shut down other targets.
	GreaseLogger::TargetTable::HashIterator iter(LOGGER->targets);
	while(!iter.atEnd()) {
		t = iter.data();
		(*t)->flushAllSync();
		iter.getNext();
	}
}

GreaseLogger::logTarget::~logTarget() {

}

GreaseLogger::logTarget::logTarget(int buffer_size, uint32_t id, GreaseLogger *o,
		targetReadyCB cb, delim_data &_delim, target_start_info *readydata) :
		readyCB(cb),                  // called when the target is ready or has failed to setup
		readyData(readydata),
		logCallbackSet(false),
		logCallback(),
		delim(std::move(_delim)),
		availBuffers(NUM_BANKS),
		writeOutBuffers(NUM_BANKS-1), // there should always be one buffer available for writingTo
		waitingOnCBBuffers(NUM_BANKS-1),
		err(), _log_fd(0),
		currentBuffer(NULL), bankSize(buffer_size), owner(o), myId(id) {
	uv_mutex_init(&writeMutex);
	for (int n=0;n<NUM_BANKS;n++) {
		_buffers[n] = new logBuf(bankSize,n,delim.duplicate());
	}
	for (int n=1;n<NUM_BANKS;n++) {
		availBuffers.add(_buffers[n]);
	}
	currentBuffer = _buffers[0];
}


void GreaseLogger::targetReady(bool ready, _errcmn::err_ev &err, logTarget *t) {
	target_start_info *info = (target_start_info *) t->readyData;
	if(ready) {
		if(t->myId == DEFAULT_TARGET) {
			t->owner->defaultTarget = t;
			if(info && info->cb)
				info->cb(t->owner,err,info);
		} else {
			t->owner->targets.addReplace(t->myId,t);
			info->cb(t->owner,err,info);
		}
	} else {
		if(err.hasErr()) {
			ERROR_PERROR("Error on creating target: ", err._errno);
		}
		ERROR_OUT("Failed to create target: %d\n", t->myId);
		// TODO shutdown?
		delete t;
	}
	if(t->myId == DEFAULT_TARGET && info) delete info;
}



void GreaseLogger::setupDefaultTarget(actionCB cb, target_start_info *i) {
//	if(defaultTarget->_log_fd == GREASE_STDOUT) {
	delim_data defaultdelim;
	defaultdelim.delim.sprintf("\n");
	i->cb = cb;
	new ttyTarget(this->bufferSize, DEFAULT_TARGET, this, targetReady,std::move(defaultdelim), i);
//	} else {
//		ERROR_OUT("Unsupported default output!");
//		defaultTarget->_log_fd = GREASE_BAD_FD;
//	}
}



void GreaseLogger::start_logger_cb(GreaseLogger *l, _errcmn::err_ev &err, void *d) {
	GreaseLogger::target_start_info *info = (GreaseLogger::target_start_info *) d;
	if(info->needsAsyncQueue) {
		info->err = std::move(err);
		if(!l->targetCallbackQueue.add(info, TARGET_CALLBACK_QUEUE_WAIT)) {
			ERROR_OUT("Failed to queue callback. Buffer Full! Target: %d\n", info->targId);
		}
		uv_async_send(&l->asyncTargetCallback);
	} else {
		if(!info->targetStartCB.IsEmpty()) {
			const unsigned argc = 1;
			Local<Value> argv[argc];
			if(!info->targetStartCB->IsUndefined()) {
				if(!err.hasErr()) {
					info->targetStartCB->Call(Context::GetCurrent()->Global(),0,NULL);
				} else {
					argv[0] = _errcmn::err_ev_to_JS(err, "Default target startup error: ")->ToObject();
					info->targetStartCB->Call(Context::GetCurrent()->Global(),1,argv);
				}
			}
		}
	}
//	delete info;
}


void GreaseLogger::start(actionCB cb, target_start_info *data) {
	// FIXME use options for non default target
	setupDefaultTarget(cb,data);

	uv_thread_create(&logThreadId,GreaseLogger::mainThread,this);
}

Handle<Value> GreaseLogger::Start(const Arguments& args) {
	HandleScope scope;
	GreaseLogger *l = GreaseLogger::setupClass();
	GreaseLogger::target_start_info *info = new GreaseLogger::target_start_info();

	if(args.Length() > 0 && args[0]->IsFunction()) {
		info->targetStartCB = Persistent<Function>::New(Local<Function>::Cast(args[0]));
	}
	l->start(start_logger_cb, info);
	return scope.Close(Undefined());
}


void GreaseLogger::start_target_cb(GreaseLogger *l, _errcmn::err_ev &err, void *d) {
	GreaseLogger::target_start_info *info = (GreaseLogger::target_start_info *) d;
	if(info->needsAsyncQueue) {
		info->err = std::move(err);
		if(!l->targetCallbackQueue.add(info, TARGET_CALLBACK_QUEUE_WAIT)) {
			ERROR_OUT("Failed to queue callback. Buffer Full! Target: %d\n", info->targId);
		}
		uv_async_send(&l->asyncTargetCallback);
	} else {
		if(!info->targetStartCB.IsEmpty()) {
			const unsigned argc = 2;
			Local<Value> argv[argc];
			if(!info->targetStartCB->IsUndefined()) {
				if(!err.hasErr()) {
					argv[0] = Integer::New(info->targId);
					info->targetStartCB->Call(Context::GetCurrent()->Global(),1,argv);
				} else {
					argv[0] = Integer::New(-1);
					argv[1] = _errcmn::err_ev_to_JS(err, "Target startup error: ")->ToObject();
					info->targetStartCB->Call(Context::GetCurrent()->Global(),2,argv);
				}
			}
		}
		delete info;
	}
}


void GreaseLogger::callV8LogCallbacks(uv_async_t *h, int status ) {
	GreaseLogger *l = GreaseLogger::setupClass();
	GreaseLogger::logTarget::writeCBData data;
	while(l->v8LogCallbacks.removeMv(data)) {
		if(!data.t->logCallback.IsEmpty()) {
			const unsigned argc = 2;
			Local<Value> argv[argc];
			argv[0] = String::New( data.b->handle.base, (int) data.b->handle.len );
			argv[1] = Integer::NewFromUnsigned( data.t->myId );
			data.t->logCallback->Call(Context::GetCurrent()->Global(),1,argv);
			data.t->finalizeV8Callback(data.b);
		}
	}
}

void GreaseLogger::callTargetCallback(uv_async_t *h, int status ) {
	target_start_info *info = NULL;
	GreaseLogger *l = GreaseLogger::setupClass();
	while(l->targetCallbackQueue.remove(info)) {
		HEAVY_DBG_OUT("********* REMOVE targetCallbackQueue: %p\n", info);
		const unsigned argc = 2;
		Local<Value> argv[argc];
		if(!info->targetStartCB.IsEmpty() && !info->targetStartCB->IsUndefined()) {
			if(!info->err.hasErr()) {
				argv[0] = Integer::New(info->targId);
				info->targetStartCB->Call(Context::GetCurrent()->Global(),1,argv);
			} else {
				argv[0] = Integer::New(-1);
				argv[1] = _errcmn::err_ev_to_JS(info->err, "Target startup error: ")->ToObject();
				info->targetStartCB->Call(Context::GetCurrent()->Global(),2,argv);
			}
		}
		delete info;
	}
}

/**
 * addFilter(obj) where
 * obj = {
 *      // at least origin and/or tag must be present
 *      origin: 0x33,    // any positive number > 0
 *      tag: 0x25        // any positive number > 0,
 *      target: 3,       // mandatory
 *      mask:  0x4000000 // optional (default, show everything: 0xFFFFFFF)
 * }
 */
Handle<Value> GreaseLogger::AddFilter(const Arguments& args) {
	HandleScope scope;
	GreaseLogger *l = GreaseLogger::setupClass();
	FilterId id;
	TagId tagId = 0;
	OriginId originId = 0;
	TargetId targetId = 0;
	LevelMask mask = ALL_LEVELS;
	Handle<Value> ret = Boolean::New(false);
	if(args.Length() > 0 && args[0]->IsObject()) {
		Local<Object> jsObj = args[0]->ToObject();

		bool ok = false;
		Local<Value> jsTag = jsObj->Get(String::New("tag"));
		Local<Value> jsOrigin = jsObj->Get(String::New("origin"));
		Local<Value> jsTarget = jsObj->Get(String::New("target"));
		Local<Value> jsMask = jsObj->Get(String::New("mask"));

		if(jsTag->IsUint32()) {
			tagId = (TagId) jsTag->Uint32Value();
			ok = true;
		}
		if(jsOrigin->IsUint32()) {
			originId = (OriginId) jsOrigin->Uint32Value();
			ok = true;
		}
		if(jsMask->IsUint32()) {
			mask = jsMask->Uint32Value();
			ok = true;
		} else if (!jsMask->IsUndefined()) {
			return ThrowException(Exception::TypeError(String::New("addFilter: bad parameters (mask)")));
		}
		if(jsTarget->IsUint32()) {
			targetId = (OriginId) jsTarget->Uint32Value();
		} else {
			ok = false;
		}
		if(!ok) {
			return ThrowException(Exception::TypeError(String::New("addFilter: bad parameters")));
		}

		if(l->_addFilter(targetId,originId,tagId,mask,id))
			ret = Integer::NewFromUnsigned(id);
		else
			ret = Boolean::New(false);

	} else
		return ThrowException(Exception::TypeError(String::New("addFilter: bad parameters")));
	return scope.Close(ret);
}

Handle<Value> GreaseLogger::RemoveFilter(const Arguments& args) {
	HandleScope scope;
	return scope.Close(Undefined());

}


/**
 * obj = {
 *    pipe: "/var/mysink"   // currently our only option is a named socket / pipe
 * }
 */
Handle<Value> GreaseLogger::AddSink(const Arguments& args) {
	HandleScope scope;
	GreaseLogger *l = GreaseLogger::setupClass();




	return scope.Close(Undefined());
};


/**
 * addTarget(obj,finishCb) where obj is
 * obj = {
 *    tty: "/path"
 * }
 * obj = {
 *    file: "/path",
 *    append: true,   // default
 *    rotate: {
 *       max_files: 5,
 *       max_file_size:  100000,  // 100k
 *       max_total_size: 10000000,    // 10MB
 *       rotate_on_start: false   // default false
 *    }
 * }
 * obj = {
 *    callback: cb // some function
 * }
 *
 * finishCb(id) {
 *  // where id == the target's ID
 * }
 */
Handle<Value> GreaseLogger::AddTarget(const Arguments& args) {
	HandleScope scope;
	GreaseLogger *l = GreaseLogger::setupClass();
	uint32_t target = DEFAULT_TARGET;

//	iterate_plhdr();

	if(args.Length() > 1 && args[0]->IsObject()){
		Local<Object> jsTarg = args[0]->ToObject();
		Local<Value> isTty = jsTarg->Get(String::New("tty"));
		Local<Value> isFile = jsTarg->Get(String::New("file"));

		int buffsize = l->bufferSize;
		Local<Value> jsBufSize = jsTarg->Get(String::New("bufferSize"));
		if(jsBufSize->IsInt32()) {
			buffsize = jsBufSize->Int32Value();
			if(buffsize < 0) {
				buffsize = l->bufferSize;
			}
		}

		TargetId id;
		logTarget *targ = NULL;
		uv_mutex_lock(&l->nextIdMutex);
		id = l->nextTargetId++;
		uv_mutex_unlock(&l->nextIdMutex);

		Local<Value> jsDelim = jsTarg->Get(String::New("delim"));
		Local<Value> jsDelimOut = jsTarg->Get(String::New("delim_output"));
		Local<Value> jsCallback = jsTarg->Get(String::New("callback"));

		delim_data delims;

		if(jsDelim->IsString()) {
			v8::String::Utf8Value v8str(jsDelim);
			delims.delim.malloc(v8str.length());
			delims.delim.memcpy(v8str.operator *(),v8str.length());
		}

		if(jsDelimOut->IsString()) {
			v8::String::Utf8Value v8str(jsDelimOut);
			delims.delim_output.malloc(v8str.length());
			delims.delim_output.memcpy(v8str.operator *(),v8str.length());
		}

		if(isTty->IsString()) {
			v8::String::Utf8Value v8str(isTty);
//			obj->setIfName(v8str.operator *(),v8str.length());
			target_start_info *i = new target_start_info();
//			i->system_start_info = data;
			i->cb = start_target_cb;
			i->targId = id;
			if(args.Length() > 0 && args[1]->IsFunction())
				i->targetStartCB = Persistent<Function>::New(Local<Function>::Cast(args[1]));
			targ = new ttyTarget(buffsize, id, l, targetReady,  std::move(delims), i, v8str.operator *());
		} else if (isFile->IsString()) {
			Local<Value> jsMode = jsTarg->Get(String::New("mode"));
			Local<Value> jsFlags = jsTarg->Get(String::New("flags"));
			Local<Value> jsRotate = jsTarg->Get(String::New("rotate"));
			int mode = DEFAULT_MODE_FILE_TARGET;
			int flags = DEFAULT_FLAGS_FILE_TARGET;
			if(jsMode->IsInt32()) {
				mode = jsMode->Int32Value();
			}
			if(jsFlags->IsInt32()) {
				flags = jsFlags->Int32Value();
			}


			v8::String::Utf8Value v8str(isFile);
//			obj->setIfName(v8str.operator *(),v8str.length());
			target_start_info *i = new target_start_info();
//			i->system_start_info = data;
			if(args.Length() > 0 && args[1]->IsFunction())
				i->targetStartCB = Persistent<Function>::New(Local<Function>::Cast(args[1]));
			i->cb = start_target_cb;
			i->targId = id;
			fileTarget::rotationOpts opts;
			if(jsRotate->IsObject()) {
				Local<Object> jsObj = jsRotate->ToObject();
				Local<Value> js_max_files = jsObj->Get(String::New("max_files"));
				Local<Value> js_max_file_size = jsObj->Get(String::New("max_file_size"));
				Local<Value> js_total_size = jsObj->Get(String::New("max_total_size"));
				Local<Value> js_on_start = jsObj->Get(String::New("rotate_on_start"));
				if(js_max_files->IsUint32()) {
					opts.max_files = js_max_files->Uint32Value(); opts.enabled = true;
				}
				if(js_max_file_size->IsUint32()) {
					opts.max_file_size = js_max_file_size->Uint32Value(); opts.enabled = true;
				}
				if(js_total_size->IsUint32()) {
					opts.max_total_size = js_total_size->Uint32Value(); opts.enabled = true;
				}
				if(js_on_start->IsBoolean()) {
					if(js_on_start->IsTrue())
						opts.rotate_on_start = true; opts.enabled = true;
				}
			}
			HEAVY_DBG_OUT("********* NEW target_start_info: %p\n", i);

			if(opts.enabled)
				targ = new fileTarget(buffsize, id, l, flags, mode, v8str.operator *(), std::move(delims), i, targetReady, opts);
			else
				targ = new fileTarget(buffsize, id, l, flags, mode, v8str.operator *(), std::move(delims), i, targetReady);

		} else {
			return ThrowException(Exception::TypeError(String::New("Unknown target type passed into addTarget()")));
		}

		if(jsCallback->IsFunction()) {
			Local<Function> f = Local<Function>::Cast(jsCallback);
			targ->setCallback(f);
		}

	} else {
		return ThrowException(Exception::TypeError(String::New("Malformed call to addTarget()")));
	}
	return scope.Close(Undefined());
}

/**
 * logstring and level manadatory
 * all else optional
 * log(message(number), level{number},tag{number},origin{number})
 * @method log
 *
 */
Handle<Value> GreaseLogger::Log(const Arguments& args) {
	static logMeta meta; // static - this call is single threaded from node.
	HandleScope scope;
	uint32_t target = DEFAULT_TARGET;
	if(args.Length() > 1 && args[0]->IsString() && args[1]->IsInt32()){
		GreaseLogger *l = GreaseLogger::setupClass();
		v8::String::Utf8Value v8str(args[0]->ToString());
		meta.level = (uint32_t) args[1]->Int32Value();

		if(args.Length() > 2 && args[2]->IsInt32()) // tag
			meta.tag = (uint32_t) args[2]->Int32Value();
		else
			meta.tag = 0;

		if(args.Length() > 3 && args[3]->IsInt32()) // tag
			meta.origin = (uint32_t) args[3]->Int32Value();
		else
			meta.origin = 0;

		FilterList *list = NULL;
		if(l->sift(meta, list)) {
			l->_log(list,meta,v8str.operator *(),v8str.length());
		}
	}
	return scope.Close(Undefined());
}

Handle<Value> GreaseLogger::LogSync(const Arguments& args) {
	static logMeta meta; // static - this call is single threaded from node.
	HandleScope scope;
	uint32_t target = DEFAULT_TARGET;
	if(args.Length() > 1 && args[0]->IsString() && args[1]->IsInt32()){
		GreaseLogger *l = GreaseLogger::setupClass();
		v8::String::Utf8Value v8str(args[0]->ToString());
		meta.level = (uint32_t) args[1]->Int32Value();

		if(args.Length() > 2 && args[2]->IsInt32()) // tag
			meta.tag = (uint32_t) args[2]->Int32Value();
		else
			meta.tag = 0;

		if(args.Length() > 3 && args[3]->IsInt32()) // tag
			meta.origin = (uint32_t) args[3]->Int32Value();
		else
			meta.origin = 0;

		FilterList *list = NULL;
		if(l->sift(meta, list)) {
			l->_logSync(list,meta,v8str.operator *(),v8str.length());
		}
	}
	return scope.Close(Undefined());
}

Handle<Value> GreaseLogger::Flush(const Arguments& args) {
	HandleScope scope;
	GreaseLogger *l = GreaseLogger::setupClass();
	if(args.Length() > 1 && args[0]->Int32Value()){
		uint32_t tnum = (uint32_t) args[0]->Int32Value();
		logTarget *t = NULL;
		if(l->targets.find(tnum,t)) {
			t->flushAll();
		}
	} else {
		l->flushAll();
	}
	return scope.Close(Undefined());

}

int GreaseLogger::_log(FilterList *list, const logMeta &meta, const char *s, int len) { // internal log cmd
//	HEAVY_DBG_OUT("out len: %d\n",len);
//	DBG_OUT("meta.level %x",meta.level);
	if(len > GREASE_MAX_MESSAGE_SIZE)
		return GREASE_OVERFLOW;
	if(!list) {
		defaultTarget->write(s,len);
	} else if (list->valid(meta.level)) {
		int n = 0;
		while(list->list[n].id != 0) {
			logTarget *t = NULL;
			if((list->list[n].levelMask & meta.level) && targets.find(list->list[n].targetId, t)) {
				t->write(s,len);
			} else {
				ERROR_OUT("Orphaned target id: %d\n",list->list[n].targetId);
			}
			n++;
		}
	} else {  // write out to default target if the list does not apply to this level
//		HEAVY_DBG_OUT("pass thru to default");
		defaultTarget->write(s,len);
	}
	return GREASE_OK;
}

int GreaseLogger::_logSync(FilterList *list, const logMeta &meta, const char *s, int len) { // internal log cmd
	if(len > GREASE_MAX_MESSAGE_SIZE)
		return GREASE_OVERFLOW;
	if(!list) {
		defaultTarget->writeSync(s,len);
	} else {
		int n = 0;
		while(list->list[n].id != 0) {
			logTarget *t = NULL;
			if(targets.find(list->list[n].targetId, t)) {
				t->writeSync(s,len);
			} else {
				ERROR_OUT("Orphaned target id: %d\n",list->list[n].targetId);
			}
			n++;
		}
	}
	return GREASE_OK;
}


void GreaseLogger::Init() {
	Local<FunctionTemplate> tpl = FunctionTemplate::New(New);
	tpl->SetClassName(String::NewSymbol("GreaseLogger"));
	tpl->InstanceTemplate()->SetInternalFieldCount(1);

	tpl->PrototypeTemplate()->SetInternalFieldCount(2);

//	if(args.Length() > 0) {
//		if(args[0]->IsObject()) {
//			Local<Object> base = args[0]->ToObject();
//			Local<Array> keys = base->GetPropertyNames();
//			for(int n=0;n<keys->Length();n++) {
//				Local<String> keyname = keys->Get(n)->ToString();
//				tpl->InstanceTemplate()->Set(keyname, base->Get(keyname));
//			}
//		}
//	}

	tpl->InstanceTemplate()->Set(String::NewSymbol("start"), FunctionTemplate::New(Start)->GetFunction());
	tpl->InstanceTemplate()->Set(String::NewSymbol("flush"), FunctionTemplate::New(Flush)->GetFunction());
	tpl->InstanceTemplate()->Set(String::NewSymbol("logSync"), FunctionTemplate::New(LogSync)->GetFunction());
	tpl->InstanceTemplate()->Set(String::NewSymbol("log"), FunctionTemplate::New(Log)->GetFunction());
	tpl->InstanceTemplate()->Set(String::NewSymbol("addFilter"), FunctionTemplate::New(AddFilter)->GetFunction());
	tpl->InstanceTemplate()->Set(String::NewSymbol("removeFilter"), FunctionTemplate::New(RemoveFilter)->GetFunction());
	tpl->InstanceTemplate()->Set(String::NewSymbol("addTarget"), FunctionTemplate::New(AddTarget)->GetFunction());
	tpl->InstanceTemplate()->Set(String::NewSymbol("addSink"), FunctionTemplate::New(AddSink)->GetFunction());

//	tpl->InstanceTemplate()->Set(String::NewSymbol("setupFilter"), FunctionTemplate::New(SetupFilter)->GetFunction());

//	tpl->InstanceTemplate()->Set(String::NewSymbol("create"), FunctionTemplate::New(Create)->GetFunction());
//	tpl->InstanceTemplate()->SetAccessor(String::New("ifname"), GetIfName, SetIfName);
//	tpl->InstanceTemplate()->SetAccessor(String::New("fd"), GetIfFD, SetIfFD);
//	tpl->InstanceTemplate()->SetAccessor(String::New("flags"), GetIfFlags, SetIfFlags);
//	tpl->InstanceTemplate()->SetAccessor(String::New("lastError"), GetLastError, SetLastError);
//	tpl->InstanceTemplate()->SetAccessor(String::New("lastErrorStr"), GetLastErrorStr, SetLastErrorStr);

//	tpl->InstanceTemplate()->SetAccessor(String::New("_readChunkSize"), GetReadChunkSize, SetReadChunkSize);
//	tpl->InstanceTemplate()->Set(String::NewSymbol("_open"), FunctionTemplate::New(Open)->GetFunction());
//	tpl->InstanceTemplate()->Set(String::NewSymbol("_close"), FunctionTemplate::New(Close)->GetFunction());
//	tpl->InstanceTemplate()->Set(String::NewSymbol("_getData"), FunctionTemplate::New(GetData)->GetFunction());
//	tpl->InstanceTemplate()->Set(String::NewSymbol("_sendData"), FunctionTemplate::New(SendData)->GetFunction());


//	TunInterface::prototype = Persistent<ObjectTemplate>::New(tpl->PrototypeTemplate());
	GreaseLogger::constructor = Persistent<Function>::New(tpl->GetFunction());

}


/** TunInterface(opts)
 * opts {
 * 	    ifname: "tun77"
 * }
 * @param args
 * @return
 **/
Handle<Value> GreaseLogger::New(const Arguments& args) {
	HandleScope scope;

	GreaseLogger* obj = NULL;

	if (args.IsConstructCall()) {
	    // Invoked as constructor: `new MyObject(...)`
//	    double value = args[0]->IsUndefined() ? 0 : args[0]->NumberValue();
		if(args.Length() > 0) {
			if(!args[0]->IsObject()) {
				return ThrowException(Exception::TypeError(String::New("Improper first arg to TunInterface cstor. Must be an object.")));
			}
//			Local<Value> ifname = args[0]->ToObject()->Get(String::New("ifname"));

//			Local<Value> doTap = args[0]->ToObject()->Get(String::New("tap"));
//			if(!doTap->IsUndefined() && !doTap->IsNull()) {
//				DBG_OUT("TAP TAP TAP");
//			} else
//				obj = new GreaseLogger();

//			if(!ifname->IsUndefined()) {
////				v8::String::Utf8Value v8str(ifname);
////				obj->setIfName(v8str.operator *(),v8str.length());
//			}
			obj = GreaseLogger::setupClass();
		} else {
			obj = GreaseLogger::setupClass();
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

Handle<Value> GreaseLogger::NewInstance(const Arguments& args) {
	HandleScope scope;
	int n = args.Length();
	Local<Object> instance;

	if(args.Length() > 0) {
		Handle<Value> argv[n];
		for(int x=0;x<n;x++)
			argv[x] = args[x];
		instance = GreaseLogger::constructor->NewInstance(n, argv);
	} else {
		instance = GreaseLogger::constructor->NewInstance();
	}

	return scope.Close(instance);
}


#ifdef __cplusplus
extern "C" {
#endif

int grease_logLocal(const logMeta *f, const char *s, RawLogLen len) {
	GreaseLogger *l = GreaseLogger::setupClass();
	return l->log(*f,s,len);
}

#ifdef __cplusplus
};
#endif


//
//void TunInterface::SetIfName(Local<String> property, Local<Value> val, const AccessorInfo &info) {
//	HandleScope scope;
//	TunInterface* obj = ObjectWrap::Unwrap<TunInterface>(info.This());
//
//	if(val->IsString()) {
//		v8::String::Utf8Value v8str(val);
//		obj->setIfName(v8str.operator *(),v8str.length());
//	} else {
//		ERROR_OUT( "Invalid assignment to TunInterface object->ifname\n");
//	}
////	obj->SetIfName()
//}
//
//Handle<Value> TunInterface::GetIfName(Local<String> property, const AccessorInfo &info) {
//	HandleScope scope;
//	TunInterface* obj = ObjectWrap::Unwrap<TunInterface>(info.This());
//	if(obj->_if_name)
//		return scope.Close(String::New(obj->_if_name, strlen(obj->_if_name)));
//	else
//		return scope.Close(Undefined());
//}
//
//void TunInterface::SetIfFD(Local<String> property, Local<Value> val, const AccessorInfo &info) {
//	// does nothing - read only
//}
//
//Handle<Value> TunInterface::GetIfFD(Local<String> property, const AccessorInfo &info) {
//	HandleScope scope;
//	TunInterface* obj = ObjectWrap::Unwrap<TunInterface>(info.This());
//	if(obj->_if_fd) // 0 is default which is nothing (no device created)
//		return scope.Close(Integer::New(obj->_if_fd));
//	else
//		return scope.Close(Undefined());
//}
//
//void TunInterface::SetReadChunkSize(Local<String> property, Local<Value> val, const AccessorInfo &info) {
//	HandleScope scope;
//	TunInterface* obj = ObjectWrap::Unwrap<TunInterface>(info.This());
//	if(val->IsInt32()) {
//		obj->read_chunk_size = (int) val->Int32Value();
//	} else {
//		ERROR_OUT("Assignment to ->read_chunk_size with non Int32 type.");
//	}
//
//}
//
//Handle<Value> TunInterface::GetReadChunkSize(Local<String> property, const AccessorInfo &info) {
//	HandleScope scope;
//	TunInterface* obj = ObjectWrap::Unwrap<TunInterface>(info.This());
//	return scope.Close(Integer::New(obj->read_chunk_size));
//}
//
//
//void TunInterface::SetLastError(Local<String> property, Local<Value> val, const AccessorInfo &info) {
//	// does nothing - read only
//}
//
//Handle<Value> TunInterface::GetLastError(Local<String> property, const AccessorInfo &info) {
//	HandleScope scope;
//	TunInterface* obj = ObjectWrap::Unwrap<TunInterface>(info.This());
//	if(obj->err.hasErr()) {
//		return scope.Close(_net::err_ev_to_JS(obj->err, "TunInterface: "));
//	} else
//		return scope.Close(Undefined());
//}
//
//void TunInterface::SetIfFlags(Local<String> property, Local<Value> val, const AccessorInfo &info) {
//	TunInterface* obj = ObjectWrap::Unwrap<TunInterface>(info.This());
//	if(val->IsInt32()) {
//		obj->_if_flags = (int) val->ToInt32()->Int32Value();
//	} else {
//		ERROR_OUT("Assignment to ->_if_flags with non Int32 type.");
//	}
//}
//
//Handle<Value> TunInterface::GetIfFlags(Local<String> property, const AccessorInfo &info) {
//	HandleScope scope;
//	TunInterface* obj = ObjectWrap::Unwrap<TunInterface>(info.This());
//	return scope.Close(Integer::New(obj->_if_flags));
//}
//
//
//Handle<Value> TunInterface::IsCreated(const Arguments &args) {
//	HandleScope scope;
//	TunInterface* obj = ObjectWrap::Unwrap<TunInterface>(args.This());
//	if(obj->_if_fd) // 0 is default which is nothing (no device created)
//		return scope.Close(Boolean::New(true));
//	else
//		return scope.Close(Boolean::New(false));
//}


/**
 * Assigns a callback to be called when data arrives from the TUN interface. If data is already available, then
 * this function calls the callback immediately. The callback will *not* be called again once it is called once.
 * @param
 * func(callback, size)
 * size is 'advisory' -->  http://nodejs.org/api/stream.html#stream_readable_read_size_1
 * callback = function(Buffer,amountread,error) {}
 */
//Handle<Value> TunInterface::GetData(const Arguments& args) {
//	HandleScope scope;
//	if(args.Length() > 0 && args[0]->IsFunction()) {
//		int sizereq = 0;
//		if(args.Length() > 1 && args[1]->IsInt32())
//			sizereq = (int) args[1]->Int32Value();
//		TunInterface* obj = ObjectWrap::Unwrap<TunInterface>(args.This());
//
//		TunInterface::readReq *req = new TunInterface::readReq(obj);
//		if(sizereq < obj->read_chunk_size) sizereq = obj->read_chunk_size; // read at least the MTU, regardless of req read size
//		// FIXME for node 0.12 this will change. Take note.
//		Handle<Object> buf = UNI_BUFFER_NEW(sizereq);
//		// make new Buffer object. Make it Persistent to keep it around after the HandleScope closes.
//		// we will do the read in a different thread. We don't want to call v8 in another thread, so just do the unwrapping here before we do the work..
//		// in the work we will just copy stuff to the _backing store.
//		req->buffer = Persistent<Object>::New(buf);
//
////		buf->Ref();
//		req->_backing = node::Buffer::Data(buf);
//		req->len = sizereq;
//		req->completeCB = Persistent<Function>::New(Local<Function>::Cast(args[0]));
//		// queue up read job...
//		DBG_OUT("Queuing work for read()\n");
//		uv_queue_work(uv_default_loop(), &(req->work), TunInterface::do_read, TunInterface::post_read);
//
//		return scope.Close(Undefined());
//	} else {
//		return ThrowException(Exception::TypeError(String::New("send() -> Need at least two params: getData([int32], [function])")));
//	}
//}
//
//
//void TunInterface::do_read(uv_work_t *req) {
//	readReq *job = (readReq *) req->data;
//	DBG_OUT("do_read()\n");
//
//	if(job->self->_if_fd) {
//		int ret = read(job->self->_if_fd,job->_backing,job->len);
//		DBG_OUT("ret = %d\n", ret);
//		if(ret < 0) {
//			job->_errno = errno;  // an error occurred, so record error info
//			job->len = 0;
//		} else {
//			job->len = ret; // record number of bytes read
//		}
//	}
//}
//
//void TunInterface::post_read(uv_work_t *req, int status) {
//	readReq *job = (readReq *) req->data;
//
//	const unsigned argc = 3;
//	Local<Value> argv[argc];
//	if(job->buffer->IsUndefined()) {
//		ERROR_OUT("**** Failure on read: Why is buffer not defined??\n");
//	} else
//		argv[0] = job->buffer->ToObject();
//	argv[1] = Integer::New(job->len);
//
//	if(job->_errno == 0) {
////		Buffer* rawbuffer = ObjectWrap<Buffer>(job->buffer);
//
//		if(!job->completeCB->IsUndefined()) {
//			job->completeCB->Call(Context::GetCurrent()->Global(),2,argv);
//		}
//	} else { // failure
//		if(!job->completeCB->IsUndefined()) {
//			argv[2] = _net::errno_to_JS(job->_errno,"Error in read(): ");
//			job->completeCB->Call(Context::GetCurrent()->Global(),3,argv);
//		}
//	}
//
//	delete job; // should delete Persistent Handles and allow the Buffer object to be GC'ed
//}
//
///**
// * Sends data to the TUN interface:
// * send(Buffer, CallbackSuccess, CallbackError) { }
// */
//Handle<Value> TunInterface::SendData(const Arguments& args) {
//	HandleScope scope;
//	if(args.Length() > 2 && args[1]->IsFunction() && args[0]->IsObject()) {
//		TunInterface* obj = ObjectWrap::Unwrap<TunInterface>(args.This());
//
//		TunInterface::writeReq *req = new TunInterface::writeReq(obj);
//		req->buffer = Persistent<Object>::New(args[0]->ToObject()); // keep the Buffer persistent until the write is done...
//		if(!Buffer::HasInstance(args[0])) {
//			return ThrowException(Exception::TypeError(String::New("send() -> passed in Buffer has no backing!")));
//		}
//
//		req->_backing = node::Buffer::Data(args[0]->ToObject());
//		req->len = node::Buffer::Length(args[0]->ToObject());
//		req->onSendSuccessCB = Persistent<Function>::New(Local<Function>::Cast(args[1]));
//		if(args.Length() > 2 && args[2]->IsFunction()) {
//			req->onSendFailureCB = Persistent<Function>::New(Local<Function>::Cast(args[2]));
//		}
//		// queue up read job...
//		uv_queue_work(uv_default_loop(), &(req->work), TunInterface::do_write, TunInterface::post_write);
//
//		return scope.Close(Undefined());
//	} else {
//		return ThrowException(Exception::TypeError(String::New("send() -> Need at least two params: send(Buffer, successCallback)")));
//	}
//}
//
//void TunInterface::do_write(uv_work_t *req) {
//	writeReq *job = (writeReq *) req->data;
//
//	int ret = 0;
//	int written = 0;
//	char *buf = job->_backing;
//	job->_errno = 0;
//	if(job->self->_if_fd) {
//		while (ret >= 0 && written < job->len) {
//			int ret = write(job->self->_if_fd,buf,job->len - written);
//			if(ret < 0) {
//				job->_errno = errno;  // an error occurred, so record error info
//				break;
//			} else {
//				written += ret; // record number of bytes written
//			}
//			buf += written;
//		}
//		job->len = written;
//	}
//	// TODO do read
//}
//
//void TunInterface::post_write(uv_work_t *req, int status) {
//	writeReq *job = (writeReq *) req->data;
//
//	const unsigned argc = 2;
//	Local<Value> argv[argc];
//	argv[0] = Integer::New(job->len); // first param to call back is always amount of bytes written
//
//	if(job->_errno == 0) {
////		Buffer* rawbuffer = ObjectWrap<Buffer>(job->buffer);
//		if(!job->onSendSuccessCB->IsUndefined()) {
//			job->onSendSuccessCB->Call(Context::GetCurrent()->Global(),1,argv);
//		}
//	} else { // failure
//		if(!job->onSendFailureCB->IsUndefined()) {
//			argv[1] = _net::errno_to_JS(job->_errno,"Error in write(): ");
//			job->onSendFailureCB->Call(Context::GetCurrent()->Global(),2,argv);
//		}
//	}
//
//	delete job;
//}
//
//
//
//Handle<Value> TunInterface::Open(const Arguments& args) {
//	HandleScope scope;
//	TunInterface* obj = ObjectWrap::Unwrap<TunInterface>(args.This());
//
//	// FIXME - this only uses the fd created by Create() - we later should try to reopen a closed TUN device.
//
//	if(obj->_if_fd > 0) {
//		return scope.Close(Boolean::New(true));
//	} else
//		return scope.Close(Boolean::New(false));
//
//}
//
//Handle<Value> TunInterface::Close(const Arguments& args) {
//	HandleScope scope;
//	TunInterface* obj = ObjectWrap::Unwrap<TunInterface>(args.This());
//
//	if(obj->_if_fd > 0) {
//		if(close(obj->_if_fd) < 0) {
//			return scope.Close(Boolean::New(false));
//		} else
//			return scope.Close(Boolean::New(true));
//	} else {
//		obj->err.setError(_net::OTHER_ERROR, "not open!");
//		return scope.Close(Boolean::New(false));
//	}
//}
//
//
//
///**
// * Creates the TUN interface.
// */
//Handle<Value> TunInterface::Create(const Arguments& args) {
//	HandleScope scope;
//	TunInterface* obj = ObjectWrap::Unwrap<TunInterface>(args.This());
//
//	obj->err.clear();
//	obj->tun_create();
//
//	if(!obj->err.hasErr())
//		return scope.Close(Boolean::New(true));
//	else {
//		return scope.Close(Boolean::New(false));
//	}
//}














///**
// * Bring the interface up
// */
//Handle<Value> TunInterface::IfUp(const Arguments& args) {
//	HandleScope scope;
//	TunInterface* obj = ObjectWrap::Unwrap<TunInterface>(args.This());
//
//	char if_tmp_buf[255];
//
//	char *errstr = NULL;
//
//
//	int err = 0;
//	obj->_if_error = 0;
//	obj->tun_create();
//
//	if(!obj->_if_error)
//		return scope.Close(Boolean::New(true));
//	else {
//		return scope.Close(Boolean::New(false));
//	}
//}
//
///**
// * Bring the interface down
// */
//Handle<Value> TunInterface::IfDown(const Arguments& args) {
//	HandleScope scope;
//	TunInterface* obj = ObjectWrap::Unwrap<TunInterface>(args.This());
//
//	char if_tmp_buf[255];
//
//	char *errstr = NULL;
//
//
//	int err = 0;
//	obj->_if_error = 0;
//	obj->tun_create();
//
//	if(!obj->_if_error)
//		return scope.Close(Boolean::New(true));
//	else {
//		return scope.Close(Boolean::New(false));
//	}
//}
//
