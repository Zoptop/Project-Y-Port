# ProjectY – Multiplayer, Commands, Determinism, Save/Load

Source: Ghidra decompilation of `libProjectY.so` (ARM32, armeabi-v7a; Ghidra addresses = ELF vaddr + 0x10000),
plus `assets/config.txt`, `assets/md5.txt`, `dynsym.txt`, `relocs.txt`, and raw disassembly where the decompiler
was ambiguous. Engine field names such as `mCumRuntime` and `mSimStepTime` come from the log strings in the binary.
**[?]** marks an inference or an uncertain point. `E+0xNNN` means a `CEngine` field offset.

---

## 0. TL;DR for the port

* Lockstep runs at a **fixed step of 0.09999847 s** (≈10 Hz, bit pattern `0x3DCCCC00`). Commands are scheduled
  **10 steps (≈1 s) ahead**. Every client sends one command packet and one checksum packet per step.
* The topology is a **star with the host as relay**. `CServer` (RakNet, UDP **60000**) runs in a thread inside the
  host app, and the host also connects to it as an ordinary `CGameClient`. The server is not authoritative: it only
  rebroadcasts command and checksum packets.
* The checksum is a **32-bit wrapping sum of raw words** (raw float bits, ints, bools encoded as 1 or 2).
  On a mismatch the game **pauses for good** and uploads the gzipped log. There is no resync.
* **The simulation is compiled with soft-float** (`__addsf3`, `__mulsf3`, … from libgcc; about 14k call sites),
  apparently at -O0. That gives strict IEEE-754 binary32 with round-to-nearest-even, no FMA, no excess precision and
  no flush-to-zero. `CMath` contains its own fdlibm float routines, plus sin/tan **look-up tables (4096 entries)**.
  Detour's `dtSqrt` was patched to call `CMath::__ieee754_sqrtf`. The developers clearly designed for cross-device
  determinism, and the port has to keep that up.
* There are two RNGs with **identical algorithms** (Marsaglia multiply-with-carry): `CRngSafe` is the synced
  simulation RNG, seeded from the host's seed in GameSetup. `CRng` is the unsynced RNG, seeded from the clock, and
  is used for visuals, for AI (which runs on the host only) and for decisions by the local player that are then sent
  as commands.
* Single-player (`CCommandManagerLocal`) runs **variable-timestep** `CGame::timeUpdate(frameDt)` and executes
  commands immediately.
* The lobby/account server (luminencelabs.com:60500) handled login, chat, friends, stats, highscores, achievements,
  campaign sync, the public server list, ranked results and OOS uploads. **None of it is needed for LAN play.**
  LAN discovery is a RakNet unconnected-ping broadcast to 255.255.255.255:60000.

---

## 1. Simulation tick and timing

### 1.1 Real time → game time (`CEngine::draw` @0x1674dc)
```
nowMs   = CUtils::getTimer()                 // clock_gettime(CLOCK_MONOTONIC): sec*1000 + nsec/1e6, as float ms
dt      = (nowMs - E.lastFrameMs[0x180]) / 1000
dt      = min(dt, 0.2f)                      // 0x3E4CCCCD clamp
dt     *= (activeInterface->mode(+0x34)==1) ? E.GameSpeedFactor(0x1d0) : 1.0f
E.mDeltaT(0x1a8) = dt
if (!E.paused(0x1c2)) E.mCumRuntime(0x198) += dt    // game time, in seconds (float)
E.realRuntime(0x19c) += dt                          // always advances
...  CGameClient::timeUpdate(); CLobbyClient::timeUpdate(); interface->timeUpdate(dt) → commandManager->timeUpdate()
```
* `GameSpeedFactor` (a float in options.txt, default 1.0) is a **local** time scale. The draw code applies it only
  when the active interface mode is 1 **[? probably "in-game"]**. In multiplayer a faster client just reaches its
  next step sooner and then stalls in a forced pause until the others' packets arrive. **Recommendation: force 1.0
  in MP.**
* `CEngine::startTimer` resets `CUtils` timer state. `CEngine::resumeGame` calls it, so pause time is not counted.
  `resumeGame` refuses while `commandManager->isForcedPause()` (vtable +0x58) is true.
* `ProductionsSpeedFactor` is **not** a time scale (see 1.4).

### 1.2 Fixed step in networked mode (`CCommandManagerNetworked::timeUpdate` @0x15d0d4)
* `CCommandManager::CCommandManager` sets `mSimStepTime(+8) = 0x3DCCCC00` = **0.0999984741 s**. This is not exactly
  `0.1f` (`0x3DCCCCCD`): the low mantissa bits are cleared. The port must use the same bit pattern.
* `targetStep(+0xdc) = (uint)(E.mCumRuntime / mSimStepTime)` (`__fixunssfsi`, truncation).
* `while lastSimStep(+0xd4) < targetStep && !paused`:
  1. `step = lastSimStep+1`
  2. For the duration of the step, the engine fields are temporarily overwritten:
     `E.mCumRuntime = float(step) * mSimStepTime` and `E.mDeltaT = mSimStepTime`. Both are restored afterwards.
     As a result the simulation only ever sees exact multiples and a constant dt.
  3. `E+0x310 = step` (current timestep, used as the key for the checksum history)
  4. `sendChecksum(step)` checksums the state **before** this step is updated (see §3.5)
  5. `CGame::timeUpdate(dt=mSimStepTime)`
  6. `doCmds(step)` first sends our own queued commands for **step+10**, then **executes all commands scheduled for
     `step`** (after the `timeUpdate` of that step). If any active player's packet for `step` is missing (only
     checked when `step ≥ 12`), the game pauses (forced) and the step stays "half done": `timeUpdate` has run but the
     commands have not.
  7. On success: `updateChecksumAt()` (debug history), then `lastSimStep = step`. Every 20 steps, for ranked
     network games, `sendRankedPlayerStats(…, -1, …)` is called.
* Before the loop, `CEngine::restoreState()` runs. After the loop, `CEngine::holdState()` runs. Between steps
  (frames without a new step) `CGame::timeUpdateSimple()` extrapolates positions for rendering. `holdState` copies
  the authoritative fields (for example `CUnit` +0x90..0x98 pos, +0xc0 yaw, and weapon state) into shadow copies;
  `restoreState` copies them back before the next real step. **The port should keep render interpolation strictly
  outside the simulation state.**
* Catch-up: a single frame may run several steps (no cap other than the 0.2 s dt clamp, which means at most about
  2 steps per frame).
