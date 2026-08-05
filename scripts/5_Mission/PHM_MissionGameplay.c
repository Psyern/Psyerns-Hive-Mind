//! Client entry point for the admin debug map.
//!
//! Registers a bindable input so the key is chosen in the DayZ controls menu
//! instead of being hardcoded (Input.RegisterInput, UAInput.c:191), and receives
//! the snapshot RPC through DayZGame.Event_OnRPC (DayZGame.c:970/:3089).
//!
//! Nothing here grants any authority: the server decides who is an admin and
//! simply never sends snapshots to anyone else.
modded class MissionGameplay
{
	protected UAInput m_PHM_MapInput;
	protected ref PHM_HiveMapMenu m_PHM_MapMenu;
	protected bool m_PHM_Registered;

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

		PHM_RegisterInput();
	}

	protected void PHM_RegisterInput()
	{
		if (m_PHM_Registered)
			return;

		//! RegisterGroup/RegisterInput live on UAInputAPI (UAInput.c:165/:191),
		//! not on Input - GetUApi() is the accessor (UAInput.c:239).
		UAInputAPI api = GetUApi();
		if (!api)
			return;

		api.RegisterGroup(PHM_Constants.INPUT_GROUP, "Psyerns Hive Mind");
		m_PHM_MapInput = api.RegisterInput(PHM_Constants.INPUT_MAP, "Hive Debug Map", PHM_Constants.INPUT_GROUP);

		m_PHM_Registered = true;
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
