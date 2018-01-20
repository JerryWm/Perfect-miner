
const EventEmitter = require("events").EventEmitter;

const Logger = require("./Logger");
const Common = require("./Common");
const HashRate = require("./HashRate");
const StratumConfig = require("./StratumConfig");
const StratumCommon = require("./StratumCommon");

const spawn = require('child_process').spawn;



class SetIntervalMng {
	constructor(timeInterval) {
		this.list = [];
		this.iid = setInterval(this.frame.bind(this), timeInterval);
	}
	
	frame() {
		for(let cb of this.list) {
			cb();
		}
	}
	
	on(cb) {
		this.list.push(cb);
	}
	close() {
		clearInterval(this.iid);
	}
}


/**
{
	pool_address: "",
	pool.pool_password: "",
	wallet_address: "",
	keepalive: 60,
	response_timeout: 20,
}
*/

var BLOCK_OFFSETS = {
	VERSION    : 0,
	PREVHASH   : 0+4,
	MERKLE_ROOT: 0+4+32,
	NTIME      : 0+4+32+32,
	NBITS      : 0+4+32+32+4,
	NONCE      : 0+4+32+32+4+4,
};

class Job {
	constructor() {
		this.id = Common.getGlobalUniqueId();
		////////
		this.difficulty_factor = 65536.0;
		////////
	
		this.job_id = null;
		
		this.difficulty = 1.0;
		this.difficulty_real = this.getRealDifficulty();
		
		this.extranonce1_blob = null;
		this.extranonce2_blob = null;
		this.nonce_blob = new Buffer([0,0,0,0]);
		
		this.prevhash_blob = null;

		this.merkle_branch_blob = null;

		this.coinb1_blob = null;
		this.coinb2_blob = null;

		this.coinbase_blob = null;
		
		this.version_blob = null;
		this.nbits_blob = null;
		this.ntime_blob = null;
		this.clean_jobs = null;
		
		this.block_height = null;
		
		this.target_blob = null;
		
		this.block_header_blob = null;
		
		this.offset_ntime = BLOCK_OFFSETS.NTIME;
		this.offset_nbits = BLOCK_OFFSETS.NBITS;
	}
	
	updateExtranonce(extranonce1, extranonce2_size) {
		this.extranonce1_blob = new Buffer(extranonce1, "hex");
	
		this.extranonce2_blob = new Buffer(extranonce2_size);
		this.extranonce2_blob.fill(0);
	}

	updateJob(job_id, difficulty, prevhash, coinb1, coinb2, merkle_branch, version, nbits, ntime, clean_jobs) {
		this.prevhash_blob = new Buffer(prevhash, "hex");
		this.coinb1_blob   = new Buffer(coinb1  , "hex");
		this.coinb2_blob   = new Buffer(coinb2  , "hex");
		this.version_blob  = new Buffer(version , "hex");
		this.nbits_blob    = new Buffer(nbits   , "hex");
		this.ntime_blob    = new Buffer(ntime   , "hex");
		this.clean_jobs    = clean_jobs;
		
		this.merkle_branch_blob = [];
		for(let merkle of merkle_branch) {
			this.merkle_branch_blob.push(new Buffer(merkle.toLowerCase(), "hex"));
		}
		
		if ( job_id !== this.job_id ) {
			this.extranonce2_blob.fill(0);
			this.nonce_blob.fill(0);
			this.id = Common.getGlobalUniqueId();
		}

		this.difficulty = difficulty;
		this.difficulty_real = this.getRealDifficulty();
		
		this.target_blob = this.diffToTarget(difficulty, this.difficulty_factor);
		
		this.job_id = job_id;
		
		this.makeCoinbase();
		
		this.block_height = this.getBlockHeight()
	}
	
	incExtranonce2() {
		for(let i = 0; i < this.extranonce2_blob.length; i++) {
			this.extranonce2_blob[i]++;
			if ( this.extranonce2_blob[i] ) {
				break;
			}
		}
	}
	writeExtranonce2(extranonce2_blob) {
		extranonce2_blob.copy(this.coinbase_blob, this.coinb1_blob.length + this.extranonce1_blob.length);
	}
	