* Resume after a forced pause: in `timeUpdate`, while paused with `forcedPause(+0xc4)`, `resumeAllowed(s)` is checked
  for `s = lastSimStep .. lastSimStep+4`. When the check passes, `replayDelayed(+0xd8)=1` is set and `resumeGame()` is
  called. The next frame runs `doCmds(lastSimStep+1)` + `updateChecksumAt` **without** running `timeUpdate` again
  ("[replay for delayed commands]"), and that completes the half-done step.
* While waiting, `resumeAllowed` sends `0x97 KeepAliveWaiting(myId, idOfPlayerWeWaitFor)`. `drawInfo` shows
  "Waiting for commands from player %s" with a countdown from 30 s (`0x46EA6000` = 30000 ms), or "Waiting for player
  %s to reconnect" with the server-provided remaining time.

### 1.3 Single-player / split-screen (`CCommandManagerLocal`)
* `CGame::init` picks `CCommandManagerNetworked` only when `E.GameMode(0x1b0)==2`. Modes 0 (single) and 1 (split
  screen) use `CCommandManagerLocal`.
* `CCommandManagerLocal::timeUpdate` simply calls `CGame::timeUpdate(frameDt)` + `timeUpdateExplodedPieces`. The
  timestep is **variable**, there is no lockstep, and every `cmd*` executes **immediately**.
  `isForcedPause()=0`, `drawInfo()` is empty. The port can run SP on the same fixed step, which makes SP replays
  deterministic, although feel and balance may then differ slightly from the original **[?]**.

### 1.4 "Game speed" settings
| Setting | Field | Storage | Effect |
|---|---|---|---|
| `ProductionsSpeedFactor` (int 0..5, default 3) | `E+0x1b8` | options.txt, GameSetup byte, savegame | `CEngine::getSpeedFactor()` = table @0x4ba364 = **{0.1, 0.2, 0.5, 1.0, 1.5, 2.0}**; index ≥6 → 1.0. It **multiplies** production time (`CBuilding` getUnitProductionTime/getOperationTime), research time (`CTechManager::getTechNominalResearchTime`), turret weapon build time, crate timing, scripts and flag posts. Lower is faster. It is part of the synced setup. |
| `GameSpeedFactor` (float, default 1.0) | `E+0x1d0` | options.txt only | Local real-time multiplier (see 1.1). Not synced. |

### 1.5 Latency summary
* Input delay = 10 steps ≈ **1.0 s**. Every client sends one `0x8e` packet per step, even when it is empty, plus one
  `0x8d`. That is roughly 20 packets/s per client, all `RELIABLE_ORDERED` on channel 0 with `HIGH_PRIORITY`.
* Steps 1..11 never block: the first sends target step 11, and the check starts at step 12.

---

## 2. Command set

### 2.1 Structures
```
CCmdForSendQueue (0x18, local queue = std::deque at mgr+0x84)
  +0x00 u32 type   +0x04 u32 source   +0x08 u32 target   +0x0c f32 x   +0x10 f32 y   +0x14 u32 player
CCmd (0x1c, received/executed)
  +0x00 player  +0x04 step  +0x08 type  +0x0c source  +0x10 target  +0x14 f32 x  +0x18 f32 y
CCmdPack (0x14)  { u32 step; vector<vector<CCmd*>> perPlayer (index player-1); u8 received[player+0xf] }
  stored in vector<CCmdPack*> mgr+0xac, indexed by step, grown to step+0x6e when step+12 is reached
CChecksumLog (0x14) { u32 step; u32 chk[1..4] }  vector at mgr+0xb8, indexed by step
```

### 2.2 Command types (`CCommandManagerNetworked::processCmd` @0x1597e8; vtable order in brackets)
"Net check" lists the extra validation done in networked execution. Local execution calls the same `CUnit`/vtable
method directly with fewer checks.

| id | vtable | API (player, source, …) | wire payload after type+source | Executes | Net check (Local differs) |
|---|---|---|---|---|---|
| 0 | +0x00 | cmdUnitOrderEnterUnit(p,unit,tgtUnit) | u16 target | `tgt->vf58(unit)` ok → `CUnit::setUnitOrderEngangeTarget(tgt,false)` (enter a transport or an enterable unit by engaging it **[?]**) | +!isCommander |
| 1 | +0x04 | cmdUnitOrderRemoveCommander(p,obj) | – | `obj->vf88()` (has commander) → `obj->vf8c()` removeCommander | same |
| 2 | +0x08 | cmdUnitOrderMoveTo(p,unit,vec2) | f32 x, f32 y | `setUnitOrderMoveToLocation(pos,true)` | +!isCommander && unit->vf64() (canMove); Local: no checks |
| 3 | +0x10 | cmdUnitOrderAttackUnit(p,unit,tgt) | u16 target | `tgt->vf60(unit)` (attackable) → engage target | +!isCommander |
| 4 | +0x14 | cmdUnitOrderEnterBuilding(p,unit,bld) | u16 target | `bld->vf58(unit)` → `setUnitOrderEnterBuilding` | +!isCommander |
| 5 | +0x1c | cmdBuildingOrderDoSpecOp(p,bld,vec2) | f32 x, f32 y | `bld->vf68()` ready → `bld->vfd0(pos)`: the **special operation / airstrike** at a target position | same |
| 6 | +0x0c | cmdUnitCallCommander(p,obj) | – | if obj is a building (type 2) or a neutral unit: `obj->vf5c(player)` → `obj->vf4c(player,0)` (call or occupy a commander; capture) | same |
| 7 | +0x20 | cmdUnitOrderLookAt(p,unit,vec2) | f32 x, f32 y | `unit->vf70()` → `yaw = CMath::__ieee754_atan2f(y-pos.y, x-pos.x)` → `unit->vfd8(yaw)` | +!isCommander |
| 8 | +0x28 | cmdBuildingOrderRallyTarget(p,bld,obj) | u16 target | `bld->vf6c()` → `bld->vfd4(obj,0,vec)` | same |
| 9 | +0x24 | cmdBuildingOrderRallyDest(p,bld,vec2) | f32 x, f32 y | `bld->vfd4(0,0,&pos)` | same |
| 10 | +0x30 | cmdProcessDialButton(p,obj,button) | u16 target=EDialButtons | `obj->vf84(button)` = `processDialButton`: **produce, research, upgrades, specop selection** (see 2.4) | same |
| 11 | +0x34 | cmdUnitOrderHoldPosition(p,unit) | – | `setUnitOrderHoldPosition` | +!isCommander |
| 12 | +0x38 | cmdProcessActionBarButton(p,obj,action) | u16 target | `obj->vf98(action)` = `processActionBarAction` (EMP, shield, mine drop, …) | same |
| 13 | +0x3c | cmdUnitOrderDefendUnit(p,unit,tgt) | u16 target | `tgt->vfbc(unit)` → `setUnitOrderDefendUnit` | +!isCommander |
| 14 | +0x40 | cmdUnitOrderOnWatch(p,unit) | – | `setUnitOrderOnWatch` (guard/patrol **[?]**) | same |
| 15 | +0x44 | cmdUnitOrderSetMaxVelocityLimiter(p,unit,f) | f32 x(=value), f32 y(=0) | `unit+0xfc8 = value` (formation speed cap) | same |
| 16 | +0x2c | cmdBuildingOrderRallyGroup(p,bld,group) | u16 target=group | `bld->vfd4(0,group,vec)` | same |
| 17 | +0x48 | cmdPlayerSurrenders(p) | – (source written as 0) | Net: if player not lost (+0x8c) or gone (+0x8d) → `CGame::checkGameEndingCondition(true,player)`. Local: `CPlayer::lost(true)` | differs |
| 18 | +0x4c | cmdUnitOrderGatherResourcesAtLocation(p,unit,vec2) | f32 x, f32 y | `setUnitOrderGatherResourcesAtLocation(pos,true,NULL)` | same |
| 19 | +0x18 | cmdUnitOrderGatherResourcesReturnToBuildingAndStop(p,unit,bld) | u16 target | `setUnitOrderGatherResourcesReturnToBuildingAndStop(bld)` | Net: +bld->vf58(unit) && !isCommander; Local: no checks |

