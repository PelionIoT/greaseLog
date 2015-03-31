/*
 * GreaseLogger.h
 *
 *  Created on: Aug 27, 2014
 *      Author: ed
 * (c) 2014, Framez Inc
 */
#ifndef GreaseLogger_H_
#define GreaseLogger_H_

#include <v8.h>
#include <node.h>
#include <uv.h>
#include <node_buffer.h>

using namespace node;
using namespace v8;

#include <sys/types.h>
#include <sys/socket.h>
#include <linux/if.h>
#include <linux/if_tun.h>
#include <unistd.h>
#include <sys/ioctl.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <linux/fs.h>

#include <string.h>
#include <stdlib.h>
#include <stdio.h>
#include <uv.h>

#include <TW/tw_alloc.h>
#include <TW/tw_fifo.h>
#include <TW/tw_khash.h>
#include <TW/tw_circular.h>

#include "error-common.h"

#include <google/tcmalloc.h>

#define LMALLOC tc_malloc_skip_new_handler
#define LCALLOC tc_calloc
#define LREALLOC tc_realloc
#define LFREE tc_free

namespace Grease {

typedef uint64_t FilterHash;   // format: [N1N2] where N1 is [Tag id] and N2 is [Origin Id]
typedef uint32_t FilterId;     // filter id is always > 0
typedef uint32_t TargetId;     // id is always > 0
typedef uint32_t OriginId;     // id is always > 0
typedef uint32_t TagId;        // id is always > 0
typedef uint32_t LevelMask;    // id is always > 0

using namespace TWlib;

struct Alloc_LMALLOC {
	static void *malloc (tw_size nbytes) {  return LMALLOC((int) nbytes); }
	static void *calloc (tw_size nelem, tw_size elemsize) { return LCALLOC((size_t) nelem,(size_t) elemsize); }
	static void *realloc(void *d, tw_size s) { return LREALLOC(d,(size_t) s); }
	static void free(void *p) { LFREE(p); }
	static void sync(void *addr, tw_size len, int flags = 0) { } // does nothing - not shared memory
	static void *memcpy(void *d, const void *s, size_t n) { return ::memcpy(d,s,n); };
	static void *memmove(void *d, const void *s, size_t n) { return ::memmove(d,s,n); };
	static int memcmp(void *l, void *r, size_t n) { return ::memcmp(l, r, n); }
	static void *memset(void *d, int c, size_t n) { return ::memset(d,c,n); }
	static const char *ALLOC_NOMEM_ERROR_MESSAGE;
};


typedef TWlib::Allocator<Alloc_LMALLOC> LoggerAlloc;


struct uint32_t_eqstrP {
	  inline int operator() (const uint32_t *l, const uint32_t *r) const
	  {
		  return (*l == *r);
	  }
};

struct TargetId_eqstrP {
	  inline int operator() (const TargetId *l, const TargetId *r) const
	  {
		  return (*l == *r);
	  }
};

struct uint64_t_eqstrP {
	  inline int operator() (const uint64_t *l, const uint64_t *r) const
	  {
		  return (*l == *r);
	  }
};

// http://broken.build/2012/11/10/magical-container_of-macro/
// http://www.kroah.com/log/linux/container_of.html
#ifndef offsetof
	#define offsetof(TYPE, MEMBER) ((size_t) &((TYPE *)0)->MEMBER)
#endif
#ifndef container_of
#define container_of(ptr, type, member) ({ \
                const typeof( ((type *)0)->member ) *__mptr = (ptr); \
                (type *)( (char *)__mptr - offsetof(type,member) );})
#endif

#define ERROR_UV(msg, code, loop) do {                                              \
  uv_err_t __err = uv_last_error(loop);                                             \
  fprintf(stderr, "%s: [%s: %s]\n", msg, uv_err_name(__err), uv_strerror(__err));   \
} while(0);

//  assert(0);

#define COMMAND_QUEUE_NODE_SIZE 200
#define INTERNAL_QUEUE_SIZE 200
#define V8_LOG_CALLBACK_QUEUE_SIZE 10
#define MAX_TARGET_CALLBACK_STACK 20
#define TARGET_CALLBACK_QUEUE_WAIT 2000000  // 2 seconds
#define MAX_ROTATED_FILES 10

#define LOGGER_DEFAULT_CHUNK_SIZE  1500
#define DEFAULT_BUFFER_SIZE  2000

#define DEFAULT_TARGET 0
#define DEFAULT_ID 0

#define ALL_LEVELS 0xFFFFFFFF
#define GREASE_STDOUT 1
#define GREASE_STDERR 2
#define GREASE_BAD_FD -1

#define TTY_NORMAL 0
#define TTY_RAW    1

#define NOTREADABLE 0
#define READABLE    1

#define NUM_BANKS 3



#define DEFAULT_MODE_FILE_TARGET (S_IRUSR | S_IWUSR | S_IRGRP | S_IWGRP)
#define DEFAULT_FLAGS_FILE_TARGET (O_APPEND | O_CREAT | O_WRONLY)

#define LOGGER_HEAVY_DEBUG 1
#define MAX_IDENTICAL_FILTERS 16

#ifdef LOGGER_HEAVY_DEBUG
#pragma message "Build is Debug Heavy!!"
// confused? here: https://gcc.gnu.org/onlinedocs/cpp/Variadic-Macros.html
#define HEAVY_DBG_OUT(s,...) fprintf(stderr, "**DEBUG** " s "\n", ##__VA_ARGS__ )
#define IF_HEAVY_DBG( x ) { x }
#else
#define HEAVY_DBG_OUT(s,...) {}
#define IF_DBG( x ) {}
#endif

const int MAX_IF_NAME_LEN = 16;


class GreaseLogger : public node::ObjectWrap {
public:
	typedef void (*actionCB)(GreaseLogger *, _errcmn::err_ev &err, void *data);

//	struct start_info {
//		Handle<Function> callback;
//		start_info() : callback() {}
//	};

	class target_start_info final {
	public:
		bool needsAsyncQueue; // if the callback must be called in the v8 thread
		_errcmn::err_ev err;      // used if above is true
		actionCB cb;
//		start_info *system_start_info;
		Handle<Function> targetStartCB;
		TargetId targId;
		target_start_info() : needsAsyncQueue(false), err(), cb(NULL), targetStartCB(), targId(0) {}
		target_start_info& operator=(target_start_info&& o) {
			if(o.err.hasErr())
				err = std::move(o.err);
			cb = o.cb; o.cb = NULL;
			targetStartCB = o.targetStartCB; o.targetStartCB.Clear();
			targId = o.targId;
			return *this;
		}
	};

protected:
	static GreaseLogger *LOGGER;  // this class is a Singleton
	int bufferSize;  // size of each buffer
	int chunkSize;

