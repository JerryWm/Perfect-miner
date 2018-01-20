const crypto = require('crypto');
const EventEmitter = require('events').EventEmitter;
const spawn = require('child_process').spawn;
const fs = require('fs');
//const lockFile = require('lockfile');

const _ = require('c-struct');
const dgram = require('dgram');

const os = require('os');

const Common = require('./Common');
const Control = require('./Control');
const Constants = require('./../Share/App/Constants');
const NetClientEventEmitter = require('./../Share/Net/NetClientEventEmitter');

const CONFIG_PATH = "./client.json";

function lockFile(path, done, fail) {
	let locked = true;
	let fd = null;

	function removeFile() {
		try {
			fs.unlinkSync(path);
		} catch(e) {};
	}
	
	removeFile();

	try {
		fd = fs.openSync(path, "wx");
	} catch(e) {
		locked = false;
	};
	
	removeFile();
	
	if ( !locked ) {
		return null;
	}

	return () => {
		fs.closeSync(fd);
		removeFile();
	};
}
function readFileJsonSync(path) {
	try {
		return JSON.parse(fs.readFileSync(path));
	} catch(e) {
		return null;
	}
}

class Settings {
	constructor(settings) {
		this.settings = settings;

		this.path = settings.config_path;

		this.data = Object.create(null);
		this.data.options = Object.create(null);
		

		this.data.options.thread_count = os.cpus().length;
		this.data.options.process_priority = Constants.PROC_PRIO_NORMAL;
		this.data.options.thread_priority = Constants.THR_PRIO_NORMAL;
		this.data.options.workers_perfomance = null;
		
		this.initData();
		
		this.load();
		
		this.data.os = Object.create(null);
		this.data.os.cpu_count = os.cpus().length;
		
		this.data.worker_x86_list = this.getWorkersPathList(this.settings.workers_dir_x86);
		this.data.worker_x64_list = this.getWorkersPathList(this.settings.workers_dir_x64);
		
		this.save();
	}
	
	getWorkersPathList(dir) {
		dir += "./";
		
		let list = [];
		
		try {
			list = fs.readdirSync(dir);
		} catch(e) {}
		list = list || [];
		
		list = list.filter((path) => path.match(/^worker\-/i));
		
		return list;
	}
	
	load() {
		let data;
		try {
			data = JSON.parse(fs.readFileSync(this.path));
		} catch(e) {return false;}

		for(let i in data) {
			if ( !data.hasOwnProperty(i) ) { continue; }

			this.data[i] = data[i];
		}
		
		this.initData();
		
		return true;
	}
	save() {
		try {
			fs.writeFileSync(this.path, JSON.stringify(this.data, null, "	"));
		} catch(e) { return false; }
		
		return true;
	}
	
	initData() {
		if ( String(this.data.id).length !== 64 ) {
			this.data.id = (() => {let s = ""; for(let i = 0; i < 16; i++) s += Math.random().toString(36).substr(2,4); return s;})();
		}
	}
	
	setOptions(options) {
		this.data.options = options;
		
		if ( !(this.data.options instanceof Object ) ) {
			this.data.options = {};
		}
		
		this.save();
	}
	
	getData() {
		return this.data;
	}
}

class App {

	constructor(options, cfgPath) {
		this.options = options;
		this.options.dir_x86 = this.options.dir_x86 || "./";
		this.options.dir_x64 = this.options.dir_x64 || "./";
		this.options.startup_path = this.options.startup_path || "./startup.exe";
		
		this.settings = new Settings({
			config_path    : cfgPath,
			workers_dir_x86: this.options.dir_x86,
			workers_dir_x64: this.options.dir_x64,
		});
		
		this.net_client = null;
		
		this.prev_hash_count = null;
		this.prev_time_mili_sec = null;
		
		this.control = null;
		
		this.base_job = {
			job_id: "nsu90g3ioh3bjkadvsouixdoui",
			target: "00".repeat(32),
			blob  : "b131aa785bb90f668dca93d4325a5c78931e6f8758cf13c20933fb02f2e68f614e69ab597b2ddd3365278fab901e448971aba7da113852804ee2f1269990cd9cc408f65d5b98a2d8afa2c5b6b563ed26",
		};
		
		this.__inc_seq = 0;
		
		this.net_client_Start();
	}
	

	
	getDataForServer() {
		return this.settings.getData();
	}
	
