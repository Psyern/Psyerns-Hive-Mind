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
		PHM_HiveManager.PHM_ForgetZombie(this);

		if (!s_PHM_Registry)
			return;

		int index = s_PHM_Registry.Find(this);
		if (index >= 0)
			s_PHM_Registry.Remove(index);
	}
}
