// basic test of logging.
var util = require('util');
var logger = require('../index.js')();

//console.dir(logger);

var N = 0;

var Console = require('console').Console;

// var newstdout = logger.getNewWritableConsole(logger.LEVELS.log);
// var newstderr = logger.getNewWritableConsole(logger.LEVELS.error);

// var console = new Console(newstdout,newstderr);
	
// process.stdout = newstdout;
// process.stderr = newstderr;

// console.log("newstdout.__proto__ ----->");
// console.dir(newstdout.__proto__);
// console.log("process.stdout.__proto__ ------------>"); 
// console.dir(process.stdout.__proto__);
// console.log("process.stdout.__proto__.__proto__ ------------>"); 
// console.dir(process.stdout.__proto__.__proto__);
// console.log("------------"); 
// console.dir(newstdout);
// console.log("------------"); 
// console.dir(process.stdout);


// process.stdout = newstdout;
// process.stderr = newstderr;


process.stdout.write = logger.stdoutWriteOverridePID;
process.stderr.write = logger.stderrWriteOverridePID;

logger.setProcessName('testprog');

logger.modifyDefaultTarget({
	format: {
    	time: "[%ld:%d] ",
    	level: "%-10s ", // left align
    	tag: "\x1B[33m%-10s\x1B[39m ",
    	origin: "\x1B[37m\x1B[100m%-10s\x1B[39m\x1B[49m ",
	}
});	
// process.stderr.write = function(string,encoding,fd) {
// 	logger.error("STDERR: " + string);
// };

			var ret = logger.addFilter({ 
				// target: targetid,   // same as saying target: 0 (aka default target)
				mask: logger.LEVELS.debug,
				pre: "\x1B[90m",  // grey
				post: "\x1B[39m"
			}); 
			var ret = logger.addFilter({ 
				// target: targetid,
				mask: logger.LEVELS.warn,
				pre: '\x1B[33m',  // yellow
				post: '\x1B[39m'
			});
			var ret = logger.addFilter({ 
				// target: targetid,
				mask: logger.LEVELS.error,
				pre: '\x1B[31m',  // red
				post: '\x1B[39m'
			});
			var ret = logger.addFilter({ 
				// target: targetid,
				tag: "stderr",
				mask: logger.LEVELS.error,
				post_fmt_pre_msg: '\x1B[90m[console] \x1B[31m',  // red
				post: '\x1B[39m'
			});
			var ret = logger.addFilter({ 
				// target: targetid,
				mask: logger.LEVELS.success,
				pre: '\x1B[32m',  // green
				post: '\x1B[39m'
			});
			var ret = logger.addFilter({ 
				// target: targetid,
				mask: logger.LEVELS.log,
				pre: '\x1B[39m'  // default
//				post: '\x1B[39m'
			});
			var ret = logger.addFilter({ 
				// target: targetid,
				mask: logger.LEVELS.log,
				tag: "stdout",
				post_fmt_pre_msg: '\x1B[90m[console] \x1B[39m'  // default
//				post: '\x1B[39m'
			});


console.log("Hello. Log.");


var testCallback = function(str,id) {
	var entries = str.split("ϐ");      // use special char to separate entries...
	for(var n=0;n<entries.length;n++)
		util.log("CB (" + id + ")>" + entries[n] + "<"); // DO NOTE: you need to use util.log here, otherwise it will be recursive test ;)
}




// logger.addTarget({
// 		tty
// 	    callback: testCallback,

// 	    delim: 'ϐ' // separate each entry with a special char
// 	},function(targetid,err){
// 		if(err) {
// 			console.log("error: "+ util.inspect(err));
// 		} else {