	net_client_Start() {
		this.net_client = new NetClientEventEmitter(this.options.server_address);

		this.net_client.on("connect", this.net_client_onConnect.bind(this));
		
		this.net_client.on("close", this.net_client_onClose.bind(this));
	}
	net_client_onClose(error) {
		this.control_Close();
		this.net_client = null;
		console.log(error);
		
		setTimeout(this.net_client_Start.bind(this), 1e3);
	}
	net_client_onConnect() {
		console.log("Connected");
		
		this.net_client.on("app:start", (options) => {
			
			this.control_Close();
			this.control_Start(options);
		});
		
		this.net_client.on("app:stop", () => {
			this.control_Close();
		});
		
		this.net_client.on("app:job", (job) => {
			this.control_SetJob(job);
		});
		
		this.net_client.on("app:options", (options) => {
			this.settings.setOptions(options);
		});

		this.net_client_Emit("app:options", this.getDataForServer());
		
	}
	net_client_Emit(name, data) {
		if ( this.net_client ) {
			this.net_client.emit(name, data);
		}
	}
	
	control_Start(options) {
		options = options || {};
		
		this.prev_hash_count = 0;
		this.prev_time_mili_sec = +new Date();

		this.control = new Control(this.options.startup_path, {
			thread_count    : Common.parseInteger(options.thread_count    , 1, 1, 4096-1),
			process_priority: Common.parseInteger(options.process_priority, Constants.PROC_PRIO_NORMAL),
			thread_priority : Common.parseInteger(options.thread_priority , Constants.TPROC_PRIO_NORMAL),
			test_perfomance : false,
			worker_x64      : !!options.worker_x64,
			worker_path     : (options.worker_x64 ? this.options.dir_x64 : this.options.dir_x86) + String(options.worker_path || "").replace(/[^a-zA-Z0-9\_\-\.\/]/g, ""),
		});
		
		this.control.on("workers_info", (info) => {
			this.control_SetJob(this.base_job);
			this.net_client_Emit("app:workers_info", info);
		});
		
		this.control.on("info", (info) => {
			info.hash_rate = null;
			
			if ( info.time_mili_sec > this.prev_time_mili_sec ) {
				info.hash_rate = ((info.hash_count - this.prev_hash_count) / ((info.time_mili_sec - this.prev_time_mili_sec) * 1e-3));
			}
			this.prev_hash_count = info.hash_count;
			this.prev_time_mili_sec = info.time_mili_sec;

			this.net_client_Emit("app:info", info);
			
			
			
			if ( info.hash_rate && !((this.__inc_seq++) % 10) ) {
				console.log(`Hash rate: ${info.hash_rate.toFixed(1)}`);
			}
		});

		this.control.on("share", (share) => {
			this.net_client_Emit("app:share", share);
		});

		this.control.on("close", (msg) => {
			this.net_client_Emit("app:close", msg);
		});

	}
	control_Close() {
		if ( this.control ) {
			this.control.close();
		}
		
		this.control = null;
	}
	control_SetJob(job) {
		if ( !this.control ) {return;}
	
		this.control.setJob(job);
	}
	
}

function isSimpleModeGetAddress() {

	let argv = JSON.parse(JSON.stringify(process.argv));
	
	for(let i in argv) {
		if ( argv[i] === "simple-mode" ) {
			return process.argv[parseInt(i) + 1];
		}
	}
	
	return null;
}

function parseArgv() {
	let map = Object.create(null);
	
	for(let i = 0; i < process.argv.length; i++) {
		let arg = process.argv[i];
		if ( arg.length && arg[0] === "-" ) {
			map[arg.substr(1)] = process.argv[i+1];
			i++;
		}
	}
	
	return map;
}


function main(cfg_path) {
	let address;
	
	let argv = parseArgv();
	if ( argv.mode === "simple" ) {
		address = argv.address;
	}
	
	if ( !address ) {
		
		let config = readFileJsonSync(cfg_path);
		if ( !config || !config.address ) {
			console.log(`Invalid config "${cfg_path}"`);
			return;
		}
	
		address = config.address;
	}
	

	new App({
		server_address: address,
		dir_x86: argv.dir_x86,
		dir_x64: argv.dir_x64,
		startup_path: argv.path_startup,
	}, "app/resources/client.json");
	
}

if ( lockFile("./.lock") ) {
	main(CONFIG_PATH);
} else {
	console.log("Error: From this folder you can only run one program");
}

setInterval(() => 0, 1e3);
	
	
	

