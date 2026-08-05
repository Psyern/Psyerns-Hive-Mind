//! The actual transmission channel into the engine's AI perception.
//!
//! GetMaxVisionRangeModifier is the only documented point where the engine asks
//! script during target evaluation, and it does so per target AND per observing
//! AI (AITarget_callbacks.c:13). The sibling method already casts pApplicant to
//! DayZInfected and reads its mind state (AITargetCallbacksPlayer.c:24-33), which
//! proves the observing infected is what gets passed in.
//!
//! Vanilla only ever instantiates this class server side (PlayerBase.c:6033-6042,
//! guarded by INSTANCETYPE_SERVER || !IsMultiplayer), so the hook is inherently
//! server authoritative.
//!
//! The vanilla member m_Player is private (AITargetCallbacksPlayer.c:5) and is
//! deliberately not touched. All state comes from pApplicant, which also keeps
//! this path allocation free.
modded class AITargetCallbacksPlayer
{
	override float GetMaxVisionRangeModifier(EntityAI pApplicant)
	{
		float result = super.GetMaxVisionRangeModifier(pApplicant);

		if (!pApplicant)
			return result;

		//! Cheapest possible rejection first: skips animals and foreign AI
		//! entities without paying for a cast.
		if (!pApplicant.IsZombie())
			return result;

		if (!g_Game)
			return result;

		if (!g_Game.IsDedicatedServer())
			return result;

		ZombieBase zombie = ZombieBase.Cast(pApplicant);
		if (!zombie)
			return result;

		if (!zombie.PHM_IsHiveAlerted())
			return result;

		PHM_Settings settings = PHM_SettingsHolder.Get();
		if (!settings)
			return result;

		if (!settings.Enabled)
			return result;

		//! Never do file IO, logging or a registry walk in here. The engine calls
		//! this per target and per observing AI.
		float boosted = result * settings.VisionRangeMultiplier;
		return boosted;
	}
}
