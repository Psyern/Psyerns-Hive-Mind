//! Pure JSON DTO. Member names carry NO mod prefix on purpose: they are the
//! JSON keys the server admin edits.
class PHM_Settings
{
	int Version;

	bool Enabled;

	int ActiveTimeWindow;
	bool UseCustomNightHours;
	int NightStartHour;
	int NightEndHour;

	float ShareRadius;
	int MaxSharedZombies;

	int TriggerLevel;

	float BoostDurationSeconds;
	float VisionRangeMultiplier;

	int MaxRelayGenerations;
	float RelayMemorySeconds;
	float SenderCooldownSeconds;
	float MaxBroadcastsPerSecond;

	bool EnableNoisePing;
	string NoiseConfigPath;
	float NoiseLifetimeSeconds;
	float NoiseStrengthMultiplier;
	float NoisePingIntervalSeconds;
	bool LiveTrackWhileSeen;

	bool ExperimentalAlertOverride;
	int AlertOverrideLevel;
	float AlertOverrideInLevel;

	bool EnableLadderClimb;
	float ClimbMinHeight;
	float ClimbMaxDistance;
	float ClimbDurationSeconds;
	float ClimbCooldownSeconds;
	int ClimbersPerTick;

	bool EnableDoorOpening;
	float DoorOpenDelaySeconds;
	float DoorCooldownSeconds;
	float DoorMaxDistance;
	int DoorsPerTick;

	bool EnablePursuit;
	float PursuitRepathSeconds;
	float PursuitWaypointRadius;
	float PursuitSpeed;
	float PursuitMaxDistance;

	bool EnableMotorProbe;
	float ProbeSearchRadius;
	float ProbeDurationSeconds;
	float ProbeCooldownSeconds;
	float ProbeSpeed;
	float ProbeBearing;
	int ProbeTurnType;
	int ProbeTurnMode;
	float ProbeTurnThresholdDeg;

	bool ProbeSelfSpawn;
	string ProbeSpawnType;
	float ProbeSpawnX;
	float ProbeSpawnY;
	float ProbeSpawnZ;

	float SettingsReloadSeconds;
	bool LogBroadcasts;

	bool DebugMapEnabled;
	float DebugMapIntervalSeconds;
	int DebugMapMaxNodes;
	int DebugMapEventHistory;
	float DebugMapEventLifetime;
	ref array<string> DebugMapAdmins;

	void PHM_Settings()
	{
		DebugMapAdmins = new array<string>;
		Defaults();
	}

