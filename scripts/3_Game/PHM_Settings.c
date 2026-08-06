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
