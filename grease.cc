/**
 * (c) 2015 WigWag Inc.
 *
 * Author: ed
 */
#include "logger.h"

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


// BEGIN TESTS
//
//#define TAIL_DATA(strct,p) ((char *)(&(p)) + sizeof(strct))
//
//struct packTestDat {
//	uint8_t first;
//	char second;
//	uint32_t third;
//	uint8_t other[5];
//	// TAIL_DATA follows
//}
//#ifdef __GNUC__
//__attribute__ ((aligned (16)));  // ensure we are using 16-bit alignment on the structure
//#else
//;
//#endif
//
//static
//char *toBytesString(uint8_t *d,int n) {
//	const int s = n*3+4;
//	char *ret = (char *) malloc(s);
//	uint8_t *look = d;
//	int q = 0;
//	ret[0] = '[';
//	while(q < n) {
//		snprintf(ret+1+(q*3), s-3-(q*3),"%2x ",*(look + q));
//		q++;
//	}
//	ret[q*3] = ']'; q++;
//	ret[q*3] = '\0';
//	return ret;
//}
//
//Handle<Value> PackTest(const Arguments& args) {
//	HandleScope scope;
//
//	DBG_OUT("PackTest\n");
//
//	if(args.Length() > 0 && args[0]->IsObject()) {
//			char *backing = node::Buffer::Data(args[0]->ToObject());
//			packTestDat *d = (packTestDat *) backing;
//			DBG_OUT("first: 0x%02x", d->first);
//			DBG_OUT("second: %d", d->second);
//			DBG_OUT("third: 0x%04x or %d", d->third, d->third); // to check endianess
//			char *s = toBytesString(d->other,5);
//			DBG_OUT("other: %s", s);
//			free(s);
//			DBG_OUT("something: %s", TAIL_DATA(packTestDat,*d));
//	}
//
////	__u32		nlmsg_len;	/* Length of message including header */
////		__u16		nlmsg_type;	/* Message content */
////		__u16		nlmsg_flags;	/* Additional flags */
////		__u32		nlmsg_seq;	/* Sequence number */
////		__u32		nlmsg_pid;
//	if(args.Length() > 1 && args[1]->IsObject()) {
//			char *backing = node::Buffer::Data(args[1]->ToObject());
//			nlmsghdr *d = (nlmsghdr *) backing;
//			DBG_OUT("a nlmsghdr:");
//			DBG_OUT("_len: 0x%08x", d->nlmsg_len);
//			DBG_OUT("_type: 0x%04x", d->nlmsg_type);
//			DBG_OUT("_flags: 0x%04x", d->nlmsg_flags);
//			DBG_OUT("_seq: 0x%08x", d->nlmsg_seq);
//			DBG_OUT("_pid: 0x%08x", d->nlmsg_pid);
//	}
//
//	return scope.Close(Undefined());
//}
//
//void free_test_cb(char *m,void *hint) {
//	DBG_OUT("FREEING MEMORY.");
//	free(m);
//}
//
//void weak_cb(Persistent<Value> object, void* parameter) {
//	object.Dispose();
//}

//Handle<Value> WrapMemBufferTest(const Arguments& args) {
//	HandleScope scope;
//	char *mem = (char *) ::malloc(100);
//	memset(mem,'A',100);
//	node::Buffer *buf = node::Buffer::New(mem,100,free_test_cb,0);
////	node::Buffer *buf = UNI_BUFFER_NEW_WRAP(mem,100,free_test_cb,NULL);
////	buf->handle_.MakeWeak(NULL, weak_cb);
//	return scope.Close(buf->handle_);
//}

/// END TESTS




Handle<Value> ErrorFromErrno(const Arguments& args) {
	HandleScope scope;

	if(args.Length() > 0 && args[0]->Int32Value()) {
		Local<Value> err = _errcmn::errno_to_JS(args[0]->Int32Value(),"netkit: ");
		return scope.Close(err);
	} else {
		return scope.Close(Undefined());
	}

}

Handle<Value> NewLogger(const Arguments& args) {
	HandleScope scope;

	return scope.Close(GreaseLogger::NewInstance(args));

}




// this is not a standard module thing, called internally
void ShutdownModule() {
//	TunInterface::Shutdown();
}



void InitAll(Handle<Object> exports, Handle<Object> module) {
//	NodeTransactionWrapper::Init();
//	NodeClientWrapper::Init();
//	exports->Set(String::NewSymbol("cloneRepo"), FunctionTemplate::New(CreateClient)->GetFunction());


//	TunInterface::Init();
//	exports->Set(String::NewSymbol("InitNativeTun"), FunctionTemplate::New(TunInterface::Init)->GetFunction());
//	exports->Set(String::NewSymbol("InitNetlinkSocket"), FunctionTemplate::New(NetlinkSocket::Init)->GetFunction());
	exports->Set(String::NewSymbol("newLogger"), FunctionTemplate::New(NewLogger)->GetFunction());
//	exports->Set(String::NewSymbol("newNetlinkSocket"), FunctionTemplate::New(NewNetlinkSocket)->GetFunction());
//	exports->Set(String::NewSymbol("ifNameToIndex"), FunctionTemplate::New(IfNameToIndex)->GetFunction());
//	exports->Set(String::NewSymbol("ifIndexToName"), FunctionTemplate::New(IfIndexToName)->GetFunction());
//	exports->Set(String::NewSymbol("toAddress"), FunctionTemplate::New(ToAddress)->GetFunction());
//	exports->Set(String::NewSymbol("errorFromErrno"), FunctionTemplate::New(ErrorFromErrno)->GetFunction());

//	exports->Set(String::NewSymbol("wrapMemBufferTest"), FunctionTemplate::New(WrapMemBufferTest)->GetFunction());
//	exports->Set(String::NewSymbol("packTest"), FunctionTemplate::New(PackTest)->GetFunction());
	GreaseLogger::Init();

	Handle<Object> errconsts = Object::New();
	_errcmn::DefineConstants(errconsts);
	exports->Set(String::NewSymbol("ERR"), errconsts);

//	exports->Set(String::NewSymbol("_TunInterface_cstor"), TunInterface::constructor);

	//	exports->Set(String::NewSymbol("_TunInterface_proto"), TunInterface::prototype);

//	exports->Set(String::NewSymbol("shutdownTunInteface"), FunctionTemplate::New(ShutdownTunInterface)->GetFunction());

}

NODE_MODULE(greaseLog, InitAll)
