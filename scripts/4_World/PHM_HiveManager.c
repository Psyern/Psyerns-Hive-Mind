//! Distribution core. Lives in 4_World because it works with ZombieBase and
//! PlayerBase, which 3_Game is not allowed to reference.
//!
//! Performs no engine space query at all: candidate selection walks the mod's
//! own static registry, so both the radius and the count cap are pure script
//! arithmetic and therefore exact and reproducible.
class PHM_HiveManager
{
	protected static ref PHM_HiveManager s_PHM_Instance;

	//! Every zombie currently carrying a hive mark, server wide. This is what
	//! makes MaxSharedZombies a real ceiling instead of a per message limit.
	protected ref array<ZombieBase> m_PHM_Marked;

	protected ref array<ZombieBase> m_PHM_Candidates;
	protected ref array<float> m_PHM_CandidateDistance;
	protected ref array<ZombieBase> m_PHM_Selected;
	protected ref array<float> m_PHM_SelectedDistance;

	protected ref NoiseParams m_PHM_NoiseParams;
	protected string m_PHM_LoadedNoisePath;

	protected float m_PHM_TokenBudget;
	protected float m_PHM_LastTokenRefill;
	protected int m_PHM_RefreshedCount;

	//! Weak handle to the debug tracker, set per broadcast and null when nobody is
	//! watching. Non-ref on purpose: the tracker owns itself through its own static.
	protected PHM_DebugTracker m_PHM_Recorder;
	protected vector m_PHM_SenderPos;

	//! Last position a player was seen at by any broadcast, and until when the
	//! hive keeps pulling towards it. Field-tested reason this exists: a single
	//! AddNoiseTarget per broadcast (10 s lifetime) fires once and then the marked
	//! infected without line of sight simply stop. The refresher below re-pings
	//! this position for the boost duration, which is what actually moves a horde.
	protected vector m_PHM_LastSeenPos;
	protected float m_PHM_LastSeenUntil;
	protected bool m_PHM_NoiseTimerRunning;

	void PHM_HiveManager()
	{
		m_PHM_Marked = new array<ZombieBase>;
		m_PHM_Candidates = new array<ZombieBase>;
		m_PHM_CandidateDistance = new array<float>;
		m_PHM_Selected = new array<ZombieBase>;
		m_PHM_SelectedDistance = new array<float>;

		m_PHM_LoadedNoisePath = "";
		m_PHM_TokenBudget = 0.0;
		m_PHM_LastTokenRefill = 0.0;
		m_PHM_RefreshedCount = 0;

		m_PHM_LastSeenUntil = 0.0;
		m_PHM_NoiseTimerRunning = false;
	}

	static PHM_HiveManager GetInstance()
	{
		if (!s_PHM_Instance)
			s_PHM_Instance = new PHM_HiveManager();

		return s_PHM_Instance;
	}

	//! Called from ZombieBase teardown. Must not create the instance.
	static void PHM_ForgetZombie(ZombieBase zombie)
	{
		if (!s_PHM_Instance)
			return;

		s_PHM_Instance.ForgetZombie(zombie);
	}

	void ForgetZombie(ZombieBase zombie)
	{
		if (!m_PHM_Marked)
			return;

		int index = m_PHM_Marked.Find(zombie);
		if (index >= 0)
			m_PHM_Marked.Remove(index);
	}

	array<ZombieBase> PHM_GetMarked()
	{
		return m_PHM_Marked;
	}

