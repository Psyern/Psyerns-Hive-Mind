//! Step 0 motor probe. Diagnostic only, and the only component of the v2 build
//! order that exists to MEASURE rather than to behave.
//!
//! It possesses exactly ONE infected at a time - suspending the native brain with
//! AIAgent.SetKeepInIdle(true), the same call vanilla's own possession tool makes
//! (PluginDayZInfectedDebug.c:368) - drives it in a straight line for
//! ProbeDurationSeconds, then releases it. Per-tick motor writes mirror
//! PluginDayZInfectedDebug.c:385-396 exactly, because that block is the only
//! proof in the whole engine that possession works on an INFECTED rather than on
//! an animal.
//!
//! Four open unknowns from the design doc are answered by grepping ONE RPT for
//! "[PHM]" lines produced here:
//!   1. does GetTargetEntity() stay dead under SetKeepInIdle(true)  -> tgt= / sawTarget=
//!   2. is OverrideMovementSpeed for infected the 0..3 enum or m/s  -> spd= vs vel= vs avg=
//!   3. is StartTurn's direction absolute yaw or a relative delta   -> turnMode= vs yaw=/err=
//!   4. does animation survive multi-minute possession              -> vel= staying non-zero
//!
//! Lives in 4_World, not 3_Game: it references ZombieBase and PlayerBase, which
//! 3_Game is not allowed to see.
//!
//! Possession is a PINNED state. A missed release paralyses an infected until it
//! despawns, which is strictly worse than the measurement never being taken - so
//! every exit path in this file funnels through Release(), Release() is idempotent,
//! and it clears the mod side state BEFORE it touches anything that can fail.
class PHM_Probe
{
	//! ProbeTurnMode ordinals. Named here rather than in PHM_Constants because
	//! they are probe-local and never reach the JSON as anything but an int.
	static const int TURN_MODE_ABSOLUTE = 0;
	static const int TURN_MODE_RELATIVE = 1;

	//! Fixed instead of Math.RandomInt(0, 4) like vanilla spawn does
	//! (ZombieBase.c:96): a probe run has to be reproducible between restarts, and
	//! the stance variation is a cosmetic idle variant with no effect on motion
	//! (the design doc's "the stance system" exclusion).
	static const int STANCE_VARIATION = 0;

	//! 2 is what vanilla passes for MINDSTATE_CHASE (ZombieBase.c:471). The chase
	//! idle is the one with a locomotion gait, which is what has to be observed.
	static const int IDLE_STATE_CHASE = 2;

	//! DriveTick runs at command-tick rate (~50 Hz) and PrintToRPT fflushes on
	//! every write (EnDebug.c:98, and the same rationale is written up at
	//! PHM_MissionServer.c:127-133). Unthrottled telemetry would be ~50 synchronous
	//! disk writes per second for as long as the probe runs.
	static const float TELEMETRY_INTERVAL_SECONDS = 1.0;

	protected static ref PHM_Probe s_PHM_Instance;

	//! WEAK on purpose, exactly like ZombieBase.s_PHM_Registry: a ref here would
	//! keep the possessed infected alive after the world dropped it, and the
	//! entity teardown that calls ReleaseAll would then never run.
	protected ZombieBase m_PHM_Subject;

	//! Flag first, same shape as PHM_StopDriving (PHM_ZombieBase.c:540-545). This
	//! is what makes Release idempotent, and it stays authoritative even when the
	//! subject handle has already gone null underneath us.
	protected bool m_PHM_Active;

	protected vector m_PHM_StartPos;
	protected vector m_PHM_LastPos;
	protected float m_PHM_StartTime;
	protected float m_PHM_StartYaw;
	protected float m_PHM_NextLogAt;
	protected float m_PHM_CooldownUntil;

	//! Captured at possession so Release never has to reach for the settings
	//! holder - it must work on the shutdown path, where load order is not ours.
	protected float m_PHM_CooldownSeconds;

	//! What the motor actually wrote, refreshed every tick from live settings so a
	//! hot reload mid-run cannot make the summary line lie about the run.
	protected float m_PHM_UsedSpeed;
	protected int m_PHM_UsedTurnMode;
	protected int m_PHM_UsedTurnType;
	protected int m_PHM_TurnCount;

