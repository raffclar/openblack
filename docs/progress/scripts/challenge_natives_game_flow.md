# Challenge natives: game flow, time and scripts

The challenge scripts' functions for running the story: timers, the time of day, random numbers, stopping scripts, loading the next land, game speed, the temple, the real clock, keys and profile choices. The game has 464 of these functions in all; the language statement each comes from is shown in italics, and "called" counts are calls in the shipped `challenge.chl`. How the virtual machine runs them is in [../engine/script_vm.md](../engine/script_vm.md); what each challenge is about is in [../story/](../story/).

**Progress: 10/62 done, 7 partial — 22%**

## Used by the shipped scripts

| Behaviour | Status | Where / notes |
|-----------|--------|---------------|
| Gives the distance between two positions: *get distance from ‹p0› to ‹p1›* (called 712 times in 200 scripts) | done | `GetDistance`: plain distance |
| Gives a random number between two numbers, from the game's own random numbers: *number from ‹min› to ‹max›* (called 162 times in 51 scripts) | partial | `Random`: uses the C library's random numbers, not the game's own sequence |
| Gives the time the game has been running: *time* (called 51 times in 20 scripts) | todo | `DllGettime` pushes nothing back, so the script's stack is off by one value afterwards |
| Gives a random whole number between two numbers: *constant from ‹min› to ‹max›* (called 40 times in 22 scripts) | todo | `RandomUlong` logs "not implemented" |
| Changes how fast the game runs: *set game speed to ‹speed›* (called 6 times in 3 scripts) | todo | `SetGamespeed` logs "not implemented" |
| Sets the hour of the day: *set game time ‹time›* (called 31 times in 13 scripts) | done | `SetGameTime`: `SkySystem::SetTime` |
| Gives the hour of the day: *get game time* (called 27 times in 8 scripts) | done | `GetGameTime`: the clock's script time |
| Puts the game at cinema speed for a cut scene (part of the cinema block): *begin cinema (game speed part)* (called 328 times in 181 scripts) | todo | `StartGameSpeed` logs "not implemented" |
| Puts the game back to its normal speed after a cut scene: *end cinema (game speed part)* (called 450 times in 238 scripts) | todo | `EndGameSpeed` logs "not implemented" |
| Gives a position at a distance and angle from one place towards another: *get target from ‹from› to ‹to› distance ‹distance› angle ‹angle›* (called 10 times in 7 scripts) | todo | `GetTargetRelativePos` logs "not implemented" |
| Sets a timer's time: *set ‹timer› time to ‹time› seconds* (called 161 times in 68 scripts) | todo | `SetTimerTime` logs "not implemented" |
| Creates a timer running for a number of seconds: *create timer for ‹timeout› seconds* (called 137 times in 84 scripts) | todo | `CreateTimer` logs "not implemented" |
| Gives the time a timer has left: *get ‹timer› time remaining* (called 154 times in 79 scripts) | todo | `GetTimerTimeRemaining` logs "not implemented" |
| Gives the time since a timer was set: *get ‹timer› time since set* (called 6 times in 6 scripts) | todo | `GetTimerTimeSinceSet` logs "not implemented" |
| Loads the next land (its land script) for the story: *load map ‹path›* (called 4 times in 1 script) | todo | `LoadMap` does nothing: the next land is never loaded |
| Stops every script except those from the named source files: *stop all scripts excluding files ‹source filenames›* (called 6 times in 2 scripts) | partial | `StopAllScriptsInFilesExcluding`: stops by source file through `LHVM::StopScripts` |
| Stops a script by name: *stop script ‹script name›* (called 69 times in 31 scripts) | partial | `StopScript`: stops by script name through `LHVM::StopScripts`; openblack compares the whole text with each script's name; the game is reported to split it into several names at spaces, commas and tabs (read from the executable, not yet verified) |
| Stops every script from the named source files: *stop scripts in files ‹source filenames›* (called once in 1 script) | partial | `StopScriptsInFiles`: stops by source file through `LHVM::StopScripts` |
| Runs one of the developers' functions by number: *run ‹func› developer function* (called 23 times in 15 scripts) | todo | `DevFunction` logs "not implemented" |
| Whether the player's mouse has a wheel: *player has mouse wheel* (called once in 1 script) | todo | `HasMouseWheel` logs "not implemented" |
| Gives how many times a kind of event has happened: *get ‹type› total event* (called 23 times in 7 scripts) | todo | `GetTotalEvents` logs "not implemented" |
| Stops or starts the day going by: *enable/disable game time* (called 36 times in 13 scripts) | done | `GameTimeOnOff`: the clock stops and starts |
| Moves the time of day to an hour over a time: *move game time ‹hour of the day› time ‹duration›* (called 5 times in 5 scripts) | done | `MoveGameTime`: the clock's scripted move |
| Stops the scripts of the named files except those named: *stop scripts in files ‹source filenames› excluding ‹script names›* (called once in 1 script) | partial | `StopScriptsInFilesExcluding`: stops by file and name through `LHVM::StopScripts` |
| Whether the player is inside the temple: *inside temple* (called 3 times in 1 script) | todo | `InsideTemple` logs "not implemented" |
| Lets the player into the temple or not: *enable/disable temple* (called 2 times in 2 scripts) | todo | `SetInterfaceCitadel` logs "not implemented" |
| Runs a line of land script: *run map script line ‹command›* (called 2 times in 2 scripts) | todo | `MapScriptFunction` logs "not implemented" |
| Whether a key is held down: *key ‹key› down* (called 13 times in 8 scripts) | todo | `KeyDown` logs "not implemented" |
| Starts an immersion (force feedback) effect: *start immersion ‹effect›* (called once in 1 script) | todo | `StartImmersion` logs "not implemented" |
| Gives the position of a player's temple: *player ‹player› temple position* (called 2 times in 2 scripts) | todo | `GetTemplePosition` logs "not implemented" |
| Gives a miracle's icon in the temple: *get spell icon ‹spell› in ‹temple›* (called once in 1 script) | todo | `GetSpellIconInTemple` logs "not implemented" |
| Gives the position of a player's temple entrance: *player ‹player› temple entrance position radius ‹radius› height ‹height›* (called 2 times in 1 script) | todo | `GetTempleEntrancePosition` logs "not implemented" |
| Whether the player chose to skip the tutorial: *can skip tutorial* (called once in 1 script) | todo | `CanSkipTutorial` logs "not implemented" |
| Whether the player chose to skip the creature's training: *can skip creature training* (called once in 1 script) | todo | `CanSkipCreatureTraining` logs "not implemented" |
| Whether the player is keeping their old creature: *is keeping old creature* (called once in 1 script) | todo | `IsKeepingOldCreature` logs "not implemented" |
| Whether the current profile has a creature: *current profile has creature* (called once in 1 script) | todo | `CurrentProfileHasCreature` logs "not implemented" |

