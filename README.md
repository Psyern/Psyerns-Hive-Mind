# Psyerns Hive Mind

<p align="center">
  <img src="https://img.shields.io/badge/DayZ-1.29+-0074D9?style=for-the-badge&logo=steam&logoColor=white" alt="DayZ 1.29+">
  <img src="https://img.shields.io/badge/Enforce_Script-Enfusion-FF851B?style=for-the-badge" alt="Enforce Script">
  <img src="https://img.shields.io/badge/Dependencies-Zero-2ECC40?style=for-the-badge" alt="Zero Dependencies">
  <a href="LICENSE"><img src="https://img.shields.io/badge/License-MIT-green?style=for-the-badge" alt="License MIT"></a>
</p>

<p align="center">
  <img src="https://img.shields.io/badge/Authority-Server_Side-8E44AD?style=flat-square" alt="Server Side">
  <img src="https://img.shields.io/badge/Infected-Shared_Vision-E74C3C?style=flat-square" alt="Shared Vision">
  <img src="https://img.shields.io/badge/Config-JSON_Hot_Reload-F0C040?style=flat-square" alt="JSON Config">
  <img src="https://img.shields.io/badge/Admin-In_Game_Debug_Map-1ABC9C?style=flat-square" alt="Debug Map">
</p>

<p align="center">
  <b>All infected share one neural network. If one sees you, prepare yourself.</b><br>
  A hardcore mutation for the Knox region &mdash; and a tool, if you are clever enough to use it.
</p>

<p align="center">
  <a href="https://deadmans-echo.de">
    <img src="https://img.shields.io/badge/Community-Deadmans_Echo-F0C040?style=for-the-badge" alt="Deadmans Echo">
  </a>
</p>

---

## Repository Layout

This repository is the DayZ mod itself &mdash; no companion services, no external backend, no build step.

```text
Psyerns_Hive_Mind_V1/               ← DayZ mod (this README)
├── config.cpp                      ← CfgPatches / CfgMods, script modules, inputs
├── stringtable.csv                 ← keybind labels
├── gui/layouts/                    ← admin debug map layouts
└── scripts/
    ├── data/Inputs.xml             ← debug map keybind (default F10)
    ├── 3_Game/                     ← settings, constants, enums, RPC ids, DTOs
    ├── 4_World/                    ← hive core, infected registry, engine hook
    └── 5_Mission/                  ← server entry, client entry, map menu
```

The mod is **standalone and zero-dependency**: `requiredAddons[]` contains nothing but `"DZ_Data"`.

---

## Features

<table>
<tr>
<td width="33%" valign="top">

### Hive Core
- Shared field of vision
- Engine-native perception boost
- Noise channel for no-LoS pull
- Event driven, no per-zombie tick
- Own infected registry
- Vanilla navmesh pathing
- Relay chains with hop depth

</td>
<td width="33%" valign="top">

### Customization
- All day / night only / day only
- Native or fixed night hours
- Share radius 0&ndash;2000 m
- Hard cap down to 3 infected
- Configurable trigger mind state
- Relay depth limit
- Live config reload

</td>
<td width="33%" valign="top">

### Safety &amp; Ops
- Server authoritative throughout
- Four independent cascade brakes
- Global broadcast rate limit
- No engine space queries
- Admin debug map (Steam64 gated)
- RPT diagnostics on demand
- Fail-closed on every guard

</td>
</tr>
</table>

### How It Works

DayZ exposes **no target setter for infected**. `DayZInfectedInputController` is getter-only, and `ZombieBase.m_ActualTarget` is overwritten from the engine every tick. The mod therefore uses the only two channels that are actually provable against the 1.29 sources:

| Channel | What it does | Deterministic |
|---|---|---|
| `AITargetCallbacksPlayer.GetMaxVisionRangeModifier` | The only script hook the engine calls **per target AND per observing AI**. Marked infected receive a boosted vision range; vanilla pathing does the rest. | Yes &mdash; the mark is a pure script flag |
| `NoiseSystem.AddNoiseTarget` | A stimulus at the player position that also reaches infected **without line of sight**. This is what makes luring hordes possible. | No &mdash; engine broadcast, ignores the cap |

Because the mark exists only as the mod's own flag on the zombie, both the share radius and the server-wide cap on simultaneously marked infected are **exact and reproducible**. Candidate selection walks a static registry the mod maintains itself &mdash; there is not a single `GetObjectsAtPosition` call anywhere in the mod.

> **Line of sight still applies.** The vision boost scales *range*, it does not see through walls. A marked zombie behind a building is pulled in by the noise channel until it gains sight. That is deliberate: it keeps the feature fair.

