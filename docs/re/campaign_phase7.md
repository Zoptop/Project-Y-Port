# Phase 7: the campaign

The campaign "ReRun" (cmp1, 3 chapters x 6 missions, maps 301..318) and the tutorial (map 201), from the
menus to the end credits. Conventions as in `ui_phase5.md` and `sim_phase4.md`.

## Code

| piece | where | original |
|---|---|---|
| `CCampaign` (definition, progress, texts, avatars) | `src/game/CCampaign.{h,cpp}` | 0x151430..0x1536e8 |
| campaign map, `initCampaign`, `getCurCampaign` | `CEngine` (+0x2e0, +0x2f8) | `CEngine::init`, 0x16bec4 |
| campaign screen 25, `createElementCampaignSlider` | `CInterfaceMenuScreens.cpp` | setupElements case 0x19, 0x1d1468 |
| campaign progress screen 26 (episode list) | `CInterfaceMenuScreens.cpp`, `CInterfaceElementCampaignButtons` | case 0x1a, 0x18bf18.. |
| campaign picker | `CInterfaceElementCampaignImageSlider` | 0x18d814.. |
| actions 0x4d, 0x4f..0x54, 0x1f/0x42/0x71 campaign init | `CInterfaceMenuActions.cpp` (`startCurrentCampaignEpisode` = the shared tail 0x1cd818) | 0x1cd3f8 |
| loading screen (draw mode 1, game type 1), credits roll (draw mode 3) | `CInterfaceMenu.cpp` | 0x1d3b10 |
| the "mission complete" hook | aftermath screen 12 in `setupElements` | 0x1ba548 |
| speaker avatars | `IPP::setOverlayText`, `drawOverlayText` | 0x1a7ea8, 0x1a2484 |
| scripts' HUD effects | `IScriptUI` (`CScriptManager.h`), `CInterfaceIngame` | the scripts' `run` |

## Flow

Single player -> **Campaign** (0x4d: `CCampaign::startInitAsync`, then 0x5a) -> screen 25 (sets game type 1):
the slider shows the campaign ("18 Missions - p% done", the description, the current chapter and episode),
**Start Campaign** / **Continue** (0x50) and **Replay Episode** (0x4e, enabled once something is done) ->
screen 26: the unlocked episodes per chapter (a tap selects: 0x51), **Start Replay** (0x52), Back (0x4d).
0x50/0x52 (and the aftermath's 0x53) end in the same code: slot 0 neutral, slot 1 human, slots 2..4 AI
(names from config, id = team = colour = slot), map and player count of the current episode, production speed
2, game speed 1, FOW on, crates off, no presets; draw mode 1 shows the episode over `cmp1_cut_<n>.jpg`, draw
mode 2 calls `initSingleplayerGame`. There is no separate campaign start function.

Mission end: `CScriptGamestate::run` -> aftermath (12). **The "mission complete" hook** (the [?] of
scripts_rendering_ui.md A.9) is in setupElements' aftermath case: a victory with game type 1 calls
`CCampaign::markMapCompleted(engine mapId)`; it makes the next episode current, raises the highest unlocked
one and writes `cmp1prog.txt`. Buttons: **Continue** (0x53: `resetGame`, start the now current episode) and
Main menu; after the last episode only **Continue** (0x54: credits; Back does the same). A defeat (or a
draw) offers **Replay** (0x53 with CTxtLoc 0x1ae: the same episode again) and Main menu.

Credits (draw mode 3): `cmp1_cut_<finalcut>.jpg`, `cmp1_final_<Lang>.txt` scrolling up 25 px/s (32 px x GUI
scale per line unit, `# S <scale>`, `-` = empty line), then CTxtLoc 0x1c6/0x1c7. A tap returns to the main
menu (0x17).

Tutorial: screen 9 -> action 0xe (map 201, two players, game type left at 0 by action 1). It uses the same
script machinery; its texts have no avatars. Only 201 exists in the data (config: CurrentMapRevision201).

## Saved files

| file | content | written by |
|---|---|---|
| `<save dir>/cmp1prog.txt` | `chp N` / `eps N` / `cmp 0|1`: the highest unlocked episode and "completed" | `markMapCompleted` |
| `<save dir>/options.txt` | the options (phase 5d) | the option actions |
| `stats_ranked/unranked.cache` (AES) | lobby statistics | **not ported**: only the lobby writes them (`CLobbyClient`), never a single player or campaign game |

[PC] `startInitAsync` runs at once (no worker thread, no MD5 sums); the lobby's progress sync is not ported.

## Quirks kept ([!])

- `markMapCompleted` skipping into a later chapter (only possible with `--mission`) unlocks that chapter's
  first episode only. Replaying an old episode makes the one after it current, the highest stays.
- `parseDef` keeps the `'\r'` of CRLF lines in names; an `id` mismatch is logged, then overwritten as "loaded".
- The text listing splits on `'\n'` only: an empty CRLF line is the text `"\r"`.
- The slider line is `"%.0f %s - %.0f%%%% %s"` (a format string again when drawn).
- `timetrigger player(0) show` gives the timer to players 0..n-1: on a one-player mission nobody sees it.
- Two speakers started in the same script tick share the one overlay: the first is overwritten and never
  completes, so everything waiting on it stalls (relevant when two triggers fire together).
- An `iconname` without a `char` line makes the original throw (`std::map::at`): map 314's first speaker
  (`iconname("a1")`) ends the original game. [PC] the text is shown without an avatar.
- The game-end text (`CGame::checkGameEndingCondition`) has *nopause* set and no icon (0x17de1c); the HUD
  package had the two flags swapped (it paused the game).
- Map data: 307's `gamestate id(25) reqid(2) result(1)` makes losing your ThinkerBrain a **victory**;
  309's `marker id(9)` points at object 3, which does not exist; 313 uses `unittrigger type(1)` (unit flag
  +0x1294, "under attack") for "ThinkerBrain destroyed", so the player's brain being hit ends the mission.

