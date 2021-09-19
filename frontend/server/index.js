const express = require("express");
const cp = require("child_process");
const fs = require("fs");
const port = process.env.PORT || 3001;
const app = express();

cp.spawn('rm', ['-f', '/tmp/pipe_a']);
let fifop = cp.spawn('mkfifo', ["/tmp/pipe_a"]);  // Create Pipe B
fifop.on('exit', function(status) {
	if(status != 0) console.log('oh god oh fuck');
});


let lastChild = null;

app.get("/api", (req, res) => {
	if (req.query.command == "on") {
		console.log("open");
		cp.exec("/home/theninja583/Documents/advd/advd");
		res.send({status: "open success"});
	}
	else if (req.query.command == "off") {
		console.log("close");
		let yeet = cp.spawn("./write.sh");
		yeet.on('exit', function(status) {
			if(status != 0) console.log('sad');
		});
		res.send({status: "close success"});
	}
	else {
		res.send({status: "no action taken"});
	}
});

app.listen(port, () => {
	console.log(`Server listening on port ${port}`);
});