	bool Broadcast(ZombieBase sender, PlayerBase seenPlayer, int hop)
	{
		if (!g_Game)
			return false;

		if (!sender)
			return false;

		if (!seenPlayer)
			return false;

		//! Cheap float compare in the common case, lets admins calibrate without
		//! a server restart.
		PHM_SettingsHolder.ReloadIfDue();

		PHM_Settings settings = PHM_SettingsHolder.Get();
		if (!settings)
			return false;

		if (!settings.Enabled)
			return false;

		//! Time window first, before any token or cooldown is spent. Otherwise a
		//! NIGHT_ONLY server would burn budget all day on messages it then drops.
		if (!PHM_TimeGate.IsActive(settings))
			return false;

		float now = g_Game.GetTickTime();

		//! Peek only, spent further down once the registry walk has actually run.
		if (!HasToken(now, settings))
			return false;

		int cap = settings.MaxSharedZombies;
		if (cap <= 0)
			return false;

		PruneMarked();

		int headroom = cap - m_PHM_Marked.Count();

		vector origin = sender.GetPosition();
		float radius = settings.ShareRadius;

		//! One settings read on a production server. GetInstance (not GetRecorder):
		//! recording must work before any admin has ever opened the map.
		m_PHM_Recorder = null;
		if (PHM_DebugTracker.IsRecording())
			m_PHM_Recorder = PHM_DebugTracker.GetInstance();

		m_PHM_SenderPos = origin;

		CollectCandidates(sender, origin, radius, settings, hop);

		//! The registry walk above IS the cost this limiter exists for, so the
		//! token is spent here rather than only on success. Otherwise a saturated
		//! cap would let every chasing zombie re-walk the whole registry once per
		//! second forever, unthrottled, because nothing ever "succeeded".
		//! The sender cooldown and the rearm flag stay tied to the real outcome
		//! below, so a throttled sighting is never lost permanently.
		ConsumeToken();

		int markedCount = 0;
		if (headroom > 0)
			markedCount = MarkNearestFresh(headroom, settings, hop);

		bool pinged = false;
		if (settings.EnableNoisePing)
			pinged = EmitNoisePing(seenPlayer.GetPosition(), settings);

		if (m_PHM_RefreshedCount == 0 && markedCount == 0 && !pinged)
		{
			m_PHM_Recorder = null;
			return false;
		}

		//! Remember where the player was seen and keep pulling towards it for the
		//! boost duration. Deliberately the LAST SEEN position, never live tracking:
		//! the hive knows where you were spotted, not where you went.
		m_PHM_LastSeenPos = seenPlayer.GetPosition();
		m_PHM_LastSeenUntil = now + settings.BoostDurationSeconds;
		StartNoiseRefresh(settings);

		if (m_PHM_Recorder)
		{
			string playerName = "";
			PlayerIdentity identity = seenPlayer.GetIdentity();
			if (identity)
				playerName = identity.GetPlainName();

			m_PHM_Recorder.RecordSpotter(origin, seenPlayer.GetPosition(), playerName, radius, hop, settings);
			m_PHM_Recorder = null;
		}

		if (settings.LogBroadcasts)
		{
			string report = "broadcast hop=" + hop.ToString();
			report = report + " candidates=" + m_PHM_Candidates.Count().ToString();
			report = report + " newlyMarked=" + markedCount.ToString();
			report = report + " refreshed=" + m_PHM_RefreshedCount.ToString();
			report = report + " markedTotal=" + m_PHM_Marked.Count().ToString();
			report = report + " cap=" + cap.ToString();
			report = report + " noisePing=" + pinged.ToString();
			PHM_Logger.Debug(report);
		}

		return true;
	}

	//! One repeating server-wide timer, started lazily on the first successful
	//! broadcast and never removed - the manager is a permanent singleton and the
	//! callback early-outs on a couple of float compares while the hive is idle.
	//! Vanilla pairing: missionServer.c:66 uses the same CALL_CATEGORY_SYSTEM
	//! CallLater(func, interval, true) shape.
	//!
	//! The cadence is fixed from NoiseLifetimeSeconds at first start; a hot
	//! reloaded lifetime changes the ping length immediately but the cadence only
	//! after a server restart.
	protected void StartNoiseRefresh(PHM_Settings settings)
	{
		if (m_PHM_NoiseTimerRunning)
			return;

		if (!settings.EnableNoisePing)
			return;

		if (!g_Game)
			return;

		ScriptCallQueue queue = g_Game.GetCallQueue(CALL_CATEGORY_SYSTEM);
		if (!queue)
			return;

		int interval = settings.NoisePingIntervalSeconds * 1000;
		if (interval < 500)
			interval = 500;

		queue.CallLater(PHM_RefreshNoise, interval, true);
		m_PHM_NoiseTimerRunning = true;
	}

	//! Keeps the pursuit alive while any mark exists. Two behaviours:
	//! - LiveTrackWhileSeen: if any marked infected actively targets a player, the
	//!   ping aims at that player's CURRENT position and the pursuit window is
	//!   extended - "while any hive eye sees you, the hive knows where you are".
	//!   Without this, pings aim at a stale position and stop BoostDurationSeconds
	//!   after the last broadcast even during an ongoing chase.
	//! - Otherwise: re-pings the last seen position until the window expires.
	void PHM_RefreshNoise()
	{
		if (!g_Game)
			return;

		if (!g_Game.IsDedicatedServer())
			return;

		PHM_Settings settings = PHM_SettingsHolder.Get();
		if (!settings)
			return;

		if (!settings.Enabled)
			return;

		if (!settings.EnableNoisePing)
			return;

		if (!PHM_TimeGate.IsActive(settings))
			return;

		PruneMarked();
		if (m_PHM_Marked.Count() == 0)
			return;

		float now = g_Game.GetTickTime();

		if (settings.LiveTrackWhileSeen)
		{
			PlayerBase seenPlayer = FindActivelySeenPlayer();
			if (seenPlayer)
			{
				m_PHM_LastSeenPos = seenPlayer.GetPosition();
				m_PHM_LastSeenUntil = now + settings.BoostDurationSeconds;
			}
		}

		if (now >= m_PHM_LastSeenUntil)
			return;

		EmitNoisePing(m_PHM_LastSeenPos, settings);
	}

