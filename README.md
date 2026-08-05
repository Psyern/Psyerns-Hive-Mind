# Psyerns Hive Mind

Alle Infizierten teilen sich ein neuronales Netz. Sieht ein Zombie einen Spieler,
wissen es die Zombies im Teilungsradius — und kommen.

Reiner **serverseitiger** Verhaltensmod. Kein Client-Anteil, keine RPCs, keine
eigenen NetSync-Variablen, keine neuen Items.

---

## Wie es technisch funktioniert

DayZ hat **keine** Script-API, mit der man einem Infizierten ein Ziel zuweisen
könnte. `DayZInfectedInputController` besitzt ausschließlich Getter
(`GetMindState`, `GetTargetEntity`), und `ZombieBase.m_ActualTarget` wird jeden
Tick aus dem Controller neu gelesen. Der Mod nutzt deshalb die beiden einzigen
belegten Kanäle:

**1. Wahrnehmungs-Boost (der eigentliche Hive — deterministisch)**
`AITargetCallbacks.GetMaxVisionRangeModifier(EntityAI pApplicant)` ist der einzige
Punkt, an dem die Engine während der KI-Wahrnehmung Script fragt — und zwar *pro
Ziel und pro beobachtender KI*. Markierte Zombies bekommen dort einen erhöhten
Sichtweiten-Faktor und erfassen den Spieler dadurch auf große Entfernung. Ab da
übernimmt die normale Vanilla-Verfolgung mit Navmesh und Pfadfindung — der Mod
bewegt keinen Zombie selbst.

Weil die Markierung ausschließlich als eigenes Script-Flag existiert, sind
Radius und Anzahl exakt und reproduzierbar.

**2. Noise-Ping (die Zugkraft — nicht deterministisch)**
`NoiseSystem.AddNoiseTarget` setzt einen Reiz an der Spielerposition, den die KI
für die eingestellte Dauer „sieht". Das erreicht auch Zombies **ohne Sichtlinie**
und ist der Grund, warum Hordenlocken überhaupt funktioniert. Dieser Kanal ist ein
Engine-Broadcast: er ignoriert `MaxSharedZombies`, und seine Reichweite lässt sich
nicht exakt in Metern einstellen.

### Wichtig zu verstehen

- Der Sicht-Boost skaliert die Sicht**reichweite**, er ersetzt **keine
  Sichtlinie**. Ein markierter Zombie hinter einem Gebäude sieht den Spieler
  nicht — er wird über den Noise-Ping herangezogen, bis er Sicht bekommt.
- `VisionRangeMultiplier` ist ein Faktor auf einen engine-internen Wert, **keine
  Meterangabe**. Vanilla liefert dort ca. 0.225 – 1.25. Der Wert muss auf dem
  Zielserver empirisch kalibriert werden (`LogBroadcasts` einschalten).
- Markierte Zombies sind gegenüber **allen** Spielern hyperaufmerksam, nicht nur
  gegenüber dem ursprünglich gesehenen. Genau das macht „Horde auf einen anderen
  Spieler ziehen" möglich — Admins sollten es kennen.
- Es gibt **keinen** periodischen Tick. Gesendet wird nur beim Flankenwechsel des
  Mindstates, alle Laufzeiten sind absolute Zeitstempel und werden lazy
  verglichen.

---

## Installation

1. `Psyerns_Hive_Mind_V1` zu einem PBO packen.
2. Über **`-mod`** laden — **nicht** `-serverMod`.
3. Server einmal starten — die Settings-Datei wird mit Defaults angelegt.

> **Wichtig:** Der Mod enthält seit der Admin-Debug-Karte Client-Inhalte (Layouts,
> Menü, Keybind). PBOs, die über `-serverMod` geladen werden, werden Clients nie
> angekündigt — die Karte funktioniert dann nicht, und je nach Setup werden
> Clients beim Join abgelehnt. Der Mod muss deshalb über `-mod` laufen und liegt
> damit bei **allen** Spielern.
>
> Die Hive-Logik selbst bleibt davon unberührt: sie ist durchgehend mit
> `IsDedicatedServer()` abgesichert und läuft ausschließlich auf dem Server.
> Ohne Whitelist-Eintrag bekommt ein Client keinerlei Hive-Daten.

Die Settings liegen unter dem `-profiles`-Pfad des Servers:

```
<profiles>/Psyerns_Hive_Mind/Settings/HiveMind.json
```

---

## Einstellungen

