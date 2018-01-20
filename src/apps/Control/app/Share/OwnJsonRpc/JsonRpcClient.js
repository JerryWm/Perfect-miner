
const EventEmitter = require("events").EventEmitter;

const NetClient = require("./../../Share/Net/NetClient");

/**
events:
	open
	close
	connect
	disconnect

	notify

method:
	call
		--method
		--cb_result
		--...params
	notify
		--method
		--...params
	
	onCall
		--method
		--function
	onNitify
		--method
		--function

	close


*/
///TODO
class JsonRpcClient extends EventEmitter {	
	constructor(address) {		
		super();		
		
		this.id_seq = 1;

		this.expected_result_timeout_milisec = 20e3;
		this.expected_result_iid = null;
		this.expected_result_kv = Object.create(null);
		this.expectedResult_Create();
		
		this.ping = null;
		
		this.call_list = Object.create(null);
		this.notify_list = Object.create(null);
		
		this.net = new NetClient(address);
		
		this.setEvents();
	}
	
	setEvents() {
		this.net.on("open", () => this.emit("open"));
		this.net.on("close", (msg) => this.emit("close", msg));
		
		this.net.on("connect", () => this.emit("connect"));
		this.net.on("disconnect", (msg) => this.emit("disconnect", msg));
		
		this.net.on("data", this.onrecv.bind(this));
	}
	
	onrecv(data) {
		//console.log(data)
		if ( typeof data !== "object" ) {
			this.close('Pool sent invalid raw json');
			return;
		}

		if ( typeof data.type !== "string" ) {
			
		}
		
		switch(data.type) {
			case "call":
				
				break;
				
			case "notify":
				let notify = this.notify_list[data.method];
				if ( notify ) {
					for(cb of notify) {
						cb(data.params);
					}
				}
				break;
				
			case "answer":
				let id = data.id;
				
				break;
		}
		
		
		
		this.close('Pool sent invalid json-rpc data');
	}
	
	expectedResult_Create() {
		if ( this.expected_result_iid === null ) {
			this.expected_result_iid = setInterval(() => {
				
				let curr_time = +new Date();
				for(let id in this.expected_result_kv) {
					let expres = this.expected_result_kv[id];

					if ( expres.time + this.expected_result_timeout_milisec < curr_time ) {
						this.close("Pool did not send the result. Timeout error");
						return;
					}
				}
				
			}, 2e3);
		}
	}
	expectedResult_Close() {
		if ( this.expected_result_iid !== null ) {
			clearInterval(this.expected_result_iid);
			this.expected_result_iid = null;
		}
	}
	expectedResult_Reg(id, onresult) {
		this.expected_result_kv[id] = {
			time: +new Date(),
			onresult: onresult,
		}
	}
	expectedResult_GetCbAndDel(id) {
		let expres = this.expected_result_kv[id];
		if ( !expres ) {
			this.close("Pool sent a obj, result is not expected");
			return false;
		}
		
		this.ping = (+new Date()) - expres.time;
		
		delete this.expected_result_kv[id];
		
		return expres.onresult;
	}
	
	call(method, ...params) {
		let data = {
			type  : "call",
			method: method,
			params: params,
			id    : this.id_seq,
		};

		this.expectedResult_Reg(this.id_seq, (result) => {
			result(...params);
		});
		
		this.id_seq++;
		
		this.net.send(data);
	}
	notify(method, ...params) {
		let data = {
			type  : "notify",
			method: method,
			params: params,
		};
		
		this.net.send(data);
	}

	onCall(method, cb) {
		this.call_list[method] = cb;
	}
	onNotify(method, cb) {
		if ( this.notify_list[method] ) {
			this.notify_list[method] = [];
		}
		
		this.notify_list[method].push(cb);
	}
	
	close(msg) {
		this.expectedResult_Close();
		this.net.close(msg);
	}
	disconnect(msg) {
		this.close(msg);
	}

}

module.exports = JsonRpcClient;
 