	//! Unknown #1. Sampled EVERY tick, not just on the telemetry beat: a single
	//! frame of non-null target is enough to prove the native perception survived
	//! SetKeepInIdle, and a 1 Hz sample would very likely miss it.
	protected bool m_PHM_SawTarget;

	//! Whether the subject already had a target at the moment it was possessed.
	//! sawTarget is only meaningful next to this: see Possess().
	protected bool m_PHM_TargetAtPossess;

	//! True when THIS probe created the current subject. Such a subject is deleted
	//! on release - it exists only for the measurement.
	protected bool m_PHM_Spawned;

	//! Allocated once. GetPlayers is an out-parameter fill (Game.c:947) and this
	//! runs on a repeating timer, so a per-tick allocation would be pure garbage.
	protected ref array<Man> m_PHM_Players;

	void PHM_Probe()
	{
		m_PHM_Players = new array<Man>;
		m_PHM_Active = false;
		m_PHM_StartTime = 0.0;
		m_PHM_StartYaw = 0.0;
		m_PHM_NextLogAt = 0.0;
		m_PHM_CooldownUntil = 0.0;
		m_PHM_CooldownSeconds = 0.0;
		m_PHM_UsedSpeed = 0.0;
		m_PHM_UsedTurnMode = 0;
		m_PHM_UsedTurnType = 0;
		m_PHM_TurnCount = 0;
		m_PHM_SawTarget = false;
		m_PHM_TargetAtPossess = false;
		m_PHM_Spawned = false;
	}

	static PHM_Probe GetInstance()
	{
		if (!s_PHM_Instance)
			s_PHM_Instance = new PHM_Probe();

		return s_PHM_Instance;
	}

	//! Called from ZombieBase.CommandHandler and from entity teardown. Must NOT
	//! create the instance - same contract as PHM_HiveManager.PHM_ForgetZombie
	//! (PHM_HiveManager.c:103-109). Creating a probe from inside a per-tick hook
	//! would mean an infected could bring the measurement rig into existence.
	static PHM_Probe GetExisting()
	{
		if (!s_PHM_Instance)
			return null;

		return s_PHM_Instance;
	}

	//! Repeating server tick, ~1 Hz. Owns the possess/release SCHEDULE only; the
	//! motor writes all live in DriveTick, which is the only place that runs at
	//! command-tick rate and is therefore the only place the engine will accept
	//! them from.
	void OnServerTick()
	{
		if (!g_Game)
			return;

		if (!g_Game.IsDedicatedServer())
			return;

		PHM_Settings settings = PHM_SettingsHolder.Get();
		if (!settings)
		{
			//! Settings gone means the switch cannot be read, and an unreadable
			//! switch must never leave an infected possessed.
			ReleaseAll();
			return;
		}

		//! Hot reloading the switch off releases the subject on the next tick,
		//! without a restart. This is the config-flip exit path the spec demands.
		if (!settings.EnableMotorProbe)
		{
			ReleaseAll();
			return;
		}

		float now = g_Game.GetTickTime();

		if (m_PHM_Active)
		{
			//! Duration read live, not captured, so shortening it in the JSON
			//! cuts a run that is already in flight.
			float elapsed = now - m_PHM_StartTime;
			if (elapsed >= settings.ProbeDurationSeconds)
				Release("duration");

			return;
		}

		if (now < m_PHM_CooldownUntil)
			return;

		ZombieBase subject = FindSubject(settings);

		//! Nothing near a player. With ProbeSelfSpawn the probe creates its own
		//! test infected instead of idling, which is what makes the measurement
		//! possible on a server with nobody connected: FindSubject needs a player
		//! to measure distance from, and the Central Economy will not spawn
		//! infected without player proximity either. Vanilla's own possession tool
		//! does exactly this (PluginDayZInfectedDebug.c:365-366).
		//!
		//! A freshly spawned subject is also the CLEANEST possible sample for
		//! unknown #1: it has existed for one tick, so tgtAtPossess is guaranteed
		//! 0 and sawTarget cannot be contaminated by a carried-in target.
		if (!subject && settings.ProbeSelfSpawn)
			subject = SpawnSubject(settings);

		if (!subject)
		{
			//! No candidate is not an error - nobody is near a player. Back off
			//! for a full cooldown rather than re-walking the registry every tick.
			m_PHM_CooldownUntil = now + settings.ProbeCooldownSeconds;
			return;
		}

		Possess(subject, settings, now);
	}