	makeCoinbase() {
		this.coinbase_blob = new Buffer(this.coinb1_blob.length + this.extranonce1_blob.length + this.extranonce2_blob.length + this.coinb2_blob.length);
		
		let ofs = 0;
		this.coinb1_blob.copy(this.coinbase_blob, ofs); ofs += this.coinb1_blob.length;

		this.extranonce1_blob.copy(this.coinbase_blob, ofs); ofs += this.extranonce1_blob.length;
		this.extranonce2_blob.copy(this.coinbase_blob, ofs); ofs += this.extranonce2_blob.length;

		this.coinb2_blob.copy(this.coinbase_blob, ofs); ofs += this.coinb2_blob.length;


		this.block_height = this.getBlockHeight(this);
	}
	
	getBlockHeader(extranonce2_blob, ntime_blob, nonce_blob) {
		let blob = new Buffer(32*4);
		blob.fill(0);
		
		this.writeExtranonce2(extranonce2_blob);
		let merkle_root_blob = this.sha256d_gen_merkle_root();
		
		this.version_blob .copy(blob, BLOCK_OFFSETS.VERSION);
		
		this.prevhash_blob.copy(blob, BLOCK_OFFSETS.PREVHASH);
		
		for(let i = 0; i < 8; i++) { blob.writeInt32BE(merkle_root_blob.readInt32LE(i*4), BLOCK_OFFSETS.MERKLE_ROOT + i*4); }
		
		ntime_blob.copy(blob, this.offset_ntime);
		
		this.nbits_blob.copy(blob, this.offset_nbits);
		
		blob.writeInt32LE(0x80000000|0, 20*4);
		blob.writeInt32LE(0x00000280|0, 31*4);
		
		nonce_blob.copy(blob, BLOCK_OFFSETS.NONCE);
   
		return blob;
	}
	
	getBlockHeaderForWorker() {
		if ( this.job_id === null ) {
			return null;
		}
		
		let ret = {
			id               : this.id,
			job_id           : this.job_id,
			block_header_blob: null,
			target_blob      : new Buffer(this.target_blob),
			extranonce1_blob : new Buffer(this.extranonce1_blob),
			extranonce2_blob : new Buffer(this.extranonce2_blob),
			nonce_blob       : new Buffer(this.nonce_blob),
			ntime_blob       : new Buffer(this.ntime_blob),
			difficulty       : this.difficulty,
			difficulty_real  : this.difficulty_real,
			block_height     : this.block_height,
		};
		
		if ( !this.nonce_blob[3] ) {
			this.block_header_blob = this.getBlockHeader(this.extranonce2_blob, this.ntime_blob, this.nonce_blob);
		}
		
		this.nonce_blob.copy(this.block_header_blob, BLOCK_OFFSETS.NONCE);
		//console.log(this.nonce_blob.toString('hex'))
		
		if ( this.nonce_blob[3] === 0xFF ) {
			this.incExtranonce2();
			this.nonce_blob[3] = 0;
		} else {
			this.nonce_blob[3]++;
		}
		
		ret.block_header_blob = new Buffer(this.block_header_blob);
		
		return ret;
	}
	
	getRealDifficulty() {
		return (this.difficulty / this.difficulty_factor) * 4294967296.0;
	}
	
	getBlockHeight() {
		let height = 0;
		
		for(let i = 32; i < 32 + 128 - 1; i++) {
			if ( this.coinbase_blob[i] === 0xFF && this.coinbase_blob[i + 1] === 0xFF ) {
				for(; this.coinbase_blob[i] === 0xFF; i++);
				i++;
				let len = this.coinbase_blob[i++];

				for(let j = 0; j < len; j++) {
					height |= this.coinbase_blob[i++] << (j << 3);
				}
				
				break;
			}
		}
		
		return height;
	}


	sha256d_gen_merkle_root() {
		let tmp = new Buffer(64);
	
		let merkle_root = Common.sha256d(this.coinbase_blob);
	
		for(let merkle of this.merkle_branch_blob) {
			merkle_root.copy(tmp, 0, 0, 32);
			merkle.copy(tmp, 32, 0, 32);
		
			merkle_root = Common.sha256d(tmp);
		}
	
		return merkle_root;
	}
	
