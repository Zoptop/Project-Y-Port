# Phase 8: sound and music

The original plays sound through Cricket Audio (`Cki::`, linked statically; closed source) and
wraps it in two classes of its own, `CSoundManager` and `CSoundMoodAnalyzer`. The port keeps those
two classes as they are, with their names, offsets and Ghidra addresses (`src/game/CSoundManager.*`,
`src/game/CSoundMoodAnalyzer.*`). In place of Cricket it has a small software mixer that does what
the game needs from Cricket (`src/audio/Ck.*`), and an SDL2 audio device that plays the mix
(`src/platform/SDLAudio.*`).

## Pieces

| File | What |
|---|---|
| `src/game/CSoundManager.*` | The original manager (`0x39b1e4 playSound` & co.): ids, the bank/stream sounds, the music logic, the volumes, `playRandomSound` |
| `src/game/CSoundMoodAnalyzer.*` | The music mood (`0x399d38 update`) |
| `src/audio/Ck.*` | [PC] The Cricket stand-in: `CkMixer`, `CkBank` (the `.ckb` bank, MS-ADPCM), `CkSound` (bank and Ogg stream sounds, loops, next sound, 3D), `CkInit/CkUpdate/CkSuspend/CkResume/CkShutdown`, `ck::render` |
| `src/audio/CkVorbis.*` | [PC] Ogg Vorbis decoding with `third_party/stb/stb_vorbis.c` (v1.22, public domain) |
| `src/platform/SDLAudio.*` | [PC] The device: 44.1 kHz stereo float, its callback calls `ck::render` |
| `src/ui/CUISound.*` | The interface's `playSound(name, 0, no loop, 1.0, play)` / `playRandomSound(kind)` calls |

The bank `game.ckb` is read and decoded at start (49 sounds, about 1.5 MB of PCM). The decoder
reproduces the reference WAVs of the asset package bit for bit (the RE package's `tools/ckdec.py`
with low nibble first and the two header samples swapped). The 60 music tracks
`<ta|tn|td|aa|an|ad>_<1..10>.ogg` are opened for their length at start and decoded while they play.

## What the game does with sound (the decompile)

- **Ids:** `playSound` returns `mNextSoundSourceID++` (from 1; 0 = no sound). An unknown name
  uses up an id and returns 0. Bank sounds go to the mixer "SFX", streams to "Music".
- **Volumes (0..100, options.txt):** master = v/100, SFX = v/100, music mixer = v/100 x 3.
  [!] Each music track also keeps the music volume of the moment it was created as its own volume
  (the constructor, default 40 -> 0.4), so the default music gain is 0.4 x 1.2.
  [!] SFX 0 and music 0 together (`mAllVolumeZero`, +0x85) switch the manager off: `getSound`
  returns null, so nothing plays or moves and the music logic stops; turning one back on restarts
  the music. With all volumes 0, `stopBackgroundMusic` cannot reach the track (getSound is null),
  so it keeps playing silently and only its id is forgotten.
- **3D:** the listener is the camera (`CCameraLevel::refresh`, 0x14f9a4: eye, looking at the
  target on the ground, up = d x (d x -Z)); attenuation `set3dAttenuation(3, 200, 600, 0)`
  (inverse distance squared, full volume within 200, silent from 600); sound speed 343.2.
