# On-screen text

The words the game writes over the screen: what the advisors and characters say, text that land scripts put up, titles,
and the game's text database and fonts that all of it comes from. The script functions themselves are listed in
../story/.

**Progress: 6/24 done, 1 partial — 27%**

## Text and fonts

| Behaviour | Status | Where / notes |
|-----------|--------|---------------|
| The game's texts are read from its info scripts by name, later scripts replacing earlier ones | done | `src/Gui/TextDatabase.cpp`; test `TextDatabase.ReadsTheAddTextLines` |
| Each text says who speaks it (narrator, good or evil advisor, a character) | todo | the speaker in each line is not read |
| The game's fonts are read and drawn as the game draws them, with a question mark for missing letters | done | `src/Gui/GameFont.cpp`; tests `GameFont.*` |
| Lines wrap after spaces and hyphens and at line breaks | done | `GameFont::Wrap`; test `GameFont.WrapsAtSpacesHyphensAndLineBreaks` |
| Text in other languages (the game's other language files) | todo | only the English scripts are read (unconfirmed which others the game ships) |

## Spoken text

| Behaviour | Status | Where / notes |
|-----------|--------|---------------|
| What is said appears over the screen, a word at a time in step with the voice | todo | |
| The story text setting chooses no text, story text only, or all text | todo | ([options.md](options.md)) |
| Text sits at the top of the screen, or at the bottom with the setting | todo | |
| Text stays up for as long as it takes to read, by the player's read speed, then fades | todo | |
| Some text waits for a click to go on, shown by a prompt | todo | |
| Codes inside a text change its colour or put in a picture of the key or mouse button to press | todo | |
| A number can be filled into a text | todo | `RUN_TEXT_WITH_NUMBER` in `src/CHLApi.cpp` logs "not implemented" |
| Text running when the player goes into the temple is put away and comes back on leaving | todo | |

## Script text

| Behaviour | Status | Where / notes |
|-----------|--------|---------------|
| A script runs a text by name, on one line or wrapped, with or without waiting for the player | todo | `RUN_TEXT`, `TEMP_TEXT` log "not implemented" |
| A script draws text anywhere on screen at a size, fading in, in a colour it sets | todo | `GAME_DRAW_TEXT`, `GAME_DRAW_TEMP_TEXT`, `SET_DRAW_TEXT_COLOUR` log "not implemented" |
| A script fades all its drawn text out | todo | `FADE_ALL_DRAW_TEXT` |
| A script asks whether the text has been read | todo | `TEXT_READ` |
| Land titles and challenge titles shown as a challenge begins | todo | see ../story/ |
| Words shown over the screen by the temple's future room | done | `GameInterface::SetMessage`; see ../temple/ |
| Game messages a land's feature script starts and adds lines to | todo | `START_GAME_MESSAGE`, `ADD_GAME_MESSAGE_LINE` in `src/LHScriptX/FeatureScriptCommands.cpp` do nothing |

## Other overlays

| Behaviour | Status | Where / notes |
|-----------|--------|---------------|
| Text shown while the game is paused | todo | (unconfirmed what it says) |
| Villagers' names, and their details, over their heads | partial | drawn by the debug overlay (`src/Debug/Gui.cpp`), not in the game's font or look |
| The creature's status panel and the fight's panel | done | `GameInterface::SetCreaturePanel`, `SetFightPanel`; tests `test_creature_status_panel.cpp`; see ../creature/ |
| Cinema bars and screen fades | done | `src/Gui/CinemaBars.cpp`, `src/Gui/ScreenFade.cpp`; see ../camera/ and ../story/ |
