// basic test of logging.

var logger = require('../index.js')();
var util = require('util');


//console.dir(logger);

var N = 0;


// logger.addTarget({
// 	    file: "testlog.log"
// 	},function(targetid,err){
// 		if(err) {
// 			console.log("error: "+ util.inspect(err));
// 		} else {
// 			console.log("added target: " + targetid);
// 			var ret = logger.addFilter({ 
// 				target: targetid,
// 				mask: logger.LEVELS.error
// 			});
// 			console.log("added filter: " + ret);
// 			var ret = logger.addFilter({ 
// 				target: targetid,
// 				mask: logger.LEVELS.debug,
// 				tag: 'Eds'
// 			});
// 			console.log("added filter: " + ret);
// 			var ret = logger.addFilter({ 
// 				target: targetid,
// 				mask: logger.LEVELS.debug,
// 				tag: 'Eds',
// 				origin: 'special.js'
// 			});
// 			console.log("added filter: " + ret);
// 		}

// 		logger.error("************ FIRST **********");
// 		for(var n=0;n<1000;n++) {
// 			logger.debug("....DEBUG....");
// 			logger.debug('Eds', "....DEBUG....");			
// 			logger.error("....ERROR....");
// 			logger.log(" log log log");
// 		}
// 		logger.error("************ LAST **********");
// 	});

var testCallback = function(str,id) {
	console.log("CB (" + id + ")>" + str + "<");
}

var fd = null;

logger.setGlobalOpts({
//	levelFilterOutMask: logger.LEVELS.debug, // will block all debug messages, regardless of filters / targets /etc
//	defaultFilterOut: true
	show_errors: true
});

logger.modifyDefaultTarget({
	format: {
    	pre: 'PRE>', // pre: "pre>"   // 'bold' escape sequence
    	time: "[%ld:%d] ",
    	level: "<%s> ",
    	tag: "{%s} ",
    	origin: "(%s) ",
    	post: "<POST" // post: "<post"
	}
});	

// logger.createPTS(function(err,pty){
// 	if(err) {
// 		console.error("Error creating PTS: " + util.inspect(err));
// 	} else {

// 		fd = pty.fd;
// 		console.log("PTY: " + pty.path);

// 	}
// });

// logger.addTagLabel('example_tag');
// logger.addOriginLabel('example_origin');

// logger.addTarget({
// 	    tty: fd,
// 	    callback: testCallback,
// 	    delim: '\n', // separate each entry with a hard return
// 	    format: {
// //	    	pre: 'targ-pre>', // pre: "pre>"   // 'bold' escape sequence
// 	    	time: "[%ld:%d] ",
// 	    	level: "<%s> ",
// 	    	tag: "{%s} ",
// 	    	origin: "(%s) ",
// //	    	post: "<targ-post" // post: "<post"
// 	    },
// 	},function(targetid,err){
// 		if(err) {
// 			console.log("error: "+ util.inspect(err));
// 		} else {
// 			console.log("added rotate target: " + targetid);
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
		// }		
		var N = 1000;
		logger.debug("************ FIRST **********");
		var I = setInterval(function(){
			for(var n=0;n<10;n++) {
				logger.debug('example_tag','example_origin',"....debug me ["+N+"]....");
				logger.warn("....warn me ["+N+"]....");	
				logger.log("....log me ["+N+"]....");					
				logger.error("....error me ["+N+"]....");	
				logger.success("....info me ["+N+"]....");					
				N--;
			}
			if(N == 0) {
				clearInterval(I);
				logger.debug("************ LAST **********");	
//				setTimeout(function(){},2000);
			}
		},500);


	// });



// var big = "123456789012345678901234567890123456789012345678901234567890123456789012345678901234567890123456789012345678901234567890123456789012345678901234567890123456789012345678901234567890123456789012345678901234567890123456789012345678901234567890123456789012345678901234567890123456789012345678901234567890123456789012345678901234567890123456789012345678901234567890123456789012345678901234567890123456789012345678901234567890123456789012345678901234567890123456789012345678901234567890123456789012345678901234567890123456789012345678901234567890123456789012345678901234567890123456789012345678901234567890123456789012345678901234567890123456789012345678901234567890123456789012345678901234567890123456789012345678901234567890123456789012345678901234567890123456789012345678901234567890123456789012345678901234567890123456789012345678901234567890123456789012345678901234567890123456789012345678901234567890123456789012345678901234567890123456789012345678901234567890123456789012345678901234567890123456789012345678901234567890123456789012345678901234567890123456789012345678901234567890123456789012345678901234567890123456789012345678901234567890123456789012345678901234567890123456789012345678901234567890123456789012345678901234567890123456789012345678901234567890123456789012345678901234567890123456789012345678901234567890123456789012345678901234567890123456789012345678901234567890123456789012345678901234567890123456789012345678901234567890123456789012345678901234567890123456789012345678901234567890123456789012345678901234567890123456789012345678901234567890123456789012345678901234567890123456789012345678901234567890123456789012345678901234567890123456789012345678901234567890123456789012345678901234567890123456789012345678901234567890123456789012345678901234567890123456789012345678901234567890123456789012345678901234567890123456789012345678901234567890123456789012345678901234567890123456789012345678901234567890123456789012345678901234567890123456789012345678901234567890123456789012345678901234567890123456789012345678901234567890123456789012345678901234567890123456789012345678901234567890123456789012345678901234567890123456789012345678901234567890123456789012345678901234567890123456789012345678901234567890123456789012345678901234567890123456789012345678901234567890123456789012345678901234567890123456789012345678901234567890123456789012345678901234567890123456789012345678901234567890123456789012345678901234567890123456789012345678901234567890123456789012345678901234567890123456789012345678901234567890123456789012345678901234567890123456789012345678901234567890123456789012345678901234567890123456789012345678901234567890123456789012345678901234567890123456789012345678901234567890123456789012345678901234567890123456789012345678901234567890123456789012345678901234567890123456789012345678901234567890";
// for(var n=0;n<10000;n++) {
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





//console.log("done");