Payload masks used by `sendQueuedCmds`, `CServer::recordGameSaveEntry` and the playback code:
`targetMask = 0x93519` (types 0,3,4,8,10,12,13,16,19 carry a u16 target), `vecMask = 0x482A4` (types 2,5,7,9,15,18
carry two f32), `sourceOnlyMask = 0x24842` (1,6,11,14,17). Types ≥ 0x14 are skipped.
Commands that refer to a missing object are dropped silently.
**`processCmd` does not check that the player owns the unit.** Ownership is enforced only by the UI.

### 2.3 Who issues commands
* The UI (`CInterfaceIngamePerPlayer::tryAction*`) calls the command manager through its vtable.
* **AI (`CPlayerKI`, `CKISquad`) also issues orders through the command manager** (`game+0x178` vtable calls). In MP
  the AI is created in "passive" mode on every non-host machine (`CGame::createPlayerFromPlayerSetup`: the last
  ctor arg is `localPlayerId != 1`), so only the host (player 1) thinks for the AI. The AI uses the unsynced `CRng`,
  and its decisions spread through commands.
* Local-owner decisions follow the same pattern. Examples: `CBuildingScienceLab` auto-random-research (dial 0x49),
  `CPlayer::techResearched` and `CUnitRoboBrain` auto-rank are gated on `isLocalPlayer`/`localPlayerId==owner`; they
  call `CRng` and then queue `cmdProcessDialButton`.
* `sendQueuedCmds(player, step)` serializes the **whole** shared deque under the given player id. The host calls it
  first for itself, then once per AI player (`PlayerType==2`). The AI calls therefore send **empty** packets, which
  only exist so the lockstep "all players present" check passes. AI orders travel in the host's packet attributed to
  player 1. This is harmless because execution ignores ownership **[observed; the port can do it more cleanly]**.

### 2.4 EDialButtons (partial, from the `processDialButton` implementations; semantics **[?]**)
* `CBuildingProduction`: 1..12 select a unit type → `+0x36c` = {2,3,5,10,1,6,9,12,8,14,13,16}; 0x15 sets a flag
  (+0x388, repeat?); 0x16 confirms/starts (`+0x350 = selection`, production timer from `getUnitProductionTime`).
  `CBuildingHQ` forwards to Production unless busy (+0x328).
* `CBuildingScienceLab`: 0x28+tech selects a tech; 0x16 starts research; 0x15/0x17 flag on/off; 0x18/0x19 a second
  toggle; 0x49/0x4a auto-random-research on/off.
* `CBuildingSpecop`: 0x12,0x13,0x14,0x1a,0x1b select specop type 0..4; 0x18/0x19 toggle. Execution of the specop is
  command 5.
* `CUnitTurret`: 0x1c..0x1f build weapon {10,1,8,14}. `CUnitRoboBrain`: 0xd..0x10 → `addRank(4,2,5,3)`.

