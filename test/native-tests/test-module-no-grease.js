
// some logging in node-land....
var logger = console;

for(var n=0;n<10;n++)
	logger.log("I am from node.js"); // most of these will be at the end, that's ok for this test

var clearme = setInterval(function(){
	for(var n=0;n<4;n++) {
		logger.log("I am from node.js"); // most of these will be at the end, that's ok for this test
		logger.error("I am from node.js"); // most of these will be at the end, that's ok for this test
	}
},100); 

// -------------------------------------------------------------------------------------------------------
// THE TEST:
// -------------------------------------------------------------------------------------------------------

// Your module is pulled in. When it is, INIT_GLOG is called by your 
// InitAll(). Grease will 'auto-detect' (using glib symbol hunt magic)
// the other shared libraries (greaselog.node) library calls. So they will go to the same backed logger.
var testmodule = null;
try {
	testmodule = require('./build/Debug/obj.target/testmodule.node');
} catch(e) {
	if(e.code == 'MODULE_NOT_FOUND')
		testmodule = require('./build/Release/obj.target/testmodule.node');
	else
		console.error("Error in loading test module [debug]: " + e + " --> " + e.stack);
}

testmodule.doSomeLoggin("Hello",100,10,function(){
	console.log("native test over");
});


// grease buffers, so wait a second to see it all dump out for the test.
setTimeout(function(){
	clearInterval(clearme);
	logger.log("end (logger)"); // might not be at end (expected - b/c of interval above)
},2000);