// 			var ret = logger.addFilter({ 
// 				target: targetid,
// 				mask: logger.LEVELS.log,
// 				pre: "LOG>   "
// 			});
// 			var ret = logger.addFilter({ 
// 				target: targetid,
// 				mask: logger.LEVELS.error,
// 				pre: "ERROR> "
// 			});
// 			var ret = logger.addFilter({ 
// 				target: targetid,
// 				mask: logger.LEVELS.warn,
// 				pre: "WARN>  "            // unfortunately, Console does not differentiate these.
// 			});

			setTimeout(function(){
				for(var n=0;n<100;n++) {
					console.log("Hello. Log. " + n);				
					console.error("Hello. Error. " + n);				
					console.warn("Hello. Warn." + n);				
					console.dir({hello:"there", n: n});
				}
			},1000);
// 		}
// 	}
// );

// var big = "123456789012345678901234567890123456789012345678901234567890123456789012345678901234567890123456789012345678901234567890123456789012345678901234567890123456789012345678901234567890123456789012345678901234567890123456789012345678901234567890123456789012345678901234567890123456789012345678901234567890123456789012345678901234567890123456789012345678901234567890123456789012345678901234567890123456789012345678901234567890123456789012345678901234567890123456789012345678901234567890123456789012345678901234567890123456789012345678901234567890123456789012345678901234567890123456789012345678901234567890123456789012345678901234567890123456789012345678901234567890123456789012345678901234567890123456789012345678901234567890123456789012345678901234567890123456789012345678901234567890123456789012345678901234567890123456789012345678901234567890123456789012345678901234567890123456789012345678901234567890123456789012345678901234567890123456789012345678901234567890123456789012345678901234567890123456789012345678901234567890123456789012345678901234567890123456789012345678901234567890123456789012345678901234567890123456789012345678901234567890123456789012345678901234567890123456789012345678901234567890123456789012345678901234567890123456789012345678901234567890123456789012345678901234567890123456789012345678901234567890123456789012345678901234567890123456789012345678901234567890123456789012345678901234567890123456789012345678901234567890123456789012345678901234567890123456789012345678901234567890123456789012345678901234567890123456789012345678901234567890123456789012345678901234567890123456789012345678901234567890123456789012345678901234567890123456789012345678901234567890123456789012345678901234567890123456789012345678901234567890123456789012345678901234567890123456789012345678901234567890123456789012345678901234567890123456789012345678901234567890123456789012345678901234567890123456789012345678901234567890123456789012345678901234567890123456789012345678901234567890123456789012345678901234567890123456789012345678901234567890123456789012345678901234567890123456789012345678901234567890123456789012345678901234567890123456789012345678901234567890123456789012345678901234567890123456789012345678901234567890123456789012345678901234567890123456789012345678901234567890123456789012345678901234567890123456789012345678901234567890123456789012345678901234567890123456789012345678901234567890123456789012345678901234567890123456789012345678901234567890123456789012345678901234567890123456789012345678901234567890123456789012345678901234567890123456789012345678901234567890123456789012345678901234567890123456789012345678901234567890123456789012345678901234567890123456789012345678901234567890123456789012345678901234567890123456789012345678901234567890123456789012345678901234567890123456789012345678901234567890";
// for(var n=0;n<10000;n++) {`
// 	logger.log("test log " + n);
// 	setTimeout(function(q){
// 		logger.error("AAAAAAAAAAAAAAAAAAAAAAAAAAAAA--"+q+"--");
// 		N++;
// 		if(N == 9999) {
// 			setTimeout(function(p) {
// 				console.log("------------END--------------");
// 				logger.debug("LAST ONELAST ONELAST ONELAST ONELAST ONELAST ONELAST ONELAST ONELAST ONELAST ONELAST ONE");				
// 			}, 10+n*10, n);
// 		}
// 	}, 50+n*10,n);
// 	if(n % 11 == 0) {
// 		setTimeout(function(q){
// 			logger.log(big +"#################" + q + "*");
// 		}, 100+n*10,n);
// 	}		

// }

setTimeout(function(){
	console.log("DONE.");
},3000);



//console.log("done");