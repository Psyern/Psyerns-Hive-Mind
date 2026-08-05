//! Server side collector for the admin debug map.
//!
//! Records whenever DebugMapEnabled is set - NOT only while a map is open.
//! Field-tested reason: marks expire after BoostDurationSeconds, so by the time
//! an admin opens the map after a fight the live state is empty; the recorded
//! history is the only thing left to show. Recording is bounded (event history
//! plus the per-snapshot edge cap), and on a production server with
//! DebugMapEnabled=false the hive pays one settings read per broadcast.
class PHM_DebugTracker
{
	protected static ref PHM_DebugTracker s_PHM_Instance;

	//! Steam64 ids of admins with an open map. Identities are resolved fresh on
	//! every push instead of being cached, so a disconnect cannot leave a stale
	//! PlayerIdentity behind.
	protected ref array<string> m_PHM_Subscribers;

	protected ref array<ref PHM_DebugEdge> m_PHM_Edges;
	protected ref array<ref PHM_DebugSpotter> m_PHM_Spotters;
	protected ref PHM_DebugSnapshot m_PHM_Snapshot;

	void PHM_DebugTracker()
	{
		m_PHM_Subscribers = new array<string>;
		m_PHM_Edges = new array<ref PHM_DebugEdge>;
		m_PHM_Spotters = new array<ref PHM_DebugSpotter>;
		m_PHM_Snapshot = new PHM_DebugSnapshot();
	}

	static PHM_DebugTracker GetInstance()
	{
		if (!s_PHM_Instance)
			s_PHM_Instance = new PHM_DebugTracker();

		return s_PHM_Instance;
	}

	//! Hot path guard, called once per broadcast. Driven by the setting, not by
	//! open maps, so history exists BEFORE the admin looks at it.
	static bool IsRecording()
	{
		PHM_Settings settings = PHM_SettingsHolder.Get();
		if (!settings)
			return false;

		return settings.DebugMapEnabled;
	}

	static PHM_DebugTracker GetRecorder()
	{
		return s_PHM_Instance;
	}

	bool HasSubscribers()
	{
		if (!m_PHM_Subscribers)
			return false;

		return m_PHM_Subscribers.Count() > 0;
	}

	int GetSubscriberCount()
	{
		if (!m_PHM_Subscribers)
			return 0;

		return m_PHM_Subscribers.Count();
	}

	bool IsSubscribed(string steamId)
	{
		if (!m_PHM_Subscribers)
			return false;

		return m_PHM_Subscribers.Find(steamId) >= 0;
	}

	void Subscribe(string steamId)
	{
		if (steamId == "")
			return;

		if (m_PHM_Subscribers.Find(steamId) >= 0)
			return;

		m_PHM_Subscribers.Insert(steamId);
		PHM_Logger.Notice("Debug map subscribed: " + steamId);
	}

	void Unsubscribe(string steamId)
	{
		int index = m_PHM_Subscribers.Find(steamId);
		if (index < 0)
			return;

		m_PHM_Subscribers.Remove(index);
		PHM_Logger.Notice("Debug map unsubscribed: " + steamId);

		//! History is deliberately KEPT across close/open cycles. It is bounded,
		//! ages out via DebugMapEventLifetime, and reopening the map after a fight
		//! must show what just happened - that is the whole point of the map.
	}

	void RecordSpotter(vector senderPos, vector playerPos, string playerName, float radius, int hop, PHM_Settings settings)
	{
		if (!g_Game)
			return;

		PHM_DebugSpotter spotter = new PHM_DebugSpotter();
		spotter.x = senderPos[0];
		spotter.z = senderPos[2];
		spotter.px = playerPos[0];
		spotter.pz = playerPos[2];
		spotter.radius = radius;
		spotter.age = g_Game.GetTickTime();
		spotter.hop = hop;
		spotter.playerName = playerName;

		m_PHM_Spotters.Insert(spotter);
		TrimHistory(m_PHM_Spotters.Count() - settings.DebugMapEventHistory, m_PHM_Spotters, null);
	}

	void RecordEdge(vector fromPos, vector toPos, int hop, bool refreshOnly, PHM_Settings settings)
	{
		if (!g_Game)
			return;

		PHM_DebugEdge edge = new PHM_DebugEdge();
		edge.ax = fromPos[0];
		edge.az = fromPos[2];
		edge.bx = toPos[0];
		edge.bz = toPos[2];
		edge.hop = hop;
		edge.refreshOnly = refreshOnly;
		edge.age = g_Game.GetTickTime();

		m_PHM_Edges.Insert(edge);

		//! Each broadcast can add up to MaxSharedZombies edges, so the edge budget
		//! is history times cap rather than history alone.
		int edgeBudget = settings.DebugMapEventHistory * settings.MaxSharedZombies;
		if (edgeBudget < settings.DebugMapEventHistory)
			edgeBudget = settings.DebugMapEventHistory;

		TrimHistory(m_PHM_Edges.Count() - edgeBudget, null, m_PHM_Edges);
	}

