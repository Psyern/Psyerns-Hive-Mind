//! Carries the infected registry, the per zombie hive state and the event
//! trigger.
//!
//! There is no periodic tick anywhere in this mod. All lifetimes are absolute
//! timestamps taken from g_Game.GetTickTime() and compared lazily. That is
//! deliberate: a countdown driven from ModCommandHandlerBefore would silently
//! stop whenever another mod returns true from that hook without calling super
//! (DayZExpansion Core ZombieBase.c:144-158 does exactly that for lobotomised
//! infected), leaving a zombie permanently hive alerted.
modded class ZombieBase
{
	//! Weak elements on purpose. array<ref ZombieBase> would keep every infected
	//! alive forever and the destructor below would never run.
	static ref array<ZombieBase> s_PHM_Registry = new array<ZombieBase>;

	protected float m_PHM_HiveUntil;
	protected float m_PHM_RelayUntil;
	protected float m_PHM_SendReadyAt;
	protected int m_PHM_Hop;
	protected int m_PHM_PrevMindState;

	//! Ladder climb bookkeeping. ReadyAt > 0 means the zombie is "climbing"
	//! (armed, waiting at the base); CooldownUntil throttles repeat climbs.
	protected float m_PHM_ClimbReadyAt;
	protected float m_PHM_ClimbCooldownUntil;

	//! Door opening bookkeeping, same state machine as the climb.
	protected float m_PHM_DoorReadyAt;
	protected float m_PHM_DoorCooldownUntil;

	//! Navmesh pursuit state. The waypoint array is allocated lazily on the first
	//! pursuit, so the overwhelming majority of infected - the ones that are
	//! never marked - never pay for it.
	protected ref array<vector> m_PHM_Path;
	protected int m_PHM_PathIndex;
	protected float m_PHM_RepathAt;

	//! True while this infected's heading and speed are being written by the mod.
	//! Also the flag that guarantees the overrides get released exactly once.
	protected bool m_PHM_Driving;

	//! True while the step 0 motor probe owns this infected outright: its native
	//! brain is suspended with AIAgent.SetKeepInIdle(true) (the same possession
	//! vanilla's own tool performs, PluginDayZInfectedDebug.c:368) and PHM_Probe
	//! writes the input controller instead. Owned by PHM_Probe via
	//! PHM_SetProbeActive - nothing else in this file may set it.
	protected bool m_PHM_ProbeActive;

	void ZombieBase()
	{
		m_PHM_HiveUntil = 0.0;
		m_PHM_RelayUntil = 0.0;
		m_PHM_SendReadyAt = 0.0;
		m_PHM_Hop = 0;
		m_PHM_ClimbReadyAt = 0.0;
		m_PHM_ClimbCooldownUntil = 0.0;
		m_PHM_DoorReadyAt = 0.0;
		m_PHM_DoorCooldownUntil = 0.0;
		m_PHM_PathIndex = 0;
		m_PHM_RepathAt = 0.0;
		m_PHM_Driving = false;
		m_PHM_ProbeActive = false;

		//! Matches vanilla m_LastMindState / m_MindState, which both start at -1.
		//! 0 would be a value the mind state range never produces.
		m_PHM_PrevMindState = -1;

		if (!g_Game)
			return;

		if (!g_Game.IsDedicatedServer())
			return;

		if (!s_PHM_Registry)
			return;

		s_PHM_Registry.Insert(this);
	}

	void ~ZombieBase()
	{
		PHM_Unregister();
	}

	//! Whether EEDelete actually fires for DayZCreature derivatives is not
	//! provable from the script sources (no vanilla or Expansion creature class
	//! overrides it), so unregistering happens here AND in the destructor. Both
	//! paths are idempotent.
	override void EEDelete(EntityAI parent)
	{
		super.EEDelete(parent);

		PHM_Unregister();
	}

	static array<ZombieBase> PHM_GetRegistry()
	{
		return s_PHM_Registry;
	}

	bool PHM_IsHiveAlerted()
	{
		if (!g_Game)
			return false;

		return g_Game.GetTickTime() < m_PHM_HiveUntil;
	}

	//! Effective relay depth. Reports 0 once the relay memory has run out, so an
	//! expired chain cannot resurrect a stale hop.
	int PHM_GetHop()
	{
		if (!g_Game)
			return 0;

		if (g_Game.GetTickTime() >= m_PHM_RelayUntil)
			return 0;

		return m_PHM_Hop;
	}

	//! The player this infected is actively targeting right now, or null.
	//! Reading the input controller outside CommandHandler has vanilla precedent:
	//! the engine does exactly that during perception (AITargetCallbacksPlayer.c:24-33).
	PlayerBase PHM_GetActiveTargetPlayer()
	{
		if (IsSetForDeletion())
			return null;

		if (!IsAlive())
			return null;

		DayZInfectedInputController controller = GetInputController();
		if (!controller)
			return null;

		EntityAI target = controller.GetTargetEntity();
		if (!target)
			return null;

		PlayerBase player = PlayerBase.Cast(target);
		if (!player)
			return null;

		if (!player.CanBeTargetedByAI(this))
			return null;

		return player;
	}

	//! Experimental: force the engine alert level while marked. Signature verified
	//! (DayZCreatureAIInputController.c:21), parameter SEMANTICS are not - which is
	//! why level/inLevel come from config and the feature defaults to off.
	void PHM_ApplyAlertOverride(int level, float inLevel)
	{
		if (!g_Game)
			return;

		if (!g_Game.IsDedicatedServer())
			return;

		if (IsSetForDeletion())
			return;

		if (!IsAlive())
			return;

		DayZInfectedInputController controller = GetInputController();
		if (!controller)
			return;

		controller.OverrideAlertLevel(true, true, level, inLevel);
	}

	//! Releases the override. Called unconditionally when a zombie leaves the
	//! marked set - releasing a never-set override is harmless (state=false is the
	//! documented release form, same shape Expansion uses for its overrides).
	void PHM_ReleaseAlertOverride()
	{
		if (!g_Game)
			return;

		if (!g_Game.IsDedicatedServer())
			return;

		if (IsSetForDeletion())
			return;

		if (!IsAlive())
			return;

		DayZInfectedInputController controller = GetInputController();
		if (!controller)
			return;

		controller.OverrideAlertLevel(false, false, 0, 0.0);
	}

	bool PHM_ClimbOnCooldown(float now)
	{
		return now < m_PHM_ClimbCooldownUntil;
	}

	float PHM_GetClimbReadyAt()
	{
		return m_PHM_ClimbReadyAt;
	}

	void PHM_ArmClimb(float readyAt)
	{
		m_PHM_ClimbReadyAt = readyAt;
	}

	void PHM_DisarmClimb()
	{
		m_PHM_ClimbReadyAt = 0.0;
	}

	void PHM_FinishClimb(float cooldownUntil)
	{
		m_PHM_ClimbReadyAt = 0.0;
		m_PHM_ClimbCooldownUntil = cooldownUntil;
	}

	//! Diagnosis only: readable alert state of this infected, so the RPT shows
	//! whether the experimental override actually sticks.
	string PHM_GetAlertDebug()
	{
		DayZInfectedInputController controller = GetInputController();
		if (!controller)
			return "noctrl";

		int level = controller.GetAlertLevel();
		float inLevel = controller.GetAlertInLevel();
		bool alerted = controller.IsAlerted();
		return "lvl=" + level.ToString() + " in=" + inLevel.ToString() + " alerted=" + alerted.ToString();
	}

	bool PHM_DoorOnCooldown(float now)
	{
		return now < m_PHM_DoorCooldownUntil;
	}

	float PHM_GetDoorReadyAt()
	{
		return m_PHM_DoorReadyAt;
	}

	void PHM_ArmDoor(float readyAt)
	{
		m_PHM_DoorReadyAt = readyAt;
	}

	void PHM_DisarmDoor()
	{
		m_PHM_DoorReadyAt = 0.0;
	}

	void PHM_FinishDoor(float cooldownUntil)
	{
		m_PHM_DoorReadyAt = 0.0;
		m_PHM_DoorCooldownUntil = cooldownUntil;
	}

	//! Seconds of vision boost left. Debug map only.
	float PHM_GetHiveRemaining()
	{
		if (!g_Game)
			return 0.0;

		float remaining = m_PHM_HiveUntil - g_Game.GetTickTime();
		if (remaining < 0.0)
			return 0.0;

		return remaining;
	}

	void PHM_ApplyHiveAlert(float boostSeconds, float relaySeconds, int hop)
	{
		if (!g_Game)
			return;

		float now = g_Game.GetTickTime();

		//! Read the effective hop before extending the relay window, otherwise a
		//! stale hop depth from an expired chain would be resurrected.
		int effectiveHop = 0;
		if (now < m_PHM_RelayUntil)
			effectiveHop = m_PHM_Hop;

		//! Take the maximum, never the incoming value. A zombie that already sits
		//! deep in a chain must not be rejuvenated to a shallower hop by a second
		//! message, which would let it escape the relay limit.
		if (hop > effectiveHop)
			effectiveHop = hop;

		m_PHM_Hop = effectiveHop;

		float boostUntil = now + boostSeconds;
		if (boostUntil > m_PHM_HiveUntil)
			m_PHM_HiveUntil = boostUntil;

		float relayUntil = now + relaySeconds;
		if (relayUntil > m_PHM_RelayUntil)
			m_PHM_RelayUntil = relayUntil;
	}

	//! Per tick entry point for the navmesh pursuit.
	//!
	//! CommandHandler, not ModCommandHandlerBefore/Inside/After: those three are
	//! all skipped whenever an earlier branch of the vanilla handler returns, and
	//! Before is additionally hijacked outright by DayZExpansion Core for
	//! lobotomised infected (ZombieBase.c:144-158, returns true without calling
	//! super). Overriding CommandHandler and calling super FIRST runs regardless
	//! of which branch super took, and is exactly what Expansion Core itself does
	//! (DayZExpansion_Core Creatures/Infected/ZombieBase.c:165).
	override void CommandHandler(float pDt, int pCurrentCommandID, bool pCurrentCommandFinished)
	{
		super.CommandHandler(pDt, pCurrentCommandID, pCurrentCommandFinished);

		//! Possession pre-empts pursuit, and the return is the whole point: the
		//! probe and PHM_DriveTowards are two writers on ONE input controller
		//! (both call OverrideMovementSpeed), and a contaminated speed channel is
		//! exactly the measurement step 0 exists to take cleanly. One plain field
		//! read for the 99.9% that are never probed - same shape as the pursuit
		//! gate below.
		if (m_PHM_ProbeActive)
		{
			PHM_Probe probe = PHM_Probe.GetExisting();
			if (probe)
			{
				probe.DriveTick(this, pDt);
				return;
			}

			//! Flag set but the singleton is gone. Should be unreachable - the
			//! probe's own teardown calls ReleaseAll() before dropping itself,
			//! and that clears this flag - so getting here means the mod side
			//! state outlived its owner. Clear it rather than keep returning
			//! forever with nobody left to drive.
			m_PHM_ProbeActive = false;
		}

		PHM_PursuitTick(pDt);
	}

	//! Handover between the two motor channels. Taking the probe means dropping
	//! pursuit FIRST: PHM_PursuitTick never runs again while the flag is up (the
	//! branch above returns), so a zombie grabbed mid-pursuit would keep
	//! m_PHM_Driving true and its OverrideHeading/OverrideTurnSpeed pinned from
	//! the last pursuit tick, silently fighting every value the probe writes.
	//! PHM_StopDriving is idempotent, so this costs one field read when the
	//! zombie was not driving.
	void PHM_SetProbeActive(bool active)
	{
		if (active)
			PHM_StopDriving();

		m_PHM_ProbeActive = active;
	}

	bool PHM_IsProbeActive()
	{
		return m_PHM_ProbeActive;
	}

	//! Drives a marked infected along a navmesh route towards the hive's pursuit
	//! target. This is the only channel in the mod that can actually MOVE an
	//! infected: the vision multiplier needs line of sight and the noise ping
	//! reaches only as far as its noise config allows, so without this a mark
	//! beyond either range was silently inert.
	protected void PHM_PursuitTick(float pDt)
	{
		//! The gate that makes this hook affordable: two plain field reads, no
		//! call. An infected that was never marked and is not currently being
		//! driven leaves on this line, every tick, forever.
		if (!m_PHM_Driving && m_PHM_HiveUntil <= 0.0)
			return;

		if (!g_Game)
			return;

		if (!g_Game.IsDedicatedServer())
			return;

		if (IsSetForDeletion())
			return;

		if (!IsAlive())
		{
			PHM_StopDriving();
			return;
		}

		float now = g_Game.GetTickTime();

		//! Mark expired - hand the infected back to vanilla untouched.
		//!
		//! Checked BEFORE the feature switches on purpose. m_PHM_HiveUntil is the
		//! cheap gate at the top of this function, and it has to be zeroed on
		//! expiry or every infected that was EVER marked keeps paying for the
		//! full chain on every tick for the rest of its life. Putting this behind
		//! the EnablePursuit check would skip the zeroing entirely on the default
		//! configuration, where pursuit is off - i.e. exactly where the saving
		//! matters most. Zero reads as "expired" to PHM_IsHiveAlerted and
		//! PHM_GetHiveRemaining alike, and PHM_ApplyHiveAlert only ever takes the
		//! maximum, so a later re-mark is unaffected.
		if (now >= m_PHM_HiveUntil)
		{
			PHM_StopDriving();
			m_PHM_HiveUntil = 0.0;
			return;
		}

		PHM_Settings settings = PHM_SettingsHolder.Get();
		if (!settings)
		{
			PHM_StopDriving();
			return;
		}

		if (!settings.Enabled)
		{
			PHM_StopDriving();
			return;
		}

		//! Hot reloading the switch off releases every driven infected on its
		//! next tick, without a restart.
		if (!settings.EnablePursuit)
		{
			PHM_StopDriving();
			return;
		}

		DayZInfectedInputController controller = GetInputController();
		if (!controller)
			return;

		//! It can see the player by itself now. The vanilla chase is strictly
		//! better than anything a scripted heading can steer, so release the
		//! overrides and get out of its way.
		if (controller.GetTargetEntity())
		{
			PHM_StopDriving();
			return;
		}

		PHM_HiveManager manager = PHM_HiveManager.GetInstance();
		if (!manager)
		{
			PHM_StopDriving();
			return;
		}

		vector destination;
		if (!manager.PHM_GetPursuitTarget(now, destination))
		{
			PHM_StopDriving();
			return;
		}

		vector position = GetPosition();

		float distance = vector.Distance(position, destination);
		if (distance > settings.PursuitMaxDistance)
		{
			PHM_StopDriving();
			return;
		}

		if (!PHM_EnsurePath(manager, now, position, destination, settings))
		{
			PHM_StopDriving();
			return;
		}

		//! Route fully consumed: standing on the last known position. Vanilla
		//! perception takes over from here, which is the intended hand-off.
		if (!PHM_AdvanceWaypoint(position, settings))
		{
			PHM_StopDriving();
			return;
		}

		vector waypoint = m_PHM_Path.Get(m_PHM_PathIndex);
		PHM_DriveTowards(controller, position, waypoint, pDt, settings);
	}

	//! Returns true when a usable route exists. Recomputes at most once per
	//! PursuitRepathSeconds, and backs off harder after a failed search so an
	//! infected with no route at all cannot call FindPath every tick.
	protected bool PHM_EnsurePath(PHM_HiveManager manager, float now, vector position, vector destination, PHM_Settings settings)
	{
		bool hasRoute = false;
		if (m_PHM_Path && m_PHM_PathIndex < m_PHM_Path.Count())
			hasRoute = true;

		if (now < m_PHM_RepathAt)
			return hasRoute;

		if (!m_PHM_Path)
			m_PHM_Path = new array<vector>;

		bool found = manager.PHM_FindPursuitPath(position, destination, m_PHM_Path);

		//! Jitter on purpose: 60 marked infected sharing one repath cadence would
		//! otherwise put 60 FindPath calls into the same frame.
		float jitter = Math.RandomFloat(0.0, PHM_Constants.PURSUIT_REPATH_JITTER);

		if (!found)
		{
			m_PHM_Path.Clear();
			m_PHM_PathIndex = 0;
			m_PHM_RepathAt = now + PHM_Constants.PURSUIT_FAILED_REPATH_SECONDS + jitter;
			return false;
		}

		m_PHM_PathIndex = 0;
		m_PHM_RepathAt = now + settings.PursuitRepathSeconds + jitter;
		return true;
	}

	//! Skips every waypoint already reached. Returns false once the route is
	//! exhausted.
	protected bool PHM_AdvanceWaypoint(vector position, PHM_Settings settings)
	{
		if (!m_PHM_Path)
			return false;

		int count = m_PHM_Path.Count();
		float radius = settings.PursuitWaypointRadius;

		//! Compared on the ground plane only. FindPath returns the standing
		//! position as its first waypoint and puts points on stairs and ladders
		//! at the same XZ as the floor below, so a 3D compare would leave a
		//! zombie steering at a point directly above its own head.
		vector flatPosition = Vector(position[0], 0.0, position[2]);
		vector waypoint;
		vector flatWaypoint;
		float distance;

		while (m_PHM_PathIndex < count)
		{
			waypoint = m_PHM_Path.Get(m_PHM_PathIndex);
			flatWaypoint = Vector(waypoint[0], 0.0, waypoint[2]);
			distance = vector.Distance(flatPosition, flatWaypoint);

			if (distance > radius)
				return true;

			m_PHM_PathIndex = m_PHM_PathIndex + 1;
		}

		return false;
	}

	//! Writes heading and speed into the input controller for this tick.
	protected void PHM_DriveTowards(DayZInfectedInputController controller, vector position, vector waypoint, float pDt, PHM_Settings settings)
	{
		if (pDt <= 0.0)
			return;

		vector direction = waypoint - position;
		direction[1] = 0.0;

		if (direction.Length() < 0.01)
			return;

		//! VectorToAngles()[0] is the yaw in degrees, the same space
		//! GetOrientation()[0] lives in - and degrees-to-radians is exactly the
		//! conversion vanilla applies before handing a heading to a script driven
		//! creature (DayZAnimal.c:465 builds it, :550 converts it).
		vector angles = direction.VectorToAngles();
		float headingDeg = Math.NormalizeAngle(angles[0]);
		float headingRad = headingDeg * Math.DEG2RAD;

		//! Same instant-turn form vanilla uses in that block (DayZAnimal.c:548).
		float turnSpeed = Math.PI2 / pDt;

		controller.OverrideTurnSpeed(true, turnSpeed);
		controller.OverrideHeading(true, headingRad);
		controller.OverrideMovementSpeed(true, settings.PursuitSpeed);

		m_PHM_Driving = true;
	}

	//! Releases the movement overrides. Idempotent, and called from every exit
	//! path above - leaving heading and speed pinned would freeze an infected
	//! permanently, which is far worse than never having driven it at all.
	void PHM_StopDriving()
	{
		if (!m_PHM_Driving)
			return;

		m_PHM_Driving = false;
		m_PHM_PathIndex = 0;

		if (m_PHM_Path)
			m_PHM_Path.Clear();

		if (!g_Game)
			return;

		if (IsSetForDeletion())
			return;

		DayZInfectedInputController controller = GetInputController();
		if (!controller)
			return;

		//! state=false is the release form; the value argument is ignored.
		controller.OverrideTurnSpeed(false, 0.0);
		controller.OverrideHeading(false, 0.0);
		controller.OverrideMovementSpeed(false, 0.0);
	}

	bool PHM_IsDriving()
	{
		return m_PHM_Driving;
	}

	//! Diagnosis only: pursuit state of this infected for the RPT telemetry.
	string PHM_GetPursuitDebug()
	{
		int waypoints = 0;
		if (m_PHM_Path)
			waypoints = m_PHM_Path.Count();

		string info = "driving=" + m_PHM_Driving.ToString();
		info = info + " wp=" + m_PHM_PathIndex.ToString() + "/" + waypoints.ToString();
		return info;
	}

	override bool HandleMindStateChange(int pCurrentCommandID, DayZInfectedInputController pInputController, float pDt)
	{
		//! The edge is tracked on an own member, not on vanilla m_LastMindState:
		//! super overwrites that one before returning (ZombieBase.c:475), so it is
		//! already gone by the time this override could read it. Plain field read,
		//! no call, so the "super first" rule is untouched.
		int previousMindState = m_PHM_PrevMindState;

		bool result = super.HandleMindStateChange(pCurrentCommandID, pInputController, pDt);

		if (!g_Game)
			return result;

		if (!g_Game.IsDedicatedServer())
			return result;

		if (!pInputController)
			return result;

		PHM_Settings settings = PHM_SettingsHolder.Get();
		if (!settings)
			return result;

		if (!settings.Enabled)
			return result;

		//! Server side truth. GetMindStateSynced() must never be used here: vanilla
		//! registers m_MindState with RegisterNetSyncVariableInt(-1, 4) while the
		//! actual values run 7..11, so the replicated value is unusable.
		int currentMindState = pInputController.GetMindState();
		int threshold = PHM_ThresholdMindState(settings.TriggerLevel);

		if (currentMindState < threshold)
		{
			//! Back below the threshold, re-arm for the next rising edge.
			m_PHM_PrevMindState = currentMindState;
			return result;
		}

		if (previousMindState >= threshold)
			return result;

		//! Rising edge across the threshold. The edge is only consumed when a
		//! broadcast really happened, otherwise a rejected attempt would silence
		//! this zombie for the rest of the chase.
		bool sent = PHM_TryBroadcast(pInputController, settings);
		if (sent)
			m_PHM_PrevMindState = currentMindState;

		return result;
	}

	protected int PHM_ThresholdMindState(int triggerLevel)
	{
		if (triggerLevel == EPHM_TriggerLevel.DISTURBED)
			return DayZInfectedConstants.MINDSTATE_DISTURBED;

		if (triggerLevel == EPHM_TriggerLevel.ALERTED)
			return DayZInfectedConstants.MINDSTATE_ALERTED;

		if (triggerLevel == EPHM_TriggerLevel.FIGHT)
			return DayZInfectedConstants.MINDSTATE_FIGHT;

		return DayZInfectedConstants.MINDSTATE_CHASE;
	}

	protected bool PHM_TryBroadcast(DayZInfectedInputController pInputController, PHM_Settings settings)
	{
		if (!g_Game)
			return false;

		float now = g_Game.GetTickTime();
		if (now < m_PHM_SendReadyAt)
			return false;

		EntityAI targetEntity = pInputController.GetTargetEntity();
		if (!targetEntity)
		{
			m_PHM_SendReadyAt = now + PHM_Constants.FAILED_RETRY_SECONDS;
			return false;
		}

		PlayerBase seenPlayer = PlayerBase.Cast(targetEntity);
		if (!seenPlayer)
		{
			m_PHM_SendReadyAt = now + PHM_Constants.FAILED_RETRY_SECONDS;
			return false;
		}

		//! A player the AI cannot acquire at all makes the whole broadcast
		//! pointless: unconscious or inside a vehicle (PlayerBase.c:3665).
		if (!seenPlayer.CanBeTargetedByAI(this))
		{
			m_PHM_SendReadyAt = now + PHM_Constants.FAILED_RETRY_SECONDS;
			return false;
		}

		//! Hop 0 means this zombie saw the player organically. Anything else means
		//! it was itself woken by the hive and is now relaying.
		int hop = 0;
		if (now < m_PHM_RelayUntil)
			hop = m_PHM_Hop + 1;

		if (hop > settings.MaxRelayGenerations)
		{
			m_PHM_SendReadyAt = now + PHM_Constants.FAILED_RETRY_SECONDS;
			return false;
		}

		PHM_HiveManager manager = PHM_HiveManager.GetInstance();
		if (!manager)
			return false;

		bool broadcast = manager.Broadcast(this, seenPlayer, hop);

		if (broadcast)
			m_PHM_SendReadyAt = now + settings.SenderCooldownSeconds;
		else
			m_PHM_SendReadyAt = now + PHM_Constants.FAILED_RETRY_SECONDS;

		return broadcast;
	}

	protected void PHM_Unregister()
	{
		//! Possession is a pinned state, so the handle has to die with the entity.
		//! Cleared before the call so the ReleaseAll pass sees a zombie that is
		//! already disowned and cannot loop back in here. ReleaseAll rather than a
		//! per-zombie drop because the probe's frozen surface has no drop-one
		//! entry point, and step 0 possesses one infected at a time - so when THIS
		//! zombie is the subject, all is exactly one. Gated on the flag: an
		//! ordinary infected being deleted must not cancel somebody else's probe.
		if (m_PHM_ProbeActive)
		{
			m_PHM_ProbeActive = false;

			PHM_Probe probe = PHM_Probe.GetExisting();
			if (probe)
				probe.ReleaseAll();
		}

		PHM_HiveManager.PHM_ForgetZombie(this);

		if (!s_PHM_Registry)
			return;

		int index = s_PHM_Registry.Find(this);
		if (index >= 0)
			s_PHM_Registry.Remove(index);
	}
}