### Cascade Brakes

One sighting must never cascade across the whole map. Four independent brakes work together:

| Brake | Effect |
|---|---|
| Relay depth (`MaxRelayGenerations`) | Limits how many times an alert may be passed on |
| Sender cooldown (`SenderCooldownSeconds`) | Limits how often a single zombie may broadcast |
| Re-arm guard | One rising mind state edge produces exactly one broadcast |
| Global rate limit (`MaxBroadcastsPerSecond`) | Token bucket, capped at one second of budget |

---

## Quick Start

```
1. Add Psyerns_Hive_Mind_V1 to your server mod load order (-mod, not -serverMod)
2. Start the server → HiveMind.json auto-generates with defaults
3. Defaults are already the intended hardcore experience: all day, 100 m radius
4. Tune ShareRadius / MaxSharedZombies → changes apply live after 60 s
```

---

## Script Structure

```text
config.cpp                          ← requiredVersion 0.1, requiredAddons DZ_Data,
                                      inputs = scripts/data/Inputs.xml
stringtable.csv                     ← STR_PHM_GROUP, STR_PHM_HIVE_MAP_TOGGLE
gui/layouts/
├── phm_hive_map.layout             ← admin map screen
└── phm_hive_line.layout            ← single pooled connection line
scripts/
├── data/Inputs.xml                 ← UAPHMHiveMapToggle, preset kF10
├── 3_Game/
│   ├── PHM_Constants.c             ← paths, clamp bounds, colors, input names
│   ├── PHM_Enums.c                 ← EPHM_TimeWindow, EPHM_TriggerLevel
│   ├── PHM_Logger.c                ← RPT wrapper, debug output gated
│   ├── PHM_Settings.c              ← JSON DTO, Defaults(), Validate(), admin check
│   ├── PHM_SettingsHolder.c        ← file IO, migration, lazy hot reload
│   ├── PHM_TimeGate.c              ← day / night decision
│   ├── PHM_RPC.c                   ← RPC ids (>= 10000)
│   └── PHM_DebugData.c             ← snapshot DTOs for the debug map
├── 4_World/
│   ├── PHM_ZombieBase.c            ← registry, hive state, mind state trigger
│   ├── PHM_HiveManager.c           ← selection, cap enforcement, noise ping
│   ├── PHM_AITargetCallbacksPlayer.c ← the engine perception hook
│   └── PHM_DebugTracker.c          ← records edges, builds snapshots
└── 5_Mission/
    ├── PHM_MissionServer.c         ← settings load, RPC receive, snapshot push
    ├── PHM_MissionGameplay.c       ← keybind registration, client RPC receive
    ├── PHM_HiveMapClient.c         ← subscription glue
    └── PHM_HiveMapMenu.c           ← the map screen itself
```

> **No per-zombie tick.** Broadcasting is driven by the mind state edge in `HandleMindStateChange`, and every lifetime is an absolute timestamp from `GetTickTime()` compared lazily. The mod runs exactly two server-wide repeating timers: the noise refresher (cadence `NoiseLifetimeSeconds`, early-outs on two float compares while the hive is idle) and the debug map push, whose callback returns immediately while nobody is watching. Neither scales with the number of infected.

## Profile Structure

```text
profiles/<your-profile>/Psyerns_Hive_Mind/
└── Settings/
    └── HiveMind.json               ← auto-generated on first start
```

---

## Configuration

**One file for everything:** `$profile:Psyerns_Hive_Mind\Settings\HiveMind.json`

```json
{
    "Version": 1,
    "Enabled": true,
    "ActiveTimeWindow": 0,
    "UseCustomNightHours": false,
    "NightStartHour": 20,
    "NightEndHour": 6,
    "ShareRadius": 100.0,
    "MaxSharedZombies": 16,
    "TriggerLevel": 2,
    "BoostDurationSeconds": 20.0,
    "VisionRangeMultiplier": 3.0,
    "MaxRelayGenerations": 1,
    "RelayMemorySeconds": 60.0,
    "SenderCooldownSeconds": 8.0,
    "MaxBroadcastsPerSecond": 10.0,
    "EnableNoisePing": true,
    "NoiseConfigPath": "CfgVehicles SurvivorBase NoiseShout",
    "NoiseLifetimeSeconds": 10.0,
    "NoiseStrengthMultiplier": 1.0,
    "SettingsReloadSeconds": 60.0,
    "LogBroadcasts": false,
    "DebugMapEnabled": false,
    "DebugMapIntervalSeconds": 1.0,
    "DebugMapMaxNodes": 150,
    "DebugMapEventHistory": 20,
    "DebugMapEventLifetime": 60.0,
    "DebugMapAdmins": []
}
```

