# Cinematics

What the scripts do to the picture around their cut scenes: the cinema bars, fading to a colour and back, putting the
player's interface away, and close clipping. The cut scenes and films themselves are in `../story/`.

**Progress: 13/15 done, 1 partial — 90%**

## Cinema bars

| Behaviour | Status | Where / notes |
|-----------|--------|---------------|
| The cinema bars slide in to a 16:9 picture and back out | done | `CinematicDirectorSystem::SetWideScreen`; `test/gui/test_cinematic_director.cpp` |
| Only the script that brought the bars in can take them out, or any script while none has | done | `SET_WIDESCREEN` in `src/CHLApi.cpp` |
| A script's bars put the hand and the player's interface away and hide the game's dialogs | done | `CinematicDirector.AScriptsBarsPutTheInterfaceAway` |
| The game's own bars leave the interface | done | `CinematicDirector.TheGamesOwnBarsLeaveTheInterface` |
| Setting the bars the same again changes nothing | done | `CinematicDirector.SettingTheSameAgainChangesNothing` |
| "widescreen ready": whether the bars have finished sliding | done | `WIDESCREEN_TRANSISTION_FINISHED` |
| The bars slide with game time and stop while the game is paused | done | `CinematicDirectorSystem::Update` |
| The player's input is blocked while the bars are in | done | port notes (`PROGRESS-bw-clean.md`, widescreen input block) |
| Escape skips a cut scene or film | partial | skipping films waits for the film player (port notes) |

## Fades

| Behaviour | Status | Where / notes |
|-----------|--------|---------------|
| Fade the picture to a colour over whole seconds, or at once | done | `SET_FADE`, `CinematicDirectorSystem::FadeTo`; `test/gui/test_screen_fade.cpp` |
| Fade back to normal over whole seconds | done | `SET_FADE_IN` |
| "fade finished": whether the fade is done | done | `FADE_FINISHED` |
| A new land opens with no fade, no bars and no close clipping | done | `CinematicDirectorSystem::Reset` |

## Close shots

| Behaviour | Status | Where / notes |
|-----------|--------|---------------|
| Scripts bring the near plane right in for close shots | done | `SET_GRAPHICS_CLIPPING`; `NearClipping.ScriptsCanClipClose` |
| Close clipping is cleared when the scripts restart | todo | openblack has no script reboot yet (port notes) |