	//! First marked infected with an active player target wins. Bounded by the
	//! marked cap (max 256), runs only on the refresher cadence.
	protected PlayerBase FindActivelySeenPlayer()
	{
		int count = m_PHM_Marked.Count();
		int index;
		ZombieBase zombie;
		PlayerBase player;

		for (index = 0; index < count; index++)
		{
			zombie = m_PHM_Marked.Get(index);
			if (!zombie)
				continue;

			player = zombie.PHM_GetActiveTargetPlayer();
			if (player)
				return player;
		}

		return null;
	}

	//! Token bucket, refilled lazily. Capped at one second worth of budget so a
	//! quiet period cannot accumulate an unbounded burst.
	protected bool HasToken(float now, PHM_Settings settings)
	{
		if (m_PHM_LastTokenRefill <= 0.0)
		{
			m_PHM_LastTokenRefill = now;
			m_PHM_TokenBudget = settings.MaxBroadcastsPerSecond * PHM_Constants.TOKEN_BUCKET_SECONDS;
		}

		float elapsed = now - m_PHM_LastTokenRefill;
		if (elapsed > 0.0)
		{
			m_PHM_LastTokenRefill = now;
			m_PHM_TokenBudget = m_PHM_TokenBudget + (elapsed * settings.MaxBroadcastsPerSecond);
		}

		float ceiling = settings.MaxBroadcastsPerSecond * PHM_Constants.TOKEN_BUCKET_SECONDS;
		if (m_PHM_TokenBudget > ceiling)
			m_PHM_TokenBudget = ceiling;

		return m_PHM_TokenBudget >= 1.0;
	}

	protected void ConsumeToken()
	{
		m_PHM_TokenBudget = m_PHM_TokenBudget - 1.0;
		if (m_PHM_TokenBudget < 0.0)
			m_PHM_TokenBudget = 0.0;
	}

	//! Drops entries whose mark has expired or whose zombie is gone. Runs only on
	//! the rate limited broadcast path, and the array is bounded by
	//! MaxSharedZombies (max 256), so this is cheap.
	void PruneMarked()
	{
		if (!m_PHM_Marked)
			return;

		int index = m_PHM_Marked.Count() - 1;
		ZombieBase zombie;

		while (index >= 0)
		{
			zombie = m_PHM_Marked.Get(index);

			if (!zombie)
			{
				m_PHM_Marked.Remove(index);
			}
			else if (zombie.IsSetForDeletion())
			{
				m_PHM_Marked.Remove(index);
			}
			else if (!zombie.IsAlive())
			{
				m_PHM_Marked.Remove(index);
			}
			else if (!zombie.PHM_IsHiveAlerted())
			{
				//! Leaving the marked set alive: release the experimental alert
				//! override so the zombie falls back to untouched vanilla behaviour.
				zombie.PHM_ReleaseAlertOverride();
				m_PHM_Marked.Remove(index);
			}

			index = index - 1;
		}
	}

	//! Single pass over the registry. Zombies that are already marked get their
	//! timer refreshed right here and do NOT consume headroom, since they are
	//! already counted in m_PHM_Marked. Everything else lands in the candidate
	//! arrays for the bounded nearest-N selection.
	protected void CollectCandidates(ZombieBase sender, vector origin, float radius, PHM_Settings settings, int hop)
	{
		m_PHM_Candidates.Clear();
		m_PHM_CandidateDistance.Clear();
		m_PHM_RefreshedCount = 0;

		array<ZombieBase> registry = ZombieBase.PHM_GetRegistry();
		if (!registry)
			return;

		int count = registry.Count();
		int index;
		ZombieBase zombie;
		float distance;

		for (index = 0; index < count; index++)
		{
			zombie = registry.Get(index);

			if (!zombie)
				continue;

			if (zombie == sender)
				continue;

			if (zombie.IsSetForDeletion())
				continue;

			if (!zombie.IsAlive())
				continue;

			//! Without an AI agent the zombie cannot act on anything.
			if (!zombie.GetAIAgent())
				continue;

			distance = vector.Distance(origin, zombie.GetPosition());
			if (distance > radius)
				continue;

			if (zombie.PHM_IsHiveAlerted())
			{
				zombie.PHM_ApplyHiveAlert(settings.BoostDurationSeconds, settings.RelayMemorySeconds, hop);

				//! Re-applied on refresh so a hot-reloaded flip of the experiment
				//! switch reaches zombies that are already marked. Idempotent.
				if (settings.ExperimentalAlertOverride)
					zombie.PHM_ApplyAlertOverride(settings.AlertOverrideLevel, settings.AlertOverrideInLevel);

				m_PHM_RefreshedCount = m_PHM_RefreshedCount + 1;

				if (m_PHM_Recorder)
					m_PHM_Recorder.RecordEdge(origin, zombie.GetPosition(), hop, true, settings);

				continue;
			}

			m_PHM_Candidates.Insert(zombie);
			m_PHM_CandidateDistance.Insert(distance);
		}
	}

