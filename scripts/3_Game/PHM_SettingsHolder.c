//! Static holder plus file IO, kept separate from the DTO.
//!
//! Uses LoadFile/SaveFile, NOT the JsonLoadFile/JsonSaveFile pair shown in older
//! guides: those are marked DEPRECATED in 1.29 (JsonFileLoader.c:99-104) and
//! swallow write errors silently.
class PHM_SettingsHolder
{
	static ref PHM_Settings s_PHM_Settings;
	static float s_PHM_NextReloadCheck;

	static PHM_Settings Get()
	{
		return s_PHM_Settings;
	}

	static void EnsureDirectories()
	{
		//! MakeDirectory is not recursive, so every level is created separately.
		if (!FileExist(PHM_Constants.SETTINGS_ROOT))
			MakeDirectory(PHM_Constants.SETTINGS_ROOT);

		if (!FileExist(PHM_Constants.SETTINGS_DIR))
			MakeDirectory(PHM_Constants.SETTINGS_DIR);
	}

	static bool Load()
	{
		EnsureDirectories();

		PHM_Settings settings = new PHM_Settings();
		bool needsSave = false;

		if (!FileExist(PHM_Constants.SETTINGS_FILE))
		{
			settings.Defaults();
			needsSave = true;
			PHM_Logger.Notice("No settings file found, writing defaults to " + PHM_Constants.SETTINGS_FILE);
		}
		else
		{
			string errorMessage;
			bool loaded = JsonFileLoader<PHM_Settings>.LoadFile(PHM_Constants.SETTINGS_FILE, settings, errorMessage);
			if (!loaded)
			{
				PHM_Logger.Error("Failed to read settings: " + errorMessage);
				PHM_Logger.Error("Falling back to defaults. The existing file is NOT overwritten so it can be repaired.");
				settings.Defaults();
				s_PHM_Settings = settings;
				ScheduleNextReloadCheck();
				return false;
			}

			if (settings.Version < PHM_Constants.SETTINGS_VERSION)
			{
				int oldVersion = settings.Version;
				int newVersion = PHM_Constants.SETTINGS_VERSION;
				PHM_Logger.Notice("Migrating settings from version " + oldVersion.ToString() + " to " + newVersion.ToString());
				settings.Version = newVersion;
				needsSave = true;
			}
		}

		settings.Validate();
		s_PHM_Settings = settings;

		if (needsSave)
			Save();

		ScheduleNextReloadCheck();
		return true;
	}

	static bool Save()
	{
		if (!s_PHM_Settings)
			return false;

		EnsureDirectories();

		string errorMessage;
		bool saved = JsonFileLoader<PHM_Settings>.SaveFile(PHM_Constants.SETTINGS_FILE, s_PHM_Settings, errorMessage);
		if (!saved)
		{
			PHM_Logger.Error("Failed to write settings: " + errorMessage);
			return false;
		}

		return true;
	}

	//! Lazy hot reload. Called from the (rate limited) broadcast path only, so
	//! there is no timer and no per-frame cost. Lets an admin calibrate
	//! ShareRadius / VisionRangeMultiplier without restarting the server.
	static void ReloadIfDue()
	{
		if (!s_PHM_Settings)
			return;

		if (s_PHM_Settings.SettingsReloadSeconds <= 0.0)
			return;

		if (!g_Game)
			return;

		float now = g_Game.GetTickTime();
		if (now < s_PHM_NextReloadCheck)
			return;

		Load();
	}

	protected static void ScheduleNextReloadCheck()
	{
		if (!g_Game)
			return;

		if (!s_PHM_Settings)
			return;

		s_PHM_NextReloadCheck = g_Game.GetTickTime() + s_PHM_Settings.SettingsReloadSeconds;
	}
}
