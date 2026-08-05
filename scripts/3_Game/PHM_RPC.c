//! Native DayZ RPC ids. Values must stay >= 10000 so they cannot collide with
//! vanilla ERPCs.
enum EPHM_RPC
{
	PHM_RPC_START = 10000,
	PHM_RPC_DEBUG_SUBSCRIBE,
	PHM_RPC_DEBUG_SNAPSHOT
}
