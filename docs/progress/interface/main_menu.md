# Main menu

The menu Escape brings up over the game (continue, skirmish, online, options, quit, with a statistics tab), the boxes
the game shows before play starts (choosing or making a player), and the dialog controls all of them are built from.

**Progress: 19/36 done, 3 partial — 57%**

## Opening and closing

| Behaviour | Status | Where / notes |
|-----------|--------|---------------|
| Escape brings the menu up over the game, and the game's dialogs take the mouse and keys while it is open | done | `src/Gui/GameInterface.cpp`, `src/Gui/GameMenu.cpp` |
| The menu fades in over half a second and out over a fifth | done | `src/Gui/GameMenu.cpp`; test `GameMenu.FadesInAndOut` |
| The game is paused while the menu is open and goes back to how it was when it closes | done | `Game::HandleInterfaceAction` in `src/Game.cpp` |
| Escape backs out of an options page to the first page, and closes the menu from there | done | test `GameMenu.EscapeBacksOut` |
| The menu draws its own mouse pointer, over the debug windows too | done | `GameInterface::Draw` |
| Inside the temple the options' first tab is the World Room, which closes them | done | `GameMenu::SetInsideTemple`, called from `src/3D/Implementations/TempleInterior.cpp` |
| Some boxes can't be closed with Escape (asked per box), and the menu can't be reopened while a box forbids it | partial | Escape always backs out; no box forbids it yet |
| Closing the options writes the settings back so they are there next time | todo | nothing is saved; see [options.md](options.md) |

## The first page

| Behaviour | Status | Where / notes |
|-----------|--------|---------------|
| Greets the player by their profile's name above the buttons | partial | greets by the name the user logged into the computer with; openblack has no profiles ([profiles.md](profiles.md)) |
| Continue Game closes the menu | done | `GameMenu::Action::Continue` |
| Start Skirmish Game | todo | logs "not available yet"; see [../multiplayer/skirmish.md](../multiplayer/skirmish.md) |
| Join Online Game | todo | logs "not available yet"; see ../multiplayer/ |
| The first button reads Leave Skirmish Game during a skirmish and Exit Online Game during a network game | todo | noted in `GameMenu.cpp`, not done; see [../multiplayer/skirmish.md](../multiplayer/skirmish.md) |
| Options opens the options' pages | done | test `GameMenu.OptionsOpenTheOptionsTabs` |
| Quit asks "are you sure" in a smaller opaque box with Yes and No arrows; Yes quits | done | test `GameMenu.QuittingAsksFirst` |
| The Statistics tab opens the game's statistics | todo | logs "not available yet"; see [statistics.md](statistics.md) |
| Laid out as the original: box, five tabs, button places and text sizes | done | test `GameMenu.IsLaidOutAsTheOriginal` |

## Before play starts

| Behaviour | Status | Where / notes |
|-----------|--------|---------------|
| The game starts on a front end that asks which player is playing before the first land | todo | openblack loads straight into a land |
| A first run asks for a new player's name in its own box | todo | |
| A skirmish game is set up in its own box (land, opponents) before it starts | todo | see [../multiplayer/skirmish.md](../multiplayer/skirmish.md) (the box only picks the land) |
| Changing the player during a game asks to restart | todo | |
| A box tells the player the game is being saved while it saves | todo | see ../engine/ for saving itself |
| The front end has its own pointer and turns the hand off while a box is up | partial | the menu's pointer is drawn; there is no front end yet |

## Dialog look and controls

| Behaviour | Status | Where / notes |
|-----------|--------|---------------|
| Dialogs are laid out in an 800 by 600 space, centred, and scaled down on smaller screens | done | `src/Gui/DialogPainter.cpp` |
| Boxes, bevels, tabs and shadows are drawn from the front end atlas, text in the game's font | done | `DialogPainter`, `src/Gui/GameFont.cpp`; tests `GameFont.*` |
| The control under the pointer lights up orange; a control acts when the button is let go over the one it went down on | done | `src/Gui/Dialog.cpp`; test `GameMenu.ButtonsActWhenLetGoOverTheOneTheyWentDownOn` |
| Activating a control plays the menu button sound | done | `GameInterface.cpp` |
| Buttons, big arrow buttons and square buttons | done | `src/Gui/Controls.cpp`; test `GameMenu.ClicksOnEveryControlThatActs` |
| Sliders step by clicks or follow the knob as it is dragged | done | test `GameMenu.SlidersStepOrFollowTheKnob` |
| Check boxes and radio-style selectors | done | test `GameMenu.SelectorsAndCheckBoxesChangeTheSettings` |
| Edit boxes take typed text with a caret | done | `EditBox`; test `GameMenu.PlayersNameTheirCreatureAndPickASymbol` |
| Lists scroll with the wheel and a bar, and select rows | done | `List` in `Controls.cpp`; test `GameMenu.ListsTheControls` |
| A colour picker (for the tattoo colours) | todo | |
| Line and bar graphs (the statistics) | todo | see [statistics.md](statistics.md) |
| Tab and the arrow keys move the focus between controls; Enter acts | todo | |
| Controls show a tooltip when the pointer rests on them | todo | |
