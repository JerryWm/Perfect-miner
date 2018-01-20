
const EventEmitter = require("events").EventEmitter;

const Logger = require("./../Logger");
const Common = require("./../Common");
const HashRate = require("./../HashRate");
const StratumConfig = require("./../StratumConfig");
const StratumCommon = require("./../StratumCommon");

const NetCommon = require("./../../Share/Net/NetCommon");
const JsonRpcClient = require("./../../Share/JsonRpc/JsonRpcClient");

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

		this.difficulty = null;
		this.difficulty_pool = null;
		this.difficulty_real = null;

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

	updateJob(job_id, difficulty, difficulty_pool, difficulty_real, prevhash, coinb1, coinb2, merkle_branch, version, nbits, ntime, clean_jobs) {
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
		this.difficulty_pool = difficulty_pool;
		this.difficulty_real = difficulty_real;
		
		this.target_blob = this.diffToTarget(this.difficulty);
		
		this.job_id = job_id;
		
		this.makeCoinbase();
		
		this.block_height = this.getBlockHeight();
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
			difficulty_pool  : this.difficulty_pool,
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
			difficulty_pool: this.difficulty_pool,
			difficulty_real: this.difficulty_real,
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
	
	updateJob(job_id, difficulty, difficulty_pool, difficulty_real, prevhash, coinb1, coinb2, merkle_branch, version, nbits, ntime, clean_jobs) {
		let isUpdate = false;
		
		if ( job_id !== this.job.job_id ) {
			this.cleanJobs();
			isUpdate = true;
		}
		
		this.job.updateJob(job_id, difficulty, difficulty_pool, difficulty_real, prevhash, coinb1, coinb2, merkle_branch, version, nbits, ntime, clean_jobs);
		
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
		this.id = Common.getGlobalUniqueId();
		this.prefix = "stratum:client:";
		this.options = options;
		this.events = events;
		this.closed = false;

		this.logger = new Logger(logger, "STRATUM-CLIENT #" + this.id);

		this.events.emit(this.prefix + "open", this);

		this.pool_address_string = this.options.pool_address.replace(/^\s*stratum\+/, "");
		this.pool_address_info =  NetCommon.parseAddress(this.options.pool_address.replace(/^\s*stratum\+/, ""));
		if ( !this.pool_address_info ) {
			this.close(`Invalid address ${this.options.pool_address}`);
			return;
		}
		
		this.pool = new StratumConfig(this.logger, options);
		if ( !this.pool.valid ) {
			this.close(this.pool.error_text);
			return;
		}
		
		this.USER_AGENT = "JerryStratumClient";
		
		/*** STAT ***/
		this.job_count = 0;
		this.accepted_share_count = 0;
		this.rejected_share_count = 0;
		this.share_count = 0;
		this.hash_count = 0;
		
		this.hashRate = new HashRate.HashRate();
		this.hashRateLast = new HashRate.HashRateLast(10);
		
		this.ping = null;
		/*** STAT ***/
		
		this.job = null;
		
		
		//*********
		this.job_id = null;
		this.sid = null;
		this.difficulty = 1.0;
		this.difficulty_pool = 1.0;
		this.difficulty_real = this.diffToRealDiff(this.difficulty);
		this.extranonce1 = null;
		this.extranonce2_size = null;
		this.jobMng = new JobMng();
		//*********
		
		
		this.lastShareUpdateTime = Common.currTimeMiliSec();
		
		this.sendSeq = 1;
		this.expectedResult = Object.create(null);
		
		
		
		this.jsonRpcClient = new JsonRpcClient(this.pool_address_string);
		this.setEvents();
	}
	
	diffProcess(diff) {
		return diff / 65536.0;
	}
	diffToRealDiff(diff) {
		return diff * 65536.0;
	}
	
	logPoolInfo(colorSelect, colorNormal) {
		return '['+colorSelect+(this.pool_address_info.ssl?"SSL ON":"SSL OFF")+colorNormal+'] ' + 
			'[' + colorSelect + (this.options.keepalive?"KPALV "+(this.options.keepalive):"KPALV OFF") +colorNormal+'] ' +
			'"'+colorSelect+ this.pool_address_info.host+':'+this.pool_address_info.port +colorNormal+'" ' ;
	}
	
	disconnect(msg) {
		this.close(msg);
	}
	
	setEvents() {
		this.jsonRpcClient.on("open", () => {
			this.logger.notice("Attempting to connect to "+this.logPoolInfo(Logger.LOG_COLOR_MAGENTA_LIGHT, Logger.LOG_COLOR_GRAY));

			//this.events.emit(this.prefix + "open", this);
		});
		
		this.jsonRpcClient.on("connect", () => {
			this.logger.success('Connected to ' + this.logPoolInfo(Logger.LOG_COLOR_MAGENTA, Logger.LOG_COLOR_GREEN));
			
			this.events.emit(this.prefix + "connect", this);
			
			this.onConnect();	
		});
		
		this.jsonRpcClient.on("notify", (method, params) => {
			switch(method) {
				case "mining.set_difficulty":
					this.mining_set_difficulty(params);
					break;
				case "mining.set_extranonce":
					this.mining_set_extranonce(params);
					break;
				case "mining.notify":
					this.mining_notify(params);
					break;
				default:
					this.logErrorDisconnect("Pool send not support notify(method: "+method+")");
					break;
			}
		});
		
		
		this.jsonRpcClient.on("close", this.onCloseEmitAndLog.bind(this));
		
		this.jsonRpcClient.on("disconnect", (msg) => this.events.emit(this.prefix + "disconnect", this, msg));
	}
	
	onCloseEmitAndLog(msg) {
		if ( msg ) {
			this.logger.error("Pool close. Error: " + msg);
		} else {
			this.logger.notice("Pool close");
		}	
		
		this.events.emit(this.prefix + "close", this, msg);
	}
	
	onConnect() {
		this.sendMethod("mining.subscribe", [], (result, error) => {
			
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
								this.sid = val[1];
								break;
						}
					}
				}
			}
				
			if ( this.mining_set_extranonce(result, 1, errPrefix) ) {
				this.sendMethod("mining.authorize", [this.pool.wallet_address, this.pool.pool_password], (result, error, error_text) => {
					if ( !result ) {
						this.logErrorDisconnect("Authentication on the pool is unsuccessful. " + error_text);
					}
				});
				
				return;
			}
			
			
		});
	}
		
	sendMethod(method, params, cbResult, errorDisconnect = true) {
		params = params || [];
		
		this.jsonRpcClient.sendMethod(method, params, (result, error) => {
			this.ping = this.jsonRpcClient.ping;

			let error_text = "";
			if ( error && error instanceof Array && error.length >= 2 ) {
				error_text = `#${error[0]} ${error[1]}`;
			}
			
			if ( error && errorDisconnect ) {
				this.logErrorDisconnect("Pool send error: " + error_text);
				return;
			}

			if ( cbResult ) {
				cbResult(result, error, error_text);
			}
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

		this.difficulty_pool = diff;
		this.difficulty = this.diffProcess(diff);
		this.difficulty_real = this.diffToRealDiff(diff);
	}
	mining_set_extranonce(params, id = 0, errPrefix = "") {
		if ( !Common.checkArray(params, id + 1) ) {
			return this.logErrorDisconnect(errPrefix+"Pool send invalid extranonce1,2");
		}

		this.extranonce1 = params[id];
		let extranonce1_size = this.extranonce1.length >> 1;
		if ( !Common.checkHex(this.extranonce1) || extranonce1_size > 16 ) {
			return this.logErrorDisconnect(errPrefix+"Pool send invalid extranonce1");
		}

		this.extranonce2_size = Common.parseInteger(params[id + 1], -1);
		if ( this.extranonce2_size < 2 || this.extranonce2_size > 16 ) {
			return this.logErrorDisconnect(errPrefix+"Pool send invalid extranonce2_size");
		}
		
		this.jobMng.updateExtranonce(this.extranonce1, this.extranonce2_size);
		
		return true;
	}
	mining_notify(result) {
		let errPrefix = "[mining.authorize] ";
		
		if ( this.difficulty === null || this.difficulty_real === null ) { return this.logErrorDisconnect(errPrefix+"Pool no send difficulty"); }
		if ( this.extranonce1 === null || this.extranonce2_size === null ) { return this.logErrorDisconnect(errPrefix+"Pool no send extranonce1 and extranonce2_size"); }
		
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
		
		if ( this.jobMng.updateJob(job_id, this.difficulty, this.difficulty_pool, this.difficulty_real, prevhash, coinb1, coinb2, merkle_branch, version, nbits, ntime, clean_jobs) ) {
			this.events.emit(this.prefix+"accepted_job", this, this.jobMng.getStat());
			this.job_count++;
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
			let curr_time_mili_sec = Common.currTimeMiliSec();
		
			let share_clone = {
				id          : Common.getGlobalUniqueId(),
				pool_id     : this.id,
				worker_id   : share.worker_id,
				job_id      : data.id,
				
				block_height: data.block_height,
				
				difficulty     : data.difficulty,
				difficulty_pool: data.difficulty_pool,
				difficulty_real: data.difficulty_real,
			
				share: {
					job_id: data.job_id,
					nonce : share.nonce,
					hash  : share.hash,
				},
				
				time_start  : this.lastShareUpdateTime,
				time_end    : curr_time_mili_sec,
				time_in_work: curr_time_mili_sec - this.lastShareUpdateTime,
				time        : curr_time_mili_sec,
			};
			
			this.lastShareUpdateTime = curr_time_mili_sec;
			
			this.share_count++;
			
			this.sendMethod("mining.submit", [
				this.pool.wallet_address,
					
				data.job_id,
				data.extranonce2_blob.toString('hex'),
				data.ntime_blob.toString('hex'),
				share.nonce
			], (result, error, error_text) => {
				if ( result ) {
					this.accepted_share_count++;
					this.hash_count += data.difficulty_real;
					this.hashRate.addResultJob(this.hash_count);
					this.hashRateLast.addResultJob(this.hash_count);
					
					this.logger.success("Accepted share");
					this.events.emit(this.prefix+"accepted_share", this, share_clone);
				} else {
					this.rejected_share_count++;
					
					this.events.emit(this.prefix+"rejected_share", this, share_clone, error_text);
				}
			}, false);	
		} else {
			this.logger.warning("Worker send bad share(not found job for share)");
		}
	}

	logErrorDisconnect(err, disconnect = true) {
		if ( disconnect ) {
			this.close(err);
		} else {
			this.logger.error(err);
		}
		return false;
	}

	close(msg) {
		if ( !this.closed ) {
			if ( this.jsonRpcClient ) {
				this.jsonRpcClient.close(msg);
			} else {
				this.onCloseEmitAndLog(msg);
			}
		}
		
		this.closed = true;
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
