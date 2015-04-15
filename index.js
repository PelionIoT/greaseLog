// DeviceJS
// (c) WigWag Inc 2014
//
// tuntap native interface


//var build_opts = require('build_opts.js');
var util = require('util');
var build_opts = { 
	debug: 1 
};
var colors = require('./colors.js');

var nativelib = null;
try {
	nativelib = require('./build/Release/greaseLog.node');
} catch(e) {
	if(e.code == 'MODULE_NOT_FOUND')
		nativelib = require('./build/Debug/greaseLog.node');
	else
		console.error("Error in nativelib [debug]: " + e + " --> " + e.stack);
}

var _logger = {};

var natives = Object.keys(nativelib);
for(var n=0;n<natives.length;n++) {
	console.log(natives[n] + " typeof " + typeof nativelib[natives[n]]);
	_logger[natives[n]] = nativelib[natives[n]];
}

var instance = null;
var setup = function(levels) {
	console.log("SETUP!!!!!!!!!!");

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
		'info'     : 0x01,
		'log'      : 0x01,
		'error'    : 0x02,
		'warn'     : 0x04,
		'debug'    : 0x08,
		'debug2'   : 0x10,
		'debug3'   : 0x20,
		'user1'    : 0x40,
		'user2'    : 0x80,  // Levels can use the rest of the bits too...
		'success'  : 0x100
	};

	if(!levels) levels = LEVELS_default;
	console.dir(levels);

	this.LEVELS = {};
	this.LEVELS.ALL = 0xFFFFFFFF; // max uint32_t


	var levelsK = Object.keys(levels);
	for(var n=0;n<levelsK.length;n++) {
		if(!this[levelsK[n]]) {
			var N = levels[levelsK[n]];
			var name = levelsK[n];
			this.LEVELS[name] = N;  // a copy, used for caller when creating filters
			this[name] = function(){
			}
		} else {
			throw new Error("Grease: A level name is used more than once: " + levels[n]);
		}

	}



	var self = this;


	/**
	 * Creates the user's log.LEVEL functions
	 * forms
	 * log.LEVEL(origin,tag,message)
	 * log.LEVEL(tag,message)
	 * log.LEVEL(message)
	 */

	if(!instance) {
		instance = nativelib.newLogger();
		instance.start(function(){              // replace dummy logger function with real functions... as soon as can.
			console.log("START!!!!!!!!!!!");
			var levelsK = Object.keys(levels);
			
			var createfunc = function(_name,_n) {
				self[_name] = function(){
//					console.log("log: " + util.inspect(arguments));
					if(arguments.length > 2)
						self._log(_n,arguments[2],arguments[1],arguments[0]);
					else if(arguments.length == 2)
						self._log(_n,arguments[1],arguments[0]);
					else
						self._log(_n,arguments[0]);
				}
			}

			for(var n=0;n<levelsK.length;n++) {
				var N = levels[levelsK[n]];
				var name = levelsK[n];
				instance.addLevelLabel(N,name);  // place label into native binding
				console.log("adding " + name);
				createfunc(name,N);
			}
		});
	}



	/**
	 * @param {string} level
	 * @param {string} message
	 * @param {string*} tag 
	 * @param {string*} origin 
	 * @return {[type]} [description]
	 */
	this._log = function(level,message,tag,origin) {
		var	originN = undefined;
		if(origin)
			originN = getOriginId(origin);
		var tagN = undefined;
		if(tag)
			tagN = getTagId(tag);
//		console.log(""+message+","+level + ","+tagN+","+originN);
		instance.log(message,level,tagN,originN);
	}


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

	this.createPTS = instance.createPTS;



	// this.addTagLabel = function(l){
	// 	var n =	getTagId(l);
	// 	instance.addTagLabel(n,l);
	// }

	// this.addTagLabel = function(l){
	// 	var n =	getTagId(l);
	// 	instance.addTagLabel(n,l);
	// }


	this.addOriginLabel = instance.addOriginLabel;

	this.setGlobalOpts = instance.setGlobalOpts;
}





module.exports = new setup();


