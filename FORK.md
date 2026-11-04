# About this fork

This is a fork of [YimMenuV2](https://github.com/YimMenu/YimMenuV2) for GTA V Enhanced. It adds network protections and modder detection for hosting public sessions, plus a few smaller features and fixes. Installing and opening the menu work the same as upstream: see the [README](README.md).

## Branches

| Branch | What it is |
|---|---|
| `combined` | Every branch below merged together. Build this one to get everything. |
| `enhanced` | An unchanged copy of upstream's `enhanced` branch. |
| `feature/...`, `fix/...` | One change each, as a single commit on top of `enhanced`, so each can be read, built or merged on its own. Some build on another branch: `feature/protections` on `feature/block-join`, `feature/player-more-info` on `feature/lua-language`, and `feature/gift-vehicle`, `feature/session-help` and `feature/troll-options` on `feature/player-help`. |

### Features

| Branch | What it adds | Where |
|---|---|---|
| `feature/protections` | **Work in progress.** Network protections, modder detection, the net recorder and the bail monitor ([details below](#protections-and-modder-detection)) | Settings > Protections, Debug > Misc |
| `feature/block-join` | Block Join for a saved player, with the reason their game shows when the join is refused | Network > Saved Players |
| `feature/saved-player-modder` | An Is Modder checkbox to mark or unmark a saved player by hand. It applies right away if they are in the session | Network > Saved Players |
| `feature/player-help` | A Help category for the selected player. One-time actions: health, armor, ammo, weapons, fix vehicle, clear wanted level, off the radar. Toggles: Semi Godmode, Auto Fix Vehicle, Never Wanted, Off The Radar. Give Weapons moves here from Troll. All of it is in the Lua API too | Players > Help |
| `feature/gift-vehicle` | Gift Vehicle: lets the player store the vehicle they are in, or the closest empty one, in their own garage. Street models only: vehicles that have to be bought, and personal vehicles, are refused | Players > Help > Vehicle |
| `feature/session-help` | Semi Godmode, Auto Fix Vehicles, Never Wanted and Off The Radar for everyone in the session except you | Network > Session > Misc |
| `feature/teleport-into-vehicle` | Teleport Into Vehicle: puts you in the selected player's vehicle, in the driver's seat if it is free | Players > Info > Teleport |
| `feature/player-more-info` | More Info also shows language, CEO and MC names, wallet, bank, total money, rank, K/D, kills, deaths, other stats, NAT type and host token | Players > Info > More Info |
| `feature/troll-options` | Ragdoll, Remove Weapons, Set Wanted, and vehicle options: Kick From Vehicle, Kill Engine, Burst Tires, Smash Windows, Stop, Boost, Launch, Flip 180, Open and Close Doors, Lock and Unlock. **Known issue:** Flip 180 snaps back to the way the vehicle was facing | Players > Troll |
| `feature/log-backup` | The previous run's log is moved to `backup\cout_<date>_<time>.log` at startup instead of being overwritten, so the log of a crash survives the restart | |
| `feature/debug-dumps` | Dump Game Module and Dump Native Handlers; Dump Bytecode (the selected script) and Dump All Loaded Scripts. Files go to the `dumps` folder, for offline analysis | Debug > Misc, Debug > Scripts |
| `feature/log-self-reports` | Log Self Reports (off by default): logs each anticheat report our own game sends about us, and any native the menu calls that triggers one | Debug > Misc |
| `feature/lua-kick` | `player:kick(kick_type)` and the `kick_types` table (host, script host, freemode state, null function) | Lua |
| `feature/lua-language` | `player:get_language_id()` / `get_language_name()` and `network.get_player_language_id(pid)` / `get_player_language_name(pid)` | Lua |

The Lua additions are documented in [docs/lua-api.md](docs/lua-api.md).

### Fixes

Several of these bugs are in upstream too.

| Branch | What it fixes |
|---|---|
| `fix/bitbuffer-signed-reads` | Reading signed 64-bit values from network data always failed, and negative values came out wrong (upstream has the same bug). |
| `fix/simple-interiors` | The interior list was missing the three mansions and two garages, and had the salvage yards and bail offices in the wrong order. Players entering those places were flagged as a null function kick, and Send To Interior picked the wrong building (upstream has the same bug). |
| `fix/entity-valid-off-thread` | Checking whether an entity exists from outside a game script made the game send an anticheat report about us to every player in the session. The check now uses a lookup that gives the same answer without it (upstream has the same code). |
| `fix/player-list-tags` | The player list, Players > Info and the ESP read other players' data from the drawing thread, which can crash. They now read it on the game thread. Also: no GOD tag inside interiors (the game makes everyone invincible there), players the game dropped without a leave are removed from the list, and the ESP can show player tags and health and armor bars. |
| `fix/resolve-rid-crash` | Adding a saved player by name could crash in `socialclub.dll` a moment later, because the request was freed while Social Club still used it. |
| `fix/objective-teleport` | Teleport to Objective often went to another player, because player and objective blips look alike. Player blips are now skipped, and objective colors come first. |
| `fix/socialclub-2.4.0.296` | Social Club 2.4.0.296 broke two `socialclub.dll` signatures, so adding or joining a player by name stopped working and the limit of 100 players per Saved Players request fell back to 32. The new signatures match both 2.4.0.296 and the previous version (upstream has the same bug: [#1178](https://github.com/YimMenu/YimMenuV2/issues/1178)). |
| `fix/spectate-player-left` | Spectate stayed on after the spectated player left, so it started again on the next player selected. It now turns itself off. |
| `fix/gui-settings-saved` | Settings > Customize lost most of its values on restart: layout, border and global values such as Global Alpha were saved but never applied, and font scale was never saved. All of them now come back at startup. |
| `fix/delete-vehicle` | Players > Troll > Delete Vehicle did not remove the vehicle on the target's screen, because the network delete sent the wrong ownership token. It now deletes the vehicle for everyone, and warns when the player is not in a vehicle. |

## Protections and modder detection

> **Work in progress.** These are tested in public sessions and still change often. A new check starts as log-only, or blocks without adding points, until it has been seen to hit only modders.

Most of this is ported from [legacy YimMenu](https://github.com/Mr-X-GTA/YimMenu) and checked against the Enhanced game.

### What is checked

- **Entity data from other players** (spawns and updates), before the game applies it: crash models, invalid task and index values, attachment loops, remote teleports, model mismatches, and vehicle data that can crash the game. Known crashes are always blocked. The rest are blocked while Block Bad Sync Data is on, and only logged when it is off. Players can't delete or take over our own ped or a vehicle we control, or delete another player's player object.
- **Cages:** Block Cages (on by default) refuses the cage, container and tube objects that menus spawn around a player to trap them. Only in freemode, since stunt races build their tracks from the same tubes. A player only gets detection points for a burst of different objects, because a single cage can pass to a new owner who did nothing wrong.
- **Game events:** sound and synced scene spam, rope and object control data, traffic manipulation, weapons removed from or given to us, and control requests for the vehicle we are driving. When another player's game reports them to the anticheat, they are marked as a modder.
- **Script events:** legacy's script event protections with the same defaults (bounties, forced teleports and missions, sending to cutscenes, sound spam, vehicle kicks and more), plus the crash and kick events, argument checks and rate limits.
- **Network messages:** kicks, session messages, message floods and forced takeovers of freemode's script host. The takeover is refused, and Reclaim Script Host takes it back, since this is how script host kicks start.

Explosions are only logged for now: normal play produces many owned explosions at high rates, so a reliable rule is still being tested.

### Settings > Protections

- **Script Events** and **Other:** a toggle for each blockable attack, with Enable All, Disable All and Reset To Defaults.
- **Detection:** Modder Detection and Trust Friends (friends are never marked).
- **Reactions:**
  - Save Modders: adds marked modders to Saved Players.
  - Host Kick Attackers: as session host, kicks players who try to crash or kick you or break the session.
  - Block Join Attackers: turns on Block Join for those players. Needs Save Modders.
  - Reclaim Script Host: see above.

  All four are on by default.

### How players get marked

Every block adds points, by how sure it is that only a mod menu could send it:

| Level | Points | Examples |
|---|---|---|
| Certain | 3 | crash, kick and session-breaking attacks |
| Strong | 2 | godmode or invisible attackers, super jump |
| Weak | 1 | bad entity data that isn't a known crash |

A player is marked as a modder at 3 points. The same reason counts at most once every 10 seconds, and points are kept for the whole game session, so rejoining doesn't reset them. The kick and Block Join reactions only respond to Certain detections. The player list tooltip shows each player's detections, and Network > Saved Players lists the saved reasons of a marked player. Lua scripts can add their own with `player:add_detection()` and read them with `player:get_detections()`.

### Diagnostics (Debug > Misc)

- **Net Recorder** (on by default) keeps recent network traffic in memory. It saves it to the `netlogs` folder when an attack is blocked, when the game crashes and when a session ends. Save Net Recorder saves it on demand.
- **The bail monitor** logs what ends a session: the game bailing, warning screens, game scripts stopped by errors, and session events. Log Network Events (off by default) adds more network events to the log.

## Building

The same as upstream: CMake with Visual Studio 2022 (MSVC) or clang. The DLL is `YimMenuV2.dll`.