	diffToTarget(diff, fr = 1.0) {
		diff /= fr;
		
		let target = new Buffer(32);
		for(let i = 0; i < 32; i++) { target[i] = 0xFF; }

		let k;
		for(k = 6; k > 0 && diff > 1.0; k--) {
			diff /= 4294967296.0;
		}
		
		let m = (4294901760.0 / diff);
		
		
		if ( m == 0 && k == 6 ) {
			return target;
		}
		
		for(let i = 0; i < 32; i++) { target[i] = 0x00; }
		target.writeInt32LE(0|(m % 4294967296.0), k*4);
		target.writeInt32LE(0|(m / 4294967296.0), (k+1)*4);
		
		//console.log("k: " + k+ "  diff: " + diff.toFixed(4));
		
		return target;
	}	
	
	
	getStat() {
		return  {
			id             : this.id,
			job_id         : this.job_id,
			difficulty     : this.difficulty,
			difficulty_real: this.getRealDifficulty(),
			block_height   : this.getBlockHeight(),
		};
	}
}

class JobWorker {
	constructor(job_id, block_header_blob, target_blob) {
		this.job_id = job_id;
		this.block_header_blob = block_header_blob;
		this.target_blob = target_blob;
	}
	
	toObject() {
		return {
			job_id: this.job_id,
			blob  : this.block_header_blob.toString("hex"),
			target: this.target_blob.toString("hex"),
		};
	}
	
	getHex() {
		return this.toObject();
	}
}

class JobMng {
	constructor() {
		this.job = new Job();

		this.job_workers = Object.create(null);
	}
	
	regJob(job_id, data) {
		let worker_job_id = Common.randHex(8);

		this.job_workers[worker_job_id] = data;
		
		return worker_job_id;
	}
	cleanJobs() {
		this.job_workers = Object.create(null);
	}
	
	getJobForWorker() {
		let data = this.job.getBlockHeaderForWorker();
		if ( !data ) {
			return data;
		}
		
		let worker_job_id = this.regJob(data.job_id, data);
		
		return new JobWorker(worker_job_id, data.block_header_blob, data.target_blob);
	}
	
	updateJob(job_id, difficulty, prevhash, coinb1, coinb2, merkle_branch, version, nbits, ntime, clean_jobs) {
		let isUpdate = false;
		
		if ( job_id !== this.job.job_id ) {
			this.cleanJobs();
			isUpdate = true;
		}
		
		this.job.updateJob(job_id, difficulty, prevhash, coinb1, coinb2, merkle_branch, version, nbits, ntime, clean_jobs);
		
		return isUpdate;
	}
	updateExtranonce(extranonce1, extranonce2_size) {
		this.job.updateExtranonce(extranonce1, extranonce2_size);
	}
	
	getStat() {
		return this.job.getStat();
	}
	
	getDataForWorker(worker_job_id) {
		if ( !this.job_workers[worker_job_id] ) {
			return null;
		}
		
		return this.job_workers[worker_job_id];
	}	

}

class StratumClient {	
	/**
	{
		EventEmitter {
			stratum:client:open
			stratum:client:close
			stratum:client:connect
			stratum:client:disconnect
			stratum:client:ping
			stratum:client:accepted_job
			stratum:client:accepted_share
			stratum:client:rejected_share
		}
	}
	*/
	constructor(options, events, logger) {		
		this.prefix = "stratum:client:";
		this.events = events;
		
		this.id = Common.getGlobalUniqueId();
		
		this.logger = new Logger(logger, "STRATUM-CLIENT #" + this.id);

		this.events.emit(this.prefix + "open", this);
		
		this.pool = new StratumConfig(this.logger, options);
		if ( !this.pool.valid ) {
			this.events.emit(this.prefix + "close", this);
			return;
		}
		
		this.pool_id = null;
		
		this.ping = null;

		this.incoming = new StratumCommon.Recv();
		
		this.socket = null;
		
		this.USER_AGENT = "JerryPROXY~CRYPTONIGHT";
		
		this.difficulty = null;
		
		this.accepted_job_count = 0;
		this.accepted_share_count = 0;
		this.rejected_share_count = 0;
		this.share_count = 0;
		this.hash_count = 0;
		
		this.job = null;
		
		
		this.disconnected = true;
		this.closed = false;
		
		this.target = null;
		
		//*********
		this.ctx = {};
		this.ctx.sid = null;
		this.ctx.difficulty = null;
		this.ctx.extranonce = null;
		this.ctx.extranonce_size = null;
		this.ctx.job_id = null;
		this.ctx.prevhash = null;
		this.ctx.merkle_branch = null;
		this.ctx.merkle_branch_blob = null;
		
		this.ctx.job = new Job();
		this.jobMng = new JobMng();
		//*********
		
		
		this.lastShareUpdateTime = Common.currTimeMiliSec();
		
		this.hashRate = new HashRate.HashRate();
		this.hashRateLast = new HashRate.HashRateLast();
		
		this.sendSeq = 1;
		this.expectedResult = Object.create(null);
		
		this.connect();

		this.keepaliveSIM = new SetIntervalMng(this.pool.keepalive);
		if ( !this.pool.keepalive ) { this.keepaliveSIM.close(); }
		this.expectedResultSIM = new SetIntervalMng(1e3);

		this.expectedResultSIM.on(() => {
			let ctms = Common.currTimeMiliSec();
			for(let i in this.expectedResult) {
				let obj = this.expectedResult[i];
				
				if ( obj.startTime + obj.timeoutMiliSec < ctms ) {
					this.logger.error("Poole did not send the result. Timeout error");
					this.disconnect("Poole did not send the result. Timeout error");
					return;
				}
			}
		});
	}
	
