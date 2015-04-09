var tinytim = require('../tinytim'), dateFormat = require('../dateformat'), utils = require('./utils')
var colors = require('../console-colors');
var util = require('util'); // built-in

var orderedTable = require('../orderedTable');

var _ = require('../underscore.js');
var fillInDefaults = function(target) {
  for(var n=1;n<arguments.length;n++)
    _.defaults(target, arguments[n]);
  }

module.exports = (function() {



	// default config
	var _config = {
		format : "{{timestamp}} <{{title}}> {{subdir}}{{file}}:{{line}} ({{method}}) {{message}}",
		dateformat : "UTC:yyyy-mm-dd'T'HH:MM:ss.l'Z'",
		preprocess : function(data) {
		},
		transport : function(data) {
			console.log(data.output);
		},
		filters : [],
		level : 'log',
		methods : [ 'log', 'trace', 'debug', 'info', 'warn', 'error' ], 
		needstack : {
		    'log' : false,
		    'trace' : true,
		    'debug' : true,
		    'info'  : false,
		    'warn'  : true,
		    'error' : true
		},
		//there are four modes for deciding what debug messages are printed based on tag
		//inclusive: all are printed by default. only tags that are defined as false aren't.
		//exclusive: none are printed by default. only tags that are defined as true are.
		//all: messages are printed regardless of tag
		//none: no messages are printed
		ignoreLocalConf : true,
		tagMode : "inclusive",
		defaultTags: {
			"default": true
		},
		tags: {}
	};


	var args = {};
   	args = utils.union(args, arguments); // union all args

	var user_conf = null;
	var logger_conf = 'logger.conf';

	try {
		user_conf = require(logger_conf);
	} catch(e) {}

	if(!(user_conf && user_conf.ignoreLocalConf) && args && args.config_dir) { // try to load a specified logger.conf if there
		console.log("searching for local conf");
		logger_conf = args.config_dir + '/logger.conf';
		var local_user_conf;
		try {
			local_user_conf = require(logger_conf);
		} catch(e) {}
		if(!local_user_conf) {
			logger_conf = args.config_dir + '/logger.default.conf'; // allows module based defaults
			try {
				local_user_conf = require(logger_conf);
			} catch(e) {}
		}
		if (local_user_conf) {
			console.log("overriding conf with local");
			user_conf = local_user_conf;
		}
		else {
			console.log("not overriding conf with local");
		}
	}



	if(user_conf) {
		if(user_conf.tags) 
			fillInDefaults(_config.tags, user_conf.tags); 		
		if(user_conf.tagMode)
			_config.tagMode = user_conf.tagMode;
		if (user_conf.checkpoints)
			_config.checkpoints = user_conf.checkpoints;
	} else {
//		console.log("NOTE - console.js: No User Tags found");
	}

	fillInDefaults(_config.tags, _config.defaultTags); 

	// union user's config and default
	_config = utils.union(_config, arguments);

	if(global.dev$) {
   	    dev$._logGlobalHooks = new orderedTable(); // hook table allows service to intercept log calls and do stuff with them.

   	}

   	var _hook = function(func,level) {
   		this.hook = func;
   		if(level)
   			this.level = level;
   		for(var n = 0; n<_config.methods.length;n++)
   			if(_config.methods[n] == this.level) this._levelN = n;
   	}

	_hook.prototype.enabled = true;


	// main log method
	var _log = function(tag, level, title, format, filters, needstack, args, nohook) {

		if (_config.tagMode !== "all" && (_config.tagMode === "none" || (_config.tagMode === "inclusive" ? _config.tags[tag] === false : _config.tags[tag] !== true))) {
			return null;
		}

		var data = {
			timestamp : dateFormat(new Date(), _config.dateformat),
			message : "",
			title : title,
			tag: tag,
			level : level,
			args : args
		};
		data.method = data.path = data.line = data.pos = data.file = data.subdir = '';

		if (needstack) {
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
		}

		_config.preprocess(data);
		var msg = utils.format.apply(this, data.args);
		data.message = msg;

		// call micro-template to ouput
		data.output = tinytim.tim(format, data);

		if(global.dev$ && dev$._logGlobalHooks && !nohook) 
			dev$._logGlobalHooks.forEach(function(val,id,order){
				data.preout = data.output;
				val.hook(data);
			});
		

		// process every filter method
		var len = filters.length;
		for ( var i = 0; i < len; i += 1) {
			data.output = filters[i](data.output, data);
			if (!data.output)
				return data;
			// cancel next process if return a false(include null, undefined)
		}

		// if (_config.tagMode !== "all" && (_config.tagMode === "none" || (_config.tagMode === "inclusive" ? _config.tags[tag] === false : _config.tags[tag] !== true))) {
		// 	return data;
		// }
		// trans the final result
		return _config.transport(data);
	};

	var _self = {};

	/**
	 * Installs a global hook for the looger system. 
	 * @param  {[type]} func  The hook function callback:<br>
	 * <pre>
	 * function(output) {
	 * }
	 * </pre>
	 * @param  {[type]} level [description]
	 * @return {[type]}       [description]
	 */
	_self.installGlobalHook = function(func,level) {
		if(global.dev$ && dev$._logGlobalHooks) {
			var hook = new _hook(func,level);
			return dev$._logGlobalHooks.add(hook);
		} else return null;
	}

	_self.removeGlobalHook = function(id) {
		if(global.dev$ && dev$._logGlobalHooks) {
			_globalHooks.remove(id);
		}
	}

	_self.dir = function() {
		var ret = "";
		for(var n=0;n<arguments.length;n++) {
			if(n>0)
				ret += "\n";
			if(typeof arguments[n] == 'object')
				ret += util.inspect(arguments[n]);
			else
				ret += '[not object, was: ' + typeof arguments[n] + ']';
		}
		return ret;
	};

	_config.format = Array.isArray(_config.format) ? _config.format
			: [ _config.format ];

	_config.filters = Array.isArray(_config.filters) ? _config.filters
			: [ _config.filters ];

	var len = _config.methods.length, fLen = _config.filters.length, lastFilter;
	if (fLen > 0)
		if (Object.prototype.toString.call(_config.filters[fLen - 1]) != '[object Function]') {
			fLen -= 1;
			lastFilter = _config.filters[fLen];
			_config.filters = _config.filters.slice(0, fLen);
		}

	if (typeof (_config.level) == 'string')
		_config.level = _config.methods.indexOf(_config.level);

	for ( var i = 0; i < len; ++i) {
		var title = _config.methods[i];
		if (i < _config.level)
			_self[title] = (function() {
			});// empty function
		else {
			var format = _config.format[0];
			if (_config.format.length === 2 && _config.format[1][title])
				format = _config.format[1][title];
			
			var needstack = false;
			if (_config.needstack && _config.needstack[title] != undefined) {// use the _config.needstack stuff first.
				needstack = _config.needstack[title];
//				console.log("have need stack for " + title + needstack);
			} else if (/{{(method|path|line|pos|file)}}/gi.test(format))
				needstack = true;

			var filters;
			if (lastFilter && lastFilter[title])
				filters = Array.isArray(lastFilter[title]) ? lastFilter[title]
						: [ lastFilter[title] ];
			else
				filters = _config.filters;

			_self[title] = (function(tag, level, title, format, filters, needstack) {
				return (function() {
					return _log(tag, level, title, format, filters, needstack,
							arguments);
				});
			})("default", i, title, format, filters, needstack);

			// make a 'nohook' version as well, for special cases where we need to avoid hooking of log calls (like a log monitor) 
			_self[title+'_nh'] = (function(tag, level, title, format, filters, needstack) {
				return (function() {
					return _log(tag, level, title, format, filters, needstack,
							arguments, true);
				});
			})("default", i, title, format, filters, needstack);

			// make a 'forced' trace version
			_self[title+'_trace'] = (function(tag, level, title, format, filters, needstack) {
				return (function() {
					return _log(tag, level, title, format, filters, true,
							arguments, true);
				});
			})("default", i, title, format, filters, needstack);

			// and a 'forced' no-trace version
			_self[title+'_nt'] = (function(tag, level, title, format, filters, needstack) {
				return (function() {
					return _log(tag, level, title, format, filters, false,
							arguments, true);
				});
			})("default", i, title, format, filters, needstack);



		}
	}

	_self.custom = function () {
		var tag = arguments[0];
		//delete arguments[0];
		var newArguments = {};
		// for (var i = 0; i < arguments.length; ++i) if (i > 0) {
		// 	newArguments[i-1] = arguments[i];
		// }

		for (var key in arguments) {
			newArguments[parseInt(key)-1] = arguments[key];
		}
		newArguments.length = arguments.length-1;

		return _log(tag, len, "custom:"+tag, ""+_config.format, [colors.green], true,newArguments);
	};

	if (typeof _global_config_checkpoints === "undefined") {
		_global_config_checkpoints = [];
	}

	_self.checkpoint = function () {

		var
			tag = arguments[0],
			message = arguments[1];

		if (!_config.checkpoints || (_config.tagMode !== "all" && (_config.tagMode === "none" || (_config.tagMode === "inclusive" ? _config.tags[tag] === false : _config.tags[tag] !== true)))) {
			//while(true);
			//console.log("failing debugger with tag ",tag);
			return null;
		}

		
		_global_config_checkpoints.push({
			time: (new Date()).getTime(),
			tag: tag,
			message : message
		});


	};

	_self.dumpCheckpoints = function () {
		if (!_config.checkpoints) {
			return null;
		}
		console.log("dumping checkpoints:",_global_config_checkpoints.length);
		for (var i = 0; i < _global_config_checkpoints.length; ++i) {
			console.log(
				"time:",
				(i == 0 ? "st" : _global_config_checkpoints[i].time - _global_config_checkpoints[i-1].time)+"\t\t",
				"tag:",
				_global_config_checkpoints[i].tag+"\t\t",
				"\"",
				_global_config_checkpoints[i].message+"\""
			);
		}
	};

	_self.getCheckpointsHTML = function () {
		if (!_config.checkpoints) {
			return null;
		}
		var ret = "<html><head></head><body>";

		ret += "</body></html>";
		return ret;
	};


	_self.stackTrace = function() {
		var e = new Error('dummy');
		var stack = '\nstack: @ ' + e.stack
		    .replace(/^[^\(]+?[\n$]/gm, '')
			.replace(/^\s+at\s+/gm, '')
			.replace(/^Object.<anonymous>\s*\(/gm, '{anonymous}()@')
			.replace(/.*\n/,'') // drop the first stack, b/c it's this function
			.replace(/\n/gm, '\n       @ ');
        return stack;
	}

	return _self;
});