	void Defaults()
	{
		Version = PHM_Constants.SETTINGS_VERSION;

		Enabled = true;

		ActiveTimeWindow = EPHM_TimeWindow.ALWAYS;
		UseCustomNightHours = false;
		NightStartHour = 20;
		NightEndHour = 6;

		ShareRadius = 100.0;
		MaxSharedZombies = 16;

		//! CHASE, not ALERTED: DayZInfectedInputController.GetTargetEntity() is
		//! only ever read by vanilla in MINDSTATE_CHASE and MINDSTATE_FIGHT
		//! (ZombieBase.c:679 / :720, guarded by the comment "we attack only in
		//! chase & fight state" at :611). Below CHASE there is no evidence it
		//! returns anything, so a lower default would silently never fire.
		TriggerLevel = EPHM_TriggerLevel.CHASE;

		BoostDurationSeconds = 20.0;
		VisionRangeMultiplier = 3.0;

		MaxRelayGenerations = 1;
		RelayMemorySeconds = 60.0;
		SenderCooldownSeconds = 8.0;
		MaxBroadcastsPerSecond = 10.0;

		EnableNoisePing = true;
		NoiseConfigPath = "CfgVehicles SurvivorBase NoiseShout";
		NoiseLifetimeSeconds = 10.0;
		NoiseStrengthMultiplier = 1.0;

		//! Cadence below lifetime so the stimulus overlaps instead of gapping.
		NoisePingIntervalSeconds = 5.0;

		//! While any marked infected actively has a player as target, the
		//! refresher aims at that player's CURRENT position and keeps the pursuit
		//! window open. Without this, pings aim at a stale position and stop
		//! BoostDurationSeconds after the last broadcast even during a live chase.
		LiveTrackWhileSeen = true;

		//! Forces OverrideAlertLevel on marked infected. The parameter semantics
		//! are not provable from script (zero call sites anywhere), so this is an
		//! experiment switch: off by default, values tunable via hot reload.
		ExperimentalAlertOverride = false;
		AlertOverrideLevel = 3;
		AlertOverrideInLevel = 1.0;

		//! Hardcore option: marked infected "climb" ladders. Infected have no
		//! ladder animation or command, but the navmesh knows ladder connectivity
		//! (PGPolyFlags.LADDER) - a zombie that can reach the player ONLY via a
		//! ladder waits ClimbDurationSeconds at the base, then is placed on the
		//! navmesh point at the top and vanilla AI resumes the hunt up there.
		EnableLadderClimb = false;
		ClimbMinHeight = 3.0;
		ClimbMaxDistance = 15.0;
		ClimbDurationSeconds = 8.0;
		ClimbCooldownSeconds = 30.0;
		ClimbersPerTick = 2;

		//! Hardcore option: marked infected open unlocked doors that stand
		//! directly between them and their actively seen target. Locked doors
		//! keep their meaning and are never opened.
		EnableDoorOpening = false;
		DoorOpenDelaySeconds = 3.0;
		DoorCooldownSeconds = 15.0;
		DoorMaxDistance = 20.0;
		DoorsPerTick = 2;

		//! THE channel that actually moves a horde. Everything else the mod does
		//! is perception: GetMaxVisionRangeModifier only scales sight RANGE and
		//! still needs line of sight, and AddNoiseTarget is an engine broadcast
		//! whose reach comes from the noise config, not from ShareRadius. A
		//! marked infected 200 m away with no line of sight therefore had no way
		//! to learn where to walk. This drives it along a navmesh path instead.
		//!
		//! Off by default: OverrideHeading/OverrideMovementSpeed have zero
		//! vanilla call sites for INFECTED (only DayZAnimal.c:546-551 drives a
		//! creature this way), so this is proven-by-signature, not by precedent.
		EnablePursuit = false;

		PursuitRepathSeconds = 3.0;
		PursuitWaypointRadius = 1.5;

		//! 2 = RUN on the DayZInfectedConstantsMovement scale. Deliberately not
		//! SPRINT: a sprinting horde arriving from 300 m is not survivable.
		PursuitSpeed = 2.0;

		PursuitMaxDistance = 300.0;

		//! Step 0 motor probe. Diagnostic only: it periodically POSSESSES one
		//! nearby infected (AIAgent.SetKeepInIdle(true), the same suspension
		//! PluginDayZInfectedDebug.c:368 uses) and drives it at a fixed bearing so
		//! the RPT can answer the four open unknowns - speed scale, StartTurn
		//! semantics, whether GetTargetEntity survives, whether animation holds.
		//!
		//! Off by default and it must stay that way on a live server. Possession
		//! is a PINNED state: a release that fails leaves that infected paralysed
		//! until it despawns, which is worse than the measurement being missing.
		EnableMotorProbe = false;

		ProbeSearchRadius = 30.0;
		ProbeDurationSeconds = 15.0;
		ProbeCooldownSeconds = 30.0;

		//! 2 on the assumed DayZInfectedConstantsMovement scale is RUN, and on the
		//! m/s reading it is a brisk walk - either way a legible gait, which is
		//! exactly what makes the first run tell the two scales apart.
		ProbeSpeed = 2.0;

		//! Negative = keep whatever yaw the infected had at possession start, so
		//! the very first run measures translation without also introducing the
		//! unverified StartTurn semantics as a second variable.
		ProbeBearing = -1.0;

		ProbeTurnType = 0;
		ProbeTurnMode = 0;
		ProbeTurnThresholdDeg = 15.0;

		//! Lets the probe create its own test infected when no player is near one.
		//! Without this the measurement is impossible on an empty server: the
		//! subject search needs a player to measure distance from, and the Central
		//! Economy does not spawn infected without player proximity either.
		//!
		//! A freshly spawned subject is also the cleanest sample available for the
		//! "does perception survive SetKeepInIdle" question - it has existed for a
		//! single tick, so it cannot carry a target in.
		//!
		//! Off by default: it puts an infected into the world on a timer, which is
		//! not something a live server should do behind the admin's back.
		ProbeSelfSpawn = false;

		//! Same class vanilla's own possession tool spawns
		//! (PluginDayZInfectedDebug.c:365).
		ProbeSpawnType = "ZmbF_JournalistNormal_Blue";

		//! ECE_PLACE_ON_SURFACE corrects Y, so only X and Z have to be right.
		ProbeSpawnX = 0.0;
		ProbeSpawnY = 0.0;
		ProbeSpawnZ = 0.0;

		SettingsReloadSeconds = 60.0;
		LogBroadcasts = false;

		DebugMapEnabled = false;
		DebugMapIntervalSeconds = 1.0;
		DebugMapMaxNodes = 150;
		DebugMapEventHistory = 20;

		//! 60, not 20: the admin opens the map AFTER the fight. With a short
		//! lifetime everything has faded before the map is even on screen.
		DebugMapEventLifetime = 60.0;

		if (!DebugMapAdmins)
			DebugMapAdmins = new array<string>;

		DebugMapAdmins.Clear();
	}

