//! Decides whether the hive is active in the current time window.
//!
//! Primary source is the native World.IsNight() (World.c:56) because it is map
//! correct. WorldData.GetDaytime() is deliberately NOT used: its fallback builds
//! ChernarusPlusData on every custom map (missionBase.c:104-139), which would
//! produce wrong sunrise/sunset times off Chernarus.
//!
//! Evaluated lazily at broadcast time, so no timer is needed.
class PHM_TimeGate
{
	static bool IsActive(PHM_Settings settings)
	{
		if (!settings)
			return false;

		if (settings.ActiveTimeWindow == EPHM_TimeWindow.ALWAYS)
			return true;

		bool night = IsNightNow(settings);

		if (settings.ActiveTimeWindow == EPHM_TimeWindow.NIGHT_ONLY)
			return night;

		if (settings.ActiveTimeWindow == EPHM_TimeWindow.DAY_ONLY)
			return !night;

		return true;
	}

	static bool IsNightNow(PHM_Settings settings)
	{
		if (!settings)
			return false;

		if (!g_Game)
			return false;

		World world = g_Game.GetWorld();
		if (!world)
			return false;

		if (!settings.UseCustomNightHours)
			return world.IsNight();

		int year;
		int month;
		int day;
		int hour;
		int minute;
		world.GetDate(year, month, day, hour, minute);

		int start = settings.NightStartHour;
		int end = settings.NightEndHour;

		if (start == end)
			return false;

		//! Wrapping window, e.g. 20:00 .. 06:00
		if (start > end)
		{
			if (hour >= start)
				return true;

			if (hour < end)
				return true;

			return false;
		}

		//! Non wrapping window, e.g. 01:00 .. 05:00
		if (hour >= start && hour < end)
			return true;

		return false;
	}
}