### 2.5 Command wire format (`0x8E`, `sendQueuedCmds` @0x15c6d0 / `receiveCommandPaket` @0x15b520)
All multi-byte values are **big-endian**: RakNet `BitStream` with `IsNetworkOrderInternal()==false` on LE →
`ReverseBytes`. Floats are raw IEEE bits, byte-swapped.
```
u8  0x8E
u32 queuedForTimeStep        // = current step + 10
u8  playerId                 // 1..4
u8  numCmds                  // (char)deque.size() → silently wraps above 255 [bug]
repeat numCmds:
  u8  type
  u16 source                 // object IDs are truncated to 16 bits [overflow risk past 65535 IDs]
  u16 target                 // if type ∈ targetMask
  f32 x, f32 y               // if type ∈ vecMask
```
A `map<step,bool>` per player (mgr+0x10+player*0x18) guards against sending the same step twice ("paket already
sent!!"). On receive, packets from players already lost or disconnected are marked received but their commands are
discarded. **Duplicates are appended rather than replaced** (relevant after a resend; see 3.4). **The port should
deduplicate by (step, player).** Execution order inside a step: player 1..N, then queue order.

---

## 3. Lockstep protocol

### 3.1 Roles and transports
* **Host**: `CInterfaceMenu` (hostNetworkGame) creates a `CServer` and calls
  `CServer::init(1, 60000, serverName, ranked=0, mapId, numPlayers, crateDrop, revision, versionStr, record=0,
  playback=0, useFOW, gameType, startupSector, unitAvail, difficulty, prodSpeed, lobbyHost, lobbyPort, baseDir)`.
  RakPeer `Startup(maxConn=numPlayers, port 60000)`, `SetMaximumIncomingConnections(4)`, a second RakPeer for the
  lobby, then a boost thread running `threadWorkerFunction` (loop: `RakSleep(20)`, `updateGameServer`,
  `updateLobbyServer`, timeouts, ping broadcast, playback). The host then joins its own server through
  `CGameClient::doConnectSequence(ServerIP=127.0.0.1 from config, name, accId, 60000)`.
* **Client**: `CGameClient::init`: `Startup(1 connection, any port, threadPriority -99999)`.
  `connect`: `Connect(host, port, no password, attempts=12, interval=500 ms)`. `doConnectSequence` waits up to 9 s,
  then sends `PlayerName` and `PlayerAccId`.
* The server is a **dumb relay** for `0x8d`/`0x8e`: it rebroadcasts the packet to all peers (broadcast=true; this
  includes the sender, which uses its own copy as the "received" marker). It updates `slot.lastCmdStep(+0xe8)` and
  `slot.lastSeen(+0xf8)`. It never simulates.
* Slots: `CServer` has 5 slots of 0xC0 bytes at +0x40 (slot 0 = "Neutral Player"). Slot fields (offset within the
  slot): +0 name, +4 id, +8 color, +0xC type (3=open, 2=AI, 1=human **[?]**), +0x10 team, +0x14 ready, +0x18 state,
  +0x1C ping, +0x20 accId, +0x24 u16, +0x28 u32 (rank/score **[?]**). Without ranking, slots 2..4 start as AI
  "Player %i" (type 2, ready).
* The maximum is **4 players**: the checksum log has 4 entries and the server has 5 slots.

### 3.2 Packet IDs (game server ↔ client)
RakNet 4.x system IDs in use: 0x01 UNCONNECTED_PING, 0x02 PING_OPEN_CONNECTIONS, 0x10 CONNECTION_REQUEST_ACCEPTED,
0x11 CONNECTION_ATTEMPT_FAILED, 0x13 NEW_INCOMING_CONNECTION, 0x14 NO_FREE_INCOMING_CONNECTIONS,
0x15 DISCONNECTION_NOTIFICATION, 0x16 CONNECTION_LOST, 0x1C UNCONNECTED_PONG, 0x1F/0x20/0x21 remote-system notices.
Custom IDs start at **0x87 = ID_USER_PACKET_ENUM(0x86)+1 [?]**:

| ID | Dir | Name | Payload |
|---|---|---|---|
| 0x87 | S→C (on connect) | GameSetup | u8 mapId, u8 gameType, u8 crateDrop, u8 useFOW, **u32 seed**, u8 startupSector, u8 unitAvailability, u8 difficulty, u8 prodSpeedIdx, u8 numPlayers, u8 revision, u8 ranked, RakString serverName |
| 0x88 | S→all | PlayerSetup | per slot 1..N: RakString name, u8 id, u8 color, u8 type, u8 team, u8 ready, u8 state, u16 ping, u32 accId, u16 ?, u32 ? |
| 0x89 | C→S | PlayerName | string |
| 0x8A | S→C / C→S | ClientPlayerID | u8 id. Server→client assigns the slot. Client→server (on (re)connect while a game exists) reclaims the slot, and the server answers with a 0x92 broadcast |
| 0x8B | C→S | Ready | u8 bool. When all slots are ready the server calls `broadcastStartGame` |
| 0x8C | S→all | StartGame | – (client: `CInterfaceMenu::processAction(0x40)`) |
| 0x8D | C→S→all | Checksum | u32 step, u32 checksum, u8 playerId |
| 0x8E | C→S→all | Commands | see 2.5 |
| 0x8F | both | Chat | string(s) |
| 0x90 / 0x91 | C→S | Team / Color | u8 |
| 0x92 | S→all | RequestResend | – → every client calls `resendCmdsChks()` |
| 0x93 | S→all (each 1000 ms) | PlayerPing | per slot: u8 id, u16 ping, u32 ?, u16 ? |
| 0x94 | C→S | PlayerAccId | u32 |
| 0x95 | C→S | RankedPlayerStats | u8 result (-1 periodic, -2 OOS, …), u16×4 stats |
| 0x96 | S→all | WaitForReconnect | RakString name, u8 id, u32 msRemaining |
| 0x97 | C→S | KeepAliveWaiting | u8 myId, u8 waitingForId |

### 3.3 Join and start sequence
1. Client connects. The server (0x13) assigns the first free slot (state=1 "Connecting…") and sends 0x8A(id), then
   0x87 GameSetup to that GUID. It broadcasts 0x88, updates its offline ping response and pushes the server data to
   the lobby if the game is published.
2. The client stores the setup: `E.MapID(0x1c4)`, `GameType(0x2d0)`, `CrateDropEnabled(600)`, `UseFOW(0x250)`,
   `seed(0x30c)`, `GameStartupSectorSetting(0x1d4)`, `GameUnitAvailabilitySetting(0x1d8)`, `Difficulty(0x1c8)`,
   `ProductionsSpeedFactor(0x1b8)`, `NumPlayers(0x128)`, ranked(0x302) and ServerName(0x304). It then calls
   **`CRngSafe::SetSeed(seed, seed*seed+seed)`** and `CRng::SetSeedFromSystemTime()`.
3. Name, team, color and ready changes go to the server and come back as 0x88. When everyone is ready the server
   calls `broadcastStartGame`: it marks the game started (+0x428, which turns new connections into reconnects),
   optionally opens a recording file, broadcasts 0x8C, and lowers `MaxIncomingConnections` to the number of human
   slots.

### 3.4 Pause, disconnect, reconnect, timeouts (`CServer::threadWorkerFunction`, `CGameClient::receivePakets`)
* Lockstep pause (§1.2) is client-side only. A client stalls whenever a packet for the current step is missing.
* On a client connection loss (0x15/0x16) during a game, the client reconnects automatically
  (`connect(lastHost)`). On accept it sends 0x8A(id); the server re-binds GUID→slot and broadcasts **0x92**, and all
  clients resend their last `numPlayers*10+1` buffered command and checksum packets
  (`CGameClient::sendBitstreamToServer` keeps that ring).
* On the server side, a lost connection during a game sets the slot to "waiting reconnect" (+0xec) and broadcasts
  0x96. The window is **10 s** (`0x40C38800`). After it, or after **30 s without commands** (`0x40DD4C00`, "Command
  timeout and no keep alive"), the server calls `broadcastPlayerLost(slot)`, which **injects fake 0x8E packets for
  that player, containing one `PlayerSurrenders` (type 17) each, for steps lastCmdStep+1 … lastCmdStep+39**. That
  way the remaining clients unblock deterministically and the player is eliminated. The timed-out peer is then
  closed.
* `receiveKeepAliveWaiting` only parses the two bytes in the decompilation, so its effect on timeouts is unclear
  **[?]**.

### 3.5 Checksum (`CGame::calcChecksum` @0x17bc80, `CUtils::updateChecksum*`)
* `CUtils::mChecksum` is a global u32. `resetChecksum()` sets it to 0.
* `updateChecksum(int/uint/long x)`: `chk += x`. `updateChecksum(float f)`: `chk += *(u32*)&f` (raw bits).
  `updateChecksum(bool b)`: `chk += b ? 1 : 2`. `vector<bool>` adds 1 or 2 per element. `CVector2/3/CQuaternion`
  add each component's bits. All arithmetic wraps modulo 2^32. **The sum is order-independent and weak.**
* Order in `CGame::calcChecksum`: NextGameObjectBuildingID(+0x124), NextGameObjectID(+0x108),
  NextProjectileID(+0x140), NextWeaponID(+0x15c); then every game object in list +0x78 → `vf24` (calcChecksum);
  projectiles vector +0x98 → `vf28`; scene objects list +0x3c → `vf24`; players map +0x10 → `vf7c`;
  `CLevel::calcChecksum` (sectors; plus the resource grid when GameType==3).
  * `CGameObject`: position (CVector3 +0x90), yaw-ish float, id +0xf0, owner id (+0x1f8→+0x1c).
  * `CUnit` adds about 25 fields: order state, target ids, velocity, health, flags and so on. `CBuilding` adds
    commander id, queue, production state and the rally vector. `CProjectile`, `CWeapon`, `CPlayer` (its tech
    `vector<bool>` at +0xb8) and `CSector` (owner, value) follow the same pattern.
* `sendChecksum(step)` runs before `CGame::timeUpdate(step)` and sends `0x8D {step, chk, myId}`.
* `receiveChecksumPaket` stores the value in `CChecksumLog[step].chk[pid]` and calls `checkChecksums(step)`, which
  compares every pair of **non-AI** active players (`type != 2`, not lost) whose values are both non-zero. On a
  mismatch it logs "out of sync timestep …" and then:
  `oos(+0xd0)=1`, `CEngine::pauseGame()`, `printChecksumRange(step-2, step)`. That function dumps, for each step,
  the stored `mCumRuntime` and `mDeltaT` (hex) and `CGame::printChecksum()`, a per-object dump that relies on
  `CGame::updateChecksum` and `CLevel::updateChecksum`.
* `CGame::updateChecksum` (called through `updateChecksumAt` after each completed step) **is not a hash**. It
  records a per-step debug history, pruned to the **last 40 steps**: the ID counters, the object id list, a copy of
  the projectile vector, the scene object list, `mRngBeforeTU(+0x1c0)` / `mRngAfterTU(+0x1dc)`, and each object's
  `vf28` history snapshot. Each `CGame::timeUpdate` consumes 2 `CRngSafe::GetUint()` draws (at the start, ids 1 and
  2) only for this debugging.
* After an OOS (next `timeUpdate`): ranked games get `sendRankedPlayerStats(…, -2, …)`, then
  `CEngine::uploadOOSReport()`, which kills the game client, gzips `CUtils::logfilename` to `<log>.zip`
  (`CMemoryFile::compressTo`, gzip "wb") and calls `CLobbyClient::sendOOSReport`. That function sends `0xA8` {u32
  size, u32 chunks, u8 playerId, u8 ranked, f32 cumRuntime, u8 numPlayers, u16 mapId, RakString serverName, u32 seed,
  RakString version} followed by `0xA9` {u32 chunkId, u32 len, bytes} in 1024-byte chunks. **The game stays paused
  with an OOS message; nothing recovers.** A LAN port can simply write the report locally.

### 3.6 Built-in record/playback (useful for testing the port)
`CServer` flags +0x460 (record) and +0x461 (playback). The menu passes 0/0. Recording writes
`<base>record/recordedgame_<seed>_<YYYY_MM_DD_HH_MM_SS>.txt`:
```
<startTimeMs %f> <numPlayers>
<id> <type> <team> <color> <name>        × numPlayers
P <timeMs> <step> <player> <numCmds>      per 0x8E packet
C A <type> <source>                       (source-only)
C B <type> <source> <target>              (target)
C C <type> <source> <hexX> <hexY>         (vec, type_to_hex<float>)
```
Playback reads `record/recordedgame.txt`, checks that the player setup matches, and re-injects the packets at the
recorded relative times. This is the original game's own replay mechanism.

---

## 4. RNG

Both classes implement **Marsaglia's MWC ("multiply-with-carry") generator** with separate static state
(`CRng::mZ@0x4e1918, mW@0x4e191c`; `CRngSafe::mZ@0x4e1920, mW@0x4e1924`; .data initial values
`mW=0x1F123BB5, mZ=0x159A55E5`).
```c
uint32 GetUint() {                          // identical in CRng and CRngSafe (the int arg of CRngSafe is an unused call-site tag)
    mZ = 36969 * (mZ & 0xFFFF) + (mZ >> 16); // 0x9069
    mW = 18000 * (mW & 0xFFFF) + (mW >> 16);
    return (mZ << 16) + (mW & 0xFFFF);       // NOTE: (mW & 0xFFFF), not the usual mW
}
float CRng::GetUniform()     { return (float)(uint32)(GetUint() + 1) * 2.3283064e-10f; }  // 0x2F800000 = 2^-32; (u+1) wraps to 0 for 0xFFFFFFFF; float rounding can give exactly 1.0
float CRngSafe::GetUniform() { return ((float)GetUint() + 1.0f) * 2.3283000e-10f; }       // 0x2F7FFFD2; float(u) is rounded first, then +1
float GetNormal()            { u1 = GetUniform(); u2 = GetUniform();
                               return CMath::__ieee754_sqrtf(-2.0f * CMath::__ieee754_logf(u1)) * CMath::SinTable(u2 * CMath::TWO_PI); }
float GetNormal(mean, sd)    { return GetNormal() * sd + mean; }
float GetExponential()       { return -CMath::__ieee754_logf(GetUniform()); }   GetExponential(m) = m * GetExponential()
SetSeed(a,b): if(a) mW=a; if(b) mZ=b;      CRngSafe::SetSeed(a): mW=a, mZ=0x159A55E5 (CRng::SetSeed(a) only sets mW)
SetSeedFromSystemTime(): t=time(0); SetSeed(t>>16, abs(t))
```
Every float op above is a soft-float call, evaluated strictly in order (verified in the disassembly at 0x377420,
0x377464).

Usage:
* **`CRngSafe`** (synced; seeded only from GameSetup with `SetSeed(seed, seed²+seed)` in
  `CGameClient::receiveGameSetup`, or from the system time when starting SP/skirmish in `CInterfaceMenu`):
  `CGame::timeUpdate` (2 draws per tick), `CLevel` (resource generation, sector setup: tags 0x10–0x12, 0x25),
  `CBuildingHQ` (spawn scatter, `GetNormal`), `CUnitCrate`/`CScriptCratedrop` (crate contents and positions),
  `CUnitTurret`, `CUnitRoboBrain`, `CBomber`.
* **`CRng`** (unsynced, time-seeded in `CEngine::CEngine`, `CServer::CServer`, `receiveGameSetup` and the menus):
  particles (`CParticleSystem*`), `CExplodedUnitPiece`/`CGame::createEUPs`, craters, sound, menus, the host's game
  seed (`CServer::init: seed = CRng::GetUint()`), the AES IV (`CAES::fillrand`), **AI** (`CPlayerKI`, `CKISquad`;
  host only), and local-owner choices that are sent as commands (`CBuildingScienceLab::getRandomTech`,
  `CUnitRoboBrain` auto-rank).
* RNG state is **not saved** in savegames (`CGame::save` has no RNG field), so a reloaded game diverges from the
  original run.
* **Port rule:** any simulation code that is not behind a "local player only → emit command" gate must use the synced
  RNG. Keep the same draw order (including the 2 debug draws per tick) if you want to compare against original logs
  or recordings.

---

## 5. Lobby / account server (`CLobbyClient`, luminencelabs.com:60500)

`config.txt`: `AccountServerIP luminencelabs.com`, `AccountServerChatPort 60500` (config +0x40/+0x44), and
`ServerIP 127.0.0.1` for the local game server. `CLobbyClient` is its own RakPeer (2 connections), connects to the
account server, and logs in automatically on accept.

| Client→Lobby | ID | | Lobby→Client | ID |
|---|---|---|---|---|
| sendLogin (account name, PlayerName, PlayerClan, 10×u8 map-purchased flags) | 0x8E | | chat message | 0x87 |
| sendChatMessage (global or private, to accId or -1) | 0x87 | | login result | 0x8A |
| sendSetFriend / sendSetIgnore | 0x8C / 0x8D | | player list (online) | 0x8B |
| sendLogoff | 0x8F | | player stats | 0x94 |
| sendSetOnlineStatus / PublicProfile / PublicOnlineStatus | 0x90 / 0x91 / 0x92 | | highscores (Top10, `STop10`) | 0x97 |
| requestStats | 0x93 | | friends and ignores | 0x99 |
| sendSetNickClan | 0x95 | | achievements | 0x9C |
| requestHighscores(rankType) | 0x96 | | public servers list / update | 0xA3 / 0xA4 |
| requestFriendsIgnores | 0x98 | | special command result | 0xAB |
| sendSyncAchievements / requestAchievements | 0x9A / 0x9B | | campaign progress | 0xAD |
| sendAddUnrankedStats | 0x9D | | | |
| requestPublicServers | 0xA2 | | | |
| sendOOSReport header / chunk | 0xA8 / 0xA9 | | | |
| sendSpecialCommand (admin text cmd) | 0xAA | | | |
| requestCmpProgress | 0xAC | | | |

The hosting `CServer` also talks to the lobby: `sendLobbyLogin` 0x9E, `sendLobbyLogoff` 0x9F, ack 0xA0 → then
`sendLobbyServerData` 0xA1 (publishes the server in the public list if the `PublishServer` option is on),
`sendRankedStats` 0xA5, `sendRequestPlayerScore` 0xA6 → 0xA7, `sendRequestPlayerScoreAndRank` 0xAE → 0xAF.

What it provided: accounts and login (account name + nick + clan; no password field is visible in sendLogin
**[?]**), global and private chat, an online-player list, friends and ignores (mirrored locally in `friends.txt`),
ranked and unranked stats, highscores/Top10, server-side achievement and campaign-progress sync, internet game
listing (the actual game traffic still runs host↔client on port 60000, so the host needs a reachable port), ranked
match reporting, OOS crash reports, and admin "special commands". Map purchases and downloads go through
`CEngine::purchaseMap/downloadMap` (Java/HTTP side, outside this scope).
**For a LAN-only port, replace all of it with:** local profile (name and clan in options), local stats and
achievements files, LAN browser (§6), direct-IP join, and local OOS logs. Drop ranked play, global chat, friends,
public server list, campaign sync and special commands, or stub them.

`CFriendList` (`friends.txt` in the user dir, plain text): lines `F <accId>`, `I <accId>`, `N <accId> <name>`,
`C <accId> <clan>`.

---

## 6. LAN discovery

* The host's `CServer` keeps a RakNet **offline ping response** (`SetOfflinePingResponse`, refreshed by
  `updateOfflinePingResponse` on every join, leave or start) set to a space-separated ASCII string:
  ` <mapId> <gameType> <crateDrop> <useFOW> <seed> <startupSector> <unitAvail> <difficulty> <prodSpeed>
  <maxPlayers> <connectedClients> <revision> <ranked 1/0> <started 1/0> <port 60000> <serverName>`
  (the leading token comes from a stringstream; the exact first field may be offset **[?]**).
* The browser (`CInterfaceMenu`) calls `CLobbyClient::discoverLANServer()` (`CGameClient::discoverLANServer` exists
  too). It sets `SocketDescriptor.socketFamily = AF_INET` and calls
  `RakPeer::Ping("255.255.255.255", 60000, onlyReplyOnAcceptingConnections=false)`. That sends **ID_UNCONNECTED_PING
  (0x01) by UDP broadcast**. Direct entry uses `pingServer(ip, 60000 or custom port)`.
* The server's RakNet replies with ID_UNCONNECTED_PONG (0x1C) = `[0x1C][u32 sendTime][response string]`.
  `CLobbyClient::receiveServerPong` computes `ping = GetTimeMS() - sendTime` and calls
  `CInterfaceElementServerList::addServer(ip|port, ping, 0, data+5)`.
* Android requests the Wi-Fi multicast lock so it can receive the broadcast pong. On PC no lock is needed; the port
  only needs the UDP broadcast socket option. Compatibility with the original Android clients would require speaking
  RakNet's offline-message format (which includes RakNet's 16-byte OFFLINE_MESSAGE_DATA_ID magic), so the simplest
  route is to keep RakNet (open source, BSD) in the port.

