//! Server entry point.
//!
//! The hive itself needs no tick: sending is edge driven from the mind state
//! change and every lifetime is an absolute timestamp compared lazily. The only
//! repeating timer here belongs to the ADMIN DEBUG MAP, and its callback returns
//! immediately while no admin is watching.
//!
//! Native RPC is received through DayZGame.Event_OnRPC (DayZGame.c:970/:3089),
//! the same invoker vanilla uses in SyncEvents.c:7-11 - no modded DayZGame needed.
modded class MissionServer
{
	protected bool m_PHM_DebugTimerActive;
	protected bool m_PHM_ProbeTimerActive;
	protected ref array<string> m_PHM_WarnedIds;

	override void OnInit()
	{
		super.OnInit();

		if (!g_Game)
			return;

		if (!g_Game.IsDedicatedServer())
			return;

		PHM_SettingsHolder.Load();

		PHM_Settings settings = PHM_SettingsHolder.Get();
		if (!settings)
		{
			PHM_Logger.Error("Settings unavailable after load, hive mind stays inactive.");
			return;
		}

		PHM_Logger.Notice("Psyerns Hive Mind " + PHM_Constants.VERSION + " loaded.");

		string line = "Enabled=" + settings.Enabled.ToString();
		line = line + " ShareRadius=" + settings.ShareRadius.ToString();
		line = line + " MaxSharedZombies=" + settings.MaxSharedZombies.ToString();
		PHM_Logger.Notice(line);

		line = "ActiveTimeWindow=" + settings.ActiveTimeWindow.ToString();
		line = line + " TriggerLevel=" + settings.TriggerLevel.ToString();
		line = line + " MaxRelayGenerations=" + settings.MaxRelayGenerations.ToString();
		PHM_Logger.Notice(line);

		line = "VisionRangeMultiplier=" + settings.VisionRangeMultiplier.ToString();
		line = line + " EnableNoisePing=" + settings.EnableNoisePing.ToString();
		PHM_Logger.Notice(line);

		line = "EnablePursuit=" + settings.EnablePursuit.ToString();
		line = line + " PursuitSpeed=" + settings.PursuitSpeed.ToString();
		line = line + " PursuitRepathSeconds=" + settings.PursuitRepathSeconds.ToString();
		line = line + " PursuitMaxDistance=" + settings.PursuitMaxDistance.ToString();
		PHM_Logger.Notice(line);

		if (!settings.EnablePursuit)
			PHM_Logger.Notice("Pursuit is OFF - marks only boost vision range (needs line of sight) and emit a noise ping. Marked infected beyond both ranges will not move.");

		line = "EnableMotorProbe=" + settings.EnableMotorProbe.ToString();
		line = line + " ProbeSpeed=" + settings.ProbeSpeed.ToString();
		line = line + " ProbeTurnMode=" + settings.ProbeTurnMode.ToString();
		line = line + " ProbeTurnType=" + settings.ProbeTurnType.ToString();
		line = line + " ProbeDurationSeconds=" + settings.ProbeDurationSeconds.ToString();
		PHM_Logger.Notice(line);

		//! Warn, not Notice: this is the one setting in the file that takes an
		//! infected away from the engine entirely. Possession suspends the native
		//! brain (AIAgent.SetKeepInIdle(true)) and is a PINNED state, so an
		//! operator who left it on by accident must be able to see that in the RPT
		//! without reading the settings dump line above.
		if (settings.EnableMotorProbe)
			PHM_Logger.Warn("MOTOR PROBE IS ON. One nearby infected will be POSSESSED periodically - its native AI is suspended and this mod drives it instead. Diagnostic for build step 0 only, not gameplay. Turn it off on a live server.");

		PHM_Logger.Notice("Settings file: " + PHM_Constants.SETTINGS_FILE);

		DayZGame dayzGame = DayZGame.Cast(g_Game);
		if (dayzGame)
			dayzGame.Event_OnRPC.Insert(PHM_OnRPC);

		PHM_StartDebugTimer(settings);

		//! Started unconditionally, NOT gated on EnableMotorProbe. The settings
		//! holder hot reloads (PHM_SettingsHolder.ReloadIfDue), so the switch can
		//! be flipped either way without a restart - and the OFF path of the tick
		//! is what releases an infected that was mid-possession when it flipped.
		//! The tick itself costs one settings read while the probe is off.
		PHM_StartProbeTimer();
	}

	void ~MissionServer()
	{
		if (!g_Game)
			return;

		DayZGame dayzGame = DayZGame.Cast(g_Game);
		if (dayzGame)
			dayzGame.Event_OnRPC.Remove(PHM_OnRPC);

		//! Release BEFORE any of the timer bookkeeping below, because every branch
		//! down there can take an early return. Possession is a pinned state: an
		//! infected still held in SetKeepInIdle(true) when the mission tears down
		//! stays paralysed for the rest of its life. GetExisting on purpose - the
		//! destructor must never construct the probe.
		PHM_Probe probe = PHM_Probe.GetExisting();
		if (probe)
			probe.ReleaseAll();

		ScriptCallQueue queue = g_Game.GetCallQueue(CALL_CATEGORY_SYSTEM);
		if (!queue)
			return;

		if (m_PHM_ProbeTimerActive)
			queue.Remove(PHM_ProbeTick);

		if (m_PHM_DebugTimerActive)
			queue.Remove(PHM_PushDebugSnapshots);
	}

	//! CALL_CATEGORY_SYSTEM ticks unconditionally, GAMEPLAY would stall while the
	//! mission is paused. Same pair vanilla uses in missionServer.c.
	protected void PHM_StartDebugTimer(PHM_Settings settings)
	{
		if (!g_Game)
			return;

		ScriptCallQueue queue = g_Game.GetCallQueue(CALL_CATEGORY_SYSTEM);
		if (!queue)
			return;

		int interval = settings.DebugMapIntervalSeconds * 1000;
		if (interval < 250)
			interval = 250;

		queue.CallLater(PHM_PushDebugSnapshots, interval, true);
		m_PHM_DebugTimerActive = true;
	}

	//! Same shape and same queue as PHM_StartDebugTimer above. The 1 s period is
	//! fixed and not a setting: ProbeDurationSeconds / ProbeCooldownSeconds are
	//! compared against g_Game.GetTickTime() inside the probe, so the timer only
	//! decides how coarsely those deadlines are noticed. The per-tick DRIVING runs
	//! at command-tick rate inside the zombie, not here.
	protected void PHM_StartProbeTimer()
	{
		if (!g_Game)
			return;

		ScriptCallQueue queue = g_Game.GetCallQueue(CALL_CATEGORY_SYSTEM);
		if (!queue)
			return;

		queue.CallLater(PHM_ProbeTick, 1000, true);
		m_PHM_ProbeTimerActive = true;
	}

	protected void PHM_ProbeTick()
	{
		if (!g_Game)
			return;

		if (!g_Game.IsDedicatedServer())
			return;

		//! The ONLY other call site is PHM_HiveManager.c:139, inside Broadcast() -
		//! which fires only when a marked infected crosses the mind state threshold
		//! on a player, and therefore never fires at all in the clean-room setup
		//! step 0 has to be run in (Enabled=false). Without this line OnInit's Load()
		//! would be the only one that ever runs: the config-flip release path below
		//! would be dead, and every sweep configuration - two speed scales, two turn
		//! modes, several turn types - would need a full server restart instead of a
		//! JSON edit. An operator editing HiveMind.json and waiting would conclude
		//! the probe is broken.
		PHM_SettingsHolder.ReloadIfDue();

		PHM_Settings settings = PHM_SettingsHolder.Get();
		if (!settings)
			return;

		if (!settings.EnableMotorProbe)
		{
			//! GetExisting, so a server that never enables the probe never even
			//! constructs it. But the off path still has to release: if the admin
			//! hot reloaded the switch to false while an infected was possessed,
			//! this tick is the last code that will ever look at it.
			PHM_Probe existing = PHM_Probe.GetExisting();
			if (existing)
				existing.ReleaseAll();

			return;
		}

		PHM_Probe probe = PHM_Probe.GetInstance();
		if (!probe)
			return;

		probe.OnServerTick();
	}

	//! Server side RPC receiver. Only handles the subscribe request.
	void PHM_OnRPC(PlayerIdentity sender, Object target, int rpc_type, ParamsReadContext ctx)
	{
		if (rpc_type != EPHM_RPC.PHM_RPC_DEBUG_SUBSCRIBE)
			return;

		if (!g_Game)
			return;

		if (!g_Game.IsDedicatedServer())
			return;

		if (!sender)
			return;

		Param1<bool> data = new Param1<bool>(false);
		if (!ctx.Read(data))
			return;

		PHM_Settings settings = PHM_SettingsHolder.Get();
		if (!settings)
			return;

		//! Bail out BEFORE touching the log when the feature is off. This handler is
		//! reachable by any connected client, and PrintToRPT fflushes on every write
		//! ("performance warning - each write means fflush", EnDebug.c:98) - logging
		//! here unconditionally would let a client drive unbounded synchronous disk
		//! writes on the server. DebugMapEnabled is false by default, so this is the
		//! normal path on every production server.
		if (!settings.DebugMapEnabled)
			return;

		string steamId = sender.GetPlainId();

		//! Authority check happens HERE, on the server. The client asking nicely is
		//! never enough.
		if (!settings.IsDebugAdmin(steamId))
		{
			PHM_WarnRejectedOnce(steamId);
			return;
		}

		PHM_DebugTracker tracker = PHM_DebugTracker.GetInstance();
		if (!tracker)
			return;

		if (data.param1)
			tracker.Subscribe(steamId);
		else
			tracker.Unsubscribe(steamId);
	}

	//! One warning per Steam id, with a hard ceiling on how many ids are remembered.
	//! Even with the debug map switched on, a client spamming the subscribe RPC can
	//! therefore never turn the RPT into a write amplifier.
	protected void PHM_WarnRejectedOnce(string steamId)
	{
		if (!m_PHM_WarnedIds)
			m_PHM_WarnedIds = new array<string>;

		if (m_PHM_WarnedIds.Find(steamId) >= 0)
			return;

		if (m_PHM_WarnedIds.Count() >= PHM_Constants.DEBUG_WARN_IDS_MAX)
			return;

		m_PHM_WarnedIds.Insert(steamId);
		PHM_Logger.Warn("Rejected debug map request from non-admin " + steamId);
	}

	protected void PHM_PushDebugSnapshots()
	{
		if (!g_Game)
			return;

		if (!g_Game.IsDedicatedServer())
			return;

		PHM_Settings settings = PHM_SettingsHolder.Get();
		if (!settings)
			return;

		if (!settings.DebugMapEnabled)
			return;

		//! GetRecorder on purpose: the push loop must not create the tracker. And
		//! snapshots are only BUILT while somebody actually watches - recording
		//! history is independent of this and happens on the broadcast path.
		PHM_DebugTracker tracker = PHM_DebugTracker.GetRecorder();
		if (!tracker)
			return;

		if (!tracker.HasSubscribers())
			return;

		PHM_DebugSnapshot snapshot = tracker.BuildSnapshot();
		if (!snapshot)
			return;

		if (!m_Players)
			return;

		int count = m_Players.Count();
		int index;
		Man man;
		PlayerIdentity identity;
		string steamId;

		for (index = 0; index < count; index++)
		{
			man = m_Players.Get(index);
			if (!man)
				continue;

			identity = man.GetIdentity();
			if (!identity)
				continue;

			steamId = identity.GetPlainId();

			//! Re-checked every push, so revoking an admin in the JSON takes effect
			//! on the next tick without a restart.
			if (!settings.IsDebugAdmin(steamId))
			{
				tracker.Unsubscribe(steamId);
				continue;
			}

			if (!tracker.IsSubscribed(steamId))
				continue;

			Param1<PHM_DebugSnapshot> param = new Param1<PHM_DebugSnapshot>(snapshot);
			g_Game.RPCSingleParam(null, EPHM_RPC.PHM_RPC_DEBUG_SNAPSHOT, param, true, identity);
		}
	}
}