	//! Drops the oldest entries. Exactly one of the two arrays is passed.
	protected void TrimHistory(int excess, array<ref PHM_DebugSpotter> spotters, array<ref PHM_DebugEdge> edges)
	{
		int removed = 0;

		while (removed < excess)
		{
			if (spotters && spotters.Count() > 0)
				spotters.RemoveOrdered(0);

			if (edges && edges.Count() > 0)
				edges.RemoveOrdered(0);

			removed = removed + 1;
		}
	}

	//! Builds the snapshot that goes over the wire. Reuses one instance, so this
	//! allocates only the node/edge entries themselves.
	PHM_DebugSnapshot BuildSnapshot()
	{
		if (!g_Game)
			return null;

		PHM_Settings settings = PHM_SettingsHolder.Get();
		if (!settings)
			return null;

		PHM_HiveManager manager = PHM_HiveManager.GetInstance();
		if (!manager)
			return null;

		float now = g_Game.GetTickTime();

		m_PHM_Snapshot.Reset();

		m_PHM_Snapshot.cap = settings.MaxSharedZombies;
		m_PHM_Snapshot.shareRadius = settings.ShareRadius;
		m_PHM_Snapshot.relayGenerations = settings.MaxRelayGenerations;
		m_PHM_Snapshot.visionMultiplier = settings.VisionRangeMultiplier;
		m_PHM_Snapshot.noisePing = settings.EnableNoisePing;
		m_PHM_Snapshot.hiveActive = PHM_TimeGate.IsActive(settings);
		m_PHM_Snapshot.isNight = PHM_TimeGate.IsNightNow(settings);

		World world = g_Game.GetWorld();
		if (world)
		{
			int year;
			int month;
			int day;
			int hour;
			int minute;
			world.GetDate(year, month, day, hour, minute);
			m_PHM_Snapshot.worldHour = hour;
			m_PHM_Snapshot.worldMinute = minute;
		}

		array<ZombieBase> registry = ZombieBase.PHM_GetRegistry();
		if (registry)
			m_PHM_Snapshot.registrySize = registry.Count();

		manager.PruneMarked();

		array<ZombieBase> marked = manager.PHM_GetMarked();
		if (marked)
		{
			int markedCount = marked.Count();
			m_PHM_Snapshot.markedCount = markedCount;

			int limit = settings.DebugMapMaxNodes;
			if (limit > markedCount)
				limit = markedCount;

			int index;
			ZombieBase zombie;
			vector position;

			for (index = 0; index < limit; index++)
			{
				zombie = marked.Get(index);
				if (!zombie)
					continue;

				position = zombie.GetPosition();

				PHM_DebugNode node = new PHM_DebugNode();
				node.x = position[0];
				node.z = position[2];
				node.hop = zombie.PHM_GetHop();
				node.remaining = zombie.PHM_GetHiveRemaining();

				m_PHM_Snapshot.nodes.Insert(node);
			}

			m_PHM_Snapshot.nodesOmitted = markedCount - limit;
		}

		CopyRecentEdges(now, settings);
		CopyRecentSpotters(now, settings);

		return m_PHM_Snapshot;
	}

	//! Ages are converted to "seconds ago" here so the client needs no clock of
	//! its own and no assumption about server tick time.
	protected void CopyRecentEdges(float now, PHM_Settings settings)
	{
		int count = m_PHM_Edges.Count();
		int index;
		PHM_DebugEdge source;
		float age;

		//! Walk from the newest backwards so the hard packet cap drops the OLDEST
		//! edges, not the ones the admin actually wants to see.
		for (index = count - 1; index >= 0; index--)
		{
			if (m_PHM_Snapshot.edges.Count() >= PHM_Constants.DEBUG_EDGES_PER_SNAPSHOT)
				break;

			source = m_PHM_Edges.Get(index);
			if (!source)
				continue;

			age = now - source.age;
			if (age > settings.DebugMapEventLifetime)
				continue;

			PHM_DebugEdge copy = new PHM_DebugEdge();
			copy.ax = source.ax;
			copy.az = source.az;
			copy.bx = source.bx;
			copy.bz = source.bz;
			copy.hop = source.hop;
			copy.refreshOnly = source.refreshOnly;
			copy.age = age;

			m_PHM_Snapshot.edges.Insert(copy);
		}
	}

	protected void CopyRecentSpotters(float now, PHM_Settings settings)
	{
		int count = m_PHM_Spotters.Count();
		int index;
		PHM_DebugSpotter source;
		float age;

		for (index = 0; index < count; index++)
		{
			source = m_PHM_Spotters.Get(index);
			if (!source)
				continue;

			age = now - source.age;
			if (age > settings.DebugMapEventLifetime)
				continue;

			PHM_DebugSpotter copy = new PHM_DebugSpotter();
			copy.x = source.x;
			copy.z = source.z;
			copy.px = source.px;
			copy.pz = source.pz;
			copy.radius = source.radius;
			copy.hop = source.hop;
			copy.playerName = source.playerName;
			copy.age = age;

			m_PHM_Snapshot.spotters.Insert(copy);
		}
	}
}
