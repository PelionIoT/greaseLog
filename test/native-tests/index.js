
var pseudoFS = null;
try {
	pseudoFS = require('./build/Debug/pseudofs.node');
	console.error(" ********** using pseudoFS debug module.  ************* ");
} catch(e) {
	try {
		pseudoFS = require('./build/Release/pseudofs.node');
	} catch(e) {
		console.error(" ********** is pseudofs built???? ");
	}
}
// callback = function(error,Buffer,readlen) {}
var readPseudo = function(path, options, callback) {
	var _cb;
	if(callback) _cb = callback; else _cb = options;
	var wrap = function(error,buf,readlen) {
		var _buf = undefined;
		if(!error)           // why?? because apparently the writers of node::Buffer thought
			_buf = buf.slice(0,readlen);  // that making the 'length_' property a private was a good idea - so easier to adjust size here.
		else {
			if(error.errno) {   // match the standard node.js 'fs' error format.
				error.code = pseudoFS.CONSTS[error.errno];
			}
		}
		_cb(error,_buf);
	}

	if(callback) {
		pseudoFS.readPseudo(path,options,wrap);
	} else 
	    pseudoFS.readPseudo(path,wrap);
}

var writePseudo = function(path, data, options, callback) {
	var _cb;
	if(callback) _cb = callback; else _cb = options;
	var wrap = function(error) {
		var _buf = undefined;
 		if(error && error.errno) {   // match the standard node.js 'fs' error format.
 		    error.code = pseudoFS.CONSTS[error.errno];
 	    }
		_cb(error,_buf);
	}

	if(callback) {
		pseudoFS.writePseudo(path, data, options,wrap);
	} else 
	    pseudoFS.writePseudo(path, data, wrap);
}


module.exports = {
	readPseudo: readPseudo,
	writePseudo: writePseudo,
	CONSTS: pseudoFS.CONSTS
}
