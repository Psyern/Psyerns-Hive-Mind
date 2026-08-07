# Psyerns Hive Mind v2 — Script-Driven Infected AI

**Status:** approved design, not yet implemented
**Date:** 2026-08-08
**Supersedes:** the `EnablePursuit` co-pilot layer added in `79b8316`

---

## 1. Why

v1 marks infected when one of them spots a player, then tries to make the marked set act on that
knowledge through three channels. Live server logs (`log_storage/1786102332`, `log_storage/1786138934`)
prove the bookkeeping works and the behaviour does not:

| Observed | Evidence |
|---|---|
| Marking, relay and cap all correct | 22 broadcasts, hop 0→3, `newlyMarked=60 cap=60` |
| Doors never open | 27 attempts, 0 successes |
| Ladders never climbed | 3 attempts, all refused `walk path reaches target` |
| Distant marked infected do not approach | `driving=0` on all 41 refresher ticks |

The root cause is that v1's channels are **perception, not motion**:

- `GetMaxVisionRangeModifier` scales sight *range* and still requires line of sight. Its vanilla
  output range is 0.225–1.25 (`AITargetCallbacksPlayer.c:80`), multiplied onto a configured infected
  vision range that is nowhere near 300 m.
- `NoiseSystem.AddNoiseTarget` is an engine broadcast whose reach comes from the noise config, **not**
  from `ShareRadius`. v1 uses `CfgVehicles SurvivorBase NoiseShout` — the player *shout*. Vanilla uses
  `cfgAmmo … NoiseExplosion` for long pulls (`DayZGame.c:3488`).
- The `EnablePursuit` layer added later does move infected, but was built on `OverrideHeading`, which
  has **zero vanilla call sites for infected**.

A mark 250 m away behind a treeline was therefore told nothing it could act on.

## 2. Decision

Take **full script control of marked infected**, modelled on vanilla's own infected possession tool
rather than on DayZ Expansion's eAI.

This was chosen over two alternatives after mapping Expansion's AI (13-agent workflow, 260 files,
~48k lines):

| Approach | Verdict |
|---|---|
| Stimulus-only (navmesh breadcrumb trail of noise targets) | Rejected — doors stay shut forever, delivers "the horde drifts your way", not "the horde comes" |
| Co-pilot overlay (drive only while `GetTargetEntity() == null`) | Rejected — two writers on one input controller with no arbitration mode |
| **Full script brain** | **Chosen** |

### Why Expansion is a model and not a dependency

`class eAIBase: PlayerBase` (`eAIBase.c:20`). Expansion's AI are **player entities**; their FSM,
movement commands, weapon handling and animation graph all sit on the human stack. Infected are
`ZombieBase : DayZInfected : DayZCreatureAI` — a disjoint branch with a different input controller,
a different command set and a different animation graph. Nothing is reusable as-is.

The mod therefore takes Expansion's *architecture* (separated perception / navigation / motor layers,
budgeted central scheduling) and depends on **no Expansion code**, so it keeps working on servers
without Expansion.

### The reference implementation is in vanilla

`PluginDayZInfectedDebug` already does exactly this, and is the proof that possession works on infected:

```c
// PluginDayZInfectedDebug.c:368 — suspend the native brain
m_ControlledInfected.GetAIAgent().SetKeepInIdle(true);

// :385-419 — drive it, per tick, from CommandHandler
infected.GetInputController().OverrideMovementSpeed(true, speed);
DayZInfectedCommandMove moveCommand = infected.GetCommand_Move();
moveCommand.SetStanceVariation(variation);
moveCommand.SetIdleState(mindState);
moveCommand.StartTurn(direction, turnType);
infected.StartCommand_Vault(vaultType);
infected.StartCommand_Crawl(crawlType);
infected.StartCommand_Attack(null, attackType, attackDir);
```

Two consequences that shape the whole design:

1. **Turning is a discrete command**, `StartTurn(direction, turnType)` — not a continuous heading
   channel. The route follower issues a turn when angular error crosses a threshold, and writes only
   speed on every other tick. This is the concrete correction to v1's `OverrideHeading` approach.
2. **Vault survives possession.** `StartCommand_Vault` is callable directly, so suspending the brain
   does not cost obstacle traversal even though `IsVault()` / `GetVaultHeight()` go quiet.

