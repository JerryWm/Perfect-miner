
const Common = require("../Common");
const PackArray = require("../PackArray");
const WebStatBase = require("./WebStatBase");

class Workers extends WebStatBase {
	constructor(events) {
		super(events);
		
		this.events = events;
	
		this.workers = {};
		this.workersArchive = new PackArray(1024*1024);
		
		this.workersUpdateMini_v2 = [];
		
		events.on("web:server:connect_web_socket", (socket) => {
			this._workerUpdate();

			this.webEmit("workersArchive", this.workersArchive.getData(), socket);
			this.webEmit("workers", Common.objToArray(this.workers), socket);
		});		
		
		events.on("workers:server:worker:connect"      , this.workerConnect      .bind(this));
		events.on("workers:server:worker:disconnect"   , this.workerDisconnect   .bind(this));
		events.on("workers:server:worker:accepted_job" , this.workerAcceptedJob  .bind(this));
		events.on("workers:server:worker:virtual_share", this.workerVirtualShare .bind(this));
		events.on("workers:server:worker:app:options"  , this.workerOptions      .bind(this));
		events.on("workers:server:worker:app:info"     , this.workerInfo         .bind(this));
		events.on("workers:server:worker:app:close"    , this.workerAppClose     .bind(this));
		
		events.on("stratum:client:accepted_share"      , this.workerAcceptedShare.bind(this));
		events.on("stratum:client:rejected_share"      , this.workerRejectedShare.bind(this));
		events.on("stratum:proxy:pool_add_worker"      , this.workerPoolAddWorker.bind(this));
		events.on("stratum:proxy:pool_del_worker"      , this.workerPoolDelWorker.bind(this));
		
		///	TODO
		events.on("control:workers:server:worker:app:options", (origWorker, options) => {
			this._workerOptions(origWorker, options);
		});
	
		setInterval(() => {
			this.webEmit("workers_info_mini_v2", this.workersUpdateMini_v2);
			this.workersUpdateMini_v2 = [];
			
			let summary_hash_rate = 0;
			for(let i in this.workers) {
				let hash_rate = this.workers[i].hash_rate;
				if ( hash_rate !== null && hash_rate !== undefined ) {
					summary_hash_rate += hash_rate;
				}
			}
			
			this.events.emit("workers:summary_hash_rate", summary_hash_rate);
			this.webEmit("workers_summary_hash_rate", summary_hash_rate);
		}, 1e3);
	}
	