	logPoolInfo(colorSelect, colorNormal) {
		return '['+colorSelect+(this.pool.ssl?"SSL ON":"SSL OFF")+colorNormal+'] ' + 
			'[' + colorSelect + (this.pool.keepalive?"KPALV "+(this.pool.keepalive*1e-3):"KPALV OFF") +colorNormal+'] ' +
			'[' + colorSelect + ("RSP TO "+(this.pool.response_timeout*1e-3)) +colorNormal+ '] ' +
			'"'+colorSelect+ this.pool.host+':'+this.pool.port +colorNormal+'" ' ;
	}

	connect() {
		this.logger.notice("Attempting to connect to "+this.logPoolInfo(Logger.LOG_COLOR_MAGENTA_LIGHT, Logger.LOG_COLOR_GRAY));

		if ( !this.pool.ssl ) {
			this.socket = require('net').createConnection({
				host: this.pool.host,
				port: this.pool.port
			});
		} else {
			process.env.NODE_TLS_REJECT_UNAUTHORIZED = "0";
			this.socket = require('tls').connect({
				host: this.pool.host,
				port: this.pool.port,
				requestCert: false,
				rejectUnauthorized: false
			});
		}
		
		this.setEvents();
	}
	
	disconnect(msg) {
		if ( this.socket ) {
			this.socket.end();
			this.socket = null;
		}
		
		this.keepaliveSIM.close();
		this.expectedResultSIM.close();
		
		this.events.emit(this.prefix + "disconnect", this, msg || "");
		this.events.emit(this.prefix + "close", this, msg || "");
		
		this.disconnected = true;
		this.closed = true;
	}
	
	setEvents() {
		this.socket.on('connect', () => {			
			this.logger.success('Connected to ' + this.logPoolInfo(Logger.LOG_COLOR_MAGENTA, Logger.LOG_COLOR_GREEN));
			
			this.disconnected = false;

			this.events.emit(this.prefix + "connect", this);
			
			this.onConnect();			
		});
		
		this.socket.on('data', (data) => {
			if ( !this.incoming.recv(data, this.recvFrameObject.bind(this)) ) {
				this.logger.error("Pool send bad json");
				this.disconnect("Pool send bad json");
			}
		});
		
		this.socket.on('end', () => {
			this.logger.error('The server disconnected the connection');
			this.disconnect('The server disconnected the connection');
		});
		
		this.socket.on('error', (e) => {
			this.logger.error('An error has occurred ' + (e.code ? e.code : ""));
			this.disconnect('An error has occurred ' + (e.code ? e.code : ""));
		});
		
		this.socket.on('timeout', () => {
			this.logger.error('Timeout error');
			this.disconnect('Timeout error');
		});
	}
	
	onConnect() {
		this.sendMethod("mining.subscribe", [], (result) => {
			let errPrefix = "[mining.subscribe] ";
			if ( !(result instanceof Array && result.length === 3) ) {
				return this.logErrorDisconnect(errPrefix+"Pool send invalid result");
			}
			
			let arr = result[0];
			if ( arr instanceof Array ) {
				for(let val of arr) {
					if ( val instanceof Array && val.length === 2 && typeof val[0] === "string" ) {
						switch(val[0]) {
							case "mining.set_difficulty":
								break;
							case "mining.notify":
								this.ctx.sid = val[1];
								break;
						}
					}
				}
			}
				
			if ( this.mining_set_extranonce(result, 1, errPrefix) ) {
				this.sendMethod("mining.authorize", [this.pool.wallet_address, this.pool.pool_password], (result) => {
					if ( !result ) {
						this.logErrorDisconnect("Authentication on the pool is unsuccessful");
					}
				});
			}
		});
	}
	