---

## 7. Save game, options, stats, encryption and MD5

### 7.1 Savegame (`CEngine::saveGame` @0x16c448 / `loadGame` @0x168bec)
Plain **text** at `<userDir(E+0x208)>/<name>`. The loader skips 7 header lines:
```
DD.MM.YYYY hh:mm:ss
<map name (level+0xf0)>
<m>m <s>s                     (play time from mCumRuntime)
<owned> of <total>            (sectors held by the human player [?])
<version string (config+0x20)>
<mapId>
<1|0>                         (GameType==1 [?campaign])
create Engine
ShowDebug, MapID, PlayAsPlayer, NumPlayers, {ID, Name, ColorID, PlayerType, TeamID}×(NumPlayers+1),
DebugUnitID, GameMode, ProductionSpeedFactor, CumRuntime %f, DrawWater, AllowRotate, DrawCircles, Paused,
DebugDrawMode, Difficulty, UseFOW, GameType
create Game  (CGame::save: GameStartTime, NextGameObjectID, NextGameObjectBuildingID, NextProjectileID,
  NextWeaponID, AIBuildTimeFactor, AIDamageFactor, AIArmorFactor, AIBrainBaseXP, NumPlayers, CLevel::save,
  NumGameObjects + objects, CGameFeatures, NumProjectiles, NumSceneObjects, CCraterManager ...)
CParticleManager::save, CInterfaceIngame::save
```
* Every value is `Key value\n`. `CUtils::safeReadFromFile(file, key, &v)` reads one line, `sscanf("%s %f")`, and
  **requires the key to match in exact order**, otherwise the load fails.
