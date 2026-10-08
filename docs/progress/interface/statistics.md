# Statistics

The game counts what each god and their people do and shows it in a Game Statistics box: six tabs of figures, a
list of every figure with each player's value side by side, bar graphs comparing the players, line graphs of influence
and population over the whole game, and on the creature tab each player's creature turning in 3D. This file is the
box and how it is reached; what each figure counts, when, and what is saved is in
[statistics_counted.md](statistics_counted.md).

**Progress: 0/46 done, 1 partial — 1%**

## Getting there

Story lands and the end of the story show no statistics screen of their own, and no key opens the box: it is reached
only from the in-game menu and from the end-of-game box of a network or skirmish game. Other places show some of the
same figures in their own way: the temple's world scroll ([../temple/main_room.md](../temple/main_room.md)), the
creature cave's scrolls ([../creature/creature_cave.md](../creature/creature_cave.md)), the creature's status panel
([../creature/creature_mode.md](../creature/creature_mode.md)) and the creature's web page
([../creature/lessons_and_help.md](../creature/lessons_and_help.md)).

| Behaviour | Status | Where / notes |
|-----------|--------|---------------|
| The in-game menu's Statistics tab opens the Game Statistics box | todo | the tab is there (`src/Gui/GameMenu.cpp`) and logs "not available yet"; see [main_menu.md](main_menu.md) |
| The box's first tab reads "Main Menu" and goes back to the in-game menu | todo | |
| A "Back" button at the bottom left closes the box | todo | |
| The end-of-game box of a network or skirmish game has a "Statistics" tab that opens the same box | todo | the end-of-game box itself (won or lost, scores, credits, Watch Game, Leave Game) is the multiplayer domain's: [../multiplayer/network_play.md](../multiplayer/network_play.md), [../multiplayer/skirmish.md](../multiplayer/skirmish.md); when each version of it opens: [../story/losing_and_game_over.md](../story/losing_and_game_over.md) |
| Opened from the end of a game, the box's first tab reads "Game Over" and goes back to the end-of-game box | todo | |
| At the end of an online game the figures are also sent to Lionhead's statistics database | n/a | the server is gone; see [statistics_counted.md](statistics_counted.md) and [../pc_integration/online_services.md](../pc_integration/online_services.md) |

## The box

| Behaviour | Status | Where / notes |
|-----------|--------|---------------|
| The box is built from the menu's dialog controls: tab buttons, a heading, a list, check boxes and a big button | partial | openblack has these controls (`src/Gui/Controls.cpp`, `src/Gui/Dialog.cpp`) but no statistics box |
| Laid out in the menu's 800 by 600 space and scaled with it, like every dialog | todo | the scaling itself is done for the menu ([main_menu.md](main_menu.md)) |
| Six tabs along the top in small text: "Main Menu" (or "Game Over"), "Player Stats", "Creature Stats", "Town Stats", "Miracle Stats" and "Tech Stats" | todo | |
| It opens on Player Stats | todo | |
| A heading, "Game Statistics", across the top of the box in medium text | todo | |
| Below the heading, an area for graphs: three boxes side by side, each 220 by 230, at 40, 290 and 540 across | todo | |
| Below the graphs, a list 720 wide and 150 tall of the tab's figures, in small text, scrolling with a bar and the wheel | todo | the menu's list scrolls the same way ([main_menu.md](main_menu.md)) |
| At the bottom, the "Back" button on the left and two check boxes, "Graphic" and "Numeric", which work as a pair: ticking one unticks the other | todo | Graphic starts ticked |
| The figures are taken when the box opens (and each time a tab is picked) and don't change while it stays open | todo | |
| Only the first four players are shown: human and computer gods, never the neutral player | todo | |
| The controls light up and click like the rest of the menu's | todo | see [main_menu.md](main_menu.md) |

## The list of figures

