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

		PHM_Logger.Notice("Settings file: " + PHM_Constants.SETTINGS_FILE);

		DayZGame dayzGame = DayZGame.Cast(g_Game);
		if (dayzGame)
			dayzGame.Event_OnRPC.Insert(PHM_OnRPC);

		PHM_StartDebugTimer(settings);
	}

	void ~MissionServer()
	{
		if (!g_Game)
			return;

		DayZGame dayzGame = DayZGame.Cast(g_Game);
		if (dayzGame)
			dayzGame.Event_OnRPC.Remove(PHM_OnRPC);

		if (!m_PHM_DebugTimerActive)
			return;

		ScriptCallQueue queue = g_Game.GetCallQueue(CALL_CATEGORY_SYSTEM);
		if (queue)
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
