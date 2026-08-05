class PHM_Logger
{
	static void Notice(string message)
	{
		PrintToRPT("[PHM] " + message);
	}

	static void Warn(string message)
	{
		PrintToRPT("[PHM][WARN] " + message);
	}

	static void Error(string message)
	{
		PrintToRPT("[PHM][ERROR] " + message);
	}

	//! Gated behind LogBroadcasts so a hardcore server with high infected density
	//! does not flood the RPT. PrintToRPT fflushes on every write.
	static void Debug(string message)
	{
		PHM_Settings settings = PHM_SettingsHolder.Get();
		if (!settings)
			return;

		if (!settings.LogBroadcasts)
			return;

		PrintToRPT("[PHM][DBG] " + message);
	}
}