| Behaviour | Status | Where / notes |
|-----------|--------|---------------|
| Each row has the figure's name on the left and the players' values in the right-hand 300 pixels of the row | todo | |
| With Numeric ticked, each player's value is written in that player's colour, one column each | todo | |
| After the players' columns a Total column in white adds them up; it is left out for figures shown as percentages | todo | |
| In a one-player game only the Total column is drawn, with a "Total" heading over it | todo | |
| With Graphic ticked, a figure is drawn as one horizontal bar per player in the player's colour instead of numbers | todo | up to four bars a row |
| Some figures are always written as numbers, whatever is ticked: the alignment changes, the creature's alignment change and every Tech Stats figure | todo | |
| Values are written as whole numbers, as "value/most" (a value out of a maximum) or as a percentage of a maximum | todo | which figure uses which is in [statistics_counted.md](statistics_counted.md) |
| Counts have no decimals; alignments, growth, satisfaction and the creature's damage, hunger and tiredness have two; chants and frame rates have one | todo | |
| A bar's full length is the largest player's value, unless the figure has a fixed maximum (the "value/most" and percentage figures) | todo | |
| Values are held between minus and plus 10^20 before they are shown | todo | |
| Clicking a row shows that figure in the right-hand graph box as bars, one per player; clicking the row already shown does nothing | todo | |

## The bar graphs

| Behaviour | Status | Where / notes |
|-----------|--------|---------------|
| Each graph box holds one vertical bar per player in the player's colour | todo | |
| With one player, the bar is scaled to the figure's own maximum; with more, to the tallest player | todo | |
| Picking Town Stats fills its graphs with Average Satisfaction Of Villagers, Total Buildings Built and Final Total Population | todo | |
| Miracle Stats fills them with the aggressive, nice and creature miracle totals | todo | |
| Tech Stats fills them with Min Frame Rate, Max Frame Rate and Total Lines Of Code Executed | todo | |
| Player Stats shows Final Area Of Influence on the right, with the two line graphs in place of the other two boxes | todo | |
| Creature Stats shows the creatures in place of the first two boxes; the right-hand box stays empty until a row is clicked | todo | the game also fills the two hidden boxes with Battles Fought and Battles Won, which never show |

## The line graphs (Player Stats)

| Behaviour | Status | Where / notes |
|-----------|--------|---------------|
| Two graphs on the left, each 470 by 100: "Influence" above and "Population" below | todo | |
| Population draws one line per player in the player's colour across the whole game so far, two pixels thick | todo | |
| Influence stacks the players' shares of the summed influence at each moment, filled bands that always reach the top | todo | |
| Each graph is a bevelled box with its name written under its bottom edge with a shadow; the name lights up when the pointer is over the graph | todo | |
| Population has horizontal guide lines: their step starts at a quarter and doubles until they are spread out, every fourth line brighter and the zero line white | todo | |
| With the pointer over a graph, the value at the pointer's height is written by its left edge: Population with two decimals when the graph's top is 10 or less, else a whole number; Influence as a percentage | todo | |
| The graphs cover the whole game: they hold 500 points, each the average of 50 game turns at first; when full, pairs are merged and each point then covers twice as many turns | todo | how they are kept is in [statistics_counted.md](statistics_counted.md) |
| Only finished points are drawn, not the one still being averaged | todo | |

## The creatures (Creature Stats)

| Behaviour | Status | Where / notes |
|-----------|--------|---------------|
| Each player's creature is drawn in 3D in a box filled with the player's colour | todo | |
| The creatures turn slowly on the spot, once every 8.2 seconds, all in step | todo | |
| One creature fills the area of two graph boxes; two stand side by side; three are two above and one centred below; four are two by two | todo | |

## Hidden, debug and cut

| Behaviour | Status | Where / notes |
|-----------|--------|---------------|
| The text tables hold labels among the box's that nothing uses: "Advanced", a second "Back", "Credits" and "Total Polygons Drawn" | n/a | unused by the game; nothing to do |
| The text tables hold labels for five creature miracles the box never lists (Hungry, Frightened, Tired, Ill, Thirsty) | n/a | unused by the game; see [statistics_counted.md](statistics_counted.md) |
| Older versions saved the graph lines as objects of their own; the save code still names that type as obsolete | n/a | nothing to load |
| With the game's debug messages on, the local player's population share, births, deaths, abodes and wonders are written every turn | todo | openblack's debug views ([../debug/diagnostics.md](../debug/diagnostics.md)) don't show these |