	//! Creates a throwaway test infected at the configured coordinate and returns
	//! it, or null if the spawn failed.
	//!
	//! ECE_INITAI is what makes the difference between a prop and a working
	//! infected - without it the entity has no AIAgent and SetKeepInIdle would be
	//! called on null. ECE_PLACE_ON_SURFACE corrects the Y so the coordinate only
	//! has to be roughly right. Same flag set vanilla uses at
	//! PluginDayZInfectedDebug.c:365.
	protected ZombieBase SpawnSubject(PHM_Settings settings)
	{
		if (!g_Game)
			return null;

		if (settings.ProbeSpawnType == "")
			return null;

		vector position = Vector(settings.ProbeSpawnX, settings.ProbeSpawnY, settings.ProbeSpawnZ);

		int flags = ECE_PLACE_ON_SURFACE | ECE_INITAI;
		Object created = g_Game.CreateObjectEx(settings.ProbeSpawnType, position, flags);
		if (!created)
		{
			PHM_Logger.Warn("Probe self-spawn failed: CreateObjectEx returned null for '" + settings.ProbeSpawnType + "'. Check the class name.");
			return null;
		}

		ZombieBase spawned = ZombieBase.Cast(created);
		if (!spawned)
		{
			//! Not an infected - delete it again rather than leaving a stray object.
			PHM_Logger.Warn("Probe self-spawn failed: '" + settings.ProbeSpawnType + "' is not a ZombieBase.");
			g_Game.ObjectDelete(created);
			return null;
		}

		//! Without an AI agent there is no brain to suspend and nothing to measure,
		//! so this is a failed spawn rather than a usable subject.
		if (!spawned.GetAIAgent())
		{
			PHM_Logger.Warn("Probe self-spawn failed: spawned infected has no AIAgent (ECE_INITAI did not take).");
			g_Game.ObjectDelete(spawned);
			return null;
		}

		m_PHM_Spawned = true;

		string line = "Probe self-spawned " + settings.ProbeSpawnType;
		line = line + " at " + spawned.GetPosition().ToString();
		PHM_Logger.Notice(line);

		return spawned;
	}

