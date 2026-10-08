# Landslide

A cut Land 1 challenge: a rock slide buries three Aztec farmers on a hillside; an Aztec fisherman asks for help, and the
player has five minutes to lift the boulders off them, which wins the small Aztec village's belief. It was never
compiled into the game; neither its title nor any of its lines exist in the text table.

**Land:** 1 (an early version of it; never started) · **Giver:** an Aztec fisherman on the hillside · **Script:** Landslide · **Reward:** belief from the small Aztec village · **Repeatable:** no

**Progress: 0/0 done, 0 partial — 0%**

Sources: the script's source text, its test launcher, the uncompiled early Land 1 control script, Land 1's map script
and the game's text table. Every row is n/a: the game never runs this quest.

## Is it in the game?

| Behaviour | Status | Where / notes |
|-----------|--------|---------------|
| Not in the compiled challenge file; only the uncompiled early Land 1 control script and a single-quest test launcher start it | n/a | never started by the game |
| Its title, reminder and all nine lines are missing from the text table | n/a | never started by the game |
| It is set on Land 1 (it resets the camera zones to Land 1's at the end), but the Aztec village it looks for within 50 of a spot is 79 away on the shipped map, so it would find no village | n/a | never started by the game: checked against `Land1.txt` |
| It calls the scroll's nag helper with one argument too many for the shipped helper | n/a | never started by the game |
| Not reachable at all in the shipped game | n/a | never started by the game |

## How it starts

| Behaviour | Status | Where / notes |
|-----------|--------|---------------|
| An Aztec fisherman is made on the hillside, with a silver scroll over him; the good advisor nags with an evil advisor's line ("Whew! As if you ain't busy enough, here's something for you to do.") until clicked | n/a | never started by the game: the line belongs to the evil advisor's voice |
| On click the challenge record opens and three half-size rocks are placed where the farmers lie buried | n/a | never started by the game |

## The slide

| Behaviour | Status | Where / notes |
|-----------|--------|---------------|
| Cut scene: the camera flies a path while the ground shakes within 100 of the fisherman for 4 seconds, and nine rocks of sizes 0.1 to 0.8 appear high on the slope over about 5 seconds and tumble down | n/a | never started by the game |
| One line, then the camera returns; the evil advisor speaks; the good advisor points in turn at the three buried men (who don't exist yet, so the pointing goes nowhere) and speaks two lines | n/a | never started by the game: lines missing; pointing at the men before they are made is a script bug |
| A 5 minute timer starts | n/a | never started by the game |

## Digging them out

| Behaviour | Status | Where / notes |
|-----------|--------|---------------|
| Picking up or moving one of the three rocks uncovers an Aztec farmer at half health where it lay, who joins the village | n/a | never started by the game |
| When all three are uncovered, the fisherman joins the village too, both advisors speak three lines, and the village's belief in the player is set to three times relative belief | n/a | never started by the game |
| When the timer reaches zero first, the ground shakes again, six rocks come down, both advisors speak two lines and the quest ends | n/a | never started by the game |
| Neither ending closes the challenge record, so it would stay open in the log | n/a | never started by the game: script bug |
| At the end the camera zones are set to Land 1's | n/a | never started by the game |
