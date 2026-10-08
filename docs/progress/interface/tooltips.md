# Tooltips

The words, mouse picture and arrows the hand shows beside it for what it is over: what a click would do, and numbers
such as a town's desires or a store's food. Which tooltip each thing in the world offers as the hand points at it is
in ../hand/; this file is the tooltip system and the status readouts it shows.

**Progress: 16/31 done, 0 partial — 52%**

## The tooltip system

| Behaviour | Status | Where / notes |
|-----------|--------|---------------|
| The game has 170 tooltips, each with a priority and how long it shows, from the info scripts | done | `src/Gui/ToolTips.h`, read from the info tables; test `ToolTips.NamesTheirTexts` |
| What the hand is over offers a tooltip every turn; once a turn the shown one lives on or ends | done | `ToolTips::Submit`, `ToolTips::ProcessTurn` |
| No tooltips at the None level | done | `ToolTipLevel::None` |
| At the Minimum level only the important ones (numbers and stats) show | done | test `ToolTips.ShowOnlyTheImportantAtTheMinimumLevel` |
| At the Intelligent level each fades in a second slower each time it is shown, and fades out once read | done | tests `ToolTips.FadeInOverOneMoreSecondEachTimeShown`, `ToolTips.FadeOutOnceReadAtTheIntelligentLevel` |
| At the All level they show at once and stay | done | test `ToolTips.ShowAtOnceAndStayAtTheAllLevel` |
| A tooltip keeps others off for its display time, and some linger after the hand leaves | done | tests `ToolTips.KeepOthersOffForTheirDisplayTime`, `ToolTips.LingerAfterFocusWhenTheirInfoSaysSo` |
| A forced tooltip shows at once, over one still showing | done | `Submit(..., force)` |
| The level is the one the player picked in the options | todo | the menu's choice is not passed to the tooltips ([options.md](options.md)) |
| How many times each tooltip has been shown is kept with the player's profile, so intelligent tooltips stay learnt | todo | |

## How a tooltip looks

| Behaviour | Status | Where / notes |
|-----------|--------|---------------|
| Words beside the hand, sized by the screen's height and kept on the screen | done | `GameInterface::DrawToolTip` |
| A picture of the mouse with the button to press lit | done | the mice picture, `ToolTipAction` |
| Arrows about the mouse for the ways it can be dragged | done | `ToolTipArrows` |
| Soft glows behind the mouse and the words | done | `GameInterface::DrawGlow` |
| Right of the hand, or left of it near the right of the screen | done | `DrawToolTip` |
| Numbers filled into the words, as in "Food Amount: 120" | todo | no tooltip with a number is offered yet |
| The tooltip hides while the menu or a script's cinema bars are up | done | `Game::ProcessHandToolTipTurn` |

## Where tooltips are offered

| Behaviour | Status | Where / notes |
|-----------|--------|---------------|
| Over the player's own creature: interact | done | `Game::ProcessHandToolTipTurn` |
| Inside the temple: moving, zooming, doors, scrolls and the rooms' toggles | done | `src/3D/TempleToolTips.cpp`; tests `TempleToolTips.*`; see ../temple/ |
| Over everything else in the world (pick up, throw, cast, leash, tap and so on) | todo | see ../hand/ |
| On the dialogs' controls | todo | see [main_menu.md](main_menu.md) |

## Status readouts

| Behaviour | Status | Where / notes |
|-----------|--------|---------------|
| Over a village centre: the town's population and its strongest desires as percentages | todo | see ../town/ for the desires themselves |
| Over the village store: the food and wood stored | todo | |
| Over a building being built: how far it has got, and the wood it still needs | todo | |
| Over a workshop: how far its scaffold is made, and a scaffold's value | todo | see [../building/workshop_and_scaffolds.md](../building/workshop_and_scaffolds.md) |
| Over a worship site: people worshipping, prayer power, and a miracle's charge | todo | |
| Over another god's town: the belief needed to win it, or left, or got | todo | |
| Over a villager: their need as a percentage | todo | |
| Over the temple: its health | todo | |
| Over a pile or a field: the amount of food or wood | todo | |
| Over a signpost or a scroll: what it is and that it can be read | todo | see [scrolls_and_signs.md](scrolls_and_signs.md) |
