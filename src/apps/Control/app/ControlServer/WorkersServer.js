
const EventEmitter = require("events").EventEmitter;

const Logger = require("./Logger");
const Common = require("./Common");
const NetServerEventEmitter = require('./../Share/Net/NetServerEventEmitter');
const HashRate = require("./HashRate");

class WorkersServer {
	constructor(options, events, logger) {
		this.prefix = "workers:server:";
		this.events = events;
		this.options = options;
		this.id = Common.getGlobalUniqueId();
		this.logger = new Logger(logger, "WORKERS-SERVER #" + this.id);

		this.workers = [];
		
		this.net = new NetServerEventEmitter(options);
		this.logger.notice(`Attempting opened server on ${this.logInfoServer(Logger.LOG_COLOR_MAGENTA_LIGHT, Logger.LOG_COLOR_GRAY)}`);
		
		this.setEvents();
	}
	
	setEvents() {
		this.net.on("connection", (cl) => {
			//this.workers.push(
				new WorkersServerClient(this.options, this.events, cl, this.logger);
			//);
		});
		
		this.net.on("listening", (addr) => {
			this.logger.success(`Opened server on ${this.logInfoServerHostPort(addr.address, addr.port, Logger.LOG_COLOR_MAGENTA, Logger.LOG_COLOR_GREEN)}`);
			this.events.emit(this.prefix + "listening", this, addr);
		});
		
		this.net.on("close", (msg) => {
			this.events.emit(this.prefix + "close", this, msg);
		});
	}
	
	logInfoServer(color, prevColor) {
		return `[${color}SSL ${this.options.ssl?"ON":"OFF"}${prevColor}] "${color}${this.options.bind_address}${prevColor}"`;
	}
	logInfoServerHostPort(host, port, color, prevColor) {
		return `[${color}SSL ${this.options.ssl?"ON":"OFF"}${prevColor}] "${color}${host}:${port}${prevColor}"`;
	}
}

class WorkersServerClient {
	constructor(options, events, net, logger) {
		this.prefix = "workers:server:worker:";
		this.events = events;
		this.options = options;
		this.id = Common.getGlobalUniqueId();
		this.logger = new Logger(logger, "WORKER #" + this.id);
		
		/****/
		this.worker_options = null;
		/****/
		
		this.net = net;

		this.address = this.net.net.socket.address().address + ":" + this.net.net.socket.address().port;
		this.name = "";
		this.workers_info = null;
		this.hash_count = 0;
		this.share_count = 0;
		this.hash_rate = 0;
		this.job_count = 0;
		
		//this.hashRate = new HashRate.HashRate();
		//this.hashRateLast = new HashRate.HashRateLast(10);
		
		
		
		this.prev_hash_count = 0;
		this.hash_count_delta = 0;
		this.share_count_delta = 0;
		
		this.logger.notice(`Accepted worker`);
		
		this.events.emit(this.prefix + "connect", this);
			
		this.net.on("close", (msg) => {
			this.logger.notice("Disconnected... (msg: " + msg + ")");
			this.events.emit(this.prefix + "disconnect", this, msg);
			this.events.emit(this.prefix + "close", this, msg);
		});
		
		this.setEvents();
	}
	
	setEvents() {
		this.net.on("app:options", (options) => {
			this.worker_options = options;
			
			if ( (typeof this.worker_options !== "object") || !String(this.worker_options.id).length ) {
				this.close("Worker send invalid data");
				return;
			}
			
			this.events.emit(this.prefix + "app:options", this, options);
		});
		
		this.net.on("app:workers_info", (workers_info) => {
			this.workers_info = workers_info;

			this.events.emit(this.prefix + "app:workers_info", this, workers_info);
		});
		
		this.net.on("app:info", (info) => {
			if ( info.hash_count <= this.prev_hash_count ) {
				this.hash_count_delta = this.hash_count;
				this.share_count_delta = this.share_count;
			}
			this.prev_hash_count = info.hash_count;
			
			this.hash_count = this.hash_count_delta + info.hash_count;
			this.share_count = this.share_count_delta + info.share_count;

			this.hash_rate = info.hash_rate;
			
			this.events.emit(this.prefix + "app:info", this, info);
		});
		
		this.net.on("app:share", (share) => {
			share.worker_id = this.id;
			this.events.emit(this.prefix + "app:share", this, share);
		});
		
		this.net.on("app:close", (msg) => {
			this.hash_rate = 0;
			this.events.emit(this.prefix + "app:close", this, msg);			
		});
	}
	
	emit(name, ...data) {
		this.net.emit(name, ...data);
	}
	
	setJob(job) {
		this.job_count++;
		this.emit("app:job", job);
	}

	appStart(options) {
		this.emit("app:start", options);
	}
	appStop() {
		this.emit("app:stop");
	}
	appOptions(options) {
		this.emit("app:options", options);
	}
	
	close(msg) {
		this.net.close(msg);
	}
}

module.exports = WorkersServer;