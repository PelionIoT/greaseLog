var testmodule = require('./build/Debug/obj.target/testmodule.node');


//console.dir(testmodule);

testmodule.doSomeLoggin("Hello",100,10,function(){
	console.log("DONE");
})


// var pseudoFS = null;
// try {
// 	pseudoFS = require('./build/Debug/pseudofs.node');
// 	console.error(" ********** using pseudoFS debug module.  ************* ");
// } catch(e) {
// 	try {
// 		pseudoFS = require('./build/Release/pseudofs.node');
// 	} catch(e) {
// 		console.error(" ********** is pseudofs built???? ");
// 	}
// }

// module.exports = {
// 	readPseudo: readPseudo,
// 	writePseudo: writePseudo,
// 	CONSTS: pseudoFS.CONSTS
// }
