# Vanilla bugs: interface and text

Bugs in the original game's interface, help and advisor code, and errors in its text tables and data files that are not
specific to one land script. Lines are quoted as the shipped English text table has them.

**Bugs: 11 (soft-locks and lost progress: 0)**

## Missing or wrong feedback

| Bug | What the player sees | Evidence | Found in | openblack |
|-----|----------------------|----------|----------|-----------|
| The advisors' camera lesson only ever answers "yes" when the player turns or tilts the camera | The advisors never say "no" for a wrong turn | Read from the advisor code (ported on Diego's bw-clean branch, which keeps the slip) | [advisors.md](../story/advisors.md) | undecided |
| The hand's "catch" help message is tested after the caught thing has already left the physics, so the pick-up message is always chosen | The catch help never appears; the first pick-up's help does | Read from the executable | [picking_up.md](../hand/picking_up.md) | undecided (openblack has no help system yet) |
| A fourth target in the creature cave has a tooltip saying it zooms in, but clicking it does nothing | A tooltip that promises a zoom that never comes | Read from the cave's targets | [creature_cave.md](../creature/creature_cave.md) | reproduced |

## Data and text errors

| Bug | What the player sees | Evidence | Found in | openblack |
|-----|----------------------|----------|----------|-----------|
| The beach temple scroll line reads "There's someone is this little temple. He definitely wants something." | The typo in the dialogue text | The English text table | [the_beach_temple_puzzle.md](../story/silver_scrolls/the_beach_temple_puzzle.md) | undecided |
| The evil advisor's line reads "On no. It's those crazy missionaries again." | The typo in the dialogue text | The English text table | [the_explorers_again.md](../story/silver_scrolls/the_explorers_again.md) | undecided |
| A sailor's line reads "We're travelled far and done much in your name." | The typo in the dialogue text | The English text table | [the_explorers_again.md](../story/silver_scrolls/the_explorers_again.md) | undecided |
| Khazar's line reads "Come with me. I have a set up an area where you can learn more." | The typo in the dialogue text | The English text table | [khazars_fireball_challenge.md](../story/gold_scrolls/khazars_fireball_challenge.md) | undecided |
| The Sea's failure line reads "Er, lordly one. You're gave it a good go. Thank you for trying." | nothing visible: no script uses the line | The English text table | [the_sea.md](../story/silver_scrolls/the_sea.md) | undecided |
| Two of the narrators the info scripts use (the guide and the monk) aren't among the narrator numbers the script declares | Undetermined which voice the game gives them | The info script data | [info_scripts.md](../scripts/info_scripts.md) | undecided |
| Three of the developers' key layout names are spelt "Conifg" ("George Backer Conifg", "Tim's Conifg", "TBL's Conifg") | nothing visible: the table of names is never read | Strings in the executable | [hidden_keys_and_cheats.md](../easter_eggs/hidden_keys_and_cheats.md) | undecided |
| An internal error message is spelt "Wierd - this bitstring (11b) is reserved." | nothing visible: shown only on an internal error | Strings in the executable | [developer_jokes.md](../easter_eggs/developer_jokes.md) | undecided |