	//! Nearest alive infected to the nearest player, within ProbeSearchRadius.
	//!
	//! Walks the mod's own registry (PHM_ZombieBase.c:88) rather than calling
	//! GetObjectsAtPosition: the registry exists precisely so this mod never
	//! performs an engine space query, which keeps both the radius and the
	//! selection exact and reproducible - and a measurement run that cannot be
	//! reproduced is not a measurement.
	//!
	//! Cost is registry count x player count, once per second, and only while no
	//! run is in flight. That is affordable here because the whole component is a
	//! default-off diagnostic that possesses one infected; it is explicitly NOT the
	//! shape the real navigator gets, which is centrally budgeted per frame.
	protected ZombieBase FindSubject(PHM_Settings settings)
	{
		if (!g_Game)
			return null;

		if (!m_PHM_Players)
			m_PHM_Players = new array<Man>;

		m_PHM_Players.Clear();
		g_Game.GetPlayers(m_PHM_Players);

		int playerCount = m_PHM_Players.Count();
		if (playerCount == 0)
			return null;

		array<ZombieBase> registry = ZombieBase.PHM_GetRegistry();
		if (!registry)
			return null;

		int zombieCount = registry.Count();
		float radius = settings.ProbeSearchRadius;

		//! Two tiers. A candidate that the v1 pursuit is already driving, or that
		//! carries a hive mark, is a SECOND writer on the same input controller -
		//! measuring the motor through one of those would measure the contamination
		//! instead. Preferred, not required: on a server where everything nearby is
		//! marked, a contaminated run flagged in the log still beats no run at all.
		//!
		//! Both distances seed ABOVE the radius, not at it, so a candidate sitting
		//! exactly on the radius still wins the strict less-than below instead of
		//! being silently dropped.
		ZombieBase best = null;
		float bestDistance = radius + 1.0;
		ZombieBase fallback = null;
		float fallbackDistance = radius + 1.0;

		int zombieIndex;
		ZombieBase zombie;
		float nearest;
		bool clean;

		for (zombieIndex = 0; zombieIndex < zombieCount; zombieIndex++)
		{
			zombie = registry.Get(zombieIndex);

			//! Weak registry entries go null when the entity dies - the same guard
			//! ladder CollectCandidates uses (PHM_HiveManager.c:1029-1043).
			if (!zombie)
				continue;

			if (zombie.IsSetForDeletion())
				continue;

			if (!zombie.IsAlive())
				continue;

			//! Without an agent there is no brain to suspend, so possession is a
			//! no-op and the run would measure nothing.
			if (!zombie.GetAIAgent())
				continue;

			//! SetKeepInIdle is NOT probe-exclusive: the backstab finisher sets it
			//! (ZombieBase.c:1056) and OnRecoverFromDeath clears it (:1087). A probe
			//! release landing on a backstabbed infected would re-enable AI
			//! simulation in the middle of the finisher animation.
			if (zombie.IsBeingBackstabbed())
				continue;

			//! Defensive: a stale flag from a previous instance would otherwise let
			//! the probe possess a zombie that already thinks it is possessed.
			if (zombie.PHM_IsProbeActive())
				continue;

			nearest = FindNearestPlayerDistance(zombie, playerCount);
			if (nearest < 0.0)
				continue;

			if (nearest > radius)
				continue;

			clean = true;
			if (zombie.PHM_IsDriving())
				clean = false;
			if (zombie.PHM_IsHiveAlerted())
				clean = false;

			//! An infected that ALREADY has a target is contaminated for unknown #1.
			//! The probe deliberately picks the candidate nearest to a player, which
			//! is exactly the population most likely to be mid-chase - and
			//! m_PHM_SawTarget is a sticky OR sampled from the first driven tick, so
			//! a carried-in target would latch it true and the release summary would
			//! read as "native perception survives SetKeepInIdle". That verdict
			//! deletes the whole Perception layer from the design
			//! (docs/superpowers/specs/2026-08-08-zombie-ai-design.md, unknown 1), so
			//! a false positive here is the most expensive wrong answer this step can
			//! produce. Prefer untargeted subjects; the contaminated case stays
			//! reachable as a fallback rather than starving the probe, and the
			//! tgtAtPossess baseline recorded in Possess() makes every run
			//! self-describing either way.
			DayZInfectedInputController scanController = zombie.GetInputController();
			if (scanController && scanController.GetTargetEntity())
				clean = false;

			if (clean)
			{
				if (nearest < bestDistance)
				{
					best = zombie;
					bestDistance = nearest;
				}
			}
			else if (nearest < fallbackDistance)
			{
				fallback = zombie;
				fallbackDistance = nearest;
			}
		}

		if (best)
			return best;

		if (fallback)
		{
			PHM_Logger.Warn("Probe found only contaminated candidates - subject is hive marked or pursuit driven. Run the measurement with EnablePursuit off.");
			return fallback;
		}

		return null;
	}

	//! Distance from this infected to the closest live player, or -1 when there is
	//! none. Split out of the search loop so the loop body stays readable and so
	//! the player scan has one place to grow guards.
	protected float FindNearestPlayerDistance(ZombieBase zombie, int playerCount)
	{
		vector position = zombie.GetPosition();
		float nearest = -1.0;

		int index;
		Man man;
		PlayerBase player;
		float distance;

		for (index = 0; index < playerCount; index++)
		{
			man = m_PHM_Players.Get(index);
			if (!man)
				continue;

			player = PlayerBase.Cast(man);
			if (!player)
				continue;

			if (!player.IsAlive())
				continue;

			distance = vector.Distance(position, player.GetPosition());

			if (nearest < 0.0)
				nearest = distance;
			else if (distance < nearest)
				nearest = distance;
		}

		return nearest;
	}