	void Validate()
	{
		if (Version < 1)
			Version = PHM_Constants.SETTINGS_VERSION;

		int timeWindow = ActiveTimeWindow;
		if (timeWindow < EPHM_TimeWindow.ALWAYS)
			timeWindow = EPHM_TimeWindow.ALWAYS;
		if (timeWindow > EPHM_TimeWindow.DAY_ONLY)
			timeWindow = EPHM_TimeWindow.ALWAYS;
		ActiveTimeWindow = timeWindow;

		int trigger = TriggerLevel;
		if (trigger < EPHM_TriggerLevel.DISTURBED)
			trigger = EPHM_TriggerLevel.CHASE;
		if (trigger > EPHM_TriggerLevel.FIGHT)
			trigger = EPHM_TriggerLevel.CHASE;
		TriggerLevel = trigger;

		NightStartHour = Math.Clamp(NightStartHour, PHM_Constants.HOUR_MIN, PHM_Constants.HOUR_MAX);
		NightEndHour = Math.Clamp(NightEndHour, PHM_Constants.HOUR_MIN, PHM_Constants.HOUR_MAX);

		ShareRadius = Math.Clamp(ShareRadius, PHM_Constants.RADIUS_MIN, PHM_Constants.RADIUS_MAX);
		MaxSharedZombies = Math.Clamp(MaxSharedZombies, PHM_Constants.SHARE_COUNT_MIN, PHM_Constants.SHARE_COUNT_MAX);

		BoostDurationSeconds = Math.Clamp(BoostDurationSeconds, PHM_Constants.BOOST_SECONDS_MIN, PHM_Constants.BOOST_SECONDS_MAX);
		VisionRangeMultiplier = Math.Clamp(VisionRangeMultiplier, PHM_Constants.VISION_MULT_MIN, PHM_Constants.VISION_MULT_MAX);

		MaxRelayGenerations = Math.Clamp(MaxRelayGenerations, PHM_Constants.RELAY_GEN_MIN, PHM_Constants.RELAY_GEN_MAX);
		RelayMemorySeconds = Math.Clamp(RelayMemorySeconds, PHM_Constants.RELAY_MEMORY_MIN, PHM_Constants.RELAY_MEMORY_MAX);
		SenderCooldownSeconds = Math.Clamp(SenderCooldownSeconds, PHM_Constants.SEND_COOLDOWN_MIN, PHM_Constants.SEND_COOLDOWN_MAX);
		MaxBroadcastsPerSecond = Math.Clamp(MaxBroadcastsPerSecond, PHM_Constants.BROADCAST_RATE_MIN, PHM_Constants.BROADCAST_RATE_MAX);

		NoiseLifetimeSeconds = Math.Clamp(NoiseLifetimeSeconds, PHM_Constants.NOISE_LIFETIME_MIN, PHM_Constants.NOISE_LIFETIME_MAX);
		NoiseStrengthMultiplier = Math.Clamp(NoiseStrengthMultiplier, PHM_Constants.NOISE_STRENGTH_MIN, PHM_Constants.NOISE_STRENGTH_MAX);
		NoisePingIntervalSeconds = Math.Clamp(NoisePingIntervalSeconds, PHM_Constants.PING_INTERVAL_MIN, PHM_Constants.PING_INTERVAL_MAX);
		AlertOverrideLevel = Math.Clamp(AlertOverrideLevel, PHM_Constants.ALERT_LEVEL_MIN, PHM_Constants.ALERT_LEVEL_MAX);
		AlertOverrideInLevel = Math.Clamp(AlertOverrideInLevel, PHM_Constants.ALERT_INLEVEL_MIN, PHM_Constants.ALERT_INLEVEL_MAX);

		ClimbMinHeight = Math.Clamp(ClimbMinHeight, PHM_Constants.CLIMB_HEIGHT_MIN, PHM_Constants.CLIMB_HEIGHT_MAX);
		ClimbMaxDistance = Math.Clamp(ClimbMaxDistance, PHM_Constants.CLIMB_DIST_MIN, PHM_Constants.CLIMB_DIST_MAX);
		ClimbDurationSeconds = Math.Clamp(ClimbDurationSeconds, PHM_Constants.CLIMB_DURATION_MIN, PHM_Constants.CLIMB_DURATION_MAX);
		ClimbCooldownSeconds = Math.Clamp(ClimbCooldownSeconds, PHM_Constants.CLIMB_COOLDOWN_MIN, PHM_Constants.CLIMB_COOLDOWN_MAX);
		ClimbersPerTick = Math.Clamp(ClimbersPerTick, PHM_Constants.CLIMBERS_PER_TICK_MIN, PHM_Constants.CLIMBERS_PER_TICK_MAX);

		DoorOpenDelaySeconds = Math.Clamp(DoorOpenDelaySeconds, PHM_Constants.DOOR_DELAY_MIN, PHM_Constants.DOOR_DELAY_MAX);
		DoorCooldownSeconds = Math.Clamp(DoorCooldownSeconds, PHM_Constants.DOOR_COOLDOWN_MIN, PHM_Constants.DOOR_COOLDOWN_MAX);
		DoorMaxDistance = Math.Clamp(DoorMaxDistance, PHM_Constants.DOOR_DIST_MIN, PHM_Constants.DOOR_DIST_MAX);
		DoorsPerTick = Math.Clamp(DoorsPerTick, PHM_Constants.DOORS_PER_TICK_MIN, PHM_Constants.DOORS_PER_TICK_MAX);

		PursuitRepathSeconds = Math.Clamp(PursuitRepathSeconds, PHM_Constants.PURSUIT_REPATH_MIN, PHM_Constants.PURSUIT_REPATH_MAX);
		PursuitWaypointRadius = Math.Clamp(PursuitWaypointRadius, PHM_Constants.PURSUIT_WAYPOINT_MIN, PHM_Constants.PURSUIT_WAYPOINT_MAX);
		PursuitSpeed = Math.Clamp(PursuitSpeed, PHM_Constants.PURSUIT_SPEED_MIN, PHM_Constants.PURSUIT_SPEED_MAX);
		PursuitMaxDistance = Math.Clamp(PursuitMaxDistance, PHM_Constants.PURSUIT_DIST_MIN, PHM_Constants.PURSUIT_DIST_MAX);

		ProbeSearchRadius = Math.Clamp(ProbeSearchRadius, PHM_Constants.PROBE_RADIUS_MIN, PHM_Constants.PROBE_RADIUS_MAX);
		ProbeDurationSeconds = Math.Clamp(ProbeDurationSeconds, PHM_Constants.PROBE_DURATION_MIN, PHM_Constants.PROBE_DURATION_MAX);
		ProbeCooldownSeconds = Math.Clamp(ProbeCooldownSeconds, PHM_Constants.PROBE_COOLDOWN_MIN, PHM_Constants.PROBE_COOLDOWN_MAX);
		ProbeSpeed = Math.Clamp(ProbeSpeed, PHM_Constants.PROBE_SPEED_MIN, PHM_Constants.PROBE_SPEED_MAX);
		ProbeTurnType = Math.Clamp(ProbeTurnType, PHM_Constants.PROBE_TURN_TYPE_MIN, PHM_Constants.PROBE_TURN_TYPE_MAX);
		ProbeTurnMode = Math.Clamp(ProbeTurnMode, PHM_Constants.PROBE_TURN_MODE_MIN, PHM_Constants.PROBE_TURN_MODE_MAX);
		ProbeTurnThresholdDeg = Math.Clamp(ProbeTurnThresholdDeg, PHM_Constants.PROBE_TURN_THRESHOLD_MIN, PHM_Constants.PROBE_TURN_THRESHOLD_MAX);

		//! An empty class name would make CreateObjectEx fail on every tick. Fall
		//! back rather than let the switch look enabled while nothing can spawn.
		if (ProbeSpawnType == "")
			ProbeSpawnType = "ZmbF_JournalistNormal_Blue";

		//! A self-spawn at the world origin is always a mistake - 0/0 is the map
		//! corner - so refuse to arm the feature instead of dropping an infected
		//! into the sea every cooldown.
		bool originUnset = false;
		if (ProbeSpawnX == 0.0 && ProbeSpawnZ == 0.0)
			originUnset = true;

		if (ProbeSelfSpawn && originUnset)
			ProbeSelfSpawn = false;

		//! Every negative bearing collapses onto the single sentinel BEFORE the
		//! clamp, so -0.3 and -999 both mean "keep the captured yaw" instead of
		//! one of them surviving as a near-zero angle the operator never asked
		//! for. A plain Math.Clamp alone could not express that.
		if (ProbeBearing < 0.0)
			ProbeBearing = PHM_Constants.PROBE_BEARING_MIN;

		ProbeBearing = Math.Clamp(ProbeBearing, PHM_Constants.PROBE_BEARING_MIN, PHM_Constants.PROBE_BEARING_MAX);

		SettingsReloadSeconds = Math.Clamp(SettingsReloadSeconds, PHM_Constants.RELOAD_SECONDS_MIN, PHM_Constants.RELOAD_SECONDS_MAX);

		//! The relay memory has to outlive the vision boost, otherwise a zombie
		//! forgets its hop depth while it is still hive alerted and re-enters the
		//! chain as a fresh organic contact. That would make MaxRelayGenerations
		//! meaningless and let one sighting walk across the whole map.
		if (RelayMemorySeconds < BoostDurationSeconds)
			RelayMemorySeconds = BoostDurationSeconds;

		if (NoiseConfigPath == "")
			NoiseConfigPath = "CfgVehicles SurvivorBase NoiseShout";

		DebugMapIntervalSeconds = Math.Clamp(DebugMapIntervalSeconds, PHM_Constants.DEBUG_INTERVAL_MIN, PHM_Constants.DEBUG_INTERVAL_MAX);
		DebugMapMaxNodes = Math.Clamp(DebugMapMaxNodes, PHM_Constants.DEBUG_NODES_MIN, PHM_Constants.DEBUG_NODES_MAX);
		DebugMapEventHistory = Math.Clamp(DebugMapEventHistory, PHM_Constants.DEBUG_EVENTS_MIN, PHM_Constants.DEBUG_EVENTS_MAX);
		DebugMapEventLifetime = Math.Clamp(DebugMapEventLifetime, PHM_Constants.DEBUG_EVENT_LIFETIME_MIN, PHM_Constants.DEBUG_EVENT_LIFETIME_MAX);

		//! A missing array in the JSON leaves the member null after deserialization.
		if (!DebugMapAdmins)
			DebugMapAdmins = new array<string>;
	}

	//! Steam64 whitelist check. Empty list means nobody gets the debug map, even
	//! with DebugMapEnabled set - fail closed, never fail open.
	bool IsDebugAdmin(string steamId)
	{
		if (!DebugMapEnabled)
			return false;

		if (!DebugMapAdmins)
			return false;

		if (steamId == "")
			return false;

		int count = DebugMapAdmins.Count();
		int index;

		for (index = 0; index < count; index++)
		{
			if (DebugMapAdmins.Get(index) == steamId)
				return true;
		}

		return false;
	}
}
