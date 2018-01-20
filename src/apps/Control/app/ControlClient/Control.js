
const fs = require('fs');
const EventEmitter = require('events').EventEmitter;
const spawn = require('child_process').spawn;

const _ = require('c-struct');
const dgram = require('dgram');
 


class UdpLocalServer extends EventEmitter {
	constructor() {
		super();
		
		this.prefix = "";
		this.closed = true;
		this.clients = {};

		this.server = dgram.createSocket('udp4');
		
		this.server.on("error", (err) => {
			this.server.close();
		});
		
		this.server.on("close", (err) => {
			this.closed = true;
			this.emit(this.prefix+"close");
		});
		
		this.server.on("message", (msg, rinfo) => {
			this.clients[rinfo.port] = rinfo;

			this.emit(this.prefix+"message", msg, rinfo);
		});
		
		this.server.on('listening', () => {
			this.closed = false;
			this.emit(this.prefix+"listening", (this.server.address()));
		});
		
		this.server.bind(0, "127.0.0.1");
	}
	
	send(msg) {
		if ( !this.closed ) {
			for(let port in this.clients) {
				this.server.send(msg, port, "127.0.0.1");
			}
		}
	}
	
	close() {
		if ( !this.closed ) {
			this.server.close();
		}
	}
}
class CmdStruct {
	constructor() {
		this.cStruct = require('c-struct');
		this.cmds = Object.create(null);
	
		this.idForName = 1;
		this.prefixForName = Math.random().toString(36).substr(2);
	}
	
	register(cmd, size, _struct) {
		let struct = {
			cmd: _.type.uint32
		};
		
		for(let i in _struct) {
			struct[i] = _struct[i];
		}
		
		let name = "_" + this.prefixForName + "_" +(this.idForName++);

		let sch = this.cStruct.Schema;
		this.cStruct.register(name, new sch(struct));
		
		this.cmds[cmd] = {
			name: name,
			cmd : cmd,
			size: size + 4,
		};
		
	}
	
	unpack(cmd, buffer) {
		let obj = this.cmds[cmd];
		if ( !obj ) { return null; }
		
		if ( obj.size > buffer.length ) {
			return null;
		}
		
		return _.unpackSync(obj.name, buffer);
	}
	pack(cmd, data) {
		let obj = this.cmds[cmd];
		if ( !obj ) { return null; }
		
		data.cmd = cmd;
		return _.packSync(obj.name, data);
	}

	auto_unpack(buffer) {
		if ( buffer.length < 4 ) {
			return null;
		}
		
		return this.unpack(buffer.readInt32LE(0), buffer);
	}
}
const CMDSTRUCT = new CmdStruct();

const SV_CMD_STARTUP_INFO	= (1);
const SV_CMD_JOB 			= (2);
const SV_CMD_PING 			= (3);

const CL_CMD_SHARE			= (81);
const CL_CMD_INFO			= (82);
const CL_CMD_PONG 			= (84);
const CL_CMD_HELLO			= (85);
const CL_CMD_ERROR			= (86);
const CL_CMD_WORKER_INFO	= (87);

CMDSTRUCT.register(SV_CMD_PING, 4, {
	ping: {
		id: _.type.uint32
	}
});
CMDSTRUCT.register(SV_CMD_STARTUP_INFO, 4*4+500, {
	startup_info: {
		thread_count    : _.type.uint32,
		process_priority: _.type.uint32,
		thread_priority : _.type.uint32,
		test_perfomance : _.type.uint32,
		
		worker_x64      : _.type.uint32,
		worker_path		: _.type.string(500),
	}
});
CMDSTRUCT.register(SV_CMD_JOB, 4+4+4*8, {
	job: {
		job_id: _.type.uint32,
		hash  : (() => {let a = []; for(let i = 0; i < 80; i++) a.push(_.type.uint8); return a;})(),
		target: (() => {let a = []; for(let i = 0; i < 32; i++) a.push(_.type.uint8); return a;})(),
	}
});