	uv_thread_t logThreadId;
	uv_async_t asyncV8LogCallback;  // used to wakeup v8 to call log callbacks (see v8LogCallbacks)
	uv_async_t asyncTargetCallback;
	TWlib::tw_safeCircular<target_start_info  *, LoggerAlloc > targetCallbackQueue;
	_errcmn::err_ev err;

	// Definitions:
	// Target - a final place a log entry goes: TTY, file, etc.
	// Filter - a condition, which if matching, will cause a log to go to a target
	// FilterHash - a number used to point to one or more filters
	// Log entry - the log entry (1) - synonymous with a single call to log() or logSync()
	// Meta - the data in a log Entry beyond the message itself

	// Default target - where stuff goes if no matching filter is found
	//

    //
	// filter id -> { [N1:0 0:N2 N1:N2], [level mask], [target] }

	// table:tag -> N1  --> [filter id list]
	// table:origin -> N2  --> [filter id list]
	// search N1:0 0:N2 N1:N2 --> [filter id list]
	// filterMasterTable: uint64_t -> [ filter id list ]

	// { tag: "mystuff", origin: "crazy.js", level" 0x02 }


	uv_mutex_t nextIdMutex;
	FilterId nextFilterId;
	TargetId nextTargetId;


	class Filter final {
	public:
		FilterId id;
		LevelMask levelMask;
		TargetId targetId;
		Filter() {
			id = 0;  // an ID of 0 means - empty Filter
			levelMask = 0;
			targetId = 0;
		};
		Filter(FilterId id, LevelMask mask, TargetId t) : id(id), levelMask(mask), targetId(t) {}
//		Filter(Filter &&o) : id(o.id), levelMask(o.mask), targetId(o.t) {};
		Filter(Filter &o) :  id(o.id), levelMask(o.levelMask), targetId(o.targetId) {};
		Filter& operator=(Filter& o) {
			id = o.id;
			levelMask = o.levelMask;
			targetId = o.targetId;
			return *this;
		}
	};

	class FilterList final {
	public:
		Filter list[MAX_IDENTICAL_FILTERS]; // a non-zero entry is valid. when encountering the first zero, the rest array elements are skipped
		LevelMask bloom; // this is the bloom filter for the list. If this does not match, the list does not log this level
		FilterList() : bloom(0) {}
		inline bool valid(LevelMask m) {
			return (bloom & m);
		}
		inline bool add(Filter &filter) {
			bool ret = false;
			for(int n=0;n<MAX_IDENTICAL_FILTERS;n++) {
				if(list[n].id == 0) {
					bloom |= filter.levelMask;
					list[n] = filter;
					ret = true;
					break;
				}
			}
			return ret;
		}
	};




	class logMeta final {   // meta data for each log entry
	public:
		uint32_t tag;    // 0 means no tag
		uint32_t level;  // 0 means no level
		uint32_t origin; // 0 means no origin

		uint32_t target; // 0 means default target
	};

	struct logBuf {
		uv_buf_t handle;
		uv_mutex_t mutex;
		int id;
		int space;
		logBuf(int s, int id) : id(id), space(s) {
			handle.base = (char *) LMALLOC(space);
			handle.len = 0;
			uv_mutex_init(&mutex);
		}
		logBuf() = delete;
		~logBuf() {
			if(handle.base) LFREE(handle.base);
		}
		void copyIn(char *s, int n) {
			uv_mutex_lock(&mutex);
			memcpy((void *) (handle.base + handle.len), s, n);
			handle.len += n;
			uv_mutex_unlock(&mutex);
		}
		void clear() {
			uv_mutex_lock(&mutex);
			handle.len = 0;
			uv_mutex_unlock(&mutex);
		}
		int remain() {
			int ret = 0;
			uv_mutex_lock(&mutex);
			ret = space - handle.len;
			uv_mutex_unlock(&mutex);
			return ret;
		}
	};

	struct overflowBuf final {
		uv_buf_t handle;
		overflowBuf(char *d, int n) {
			handle.len = n;
			handle.base = (char *) LMALLOC(n);
			memcpy(handle.base,d,n);
		}
		overflowBuf() = delete;
		~overflowBuf() {
			if(handle.base) LFREE(handle.base);
		}
	};

	enum nodeCommand {
		NOOP,
		GLOBAL_FLUSH,  // flush all targets
		SHUTDOWN
//		ADD_TARGET,
//		CHANGE_FILTER
	};

	enum internalCommand {
		INTERNALNOOP,
		TARGET_ROTATE_BUFFER,   // we had to rotate buffers on a target
		WRITE_TARGET_OVERFLOW,   // too big to fit in buffer... will flush existing target, and then make large write.
		INTERNAL_SHUTDOWN
	};
//	class data {
//	public:
//		int x;
//		data() : x(0) {}
//		data(data &) = delete;
//		data(data &&o) : x(o.x) { o.x = 0; }
//		data& operator=(data&& other) {
//		     x = other.x;
//		     other.x = 0;
//		     return *this;
//		}
//	};

	struct internalCmdReq final {
		internalCommand c;
		uint32_t d;
		void *aux;
		int auxInt;
		internalCmdReq(internalCommand _c, uint32_t _d = 0): c(_c), d(_d), aux(NULL), auxInt(0) {}
		internalCmdReq(internalCmdReq &) = delete;
		internalCmdReq() : c(INTERNALNOOP), d(0), aux(NULL), auxInt(0) {}
		internalCmdReq(internalCmdReq &&o) : c(o.c), d(o.d), aux(o.aux), auxInt(o.auxInt) {};
		internalCmdReq& operator=(internalCmdReq&& other) {
			c = other.c;
			d = other.d;
			aux = other.aux;
			auxInt = other.auxInt;
			return *this;
		}
	};

	struct logReq final {
		uv_work_t work;
		int _errno; // the errno that happened on read if an error occurred.
//		v8::Persistent<Function> onSendSuccessCB;
//		v8::Persistent<Function> onSendFailureCB;
//		v8::Persistent<Object> buffer; // Buffer object passed in
//		char *_backing; // backing of the passed in Buffer
//		int len;
		GreaseLogger *self;
		// need Buffer
		logReq(GreaseLogger *i) : _errno(0),
//				onSendSuccessCB(), onSendFailureCB(), buffer(), _backing(NULL), len(0),
				self(i) {
			work.data = this;
		}
		logReq() = delete;
	};