## 3. Scope

**Only hive-marked infected are possessed.** Everything else on the server stays vanilla.

This is a deliberate blast-radius decision, not a performance one. Possession is a pinned state; a bug
that fails to release leaves an infected paralysed. Bounding the possessed set to `MaxSharedZombies`
(currently 60, hard clamp 256) and gating the whole feature behind a default-off config switch means a
failure is visible, local and revertible by editing one JSON line.

## 4. Architecture

```
PHM_HiveManager        marking, relay, registry, cap        (carried over from v1)
       │ marks
       ▼
PHM_Brain              one instance per possessed infected
       ├─ Perception   own target acquisition
       ├─ Navigator    route to destination
       ├─ Motor        engine command translation
       ├─ Obstacle     vault, doors, stuck recovery
       └─ Combat       attack execution
```

Each layer is independently testable and has one job. `PHM_Brain` owns possession lifecycle and
nothing else; the layers below it never call `SetKeepInIdle`.

### Layer responsibilities

| Layer | Job | Primary API | Proven for infected? |
|---|---|---|---|
| Motor | "go this way at this speed" → engine commands | `OverrideMovementSpeed`, `GetCommand_Move().StartTurn`, `SetIdleState`, `SetStanceVariation` | **Yes** — `PluginDayZInfectedDebug.c:385-396` |
| Navigator | destination → waypoint list | `AIWorld.FindPath`, `SampleNavmeshPosition` | Yes — `AIWorld.c:98/122` |
| Perception | who is the target | `CGame.GetPlayers`, `DayZPhysics.RaycastRV` | Yes (generic APIs) |
| Obstacle | vault / door / unstick | `StartCommand_Vault`, `GetVaultType`, `SphereCastBullet`, `Building.OpenDoor` | Yes — `ZombieBase.c:427/438`, `Building.c:50` |
| Combat | attack | `CanAttackToPosition`, `StartCommand_Attack` | Yes — `ZombieBase.c:692/709` |

### Per-tick data flow

Hook is `override void CommandHandler(...)` with `super` called **first** — not
`ModCommandHandlerBefore`, which is skipped whenever an earlier vanilla branch returns and is hijacked
outright by DayZExpansion Core for lobotomised infected (returns `true` without calling super).
Expansion Core itself uses the `CommandHandler` override.

```
CommandHandler
  └─ super.CommandHandler(...)          always first
  └─ if not possessed → cheap field-read exit
  └─ Perception   (throttled, not every tick)
  └─ Navigator    (only when repath is due)
  └─ Motor        (every tick — speed always, turn on angular-error threshold)
  └─ Obstacle     (only when the stuck detector latches)
```

### Scheduling

Pathfinding is centrally budgeted off `DayZGame.OnUpdate`: at most `PathsPerFrame` `FindPath` calls per
frame across the whole possessed set, processed round-robin so no zombie starves. v1 gave each zombie
its own repath timer with jitter, which makes worst-case frame cost a function of horde size. A central
budget makes it a constant that is chosen, not observed.

`PathsPerFrame` is a config field with a hard clamp; its default is set from the step-2 measurement, not
guessed now. Until that measurement exists there is no defensible number, and writing one into this spec
would only make a guess look like a decision.

## 5. Open unknowns

These cannot be answered from source and are what the first build step measures. **No other component
is built before these are resolved.**

| # | Unknown | Why it matters | If it goes badly |
|---|---|---|---|
| 1 | Does `GetTargetEntity()` stay dead under `SetKeepInIdle(true)`? | Decides whether Perception must be rebuilt at all | If it survives, Perception is deleted and the design gets much cheaper |
| 2 | Unit domain of `OverrideMovementSpeed` for infected — the `0..3` `DayZInfectedConstantsMovement` scale, or m/s? | Every speed literal is a guess until measured. Vanilla's only float mapping (0/2/3/5) is for **animals** (`DayZAnimal.c:531-549`), while infected read back the 0..3 enum (`ZombieBase.c:300`) | Wrong scale = sliding or frozen zombies |
| 3 | `StartTurn(direction, turnType)` semantics — is `direction` absolute yaw or relative delta, and what are the legal turn types? | The follower's core loop depends on it. Legal values live in the debug `.layout`, not in script | Follower cannot steer |
| 4 | Does animation survive multi-minute possession? | Vanilla only possesses interactively; sustained use across dozens of entities is untested | Fall back to stimulus-only |

