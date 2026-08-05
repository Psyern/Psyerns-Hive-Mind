//! Transport types for the admin debug map.
//!
//! These travel as a single Param1<PHM_DebugSnapshot> over the native RPC. The
//! engine serializer does deep serialization of classes and dynamic containers
//! (Serializer.c:3-8), so nested ref arrays are carried automatically - vanilla
//! ships whole JSON DTO classes the same way
//! (CfgPlayerRestrictedAreaHandler.c:67).
//!
//! Keep these primitive-only: no entity references, no vectors. Positions travel
//! as separate floats because no vanilla or Expansion serialized class uses a
//! vector member.

//! One hive marked infected.
class PHM_DebugNode
{
	float x;
	float z;
	int hop;
	float remaining;
}

//! One "A informed B" link, drawn as a line on the map.
class PHM_DebugEdge
{
	float ax;
	float az;
	float bx;
	float bz;
	int hop;
	float age;
	bool refreshOnly;
}

//! An infected that actually saw a player, plus the player it saw.
class PHM_DebugSpotter
{
	float x;
	float z;
	float px;
	float pz;
	float radius;
	float age;
	int hop;
	string playerName;
}

class PHM_DebugSnapshot
{
	int markedCount;
	int registrySize;
	int cap;
	int relayGenerations;
	int worldHour;
	int worldMinute;
	int nodesOmitted;

	float shareRadius;
	float visionMultiplier;

	bool isNight;
	bool hiveActive;
	bool noisePing;

	ref array<ref PHM_DebugNode> nodes;
	ref array<ref PHM_DebugEdge> edges;
	ref array<ref PHM_DebugSpotter> spotters;

	void PHM_DebugSnapshot()
	{
		nodes = new array<ref PHM_DebugNode>;
		edges = new array<ref PHM_DebugEdge>;
		spotters = new array<ref PHM_DebugSpotter>;
	}

	void Reset()
	{
		markedCount = 0;
		registrySize = 0;
		cap = 0;
		relayGenerations = 0;
		worldHour = 0;
		worldMinute = 0;
		nodesOmitted = 0;

		shareRadius = 0.0;
		visionMultiplier = 1.0;

		isNight = false;
		hiveActive = false;
		noisePing = false;

		nodes.Clear();
		edges.Clear();
		spotters.Clear();
	}
}
