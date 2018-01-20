
const Common = require("../Common");
const PackArray = require("../PackArray");
const WebStatBase = require("./WebStatBase");

class Shares extends WebStatBase {
	constructor(events) {
		super(events);

		this.sharesArchive = new PackArray(1024*1024);

		events.on("web:server:connect_web_socket", (socket) => {
			this.webEmit("sharesArchive", this.sharesArchive.getData(), socket);
		});

		events.on("stratum:client:accepted_share", (origPool, origShare, msg) => {
			this.processShare(origPool, origShare, true, msg);
		});
		events.on("stratum:client:rejected_share", (origPool, origShare, msg) => {
			this.processShare(origPool, origShare, false, msg);
		});
	}

	processShare(origPool, origShare, isAccepted, msg) {
		let share = {
			id        : origShare.id,
			pool_id   : origShare.pool_id,
			worker_id : origShare.worker_id,
			job_id    : origShare.job_id,
			
			share     : {
				job_id    : origShare.share.job_id,
				nonce     : origShare.share.nonce,
				hash      : origShare.share.hash,
			},
			
			block_height: origShare.block_height,
			
			difficulty: origShare.difficulty_pool,
			difficulty_real: origShare.difficulty_real,
			
			status    : isAccepted ? "accepted" : "rejected",
			status_msg: msg,
			
			time_in_work: origShare.time_in_work,
			time_start  : origShare.time_start,
			time_end    : origShare.time_end,
			
			time: origShare.time,
		};
		
		this.sharesArchive.write("share", share);
		
		this.webEmit("shares", [share]);
	}
}

module.exports = Shares;