## Not used by the shipped scripts

The game has these but no shipped script calls them; mods and fan-made challenges can.

| Behaviour | Status | Where / notes |
|-----------|--------|---------------|
| Starts the countdown clock shown on screen: *start the countdown clock (no statement used)* (not called by the shipped scripts) | todo | `StartCountdownTimer` logs "not implemented" |
| Removes the countdown clock: *remove the countdown clock (no statement used)* (not called by the shipped scripts) | todo | `RemoveCountdownTimer` logs "not implemented" |
| Gives the time left on the countdown clock: *time left on the countdown clock (no statement used)* (not called by the shipped scripts) | todo | `GetCountdownTimer` logs "not implemented" |
| Whether the countdown clock exists: *countdown clock exists (no statement used)* (not called by the shipped scripts) | todo | `CountdownTimerExists` logs "not implemented" |
| Hides the countdown clock: *hide the countdown clock (no statement used)* (not called by the shipped scripts) | todo | `HideCountdownTimer` logs "not implemented" |
| Gives how full the moon is: *get moon percentage* (not called by the shipped scripts) | todo | `GetMoonPercentage` logs "not implemented" |
| Keeps an object from being deleted while a script holds it: *keeping an object (emitted by the compiler, not written)* (not called by the shipped scripts) | done | `ScriptObjectsSystem::AddReference` (`410656e9`): the object takes a place in the scripts' table, is in a script, and is controlled at its first reference if a script made it or already controlled it; cleared with the land (`d39f8402`) |
| Lets go of an object the script was keeping: *letting an object go (emitted by the compiler, not written)* (not called by the shipped scripts) | done | `ScriptObjectsSystem::RemoveReference` (`410656e9`): the count never goes below nothing and the place stays the object's until the land's scripts are cleared |
| Gives the real time on the player's computer clock: *get real time* (not called by the shipped scripts) | todo | `GetRealTime` logs "not implemented" |
| Gives the real day of the week: *get real day* (not called by the shipped scripts) | todo | `GetRealDay115` logs "not implemented" |
| Gives the real day of the month: *get real day* (not called by the shipped scripts) | todo | `GetRealDay116` logs "not implemented" |
| Gives the real month: *get real month* (not called by the shipped scripts) | todo | `GetRealMonth` logs "not implemented" |
| Gives the real year: *get real year* (not called by the shipped scripts) | todo | `GetRealYear` logs "not implemented" |
| Shows the countdown clock again: *show the countdown clock (no statement used)* (not called by the shipped scripts) | todo | `RevealCountdownTimer` logs "not implemented" |
| Stops every script except those named: *stop all scripts excluding ‹script names›* (not called by the shipped scripts) | partial | `StopAllScriptsExcluding`: stops by script name through `LHVM::StopScripts` |
| Gives the number of buttons on the player's mouse: *number of mouse buttons* (not called by the shipped scripts) | todo | `NumMouseButtons` logs "not implemented" |
| Gives how often a kind of event happens a second: *get ‹type› event per seconds* (not called by the shipped scripts) | todo | `GetEventsPerSecond` logs "not implemented" |
| Gives the time since a kind of event last happened: *get time since ‹type› event* (not called by the shipped scripts) | todo | `GetTimeSince` logs "not implemented" |
| Turns the intro building sequence on or off (unconfirmed): *enable/disable intro building* (not called by the shipped scripts) | todo | `SetIntroBuilding` logs "not implemented" |
| Takes the player into the temple or back out: *enter/exit temple* (not called by the shipped scripts) | partial | `EnterExitCitadel`: activates or leaves the temple; the game's walk-in and walk-out unconfirmed |
| Gives the square root of a number: *square root ‹value›* (not called by the shipped scripts) | done | `SquareRoot`: square root, nought for nought or less |
| Sets the length of a day and how much of it is night and dawn and dusk: *set game time properties duration ‹duration› percentage night ‹percentage night› percentage dawn dusk ‹percentage change›* (not called by the shipped scripts) | done | `SetGameTimeProperties`: the clock's cycle |
| Puts the day's length back to normal: *reset game time properties* (not called by the shipped scripts) | done | `ResetGameTimeProperties`: the clock's default cycle |
| Stops an immersion effect: *stop immersion ‹effect›* (not called by the shipped scripts) | todo | `StopImmersion` logs "not implemented" |
| Stops all immersion effects: *stop all immersion* (not called by the shipped scripts) | todo | `StopAllImmersion` logs "not implemented" |
| Saves the game in a slot: *save game in slot ‹slot›* (not called by the shipped scripts) | todo | `SaveGameInSlot` logs "not implemented" |