- **Music:** `startBackgroundMusic` plays a random track of the current mood (CEngine::init after
  the menu, CGame::init, "leave game"). `updateBackgroundMusic` (every frame) queues the next one
  5 s before the end of the current track: another track is chained with `CkSound::setNextSound`,
  the same track gets one more loop. When the track has ended (or wrapped into the extra loop) the
  queued one becomes current. [!] If the frame loop stalls longer than the 5 s, the chained track
  can end before the switch, and the music stops until the next `startBackgroundMusic` (the
  original's logic; seen under llvmpipe, where single frames of a skirmish can take 30-50 s).
  [!] `stopBackgroundMusic` forgets the current track but not a queued one: a game started (or
  left) in the last 5 s of a track inherits the queue flag, so its first track queues nothing and
  the music stops after it (until the next `startBackgroundMusic`).
- **Mood (`CSoundMoodAnalyzer`):** once a second of frame time: the counters decay by
  `1 - 0.2 t`, the tranquility grows by `t`, the influence of the local player's and the enemies'
  units is summed (`CUnit::getInfluenceAndFalloff`). Tranquil if tranquility > the sum of the
  counters, else action; aggressive if (dealt + destroyed + captured + flag capturing + own
  influence share) > 0.6 of all, defensive if the received/lost side is, else neutral. Fed by
  `CUnit::hit` (damage x 0.01), `CUnit::die` (5 per unit), `CSector::ownerChanged` (10 per sector)
  and `CBuildingFlagpost::timeUpdate` (flag life lost x 0.1; [!] for every losing flag, the
  owner check's result is ignored). `sectorFlagCapturing` has no caller.
- **Random voices:** `playRandomSound(0)` (orders) picks roger / affirmative / ok / yes sir / on
  my way, `playRandomSound(1)` (attack) engage / attacking / approaching target.

## Randomness

`getRandomSoundForMood` and `playRandomSound` draw from **CRng**, the unsynchronised RNG that also
drives the AI (one draw each). The port makes exactly these draws, at the same moments. They never
touch CRngSafe, so lockstep is not affected. The draws do not depend on the audio device: the whole
manager runs without one (`--no-sound`, no driver, a failed device), only the mixing is skipped and
`CkUpdate` advances the playing sounds by the frame clock instead (like Cricket's dummy output). So a
game with `--no-sound` draws the same as one with sound. With all volumes 0, `playRandomSound` and the
music queue do not draw (as in the original), but `startBackgroundMusic` does.

The tests and tools that create no sound manager (`sim_probe`, the unit tests of other phases)
make none of these draws, matching `tools/emu/game_oracle.py`, which stubs all of `CSoundManager`.

## Cricket, from its disassembly

Cricket was compiled as Thumb-2 with VFP hardware floats; the formulas below are from the ELF
addresses given (Ghidra = ELF + 0x10000). The port evaluates them in the same order.

- `Cki::Sound::AttenParams::set` (0x3eac94): near >= 1e-12, far >= near + 1e-12, farVol in [0, 1];
  the inverse modes store `scale = (farVol - 1) / (1/far' - 1/near')`, `offset = 1 - scale/near'`
  (near', far' squared for mode 3), so the volume is `scale/d^2 + offset`.
- `Cki::Sound::update3d` (0x3eb080): volume 1 inside near, farVol from far on, the mode's formula
  between, clamped to [farVol, 1]. Pan: the direction to the sound projected onto the plane normal
  to the up vector, dotted with the listener's right vector (forward x up); 0 within near/10, faded
  linearly to 0 between near and near/10. Doppler: `(c + max(0.99c, v_listener)) / (c + max(0.99c,
  v_source))` - [!] Cricket's max should be a min, so the factor is 1 unless something moves at
  0.99 c. A volume below 0.005 makes the sound virtual (not mixed, time goes on).
- `Cki::Sound::set3dListenerPosition` (0x3eb93c): the up vector is normalised (kept if ~0).
- `Cki::Sound::getFinalVolumeMatrix` (0x3ebff4) with `Cki::VolumeMatrix::setPan` /
  `setStereoPan` (0x3ecc58 / 0x3ecd40) and `Cki::AudioUtil::monoPan_default` /
  `stereoPan_default` (0x3f4d8c / 0x3f4740): gain = volume x mixer x 3D volume; mono sources pan
  with constant power, `L = cos a, R = sin a, a = (pan + 1) pi/4`; stereo sources fold the far
  channel in and the whole matrix is halved (`0.5 [cos a, 0; sin a, 1]` for pan >= 0).
- `Cki::Mixer::update` (0x3e991c): mixed volume = volume x the parent's (not clamped).
- `Cki::Sound::play` (0x3eafc8): stops and restarts a playing sound.
- Volume changes ramp over 40 ms (`Ck_volumeRampTimeMsDefault`); the output is clipped.

Cricket has no limit on simultaneous sounds and no priorities; neither does the game. Sounds the
listener cannot hear (beyond 600) are virtual and cost nothing.

## The call sites

| Where (port) | Original | Sound |
|---|---|---|
| `CEngine::initSound` / `initInterfaces` / `draw` / `loadOptions` | CEngine::init 0x16d074, draw 0x1674dc, loadOptions 0x16b600 | manager, menu music, `update` every frame, the three volumes |
| `CGame::init` / `~CGame` | 0x17cf28 / 0x17cabc | mood reset, music start; `clearSFX(false)` |
| `CUnit::die` | 0x3aa0d4 | "explosion" (3D); mood unit lost / destroyed |
| `CUnit::hit` | 0x3ab5a4 | mood damage received / dealt |
| `CSector::ownerChanged` | 0x391a30 | mood sector captured / lost |
| `CBuildingFlagpost::timeUpdate` | 0x139e58 | mood flag losing |
| `CBomber::init` / `timeUpdate` | 0x128fa4 / 0x126a2c | "staticself" engine loop (3D, 0.7; moved every step; 0.3 while braking, 0.7 after the drop; stopped above 700); "explosion" after the bombs |
| `CProjectileLaserBlastA/B`, `CProjectileSniperBullet::init` | 0x37a93c, 0x37b8d0, 0x37f5dc | "shotlasera" / "shotlaserb" / "shotlasera" (3D, 0.5) |
| `CProjectileRocket::init` / `update` | 0x37e160 / 0x37d1fc | "rocketlaunch", "explosionrocket" (3D) |
| `CUnitSpotter::timeUpdate` | 0x3c9354 | "explosion" when the UAV crashes |
| `CWeaponLaserGatling/HeavyLaserGatling/MiniLaser::timeUpdate`, `deactivate`, destructors | 0x3dac9c, 0x3da38c, 0x3dfbc4, ... | the config's `SoundSourceName` ("shooting1" / "shooting2") looping at the beam start while firing (`CWeapon::updateBeamSound` / `stopBeamSound`) |
| `CEventBar::addEvent` | 0x17343c | the 18 event voices (production, drops ready, sector under attack / lost / taken), as the EventVerboseness option lets the event through |
| `CInterfaceIngamePerPlayer` | 0x1a7364, 0x1a88dc.. 0x1a9888 | "click"; acknowledgements (`playRandomSound`); "txtenteringunit/building"; the drop voices; "action_rallypointset" |
| `CInterfaceIngame::showGameEndText` / `showSpeakerText` | CGame::checkGameEndingCondition 0x17dbc4, CScriptSpeaker::run 0x390b8c | "ringringring" |
| `CInterfaceMenu::processEventActionUp` / `processAction` | 0x1d5a88 / 0x1cd3f8 | "click" / "clickfail"; music restart on "leave game"; music and SFX volume sliders |
| `CCameraLevel::refresh` | 0x14f9a4 | the listener |

Campaign (phase 7): the missions' speaker texts play "ringringring" (`showSpeakerText`); the
original's CCampaign, the loading screen and the credits roll make no sound calls (the music
keeps playing; a mission's CGame::init restarts it like a skirmish).

Not ported (no caller in the port yet): `CGameClient::receivePlayerSetup` ("infoplayerconnected",
"infoplayerdisconnected": multiplayer), `CBomber::timeUpdateSimple` / `load` (network
extrapolation, savegames). `CSoundManager::drawDebug` is ported but, as in the original, nothing
calls it. The bank's "clickverb", "explobrain" and "levelupavailable" are not used by the original.

[PC] differences: the weapons' destructors stop their beam sound only (the original's
`deactivate` there also disables the beam particle system, which the port leaves to the particle
manager as before); failed sounds are freed instead of leaked; minimising the window suspends the
mixer (the original's AppOnPause called CkSuspend), restoring resumes it. Pausing the game does not
pause sounds (as in the original).

## Checks

- `tests/test_sound.cpp`: the 3D gains bit for bit against a float32 Python reimplementation of the
  Cricket formulas, the pan matrices, the ADPCM block decoder (against `ckdec.py`) and the bank
  (frame counts and CRCs of four sounds), time without output, loops and next sounds, the mixer's
  gains, the mood transitions (hand-computed), the manager's ids, gating, CRng draws and music
  switching, and that every sound name the code uses is in the bank (`tools/check_sounds.py` lists
  them).
- `tools/verify/compare_sim.py --probe-sound`: the port's simulation with a sound manager (created
  after CGame::init) still matches the original step by step, including the RNG states.
- The mix: `SDL_AUDIODRIVER=disk SDL_DISKAUDIOFILE=mix.raw ... --log-sounds --input
  tools/ui/ingame_sound.input`, then `tools/verify/check_mix.py mix.raw projecty.log`: every logged
  effect must stand out of what was playing before it. `--log-sounds` logs each sound with the
  mixer's output position, which is the position in mix.raw.

[PC] options: `--no-sound` (no device; the logic still runs), `--log-sounds`. `SDL_AUDIODRIVER`
chooses the driver (`dummy` for a silent device, `disk` to record).
