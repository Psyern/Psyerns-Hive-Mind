//! Client side glue between the RPC channel and the open map menu.
//!
//! Kept separate from the menu so an arriving snapshot never depends on the menu
//! existing, and so subscribe/unsubscribe survives the menu being recreated.
class PHM_HiveMapClient
{
	protected static PHM_HiveMapMenu s_PHM_Menu;
	protected static bool s_PHM_Subscribed;

	static void SetMenu(PHM_HiveMapMenu menu)
	{
		s_PHM_Menu = menu;
	}

	static PHM_HiveMapMenu GetMenu()
	{
		return s_PHM_Menu;
	}

	static bool IsSubscribed()
	{
		return s_PHM_Subscribed;
	}

	//! Tells the server to start or stop streaming. The server re-checks the admin
	//! whitelist on every push, so this request carries no authority by itself.
	static void SetSubscribed(bool subscribed)
	{
		if (!g_Game)
			return;

		s_PHM_Subscribed = subscribed;

		Param1<bool> data = new Param1<bool>(subscribed);
		g_Game.RPCSingleParam(null, EPHM_RPC.PHM_RPC_DEBUG_SUBSCRIBE, data, true, null);
	}

	static void OnSnapshot(ParamsReadContext ctx)
	{
		Param1<PHM_DebugSnapshot> data = new Param1<PHM_DebugSnapshot>(null);
		if (!ctx.Read(data))
			return;

		if (!data.param1)
			return;

		if (!s_PHM_Menu)
			return;

		s_PHM_Menu.PHM_SetSnapshot(data.param1);
	}
}
