class CfgPatches
{
	class Psyerns_Hive_Mind_V1_Scripts
	{
		units[] = {};
		weapons[] = {};
		requiredVersion = 0.1;
		requiredAddons[] = {"DZ_Data"};
	};
};

class CfgMods
{
	class Psyerns_Hive_Mind_V1
	{
		dir = "Psyerns_Hive_Mind_V1";
		picture = "";
		action = "";
		hideName = 0;
		hidePicture = 1;
		name = "Psyerns Hive Mind";
		credits = "";
		version = "1.0.0";
		inputs = "Psyerns_Hive_Mind_V1/scripts/data/Inputs.xml";
		author = "Psyern";
		extra = 0;
		type = "mod";
		dependencies[] = {"Game", "World", "Mission"};

		class defs
		{
			class gameScriptModule
			{
				value = "";
				files[] = {"Psyerns_Hive_Mind_V1/scripts/3_Game"};
			};
			class worldScriptModule
			{
				value = "";
				files[] = {"Psyerns_Hive_Mind_V1/scripts/4_World"};
			};
			class missionScriptModule
			{
				value = "";
				files[] = {"Psyerns_Hive_Mind_V1/scripts/5_Mission"};
			};
		};
	};
};