	struct nodeCmdReq final {
//		uv_work_t work;
		nodeCommand cmd;
		int _errno; // the errno that happened on read if an error occurred.
		v8::Persistent<Function> callback;
//		v8::Persistent<Function> onFailureCB;
//		v8::Persistent<Object> buffer; // Buffer object passed in
//		char *_backing; // backing of the passed in Buffer
//		int len;
		GreaseLogger *self;
		// need Buffer
		nodeCmdReq(nodeCommand c, GreaseLogger *i) : cmd(c), _errno(0),
				callback(),
//				onSuccessCB(), onFailureCB(),
//				buffer(), _backing(NULL), len(0),
				self(i) {
//			work.data = this;
		}
		nodeCmdReq(nodeCmdReq &) = delete;
		nodeCmdReq() : cmd(nodeCommand::NOOP), _errno(0), callback(), self(NULL) {}
		nodeCmdReq(nodeCmdReq &&o) :cmd(o.cmd), _errno(o._errno), callback(), self(o.self) {
			if(!o.callback.IsEmpty()) {
				callback = o.callback; o.callback.Clear();
			}
		};
		nodeCmdReq& operator=(nodeCmdReq&& o) {
			cmd = o.cmd;
			_errno = o._errno;
			if(!o.callback.IsEmpty()) {
				callback = o.callback; o.callback.Clear();
			}
			return *this;
		}
	};


	class logTarget {
	public:
		typedef void (*targetReadyCB)(bool ready, _errcmn::err_ev &err, logTarget *t);
		targetReadyCB readyCB;
		target_start_info *readyData;
		logBuf *_buffers[NUM_BANKS];
		bool logCallbackSet; // if true, logCallback (below) is set (we prefer to not touch any v8 stuff, when outside the v8 thread)
		Persistent<Function>  logCallback;
		// Queue flow:
		// 1) un-used logBuf taken out of availBuffers, filled with data
		// 2) placed in writeOutBuffers for write out
		// 2) logBuf gets written out (usually in a separate thread)
		// 3) after being wirrten out, if no 'logCallbac' is assigned, logBuf is returned to availBuffers, OR
		//    if logCallback is set, then it is queued in waitingOnCBBuffers
		// 4) when v8 thread becomes active, callback is called, then logBuf is returned to availBuffers
		TWlib::tw_safeCircular<logBuf *, LoggerAlloc > availBuffers; // buffers which are available for writing
		TWlib::tw_safeCircular<logBuf *, LoggerAlloc > writeOutBuffers; // buffers which are available for writing
		TWlib::tw_safeCircular<logBuf *, LoggerAlloc > waitingOnCBBuffers; // buffers which are available for writing
		_errcmn::err_ev err;
		int _log_fd;

		uv_mutex_t writeMutex;
//		int logto_n;    // log to use to buffer log.X calls. The 'current Buffer' index in _buffers
		logBuf *currentBuffer;
		int bankSize;
		GreaseLogger *owner;
		uint32_t myId;
		logTarget(int buffer_size, uint32_t id, GreaseLogger *o, targetReadyCB cb, target_start_info *readydata = NULL);
		logTarget() = delete;

		class writeCBData final {
		public:
			logTarget *t;
			logBuf *b;
			writeCBData() : t(NULL), b(NULL) {}
			writeCBData(logTarget *_t, logBuf *_b) : t(_t), b(_b) {}
			writeCBData& operator=(writeCBData&& o) {
				b = o.b;
				t = o.t;
				return *this;
			}
		};

		// called when the V8 callback is done
		void finalizeV8Callback(logBuf *b) {
			b->clear();
			availBuffers.add(b);
		}

		void returnBuffer(logBuf *b) {
			if(logCallbackSet && b->handle.len > 0) {
				writeCBData cbdat(this,b);
				if(!owner->v8LogCallbacks.addMvIfRoom(cbdat)) {
					ERROR_OUT(" !!! v8LogCallbacks is full! Can't rotate. Callback will be skipped.");
				}
				uv_async_send(&owner->asyncV8LogCallback);
			} else {
				b->clear();
				availBuffers.add(b);
			}
		}

		void setCallback(Local<Function> &func) {
			uv_mutex_lock(&writeMutex);
			if(!func.IsEmpty()) {
				logCallbackSet = true;
				logCallback = Persistent<Function>::New(func);
			}
			uv_mutex_unlock(&writeMutex);
		}

		bool rotate() {
			bool ret = false;
			logBuf *next = NULL;
			uv_mutex_lock(&writeMutex);
			if(currentBuffer->handle.len > 0) {
				ret = availBuffers.remove(next);  // won't block
				if(ret) {
					ret = writeOutBuffers.addIfRoom(currentBuffer); // won't block
					if(ret) {
						currentBuffer = next;
					} else {
						ERROR_OUT(" !!! writeOutBuffers is full! Can't rotate. Data will be lost.");
						if(!availBuffers.addIfRoom(next)) {  // won't block
							ERROR_OUT(" !!!!!!!! CRITICAL - can't put Buffer back in availBuffers. Losing Buffer ?!@?!#!@");
						}
						currentBuffer->clear();
					}
				} else {
					ERROR_OUT(" !!! Can't rotate. NO BUFFERS - Overwriting buffer!! Data will be lost. [target %d]", myId);
					currentBuffer->clear();
				}
			} else {
				DBG_OUT("Skipping rotation. No data in current buffer.");
			}
//			if(!ret) {
//				ERROR_OUT(" !!! Can't rotate. Overwriting buffer!! Data will be lost.");
//			}
			uv_mutex_unlock(&writeMutex);
			return ret;
		}

		void write(char *s, int len) {  // called from node thread...
			if(currentBuffer->remain() >= len) {
				uv_mutex_lock(&writeMutex);
				currentBuffer->copyIn(s,len);
				uv_mutex_unlock(&writeMutex);
			} else if(bankSize < len) {
				overflowBuf *B = new overflowBuf(s,len);
				internalCmdReq req(WRITE_TARGET_OVERFLOW,myId);
				req.aux = B;
				if(owner->internalCmdQueue.addMvIfRoom(req))
					uv_async_send(&owner->asyncInternalCommand);
				else {
					ERROR_OUT("Overflow. Dropping WRITE_TARGET_OVERFLOW");
					delete B;
				}
			} else {
				bool rotated = false;
				internalCmdReq req(TARGET_ROTATE_BUFFER,myId); // just rotated off a buffer
				rotated = rotate();
				if(rotated) {
					if(owner->internalCmdQueue.addMvIfRoom(req)) {
						int id = 0;
						uv_mutex_lock(&writeMutex);
						currentBuffer->copyIn(s,len);
						id = currentBuffer->id;
						uv_mutex_unlock(&writeMutex);
						DBG_OUT("Request ROTATE [%d] [target %d]", id, myId);
						uv_async_send(&owner->asyncInternalCommand);
					} else {
						ERROR_OUT("Overflow. Dropping TARGET_ROTATE_BUFFER");
					}
				}
			}
		}

		void flushAll(bool _rotate = true) {
			if(_rotate) rotate();
			while(1) {
				logBuf *b = NULL;
				if(writeOutBuffers.remove(b)) {
					flush(b);
				} else
					break;
			}
			sync();
		}

