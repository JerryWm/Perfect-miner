
const crypto = require('crypto');


class Common {
}
Common.revers2b = function(s) {
	let _s = "";
	for(let i = 0; i < s.length; i+=2) {
		_s = s.substr(i, 2) + _s;
	}
	return _s;
}
Common.parseIntByHex = function(hex) {
	return parseInt(Common.revers2b(hex), 16)|0;
}
Common.hexToUint32 = function(h) {
	return parseInt(Common.revers2b(h), 16)|0;	
}
Common.uint32ToHex = function(n) {
	let b0 = ((n >> (8*0)) & 0xFF).toString(16)
	let b1 = ((n >> (8*1)) & 0xFF).toString(16)
	let b2 = ((n >> (8*2)) & 0xFF).toString(16)
	let b3 = ((n >> (8*3)) & 0xFF).toString(16)
	
	if ( b0.length == 1 ) { b0 = "0"+b0; }
	if ( b1.length == 1 ) { b1 = "0"+b1; }
	if ( b2.length == 1 ) { b2 = "0"+b2; }
	if ( b3.length == 1 ) { b3 = "0"+b3; }
	
	return b0 + b1 + b2 + b3;
}
Common.strToHashSimplie = function(s) {
	let r = "";
	for(let i = 0; i < s.length; i++) {
		r += "_" + s.charCodeAt(i).toString(16);
	}
	return r;
}
Common.addressEx = function(address) {
	let m;
	if ( typeof address !== 'string' || !(m = address.match(/([^\\/]*?)\s*\:\s*(\d+)/)) ) {
		return null;
	}

	return [m[1], m[2]];
}
Common.randHex = function(len) {
	let s = "";
	while(len--)
		s += Math.random().toString(16).substr(4, 2);
	return s;
}
Common.getId = function(key) {
	var id_list = Object.create(null);
	
	Common.getId = function(key) {
		if ( id_list[key] === undefined ) {
			id_list[key] = 1;
		}
		
		return id_list[key]++;
	}
	
	return Common.getId(key);
}
Common.getGlobalUniqueId = function() {
	var id = 1;
	
	Common.getGlobalUniqueId = function() {
		return id++;
	}
	
	return Common.getGlobalUniqueId();
}
Common.objToArray = function(obj) {
	let arr = [];
	for(let i in obj) {
		arr.push(obj[i]);
	}
	return arr;
}
Common.setInterval = (key, timeout, cb) => {
	var kv_data = Object.create(null);

	Common.setInterval = (key, timeout, cb) => {
		timeout = parseInt(timeout);

		if ( kv_data[key] ) {
			kv_data[key].timeout = timeout;
			kv_data[key].cb = cb;
			return true;
		}
		
		kv_data[key] = {
			timeout: timeout,
			cb: cb
		};

		function tmi() {
			let data = kv_data[key];
			
			if ( !data.cb || isNaN(data.timeout) || !data.timeout ) {
				delete kv_data[key];
				return;
			}
			
			kv_data[key].cb();
			
			setTimeout(tmi, data.timeout);
		}
		tmi();
		
		return true;
	};
	
	Common.setInterval(key, timeout, cb);
}
Common.currTimeMiliSec = () => {
	return (new Date()).getTime();
}
Common.currTimeSec = () => {
	return (new Date()).getTime() * 1e-3;
}
Common.timeDeltaMiliSec = () => {
	var timeStart = Common.currTimeMiliSec();
	return () => {
		return Common.currTimeMiliSec() - timeStart;
	};
}
Common.extNonceJobBlob = (blob) => {
	return blob.substr(39*2, 8);
}

Common.parseIntegerFilter = (n, options) => {
	if ( n === null || n === undefined || isNaN(Number(n)) ) {
		n = options.def;
	} else {
		n = parseInt( Math.round(n) );
		
		if ( String(n) !== String(n|0) ) {
			n = options.def;
		}
	}

	n |= 0;

	if ( options.min === undefined ) { return n; }
	n = Math.max(n, options.min);
	
	if ( options.max === undefined ) { return n; }
	n = Math.min(n, options.max);
	
	return n;
}
Common.parseInteger = (n, def, min, max) => {
	return Common.parseIntegerFilter(n, {
		def: def,
		min: min,
		max: max,
	});
}

Common.checkHex = function(hex, min_len, max_len) {
	if ( !Common.checkString(hex, min_len, max_len) ) { return false; }
	if ( hex.length & 1 ) { return false; }
	if ( hex.match(/[^0-9a-fA-F]/) ) { return false; }
	
	return true;
}
Common.checkString = function(str, min_len, max_len) {
	if ( typeof str !== "string" ) { return false; }
	if ( min_len !== undefined && str.length < min_len * 2 ) { return false; }
	if ( max_len !== undefined && str.length > max_len * 2 ) { return false; }
	
	return true;
}
Common.checkArray = function(arr, index) {
	if ( !( arr instanceof Array ) ) { return false; }
	if ( index !== undefined && index >= arr.length ) { return false; }
	
	return true;
}

Common.sha256d = (data) => {
	return crypto.createHash('sha256').update(
		crypto.createHash('sha256').update(data).digest()
	).digest();
}



module.exports = Common;