	sendLine(line) {
		//this.logger.notice("send... : " + line);
		if ( this.socket ) {
			this.socket.write( line + '\n' );
		}
	}
	sendObj(obj, cbResult, errorDisconnect = true) {
		obj.id = this.sendSeq++;
		
		this.expectedResult[ obj.id ] = {
			startTime      : Common.currTimeMiliSec(),
			timeoutMiliSec : this.pool.response_timeout || DEF_POOL_RESPONSE_TIMEOUT,
			cbResult       : cbResult,
			errorDisconnect: errorDisconnect
		};
		
		this.sendLine( JSON.stringify(obj) );
	}
	sendMethod(method, params, cbResult, errorDisconnect = true) {
		params = params || [];
		
		this.sendObj({
			method: method,
			params: params
		}, cbResult, errorDisconnect);
	}

	
	doPing() {
		if ( !this.pool.keepalive ) {
			return;
		}

		var dlt = Common.timeDeltaMiliSec();
		
		this.send_Keepalived({}, () => {
			this.ping = dlt();
			this.events.emit(this.prefix + "ping", this, this.ping);
			//this.logger.success("Ping "+Logger.LOG_COLOR_MAGENTA+this.ping+Logger.LOG_COLOR_GREEN+" msec");
		});
	}

	
	/// COMMANDS
	mining_set_difficulty(params) {
		if ( !Common.checkArray(params, 0) ) {
			return this.logErrorDisconnect(errPrefix+"Pool send invalid difficulty");
		}
		
		let diff = parseFloat(params[0]);
		if ( !isFinite(diff) ) {
			return this.logErrorDisconnect(errPrefix+"Pool send invalid difficulty");
		}

		this.ctx.difficulty = diff;
	}
	mining_set_extranonce(params, id = 0, errPrefix = "") {
		if ( !Common.checkArray(params, id + 1) ) {
			return this.logErrorDisconnect(errPrefix+"Pool send invalid extranonce1,2");
		}

		this.ctx.extranonce1 = params[id];
		this.ctx.extranonce1_size = this.ctx.extranonce1.length >> 1;
		if ( !Common.checkHex(this.ctx.extranonce1) || this.ctx.extranonce1_size > 16 ) {
			return this.logErrorDisconnect(errPrefix+"Pool send invalid extranonce1");
		}

		this.ctx.extranonce2_size = Common.parseInteger(params[id + 1], -1);
		if ( this.ctx.extranonce2_size < 2 || this.ctx.extranonce2_size > 16 ) {
			return this.logErrorDisconnect(errPrefix+"Pool send invalid extranonce2_size");
		}
		
		this.jobMng.updateExtranonce(this.ctx.extranonce1, this.ctx.extranonce2_size);
		
		return true;
	}
	mining_notify(result) {
		let errPrefix = "[mining.authorize] ";
		
		if ( !(result instanceof Array && result.length === 9) ) {
			return this.logErrorDisconnect(errPrefix+"Pool send invalid result");
		}
		
		if ( !Common.checkString(result[0]) ) { return this.logErrorDisconnect(errPrefix+"Pool send invalid job_id"); }
		let job_id = result[0];
			
		if ( !Common.checkHex(result[1], 32, 32) ) { return this.logErrorDisconnect(errPrefix+"Pool send invalid prevhash"); }
		let prevhash = result[1].toLowerCase();
			
		if ( !Common.checkHex(result[2]) ) { return this.logErrorDisconnect(errPrefix+"Pool send invalid coinb1"); }
		let coinb1 = result[2].toLowerCase();
			
		if ( !Common.checkHex(result[3]) ) { return this.logErrorDisconnect(errPrefix+"Pool send invalid coinb2"); }
		let coinb2 = result[3].toLowerCase();
		
		let merkle_branch = [];
		if ( !(result[4] instanceof Array) ) { return this.logErrorDisconnect(errPrefix+"Pool send invalid merkle_branch"); }
		for(let merkle of result[4]) {
			if ( !Common.checkHex(merkle, 32, 32) ) { return this.logErrorDisconnect(errPrefix+"Pool send invalid merkle_branch"); }
			
			merkle_branch.push(merkle.toLowerCase());
		}
			
		if ( !Common.checkHex(result[5], 4, 4) ) { return this.logErrorDisconnect(errPrefix+"Pool send invalid 'Bitcoin block version'"); }
		let version = result[5].toLowerCase();
			
		if ( !Common.checkHex(result[6], 4, 4) ) { return this.logErrorDisconnect(errPrefix+"Pool send invalid 'Encoded current network difficulty'"); }
		let nbits = result[6].toLowerCase();
		
		if ( !Common.checkHex(result[7], 4, 4) ) { return this.logErrorDisconnect(errPrefix+"Pool send invalid 'Current ntime'"); }
		let ntime = result[7].toLowerCase();
		
		let clean_jobs = !!result[8];
		
		if ( this.jobMng.updateJob(job_id, this.ctx.difficulty, prevhash, coinb1, coinb2, merkle_branch, version, nbits, ntime, clean_jobs) ) {
			this.events.emit(this.prefix+"accepted_job", this, this.jobMng.getStat());
			this.accepted_job_count++;
		} else {
			this.events.emit(this.prefix+"update_job", this, this.jobMng.getStat());
		}
	}