		void flushAllSync(bool _rotate = true) {
			HEAVY_DBG_OUT("flushAllSync()");
			if(_rotate) rotate();
			while(1) {
				logBuf *b = NULL;
				if(writeOutBuffers.remove(b)) {
					flushSync(b);
				} else
					break;
			}
			sync();
		}

		// called from Logger thread ONLY
		virtual void flush(logBuf *b) {}; // flush buffer 'n'. This is ansynchronous
		virtual void flushSync(logBuf *b) {}; // flush buffer 'n'. This is ansynchronous
		virtual void writeAsync(overflowBuf *b) {};
		virtual void writeSync(char *s, int len) {}; // flush buffer 'n'. This is synchronous. Writes now - skips buffering
		virtual void close() {};
		virtual void sync() {};
		virtual ~logTarget();
	};

	// small helper class used to call log callbacks in v8 thread
//	class v8LogCallback final {
//	public:
//		logBuf *b;
//		logTarget *t;
//		v8LogCallback() : b(NULL), t(NULL) {}
//	};

	class ttyTarget final : public logTarget {
	public:
		int ttyFD;
		uv_tty_t tty;
		static void write_cb(uv_write_t* req, int status) {
//			HEAVY_DBG_OUT("write_cb");
			writeCBData *d = (writeCBData *) req->data;
			d->t->returnBuffer(d->b);

			delete req;
		}
		static void write_overflow_cb(uv_write_t* req, int status) {
			HEAVY_DBG_OUT("overflow_cb");
			overflowBuf *b = (overflowBuf *) req->data;
			delete b;
			delete req;
		}


		uv_write_t outReq;  // since this function is no re-entrant (below) we can do this
		void flush(logBuf *b) { // this will always be called by the logger thread (via uv_async_send)
			uv_write_t *req = new uv_write_t;
			writeCBData *d = new writeCBData;
			d->t = this;
			d->b = b;
			req->data = d;
			uv_mutex_lock(&b->mutex);  // this lock should be ok, b/c write is async
			HEAVY_DBG_OUT("TTY: flush() %d bytes", b->handle.len);
			uv_write(req, (uv_stream_t *) &tty, &b->handle, 1, write_cb);
			uv_mutex_unlock(&b->mutex);
//			b->clear(); // NOTE - there is a risk that this could get overwritten before it is written out.
		}
		void flushSync(logBuf *b) { // this will always be called by the logger thread (via uv_async_send)
			uv_write_t *req = new uv_write_t;
			writeCBData *d = new writeCBData;
			d->t = this;
			d->b = b;
			req->data = d;
			uv_mutex_lock(&b->mutex);  // this lock should be ok, b/c write is async
			HEAVY_DBG_OUT("TTY: flushSync() %d bytes", b->handle.len);
			uv_write(req, (uv_stream_t *) &tty, &b->handle, 1, NULL);
			uv_mutex_unlock(&b->mutex);
//			b->clear(); // NOTE - there is a risk that this could get overwritten before it is written out.
		}
		void writeAsync(overflowBuf *b) {
			uv_write_t *req = new uv_write_t;
			req->data = b;
			uv_write(req, (uv_stream_t *) &tty, &b->handle, 1, write_overflow_cb);
		}
		void writeSync(char *s, int l) {
			uv_write_t *req = new uv_write_t;
			uv_buf_t buf;
			buf.base = s;
			buf.len = l;
			uv_write(req, (uv_stream_t *) &tty, &buf, 1, NULL);
		}
		ttyTarget(int buffer_size, uint32_t id, GreaseLogger *o, targetReadyCB cb, target_start_info *readydata = NULL, char *ttypath = NULL) : logTarget(buffer_size, id, o, cb, readydata), ttyFD(0)  {
//			outReq.cb = write_cb;
			_errcmn::err_ev err;
			if(ttypath)
				ttyFD = ::open(ttypath, O_WRONLY, 0);
			else
				ttyFD = ::open("/dev/tty", O_WRONLY, 0);

			if(ttyFD >= 0) {
				tty.loop = o->loggerLoop;
				int r = uv_tty_init(o->loggerLoop, &tty, ttyFD, READABLE);

				if (r) ERROR_UV("initing tty", r, o->loggerLoop);

				// enable TYY formatting, flow-control, etc.
//				r = uv_tty_set_mode(&tty, TTY_NORMAL);
//				DBG_OUT("r = %d\n",r);
//				if (r) ERROR_UV("setting tty mode", r, o->loggerLoop);

				if(!r)
					readyCB(true,err,this);
				else {
					err.setError(r);
					readyCB(false,err, this);
				}
			} else {
				err.setError(errno,"Failed to open TTY");
				readyCB(false,err, this);
			}

		}
	};


	class fileTarget final : public logTarget {
	public:
		class rotationOpts final {
		public:
			bool enabled;
			bool rotate_on_start;     // rotate files on start (default false)
			uint32_t rotate_past;     // rotate when past rotate_past size in bytes, 0 - no setting
			uint32_t max_files;       // max files, including current, 0 - no setting
			uint64_t max_total_size;  // the max size to maintain, 0 - no setting
			uint32_t max_file_size;
			rotationOpts() : enabled(false), rotate_on_start(false), rotate_past(0), max_files(0), max_total_size(0), max_file_size(0) {}
			rotationOpts(rotationOpts& o) : enabled(true), rotate_on_start(o.rotate_on_start),
					rotate_past(o.rotate_past), max_files(o.max_files), max_total_size(o.max_total_size), max_file_size(o.max_file_size) {}
			rotationOpts& operator=(rotationOpts& o) {
				enabled = true;
				rotate_on_start = o.rotate_on_start; rotate_past = o.rotate_past;
				max_files = o.max_files; max_total_size = o.max_total_size;
				max_file_size = o.max_file_size;
				return *this;
			}

		};
	protected:


		// this tracks the number of submitted async write calls. When it's zero
		int submittedWrites; // ...it's safe to rotate
		rotationOpts filerotation;
		uint32_t current_size;
		uint64_t all_files_size;
	public:
		uv_rwlock_t wrLock;
		uv_file fileHandle;
		uv_fs_t fileFs;
		char *myPath;
		int myMode; int myFlags;

		static void on_open(uv_fs_t *req) {
			uv_fs_req_cleanup(req);
			fileTarget *t = (fileTarget *) req->data;
			_errcmn::err_ev err;
			if (req->result >= 0) {
				HEAVY_DBG_OUT("file: on_open() -> FD is %d", req->result);
				t->fileHandle = req->result;
				t->readyCB(true,err,t);
			} else {
				ERROR_OUT("Error opening file %s\n", t->myPath);
				err.setError(req->errorno);
				t->readyCB(false,err,t);
			}
		}

