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

Handle<Value> GreaseLogger::SetGlobalOpts(const Arguments& args) {
	HandleScope scope;

	GreaseLogger *l = GreaseLogger::setupClass();
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
		uv_mutex_lock(&l->modifyFilters);
		l->Opts.defaultFilterOut = v;
		uv_mutex_unlock(&l->modifyFilters);
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


bool GreaseLogger::sift(logMeta &f) { // returns true, then the logger should log it
	bool ret = false;
	static uint64_t zero = 0;
	if(Opts.levelFilterOutMask & f.level)
		return false;

	if(!META_HAS_CACHE(f)) {  // if the hashes aren't cached, then we have not done this...
		getHashes(f.tag,f.origin,f._cached_hash);

		uv_mutex_lock(&modifyFilters);
		FilterList *list;
		if(filterHashTable.find(f._cached_hash[0],list)) { // check both tag and orgin
			ret = true;
			META_SET_LIST(f,0,list);
		}
		if(!ret && filterHashTable.find(f._cached_hash[1],list)) { // check just tag
			ret = true;
			META_SET_LIST(f,1,list);
		}
		if(!ret && filterHashTable.find(f._cached_hash[2],list)) { // check just origin...
			ret = true;
			META_SET_LIST(f,2,list);
		}

		if(!ret && filterHashTable.find(zero,list)) {
			ret = true;
			META_SET_LIST(f,3,list);
		}

		if(!ret && Opts.defaultFilterOut)        // if neither, then do we output by default?
			ret = false;
		else
			ret = true;

		uv_mutex_unlock(&modifyFilters);
	} else
		ret = true;
	return ret;
}

GreaseLogger *GreaseLogger::LOGGER = NULL;

void GreaseLogger::handleInternalCmd(uv_async_t *handle, int status /*UNUSED*/) {
	GreaseLogger::internalCmdReq req;
	logTarget *t = NULL;
	while(GreaseLogger::LOGGER->internalCmdQueue.removeMv(req)) {
    	switch(req.c) {
    	case NEW_LOG:
    		{
    			// process a single log message
    			singleLog *l = (singleLog *) req.aux;
    			assert(l);
//    			FilterList *list = NULL;
    			if(GreaseLogger::LOGGER->sift(l->m)) { // should always be true (checked in v8 thread)
    				bool wrote = false;
    				for(int i=0;i<4;i++) {
    					FilterList *LIST = META_GET_LIST(l->m,i);
    					if (LIST && LIST->valid(l->m.level)) {
    						int n = 0;
    						while(LIST->list[n].id != 0) {
    							logTarget *t = NULL;
    							if((LIST->list[n].levelMask & l->m.level)) {
    								if(LIST->list[n].targetId == 0) {
    			    					//        						DBG_OUT("write out: %s",l->buf.handle.base);
    									GreaseLogger::LOGGER->defaultTarget->write(l->buf.handle.base,(uint32_t) l->buf.used,l->m, &LIST->list[n]);
    									wrote = true;
    								} else
    								if(GreaseLogger::LOGGER->targets.find(LIST->list[n].targetId, t)) {
    			    					//        						DBG_OUT("write out: %s",l->buf.handle.base);
        								t->write(l->buf.handle.base,(uint32_t) l->buf.used,l->m, &LIST->list[n]);
        								wrote = true;
    								} else {
    			    					//        						DBG_OUT("Orphaned target id: %d\n",list->list[n].targetId);
    								}
    							} else {
    							}
    							n++;
    						}
    					}
    				}
    				if(!wrote) {
        				GreaseLogger::LOGGER->defaultTarget->write(l->buf.handle.base,l->buf.used,l->m,&GreaseLogger::LOGGER->defaultFilter);
        			}
//        			}
//        			else {  // write out to default target if the list does not apply to this level
//        		//		HEAVY_DBG_OUT("pass thru to default");
//        				GreaseLogger::LOGGER->defaultTarget->write(l->buf.handle.base,(uint32_t)l->buf.used,l->m,&GreaseLogger::LOGGER->defaultFilter);
//        			}
    			}
    			l->clear();
    			GreaseLogger::LOGGER->masterBufferAvail.add(l); // place buffer back in queue
    		}
    		break;
    	case TARGET_ROTATE_BUFFER:
		    {
		    	if(req.d > 0)
		    		t = GreaseLogger::LOGGER->getTarget(req.d);
		    	else
		    		t = GreaseLogger::LOGGER->defaultTarget;
		    	assert(t);
				DBG_OUT("TARGET_ROTATE_BUFFER [%d]", req.auxInt);
				t->flushAll(false);
//		    	t->flush(req.auxInt);
		    }
			break;
		case WRITE_TARGET_OVERFLOW:
			{
		    	if(req.d > 0)
		    		t = GreaseLogger::LOGGER->getTarget(req.d);
		    	else
		    		t = GreaseLogger::LOGGER->defaultTarget;
		    	assert(t);
				DBG_OUT("WRITE_TARGET_OVERFLOW");
				t->flushAll();
				singleLog *big = (singleLog *) req.aux;
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
    		flushAllSync(true,true); // do rotation but no callbacks
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
//	FilterList *list = NULL;
	if(len > GREASE_MAX_MESSAGE_SIZE)
		return GREASE_OVERFLOW;
	logMeta m = f;
	if(sift(m)) {
		return _log(m,s,len);
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
	logMeta m = f;
	if(sift(m)) {
		return _logSync(f,s,len);
	} else
		return GREASE_OK;
}
void GreaseLogger::flushAll(bool nocallbacks) { // flushes buffers. Synchronous
	if(LOGGER->defaultTarget) {
		LOGGER->defaultTarget->flushAll();
	} else
		ERROR_OUT("No default target!");

	logTarget **t; // shut down other targets.
	GreaseLogger::TargetTable::HashIterator iter(LOGGER->targets);
	while(!iter.atEnd()) {
		t = iter.data();
		(*t)->flushAll(true,nocallbacks);
		iter.getNext();
	}
}
void GreaseLogger::flushAllSync(bool rotate, bool nocallbacks) { // flushes buffers. Synchronous
	if(LOGGER->defaultTarget) {
		LOGGER->defaultTarget->flushAllSync();
	} else
		ERROR_OUT("No default target!");

	logTarget **t; // shut down other targets.
	GreaseLogger::TargetTable::HashIterator iter(LOGGER->targets);
	while(!iter.atEnd()) {
		t = iter.data();
		(*t)->flushAllSync(rotate,nocallbacks);
		iter.getNext();
	}
}

GreaseLogger::logTarget::~logTarget() {

}

GreaseLogger::logTarget::logTarget(int buffer_size, uint32_t id, GreaseLogger *o,
		targetReadyCB cb, delim_data _delim, target_start_info *readydata) :
		readyCB(cb),                  // called when the target is ready or has failed to setup
		readyData(readydata),
		logCallbackSet(false),
		logCallback(),
		delim(std::move(_delim)),
		availBuffers(NUM_BANKS),
		writeOutBuffers(NUM_BANKS-1), // there should always be one buffer available for writingTo
		waitingOnCBBuffers(NUM_BANKS-1),
		err(), _log_fd(0),
		currentBuffer(NULL), bankSize(buffer_size), owner(o), myId(id),
		timeFormat(),tagFormat(),originFormat(),levelFormat(),preFormat(),postFormat()
{
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
	this->Opts.lock();
	int size = this->Opts.bufferSize;
	this->Opts.unlock();
	new ttyTarget(size, DEFAULT_TARGET, this, targetReady,std::move(defaultdelim), i);
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
			_doV8Callback(data);
		}
	}
}

void GreaseLogger::_doV8Callback(GreaseLogger::logTarget::writeCBData &data) {
	const unsigned argc = 2;
	Local<Value> argv[argc];
	argv[0] = String::New( data.b->handle.base, (int) data.b->handle.len );
	argv[1] = Integer::NewFromUnsigned( data.t->myId );
	data.t->logCallback->Call(Context::GetCurrent()->Global(),1,argv);
	data.t->finalizeV8Callback(data.b);
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

Handle<Value> GreaseLogger::createPTS(const Arguments& args) {
	HandleScope scope;

	GreaseLogger *l = GreaseLogger::setupClass();
	Local<Function> cb;

	if(args.Length() < 1 || !args[0]->IsFunction()) {
		return ThrowException(Exception::TypeError(String::New("createPTS: bad parameters")));
	} else {
		cb = Local<Function>::Cast(args[0]);
	}

	char *slavename = NULL;
	const unsigned argc = 2;
	Local<Value> argv[argc];

	_errcmn::err_ev err;

	int fdm = open("/dev/ptmx", O_RDWR);  // open master

	if(fdm < 0) {
		err.setError(errno);
	} else {
		if(grantpt(fdm) != 0) {  // change permission of slave
			err.setError(errno);
		} else {
			if(unlockpt(fdm) != 0) {                     // unlock slave
				err.setError(errno);
			}
			slavename = ptsname(fdm);      	   // get name of slave
		}
	}

	if(err.hasErr() || !slavename) {
		argv[0] = _errcmn::err_ev_to_JS(err, "Error in creating pseudo-terminal: ")->ToObject();
		cb->Call(Context::GetCurrent()->Global(),1,argv);
	} else {
		argv[0] = v8::Local<v8::Value>::New(v8::Null());

		Local<Object> obj = Object::New();
		obj->Set(String::New("fd"),Int32::New(fdm));
		obj->Set(String::New("path"),String::New(slavename,strlen(slavename)));
		argv[1] = obj;
		cb->Call(Context::GetCurrent()->Global(),2,argv);
	}

	return scope.Close(Undefined());
}



/**
 * addTagLabel(id,label)
 * @param args id is a number, label a string
 *
 * @return v8::Undefined
 */
Handle<Value> GreaseLogger::AddTagLabel(const Arguments& args) {
	HandleScope scope;

	GreaseLogger *l = GreaseLogger::setupClass();

	if(args.Length() > 1 && args[0]->IsUint32() && args[1]->IsString()) {
		v8::String::Utf8Value v8str(args[1]->ToString());
		logLabel *label = logLabel::fromUTF8(v8str.operator *(),v8str.length());
		l->tagLabels.addReplace(args[0]->Uint32Value(),label);
	} else {
		return ThrowException(Exception::TypeError(String::New("addTagLabel: bad parameters")));
	}

	return scope.Close(Undefined());
};

/**
 * addOriginLabel(id,label)
 * @param args id is a number, label a string
 *
 * @return v8::Undefined
 */
Handle<Value> GreaseLogger::AddOriginLabel(const Arguments& args) {
	HandleScope scope;

	GreaseLogger *l = GreaseLogger::setupClass();

	if(args.Length() > 1 && args[0]->IsUint32() && args[1]->IsString()) {
		v8::String::Utf8Value v8str(args[1]->ToString());
		logLabel *label = logLabel::fromUTF8(v8str.operator *(),v8str.length());
		l->originLabels.addReplace(args[0]->Uint32Value(),label);
	} else {
		return ThrowException(Exception::TypeError(String::New("addOriginLabel: bad parameters")));
	}

	return scope.Close(Undefined());
};


/**
 * addOriginLabel(id,label)
 * @param args id is a number, label a string
 *
 * @return v8::Undefined
 */
Handle<Value> GreaseLogger::AddLevelLabel(const Arguments& args) {
	HandleScope scope;

	GreaseLogger *l = GreaseLogger::setupClass();

	if(args.Length() > 1 && args[0]->IsUint32() && args[1]->IsString()) {
		v8::String::Utf8Value v8str(args[1]->ToString());
		logLabel *label = logLabel::fromUTF8(v8str.operator *(),v8str.length());
		uint32_t v = args[0]->Uint32Value();
		l->levelLabels.addReplace(v,label);
	} else {
		return ThrowException(Exception::TypeError(String::New("addLevelLabel: bad parameters")));
	}

	return scope.Close(Undefined());
};

Handle<Value> GreaseLogger::ModifyDefaultTarget(const Arguments& args) {
	HandleScope scope;

	if(args.Length() > 0 && args[0]->IsObject()){

		logTarget *targ = GreaseLogger::LOGGER->defaultTarget;
		Local<Object> jsTarg = args[0]->ToObject();

		Local<Value> jsDelim = jsTarg->Get(String::New("delim"));
		Local<Value> jsDelimOut = jsTarg->Get(String::New("delim_output"));
		// If either of these is set, the user means to change the default target
		Local<Value> jsTTY = jsTarg->Get(String::New("tty"));
		Local<Value> jsFile = jsTarg->Get(String::New("file"));

		bool makenewtarget = false;

		if(jsTTY->IsString()) {
			target_start_info *i = new target_start_info();

			i->cb = NULL;
			i->targId = DEFAULT_TARGET;


			delim_data defaultdelim;
			if(jsDelim->IsString()) {
				v8::String::Utf8Value v8str(jsDelim);
				defaultdelim.setDelim(v8str.operator *(),v8str.length());
			} else {
				defaultdelim.delim.sprintf("\n");
			}

			GreaseLogger::LOGGER->Opts.lock();
			int size = GreaseLogger::LOGGER->Opts.bufferSize;
			GreaseLogger::LOGGER->Opts.unlock();
			targ = new ttyTarget(size, DEFAULT_TARGET, GreaseLogger::LOGGER, targetReady,std::move(defaultdelim), i);

		} else if(jsFile->IsString()) {

			target_start_info *i = new target_start_info();

			delim_data defaultdelim;
			if(jsDelim->IsString()) {
				v8::String::Utf8Value v8str(jsDelim);
				defaultdelim.setDelim(v8str.operator *(),v8str.length());
			} else {
				defaultdelim.delim.sprintf("\n");
			}

			GreaseLogger::LOGGER->Opts.lock();
			int size = GreaseLogger::LOGGER->Opts.bufferSize;
			GreaseLogger::LOGGER->Opts.unlock();

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


			i->cb = NULL;
			i->targId = DEFAULT_TARGET;

			v8::String::Utf8Value v8str(jsFile);
			if(args.Length() > 0 && args[1]->IsFunction())
				i->targetStartCB = Persistent<Function>::New(Local<Function>::Cast(args[1]));
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
				targ = new fileTarget(size, DEFAULT_TARGET, GreaseLogger::LOGGER, flags, mode, v8str.operator *(), std::move(defaultdelim), i, targetReady, opts);
			else
				targ = new fileTarget(size, DEFAULT_TARGET, GreaseLogger::LOGGER, flags, mode, v8str.operator *(), std::move(defaultdelim), i, targetReady);

		} else {
			if(jsDelim->IsString()) {
				v8::String::Utf8Value v8str(jsDelim);
				targ->delim.setDelim(v8str.operator *(),v8str.length());
			}
		}


		Local<Value> jsFormat = jsTarg->Get(String::New("format"));

		if(jsFormat->IsObject()) {
			Local<Object> jsObj = jsFormat->ToObject();
			Local<Value> jsKey = jsObj->Get(String::New("pre"));
			if(jsKey->IsString()) {
				v8::String::Utf8Value v8str(jsKey);
				targ->setPreFormat(v8str.operator *(),v8str.length());
			}
			jsKey = jsObj->Get(String::New("time"));
			if(jsKey->IsString()) {
				v8::String::Utf8Value v8str(jsKey);
				targ->setTimeFormat(v8str.operator *(),v8str.length());
			}
			jsKey = jsObj->Get(String::New("level"));
			if(jsKey->IsString()) {
				v8::String::Utf8Value v8str(jsKey);
				targ->setLevelFormat(v8str.operator *(),v8str.length());
			}
			jsKey = jsObj->Get(String::New("tag"));
			if(jsKey->IsString()) {
				v8::String::Utf8Value v8str(jsKey);
				targ->setTagFormat(v8str.operator *(),v8str.length());
			}
			jsKey = jsObj->Get(String::New("origin"));
			if(jsKey->IsString()) {
				v8::String::Utf8Value v8str(jsKey);
				targ->setOriginFormat(v8str.operator *(),v8str.length());
			}
			jsKey = jsObj->Get(String::New("post"));
			if(jsKey->IsString()) {
				v8::String::Utf8Value v8str(jsKey);
				targ->setPostFormat(v8str.operator *(),v8str.length());
			}
		}
	}
	return scope.Close(Undefined());
}

/**
 * addFilter(obj) where
 * obj = {
 *      // at least origin and/or tag must be present
 *      origin: 0x33,    // any positive number > 0
 *      tag: 0x25        // any positive number > 0,
 *      target: 3,       // mandatory
 *      mask:  0x4000000 // optional (default, show everything: 0xFFFFFFF),
 *      format: {        // optional (formatting settings)
 *      	pre: 'targ-pre>', // optional pre string
 *      	post: '<targ-post;
 *      }
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
//			ok = false;
			targetId = 0;
		}

		if(!ok) {
			return ThrowException(Exception::TypeError(String::New("addFilter: bad parameters")));
		}

		logLabel *preFormat = NULL;
		logLabel *postFormat = NULL;
		Local<Value> jsKey = jsObj->Get(String::New("pre"));
		if(jsKey->IsString()) {
			v8::String::Utf8Value v8str(jsKey);
			preFormat = logLabel::fromUTF8(v8str.operator *(),v8str.length());
		}
		jsKey = jsObj->Get(String::New("post"));
		if(jsKey->IsString()) {
			v8::String::Utf8Value v8str(jsKey);
			postFormat= logLabel::fromUTF8(v8str.operator *(),v8str.length());
		}

		if(l->_addFilter(targetId,originId,tagId,mask,id,preFormat,postFormat))
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
 *    delim: "\n",    // optional. The delimitter between log entries. Any UTF8 string is fine
 *    format: {
 *       time: "[%ld]",
 *       level: "<%s>"
 *    }
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

		l->Opts.lock();
		int buffsize = l->Opts.bufferSize;
		l->Opts.unlock();
		Local<Value> jsBufSize = jsTarg->Get(String::New("bufferSize"));
		if(jsBufSize->IsInt32()) {
			buffsize = jsBufSize->Int32Value();
			if(buffsize < 0) {
				l->Opts.lock();
				buffsize = l->Opts.bufferSize;
				l->Opts.unlock();
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
		Local<Value> jsFormat = jsTarg->Get(String::New("format"));
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
		} else if (!isTty.IsEmpty() && isTty->IsInt32()) {
			target_start_info *i = new target_start_info();
//			i->system_start_info = data;
			i->cb = start_target_cb;
			i->targId = id;
			if(args.Length() > 0 && args[1]->IsFunction())
				i->targetStartCB = Persistent<Function>::New(Local<Function>::Cast(args[1]));
			targ = ttyTarget::makeTTYTargetFromFD(isTty->ToInt32()->Value(), buffsize, id, l, targetReady,  std::move(delims), i);
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

		} else if(jsCallback->IsFunction()) { // if not those, bu callback is set, then just make a 'do nothing' target - but the callback will get used
			target_start_info *i = new target_start_info();
//			i->system_start_info = data;
			i->cb = start_target_cb;
			i->targId = id;
			if(args.Length() > 0 && args[1]->IsFunction())
				i->targetStartCB = Persistent<Function>::New(Local<Function>::Cast(args[1]));
//			int buffer_size, uint32_t id, GreaseLogger *o,
//							targetReadyCB cb, delim_data &_delim, target_start_info *readydata
			targ = new callbackTarget(buffsize, id, l, targetReady, std::move(delims), i);
			_errcmn::err_ev err;
			targ->readyCB(true,err,targ);
		} else {
			return ThrowException(Exception::TypeError(String::New("Unknown target type passed into addTarget()")));
		}

		if(jsCallback->IsFunction() && targ) {
			Local<Function> f = Local<Function>::Cast(jsCallback);
			targ->setCallback(f);
		}

		if(jsFormat->IsObject()) {
			Local<Object> jsObj = jsFormat->ToObject();
			Local<Value> jsKey = jsObj->Get(String::New("pre"));
			if(jsKey->IsString()) {
				v8::String::Utf8Value v8str(jsKey);
				targ->setPreFormat(v8str.operator *(),v8str.length());
			}
			jsKey = jsObj->Get(String::New("time"));
			if(jsKey->IsString()) {
				v8::String::Utf8Value v8str(jsKey);
				targ->setTimeFormat(v8str.operator *(),v8str.length());
			}
			jsKey = jsObj->Get(String::New("level"));
			if(jsKey->IsString()) {
				v8::String::Utf8Value v8str(jsKey);
				targ->setLevelFormat(v8str.operator *(),v8str.length());
			}
			jsKey = jsObj->Get(String::New("tag"));
			if(jsKey->IsString()) {
				v8::String::Utf8Value v8str(jsKey);
				targ->setTagFormat(v8str.operator *(),v8str.length());
			}
			jsKey = jsObj->Get(String::New("origin"));
			if(jsKey->IsString()) {
				v8::String::Utf8Value v8str(jsKey);
				targ->setOriginFormat(v8str.operator *(),v8str.length());
			}
			jsKey = jsObj->Get(String::New("post"));
			if(jsKey->IsString()) {
				v8::String::Utf8Value v8str(jsKey);
				targ->setPostFormat(v8str.operator *(),v8str.length());
			}
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
	ZERO_LOGMETA(meta);
	HandleScope scope;
	uint32_t target = DEFAULT_TARGET;
	if(args.Length() > 1 && args[0]->IsString() && args[1]->IsInt32()){
		GreaseLogger *l = GreaseLogger::setupClass();
		v8::String::Utf8Value v8str(args[0]->ToString());
		meta.level = (uint32_t) args[1]->Int32Value(); // level

		if(args.Length() > 2 && args[2]->IsInt32()) // tag
			meta.tag = (uint32_t) args[2]->Int32Value();
		else
			meta.tag = 0;

		if(args.Length() > 3 && args[3]->IsInt32()) // origin
			meta.origin = (uint32_t) args[3]->Int32Value();
		else
			meta.origin = 0;

		FilterList *list = NULL;
		if(l->sift(meta)) {
			l->_log(meta,v8str.operator *(),v8str.length());
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
		if(l->sift(meta)) {
			l->_logSync(meta,v8str.operator *(),v8str.length());
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

int GreaseLogger::_log( const logMeta &meta, const char *s, int len) { // internal log cmd
//	HEAVY_DBG_OUT("out len: %d\n",len);
//	DBG_OUT("meta.level %x",meta.level);
//	if(len > GREASE_MAX_MESSAGE_SIZE)
//		return GREASE_OVERFLOW;

	singleLog *l = NULL;
	if(masterBufferAvail.remove(l)) {
		internalCmdReq req(NEW_LOG);
		l->buf.memcpy(s,len);
		l->m = meta;
		req.aux = l;
		if(internalCmdQueue.addMvIfRoom(req))
			uv_async_send(&asyncInternalCommand);
		else
			ERROR_OUT("internalCmdQueue is out of space!! Dropping. \n");
	} else {
		ERROR_OUT("masterBuffer is out of space!! Dropping. (%d)\n", masterBufferAvail.remaining());
	}


//	if(!list) {
//		defaultTarget->write(s,len,meta);
//	} else if (list->valid(meta.level)) {
//		int n = 0;
//		while(list->list[n].id != 0) {
//			logTarget *t = NULL;
//			if((list->list[n].levelMask & meta.level) && targets.find(list->list[n].targetId, t)) {
//				t->write(s,len,meta);
//			} else {
//				ERROR_OUT("Orphaned target id: %d\n",list->list[n].targetId);
//			}
//			n++;
//		}
//	} else {  // write out to default target if the list does not apply to this level
//		defaultTarget->write(s,len,meta);
//	}
	return GREASE_OK;
}

int GreaseLogger::_logSync( const logMeta &meta, const char *s, int len) { // internal log cmd
	if(len > GREASE_MAX_MESSAGE_SIZE)
		return GREASE_OVERFLOW;
// FIXME FIXME FIXME
	//	if(!list) {
//		defaultTarget->writeSync(s,len,meta);
//	} else {
//		int n = 0;
//		while(list->list[n].id != 0) {
//			logTarget *t = NULL;
//			if(targets.find(list->list[n].targetId, t)) {
//				t->writeSync(s,len,meta);
//			} else {
//				ERROR_OUT("Orphaned target id: %d\n",list->list[n].targetId);
//			}
//			n++;
//		}
//	}
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

	tpl->InstanceTemplate()->Set(String::NewSymbol("createPTS"), FunctionTemplate::New(createPTS)->GetFunction());

	tpl->InstanceTemplate()->Set(String::NewSymbol("setGlobalOpts"), FunctionTemplate::New(SetGlobalOpts)->GetFunction());

	tpl->InstanceTemplate()->Set(String::NewSymbol("addTagLabel"), FunctionTemplate::New(AddTagLabel)->GetFunction());
	tpl->InstanceTemplate()->Set(String::NewSymbol("addOriginLabel"), FunctionTemplate::New(AddOriginLabel)->GetFunction());
	tpl->InstanceTemplate()->Set(String::NewSymbol("addLevelLabel"), FunctionTemplate::New(AddLevelLabel)->GetFunction());

	tpl->InstanceTemplate()->Set(String::NewSymbol("modifyDefaultTarget"), FunctionTemplate::New(ModifyDefaultTarget)->GetFunction());

	tpl->InstanceTemplate()->Set(String::NewSymbol("addFilter"), FunctionTemplate::New(AddFilter)->GetFunction());
	tpl->InstanceTemplate()->Set(String::NewSymbol("removeFilter"), FunctionTemplate::New(RemoveFilter)->GetFunction());
	tpl->InstanceTemplate()->Set(String::NewSymbol("addTarget"), FunctionTemplate::New(AddTarget)->GetFunction());
	tpl->InstanceTemplate()->Set(String::NewSymbol("addSink"), FunctionTemplate::New(AddSink)->GetFunction());

//	tpl->InstanceTemplate()->SetAccessor(String::New("lastErrorStr"), GetLastErrorStr, SetLastErrorStr);
//	tpl->InstanceTemplate()->SetAccessor(String::New("_readChunkSize"), GetReadChunkSize, SetReadChunkSize);

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
		if(args.Length() > 0) {
			if(!args[0]->IsObject()) {
				return ThrowException(Exception::TypeError(String::New("Improper first arg to TunInterface cstor. Must be an object.")));
			}

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