| Key | Typ | Default | Bedeutung |
|---|---|---|---|
| `Version` | int | `1` | Schemaversion. Nicht von Hand ändern. |
| `Enabled` | bool | `true` | Hauptschalter. Wirkt ohne Neustart. |
| `ActiveTimeWindow` | int | `0` | `0` = immer, `1` = nur nachts, `2` = nur tagsüber. |
| `UseCustomNightHours` | bool | `false` | `false` = Nacht kommt vom kartenkorrekten `World.IsNight()`. `true` = feste Uhrzeiten unten. |
| `NightStartHour` | int | `20` | Nachtbeginn (0–23), nur bei `UseCustomNightHours`. Mitternachts-Überlauf wird unterstützt. |
| `NightEndHour` | int | `6` | Nachtende (0–23). |
| **`ShareRadius`** | float | `100.0` | **Regler 1.** Teilungsradius in Metern um den meldenden Zombie. 100 = eine Zelle. Clamp 0–2000. |
| **`MaxSharedZombies`** | int | `16` | **Regler 2.** Harte Obergrenze, wie viele Zombies **gleichzeitig serverweit** markiert sein dürfen. Es gewinnen immer die nächsten. `3` = der Hardcore-Minimalmodus, `0` = niemand. Clamp 0–256. |
| `TriggerLevel` | int | `2` | Ab welchem Mindstate gesendet wird: `0` Disturbed, `1` Alerted, `2` Chase, `3` Fight. **Unter Chase liefert `GetTargetEntity()` erwiesenermaßen nichts** — `0`/`1` sind experimentell. |
| `BoostDurationSeconds` | float | `20.0` | Wie lange ein informierter Zombie hyperaufmerksam bleibt. Clamp 1–300. |
| `VisionRangeMultiplier` | float | `3.0` | Faktor auf den Vanilla-Sichtweitenwert, solange markiert. `1.0` = kein Effekt. Empirisch kalibrieren. Clamp 1–50. |
| `MaxRelayGenerations` | int | `1` | Kaskadenbremse: maximale Weiterleitungstiefe. `0` = nur wer den Spieler selbst gesehen hat, sendet. Clamp 0–5. |
| `RelayMemorySeconds` | float | `60.0` | Wie lange ein Zombie seine Kettentiefe behält. Wird automatisch auf mindestens `BoostDurationSeconds` angehoben — sonst wäre die Weiterleitungsgrenze wirkungslos. Clamp 1–600. |
| `SenderCooldownSeconds` | float | `8.0` | Sperrzeit pro Zombie nach erfolgreichem Senden. Clamp 0–120. |
| `MaxBroadcastsPerSecond` | float | `10.0` | Serverweite Obergrenze für Meldungen pro Sekunde. Token-Bucket, auf eine Sekunde Budget gedeckelt. Clamp 0.1–100. |
| `EnableNoisePing` | bool | `true` | Zweitkanal für Zombies ohne Sichtlinie. **Ohne ihn setzt sich die markierte Menge kaum in Bewegung** — Hordenlocken funktioniert dann nicht. Ignoriert `MaxSharedZombies`. |
| `NoiseConfigPath` | string | `CfgVehicles SurvivorBase NoiseShout` | Config-Pfad für `NoiseParams.LoadFromPath`. Ein erfundener Pfad schlägt still fehl. |
| `NoiseLifetimeSeconds` | float | `10.0` | Lebensdauer des Noise-Reizes in Sekunden. Vanilla nutzt 10 für Geschosseinschläge, 21 für Explosionen. Clamp 0.5–60. |
| `NoiseStrengthMultiplier` | float | `1.0` | Stärkefaktor auf die Noise-Config. Vanilla nutzt auch Werte > 1 (z.B. 2.0 für die lange Autohupe). Clamp 0–10. |
| `SettingsReloadSeconds` | float | `60.0` | Lädt die Datei im Sendepfad neu, falls älter als N Sekunden. `0` = aus. Spart Neustarts beim Kalibrieren. Clamp 0–3600. |
| `LogBroadcasts` | bool | `false` | Schreibt pro Meldung Sender, Kandidaten, Markierte und Generation ins RPT. Im Dauerbetrieb aus lassen. |
| `DebugMapEnabled` | bool | `false` | Hauptschalter der Admin-Karte. `false` = niemand bekommt Daten, auch niemand auf der Whitelist. |
| `DebugMapAdmins` | string[] | `[]` | Steam64-IDs, die die Karte öffnen dürfen. **Leere Liste = niemand.** Wird bei jedem Push neu geprüft, Entzug wirkt also sofort ohne Neustart. |
| `DebugMapIntervalSeconds` | float | `1.0` | Wie oft der Server einen Snapshot schickt. Clamp 0.25–10. |
| `DebugMapMaxNodes` | int | `150` | Wie viele markierte Zombies pro Snapshot übertragen werden. Begrenzt die Paketgröße. Clamp 0–400. |
| `DebugMapEventHistory` | int | `20` | Wie viele zurückliegende Meldungen im Verlauf gehalten werden. Clamp 0–100. |
| `DebugMapEventLifetime` | float | `20.0` | Nach wie vielen Sekunden eine Verbindung von der Karte verschwindet. Clamp 1–120. |

---

## Rezepte

**Hardcore, ganzer Tag (Standard)**
Defaults übernehmen.

**Nur nachts**
```json
"ActiveTimeWindow": 1
```

**Nur tagsüber**
```json
"ActiveTimeWindow": 2
```

**Massiv erhöht — halbe Stadt kommt**
```json
"ShareRadius": 400.0,
"MaxSharedZombies": 128,
"MaxBroadcastsPerSecond": 25.0
```

