# Challenge natives: sound and music

The challenge scripts' functions for music, sound effects, spoken sounds and sound tags on objects. The game has 464 of these functions in all; the language statement each comes from is shown in italics, and "called" counts are calls in the shipped `challenge.chl`. How the virtual machine runs them is in [../engine/script_vm.md](../engine/script_vm.md); what each challenge is about is in [../story/](../story/).

**Progress: 0/26 done, 3 partial — 6%**

## Used by the shipped scripts

| Behaviour | Status | Where / notes |
|-----------|--------|---------------|
| Plays a sound effect from one of the sound banks, at a position or on an object: *start sound ‹sound› [‹soundbank›] ‹position› ‹with position›* (called 186 times in 54 scripts) | todo | `PlaySoundEffect` logs "not implemented" |
| Starts a piece of music: *start music ‹music›* (called 123 times in 84 scripts) | partial | `StartMusic`: `GameMusic::StartScriptMusic`; music types checked |
| Stops the script's music: *stop music* (called 108 times in 77 scripts) | partial | `StopMusic`: stops the script's music |
| Fixes a piece of music to an object so it is heard near it: *attach music ‹music› to ‹target›* (called 22 times in 13 scripts) | todo | `AttachMusic` logs "not implemented" |
| Takes the music off an object: *detach music from ‹object›* (called 12 times in 7 scripts) | todo | `DetachMusic` logs "not implemented" |
| Turns the angle sound (heard as the camera turns) on or off: *enable/disable angle sound* (called 2 times in 1 script) | todo | `StartAngleSound285` logs "not implemented" |
| Turns the creature's sounds on or off: *enable/disable creature sound* (called 42 times in 6 scripts) | todo | `SetCreatureSound` logs "not implemented" |
| Gives the line the music has got to: *music line ‹line›* (called 36 times in 3 scripts) | todo | `LastMusicLine` logs "not implemented" |
| Plays a spoken sound effect: *start say [extra] sound ‹sound› ‹position› ‹with position›* (called 74 times in 24 scripts) | todo | `GamePlaySaySoundEffect` logs "not implemented" |
| Turns the angle sound on or off (second form): *enable/disable angle sound* (called 2 times in 1 script) | todo | `StartAngleSound348` logs "not implemented" |
| Turns sound effects on or off: *enable/disable sound effects* (called 8 times in 5 scripts) | todo | `SetGameSound` logs "not implemented" |
| Stops a sound effect: *stop [say] sound ‹sound› [‹soundbank›]* (called 19 times in 7 scripts) | todo | `StopSoundEffect` logs "not implemented" |
| Turns the music that follows the player's alignment on or off: *enable/disable alignment music* (called 2 times in 1 script) | partial | `EnableDisableAlignmentMusic`: `GameMusic::SetAlignmentMusicEnabled` |
| Fixes a sound to an object, optionally heard in 3D: *attach [3d] sound tag ‹sound› [‹soundbank›] to ‹target›* (called 5 times in 2 scripts) | todo | `AttachSoundTag` logs "not implemented" |
| Takes a sound off an object: *detach sound tag ‹sound› [‹soundbank›] from ‹target›* (called 6 times in 3 scripts) | todo | `DetachSoundTag` logs "not implemented" |
| Whether a sound effect is playing: *sound ‹sound› [‹soundbank›] playing* (called 17 times in 3 scripts) | todo | `GameSoundPlaying` logs "not implemented" |
| Whether a spoken sound effect is playing: *say [extra] sound ‹sound› playing* (called 9 times in 1 script) | todo | `SaySoundEffectPlaying` logs "not implemented" |

## Not used by the shipped scripts

The game has these but no shipped script calls them; mods and fan-made challenges can.

| Behaviour | Status | Where / notes |
|-----------|--------|---------------|
| Moves a piece of music from one object to another: *move music from ‹value› to ‹value›* (not called by the shipped scripts) | todo | `MoveMusic` logs "not implemented" |
| Turns the music on an object on or off: *enable/disable music on ‹value›* (not called by the shipped scripts) | todo | `EnableDisableMusic` logs "not implemented" |
| Gives the distance from the camera to an object's music (unconfirmed): *get ‹source› music distance* (not called by the shipped scripts) | todo | `GetMusicObjDistance` logs "not implemented" |
| Gives the distance from the camera to a piece of music (unconfirmed): *get music ‹type› distance* (not called by the shipped scripts) | todo | `GetMusicEnumDistance` logs "not implemented" |
| Moves the music on an object to a point in it: *set ‹value› music position to ‹value›* (not called by the shipped scripts) | todo | `SetMusicPlayPosition` logs "not implemented" |
| Starts the music on an object again: *restart music on ‹value›* (not called by the shipped scripts) | todo | `RestartMusic` logs "not implemented" |
| Whether the music on an object has played: *‹value› music played* (not called by the shipped scripts) | todo | `MusicPlayed191` logs "not implemented" |
| Whether the music on an object has played (second form): *‹value› music played* (not called by the shipped scripts) | todo | `MusicPlayed350` logs "not implemented" |
| Whether a sound exists: *sound exists* (not called by the shipped scripts) | todo | `SoundExists` logs "not implemented" |
