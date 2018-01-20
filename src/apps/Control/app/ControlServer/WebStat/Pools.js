
const Common = require("../Common");
const PackArray = require("../PackArray");
const WebStatBase = require("./WebStatBase");

class Pools extends WebStatBase {
	constructor(events) {
		super(events);
		
		this.pools = {};
		this.poolsArchive = new PackArray(1024*1024);
	
		events.on("web:server:connect_web_socket", (socket) => {
			this._poolUpdate();

			this.webEmit("poolsArchive", this.poolsArchive.getData(), socket);
			this.webEmit("pools", Common.objToArray(this.pools), socket);
		});

		events.on("stratum:client:connect"       , this.poolConnect      .bind(this));
		events.on("stratum:client:disconnect"    , this.poolDisconnect   .bind(this));
		events.on("stratum:client:ping"          , this.poolPing         .bind(this));
		events.on("stratum:client:accepted_share", this.poolAcceptedOrRejectedShare.bind(this));
		events.on("stratum:client:rejected_share", this.poolAcceptedOrRejectedShare.bind(this));
		events.on("stratum:client:accepted_job"  , this.poolAcceptedJob  .bind(this));
		events.on("stratum:proxy:pool_add_worker", this.poolAddWorker    .bind(this));
		events.on("stratum:proxy:pool_del_worker", this.poolDelWorker    .bind(this));
	}

	_poolUpdate(id) {
		if ( !id ) {
			for(let i in this.pools) {
				this._poolUpdate(i);
			}
			return;
		}
		
		let pool = this.pools[id];
		if ( !pool ) { return; }
		
		if ( pool.alive ) {
			pool.time_in_work = Common.currTimeMiliSec() - pool.connection_time;
		}
	}
	_poolUpdateMini(id, info) {
		
		let data = [
			id, 
			info.difficulty,
			info.worker_count,
			info.job_count,
			info.accepted_share_count,
			info.rejected_share_count,
			info.hash_count,
			info.share_count,
			info.ping,
			info.hashrate,
			info.difficulty_real,
		];
		
		this.webEmit("pool_info_mini", data);
	}
	_poolUpdateHashRate(origPool) {
		let pool = this.pools[origPool.id]; if ( !pool ) { return; }
		
		let hashrate = origPool.hashRate.getHashRate([5, 10, 15, 30, 60, 60*2, 60*3, 60*6, 60*12, 60*24, "all"]);
		hashrate["current"] = origPool.hashRateLast.getHashRate();
		pool.hashrate = hashrate;
	}
	poolConnect(origPool) {
		
		let pool = this.pools[origPool.id] = {
			id  : origPool.id,

			pool_address  : origPool.pool.pool_address,
			pool_password : origPool.pool.pool_password,
			wallet_address: origPool.pool.wallet_address,
			
			time_in_work: 0,
			connection_time: Common.currTimeMiliSec(),
			disconnection_time: null,
			
			difficulty: origPool.difficulty_pool,
			difficulty_real: origPool.difficulty_real,
			
			worker_count: 0,
			job_count  : 0,
			accepted_share_count: 0,
			rejected_share_count: 0,
			
			hash_count: 0,
			share_count: 0,

			hashrate: {},
			
			disconnection_error: "",
			
			ping: origPool.ping,
			
			alive: true,
		};
		
		this._poolUpdateHashRate(origPool);

		this.webEmit("pools", [pool]);
	}
	poolDisconnect(origPool, msg) {
		let pool = this.pools[origPool.id]; if ( !pool ) { return; }
		
		pool.disconnection_time = Common.currTimeMiliSec();
		pool.time_in_work = pool.disconnection_time - pool.connection_time;
		pool.disconnection_error = msg;
		pool.alive = false;

		this.webEmit("pools", [pool]);
		
		this.poolsArchive.write("pool", pool);
		
		delete this.pools[pool.id];
	}
	poolPing(origPool, ping) {
		let pool = this.pools[origPool.id]; if ( !pool ) { return; }
		
		pool.ping = ping;
		
		this._poolUpdateMini(pool.id, {ping: ping});
	}
	poolAcceptedOrRejectedShare(origPool) {
		let pool = this.pools[origPool.id]; if ( !pool ) { return; }
		
		pool.accepted_share_count = origPool.accepted_share_count;
		pool.rejected_share_count = origPool.rejected_share_count;
		pool.share_count          = origPool.share_count;
		pool.hash_count           = origPool.hash_count;
		pool.ping                 = origPool.ping;
		
		this._poolUpdateHashRate(origPool);

		this._poolUpdateMini(pool.id, {
			accepted_share_count: pool.accepted_share_count,
			rejected_share_count: pool.rejected_share_count,
			share_count         : pool.share_count,
			hash_count          : pool.hash_count,
			ping                : pool.ping,
			
			hashrate            : pool.hashrate,
		});
	}
	poolAcceptedJob(origPool, jobStat) {
		let pool = this.pools[origPool.id]; if ( !pool ) { return; }
		
		pool.job_count = origPool.job_count;
		pool.difficulty = origPool.difficulty_pool;
		pool.difficulty_real = origPool.difficulty_real;
		
		this._poolUpdateHashRate(origPool);
		
		this._poolUpdateMini(pool.id, {
			job_count: pool.job_count,
			difficulty        : pool.difficulty,
			difficulty_real   : pool.difficulty_real,
			hashrate          : pool.hashrate,
		});
	}
	poolAddWorker(origPool, origWorker) {
		let pool = this.pools[origPool.id]; if ( !pool ) { return; }
		
		pool.worker_count++;
		
		this._poolUpdateMini(pool.id, {worker_count: pool.worker_count});
	}
	poolDelWorker(origPool, origWorker) {
		let pool = this.pools[origPool.id]; if ( !pool ) { return; }
		
		pool.worker_count--;
		
		this._poolUpdateMini(pool.id, {worker_count: pool.worker_count});
	}
	
}

module.exports = Pools;