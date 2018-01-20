const fs = require("fs");

var optionsCmpMaes = {
	__maes_msse4_2: "-maes -msse4.2",
	__maes_march_core2: "-maes -march=core2",
	__maes_march_corei7: "-maes -march=corei7",
	__maes_march_corei7_avx: "-maes -march=corei7-avx",
	__maes_march_core_avx2: "-maes -march=core-avx2",

	__maes_march_bdver1: "-maes -march=bdver1",
	__maes_march_bdver2: "-maes -march=bdver2",
	__maes_march_bdver3: "-maes -march=bdver3",
	__maes_march_btver1: "-maes -march=btver1",
};

var DISC_NAME = "C";

var headBatCode_x86 = `
set DISC_NAME=${DISC_NAME}
set CURRENT_DIR=%cd%
set path="%DISC_NAME%:/cygwin/bin"
cd "%DISC_NAME%:/cygwin/bin"		\r\n
`;

var headBatCode_x64 = `
set DISC_NAME=${DISC_NAME}
set CURRENT_DIR=%cd%
set path="%DISC_NAME%:/cygwin64/bin"
cd "%DISC_NAME%:/cygwin64/bin"		\r\n
`;

var workers = [
	{
		name: "sse",
		x86: {
			cmp_options: optionsCmpMaes,
		},
		x64: {
			cmp_options: optionsCmpMaes,
		}
	},
	{
		name: "legacy",
		x86: {
			cmp_options: {"": ""},
		},
		x64: {
			cmp_options: {"": ""},
		}
		
	}
];

function getCode(wrap_worker) {
	let worker_name = wrap_worker.name;
	let ret = {};
	for(let os_type of ["x86", "x64"]) {
		let worker = wrap_worker[os_type];
		ret[os_type] = [];
		
		for(let name_options in worker.cmp_options) {
			let worker_options = worker.cmp_options[name_options];
			
			ret[os_type].push(
				`gcc "%CURRENT_DIR%/src/apps/workers/${worker_name}/main.cpp" -o "%CURRENT_DIR%/dst/App/${os_type}/worker-${os_type}-${worker_name}${name_options}.bin" ${worker_options} -O3 -Wl,--strip-all -shared -I%CURRENT_DIR%/src/`
			);
		}
	}
	
	return ret;
}
function getCodeWorkers(workers) {
	let ret = {x86: [], x64: []};
	
	for(let worker of workers) {
		let r = getCode(worker);
		ret.x86 = ret.x86.concat(r.x86);
		ret.x64 = ret.x64.concat(r.x64);
	}
	
	return ret;
	
}

let r = getCodeWorkers(workers);
r.x86_line = r.x86.join("\r\n");
r.x64_line = r.x64.join("\r\n");

let x86_final = `${headBatCode_x86}${r.x86_line}\r\npause`;
let x64_final = `${headBatCode_x64}${r.x64_line}\r\npause`;


fs.writeFileSync("make - workers - x86.bat", x86_final);
fs.writeFileSync("make - workers - x64.bat", x64_final);



