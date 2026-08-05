class PHM_Constants
{
	static const string VERSION = "1.0.0";

	static const string SETTINGS_ROOT = "$profile:Psyerns_Hive_Mind\\";
	static const string SETTINGS_DIR = "$profile:Psyerns_Hive_Mind\\Settings\\";
	static const string SETTINGS_FILE = "$profile:Psyerns_Hive_Mind\\Settings\\HiveMind.json";

	static const int SETTINGS_VERSION = 1;

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

	//! Registered from script so the admin can bind a key in the DayZ controls
	//! menu (Input.RegisterInput, UAInput.c:191).
	static const string INPUT_GROUP = "PsyernsHiveMind";
	static const string INPUT_MAP = "PHM_UAHiveMap";

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
