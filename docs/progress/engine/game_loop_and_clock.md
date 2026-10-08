# Game loop and clock

The game's heartbeat: the world moves on in fixed game turns, ten a second, while the screen is drawn as often as it
can be. A game clock that runs at the game's speed and stops while paused decides when a turn is due; between turns the
frame works out how far through the turn it is. Two random number streams keep the game's rules repeatable.

**Progress: 21/39 done, 9 partial — 65%**

## Game turns

| Behaviour | Status | Where / notes |
|-----------|--------|---------------|
| The world moves on in game turns of a tenth of a second | done | `src/ECS/Systems/Implementations/TimeSystem.cpp`; `GameClock.TurnsFollowTheTimer` |
| A turn is due once the game clock reaches the turn number times the turn's length, so no turn loses what was left of the one before | done | `TimeSystem`; `GameClock.NoTurnLosesItsLeftover` |
| At most one turn is played in a frame in a single-player game | done | `TimeSystem`; `GameClock.OneTurnAFrame`. Network games play several to keep up: see ../multiplayer/ |
| When the game falls more than two seconds behind it gives up on the lost time rather than racing to catch up | done | `TimeSystem`; `GameClock.FarBehindGivesUpTheLostTime` |
| The turn number starts again from 0 as each land starts | done | `Game::StartNewLand` starts the game clock |
| Each turn processes the world's parts in the game's fixed order (players, creatures, villagers, scripts, weather, magic …) | partial | `Game::GameLogicLoop` turn body; port notes list "turn body in the game's order" as todo until the world systems exist |
| The per-frame (drawing-side) updates run in the game's order | todo | port notes: frame updaters in the game's order not started |
| Physics is stepped by the game's turns, not by the wall clock | done | `DynamicsSystem::ProcessTurn` runs the game's own physics once a game turn (20 steps of 5 ms); Bullet is gone |
| The temple has turns of its own while the world outside is stopped | partial | `Game::ProcessTempleAudioTurn` keeps the temple's audio going; the rest of the temple's turn: see ../temple/ |
| A game starts with a short pause before its loop: the logo plays once, then the clock is reset to the current turn | todo | openblack starts its loop straight away; logo and intro films: see ../story/ |

## Frame clock

| Behaviour | Status | Where / notes |
|-----------|--------|---------------|
| Each frame has a game-time step: how far the game clock moved since the last frame, none while paused | done | `TimeSystem::UpdateFrame`; `GameClock.TheFrameClockFollowsTheTurn` |
| Each frame knows how far it is through the current turn (0 to 0.99), so moving things can be drawn between their turn positions | partial | `GetTurnFraction`; used by fire, magic, particles and shields only; most objects are drawn where their last turn left them (unconfirmed how much the game smooths) |
| The frame's real time is whole milliseconds of the wall clock and never zero | done | `GameClock.TheFrameRealTimeIsWholeMillisecondsOfTheWallClockAndNeverNone` |
| The hand and the camera move by real time, except during a script's cut scene, when they keep to game time | done | `CameraStep` in `TimeSystemInterface.h`; `GameClock.TheHandAndCameraStepByRealTimeExceptInAScriptsCutScene` |
| While the window is minimised the game keeps running but draws nothing | todo | only the debug GUI skips drawing when minimised (`src/Debug/Gui.cpp`) |
| Quitting is noticed between frames and ends the loop cleanly once help is not mid-sequence | partial | `Game::Run` ends on quit; the help-sequence guard is not there |

## Pausing

| Behaviour | Status | Where / notes |
|-----------|--------|---------------|
| Pausing stops the game clock; unpausing starts it again from where it stopped | done | `TimeSystem::SetPaused`; `GameClock.PausingStopsTheClock` |
| No turns are played while paused | done | `TimeSystem::IsTurnDue` |
| Opening the game's menu pauses the game; leaving it restores the pause it had before | done | `Game` (`_pausedBeforeMenu`) |
| A land starts unpaused | done | port notes (starts unpaused) |
| Entering the temple stops the world outside | partial | world turns stop in the temple; see ../temple/ for the temple's side |

## Game speed

| Behaviour | Status | Where / notes |
|-----------|--------|---------------|
| The game speed scales how fast the game clock runs; time already gone keeps the speed it went at | done | `TimeSystem::SetSpeed`; `GameClock.SpeedScalesTheTimer` |
| Changing the speed while paused takes effect when the game is unpaused | done | `GameTimer::SetSpeed` keeps the saved speed |
| Challenge scripts can speed the game up for a section and set it back | todo | the two speed natives are stubs in `src/CHLApi.cpp` |
| A land's map script can set the length of a game turn | todo | the map command throws in `src/LHScriptX/MapScriptCommands.cpp` (and map commands are not run at all) |

## Calendar

| Behaviour | Status | Where / notes |
|-----------|--------|---------------|
| The game keeps a calendar date: a game year of 36 000 turns from a fixed start date, with months, days and seasons | partial | constants inside `WeatherSystem.cpp`, used for the weather only; no shared calendar service |
| A land's map script can set the start date, start time and turns per year | todo | map commands throw in `MapScriptCommands.cpp` |
| The visual time of day follows its own clock that scripts can set and scale | partial | see ../sky/ for the day and night cycle |

## Random numbers

| Behaviour | Status | Where / notes |
|-----------|--------|---------------|
| Game rules draw from one synchronised random stream, so the same game plays the same everywhere | done | `src/Common/GameRandom*`; `GameRandom.DrawsAsTheGameDoes` |
| Effects that don't change the game draw from a separate local stream | done | `GameRandomInterface::LocalRand` |
| A random fraction scales a 16-bit draw | done | `GameRandom.FloatsScaleASixteenBitDraw` |
| Drawing zero gives zero without moving the stream | done | `GameRandom.ZeroDrawsNothing` |
| Particle effects draw from the game's streams only while they step | done | `GameRandom.ParticlesDrawOnlyInAStep` |
| Some things use the C runtime's own random numbers, which behave as Microsoft's | done | `GameRandom.CrtRandIsMsvcs` |
| Both streams start from a fixed seed when the game starts and are reset as each land loads | partial | the starting seed is set; nothing resets the seeds when a land loads |
| The streams' positions are saved with a game and restored on load | todo | no save games yet (see saving_and_loading.md) |

## Determinism and input

| Behaviour | Status | Where / notes |
|-----------|--------|---------------|
| The player's actions are queued and applied at the start of the next turn, not mid-frame | todo | port notes: action packets not ported; openblack acts on input in the frame |
| Replaying the same inputs gives the same game | todo | port notes: replay determinism test and per-turn state hash not ported |
| Key presses are buffered so presses between frames are not lost | partial | SDL events are drained each frame in `Game::Update` (`Game::ProcessEvents`); not compared with the game's buffering |
