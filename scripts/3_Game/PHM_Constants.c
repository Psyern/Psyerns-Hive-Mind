class PHM_Constants
{
	static const string VERSION = "1.0.0";

	static const string SETTINGS_ROOT = "$profile:Psyerns_Hive_Mind\\";
	static const string SETTINGS_DIR = "$profile:Psyerns_Hive_Mind\\Settings\\";
	static const string SETTINGS_FILE = "$profile:Psyerns_Hive_Mind\\Settings\\HiveMind.json";

	//! v2: NoisePingIntervalSeconds, LiveTrackWhileSeen, ExperimentalAlertOverride,
	//! AlertOverrideLevel, AlertOverrideInLevel
	//! v3: EnableLadderClimb, ClimbMinHeight, ClimbMaxDistance,
	//! ClimbDurationSeconds, ClimbCooldownSeconds, ClimbersPerTick
	//! v4: EnableDoorOpening, DoorOpenDelaySeconds, DoorCooldownSeconds,
	//! DoorMaxDistance, DoorsPerTick
	//! v5: EnablePursuit, PursuitRepathSeconds, PursuitWaypointRadius,
	//! PursuitSpeed, PursuitMaxDistance
	//! v6: EnableMotorProbe, ProbeSearchRadius, ProbeDurationSeconds,
	//! ProbeCooldownSeconds, ProbeSpeed, ProbeBearing, ProbeTurnType,
	//! ProbeTurnMode, ProbeTurnThresholdDeg
	static const int SETTINGS_VERSION = 6;

	//! Clamp bounds for Validate(). No value read from JSON may reach the
	//! selection loop unclamped.
	static const float RADIUS_MIN = 0.0;
	static const float RADIUS_MAX = 2000.0;

	static const int SHARE_COUNT_MIN = 0;
	static const int SHARE_COUNT_MAX = 256;

	static const int RELAY_GEN_MIN = 0;
	static const int RELAY_GEN_MAX = 5;

	static const float BOOST_SECONDS_MIN = 1.0;
	static const float BOOST_SECONDS_MAX = 300.0;

	static const float RELAY_MEMORY_MIN = 1.0;
	static const float RELAY_MEMORY_MAX = 600.0;

	//! Vanilla GetMaxVisionRangeModifier returns roughly 0.225 .. 1.25
	//! (PlayerConstants.AI_VISIBILITY_*). Anything far above that is unverified
	//! territory, hence the ceiling.
	static const float VISION_MULT_MIN = 1.0;
	static const float VISION_MULT_MAX = 50.0;

	static const float SEND_COOLDOWN_MIN = 0.0;
	static const float SEND_COOLDOWN_MAX = 120.0;

	static const float BROADCAST_RATE_MIN = 0.1;
	static const float BROADCAST_RATE_MAX = 100.0;

	static const float NOISE_LIFETIME_MIN = 0.5;
	static const float NOISE_LIFETIME_MAX = 60.0;

	static const float NOISE_STRENGTH_MIN = 0.0;
	static const float NOISE_STRENGTH_MAX = 10.0;

	static const float RELOAD_SECONDS_MIN = 0.0;
	static const float RELOAD_SECONDS_MAX = 3600.0;

	static const int HOUR_MIN = 0;
	static const int HOUR_MAX = 23;

	//! A broadcast attempt that is rejected (rate limit, time window, no headroom)
	//! must not burn the full sender cooldown, otherwise the sighting is lost.
	//! It gets this short retry delay instead, which also stops per-frame retries.
	static const float FAILED_RETRY_SECONDS = 1.0;

	//! Token bucket ceiling, expressed in seconds worth of budget. Without it a
	//! quiet period would accumulate an unbounded burst.
	static const float TOKEN_BUCKET_SECONDS = 1.0;

	//! Admin debug map bounds. The node and event caps also bound the RPC payload.
	static const float DEBUG_INTERVAL_MIN = 0.25;
	static const float DEBUG_INTERVAL_MAX = 10.0;

	static const int DEBUG_NODES_MIN = 0;
	static const int DEBUG_NODES_MAX = 400;

	static const int DEBUG_EVENTS_MIN = 0;
	static const int DEBUG_EVENTS_MAX = 100;

	static const float DEBUG_EVENT_LIFETIME_MIN = 1.0;
	static const float DEBUG_EVENT_LIFETIME_MAX = 120.0;

	//! Hard ceiling on what a single snapshot may put on the wire, independent of
	//! the admin configurable history. Without it the edge budget
	//! (DebugMapEventHistory * MaxSharedZombies) could reach 25600 entries at the
	//! maximum settings and produce an RPC packet far outside anything vanilla
	//! sends. Newest edges win.
	static const int DEBUG_EDGES_PER_SNAPSHOT = 400;

	//! How many distinct Steam ids a rejected-request warning is remembered for.
	//! Caps RPT writes on a path any client can reach.
	static const int DEBUG_WARN_IDS_MAX = 64;

	//! Noise refresher cadence.
	static const float PING_INTERVAL_MIN = 1.0;
	static const float PING_INTERVAL_MAX = 60.0;

	//! Experimental alert override. AIBehaviourHLDataZombie2 parses the alert
	//! level names Calm/Disturbed/Attracted/Alerted from config (AIBehaviour.c:133-136),
	//! which suggests an ordinal 0..3 - but the engine-side mapping of
	//! OverrideAlertLevel's parameters is unverified, hence configurable bounds.
	static const int ALERT_LEVEL_MIN = 0;
	static const int ALERT_LEVEL_MAX = 4;
	static const float ALERT_INLEVEL_MIN = 0.0;
	static const float ALERT_INLEVEL_MAX = 10.0;

	//! Ladder climb option.
	static const float CLIMB_HEIGHT_MIN = 2.0;
	static const float CLIMB_HEIGHT_MAX = 30.0;
	static const float CLIMB_DIST_MIN = 5.0;
	static const float CLIMB_DIST_MAX = 50.0;
	static const float CLIMB_DURATION_MIN = 1.0;
	static const float CLIMB_DURATION_MAX = 60.0;
	static const float CLIMB_COOLDOWN_MIN = 5.0;
	static const float CLIMB_COOLDOWN_MAX = 600.0;
	static const int CLIMBERS_PER_TICK_MIN = 1;
	static const int CLIMBERS_PER_TICK_MAX = 10;

	//! How close to the navmesh a position must sample, and how close a path end
	//! must come to the target to count as "reaches it".
	static const float CLIMB_SAMPLE_RADIUS = 3.0;
	static const float CLIMB_REACH_EPSILON = 3.0;

	//! Placement height slack: the first ladder-path waypoint at least this close
	//! below the target's height is treated as the top of the climb.
	static const float CLIMB_TOP_SLACK = 1.5;

	//! Door opening option.
	static const float DOOR_DELAY_MIN = 0.5;
	static const float DOOR_DELAY_MAX = 30.0;
	static const float DOOR_COOLDOWN_MIN = 5.0;
	static const float DOOR_COOLDOWN_MAX = 300.0;
	static const float DOOR_DIST_MIN = 5.0;
	static const float DOOR_DIST_MAX = 50.0;
	static const int DOORS_PER_TICK_MIN = 1;
	static const int DOORS_PER_TICK_MAX = 10;

	//! Ray from the zombie's chest along its OWN facing: a door only counts when
	//! it is directly in the way at arm's length. Aiming at the player instead
	//! made the ray miss every door that was not coincidentally on the straight
	//! line zombie->player, which is why 27 of 27 attempts refused in testing.
	//! Expansion aims the same way (eAIBase.c:11545: p1 = position + direction * 1.5).
	static const float DOOR_RAY_LENGTH = 2.5;
	static const float DOOR_RAY_HEIGHT = 1.2;
	static const float DOOR_RAY_RADIUS = 0.5;

	//! Navmesh pursuit. Speeds use the DayZInfectedConstantsMovement scale
	//! (IDLE 0, WALK 1, RUN 2, SPRINT 3) - the same scale HandleMove reads back
	//! from GetMovementSpeed() (ZombieBase.c:299) and the same one DayZAnimal
	//! feeds to OverrideMovementSpeed (DayZAnimal.c:549).
	static const float PURSUIT_REPATH_MIN = 0.5;
	static const float PURSUIT_REPATH_MAX = 30.0;

	static const float PURSUIT_WAYPOINT_MIN = 0.5;
	static const float PURSUIT_WAYPOINT_MAX = 10.0;

	static const float PURSUIT_SPEED_MIN = 1.0;
	static const float PURSUIT_SPEED_MAX = 3.0;

	static const float PURSUIT_DIST_MIN = 10.0;
	static const float PURSUIT_DIST_MAX = 2000.0;

	//! How far a position may sit off the navmesh and still snap onto it.
	static const float PURSUIT_SAMPLE_RADIUS = 5.0;

	//! Hard cap on stored waypoints per zombie, so the marked set can never hold
	//! more than MaxSharedZombies * this vectors.
	static const int PURSUIT_PATH_MAX = 64;

	//! Repath jitter, so 60 marked zombies never recompute in the same tick.
	static const float PURSUIT_REPATH_JITTER = 1.0;

	//! A repath that found nothing is retried on this (longer) delay instead of
	//! hammering FindPath every tick for a target that has no route at all.
	static const float PURSUIT_FAILED_REPATH_SECONDS = 5.0;

	//! Step 0 motor probe. One infected is possessed
	//! (PluginDayZInfectedDebug.c:368 SetKeepInIdle(true)) and driven at a fixed
	//! bearing so the four unknowns in the v2 design can be read off the RPT.
	//!
	//! This block deliberately does NOT reuse the pursuit bounds above. The
	//! PURSUIT_SPEED_MIN/MAX pair and its comment assert the
	//! DayZInfectedConstantsMovement scale (IDLE 0 / WALK 1 / RUN 2 / SPRINT 3,
	//! DayZInfected.c:20-25) as fact; the probe exists to TEST that assertion
	//! against the m/s reading, so its own bounds must be wider than either
	//! candidate scale. Do not "unify" the two.
	static const float PROBE_RADIUS_MIN = 5.0;
	static const float PROBE_RADIUS_MAX = 100.0;

	static const float PROBE_DURATION_MIN = 1.0;
	static const float PROBE_DURATION_MAX = 120.0;

	static const float PROBE_COOLDOWN_MIN = 5.0;
	static const float PROBE_COOLDOWN_MAX = 600.0;

	//! 0..6, not 1..3. Vanilla passes OverrideMovementSpeed a raw float straight
	//! off a text field (PluginDayZInfectedDebug.c:385), so nothing in script
	//! bounds it. 6 covers a plausible m/s sprint as well as the whole 0..3 enum.
	static const float PROBE_SPEED_MIN = 0.0;
	static const float PROBE_SPEED_MAX = 6.0;

	//! Bearing in the same degree space as GetOrientation()[0]. PROBE_BEARING_MIN
	//! is the sentinel "keep the yaw captured at possession start" rather than a
	//! real angle, so Validate() folds every negative value onto it instead of
	//! clamping into a wrap-around no operator would expect.
	static const float PROBE_BEARING_MIN = -1.0;
	static const float PROBE_BEARING_MAX = 360.0;

	//! StartTurn's second parameter is declared pSpeedType, not pTurnType
	//! (DayZInfected.c:40), and vanilla fills it from a combo box whose entries
	//! live in the debug .layout, not in script (PluginDayZInfectedDebug.c:396).
	//! Which values are legal and what they select is one of the things this
	//! probe measures, hence a configurable ordinal rather than a named enum.
	static const int PROBE_TURN_TYPE_MIN = 0;
	static const int PROBE_TURN_TYPE_MAX = 3;

	//! 0 = hand StartTurn the absolute yaw, 1 = hand it the relative delta.
	//! Vanilla reads its direction off a free text field, so neither reading can
	//! be ruled out from source. The probe sweeps both.
	static const int PROBE_TURN_MODE_MIN = 0;
	static const int PROBE_TURN_MODE_MAX = 1;

	//! Angular error at which a turn is issued. Turning is a discrete command,
	//! not a continuous channel, so without a deadband the probe would re-issue
	//! StartTurn on every one of the ~50 command ticks per second.
	static const float PROBE_TURN_THRESHOLD_MIN = 1.0;
	static const float PROBE_TURN_THRESHOLD_MAX = 180.0;

	//! Input name as declared in scripts/data/Inputs.xml, which config.cpp binds
	//! through the CfgMods "inputs" property.
	//!
	//! Do NOT try to create this from script with UAInputAPI.RegisterInput: that
	//! method is declared (UAInput.c:191) but has zero call sites in vanilla 1.29
	//! and zero in DayZ Expansion. Every Expansion module with a keybind ships an
	//! Inputs.xml instead (e.g. Book/Scripts/Data/Inputs.xml + config.cpp:20).
	static const string INPUT_MAP = "UAPHMHiveMapToggle";

	static const string LAYOUT_MAP = "Psyerns_Hive_Mind_V1/gui/layouts/phm_hive_map.layout";
	static const string LAYOUT_LINE = "Psyerns_Hive_Mind_V1/gui/layouts/phm_hive_line.layout";

	//! Vanilla map marker texture, reused so the mod ships no textures of its own.
	//! Proven path from ScriptConsoleGeneralTab.c:968.
	static const string MARK_TEXTURE = "\\dz\\gear\\navigation\\data\\map_tree_ca.paa";

	//! ARGB literals. ARGB() is a function and cannot initialise a const.
	static const int COLOR_SPOTTER = 0xFFED3B2C;
	static const int COLOR_PLAYER = 0xFF6EDB6E;
	static const int COLOR_HOP0 = 0xFFFFBB33;
	static const int COLOR_HOP1 = 0xFF52A8FF;
	static const int COLOR_REFRESH = 0xFF8A8F99;
	static const int COLOR_RING = 0xFFED3B2C;

	static const float LINE_THICKNESS = 2.0;
	static const int LINE_POOL_MAX = 512;

	static const int RING_SEGMENTS = 24;
	static const float RING_ALPHA = 0.28;
	static const float RING_MAX_AGE = 6.0;

	static const float EDGE_FADE_SECONDS = 12.0;
	static const float EDGE_MIN_ALPHA = 0.18;
}