CMDSTRUCT.register(CL_CMD_HELLO, 0, {});
CMDSTRUCT.register(CL_CMD_PONG, 4, {
	pong: {
		id: _.type.uint32,
	}
});
CMDSTRUCT.register(CL_CMD_WORKER_INFO, 6*4, {
	worker_info: {
		process_priority       : _.type.uint32,
		thread_priority        : _.type.uint32,
		processor_package_index: _.type.uint32,
		processor_core_index   : _.type.uint32,
		processor_logical_index: _.type.uint32,
		numa_node_index        : _.type.uint32,
		large_page             : _.type.uint32,
	}
});
CMDSTRUCT.register(CL_CMD_INFO, 8+8+4, {
	info: {
		hash_count              : { u16_0: _.type.uint16, u16_1: _.type.uint16, u16_2: _.type.uint16, u16_3: _.type.uint16, },
		share_count             : { u16_0: _.type.uint16, u16_1: _.type.uint16, u16_2: _.type.uint16, u16_3: _.type.uint16, },
		min_delta_micro_sec_hash: _.type.uint32,
		time_mili_sec           : { u16_0: _.type.uint16, u16_1: _.type.uint16, u16_2: _.type.uint16, u16_3: _.type.uint16, },
	}
});
CMDSTRUCT.register(CL_CMD_SHARE, 4+4+4*8, {
	share: {
		job_id: _.type.uint32,
		hash  : (() => {let a = {}; for(let i = 0; i < 32; i++) a[i] = _.type.uint8; return a;})(),
		nonce : _.type.uint32,
	}
});

class Control extends EventEmitter {
	constructor(programPath, startupInfo) {		
		super();
	
		this.programPath = programPath;
		this.program = null;
		
		this.startupInfo = startupInfo;
		
		this.udpLocalServer = null;
		this.udpLocalServerPort = null;
		
		this.jobSeq = 1;
		this.jobs = Object.create(null);
		
		this.ping_id = 1;
		this.pongs = Object.create(null);
		this.iid_ping = null;
		
		this.workers_info = [];
		
		this.workers_ready = false;
		
		this.closed = false;
		
		this.udpLocalServer_Start();
	}
	
	createProgram(port) {
		console.log("Start: " + this.startupInfo.worker_path);
		
		this.program = spawn(this.programPath, [port]);
		
		this.program.stdout.on('data', (data) => {
			//console.log(data.toString())
		});
		this.program.stderr.on('data', (data) => {
			//console.log(data.toString())
		});
		this.program.on('error', (data) => {
			//console.log(data)
			//this.close();
		});
		this.program.on('close', (data) => {
			//console.log("?close")
			this.close();
		});
	}
	
	udpLocalServer_SetEvents() {
		this.udpLocalServer.on("listening", (addr) => {
			this.udpLocalServerPort = addr;
			this.createProgram(addr.port);
		});
		
		
		this.udpLocalServer.on("message", (buf) => {
			this.parseClMessage(buf);
		});
		
		this.udpLocalServer.on("close", this.close.bind(this));
	}
	udpLocalServer_Start() {
		this.udpLocalServer = new UdpLocalServer(this.events);
		this.udpLocalServer_SetEvents();
	}
	udpLocalServer_Send(buf) {
		this.udpLocalServer.send(buf, buf.length);
	}
	
	
	clientConnect() {
		this.emit("connect");
	
		this.iid_ping = setInterval(() => {
			this.sv_ping();
		}, 1e3);
		
		this.sv_startup_info();
	}
	
	sv_ping() {
		this.udpLocalServer_Send(CMDSTRUCT.pack(SV_CMD_PING, {
			ping: {
				id: this.ping_id
			}
		}));

		this.pongs[this.ping_id] = +new Date();
		this.ping_id++;
	}
	sv_startup_info() {
		this.udpLocalServer_Send(CMDSTRUCT.pack(SV_CMD_STARTUP_INFO, {
			startup_info: {
				thread_count    : this.startupInfo.thread_count,
				process_priority: this.startupInfo.process_priority,
				thread_priority : this.startupInfo.thread_priority,
				test_perfomance : this.startupInfo.test_perfomance,
				worker_x64      : this.startupInfo.worker_x64,
				worker_path		: this.startupInfo.worker_path,
			}
		}));
	}
	sv_job() {
		let jobLastIndex = Object.keys(this.jobs).sort((l,r) => l-r).pop()
		
		if ( !this.workers_ready || !jobLastIndex ) {
			return;
		}
		
		let job = this.jobs[jobLastIndex];
		
		let buf = new Buffer(4+4+32+80);
		
		buf.writeInt32LE(SV_CMD_JOB, 0);
		buf.writeInt32LE(job.job_seq, 4);
		
		(new Buffer(job.target_hex, "hex")).copy(buf, 4+4);
		(new Buffer(job.blob_hex  , "hex")).copy(buf, 4+4+32);
		//console.log(buf.toString('hex'));
		this.udpLocalServer_Send(buf);
	}
	