	//! Bounded insertion selection of the nearest candidates. The nearest always
	//! win, and at most 'headroom' zombies leave this function marked.
	protected int MarkNearestFresh(int headroom, PHM_Settings settings, int hop)
	{
		int candidateCount = m_PHM_Candidates.Count();
		if (candidateCount == 0)
			return 0;

		int limit = headroom;
		if (limit > candidateCount)
			limit = candidateCount;

		if (limit <= 0)
			return 0;

		m_PHM_Selected.Clear();
		m_PHM_SelectedDistance.Clear();

		int index;
		int slot;
		int selectedCount;
		ZombieBase zombie;
		float distance;
		float worstDistance;

		for (index = 0; index < candidateCount; index++)
		{
			zombie = m_PHM_Candidates.Get(index);
			distance = m_PHM_CandidateDistance.Get(index);
			selectedCount = m_PHM_Selected.Count();

			if (selectedCount < limit)
			{
				m_PHM_Selected.Insert(zombie);
				m_PHM_SelectedDistance.Insert(distance);
				BubbleUp(m_PHM_Selected.Count() - 1);
				continue;
			}

			worstDistance = m_PHM_SelectedDistance.Get(limit - 1);
			if (distance >= worstDistance)
				continue;

			m_PHM_Selected.Set(limit - 1, zombie);
			m_PHM_SelectedDistance.Set(limit - 1, distance);
			BubbleUp(limit - 1);
		}

		int applied = 0;
		int finalCount = m_PHM_Selected.Count();

		for (slot = 0; slot < finalCount; slot++)
		{
			zombie = m_PHM_Selected.Get(slot);
			if (!zombie)
				continue;

			zombie.PHM_ApplyHiveAlert(settings.BoostDurationSeconds, settings.RelayMemorySeconds, hop);

			if (settings.ExperimentalAlertOverride)
				zombie.PHM_ApplyAlertOverride(settings.AlertOverrideLevel, settings.AlertOverrideInLevel);

			m_PHM_Marked.Insert(zombie);
			applied = applied + 1;

			if (m_PHM_Recorder)
				m_PHM_Recorder.RecordEdge(m_PHM_SenderPos, zombie.GetPosition(), hop, false, settings);
		}

		return applied;
	}

	protected void BubbleUp(int startIndex)
	{
		int position = startIndex;
		float current;
		float previous;
		ZombieBase swapZombie;
		float swapDistance;

		while (position > 0)
		{
			current = m_PHM_SelectedDistance.Get(position);
			previous = m_PHM_SelectedDistance.Get(position - 1);

			if (current >= previous)
				break;

			swapZombie = m_PHM_Selected.Get(position);
			swapDistance = m_PHM_SelectedDistance.Get(position);

			m_PHM_Selected.Set(position, m_PHM_Selected.Get(position - 1));
			m_PHM_SelectedDistance.Set(position, m_PHM_SelectedDistance.Get(position - 1));

			m_PHM_Selected.Set(position - 1, swapZombie);
			m_PHM_SelectedDistance.Set(position - 1, swapDistance);

			position = position - 1;
		}
	}

	//! Secondary channel. Unlike the vision boost this reaches infected without
	//! line of sight, which is what makes luring hordes possible at all. It is an
	//! engine broadcast to a world position though: it ignores MaxSharedZombies
	//! and its range is not deterministically configurable.
	protected bool EmitNoisePing(vector position, PHM_Settings settings)
	{
		if (!g_Game)
			return false;

		NoiseSystem noiseSystem = g_Game.GetNoiseSystem();
		if (!noiseSystem)
			return false;

		if (!EnsureNoiseParams(settings))
			return false;

		noiseSystem.AddNoiseTarget(position, settings.NoiseLifetimeSeconds, m_PHM_NoiseParams, settings.NoiseStrengthMultiplier);
		return true;
	}

	//! Loaded once and reused. Reloaded only if the configured path changes.
	protected bool EnsureNoiseParams(PHM_Settings settings)
	{
		if (m_PHM_NoiseParams && m_PHM_LoadedNoisePath == settings.NoiseConfigPath)
			return true;

		if (settings.NoiseConfigPath == "")
			return false;

		m_PHM_NoiseParams = new NoiseParams();
		m_PHM_NoiseParams.LoadFromPath(settings.NoiseConfigPath);
		m_PHM_LoadedNoisePath = settings.NoiseConfigPath;

		PHM_Logger.Notice("Loaded noise params from config path: " + settings.NoiseConfigPath);
		return true;
	}
}
