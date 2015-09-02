var util = require('util');

var pseudoFS = require('../index.js');

// --- Depending on node.js version - this may / may not work.
// var fs = require('fs');
// var pseudoFS = {
// 	readPseudo: function(path,cb) {
// 		fs.readFile(path,cb);
// 	},
//     writePseudo: function(path,data,cb) {
// 		fs.writeFile(path,data,cb);
// 	}
// }

console.log("dump all error codes: ");
console.dir(pseudoFS.CONSTS);

var path = "/proc/meminfo";
console.log('test reading a procfs thing: ' + path);
pseudoFS.readPseudo(path,function(err,data){
	if(err) {
		console.error("Error: " + util.inspect(err));
	} else
	    console.log("output: " + data);
});


path = "/proc/meminfo2"; // non existant
console.log('test reading a procfs thing: ' + path);
pseudoFS.readPseudo(path,function(err,data){
	if(err) {
		console.error("Expected Error (ok): " + util.inspect(err));
	} else
	    console.log("output: " + data);
});

var orig_hostname = "";
path = "/proc/sys/kernel/hostname"; // non existant
console.log('test setting host name: ' + path);
pseudoFS.readPseudo(path,function(err,data){
	if(err) {
		console.error("Error: " + util.inspect(err));
	} else {
		console.log("current hostname: " + data);
		orig_hostname = data.toString(); // read output is a Buffer
		pseudoFS.writePseudo(path,"newhostname",function(err) {
			if(!err) {
				pseudoFS.readPseudo(path,function(err,data){
					if(err) {
						console.error("Error: " + util.inspect(err));
					} else {
						console.log("current hostname (post change): " + data);
						pseudoFS.writePseudo(path,orig_hostname,function(err) {
							if(err) {
								console.error("Error: " + util.inspect(err));
							} else {
								console.log("returned hostname to: " + orig_hostname);
							}
						});
					}
				});
			} else {
				console.error("Error setting hostname: " + util.inspect(err));
			}
		});
	}
});

setTimeout(function(){
	console.log("************************************");
	console.log("second test - use Buffer not String.");
	console.log("************************************");


var orig_hostname = "";
path = "/proc/sys/kernel/hostname"; // non existant
console.log('test setting host name: ' + path);
pseudoFS.readPseudo(path,function(err,data){
	if(err) {
		console.error("Error: " + util.inspect(err));
	} else {
		console.log("current hostname: " + data);
		orig_hostname = data.toString(); // read output is a Buffer
		var buf = Buffer("newhostname"); // default UTF8 encoding
		pseudoFS.writePseudo(path,buf,function(err) {
			if(!err) {
				pseudoFS.readPseudo(path,function(err,data){
					if(err) {
						console.error("Error: " + util.inspect(err));
					} else {
						console.log("current hostname (post change): " + data);
						pseudoFS.writePseudo(path,orig_hostname,function(err) {
							if(err) {
								console.error("Error: " + util.inspect(err));
							} else {
								console.log("returned hostname to: " + orig_hostname);
							}
						});
					}
				});
			} else {
				console.error("Error setting hostname: " + util.inspect(err));
			}
		});
	}
});



},2000);