		static void on_open_for_rotate(uv_fs_t *req) {
			uv_fs_req_cleanup(req);
			fileTarget *t = (fileTarget *) req->data;
			_errcmn::err_ev err;
			if (req->result >= 0) {
				HEAVY_DBG_OUT("file: on_open() -> FD is %d", req->result);
				t->fileHandle = req->result;
//				t->readyCB(true,err,t);
			} else {
				ERROR_OUT("Error opening file %s\n", t->myPath);
				err.setError(req->errorno);
//				t->readyCB(false,err,t);
			}
		}

		static void write_cb(uv_fs_t* req) {
//			HEAVY_DBG_OUT("write_cb");
			HEAVY_DBG_OUT("file: write_cb()");
			if(req->errorno) {
				ERROR_PERROR("file: write_cb() ",req->errorno);
			}
			writeCBData *d = (writeCBData *) req->data;
			fileTarget *ft = (fileTarget *) d->t;

			uv_rwlock_wrlock(&ft->wrLock);
			ft->submittedWrites--;

			HEAVY_DBG_OUT("sub: %d\n",ft->submittedWrites);
			// TODO - check for file rotation
			if(ft->submittedWrites < 1 && ft->filerotation.enabled) {
				if(ft->current_size > ft->filerotation.max_file_size) {
					HEAVY_DBG_OUT("Rotate: past max file size\n");
					uv_fs_close(ft->owner->loggerLoop, &ft->fileFs, ft->fileHandle, NULL);
					// TODO close file..
					ft->rotate_files();
					// TODO open new file..
					int r = uv_fs_open(ft->owner->loggerLoop, &ft->fileFs, ft->myPath, ft->myFlags, ft->myMode, NULL); // use default loop because we need on_open() cb called in event loop of node.js
					on_open_for_rotate(&ft->fileFs);
				}
			}
			uv_rwlock_wrunlock(&ft->wrLock);

			d->t->returnBuffer(d->b);
			uv_fs_req_cleanup(req);
			delete req;
		}
		static void write_overflow_cb(uv_fs_t* req) {
			HEAVY_DBG_OUT("overflow_cb");
			if(req->errorno) {
				ERROR_PERROR("file: write_overflow_cb() ",req->errorno);
			}
			overflowBuf *b = (overflowBuf *) req->data;
			uv_fs_req_cleanup(req);
			delete b;
//			delete req;
		}

		uv_write_t outReq;  // since this function is no re-entrant (below) we can do this
		void flush(logBuf *b) { // this will always be called by the logger thread (via uv_async_send)
			uv_fs_t *req = new uv_fs_t;
			writeCBData *d = new writeCBData;
			d->t = this;
			d->b = b;
			req->data = d;
			uv_mutex_lock(&b->mutex);  // this lock should be ok, b/c write is async
			HEAVY_DBG_OUT("file: flush() %d bytes", b->handle.len);
			current_size += b->handle.len;
			// NOTE: we aren't using this like a normal wrLock. What it's for is to prevent
			// a file rotation while writing
			uv_rwlock_wrlock(&wrLock);
			submittedWrites++;
			uv_fs_write(owner->loggerLoop, req, fileHandle, (void *) b->handle.base, b->handle.len, -1, write_cb);
			uv_rwlock_wrunlock(&wrLock);
//			write_cb(&req);  // debug, skip actual write
			uv_mutex_unlock(&b->mutex);
//			b->clear(); // NOTE - there is a risk that this could get overwritten before it is written out.
		}

		int sync_n;
//		uv_fs_t reqSync[4];
		static void sync_cb(uv_fs_t* req) {
			uv_fs_req_cleanup(req);
			HEAVY_DBG_OUT("file: sync() %d", req->errorno);
			delete req;
		}
		void sync() {
//			sync_n++;
//			if(sync_n >= 4) sync_n = 0;
			uv_fs_t *req = new uv_fs_t;
			req->data = this;
			uv_fs_fsync(owner->loggerLoop, req, fileHandle, sync_cb);
		}
		void flushSync(logBuf *b) { // this will always be called by the logger thread (via uv_async_send)
			uv_fs_t req;
			uv_mutex_lock(&b->mutex);  // this lock should be ok, b/c write is async
			HEAVY_DBG_OUT("file: flushSync() %d bytes", b->handle.len);
			uv_fs_write(owner->loggerLoop, &req, fileHandle, (void *) b->handle.base, b->handle.len, -1, NULL);
//			uv_write(req, (uv_stream_t *) &tty, &b->handle, 1, NULL);
			uv_mutex_unlock(&b->mutex);
			uv_fs_req_cleanup(&req);
//			b->clear(); // NOTE - there is a risk that this could get overwritten before it is written out.
		}
		void writeAsync(overflowBuf *b) {
			uv_fs_t *req = new uv_fs_t;
			req->data = b;
			// APIUPDATE libuv - will change with newer libuv
			// uv_fs_write(uv_loop_t* loop, uv_fs_t* req, uv_file file, void* buf, size_t length, int64_t offset, uv_fs_cb cb);
			uv_fs_write(owner->loggerLoop, req, fileHandle, (void *) b->handle.base, b->handle.len, -1, write_overflow_cb);
		}
		void writeSync(char *s, int l) {
			uv_fs_t req;
			uv_buf_t buf;
			buf.base = s;
			buf.len = l;
			uv_fs_write(owner->loggerLoop, &req, fileHandle, (void *) s, l, -1, NULL);
//			uv_write(req, (uv_stream_t *) &tty, &buf, 1, NULL);
		}
	protected:


		class rotatedFile final {
		public:
			char *path;
			uint32_t size;
			uint32_t num;
			bool ownMem;
			rotatedFile(char *base, int n) : path(NULL), size(0), ownMem(false), num(n) {
				path = rotatedFile::strRotateName(base,n);
				ownMem = true;
			}
			rotatedFile(internalCmdReq &) = delete;
			rotatedFile(rotatedFile && o) : path(o.path), size(o.size), num(o.num), ownMem(true) {
				o.path = NULL;
				o.ownMem = false;
			}
			rotatedFile() : path(NULL), size(0), num(0), ownMem(false) {};
			rotatedFile& operator=(rotatedFile& o) {
				if(path && ownMem)
					LFREE(path);
				path = o.path;
				size = o.size;
				num = o.num;
				ownMem = false;
				return *this;
			}
			rotatedFile& operator=(rotatedFile&& o) {
				if(path && ownMem)
					LFREE(path);
				path = o.path; o.path = NULL;
				size = o.size;
				num = o.num;
				ownMem = true; o.ownMem = false;
				return *this;
			}
			~rotatedFile() {
				if(path && ownMem)
					LFREE(path);
			}
			static char *strRotateName(char *s, int n) {
				char *ret = NULL;
				if(s && n < MAX_ROTATED_FILES && n > 0) {
					int origL = strlen(s);
					ret = (char *) LMALLOC(origL+10);
					memset(ret,0,origL+10);
					memcpy(ret,s,origL);
					snprintf(ret+origL,9,".%d",n);
				}
				return ret;
			}
		};