**Ganz weit runter — nie mehr als 3 Zombies**
```json
"MaxSharedZombies": 3,
"MaxRelayGenerations": 0,
"EnableNoisePing": false
```
Alle drei Werte gehören hier zusammen:
- `MaxSharedZombies: 3` — nie mehr als 3 gleichzeitig markierte Zombies serverweit.
- `MaxRelayGenerations: 0` — sonst rücken über die Kette laufend neue nach, sobald
  die alten ablaufen. Die Obergrenze von 3 *gleichzeitig* bleibt zwar bestehen,
  aber in Summe werden über die Zeit deutlich mehr Zombies alarmiert.
- `EnableNoisePing: false` — **wichtig.** Der Noise-Ping ist ein Engine-Broadcast an
  eine Weltposition und kennt keine Adressierung einzelner Zombies. Er ignoriert
  `MaxSharedZombies` vollständig und würde die 3er-Grenze faktisch aushebeln.

Der Preis: ohne Noise-Ping wirkt der Hive nur noch über Sichtlinie. Genau das ist
in diesem Minimalmodus aber gewollt.

**Feste Nachtzeiten statt Sonnenstand**
```json
"ActiveTimeWindow": 1,
"UseCustomNightHours": true,
"NightStartHour": 21,
"NightEndHour": 5
```

---

## Admin-Debug-Karte

Live-Ansicht des Hive im Spiel: welche Zombies gerade markiert sind, wer wen
informiert hat und wie weit der Teilungsradius wirklich reicht.

### Einrichten

1. Eigene Steam64-ID eintragen und den Schalter setzen:
   ```json
   "DebugMapEnabled": true,
   "DebugMapAdmins": ["76561198000000000"]
   ```
2. Server neu starten (oder `SettingsReloadSeconds` abwarten).
3. Im Spiel unter **Optionen → Steuerung → Psyerns Hive Mind** die Taste für
   *Hive Debug Map* belegen. Der Mod schreibt keine Taste vor.
4. Taste drücken. ESC oder dieselbe Taste schließt die Karte wieder.

### Was zu sehen ist

| Element | Bedeutung |
|---|---|
| 🔴 **SICHTER** | Zombie, der den Spieler tatsächlich selbst gesehen hat (hop 0 Quelle). |
| 🟢 **Spielername** | Der gesehene Spieler. Linie dorthin = die Sichtung selbst. |
| 🟠 Marker `hop 0` | Direkt vom Sichter informiert. |
| 🔵 Marker `hop ≥ 1` | Über die Relaiskette informiert. |
| Zahl am Marker | Restsekunden des Sicht-Boosts. |
| Farbige Linien | Wer wen informiert hat. Blassen mit dem Alter aus. |
| Graue Linien | Bereits markierter Zombie, dessen Timer nur aufgefrischt wurde. |
| Roter Ring | `ShareRadius` um einen frischen Sichter — so siehst du direkt, ob der Radius passt. |

Das Infofeld links oben zeigt Zeitfenster-Status, `markiert / cap`, Registry-Größe,
Radius, Sichtmultiplikator, Weltzeit und Relaistiefe.

### Sicherheit

Die Autoritätsprüfung liegt **ausschließlich auf dem Server**. Der Client kann nur
ein Abo *anfragen*; ob er Daten bekommt, entscheidet die Whitelist, und sie wird
bei **jedem** Push neu geprüft. Ein nicht gelisteter Spieler bekommt nichts —
ein Abholversuch landet als Warnung im RPT.

### Kosten

Ohne offene Karte kostet das Feature einen Bool-Vergleich pro Sekunde
(`PHM_DebugTracker.IsRecording()` im Timer) und einen weiteren pro Hive-Meldung.
Es wird nichts aufgezeichnet, solange niemand zuschaut.

---

## Kalibrieren

1. `"LogBroadcasts": true` setzen.
2. Ins RPT schauen: `[PHM][DBG] broadcast hop=… candidates=… newlyMarked=… markedTotal=…`
3. `candidates` zu niedrig → `ShareRadius` erhöhen.
4. Zombies reagieren nicht sichtbar → `VisionRangeMultiplier` schrittweise
   erhöhen und/oder `NoiseStrengthMultiplier`.
5. `SettingsReloadSeconds` steht auf 60 — Änderungen greifen ohne Neustart.

---

## Vor dem produktiven Einsatz testen

`EEDelete` wird von keiner Vanilla- oder Expansion-Creature-Klasse überschrieben;
ob die Engine es für `DayZCreature`-Ableitungen überhaupt feuert, ist aus den
Script-Quellen nicht beweisbar. Der Mod meldet Zombies deshalb **dreifach**
abgesichert ab (Destruktor, `EEDelete`, plus Null-/`IsSetForDeletion`-/`IsAlive`-
Prüfung in jeder Auswahlschleife). Ein Lauftest mit Zombie töten **und**
CE-Despawn abwarten ist trotzdem empfohlen.