* Floats are written with **`%f` (6 decimals) → lossy**. Combined with the missing RNG state, a loaded game is not a
  bit-exact continuation. All peers loading the same file would still agree with one another, but there is no code
  path for loading an MP save **[?]**.
* `config: MinLoadableSavegameRevision 71` gates compatibility.

### 7.2 Options and stats
* `options.txt` (plain text, `E+0x208` user dir): ShowDebug, DrawWater, DrawCircles, UseFOW, Difficulty,
  ProductionsSpeedFactor, GameSpeedFactor, CfgTouchMinMovedDistanceID, BackgroundMusicVolume, Language, MapID,
  CrateDropEnabled, GUIScalingOptionsInt, SFXVolume, MasterVolume, EventVerboseness, BeginnerMode,
  GameStartupSectorSetting, GameUnitAvailabilitySetting, PublishServer, LastVersion, PlayerName, UseDoubleTouch,
  UseAutoUpgrade, CurrentUserAccount, MinimapSize, UseLowResTextureMode, PlayerClan, AccountId, ProfileIsPrivate,
  OnlineStatusIsPrivate, ServerName, OfflineModeEnabled, GlobalChatEnabled, ShowCompass. On read, missing keys fall
  back to defaults.
* Stats: `stats_ranked.cache` / `stats_unranked.cache`. The game writes a plaintext `*_dec.cache` first (`Defeats,
  LastGame, PlaytimeSec, Score, UnitsBuilt, UnitsDestroyed, Victories`), then `CAES::encryptFile` writes the
  `.cache` file. Load decrypts `.cache` → `_dec.cache` and parses that.
