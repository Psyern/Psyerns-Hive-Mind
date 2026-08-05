//! Admin only live map of the hive.
//!
//! Client side. Receives PHM_DebugSnapshot over RPC and redraws every frame, so
//! the connection lines stay glued to the map while it is panned and zoomed.
//!
//! Markers use the native MapWidget.AddUserMark. Connection lines do not exist in
//! MapWidget, so they are thin PanelWidgets placed with MapToScreen and rotated
//! with Widget.SetRotation (EnWidgets.c:146).
class PHM_HiveMapMenu extends UIScriptedMenu
{
	protected MapWidget m_PHM_Map;
	protected Widget m_PHM_Overlay;

	protected TextWidget m_PHM_LineStatus;
	protected TextWidget m_PHM_LineMarked;
	protected TextWidget m_PHM_LineRadius;
	protected TextWidget m_PHM_LineTime;
	protected TextWidget m_PHM_LineRelay;

	protected ref array<Widget> m_PHM_LinePool;
	protected int m_PHM_LinesUsed;

	protected ref PHM_DebugSnapshot m_PHM_Snapshot;
	protected bool m_PHM_Centered;

	void PHM_HiveMapMenu()
	{
		m_PHM_LinePool = new array<Widget>;
		m_PHM_LinesUsed = 0;
		m_PHM_Centered = false;
	}

	void ~PHM_HiveMapMenu()
	{
		//! Widgets created under layoutRoot are destroyed by the engine together
		//! with it, so the pool only needs its script side references dropped.
		if (m_PHM_LinePool)
			m_PHM_LinePool.Clear();
	}

	override Widget Init()
	{
		if (!g_Game)
			return null;

		layoutRoot = g_Game.GetWorkspace().CreateWidgets(PHM_Constants.LAYOUT_MAP);
		if (!layoutRoot)
		{
			PHM_Logger.Error("Could not create layout: " + PHM_Constants.LAYOUT_MAP);
			return null;
		}

		m_PHM_Map = MapWidget.Cast(layoutRoot.FindAnyWidget("phm_map"));
		m_PHM_Overlay = layoutRoot.FindAnyWidget("phm_overlay");

		m_PHM_LineStatus = TextWidget.Cast(layoutRoot.FindAnyWidget("phm_line_status"));
		m_PHM_LineMarked = TextWidget.Cast(layoutRoot.FindAnyWidget("phm_line_marked"));
		m_PHM_LineRadius = TextWidget.Cast(layoutRoot.FindAnyWidget("phm_line_radius"));
		m_PHM_LineTime = TextWidget.Cast(layoutRoot.FindAnyWidget("phm_line_time"));
		m_PHM_LineRelay = TextWidget.Cast(layoutRoot.FindAnyWidget("phm_line_relay"));

		return layoutRoot;
	}

	override void OnShow()
	{
		super.OnShow();

		PHM_HiveMapClient.SetSubscribed(true);
		PHM_HiveMapClient.SetMenu(this);
	}

	override void OnHide()
	{
		super.OnHide();

		PHM_HiveMapClient.SetSubscribed(false);
		PHM_HiveMapClient.SetMenu(null);
	}

	void PHM_SetSnapshot(PHM_DebugSnapshot snapshot)
	{
		m_PHM_Snapshot = snapshot;
	}

	override void Update(float timeslice)
	{
		super.Update(timeslice);

		if (!m_PHM_Map)
			return;

		//! Center once on the local player so the admin does not start out looking
		//! at the map origin.
		if (!m_PHM_Centered && g_Game && g_Game.GetPlayer())
		{
			m_PHM_Map.SetMapPos(g_Game.GetPlayer().GetPosition());
			m_PHM_Map.SetScale(0.25);
			m_PHM_Centered = true;
		}

		ResetLines();
		m_PHM_Map.ClearUserMarks();

		if (!m_PHM_Snapshot)
		{
			SetText(m_PHM_LineStatus, "warte auf Server ...");
			return;
		}

		DrawSpotters();
		DrawNodes();
		DrawEdges();
		UpdateInfo();

		HideUnusedLines();
	}

