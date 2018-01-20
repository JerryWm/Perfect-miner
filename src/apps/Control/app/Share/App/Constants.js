const Constants = {
	PROC_PRIO_IDLE		: (0),
	PROC_PRIO_NORMAL 	: (1),
	PROC_PRIO_HIGH		: (2),
	PROC_PRIO_REALTIME	: (3),
	PROC_PRIO_UNDEFINED : (999),

	THR_PRIO_IDLE			: (0),
	THR_PRIO_LOWEST			: (1),
	THR_PRIO_NORMAL			: (2),
	THR_PRIO_HIGHEST		: (3),
	THR_PRIO_TIME_CRITICAL	: (4),
	THR_PRIO_UNDEFINED		: (999),
};

Constants.parseProcPrio = function(prio, def) {
	prio = parseInt(prio) | 0;
	
	if ( prio >= Constants.PROC_PRIO_IDLE || prio <= Constants.PROC_PRIO_REALTIME ) {
		return prio;
	}
	
	return def;
}
Constants.parseThrPrio = function(prio, def) {
	prio = parseInt(prio) | 0;
	
	if ( prio >= Constants.THR_PRIO_IDLE || prio <= Constants.THR_PRIO_TIME_CRITICAL ) {
		return prio;
	}
	
	return def;
}

module.exports = Constants;