> **Auto-Upgrade:** The config carries a `Version` field. When the mod ships new fields, the server detects the outdated version, keeps every existing value and writes the file back with the missing defaults added. No manual editing required.

> **Validation:** Every value passes `Math.Clamp` after loading. No number read from JSON ever reaches the selection loop unchecked.

### General

| Field | Default | Description |
|-------|---------|-------------|
| `Version` | `1` | Schema version &mdash; used for auto-upgrade, do not edit manually |
| `Enabled` | `true` | Master switch. Takes effect live, no restart needed |
| `SettingsReloadSeconds` | `60.0` | Re-reads the file from the broadcast path when older than N seconds. `0` disables. Clamp 0&ndash;3600 |
| `LogBroadcasts` | `false` | Writes sender, candidates, newly marked and hop depth to the RPT. Leave off in production |

### Time Window

| Field | Default | Description |
|-------|---------|-------------|
| `ActiveTimeWindow` | `0` | `0` always, `1` night only, `2` day only |
| `UseCustomNightHours` | `false` | `false` uses the map-correct native `World.IsNight()`. `true` uses the fixed hours below |
| `NightStartHour` | `20` | Night start (0&ndash;23). Midnight wrap supported |
| `NightEndHour` | `6` | Night end (0&ndash;23) |

### Hive Behaviour

| Field | Default | Description |
|-------|---------|-------------|
| **`ShareRadius`** | `100.0` | **Dial 1.** Share radius in metres around the reporting zombie. 100 equals one cell. Clamp 0&ndash;2000 |
| **`MaxSharedZombies`** | `16` | **Dial 2.** Hard ceiling on how many infected may be marked **simultaneously, server wide**. Nearest always win. `3` is the hardcore minimum, `0` disables sharing. Clamp 0&ndash;256 |
| `TriggerLevel` | `2` | Mind state that triggers a broadcast: `0` Disturbed, `1` Alerted, `2` Chase, `3` Fight |
| `BoostDurationSeconds` | `20.0` | How long an informed zombie stays hyper-aware. Clamp 1&ndash;300 |
| `VisionRangeMultiplier` | `3.0` | Factor on the vanilla vision range value while marked. `1.0` is no effect. Clamp 1&ndash;50 |

> **`TriggerLevel` below Chase is experimental.** Vanilla only ever reads `GetTargetEntity()` in `MINDSTATE_CHASE` and `MINDSTATE_FIGHT` (`ZombieBase.c:679` / `:720`, guarded by the comment *"we attack only in chase & fight state"*). There is no evidence it returns anything below Chase, so `0` and `1` may simply never fire.

> **`VisionRangeMultiplier` is a factor, not a distance.** Vanilla returns roughly 0.225&ndash;1.25 from this callback (`PlayerConstants.AI_VISIBILITY_*`). The engine-side conversion to metres is not exposed to script, so the value has to be calibrated empirically on your server.

### Cascade Brakes

| Field | Default | Description |
|-------|---------|-------------|
| `MaxRelayGenerations` | `1` | Maximum relay depth. `0` means only infected that saw the player themselves broadcast. Clamp 0&ndash;5 |
| `RelayMemorySeconds` | `60.0` | How long a zombie remembers its hop depth. Automatically raised to at least `BoostDurationSeconds` &mdash; otherwise the relay limit would be meaningless. Clamp 1&ndash;600 |
| `SenderCooldownSeconds` | `8.0` | Lockout per zombie after a successful broadcast. Clamp 0&ndash;120 |
| `MaxBroadcastsPerSecond` | `10.0` | Server-wide ceiling on broadcasts per second. Token bucket, capped at one second of budget. Clamp 0.1&ndash;100 |

### Noise Channel

| Field | Default | Description |
|-------|---------|-------------|
| `EnableNoisePing` | `true` | Second channel that reaches infected without line of sight. **Without it, luring hordes does not work.** Ignores `MaxSharedZombies` |
| `NoiseConfigPath` | `CfgVehicles SurvivorBase NoiseShout` | Config path for `NoiseParams.LoadFromPath`. An invented path fails silently |
| `NoiseLifetimeSeconds` | `10.0` | How long the stimulus lives. Vanilla uses 10 for bullet impacts, 21 for explosions. Clamp 0.5&ndash;60 |
| `NoiseStrengthMultiplier` | `1.0` | Strength factor on the noise config. Vanilla uses values above 1 as well (2.0 for the long car horn). Clamp 0&ndash;10 |

### Admin Debug Map