		TWlib::tw_safeCircular<rotatedFile, LoggerAlloc > rotatedFiles;

		void rotate_files() {
			rotatedFile f;
			TWlib::tw_safeCircular<rotatedFile, LoggerAlloc > tempFiles(MAX_ROTATED_FILES,true);
			int n = rotatedFiles.remaining();
			while (rotatedFiles.removeMv(f)) {
				uv_fs_t req;
				if(filerotation.max_files && ((n+1) > filerotation.max_files)) { // remove file
					int r = uv_fs_unlink(owner->loggerLoop,&req,f.path,NULL);
					if(r) {
						uv_err_t e = uv_last_error(owner->loggerLoop);
						if(e.code != UV_ENOENT) {
							ERROR_OUT("file rotation - remove %s: %s\n", f.path, uv_strerror(e));
						}
					}
				} else {  // move file to new name
//					char *newname = rotatedFile::strRotateName(myPath,f.num+1);
					rotatedFile new_f(myPath,f.num+1);
					int r = uv_fs_rename(owner->loggerLoop, &req, f.path, new_f.path, NULL);
					if(r) {
						uv_err_t e = uv_last_error(owner->loggerLoop);
						if(e.code != UV_OK) {
							ERROR_OUT("file rotation - rename %s: %s\n", f.path, uv_strerror(e));
						} else {
						}
//						if(e.code != UV_ENOENT) {
//							ERROR_OUT("file rotation - rename %s: %s\n", f.path, uv_strerror(e));
//						}
					}
					tempFiles.addMv(new_f);
				}
				n--;
			}
			uv_fs_t req;
			rotatedFile new_f(myPath,1);

			int r = uv_fs_rename(owner->loggerLoop, &req, myPath, new_f.path, NULL);
			if(r) {
				uv_err_t e = uv_last_error(owner->loggerLoop);
				if(e.code != UV_OK) {
					ERROR_OUT("file rotation - rename %s: %s\n", new_f.path, uv_strerror(e));
				} else {
				}
			} else
				tempFiles.addMv(new_f);
			rotatedFiles.transferFrom(tempFiles);
			current_size = 0; // we will use a new file, size is 0
		}

		bool check_files() {
			all_files_size = 0;
			// find all rotated files
			bool needs_rotation = false;
			uv_fs_t req;
			rotatedFiles.clear();
			// find existing file...
			int r = uv_fs_stat(owner->loggerLoop, &req, myPath, NULL);
			if(r) {
				uv_err_t e = uv_last_error(owner->loggerLoop);
				if(e.code != UV_ENOENT) {
					ERROR_OUT("file rotation: %s\n", uv_strerror(e));
				}
			} else {
				// if the current file is too big - we need to rotate.
				if(filerotation.max_file_size && filerotation.max_file_size < req.statbuf.st_size)
					needs_rotation = true;
				all_files_size += req.statbuf.st_size;
				current_size = req.statbuf.st_size;
			}

			int n = 1;
			while(1) {  // find all files...
				uv_fs_t req;
				rotatedFile f(myPath,n);
				int r = uv_fs_stat(owner->loggerLoop, &req, f.path, NULL);
				if(r) {
					uv_err_t e = uv_last_error(owner->loggerLoop);
					if(e.code && e.code != UV_ENOENT) {
						ERROR_OUT("file rotation [path:%s]: %s\n", f.path, uv_strerror(e));
					}
					break;
				} else {
					f.size = req.statbuf.st_size;
					rotatedFiles.addMv(f);
					all_files_size += req.statbuf.st_size;
					n++;
				}
			}
			rotatedFiles.reverse();
			if(all_files_size > filerotation.max_total_size)
				needs_rotation = true;
			return needs_rotation;
		}


		void post_cstor(int buffer_size, uint32_t id, GreaseLogger *o, int flags, int mode, char *path, target_start_info *readydata, targetReadyCB cb) {
			uv_rwlock_init(&wrLock);
			readydata->needsAsyncQueue = true;
			myPath = strdup(path);
			if(!path) {
				ERROR_OUT("Need a file path!!");
				_errcmn::err_ev err;
				err.setError(GREASE_UNKNOWN_NO_PATH);
				readyCB(false,err,this);
			} else {
				fileFs.data = this;

//				char *rotatename = rotatedFile::strRotateName(path,1);
//				DBG_OUT("rotatename: %s", rotatename);
//				free(rotatename);
				if(filerotation.enabled) {
					bool needs_rotate = check_files();
					HEAVY_DBG_OUT("rotate_files: total files = %d total size = %d\n",rotatedFiles.remaining()+1,all_files_size);
					if(needs_rotate || filerotation.rotate_on_start) {
						HEAVY_DBG_OUT("Needs rotate....\n");
						rotate_files();
					}


					// check for existing rotated files, based on path name
					// mv existing file to new name, if rotateopts says so
					// setup rotatedFiles array
				}

				int r = uv_fs_open(owner->loggerLoop, &fileFs, path, flags, mode, on_open); // use default loop because we need on_open() cb called in event loop of node.js

				if (r) ERROR_UV("initing file", r, owner->loggerLoop);
			}
		}

	public:
		fileTarget(int buffer_size, uint32_t id, GreaseLogger *o, int flags, int mode, char *path, target_start_info *readydata, targetReadyCB cb,
				rotationOpts rotateopts) :
				logTarget(buffer_size, id, o, cb, readydata),
				submittedWrites(0), rotatedFiles(MAX_ROTATED_FILES,true), current_size(0), all_files_size(0),
				myPath(NULL), myMode(mode), myFlags(flags), filerotation(rotateopts), sync_n(0) {
			post_cstor(buffer_size, id, o, flags, mode, path, readydata, cb);
		}

		fileTarget(int buffer_size, uint32_t id, GreaseLogger *o, int flags, int mode, char *path, target_start_info *readydata, targetReadyCB cb) :
			logTarget(buffer_size, id, o, cb, readydata),
			submittedWrites(0), rotatedFiles(MAX_ROTATED_FILES,true), current_size(0), all_files_size(0),
			myPath(NULL), myMode(mode), myFlags(flags), filerotation(), sync_n(0) {
			post_cstor(buffer_size, id, o, flags, mode, path, readydata, cb);
		}


	};



