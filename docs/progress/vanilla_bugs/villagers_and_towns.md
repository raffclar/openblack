# Vanilla bugs: villagers and towns

Bugs in the original game's villager and town code and data: daily work, carried tools and their sounds, and the town
football pitch.

**Bugs: 8 (soft-locks and lost progress: 0)**

## Wrong outcomes

| Bug | What the player sees | Evidence | Found in | openblack |
|-----|----------------------|----------|----------|-----------|
| The football score tally and the creature's "my side scored" test credit the side the villagers mourn for | The tally (seen only in the debug text) and the creature's reaction go to the wrong team, while the celebrations follow the villagers | Read from the pitch code | [football.md](../town/football.md) | undecided (the progress file says to reproduce it as-is) |
| The belief a town gains when a match ends reads a match count that nothing in the football code sets | Undetermined: the belief reward may be fixed at its minimum | Read from the pitch code | [football.md](../town/football.md) | undecided |
| With football switched off, a pitch placed by a script never runs a match, but relaxing villagers can still go to it and wait | Villagers standing at an idle pitch (undetermined what ends their wait) | The town only updates its pitch while football is on; nothing stops villagers joining | [football.md](../town/football.md) | undecided |

## Missing or wrong feedback

| Bug | What the player sees | Evidence | Found in | openblack |
|-----|----------------------|----------|----------|-----------|
| Both the hammering clip and the sledgehammer clip carry the sledgehammer sound | Hammering sounds like a sledgehammer; the separate hammer sound is never used by a villager | The clip sound events in `Data/SmallSounds.SAS` | [tools_and_carried_items.md](../villager/tools_and_carried_items.md) | undecided |
| The digging sounds belong to two farmer clips that the farming states never play | Sowing and digging up crops are silent | Clip sound events against the farming states' clips | [tools_and_carried_items.md](../villager/tools_and_carried_items.md) | undecided |
| When the football leaves the pitch, the game compares the two sides' first-listed players and sends the one further from the ball to fetch it (may be intended) | The player furthest away walks the long way to the ball while everyone else stands paused | Read from the pitch code | [football.md](../town/football.md) | undecided |
| A football spectator who leaves the match keeps their place in the crowd's wave until they are gone | A gap where the leaving spectator's turn in the wave comes round | Read from the pitch code | [football.md](../town/football.md) | undecided |

## Harmless

| Bug | What the player sees | Evidence | Found in | openblack |
|-----|----------------------|----------|----------|-----------|
| No tree record gives fruit-tree logs | nothing visible: the fruit branch is never seen in a villager's hand | The tree records in `info.dat` | [tools_and_carried_items.md](../villager/tools_and_carried_items.md) | undecided |