	protected void DrawNodes()
	{
		int count = m_PHM_Snapshot.nodes.Count();
		int index;
		PHM_DebugNode node;
		vector position;
		string label;

		for (index = 0; index < count; index++)
		{
			node = m_PHM_Snapshot.nodes.Get(index);
			if (!node)
				continue;

			position = Vector(node.x, 0.0, node.z);

			int seconds = node.remaining;
			label = "hop " + node.hop.ToString() + "  " + seconds.ToString() + "s";

			m_PHM_Map.AddUserMark(position, label, ColorForHop(node.hop), PHM_Constants.MARK_TEXTURE);
		}
	}

	protected void DrawSpotters()
	{
		int count = m_PHM_Snapshot.spotters.Count();
		int index;
		PHM_DebugSpotter spotter;

		for (index = 0; index < count; index++)
		{
			spotter = m_PHM_Snapshot.spotters.Get(index);
			if (!spotter)
				continue;

			vector senderPos = Vector(spotter.x, 0.0, spotter.z);
			vector playerPos = Vector(spotter.px, 0.0, spotter.pz);

			m_PHM_Map.AddUserMark(senderPos, "SICHTER", PHM_Constants.COLOR_SPOTTER, PHM_Constants.MARK_TEXTURE);

			string playerLabel = spotter.playerName;
			if (playerLabel == "")
				playerLabel = "Spieler";

			m_PHM_Map.AddUserMark(playerPos, playerLabel, PHM_Constants.COLOR_PLAYER, PHM_Constants.MARK_TEXTURE);

			//! Line from the spotter to the player it actually saw.
			DrawLine(senderPos, playerPos, PHM_Constants.COLOR_PLAYER, 1.0);

			//! Share radius ring, only while the sighting is fresh - otherwise the
			//! map fills up with stale circles.
			if (spotter.age <= PHM_Constants.RING_MAX_AGE)
				DrawRing(senderPos, spotter.radius, PHM_Constants.COLOR_RING);
		}
	}

	protected void DrawEdges()
	{
		int count = m_PHM_Snapshot.edges.Count();
		int index;
		PHM_DebugEdge edge;

		for (index = 0; index < count; index++)
		{
			edge = m_PHM_Snapshot.edges.Get(index);
			if (!edge)
				continue;

			vector from = Vector(edge.ax, 0.0, edge.az);
			vector to = Vector(edge.bx, 0.0, edge.bz);

			int color = ColorForHop(edge.hop);
			if (edge.refreshOnly)
				color = PHM_Constants.COLOR_REFRESH;

			//! Older links fade out so the newest wave reads clearly.
			float fade = 1.0 - (edge.age / PHM_Constants.EDGE_FADE_SECONDS);
			if (fade < PHM_Constants.EDGE_MIN_ALPHA)
				fade = PHM_Constants.EDGE_MIN_ALPHA;

			DrawLine(from, to, color, fade);
		}
	}

	//! Approximates the share radius as a closed polygon of straight segments,
	//! because there is no circle primitive in the widget system.
	protected void DrawRing(vector center, float radius, int color)
	{
		int segments = PHM_Constants.RING_SEGMENTS;
		int index;
		float step = Math.PI2 / segments;

		float previousX = center[0] + radius;
		float previousZ = center[2];
		float angle;
		float currentX;
		float currentZ;

		for (index = 1; index <= segments; index++)
		{
			angle = step * index;
			currentX = center[0] + (Math.Cos(angle) * radius);
			currentZ = center[2] + (Math.Sin(angle) * radius);

			vector a = Vector(previousX, 0.0, previousZ);
			vector b = Vector(currentX, 0.0, currentZ);
			DrawLine(a, b, color, PHM_Constants.RING_ALPHA);

			previousX = currentX;
			previousZ = currentZ;
		}
	}

	//! Places one pooled panel between two world positions.
	//!
	//! MapToScreen returns ABSOLUTE screen coordinates, so the overlay's own screen
	//! position has to be subtracted before SetPos - same correction Expansion
	//! applies in ExpansionMapMarkerPlayerArrow.c:71-74.
	protected void DrawLine(vector worldA, vector worldB, int color, float alpha)
	{
		Widget line = AcquireLine();
		if (!line)
			return;

		vector screenA = m_PHM_Map.MapToScreen(worldA);
		vector screenB = m_PHM_Map.MapToScreen(worldB);

		float originX;
		float originY;
		m_PHM_Overlay.GetScreenPos(originX, originY);

		float ax = screenA[0] - originX;
		float ay = screenA[1] - originY;
		float bx = screenB[0] - originX;
		float by = screenB[1] - originY;

		float dx = bx - ax;
		float dy = by - ay;
		float length = Math.Sqrt((dx * dx) + (dy * dy));

		if (length < 1.0)
		{
			line.Show(false);
			return;
		}

		float angle = Math.Atan2(dy, dx) * Math.RAD2DEG;
		float midX = (ax + bx) * 0.5;
		float midY = (ay + by) * 0.5;
		float thickness = PHM_Constants.LINE_THICKNESS;

		//! Sized and placed unrotated first, centered on the midpoint, then rotated
		//! about that center.
		line.SetSize(length, thickness, false);
		line.SetPos(midX - (length * 0.5), midY - (thickness * 0.5), false);
		line.SetRotation(0, 0, angle, true);
		line.SetColor(color);
		line.SetAlpha(alpha);
		line.Show(true);
	}

