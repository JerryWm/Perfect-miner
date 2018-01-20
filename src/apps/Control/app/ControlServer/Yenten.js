const crypto = require('crypto');

const Common = require("./Common");

function sha256d(data) {
	return crypto.createHash('sha256').update(
		crypto.createHash('sha256').update(data).digest()
	);
}

class Yenten {

}

Yenten.getBlockHeight = (ctx) => {
	let height = 0;
	
	for(let i = 32; i < 32 + 128 - 1; i++) {
		if ( ctx.coinbase_blob[i] === 0xFF && ctx.coinbase_blob[i + 1] === 0xFF ) {
			for(; ctx.coinbase_blob[i] === 0xFF; i++);
			i++;
			let len = ctx.coinbase_blob[i++];

			for(let j = 0; j < len; j++) {
				height |= ctx.coinbase_blob[i++] << (j << 3);
			}
			
			break;
		}
	}
	
	return height;
}

Yenten.sha256d_gen_merkle_root = function(ctx) {
	let tmp = new Buffer(64);
	
	let merkle_root = Common.sha256d(ctx.coinbase_blob);
	
	for(let merkle of ctx.merkle_branch_blob) {
		merkle_root.copy(tmp, 0, 0, 32);
		merkle.copy(tmp, 32, 0, 32);
		
		merkle_root = Common.sha256d(tmp);
	}
	
	return merkle_root;
}

Yenten.parseJob = (ctx) => {
	ctx.coinbase_size = ctx.coinb1_size + ctx.extranonce1_size + ctx.extranonce2_size + ctx.coinb2_size;
	
	ctx.coinbase = ctx.coinb1 + ctx.extranonce1 + ("00".repeat(ctx.extranonce2_size)) + ctx.coinb2;
		
	ctx.coinbase_blob = new Buffer(ctx.coinbase, "hex");
	
	ctx.block_height = Yenten.getBlockHeight(ctx);
}

Yenten.incExtraNonce = (arr) => {
	let len = arr.length;

	for(let i = 0; i < len; i++) {
		arr[i]++;
		if ( arr[i] ) {
			break;
		}
	}
}

Yenten.diffToTarget = (diff) => {
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

	
module.exports = Yenten;