| Field | Default | Description |
|-------|---------|-------------|
| `DebugMapEnabled` | `false` | Master switch. `false` means nobody receives data, not even whitelisted admins |
| `DebugMapAdmins` | `[]` | Steam64 IDs allowed to open the map. **Empty list means nobody.** Re-checked on every push, so revoking takes effect immediately |
| `DebugMapIntervalSeconds` | `1.0` | Snapshot push interval. Clamp 0.25&ndash;10 |
| `DebugMapMaxNodes` | `150` | Marked infected transmitted per snapshot. Bounds the packet size. Clamp 0&ndash;400 |
| `DebugMapEventHistory` | `20` | Past broadcasts kept in the history. Clamp 0&ndash;100 |
| `DebugMapEventLifetime` | `60.0` | Seconds until a connection fades from the map. Clamp 1&ndash;120 |

---

## Presets

### Hardcore, all day (default)

Ship as-is.

### Night only

```json
"ActiveTimeWindow": 1
```

### Day only

```json
"ActiveTimeWindow": 2
```

### Massively increased &mdash; half the town comes

```json
"ShareRadius": 400.0,
"MaxSharedZombies": 128,
"MaxBroadcastsPerSecond": 25.0
```

### All the way down &mdash; never more than 3 infected

```json
"MaxSharedZombies": 3,
"MaxRelayGenerations": 0,
"EnableNoisePing": false
```

All three values belong together:

- `MaxSharedZombies: 3` &mdash; never more than 3 marked infected server wide
- `MaxRelayGenerations: 0` &mdash; otherwise the chain keeps feeding new infected in as old marks expire. The cap of 3 *simultaneous* still holds, but far more infected get alerted over time
- `EnableNoisePing: false` &mdash; **important.** The noise ping is an engine broadcast to a world position with no per-zombie addressing. It ignores `MaxSharedZombies` entirely and would defeat the limit

The price: without the noise ping the hive only works along line of sight. In this minimal mode that is exactly the point.

### Fixed night hours instead of sun position

```json
"ActiveTimeWindow": 1,
"UseCustomNightHours": true,
"NightStartHour": 21,
"NightEndHour": 5
```

---

## Admin Debug Map

Live in-game view of the hive: which infected are marked, who informed whom, and how far the share radius actually reaches.

### Setup

1. Add your Steam64 ID and flip the switch:
   ```json
   "DebugMapEnabled": true,
   "DebugMapAdmins": ["76561198000000000"]
   ```
2. Restart the server, or wait for `SettingsReloadSeconds`
3. Press **F10** in game. Rebind it under **Options &rarr; Controls &rarr; Psyerns Hive Mind &rarr; Hive Debug Map**
4. ESC closes the map again

> The key only opens the map. Whether it shows anything is decided entirely by the server: without `DebugMapEnabled` and a matching entry in `DebugMapAdmins`, the map stays on *"warte auf Server ..."* because no snapshot is ever sent.

### What you see

| Element | Meaning |
|---|---|
| **SICHTER** marker (red) | The zombie that actually saw the player, plus a line to the player it saw |
| Marker `hop 0` (amber) | Informed directly by the spotter |
| Marker `hop >= 1` (blue) | Informed through the relay chain |
| Number on the marker | Remaining seconds of the vision boost |
| Coloured lines | Who informed whom. Fade out with age |
| Grey lines | Already marked zombie whose timer was only refreshed |
| Red ring | `ShareRadius` around a fresh spotter &mdash; shows immediately whether the radius fits |

The panel top left shows time window status, `marked / cap`, registry size, radius, vision multiplier, world time and relay depth.

### Security

The authority check lives **exclusively on the server**. A client can only *request* a subscription; whether it receives anything is decided by the whitelist, and the whitelist is re-checked on **every** push. A player not on the list receives nothing.

### Cost

Snapshots are only built and sent while a whitelisted admin actually has the map open. History recording (edges, spotters) runs whenever `DebugMapEnabled` is set &mdash; bounded arrays, a handful of small allocations per broadcast &mdash; so that opening the map *after* a fight still shows what happened. With `DebugMapEnabled` off the whole feature costs one settings read per broadcast.

---

## Installation

### Requirements

| | |
|---|---|
| **DayZ** | 1.29+ |
| **Dependencies** | None &mdash; standalone mod |
| **Load order** | `-mod` &mdash; **not** `-serverMod` |

### Step-by-Step

1. Pack `Psyerns_Hive_Mind_V1` into a PBO
2. Add it to your server mod load order via **`-mod`**
3. Start the server once &mdash; `HiveMind.json` generates with defaults
4. Tune the settings, they apply live after `SettingsReloadSeconds`