	//! Suspends the native brain and records the baseline every telemetry number
	//! below is measured against.
	protected void Possess(ZombieBase zombie, PHM_Settings settings, float now)
	{
		AIAgent agent = zombie.GetAIAgent();
		if (!agent)
			return;

		//! Baseline for unknown #1, sampled BEFORE the brain is suspended. Without
		//! it, sawTarget=1 in the release summary is ambiguous: it could mean the
		//! native perception kept running under SetKeepInIdle, or it could just be a
		//! target the infected already carried in. Only the pair
		//! (tgtAtPossess=0, sawTarget=1) is evidence that perception survived.
		bool hadTarget = false;
		DayZInfectedInputController baselineController = zombie.GetInputController();
		if (baselineController && baselineController.GetTargetEntity())
			hadTarget = true;

		//! BEFORE SetKeepInIdle: PHM_SetProbeActive(true) drops the v1 pursuit and
		//! releases its OverrideHeading/OverrideTurnSpeed (PHM_ZombieBase.c:354-360).
		//! Doing it the other way round would leave a pursuit tick free to write the
		//! controller once more after the brain was already suspended.
		zombie.PHM_SetProbeActive(true);

		agent.SetKeepInIdle(true);

		vector orientation = zombie.GetOrientation();

		m_PHM_Subject = zombie;
		m_PHM_Active = true;
		m_PHM_StartPos = zombie.GetPosition();
		m_PHM_LastPos = m_PHM_StartPos;
		m_PHM_StartTime = now;
		m_PHM_StartYaw = Math.NormalizeAngle(orientation[0]);
		m_PHM_NextLogAt = now;
		m_PHM_CooldownSeconds = settings.ProbeCooldownSeconds;
		m_PHM_UsedSpeed = settings.ProbeSpeed;
		m_PHM_UsedTurnMode = settings.ProbeTurnMode;
		m_PHM_UsedTurnType = settings.ProbeTurnType;
		m_PHM_TurnCount = 0;
		m_PHM_SawTarget = false;
		m_PHM_TargetAtPossess = hadTarget;

		string line = "Probe possess: startYaw=" + m_PHM_StartYaw.ToString();
		line = line + " tgtAtPossess=" + hadTarget.ToString();
		line = line + " bearing=" + settings.ProbeBearing.ToString();
		line = line + " speed=" + settings.ProbeSpeed.ToString();
		line = line + " turnMode=" + settings.ProbeTurnMode.ToString();
		line = line + " turnType=" + settings.ProbeTurnType.ToString();
		line = line + " threshold=" + settings.ProbeTurnThresholdDeg.ToString();
		line = line + " duration=" + settings.ProbeDurationSeconds.ToString();
		PHM_Logger.Notice(line);
	}