	protected Widget AcquireLine()
	{
		if (!g_Game)
			return null;

		if (!m_PHM_Overlay)
			return null;

		if (m_PHM_LinesUsed >= PHM_Constants.LINE_POOL_MAX)
			return null;

		if (m_PHM_LinesUsed < m_PHM_LinePool.Count())
		{
			Widget existing = m_PHM_LinePool.Get(m_PHM_LinesUsed);
			m_PHM_LinesUsed = m_PHM_LinesUsed + 1;
			return existing;
		}

		Widget created = g_Game.GetWorkspace().CreateWidgets(PHM_Constants.LAYOUT_LINE, m_PHM_Overlay);
		if (!created)
			return null;

		m_PHM_LinePool.Insert(created);
		m_PHM_LinesUsed = m_PHM_LinesUsed + 1;
		return created;
	}

	protected void ResetLines()
	{
		m_PHM_LinesUsed = 0;
	}

	protected void HideUnusedLines()
	{
		int count = m_PHM_LinePool.Count();
		int index;
		Widget line;

		for (index = m_PHM_LinesUsed; index < count; index++)
		{
			line = m_PHM_LinePool.Get(index);
			if (line)
				line.Show(false);
		}
	}

	protected int ColorForHop(int hop)
	{
		if (hop <= 0)
			return PHM_Constants.COLOR_HOP0;

		return PHM_Constants.COLOR_HOP1;
	}

	protected void UpdateInfo()
	{
		string status = "aktiv";
		if (!m_PHM_Snapshot.hiveActive)
			status = "INAKTIV (Zeitfenster)";

		string nightText = "Tag";
		if (m_PHM_Snapshot.isNight)
			nightText = "Nacht";

		SetText(m_PHM_LineStatus, "Hive: " + status);

		string markedText = "markiert: " + m_PHM_Snapshot.markedCount.ToString() + " / " + m_PHM_Snapshot.cap.ToString();
		markedText = markedText + "   Registry: " + m_PHM_Snapshot.registrySize.ToString();
		if (m_PHM_Snapshot.nodesOmitted > 0)
			markedText = markedText + "   (+" + m_PHM_Snapshot.nodesOmitted.ToString() + " nicht gezeichnet)";
		SetText(m_PHM_LineMarked, markedText);

		string radiusText = "Radius: " + m_PHM_Snapshot.shareRadius.ToString() + " m";
		radiusText = radiusText + "   Sicht x" + m_PHM_Snapshot.visionMultiplier.ToString();
		SetText(m_PHM_LineRadius, radiusText);

		string timeText = "Zeit: " + m_PHM_Snapshot.worldHour.ToString() + ":" + PadMinute(m_PHM_Snapshot.worldMinute);
		timeText = timeText + " (" + nightText + ")";
		SetText(m_PHM_LineTime, timeText);

		string relayText = "Relais: " + m_PHM_Snapshot.relayGenerations.ToString();
		relayText = relayText + "   Noise-Ping: " + m_PHM_Snapshot.noisePing.ToString();
		SetText(m_PHM_LineRelay, relayText);
	}

	protected string PadMinute(int minute)
	{
		if (minute < 10)
			return "0" + minute.ToString();

		return minute.ToString();
	}

	protected void SetText(TextWidget widget, string value)
	{
		if (!widget)
			return;

		widget.SetText(value);
	}

	override bool OnKeyPress(Widget w, int x, int y, int key)
	{
		super.OnKeyPress(w, x, y, key);

		if (key == KeyCode.KC_ESCAPE)
		{
			Close();
			return true;
		}

		return false;
	}
}
