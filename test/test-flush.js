// basic test of logging.

var logger = require('../index.js');

console.dir(logger);

var N = 0;

logger.log("ABCD1234");

setTimeout(function(){
	logger.log("---DONE---");
}, 10000);




//console.log("done");