## Tools and checks

```
build/campaign_smoke --data data [--steps 3000] [--maps 301,305|all] [--force-triggers] [--verbose]
python3 tools/verify/campaign_check.py --data data
SDL_VIDEODRIVER=offscreen ./build-game/ProjectY --data data --mission 305 ...        # [PC] start a mission directly
SDL_VIDEODRIVER=offscreen ./build-game/ProjectY --data data --save-dir /tmp/py_cmp --frames 1510 --input tools/ui/campaign_flow.input
SDL_VIDEODRIVER=offscreen ./build-game/ProjectY --data data --save-dir /tmp/py_cmp --mission 318 --frames 3510 --input tools/ui/campaign_credits.input
python3 tools/verify/compare_sim.py --mission 301 --steps 320 --oracle-out <cache>   # vs the original
```

- `campaign_smoke` starts each mission through the menu code (0x52 / 0xe + `initSingleplayerGame`), headless,
  and clicks every speaker text away once typed out. `--force-triggers` marks done every trigger from which a
  victory follows without a defeat in between, one at a time when no speaker is pending, and never lets a
  defeat trigger fire: a mission that then reaches "Victory" is completable in principle.
- `campaign_check.py` re-reads the map scripts independently: missing reqid/xorid targets, owners or avatars
  (crashes of the original), scripts that can never complete (objects, sectors or players that do not exist),
  and whether a victory gamestate's reqid ancestors can all complete.
- Input scripts: `mapscript <script line>` [PC] adds a script line to the running game (a test aid, e.g. the
  mission's own `gamestate result(1) player(1)`).
- `game_oracle.py --mission <map>` / `sim_probe --mission <map>` start a campaign mission like the menus on both
  sides (campaign read, episode current, game type 1, `processAction(0x52)`, `initSingleplayerGame`).

Not ported: `CInterfaceElementMultiTextScrollable` (used by the lobby chat screens 19/20 only), campaign
savegames (`isSavegameFromCampaign`), downloads, achievements (`addach` logs).