	class callbackTarget final : public logTarget {
	public:
		int ttyFD;
		uv_tty_t tty;
		static void write_cb(uv_write_t* req, int status) {
//			HEAVY_DBG_OUT("write_cb");
			writeCBData *d = (writeCBData *) req->data;
			d->t->returnBuffer(d->b);

			delete req;
		}
		static void write_overflow_cb(uv_write_t* req, int status) {
			HEAVY_DBG_OUT("overflow_cb");
			overflowBuf *b = (overflowBuf *) req->data;
			delete b;
			delete req;
		}


		uv_write_t outReq;  // since this function is no re-entrant (below) we can do this
		void flush(logBuf *b) { // this will always be called by the logger thread (via uv_async_send)
			uv_write_t *req = new uv_write_t;
			writeCBData *d = new writeCBData;
			d->t = this;
			d->b = b;
			req->data = d;
			uv_mutex_lock(&b->mutex);  // this lock should be ok, b/c write is async
			HEAVY_DBG_OUT("TTY: flush() %d bytes", b->handle.len);
			uv_write(req, (uv_stream_t *) &tty, &b->handle, 1, write_cb);
			uv_mutex_unlock(&b->mutex);
//			b->clear(); // NOTE - there is a risk that this could get overwritten before it is written out.
		}
		void flushSync(logBuf *b) { // this will always be called by the logger thread (via uv_async_send)
			uv_write_t *req = new uv_write_t;
			writeCBData *d = new writeCBData;
			d->t = this;
			d->b = b;
			req->data = d;
			uv_mutex_lock(&b->mutex);  // this lock should be ok, b/c write is async
			HEAVY_DBG_OUT("TTY: flushSync() %d bytes", b->handle.len);
			uv_write(req, (uv_stream_t *) &tty, &b->handle, 1, NULL);
			uv_mutex_unlock(&b->mutex);
//			b->clear(); // NOTE - there is a risk that this could get overwritten before it is written out.
		}
		void writeAsync(overflowBuf *b) {
			uv_write_t *req = new uv_write_t;
			req->data = b;
			uv_write(req, (uv_stream_t *) &tty, &b->handle, 1, write_overflow_cb);
		}
		void writeSync(char *s, int l) {
			uv_write_t *req = new uv_write_t;
			uv_buf_t buf;
			buf.base = s;
			buf.len = l;
			uv_write(req, (uv_stream_t *) &tty, &buf, 1, NULL);
		}
		callbackTarget(int buffer_size, uint32_t id, GreaseLogger *o, targetReadyCB cb, target_start_info *readydata = NULL, char *ttypath = NULL) : logTarget(buffer_size, id, o, cb, readydata), ttyFD(0)  {
//			outReq.cb = write_cb;
			_errcmn::err_ev err;
			if(ttypath)
				ttyFD = ::open(ttypath, O_WRONLY, 0);
			else
				ttyFD = ::open("/dev/tty", O_WRONLY, 0);

			if(ttyFD >= 0) {
				tty.loop = o->loggerLoop;
				int r = uv_tty_init(o->loggerLoop, &tty, ttyFD, READABLE);

				if (r) ERROR_UV("initing tty", r, o->loggerLoop);

				// enable TYY formatting, flow-control, etc.
//				r = uv_tty_set_mode(&tty, TTY_NORMAL);
//				DBG_OUT("r = %d\n",r);
//				if (r) ERROR_UV("setting tty mode", r, o->loggerLoop);

				if(!r)
					readyCB(true,err,this);
				else {
					err.setError(r);
					readyCB(false,err, this);
				}
			} else {
				err.setError(errno,"Failed to open TTY");
				readyCB(false,err, this);
			}

		}
	};


	static void targetReady(bool ready, _errcmn::err_ev &err, logTarget *t);

	TWlib::tw_safeCircular<GreaseLogger::internalCmdReq, LoggerAlloc > internalCmdQueue;
	TWlib::tw_safeCircular<GreaseLogger::nodeCmdReq, LoggerAlloc > nodeCmdQueue;
	TWlib::tw_safeCircular<GreaseLogger::logTarget::writeCBData, LoggerAlloc > v8LogCallbacks;




	uv_mutex_t modifyFilters; // if the table is being modifued, lock first
	typedef TWlib::TW_KHash_32<uint64_t, FilterList *, TWlib::TW_Mutex, uint64_t_eqstrP, TWlib::Allocator<LoggerAlloc> > FilterHashTable;
	typedef TWlib::TW_KHash_32<uint32_t, Filter *, TWlib::TW_Mutex, uint32_t_eqstrP, TWlib::Allocator<LoggerAlloc> > FilterTable;
	typedef TWlib::TW_KHash_32<TargetId, logTarget *, TWlib::TW_Mutex, TargetId_eqstrP, TWlib::Allocator<LoggerAlloc> > TargetTable;


	FilterHashTable filterHashTable;  // look Filters by tag:origin
//	FilterTable filterTable;

	bool sift(logMeta &f, FilterList *&list); // returns true, then the logger should log it	TWlib::TW_KHash_32<uint16_t, int, TWlib::TW_Mutex, uint16_t_eqstrP, TWlib::Allocator<TWlib::Alloc_Std>  > magicNumTable;
	uint32_t levelFilterOutMask;  // mandatory - all log messages have a level. If the bit is 1, the level will be logged.

	bool defaultFilterOut;
//	FilterTable tagFilterOut;
//	FilterTable originFilterOut;




//	bool showNoLevel;    // show if no filter entry? default...
//	bool showNoTag;      // ''
//	bool showNoOrigin;   // ''

	// http://stackoverflow.com/questions/1277627/overhead-of-pthread-mutexes
	uv_mutex_t modifyTargets; // if the table is being modified, lock first

	logTarget *getTarget(uint32_t key) {
		logTarget *ret = NULL;
		uv_mutex_lock(&modifyTargets);
		targets.find(key,ret);
		uv_mutex_unlock(&modifyTargets);
		return ret;
	}

	logTarget *defaultTarget;
	TargetTable targets;

	uv_async_t asyncInternalCommand;
	uv_async_t asyncExternalCommand;
	uv_timer_t flushTimer;
	static void handleInternalCmd(uv_async_t *handle, int status /*UNUSED*/); // from in the class
	static void handleExternalCmd(uv_async_t *handle, int status /*UNUSED*/); // from node
	static void flushTimer_cb(uv_timer_t* handle, int status);  // on a timer. this flushed buffers after a while
	static void mainThread(void *);

	void setupDefaultTarget(actionCB cb, target_start_info *data);

	inline FilterHash filterhash(TagId tag, OriginId origin) {
		// http://stackoverflow.com/questions/13158501/specifying-64-bit-unsigned-integer-literals-on-64-bit-data-models
		return (uint64_t) (((uint64_t) UINT64_C(0xFFFFFFFF00000000) & ((uint64_t) origin << 32)) | tag);
	}