* Also AES-encrypted the same way: `achievements.cache` (`CAchievementManager`, includes `WSC` winning streak) and
  `store2.cache` (map purchase cache).

### 7.3 CAES (Brian Gladman AES + aesxam-style file mode)
* Key: a hex string in the constructor, patched at runtime (chars [3]='A', [5]='4', [0xd]='C', [0xf]='2', [0x1e]='6')
  and parsed into 32 bytes at `this+0x41`. This is **AES-256**:
  `E2FAB4D9E2F4CCD212D4B18BEFF3B169E2F4A1D9E2F2E1D912D4BF5BEFF7B1D9`.
* `encryptFile/encfile`: a 16-byte random IV from `fillrand` (`CRng::GetUint` bytes) is written first, followed by
  CBC blocks, with ciphertext stealing or a short-file special case for the tail (structure of Gladman's `aesxam.c`
  **[?]**). `decryptFile` is the inverse.
* `encryptChars/encryptCharsDynamic/decryptChars`: **AES-256-CFB** with a 16-byte random IV prefix (out = IV ‖ CFB(in
  incl. NUL)). These are used for network strings **[? callers outside scope]**.
* This is obfuscation only. A port can read the old files with this key, then switch to plaintext or its own format.

### 7.4 CMemoryFile and MD5 validation
* `CMemoryFile::mopen(name, fromAssets)` search order: `name` → `name.zip` (first entry via minizip) →
  `$FILESYSTEMUSER/addon/<name>[.zip]` (downloaded maps) → the APK (`$FILESYSTEM`, `assets/<name>`). The whole file is
  loaded into memory (+NUL), then `updateMD5()` stores the hex MD5 (`CMD5`, a standard RFC 1321 implementation) at
  +0x100c. `compressTo` writes gzip. `minsert` inserts a string.
* `CEngine::readMD5Map` loads `assets/md5.txt` (`<file>  = <md5hex>`, 532 lines covering sounds, maps, textures and
  `mapNLang.txt`) into `map<string,string>` (E+0x278).