	//! Per command tick. Every motor write in the mod happens here and nowhere
	//! else, mirroring PluginDayZInfectedDebug.c:385-396: speed is written EVERY
	//! tick (the override is a per-tick channel, not a latched setting), and the
	//! turn is a DISCRETE command issued only when the angular error crosses the
	//! threshold. OverrideHeading is deliberately absent - it has zero vanilla call
	//! sites for infected and driving it is exactly the v1 mistake this replaces.
	void DriveTick(ZombieBase zombie, float pDt)
	{
		if (!zombie)
			return;

		//! Somebody else's flag. Clear it rather than drive a zombie the probe does
		//! not own, otherwise a stale flag pins that infected forever.
		if (zombie != m_PHM_Subject)
		{
			zombie.PHM_SetProbeActive(false);
			return;
		}

		if (!g_Game)
			return;

		if (!g_Game.IsDedicatedServer())
			return;

		if (zombie.IsSetForDeletion())
		{
			Release("deleted");
			return;
		}

		if (!zombie.IsAlive())
		{
			Release("died");
			return;
		}

		//! The finisher owns SetKeepInIdle from here on (ZombieBase.c:1056). Hand
		//! it over immediately - Release skips the SetKeepInIdle(false) call in this
		//! state so the backstab animation is not interrupted.
		if (zombie.IsBeingBackstabbed())
		{
			Release("backstab");
			return;
		}

		PHM_Settings settings = PHM_SettingsHolder.Get();
		if (!settings)
		{
			Release("nosettings");
			return;
		}

		if (!settings.EnableMotorProbe)
		{
			Release("configoff");
			return;
		}

		DayZInfectedInputController controller = zombie.GetInputController();
		if (!controller)
		{
			Release("noctrl");
			return;
		}

		//! Written unconditionally, every tick, before anything that can bail out.
		controller.OverrideMovementSpeed(true, settings.ProbeSpeed);

		m_PHM_UsedSpeed = settings.ProbeSpeed;
		m_PHM_UsedTurnMode = settings.ProbeTurnMode;
		m_PHM_UsedTurnType = settings.ProbeTurnType;
		m_PHM_LastPos = zombie.GetPosition();

		//! Unknown #1, sampled every tick. One frame of non-null is proof.
		if (controller.GetTargetEntity())
			m_PHM_SawTarget = true;

		//! GetOrientation returns yaw/pitch/roll in DEGREES (Object.c:311), the same
		//! space VectorToAngles()[0] produces - the conversion v1 already solved at
		//! PHM_ZombieBase.c:523-525.
		vector orientation = zombie.GetOrientation();
		float currentYaw = Math.NormalizeAngle(orientation[0]);

		//! Negative bearing is the sentinel for "hold the yaw captured at possession
		//! start" (PHM_Constants.PROBE_BEARING_MIN), which lets the first run measure
		//! translation without also introducing StartTurn as a second variable.
		float desiredYaw = m_PHM_StartYaw;
		if (settings.ProbeBearing >= 0.0)
			desiredYaw = Math.NormalizeAngle(settings.ProbeBearing);

		//! DiffAngle returns the RELATIVE difference angle1 - angle2, already folded
		//! into -180..180 (EnMath.c:170, the doc example gives DiffAngle(-45,45) = -90).
		float yawError = Math.DiffAngle(desiredYaw, currentYaw);
		float absError = Math.AbsFloat(yawError);

		bool turning = false;
		DayZInfectedCommandMove moveCommand = zombie.GetCommand_Move();

		if (moveCommand)
		{
			//! Both are per-tick writes in the reference implementation
			//! (PluginDayZInfectedDebug.c:390-391). Vanilla's own mind state handler
			//! additionally guards SetIdleState with !IsTurning (ZombieBase.c:460),
			//! but the possession path does not, and the possession path is the one
			//! proven to work under SetKeepInIdle.
			moveCommand.SetStanceVariation(STANCE_VARIATION);
			moveCommand.SetIdleState(IDLE_STATE_CHASE);

			turning = moveCommand.IsTurning();

			//! The deadband is what makes a discrete command usable as a steering
			//! channel: without it StartTurn would be re-issued on all ~50 ticks a
			//! second and the turn would never complete.
			if (absError > settings.ProbeTurnThresholdDeg && !turning)
			{
				//! THE ambiguity step 0 exists to resolve. StartTurn's first
				//! parameter is a bare float read off a text field in vanilla
				//! (PluginDayZInfectedDebug.c:396), so neither reading can be ruled
				//! out from source - both must be reachable from config.
				float direction = desiredYaw;
				if (settings.ProbeTurnMode == TURN_MODE_RELATIVE)
					direction = yawError;

				//! Second parameter is declared pSpeedType, not pTurnType
				//! (DayZInfected.c:40). The name is not a typo to be corrected here -
				//! which of the two it really selects is one of the four unknowns,
				//! so the value used is echoed in the release summary.
				moveCommand.StartTurn(direction, settings.ProbeTurnType);
				m_PHM_TurnCount = m_PHM_TurnCount + 1;
			}
		}

		float now = g_Game.GetTickTime();
		if (now < m_PHM_NextLogAt)
			return;

		m_PHM_NextLogAt = now + TELEMETRY_INTERVAL_SECONDS;
		LogTelemetry(zombie, controller, now, currentYaw, yawError, turning);
	}

