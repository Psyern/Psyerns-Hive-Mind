enum EPHM_TimeWindow
{
	ALWAYS = 0,
	NIGHT_ONLY = 1,
	DAY_ONLY = 2
}

//! Admin facing trigger level. Deliberately NOT the raw DayZInfectedConstants
//! values: that enum shares its ordinal range with the COMMANDID_* entries, so
//! MINDSTATE_CALM is 7 and MINDSTATE_FIGHT is 11. Exposing those numbers in a
//! JSON file would be actively misleading. This enum is mapped onto the named
//! constants in PHM_ZombieBase.c.
enum EPHM_TriggerLevel
{
	DISTURBED = 0,
	ALERTED = 1,
	CHASE = 2,
	FIGHT = 3
}
