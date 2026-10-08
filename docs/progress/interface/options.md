# Options

The options' pages of the menu: sound and video, the advanced page for help, text, tooltips and the hand, and how the
settings take effect and are kept. The player's page is in [profiles.md](profiles.md) and the controls page in
[key_bindings.md](key_bindings.md).

**Progress: 6/20 done, 8 partial — 50%**

## The pages

| Behaviour | Status | Where / notes |
|-----------|--------|---------------|
| The options are tabs of one box: Options, Players, Advanced and Controls, with a Main Menu tab and a Back arrow to the first page | done | `GameMenu::OptionsTabs`; test `GameMenu.OptionsOpenTheOptionsTabs` |
| A Quit arrow on the options page | done | `GameMenu.cpp` |
| The options room inside the temple shows a small version of the same settings | todo | see ../temple/ for the room |

## Sound and video

| Behaviour | Status | Where / notes |
|-----------|--------|---------------|
| Effects volume slider changes the sound effects' volume at once | done | `Game::HandleInterfaceAction` sets the audio service's volume |
| Music volume slider changes the music's volume at once | done | same |
| The menu starts with the volumes the game is playing at | done | `Game.cpp` fills `MenuSettings` from the audio service |
| Detail level selector from minimum to maximum detail, stepped by its button and arrows | partial | the selector works (`GameMenu.cpp`) but changes nothing; the detail level is only set by `--detail-level` on the command line |
| A changed detail level takes effect the next time the game starts, and the game says so | todo | |
| Auto save check box turns the land's automatic saves on or off | partial | the box works but nothing reads it; there are no automatic saves yet |
| Push scrolling check box makes the camera move when the hand pushes against the screen's edge | partial | the box works but nothing reads it |
| Screen resolution and the 3D card are picked outside the game, in its setup | n/a | openblack takes `--width`, `--height` and `--backend-type` on the command line |

## Advanced

| Behaviour | Status | Where / notes |
|-----------|--------|---------------|
| Help level from no help to all help sets how often the advisors offer help | partial | selector works; there is no help system to read it ([help_system.md](help_system.md)) |
| Story text: none, story only, or all text shown on screen as people speak | partial | selector works; no on-screen text to read it ([on_screen_text.md](on_screen_text.md)) |
| Tooltips: none, minimum, intelligent or all | partial | selector works and the tooltips have the four levels (`ToolTips::SetLevel`), but the menu's choice is never passed to them |
| Creature help check box turns on the help about what the creature is learning | partial | box works; nothing reads it |
| Left handed swaps the hand's look to the left hand | done | `Game::HandleInterfaceAction` sets `rightHandedHand` |
| Left handed also swaps which mouse button picks up and which acts | todo | (unconfirmed which buttons swap) |
| Text from the bottom of the screen instead of the top | partial | box works; no on-screen text to move |

## Keeping the settings

| Behaviour | Status | Where / notes |
|-----------|--------|---------------|
| The detail level, and the highest detail the computer was found to manage, are kept per computer | todo | nothing is kept between runs |
| Tooltip level, story text, help level, creature help, hand orientation, text position, push scrolling and auto save are kept with the player's profile, auto save on and the rest off or at the game's defaults when the profile has none | todo | |
| How fast the player reads (which sets how long text stays up) is kept with the profile | todo | |
| Force feedback mouse settings (strength and on/off) | n/a | for a long gone force feedback mouse; openblack does not support one |