	_workerUpdate(id) {
		if ( !id ) {
			for(let i in this.workers) {
				this._workerUpdate(i);
			}
			return;
		}
		
		let worker = this.workers[id];
		if ( !worker ) { return; }
		
		if ( worker.alive ) {
			worker.time_in_work = Common.currTimeMiliSec() - worker.connection_time;
		}
	}
	_workerUpdateMini(id, info) {
		let data = [
			id, 
			info.difficulty,
			info.job_count,
			info.accepted_share_count,
			info.rejected_share_count,
			info.hash_count,
			info.share_count,
			info.hashrate,
			info.pool_id,
			info.hash_rate,
			info.name
		];
		//console.log("Update mini worker", data)
		this.webEmit("worker_info_mini", data);		
	}
	workerConnect(origWorker) {
		let worker = this.workers[origWorker.id] = {
			id  : origWorker.id,
			
			agent         : origWorker.agent,
			address       : origWorker.address,
			pool_password : origWorker.pool_password,
			wallet_address: origWorker.wallet_address,
			
			name: "",
			
			pool_id: 0,
			
			time_in_work: 0,
			connection_time: Common.currTimeMiliSec(),
			disconnection_time: null,
			
			difficulty: null,
			
			worker_count: 0,
			job_count  : 0,
			accepted_share_count: 0,
			rejected_share_count: 0,
			
			hash_count: 0,
			share_count: 0,

			hashrate: {},
			
			hash_rate: origWorker.hash_rate,
			
			disconnection_error: "",
			
			ping: null,
			
			alive: true,
		};
		
		this.webEmit("workers", [worker]);
	}
	workerDisconnect(origWorker, msg) {
		let worker = this.workers[origWorker.id]; if ( !worker ) { return; }
		
		worker.disconnection_time = Common.currTimeMiliSec();
		worker.time_in_work = worker.disconnection_time - worker.connection_time;
		worker.disconnection_error = msg || "";
		worker.alive = false;

		this.webEmit("workers", [worker]);
		
		this.workersArchive.write("worker", worker);
		
		delete this.workers[worker.id];
	}
	workerAcceptedJob(origWorker, job) {
		let worker = this.workers[origWorker.id]; if ( !worker ) { return; }
		
		worker.job_count++;
		worker.difficulty = origWorker.difficulty;
		
		this._workerUpdateMini(worker.id, {
			job_count: worker.job_count, 
			difficulty: worker.difficulty
		});
	}
	workerVirtualShare(origWorker, share) {
		let worker = this.workers[origWorker.id]; if ( !worker ) { return; }
		 
		worker.hash_count = origWorker.hashes;
		worker.share_count = origWorker.shares;
		worker.difficulty = origWorker.difficulty;
		
		let hashrate = origWorker.hashRate.getHashRate([5, 10, 15, 30, 60, "all"]);
		hashrate["current"] = origWorker.hashRateLast.getHashRate();
		worker.hashrate = hashrate;
		
		this._workerUpdateMini(worker.id, {
			hash_count: worker.hash_count,
			share_count: worker.share_count,
			hashrate: hashrate,
		});
	}
	workerOptions(origWorker, options) {
		if ( "options" in options && "name" in options.options ) {
			this._workerOptions(origWorker, options.options);
		}
	}
	_workerOptions(origWorker, options) {
		let worker = this.workers[origWorker.id]; if ( !worker ) { return; }
		
		if ( "name" in options ) {
			worker.name = options.name;
		}
		
		this._workerUpdateMini(worker.id, {
			name: worker.name,
		});
	}
	workerInfo(origWorker, info) {
		let worker = this.workers[origWorker.id]; if ( !worker ) { return; }
		
		worker.hash_count = origWorker.hash_count;
		worker.share_count = origWorker.share_count;
		worker.hash_rate = origWorker.hash_rate;
		worker.job_count = origWorker.job_count;
		worker.hash_rate = worker.hash_rate;
		
		this.workersUpdateMini_v2.push(worker.id, worker.hash_count, worker.share_count, worker.hash_rate);
		/*
		this._workerUpdateMini(worker.id, {
			hash_count: worker.hash_count,
			share_count: worker.share_count,
			hash_rate: worker.hash_rate,
		});
		*/
	}
	workerAppClose(origWorker) {
		let worker = this.workers[origWorker.id]; if ( !worker ) { return; }
		
		worker.hash_rate = 0;
	}
	workerAcceptedShare(origPool, share) {
		if ( !share.worker_id ) { return; }
		let worker = this.workers[share.worker_id]; if ( !worker ) { return; }
		
		worker.accepted_share_count++;
		
		this._workerUpdateMini(worker.id, {
			accepted_share_count: worker.accepted_share_count,
		});
	}
	workerRejectedShare(origPool, share) {
		if ( !share.worker_id ) { return; }
		let worker = this.workers[share.worker_id]; if ( !worker ) { return; }
		
		worker.rejected_share_count++;
		
		this._workerUpdateMini(worker.id, {
			rejected_share_count: worker.rejected_share_count,
		});
	}
	workerPoolAddWorker(origPool, origWorker) {
		let worker = this.workers[origWorker.id]; if ( !worker ) { return; }
		
		worker.pool_id = origPool.id;
		
		this._workerUpdateMini(worker.id, {pool_id: worker.pool_id});
	}
	workerPoolDelWorker(origPool, origWorker) {
		let worker = this.workers[origWorker.id]; if ( !worker ) { return; }
		
		worker.pool_id = 0;
		
		this._workerUpdateMini(worker.id, {pool_id: worker.pool_id});
	}

}

module.exports = Workers;