	cl_hello(cl_data) {
		this.clientConnect();
	}
	cl_pong(cl_data) {
		if ( !this.pongs[cl_data.pong.id] ) {
			this.close("Client send bad id for ping");
		}
			
		let deltaMsec = +new Date() - this.pongs[cl_data.pong.id];
			
		//console.log("Ping: " + deltaMsec)
	}
	cl_worker_info(cl_data) {
		this.workers_info.push({
			process_priority       : cl_data.worker_info.process_priority,
			thread_priority        : cl_data.worker_info.thread_priority,
			processor_package_index: cl_data.worker_info.processor_package_index,
			processor_core_index   : cl_data.worker_info.processor_core_index,
			processor_logical_index: cl_data.worker_info.processor_logical_index,
			numa_node_index        : cl_data.worker_info.numa_node_index,
			large_page             : cl_data.worker_info.large_page,
		});
		
		if ( this.workers_info.length === this.startupInfo.thread_count ) {
			this.workers_ready = true;
			
			this.emit("workers_info", this.workers_info);
			
			this.sv_job();
		}
	}
	cl_info(cl_data) {
		let u16_64ToDouble = (n) => {
			return n.u16_0 + 
				n.u16_1 * (65536.0) +
				n.u16_2 * (65536.0 * 65536.0) +
				n.u16_3 * (65536.0 * 65536.0 * 65536.0);
		};
		
		let hash_count = u16_64ToDouble(cl_data.info.hash_count);
		let share_count = u16_64ToDouble(cl_data.info.share_count);
		let time_mili_sec = u16_64ToDouble(cl_data.info.time_mili_sec);
		
		this.emit("info", {
			hash_count: hash_count,
			share_count: share_count,
			min_delta_micro_sec_hash: cl_data.info.min_delta_micro_sec_hash,
			time_mili_sec: time_mili_sec,
		});
	}
	cl_share(cl_data, buf) {
		let buf_hex = buf.toString("hex");
		
		let hash_hex = buf_hex.substr((4+4)*2, 32*2);
		let nonce_hex = buf_hex.substr((4+4+32)*2, 4*2);
		
		let job = this.jobs[cl_data.share.job_id];
		if ( !job ) {
			return;
		}
		
		this.emit("share", {
			job_id: job.job_id,
			nonce : nonce_hex,
			hash  : hash_hex
		});
	}
	
	_setJob(job_id, target_hex, blob_hex) {
		if ( target_hex.length != 32*2 || target_hex.match(/[^a-fA-F0-9]/) ) { return; }
		if ( blob_hex.length < 80*2    || blob_hex.match(/[^a-fA-F0-9]/) ) { return; }
		blob_hex = blob_hex.substr(0, 80*2);
		
		let job = this.jobs[ this.jobSeq ] = {
			job_seq   : this.jobSeq,
			job_id    : job_id,
			target_hex: target_hex,
			blob_hex  : blob_hex,
		};
		this.jobSeq++;
		//console.log("New job: job_id: " + job.job_id + " job_seq: " + job.job_seq)
		this.sv_job();
		
		while(1) {
			let jobsKeys = Object.keys(this.jobs);
			if ( jobsKeys.length < 10 ) {
				break;
			}
			
			let jobIndex = jobsKeys.sort((l,r) => r-l).pop();
			if ( !jobIndex ) {
				break;
			}
			
			delete this.jobs[jobIndex];
		}
		
		//console.log(this.jobs);
	}
	setJob(job) {
		if ( !(typeof job === "object") ) { return; }
		if ( !String(job.job_id).length ) { return; }
		if ( !String(job.target).length ) { return; }
		if ( !String(job.blob).length ) { return; }
		
		this._setJob(job.job_id, job.target, job.blob);
	}
	
	parseClMessage(buf) {
		let map = {};
		map[CL_CMD_HELLO      ] = this.cl_hello.bind(this);
		map[CL_CMD_PONG       ] = this.cl_pong.bind(this);
		map[CL_CMD_WORKER_INFO] = this.cl_worker_info.bind(this);
		map[CL_CMD_INFO       ] = this.cl_info.bind(this);
		map[CL_CMD_SHARE      ] = this.cl_share.bind(this);
		
		let cl_data = CMDSTRUCT.auto_unpack(buf);
		if ( !cl_data || !map[cl_data.cmd] ) {
			this.close("Client send invalid command");
			return;
		}
		
		(map[cl_data.cmd])(cl_data, buf);
	}
	
	close(msg) {
		this.udpLocalServer.close();
		
		if ( this.program ) {
			this.program.kill();
		}
		
		if ( this.iid_ping !== null ) {
			clearInterval(this.iid_ping);
			this.iid_ping = null;
		}
		
		if ( !this.closed ) {
			this.emit("close", msg);
		}
		
		this.closed = true;
	}
}

module.exports = Control;