> **Why `-mod` and not `-serverMod`.** Since the admin debug map the mod ships client content: layouts, a menu and a keybind. PBOs loaded through `-serverMod` are never announced to clients, so the map would not work and, depending on the setup, clients get rejected on join. The mod therefore has to run through `-mod` and lives with **every** player.
>
> The hive logic itself is unaffected: it is guarded with `IsDedicatedServer()` throughout and runs on the server only. Without a whitelist entry a client receives no hive data whatsoever.

---

## Calibration

1. Set `"LogBroadcasts": true`
2. Watch the RPT: `[PHM][DBG] broadcast hop=... candidates=... newlyMarked=... markedTotal=...`
3. `candidates` too low &rarr; raise `ShareRadius`
4. Infected do not react visibly &rarr; raise `VisionRangeMultiplier`, and/or `NoiseStrengthMultiplier`
5. `SettingsReloadSeconds` defaults to 60, so changes apply without a restart

---

## Engine Notes

Findings from building this mod, verified against the DayZ 1.29 sources. They are the reason the mod is built the way it is.

| Finding | Consequence |
|---|---|
| No target setter exists for infected. `DayZInfectedInputController` is getter-only, `m_ActualTarget` is overwritten every tick from `GetTargetEntity()` | The hive works through perception, never by steering zombies directly |
| `GetMaxVisionRangeModifier(EntityAI pApplicant)` is called per target **and** per observing AI. `pApplicant` is the observing infected | The only hook that can express "this specific zombie perceives that specific player better" |
| `AITargetCallbacksPlayer` is instantiated server-side only (`PlayerBase.c:6033-6042`) | A `modded class` on it is inherently server authoritative |
| `DayZInfectedConstants` shares its ordinal range with the `COMMANDID_*` entries, so `MINDSTATE_CALM` is 7 and `MINDSTATE_FIGHT` is 11 &mdash; but `ZombieBase.Init` registers `m_MindState` with `RegisterNetSyncVariableInt(-1, 4)` | The replicated mind state cannot carry the real value. `GetMindStateSynced()` is unusable; the mod reads the server-side value and compares only against named constants |
| `EEDelete` is overridden by no vanilla or Expansion creature class, so it is unprovable from script whether the engine fires it for `DayZCreature` derivatives | Infected are unregistered through **three** redundant paths: destructor, `EEDelete`, plus null / `IsSetForDeletion` / `IsAlive` checks in every selection loop |
| Expansion's `ModCommandHandlerBefore` returns `true` for lobotomised infected **without** calling `super` | Countdown timers in that hook would silently stall. The mod uses absolute timestamps instead and overrides the hook not at all |
| `UAInputAPI.RegisterInput` / `RegisterGroup` are declared (`UAInput.c:191` / `:194`) but have **zero call sites** in vanilla 1.29 and zero in Expansion. Registering a keybind from script produces no usable input | Keybinds must be declared in an `Inputs.xml` referenced by the `inputs` property in `CfgMods` &mdash; the mechanism every Expansion module with a hotkey uses (`Book/Scripts/Data/Inputs.xml` + `Book/Scripts/config.cpp:20`). Script only resolves the input with `GetUApi().GetInputByName(...)` |

### Status

**Verified on a live dedicated server** alongside CF, Community Online Tools and the full DayZ Expansion bundle: the mod compiles, loads its settings, and the hive core works &mdash; broadcasts fire on the mind state edge, the cap holds at exactly `MaxSharedZombies`, and relay hops reach depth 1 as configured.

Still unverified:

- **The line geometry on the debug map.** The code assumes `SetRotation` pivots around the widget centre. If lines appear offset in game, the pivot is the corner instead and only the placement in `DrawLine` needs to change
- **`EEDelete` behaviour for infected** &mdash; kill a zombie *and* wait out a CE despawn, with the triple unregister in place as the safety net

---

## Credits

<p align="center">
  <b>Author:</b> <a href="https://steamcommunity.com/profiles/76561198043039918/">Psyern</a><br><br>
  <b>Community:</b> <a href="https://deadmansecho.com">Deadmans Echo</a><br><br>
  Built for players who think the Knox region was not hostile enough.
</p>

---

## License

Psyerns Hive Mind is licensed under the **MIT License** &mdash; see [`LICENSE`](LICENSE).

You may use, modify, repack and redistribute this mod, including commercially, as
long as the copyright notice and the license text stay with it.

The mod contains no third-party code. It targets the vanilla DayZ script API only
and has no dependency on DayZ Expansion, Community Framework or Dabs Framework &mdash;
`requiredAddons[]` lists nothing but `"DZ_Data"`.
