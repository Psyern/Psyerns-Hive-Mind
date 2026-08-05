//! Client entry point for the admin debug map.
//!
//! Resolves the bindable input declared in scripts/data/Inputs.xml so the key can
//! be rebound in the DayZ controls menu, and receives the snapshot RPC through
//! DayZGame.Event_OnRPC (DayZGame.c:970/:3089).
//!
//! Nothing here grants any authority: the server decides who is an admin and
//! simply never sends snapshots to anyone else.
modded class MissionGameplay
{
	protected UAInput m_PHM_MapInput;
	protected ref PHM_HiveMapMenu m_PHM_MapMenu;
	protected bool m_PHM_Registered;
	protected bool m_PHM_ResolveLogged;

	void MissionGameplay()
	{
		if (!g_Game)
			return;

		DayZGame dayzGame = DayZGame.Cast(g_Game);
		if (dayzGame)
			dayzGame.Event_OnRPC.Insert(PHM_OnRPC);
	}

	void ~MissionGameplay()
	{
		if (!g_Game)
			return;

		DayZGame dayzGame = DayZGame.Cast(g_Game);
		if (dayzGame)
			dayzGame.Event_OnRPC.Remove(PHM_OnRPC);
	}

	override void OnInit()
	{
		super.OnInit();

		PHM_ResolveInput();
	}

	//! Resolves the input that scripts/data/Inputs.xml declared and config.cpp
	//! registered through the CfgMods "inputs" property. The engine has already
	//! created it by the time OnInit runs; script only looks it up.
	protected void PHM_ResolveInput()
	{
		if (m_PHM_Registered)
			return;

		UAInputAPI api = GetUApi();
		if (!api)
			return;

		m_PHM_MapInput = api.GetInputByName(PHM_Constants.INPUT_MAP);
		if (!m_PHM_MapInput)
		{
			//! Logged once, then retried silently from OnUpdate, so an unexpected
			//! init ordering cannot leave the key dead for the whole session.
			if (!m_PHM_ResolveLogged)
			{
				m_PHM_ResolveLogged = true;
				PHM_Logger.Error("Input " + PHM_Constants.INPUT_MAP + " not found. Is scripts/data/Inputs.xml packed into the PBO and referenced by config.cpp?");
			}

			return;
		}

		m_PHM_Registered = true;
		PHM_Logger.Notice("Hive debug map input ready: " + PHM_Constants.INPUT_MAP);
	}

	void PHM_OnRPC(PlayerIdentity sender, Object target, int rpc_type, ParamsReadContext ctx)
	{
		if (rpc_type != EPHM_RPC.PHM_RPC_DEBUG_SNAPSHOT)
			return;

		PHM_HiveMapClient.OnSnapshot(ctx);
	}

	override void OnUpdate(float timeslice)
	{
		super.OnUpdate(timeslice);

		if (!m_PHM_Registered)
		{
			PHM_ResolveInput();
			return;
		}

		if (!m_PHM_MapInput)
			return;

		if (!m_PHM_MapInput.LocalPress())
			return;

		PHM_ToggleMap();
	}

	protected void PHM_ToggleMap()
	{
		if (!g_Game)
			return;

		UIManager manager = g_Game.GetUIManager();
		if (!manager)
			return;

		if (m_PHM_MapMenu && m_PHM_MapMenu.IsVisible())
		{
			m_PHM_MapMenu.Close();
			return;
		}

		//! ShowScriptedMenu takes a ready instance, so the mod needs no MenuID
		//! registration (UIManager.c:9).
		m_PHM_MapMenu = new PHM_HiveMapMenu();
		manager.ShowScriptedMenu(m_PHM_MapMenu, null);
	}
}