* `CEngine::validateMD5forFile(file)` strips the path at the last `\` or `/`, compares the file's MD5 with the
  reference, and caches the result in `EMD5State` (E+0x290: 0 = unknown, 1 = ok, 2 = failed). `resetMD5` clears the
  cache. Callers: `CCampaign` (campaign files) and `validateMapFilesByScript(mapId)`, which parses
  `map<id><Language>.txt` `load` lines (watercubemap ×6 png, mapmesh, mapmixtexture, mapdetailtexture, potential
  `.dat` + `occ.dat`, navbasemesh, assetmesh, assettexture) and validates each one. `isMapDownloaded` uses it.
  This protects against tampered or corrupt map packs, not against cheating in a running game. MP does **not**
  exchange map MD5s; it relies on mapId + revision being equal. **The port should add a map-hash check to the
  handshake.**

---

## 8. Floating-point determinism: what the PC port must respect

1. **Arithmetic model.** The original evaluates every float `+ − × ÷`, compare and conversion through libgcc
   soft-float (`__addsf3, __subsf3, __mulsf3, __divsf3, __aeabi_fcmp*, __floatsisf, __floatunsisf, __fixsfsi,
   __fixunssfsi`). The code looks like -O0 (every intermediate spilled to the stack). The result is correctly
   rounded binary32 with round-to-nearest-even and **denormals kept** (no FTZ). To match it on x86-64:
   * use SSE scalar `float` (never x87; for 32-bit builds `-msse2 -mfpmath=sse`), with `FLT_EVAL_METHOD==0`;
   * `-ffp-contract=off` (no FMA fusion), no `-ffast-math`/`/fp:fast`, use `/fp:strict` or `/fp:precise`, and do not
     enable `-mfma`/AVX2 auto-contraction;
   * keep MXCSR at its default (no FTZ/DAZ). Watch out for libraries that set FTZ, such as some audio middleware;
   * **keep the original operation order and association.** `a*b+c*d` must stay two roundings plus one, no
     reassociation, and do not "optimise" `x/2` into `x*0.5` unless it is bit-identical (for powers of two it is).
2. **Conversions.** `__fixsfsi` truncates. Per the libgcc soft-fp implementation **[verify]**, out-of-range or NaN
   input saturates or gives an implementation result, while x86 `cvttss2si` returns `0x80000000`. `__fixunssfsi`
   returns 0 for negative input, whereas a C cast of a negative float to `unsigned` is UB on x86 and usually wraps.
   The simulation uses `__fixunssfsi` 41 times (for example step computation and AI). **Wrap conversions in helpers
   that reproduce the libgcc semantics.**
3. **Transcendentals.** The simulation never calls the platform libm for its own math:
   * `CMath::__ieee754_sqrtf` is fdlibm's bit-by-bit integer square root, which is **correctly rounded** and
     therefore equals `sqrtss`/`std::sqrt(float)`. Hardware sqrt is safe. `InvSqrt(x) = 1.0f / sqrtf(x)` (exact
     division, not the fast-inverse-sqrt trick).
   * `sin`/`cos`: `CMath::SinTable(x)` with `mTrigTableSize = 4096` (`CEngine` "Math init" → `CMath(0x1000)`) and
     `factor = 4096.0f / TWO_PI`. For x ≥ 0: `idx = ((int)(x*factor)) % 4096`; for x < 0:
     `idx = 4096 - ((int)(-x*factor) % 4096) - 1`. The lookup does no interpolation. Cosine is done by the callers as
     `SinTable(x + HALF_PI)` **[observed pattern]**. The table is filled at startup by
     `mSinTable[i] = __ieee754_sinf(float(i)*TWO_PI/float(4096))` (fdlibm `e_sinf` + `k_sinf` + `e_rem_pio2f`);
     `TanTable` is filled the same way and is unused. **Port fdlibm's float sinf exactly, or embed the 4096-entry
     table**, and reproduce the index quirk for negative x.
   * `__ieee754_atan2f, powf, expf, logf, acosf, asinf (ACos/ASin clamp to ±1), floorf, ceilf, modf, fabsf` are fdlibm
     float ports in `CMath`. **Port these C sources verbatim** (FreeBSD msun `e_*f.c` is the same lineage) and
     compile them under the rules in point 1.
   * `CMath::PI`, `HALF_PI`, `TWO_PI`, `fDeg2Rad` and `fRad2Deg` are in .bss, i.e. computed by a static initializer
     **[? exact bits; dump or recompute them as float constants, e.g. PI = 3.14159265f]**.
   * Remaining libm imports and where they are used: `atan2f` in Detour's
     `dtObstacleAvoidanceQuery::sampleVelocityAdaptive` (crowd avoidance; **check whether the game uses dtCrowd in the
     simulation; if so, replace it with `CMath::atan2f`**), `ceil` (double) in `GridSectorizer::Init` (init time),
     `floorf` in `CLevel::updateREStexture` (render only), and `pow` in libpng. The ARM-mode xref scan found no
     callers of `sinf/cosf/sqrtf/logf/acosf/powf`.
   * Recast/Detour is embedded (`dtTileCache::update` runs inside `CGame::timeUpdate`) and `dtSqrt` was patched to the
     CMath sqrt. Other Detour float code follows the same soft-float rules.
4. **Doubles.** Double ops (`__adddf3`, …) only appear in UI, server timers, achievements, `CUtils::getTimer` and
   logging (`__extendsfdf2` for printf). **Simulation state is float-only. Keep it that way.**
5. **Iteration order.** Simulation loops walk `std::list` (game objects in creation order), `std::vector`
   (projectiles) and `std::map` keyed by integer IDs (players, sectors). **Never use pointer-keyed or unordered
   containers in the simulation**, and keep ID allocation identical (Next*ID counters, which are part of the
   checksum).
6. **Time inputs.** The simulation reads `E.mCumRuntime` (= step×stepTime as a float product) and `E.mDeltaT`
   (= stepTime). Never feed wall clock time, frame dt or `GameSpeedFactor` into networked simulation code.
   `CGame::timeUpdate` also writes `getTimer()` values into a profiling struct (`E+0xac`); that data is profiling
   only, so make sure it stays outside the simulation.
7. **Local-player branches.** Code paths such as `isLocalPlayer`, `localPlayerId==owner` and `UseAutoUpgrade` may
   only **queue commands**. They must not mutate simulation state directly. The original follows that rule
   (ScienceLab, RoboBrain), and the port must keep it.
8. **Limits to preserve or fix consciously.** Object IDs are 16-bit on the wire, there are at most 255 commands per
   packet, and step and ID types are u32. The checksum is a weak additive sum; a stronger hash (for example FNV/xxHash
   over the same fields in the same order) is fine for PC↔PC play but breaks compatibility with original clients.
9. **Cross-play with the Android original** would additionally need RakNet wire compatibility and the same seed
   handling. Given points 1–3, that is not ruled out, but it is **[unverified]**. PC↔PC sync only needs points 1–8
   applied consistently across all PC builds (same compiler flags on every platform; test MSVC vs GCC/Clang
   results).

---

### Key function index
`CCommandManager::CCommandManager` (stepTime) · `CCommandManagerNetworked::{timeUpdate, doCmds, sendQueuedCmds,
receiveCommandPaket, processCmd, resumeAllowed, sendChecksum, receiveChecksumPaket, checkChecksums,
printChecksumRange, updateChecksumAt, receiveWaitForOtherReconPaket}` · `CCommandManagerLocal::*` ·
`CGame::{timeUpdate, timeUpdateSimple, holdState, restoreState, calcChecksum, updateChecksum, printChecksum, save,
load, createPlayerFromPlayerSetup}` · `CServer::{init, updateGameServer, threadWorkerFunction, sendGameSetup,
broadcastPlayerSetup, broadcastStartGame, broadcastPlayerLost, broadcastRequestResend,
broadcastPlayerReconnectWaiting, broadcastPlayerPing, updateOfflinePingResponse, recordGameSaveEntry,
receiveDisconnectClient}` · `CGameClient::{init, connect, doConnectSequence, receivePakets, receiveGameSetup,
sendBitstreamToServer, resendCmdsChks, discoverLANServer}` · `CLobbyClient::{timeUpdate, sendLogin,
discoverLANServer, pingServer, receiveServerPong, sendOOSReport}` · `CEngine::{draw, getSpeedFactor, startTimer,
pauseGame, resumeGame, saveGame, loadGame, saveOptions, loadOptions, saveStats, loadStats, uploadOOSReport,
readMD5Map, validateMD5forFile, validateMapFilesByScript}` · `CRng/CRngSafe::*` · `CUtils::{updateChecksum*,
getTimer, safeReadFromFile}` · `CMath::{SinTable, buildTrigTables, __ieee754_*}` · `CAES::*` · `CMemoryFile::*`.