	getJob() {
		let job = this.jobMng.getJobForWorker();
		if ( !job ) {
			return null;
		}
		
		return job.getHex();
	}
	submitShare(share) {
		if ( !Common.checkHex(share.nonce, 4, 4) ) {
			this.logger.warning("Worker send bad nonce");
			return;
		}
		
		let data = this.jobMng.getDataForWorker(share.job_id);
		if ( data ) {
			share.pool_id = this.id;
			share.difficulty = data.difficulty;
			share.difficulty_real = data.difficulty_real;
			share.time = Common.currTimeMiliSec();
			
			this.share_count++;
			
			this.sendMethod("mining.submit", [
				this.pool.wallet_address,
					
				data.job_id,
				data.extranonce2_blob.toString('hex'),
				data.ntime_blob.toString('hex'),
				share.nonce
			], (result, error) => {
				if ( result ) {
					this.accepted_share_count++;
					this.hash_count += data.difficulty_real;
					
					this.logger.success("Accepted share");
					this.events.emit(this.prefix+"accepted_share", this, share);
				} else {
					this.rejected_share_count++;
					
					this.events.emit(this.prefix+"rejected_share", this, share, error);
				}
			}, false);	
		} else {
			this.logger.warning("Worker send bad share(not found job for share)");
		}
	}
	
	recvFrameObject(obj) {
		//this.logger.notice("recv...: " + JSON.stringify(obj))
		
		if ( typeof obj !== 'object' ) {
			this.logger.error('Poole sent invalid raw json');
			this.disconnect('Poole sent invalid raw json');
			return;
		}
		
		if ( (typeof obj.id === 'string' || typeof obj.id === 'number') && ( parseInt(obj.id).toString() === obj.id.toString() ) ) {
			let expectedObj = this.expectedResult[obj.id];
			delete this.expectedResult[obj.id];
			
			if ( !expectedObj ) {
				return this.logErrorDisconnect('Pool sent a obj, result is not expected');
			}
			
			let isErr = false;
			let err = "";
			if ( obj.error ) {
				isErr = true;
				if ( obj.error instanceof Array && obj.error.length >= 2 ) {
					err = `#${obj.error[0]} ${obj.error[1]}`;
				}
			}
			
			if ( isErr ) {
				this.logErrorDisconnect("Pool sent a error: " + err, expectedObj.errorDisconnect);
			}

			expectedObj.cbResult && expectedObj.cbResult(obj.result, err, obj.error);
			return;
		}
		
		if ( obj.method ) {
			switch(obj.method) {
				case "mining.set_difficulty":
					this.mining_set_difficulty(obj.params);
					break;
				
				case "mining.set_extranonce":
					this.mining_set_extranonce(obj.params, 0, "[mining.set_extranonce] ");
					break;
					
				case "mining.notify":
					this.mining_notify(obj.params);
					break;
				
				default:
					this.logger.warning('Pool sent a not support method "'+(obj.method));
					break;
			}
		}
		
	}
	
	logErrorDisconnect(err, disconnect = true) {
		this.logger.error(err);
		if ( disconnect ) {
			this.disconnect(err);
		}
		return false;
	}

	jobIdToLog(id) {
		const MAXLEN = 32;
		
		if ( id.length <= MAXLEN ) {
			return id;
		}
		
		return id.slice(0, MAXLEN>>1) + "..." + id.slice(-(MAXLEN>>1));
	}
	
}

module.exports = StratumClient;