	bool _addFilter(TargetId t, OriginId origin, TagId tag, LevelMask level, FilterId &id) {
		uint64_t hash = filterhash(tag,origin);
		bool ret = false;
		uv_mutex_lock(&nextIdMutex);
		id = nextFilterId++;
		uv_mutex_unlock(&nextIdMutex);

		FilterList *list = NULL;
		uv_mutex_lock(&modifyFilters);
		if(!filterHashTable.find(hash,list)) {
			list = new FilterList();
			DBG_OUT("new FilterList: hash %llu", hash);
			filterHashTable.addNoreplace(hash, list);
		}
		Filter f(id,level,t);
		DBG_OUT("new Filter(%x,%x,%x) to list [hash] %llu", id,level,t, hash);
		// TODO add to list
		ret = list->add(f);
		uv_mutex_unlock(&modifyFilters);
		if(!ret) {
			ERROR_OUT("Max filters for this tag/origin combination");
		}
		return ret;
	}




	uv_loop_t *loggerLoop;  // grease uses its own libuv event loop (not node's)

	void _log(FilterList *list, logMeta &meta, char *s, int len); // internal log cmd
	void _logSync(FilterList *list, logMeta &meta, char *s, int len); // internal log cmd



	void start(actionCB cb, target_start_info *data);
public:

	void log(logMeta &f, char *s, int len); // does the work of logging (for users in C++)
	void logSync(logMeta &f, char *s, int len); // does the work of logging. now. will empty any buffers first.
	static void flushAll();
	static void flushAllSync();

//	static Handle<Value> Init(const Arguments& args);
//	static void ExtendFrom(const Arguments& args);
	static void Init();
//	static void Shutdown();

    static Handle<Value> New(const Arguments& args);
    static Handle<Value> NewInstance(const Arguments& args);


    static Handle<Value> AddFilter(const Arguments& args);
    static Handle<Value> RemoveFilter(const Arguments& args);
    static Handle<Value> AddTarget(const Arguments& args);

    static Handle<Value> Start(const Arguments& args);


    static Handle<Value> Log(const Arguments& args);
    static Handle<Value> LogSync(const Arguments& args);
    static Handle<Value> Flush(const Arguments& args);


//    static Handle<Value> Create(const Arguments& args);
//    static Handle<Value> Open(const Arguments& args);
//    static Handle<Value> Close(const Arguments& args);


    static Persistent<Function> constructor;
//    static Persistent<ObjectTemplate> prototype;

    // solid reference to TUN / TAP creation is: http://backreference.org/2010/03/26/tuntap-interface-tutorial/


	void setErrno(int _errno, const char *m=NULL) {
		err.setError(_errno, m);
	}

protected:

	// calls callbacks when target starts (if target was started in non-v8 thread
	static void callTargetCallback(uv_async_t *h, int status );
	static void callV8LogCallbacks(uv_async_t *h, int status );
	static void start_target_cb(GreaseLogger *l, _errcmn::err_ev &err, void *d);
	static void start_logger_cb(GreaseLogger *l, _errcmn::err_ev &err, void *d);

    GreaseLogger(int buffer_size , int chunk_size) :
    	nextFilterId(1), nextTargetId(1),
    	bufferSize(buffer_size), chunkSize(chunk_size),
    	targetCallbackQueue(MAX_TARGET_CALLBACK_STACK, true),
    	err(),
//    	showNoLevel(true), showNoTag(true), showNoOrigin(true),
    	internalCmdQueue( INTERNAL_QUEUE_SIZE, true ),
    	nodeCmdQueue( COMMAND_QUEUE_NODE_SIZE, true ),
    	v8LogCallbacks( V8_LOG_CALLBACK_QUEUE_SIZE, true ),
    	levelFilterOutMask(0), // tagFilter(), originFilter(),
    	defaultFilterOut(false),
//    	filterTable(),
    	filterHashTable(),
    	defaultTarget(NULL), targets(),
	    loggerLoop(NULL)
    	{
    	    LOGGER = this;
    	    loggerLoop = uv_loop_new();    // we use our *own* event loop (not the node/io.js one)
    	    uv_async_init(uv_default_loop(), &asyncTargetCallback, callTargetCallback);
    	    uv_async_init(uv_default_loop(), &asyncV8LogCallback, callV8LogCallbacks);
    	    uv_unref((uv_handle_t *)&asyncTargetCallback);
    	    uv_mutex_init(&nextIdMutex);
    		uv_mutex_init(&modifyFilters);
    		uv_mutex_init(&modifyTargets);
    	}

    ~GreaseLogger() {
    	if(defaultTarget) delete defaultTarget;
    	TargetTable::HashIterator iter(targets);
    	logTarget **t; // shut down other targets.
    	while(!iter.atEnd()) {
    		t = iter.data();
    		delete (*t);
    		iter.getNext();
    	}
    }

public:
	static GreaseLogger *setupClass(int buffer_size = DEFAULT_BUFFER_SIZE, int chunk_size = LOGGER_DEFAULT_CHUNK_SIZE) {
		if(!LOGGER) {
			LOGGER = new GreaseLogger(buffer_size, chunk_size);
			atexit(shutdown);
		}
		return LOGGER;
	}


	static void shutdown() {
		GreaseLogger *l = setupClass();
		internalCmdReq req(INTERNAL_SHUTDOWN); // just rotated off a buffer
		l->internalCmdQueue.addMvIfRoom(req);
		uv_async_send(&l->asyncInternalCommand);
	}
};


//template<>
//struct logTarget<GreaseLogger::logBuf,uv_tty_t> {
//	GreaseLogger::logBuf *buffers[NUM_BANKS];
//	uv_tty_t tty;
//	_errcmn::err_ev err;
//	int _log_fd;
//	GreaseLogger *owner;
//	logTarget(int buffer_size, GreaseLogger *o) : err(), _log_fd(0), owner(o) {
//		for (int n=0;n<NUM_BANKS;n++) {
//			buffers[n] = new logBuffer(buffer_size);
//		}
//	}
//	void init() {
//		uv_tty_init(owner->loggerLoop, &tty, GREASE_STDOUT, NOTREADABLE);
////		uv_tty_init(owner->loggerLoop, &tty, GREASE_STDOUT, NOTREADABLE);
//	}
//	void write(char *s, int len) {
//
//	}
//	void flush() { // flushes buffers. Synchronous
//		uv_write(&write_req, (uv_stream_t*) &tty, &buf, 1, NULL);
//	}
//	void close() {
//
//	}
//
//};

//template<>
//uv_tty_t logTarget<GreaseLogger::logBuf,uv_tty_t>::tty;

//template<>
//void logTarget<GreaseLogger::logBuf,uv_tty_t>::init() {
//	uv_tty_init(owner->loggerLoop, &tty, GREASE_STDOUT, NOTREADABLE);
//}






} // end namepsace


#endif /* GreaseLogger_H_ */
