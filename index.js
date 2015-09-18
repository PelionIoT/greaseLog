// grease-log
// (c) WigWag Inc 2014
//


//var build_opts = require('build_opts.js');
var util = require('util');
var build_opts = { 
	debug: 1 
};
var colors = require('./colors.js');
//var _console_log = console.log;
var _console_log = function() {}

var nativelib = null;


var _logger = {};


var instance = null;
var setup = function(options) {
//	console.log("SETUP!!!!!!!!!!");

	var TAGS = {};
	var tagId = 1;

	var ORIGINS = {};
	var originId = 1;

	var getTagId = function(o) {
		var ret = TAGS[o];
		if(ret)
			return ret;
		else {
			ret = tagId++;
			TAGS[o] = ret;
			instance.addTagLabel(ret,o);
			return ret;
		}
	}

	var getOriginId = function(o) {
		var ret = ORIGINS[o];
		if(ret)
			return ret;
		else {
			ret = originId++;
			ORIGINS[o] = ret;
			instance.addOriginLabel(ret,o);	
			return ret;
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
	var client_only = false;
	if(options) {
//		console.dir(options);
		if(options.levels) levels = options.levels;
		if(options.do_trace !== undefined) do_trace = options.do_trace;
		if(options.default_sink !== undefined) {
			default_sink = options.default_sink;
		}
		if(options.client_only) { client_only = true; }
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

//					console.log("log: " + util.inspect(arguments));
				// if(arguments.length > 2)
				// 	self._log(_n,arguments[2],arguments[1],arguments[0]);
				// else if(arguments.length == 2)
				// 	self._log(_n,arguments[1],arguments[0]);
				// else
				// 	self._log(_n,arguments[0]);
			}
		} else {
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
	}


	// this.addTagLabel = function(l){
	// 	var n =	getTagId(l);
	// 	instance.addTagLabel(n,l);
	// }

	// this.addTagLabel = function(l){
	// 	var n =	getTagId(l);
	// 	instance.addTagLabel(n,l);
	// }



	this.setGlobalOpts = function(obj) {
		if(obj.levelFilterOutMask)
			self.MASK_OUT = obj.levelFilterOutMask;
		instance.setGlobalOpts(obj);
	};

}




module.exports = function(options) {
	return new setup(options);
};