## 6. Failure handling

- **Config gate** `EnablePossession`, default `false`.
- **Guaranteed release.** `SetKeepInIdle(false)` plus override release on every exit path: death,
  `EEDelete`, mark expiry, config flip, hot reload, and a belt-and-braces sweep in `PruneMarked`.
  v1 already gets this shape right (`PHM_StopDriving`) and it must survive the rewrite.
- **Fail to vanilla, never to half-driven.** If any layer errors or returns no result, the brain
  releases possession rather than continuing with partial control.
- **Idempotent release.** Releasing a never-possessed infected is a no-op.

## 7. Build order

Sequenced so the riskiest unproven assumption is tested first.

| Step | Component | In-game proof required |
|---|---|---|
| 0 | **Motor probe.** Debug command possesses one infected and drives it at a fixed bearing for ~15 s. Sweeps both speed scales and both `StartTurn` interpretations. No pathing, no marking, no doors. | Zombie translates in the commanded direction; walk/run animation plays with foot contact, not sliding; it turns rather than snapping; exactly one speed scale gives a sane gait. Resolves all four unknowns. |
| 1 | Motor layer proper + guaranteed release | Possess/release cycles leave no paralysed infected; killing a possessed zombie mid-drive restores vanilla cleanly |
| 2 | Navigator + central budgeted scheduler | 60 possessed infected converge across a town with per-frame `FindPath` count at the configured cap and no RPT frame-time regression |
| 3 | Perception (only if unknown #1 says it is needed) | Possessed infected acquire and pursue a player they did not previously see |
| 4 | Obstacle: stuck detector → `SphereCastBullet` along the commanded bearing → door open; vault via `StartCommand_Vault` | Door-open count > 0 on the first run; locked doors log a refusal and stay shut; low fences get vaulted |
| 5 | Combat via `StartCommand_Attack` | Possessed infected damage the player at melee range |
| 6 | Hardening: telemetry, Expansion Core co-existence, config surface | Running alongside Expansion Core produces no frozen or double-driven infected |

## 8. Explicitly out of scope

Carried over from the Expansion mapping — these solve *human* problems and are discarded with reason:

- **Ladder climbing.** Deleted from v1 as part of this work. Infected have no `COMMANDID_LADDER` and no
  ladder animation; the full command set is `MOVE/VAULT/DEATH/HIT/ATTACK/CRAWL/SCRIPT`
  (`DayZInfected.c:4-10`). Every possible implementation is a `SetPosition` teleport, which is a cheat,
  not a behaviour. Vault stays and is genuine.
- **All weapon handling, stamina, inventory, hands.** No `WeaponManager`, no `StaminaHandler`, no
  `GetHumanInventory` above `DayZCreatureAI`.
- **The stance system.** Zero occurrences of `STANCEIDX` in the creature tree. `SetStanceVariation` is
  a cosmetic idle variant randomised at spawn, not a stance.
- **Strafe, lean, weapon raise, aim.** No lateral axis exists — turn-then-walk is the only expressible
  motion.
- **Cover, flanking, formations.** `AIGroup` exposes only `AddAgent/RemoveAgent/GetBehaviour`; there is
  no leader or formation slot. A zombie pack wants a loose swarm, which the mark radius already gives.
- **Swimming, vehicles, fall-damage path safety, unconsciousness, self-care.** No substrate on the
  creature branch for any of them. `PGPolyFlags.SWIM`/`SWIM_SEA` go permanently into the filter's
  exclude mask.
- **Expansion's per-group target push and dogpile suppression.** These require pushing a target into
  another AI. `DayZInfectedInputController` exposes only `GetMindState()` and `GetTargetEntity()`, both
  read-only, and there is no target setter anywhere in 1.29. Coordination stays spatial.

## 9. What v1 keeps

The hive layer is sound and stays verbatim: static registry, marking with distance-ranked selection,
relay hop accounting with memory, token-bucket rate limiting, `PruneMarked`, the settings holder with
hot reload, the admin debug map, and the RPT telemetry. Only the *effector* is replaced.