	//! One line per second while driving. Everything the four unknowns need is on
	//! this line, so a single RPT grep for "Probe drive" answers all of them.
	protected void LogTelemetry(ZombieBase zombie, DayZInfectedInputController controller, float now, float currentYaw, float yawError, bool turning)
	{
		float elapsed = now - m_PHM_StartTime;

		vector position = zombie.GetPosition();
		float distance3D = vector.Distance(m_PHM_StartPos, position);

		//! Horizontal separately: a zombie sliding down a slope or dropped by the
		//! engine onto a lower surface would inflate the 3D figure and make a
		//! frozen infected look like it moved.
		float distanceFlat = HorizontalDistance(m_PHM_StartPos, position);

		//! GetVelocity is the engine's own reading (EnPhysics.c:104) and is the only
		//! number here that cannot be produced by mod side bookkeeping - it is what
		//! separates "translating" from "the animation plays while it slides".
		vector velocity = GetVelocity(zombie);
		float speed = velocity.Length();

		//! Read BACK from the controller (DayZCreatureAIInputController.c:4). If this
		//! does not echo ProbeSpeed the override never landed, and unknown #2 is
		//! answered before the gait is even looked at.
		float readSpeed = controller.GetMovementSpeed();

		int mindState = controller.GetMindState();

		bool hasTarget = false;
		if (controller.GetTargetEntity())
			hasTarget = true;

		string line = "Probe drive: t=" + elapsed.ToString();
		line = line + " dist3d=" + distance3D.ToString();
		line = line + " distflat=" + distanceFlat.ToString();
		line = line + " vel=" + speed.ToString();
		line = line + " spdRead=" + readSpeed.ToString();
		line = line + " mind=" + mindState.ToString();
		line = line + " tgt=" + hasTarget.ToString();
		line = line + " turning=" + turning.ToString();
		line = line + " yaw=" + currentYaw.ToString();
		line = line + " err=" + yawError.ToString();
		Emit(line);
	}

	//! Ground plane distance, built as two flat vectors the same way the pursuit
	//! waypoint compare does (PHM_ZombieBase.c:487-495). Explicit copies rather
	//! than zeroing a component in place, because both arguments reach here as
	//! members that the caller still needs intact.
	protected float HorizontalDistance(vector from, vector to)
	{
		vector flatFrom = Vector(from[0], 0.0, from[2]);
		vector flatTo = Vector(to[0], 0.0, to[2]);
		return vector.Distance(flatFrom, flatTo);
	}

	//! Public, frozen release entry point. Called from the MissionServer teardown
	//! and from ZombieBase teardown. Step 0 possesses one infected at a time, so
	//! "all" is exactly one - the plural name is the contract the other two
	//! components were written against.
	void ReleaseAll()
	{
		Release("external");
	}

	//! The single release path. Ordering is deliberate and copied from
	//! PHM_StopDriving (PHM_ZombieBase.c:540-565): flag first, then mod side state,
	//! and only then the engine calls that are allowed to be skipped. A release
	//! that aborts early on a dead entity must still have cleared everything the
	//! mod owns, otherwise the next tick finds a half-possessed infected.
	protected void Release(string reason)
	{
		if (!m_PHM_Active)
			return;

		m_PHM_Active = false;

		ZombieBase zombie = m_PHM_Subject;
		m_PHM_Subject = null;

		//! Cooldown from the value captured at possession, not from the settings
		//! holder: this runs on the shutdown path too, where nothing may be assumed
		//! about what is still loaded.
		if (g_Game)
			m_PHM_CooldownUntil = g_Game.GetTickTime() + m_PHM_CooldownSeconds;

		LogSummary(zombie, reason);

		if (!zombie)
			return;

		//! Mod side state on the entity, cleared BEFORE the engine guards below.
		//! Idempotent - ZombieBase teardown clears the same flag before calling in
		//! here (PHM_ZombieBase.c:767-773), and PHM_SetProbeActive(false) makes no
		//! further calls, so there is no way back into this function.
		zombie.PHM_SetProbeActive(false);

		if (!g_Game)
			return;

		//! An entity on its way out gets nothing written to it. Its overrides die
		//! with it, and its controller may already be gone.
		if (zombie.IsSetForDeletion())
			return;

		DayZInfectedInputController controller = zombie.GetInputController();
		if (controller)
		{
			//! state=false is the documented release form; the value is ignored.
			controller.OverrideMovementSpeed(false, 0.0);
		}

		//! SetKeepInIdle is shared with the backstab finisher, which sets it at
		//! ZombieBase.c:1056 and clears it in OnRecoverFromDeath at :1087. Clearing
		//! it here in either of those states would re-enable AI simulation at
		//! precisely the wrong moment, and on a dead infected there is no brain left
		//! to resume - so the mod side state above is cleared unconditionally while
		//! this one call is not.
		if (!zombie.IsAlive())
			return;

		if (zombie.IsBeingBackstabbed())
			return;

		AIAgent agent = zombie.GetAIAgent();
		if (!agent)
			return;

		agent.SetKeepInIdle(false);

		//! LAST, and only for a subject this probe created itself. A self-spawned
		//! test infected exists solely for the measurement, so leaving it standing
		//! would litter the world with one zombie per run. Deleted after the brain
		//! was handed back, never before: ObjectDelete on a possessed entity would
		//! tear it down while the override is still pinned.
		if (m_PHM_Spawned)
		{
			m_PHM_Spawned = false;
			g_Game.ObjectDelete(zombie);
		}
	}

