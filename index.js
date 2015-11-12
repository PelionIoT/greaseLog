// grease-log
// (c) WigWag Inc 2014
//


//var build_opts = require('build_opts.js');
var util = require('util');
var build_opts = { 
	debug: 1 
};

var _console_log = function() {}

var nativelib = null;


var _logger = {};

var MAX_TAG_ORIGIN_VALUE = 0xFF00; // 'special' value above this

var instance = null;
var setup = function(options) {
//	console.log("SETUP!!!!!!!!!!");

	this.makeOriginIdFromPid = function(pid) {
		return ((pid & 0xFFFFFFFF) | 0x15000000); // add value to avoid stepping on origin IDs which might be statically setup
	}

	var process_name = 'process';
	var PID = this.makeOriginIdFromPid(process.pid); // to avoid calling an Accessor constantly, if that's what this does. 
//	console.log("my pid = " + PID);										      

	var client_only = false;

	var TAGS = {};
	var tagId = 1;

	var ORIGINS = {};
	var originId = 1;

	var getTagId = function(o,val) {
		var ret = TAGS[o];
		if(ret)
			return ret;
		else {
			if(val != undefined) {
				TAGS[o] = val;
				if(!client_only)
					instance.addTagLabel(val,o);				
				return val;
			} else {
				if(tagId >= MAX_TAG_ORIGIN_VALUE) 
					tagId = 1;
				ret = tagId++;
				TAGS[o] = ret;
				if(!client_only)
					instance.addTagLabel(ret,o);
				return ret;
			}
		}
	}

	var getOriginId = function(o,val) {
		var ret = ORIGINS[o];
		if(ret)
			return ret;
		else {
			if(val != undefined) {
				ORIGINS[o] = val;
				if(!client_only)
					instance.addOriginLabel(val,o);				
				return val;
			} else {
				if(originId >= MAX_TAG_ORIGIN_VALUE) 
					originId = 1;
				ret = originId++;
				ORIGINS[o] = ret;
				if(!client_only)
					instance.addOriginLabel(ret,o);	
				return ret;
			}
		}
	}

	var LEVELS_default = {
		'log'      : 0x01,
		'error'    : 0x02,
		'warn'     : 0x04,
		'debug'    : 0x08,
		'debug2'   : 0x10,
		'debug3'   : 0x20,
		'user1'    : 0x40,
		'user2'    : 0x80,  // Levels can use the rest of the bits too...
		'info'     : 0x100,
		'success'  : 0x100,
		'trace'    : 0x200
	};
	var do_trace = true;
	var levels = LEVELS_default;
	var default_sink = {unixDgram:"/tmp/grease.socket"};
	var client_no_origin_as_pid = false;
	var default_originN = undefined;
	var old_style_API = false; // this is the API where: grease.warn(TAG,ORIGIN,MESSAGE)
	var always_use_origin = false;

	if(options) {
//		console.dir(options);
		if(options.levels) levels = options.levels;
		if(options.do_trace !== undefined) do_trace = options.do_trace;
		if(options.default_sink !== undefined) { default_sink = options.default_sink; }
		if(options.client_only) { client_only = true; }
		if(options.default_origin !== undefined) default_originN = options.default_origin;
		if(options.always_use_origin) always_use_origin = true;
		old_style_API = options.old_style_API;
	}

	if(!client_only) {
		try {
			nativelib = require('./build/Release/greaseLog.node');
		} catch(e) {
			if(e.code == 'MODULE_NOT_FOUND')
				nativelib = require('./build/Debug/greaseLog.node');
			else
				console.error("Error in nativelib [debug]: " + e + " --> " + e.stack);
		}	
	} else {
		try {
			nativelib = require('./build/Release/greaseLogClient.node');
		} catch(e) {
			if(e.code == 'MODULE_NOT_FOUND')
				nativelib = require('./build/Debug/greaseLogClient.node');
			else
				console.error("Error in nativelib (client) [debug]: " + e + " --> " + e.stack);
		}	
		if(default_originN === undefined)
			default_originN = PID;
	}

	var natives = Object.keys(nativelib);
	for(var n=0;n<natives.length;n++) {
	//	console.log(natives[n] + " typeof " + typeof nativelib[natives[n]]);
		_logger[natives[n]] = nativelib[natives[n]];
	}

	//	console.dir(levels);
	this.LEVELS_names_only = {};  // does not include 'ALL' (or other stuff like this)
	this.LEVELS = {};
	this.LEVELS.ALL = 0xFFFFFFFF; // max uint32_t
	this.MASK_OUT = 0;

	var levelsK = Object.keys(levels);
	for(var n=0;n<levelsK.length;n++) {
		if(!this[levelsK[n]]) {
			var N = levels[levelsK[n]];
			var name = levelsK[n];
			this.LEVELS_names_only[name] = N;
			this.LEVELS[name] = N;  // a copy, used for caller when creating filters
			this[name] = function(){
			}
		} else {
			throw new Error("Grease: A level name is used more than once: " + levels[n]);
		}

	}


	var getStack = function() {
		// EXPENSIVE !!!!!!!!!!!!
		var data = {};
		// get call stack, and analyze it
		// get all file,method and line number
		data.stack = (new Error()).stack.split('\n').slice(3);

		// Stack trace format :
		// http://code.google.com/p/v8/wiki/JavaScriptStackTraceApi
		var s = data.stack[0], sp = /at\s+(.*)\s+\((.*):(\d*):(\d*)\)/gi
				.exec(s)
			|| /at\s+()(.*):(\d*):(\d*)/gi.exec(s);
		if (sp && sp.length === 5) {
			data.method = sp[1];
			data.path = sp[2];
			data.line = sp[3];
			data.pos = sp[4];
			var paths = data.path.split('/');
			data.file = paths[paths.length - 1];
			if(paths.length > 1)
				data.subdir = paths[paths.length - 2] + '/';
		}
		return data;
	}

	var self = this;






	/**
	 * Creates the user's log.LEVEL functions
	 * forms
	 * log.LEVEL(options,origin,tag,message)  options = {object}
	 * log.LEVEL(origin,tag,message)
	 * log.LEVEL(tag,message)
	 * log.LEVEL(message)
	 */
	var createfunc = function(_name,_n) {
// 		if(_name == 'trace') {  
// 			self[_name] = function(){ // trace is special
// 				if(self.MASK_OUT & _n) {
// 					return; // fast track out - if this log function is turned off
// 				}
// 				var args = [];
// 				for(var n=0;n<arguments.length;n++)
// 					args[n] = arguments[n];
// 				var d = getStack();
// //						var stk = util.format.apply(undefined,args);
// //						if((typeof arguments[0] !== 'string' || arguments.length > 4)&&(!(typeof arguments[0] !== 'object' && arguments.length == 4))) {
// 				if(typeof arguments[0] !== 'string' || arguments.length > 4) {
// 					var s = "**SLOW LOG FIX ME - avoid util.format() style logging**";
// 					for(var n=0;n<arguments.length;n++) {
// 						if(typeof arguments[n] !== 'string')
// 							s += " " + util.inspect(arguments[n]);							
// 						else
// 							s += " " + arguments[n];
// 					}
// 					self._log(_n,"["+d.subdir+d.file+":"+d.line+" in "+d.method+"()] "+ s,d.subdir+d.file);														
// 				} else {
// 					if(arguments.length > 1)		
// 						self._log(_n,"["+d.subdir+d.file+":"+d.line+" in "+d.method+"()] " + arguments[0],arguments[1],d.subdir+d.file);
// 					else
// 						self._log(_n,"["+d.subdir+d.file+":"+d.line+" in "+d.method+"()] " + arguments[0],d.subdir+d.file);														
// 				}

// //					console.log("log: " + util.inspect(arguments));
// 				// if(arguments.length > 2)
// 				// 	self._log(_n,arguments[2],arguments[1],arguments[0]);
// 				// else if(arguments.length == 2)
// 				// 	self._log(_n,arguments[1],arguments[0]);
// 				// else
// 				// 	self._log(_n,arguments[0]);
// 			}
// 		} else {
		if(old_style_API) {
			if(_name == 'trace') {
				self[_name] = function(){ // trace is special
					if(self.MASK_OUT & _n) {
						return; // fast track out - if this log function is turned off
					}
					var args = [];
					for(var n=0;n<arguments.length;n++)
						args[n] = arguments[n];
					var d = getStack();
		//						var stk = util.format.apply(undefined,args);
		//						if((typeof arguments[0] !== 'string' || arguments.length > 4)&&(!(typeof arguments[0] !== 'object' && arguments.length == 4))) {
					if(typeof arguments[0] !== 'string' || arguments.length > 4) {
						var s = "**SLOW LOG FIX ME - avoid util.format() style logging**";
						for(var n=0;n<arguments.length;n++) {
							if(typeof arguments[n] !== 'string')
								s += " " + util.inspect(arguments[n]);							
							else
								s += " " + arguments[n];
						}
						self._log(_n,"["+d.subdir+d.file+":"+d.line+" in "+d.method+"()] "+ s,d.subdir+d.file);														
					} else {
						if(arguments.length > 1)		
							self._log(_n,"["+d.subdir+d.file+":"+d.line+" in "+d.method+"()] " + arguments[0],arguments[1],d.subdir+d.file);
						else
							self._log(_n,"["+d.subdir+d.file+":"+d.line+" in "+d.method+"()] " + arguments[0],d.subdir+d.file);														
					}
				}
			} // end trace
			else {
				self[_name] = function(){
					if(self.MASK_OUT & _n) {
						return; // fast track out - if this log function is turned off
					}
					// a caller used the log.X function with util.format style parameters						
					if((typeof arguments[0] !== 'string' || arguments.length > 4) && (!(typeof arguments[0] == 'object' && arguments.length == 4))) {
						var s = "**SLOW LOG FIX ME - avoid util.format() style logging**";
						for(var n=0;n<arguments.length;n++) {
							if(typeof arguments[n] !== 'string')
								s += " " + util.inspect(arguments[n]);							
							else
								s += " " + arguments[n];
						}
						self._log(_n,s);
					} else {
						if(arguments.length > 3)
							self._log(_n,arguments[3],arguments[2],arguments[1],arguments[0]);
						else if(arguments.length == 3)
							self._log(_n,arguments[2],arguments[1],arguments[0]);
						else if(arguments.length == 2)
							self._log(_n,arguments[1],arguments[0]);
						else
							self._log(_n,arguments[0]);
					}
				}
			}
		} else { // NEW STYLE API
			if(_name == 'trace') {
				if(always_use_origin) {
					self[_name] = function(){ // trace is special
						if(self.MASK_OUT & _n) {
							return; // fast track out - if this log function is turned off
						}
						var d = getStack();
						var argz = Array.prototype.slice.call(arguments);
						argz.unshift("["+d.subdir+d.file+":"+d.line+" in "+d.method+"()] ");
						var s = util.format.apply(undefined,argz);
						self._log(_n,s,undefined,process_name);
						// if(arguments.length > 1)		
						// 	self._log(_n, + arguments[0],arguments[1],d.subdir+d.file);
						// else
						// 	self._log(_n,"["+d.subdir+d.file+":"+d.line+" in "+d.method+"()] " + arguments[0],d.subdir+d.file);														
					}
				} else {
					self[_name] = function(){ // trace is special
						if(self.MASK_OUT & _n) {
							return; // fast track out - if this log function is turned off
						}
						var d = getStack();
						var argz = Array.prototype.slice.call(arguments);
						argz.unshift("["+d.subdir+d.file+":"+d.line+" in "+d.method+"()] ");
						var s = util.format.apply(undefined,argz);
						self._log(_n,s);
						// if(arguments.length > 1)		
						// 	self._log(_n, + arguments[0],arguments[1],d.subdir+d.file);
						// else
						// 	self._log(_n,"["+d.subdir+d.file+":"+d.line+" in "+d.method+"()] " + arguments[0],d.subdir+d.file);														
					}
				}
			} // end trace
			else {
				// NEW STYLE API
				if(always_use_origin) {
					self[_name] = function(){
						if(self.MASK_OUT & _n) {
							return; // fast track out - if this log function is turned off
						}
						var s = util.format.apply(undefined,arguments);
						self._log(_n,s,undefined,process_name);
					}			
				} else {
					self[_name] = function(){
						if(self.MASK_OUT & _n) {
							return; // fast track out - if this log function is turned off
						}
						var s = util.format.apply(undefined,arguments);
						self._log(_n,s);
					}			
				}
				// extended API allows you to specify TAG, ORIGIN, etc
				if(always_use_origin) {
					self[_name+'_ex'] = function() {
						if(self.MASK_OUT & _n) {
							return; // fast track out - if this log function is turned off
						}
						if(arguments.length > 1 && typeof arguments[0] === 'object') {                        
							var argz = Array.prototype.slice.call(arguments);
							argz.shift();
							var s = util.format.apply(undefined,argz);
							//(level,message,tag,origin,extras)
							if(arguments[0].origin === undefined)
								self._log(_n,s,arguments[0].tag,process_name,arguments[0]);
							else
								self._log(_n,s,arguments[0].tag,arguments[0].origin,arguments[0]);
						} else {
							var s = util.format.apply(undefined,arguments);
							self._log(_n,s,undefined,process_name);
						}
					}
				} else {
					self[_name+'_ex'] = function() {
						if(self.MASK_OUT & _n) {
							return; // fast track out - if this log function is turned off
						}
						if(arguments.length > 1 && typeof arguments[0] === 'object') {                        
							var argz = Array.prototype.slice.call(arguments);
							argz.shift();
							var s = util.format.apply(undefined,argz);
							//(level,message,tag,origin,extras)
							self._log(_n,s,arguments[0].tag,arguments[0].origin,arguments[0]);
						} else {
							var s = util.format.apply(undefined,arguments);
							self._log(_n,s);
						}
					}										
				}
			}
		}


		if(old_style_API) {
			// make a LEVEL_fmt version of the log level command - this won't accept TAG or ORIGIN, but
			// will let the user use multiple parameters, ala node.js - using util.format()
			// example: log.debug_fmt("%d", 4)
			self[_name+'_fmt'] = function() {
				var s = util.format.apply(undefined,arguments);
				self._log(_n,s);
			}
			// a LEVEL_tag_fmt variation.
			// a variation allowing a tag and fancy formating.
			// this is more expensive.
			self[_name+'_tag_fmt'] = function() {
				var args = [];
				for(var n=1;n<arguments.length;n++)
					args[n-1] = arguments[n];
				var s = util.format.apply(undefined,args);
				self._log(_n,s,arguments[0]);
			}

			// this is a LEVEL_trace version of the function.
			// it provdes a location where the log was made. this is *very* expensive.
			self[_name+'_trace'] = function() {
				if(do_trace) {
					var args = [];
					for(var n=0;n<arguments.length;n++)
						args[n] = arguments[n];
					var d = getStack();
					var s = util.format.apply(undefined,args);
					self._log(_n,"["+d.subdir+d.file+":"+d.line+" in "+d.method+"()] "+s,arguments[0],d.subdir+d.file);						
				} else
					self[_name+'_tag_fmt'].apply(self,arguments);
			}
		}
	}



	if(!instance && !client_only) {
		instance = new nativelib.Logger();
		instance.start(function(){              // replace dummy logger function with real functions... as soon as can.
			_console_log("START!!!!!!!!!!!");
			var levelsK = Object.keys(levels);
			
			if(default_sink)
				instance.addSink(default_sink);

			for(var n=0;n<levelsK.length;n++) {
				var N = levels[levelsK[n]];
				var name = levelsK[n];
				instance.addLevelLabel(N,name);  // place label into native binding
//				console.log("adding " + name);
				createfunc(name,N);
			}
		});
	} else if(!instance && client_only) {
//		console.dir(nativelib);
		instance = new nativelib.Client();
		_console_log("START!!!!!!!!!!! CLIENT");
		instance.start();

		var levelsK = Object.keys(levels);

		for(var n=0;n<levelsK.length;n++) {
			var N = levels[levelsK[n]];
			var name = levelsK[n];
			createfunc(name,N);
		}
	}


	/**
	 * @param {string} level
	 * @param {string} message
	 * @param {string*} tag 
	 * @param {string*} origin 
	 * @return {[type]} [description]
	 */
	if(client_only && !client_no_origin_as_pid) {
		this._log = function(level,message,tag,origin,extras) {
			var	originN = default_originN;
			if(origin)
				originN = getOriginId(origin);
			var tagN = undefined;
			if(tag)
				tagN = getTagId(tag);
	//		console.log(""+message+","+level + ","+tagN+","+originN);
			instance.log(message,level,tagN,originN,extras);

		}		
	} else {
		this._log = function(level,message,tag,origin,extras) {
			var	originN = undefined;
			if(origin)
				originN = getOriginId(origin);
			var tagN = undefined;
			if(tag)
				tagN = getTagId(tag);
	//		console.log(""+message+","+level + ","+tagN+","+originN);
			instance.log(message,level,tagN,originN,extras);

		}
	}

	if(!client_only) {

		this.addTarget = function(obj,cb) {
			// TODO need to validate format strings so that they don't do unsafe things in native code
			return instance.addTarget(obj,cb);
		}

		this.addFilter = function(obj) {
			// use IDs - not strings
			if(obj.tag)
				obj.tag = getTagId(obj.tag);
			if(obj.origin)
				obj.origin = getOriginId(obj.origin);
			return instance.addFilter(obj);
		}

		this.modifyFilter = function(obj) {
			// use IDs - not strings
			if(obj.tag)
				obj.tag = getTagId(obj.tag);
			if(obj.origin)
				obj.origin = getOriginId(obj.origin);
			return instance.modifyFilter(obj);
		}

		this.createPTS = function() {
			return instance.createPTS.apply(instance,arguments);
		}

		this.addSink = function() {
			return instance.addSink.apply(instance,arguments);
		}

		this.addOriginLabel = function(){
			return instance.addOriginLabel.apply(instance,arguments);
		}

		this.modifyDefaultTarget = function() {
			return instance.modifyDefaultTarget.apply(instance,arguments);
		}
		this.enableTarget = function() {
			return instance.enableTarget.apply(instance,arguments);
		}
		this.disableTarget = function() {
			return instance.disableTarget.apply(instance,arguments);
		}


		// setup well-known Tag names. 
		// See: grease_common_tags.h
		getTagId("echo",0xFFF01);
		getTagId("console",0xFFF02);
		getTagId("native",0xFFF03);

		this.setProcessName = function(s) {
			process_name = s;
			instance.addOriginLabel(PID,s);	
		}
		this.setProcessName(process_name);


	}

	this.getDefaultSink = function() {
		return default_sink;
	};

	this.setGlobalOpts = function(obj) {
		if(obj.levelFilterOutMask)
			self.MASK_OUT = obj.levelFilterOutMask;
		instance.setGlobalOpts(obj);
	};

	var stdoutTag = getTagId("stdout");
	var stderrTag = getTagId("stderr");

	this.stdoutWriteOverride = function(s) {
		if(s.length > 0) {	
			instance.log(s.substr(0,s.length-1),levels.log,stdoutTag);			
		}
	}

	this.stderrWriteOverride = function(s) {
		if(s.length > 0) {	
			instance.log(s.substr(0,s.length-1),levels.error,stderrTag);			
		}
	}

	this.stdoutWriteOverridePID = function(s) {
		if(s.length > 0) {	
			instance.log(s.substr(0,s.length-1),levels.log,stdoutTag,PID);			
		}
	}

	this.stderrWriteOverridePID = function(s) {
		if(s.length > 0) {	
			instance.log(s.substr(0,s.length-1),levels.error,stderrTag,PID);			
		}
	}



	var Writable = require('stream').Writable;

	function WritableConsole(opt) {  // TODO add opt for 'origin' and 'tag'
  		Writable.call(this, opt);
 	    this.logger = self;
 	    if(opt.level) {
 	    	this.level = opt.level;
 	    } else {
 	    	this.level = levels.log;
 	    }
 	}

	util.inherits(WritableConsole, Writable);

	WritableConsole.prototype._write = function(chunk, encoding, callback) {
//		self._log(self.LEVELS.log,util.inspect(arguments));
		if(util.isBuffer(chunk)) {			
			this.logger._log(this.level,chunk.toString('utf8'));				
		} else {
			this.logger._log(this.level,chunk);				
		}
		callback();
	}

	this.getNewWritableConsole = function(levl) {
//		return new WritableConsole({level:levl});
//		
		var s = new Writable({highWaterMark:0});
		var _level = levl;
		var cb;
		var data;
		var tries = 0;
		var offset = 0;

		s.write = function() {
//			fs.write(1, data, offset, data.length - offset, null, onwrite);		
			self._log(_level,data);
			// return cb();			
		};

		// var onwrite = function(err, written) {
		// 	if (err && err.code === 'EPIPE') return cb()
		// 	if (err && err.code === 'EAGAIN' && tries++ < 30) return setTimeout(write, 10);
		// 	if (err) return cb(err);

		// 	tries = 0;
		// 	if (offset + written >= data.length) return cb();

		// 	offset += written;
		// 	write();
		// };

		s._write = function(_data, enc, _cb) {
			offset = 0;
			cb = _cb;
			data = _data;
			write();
		};

		s._writev = null;

		s._isStdio = true;
		// s.isTTY = process.stdout.isTTY;
		s.isTTY = false;
		s.on('finish', function() {
			// fs.close(1, function(err) {
			// 	if (err) s.emit('error', err);
			// });
		});

		return s;
	}	

}




module.exports = function(options) {
	return new setup(options);
};