	//! One line per run. Carries the totals plus the exact configuration that
	//! produced them, so a run can be interpreted from the summary alone without
	//! having to trust that the JSON on disk still matches what was live.
	protected void LogSummary(ZombieBase zombie, string reason)
	{
		vector endPos = m_PHM_LastPos;
		if (zombie)
		{
			if (!zombie.IsSetForDeletion())
				endPos = zombie.GetPosition();
		}

		float elapsed = 0.0;
		if (g_Game)
			elapsed = g_Game.GetTickTime() - m_PHM_StartTime;

		float distanceFlat = HorizontalDistance(m_PHM_StartPos, endPos);

		//! Average over the whole run, which is the number that has to be compared
		//! against ProbeSpeed to settle the enum-versus-m/s question. Guarded
		//! because a release on the very first tick would divide by zero.
		float average = 0.0;
		if (elapsed > 0.0)
			average = distanceFlat / elapsed;

		string line = "Probe release (" + reason + "): elapsed=" + elapsed.ToString();
		line = line + " distflat=" + distanceFlat.ToString();
		line = line + " avgSpeed=" + average.ToString();
		//! Read these two TOGETHER. tgtAtPossess=0 with sawTarget=1 is the only
		//! combination that proves native perception kept running under
		//! SetKeepInIdle(true); tgtAtPossess=1 makes sawTarget meaningless and the
		//! run must be discarded for unknown #1.
		line = line + " tgtAtPossess=" + m_PHM_TargetAtPossess.ToString();
		line = line + " sawTarget=" + m_PHM_SawTarget.ToString();
		line = line + " turns=" + m_PHM_TurnCount.ToString();
		line = line + " cfgSpeed=" + m_PHM_UsedSpeed.ToString();
		line = line + " cfgTurnMode=" + m_PHM_UsedTurnMode.ToString();
		line = line + " cfgTurnType=" + m_PHM_UsedTurnType.ToString();
		PHM_Logger.Notice(line);
	}

	//! Telemetry sink.
	//!
	//! PHM_Logger.Debug returns early unless LogBroadcasts is true
	//! (PHM_Logger.c:20-30), and step 0's entire value IS the RPT output - routing
	//! the per-second lines through Debug unconditionally would make the whole
	//! measurement silent on a default config. So: Debug when the debug channel is
	//! open (the lines land next to the rest of the mod's diagnostics with the same
	//! [DBG] prefix), Notice when it is not. Exactly one line either way, and no
	//! dependency on a shared file this component does not own.
	//!
	//! The volume this can produce is bounded and small: one line per second, for
	//! at most ProbeDurationSeconds (hard clamp 120), for ONE infected, behind a
	//! switch that is off by default.
	protected void Emit(string message)
	{
		PHM_Settings settings = PHM_SettingsHolder.Get();

		if (settings)
		{
			if (settings.LogBroadcasts)
			{
				PHM_Logger.Debug(message);
				return;
			}
		}

		PHM_Logger.Notice(message);
	}

	//! Diagnosis only: is a run in flight right now.
	bool PHM_IsActive()
	{
		return m_PHM_Active;
	}
}
