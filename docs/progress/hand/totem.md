# Totem

Each village has a totem pole by its village centre. The player takes hold of the totem with the hand and slides it up
or down to choose how many of the village's people worship rather than work. The hand locks onto the pole while the
button is held. Worship itself is in [../worship/](../worship/). Khazar teaches the totem on the second land, in
[Worship Site](../story/gold_scrolls/worship_site.md#khazars-lesson-on-worship).

**Progress: 0/26 done, 0 partial — 0%**

## Taking hold

| Behaviour | Status | Where / notes |
|-----------|--------|---------------|
| Holding the Action button on a totem locks the hand onto it until the button is let go | todo | openblack has no totem |
| Only a totem of the player's own village can be worked | todo | |
| Only a totem that is ready to be used, and where the player may act, can be worked | todo | (unconfirmed what the two further checks the game makes are) |
| Taking hold, the hand moves to the top of the pole, 0.7 of its size above the pole's base | todo | |
| The pointer is hidden and held at the hand's place on screen while the totem is held | todo | |
| The player's creature watches the player use the totem and may learn to do it | todo | see [../creature/](../creature/) |
| Villagers and animals nearby react to the hand using the totem | todo | see [../villager/](../villager/) |
| The totem's raise sound starts | todo | see [hand_sounds.md](hand_sounds.md) |

## Sliding

| Behaviour | Status | Where / notes |
|-----------|--------|---------------|
| Moving the mouse up or down slides the pole by three times its size for each screen height moved | todo | |
| Sliding down stops when the hand would go below the ground | todo | |
| The pole slides between its top and a drop of 1.3 times its height | todo | |
| The village's share of worshippers follows how far the pole is down | todo | see [../worship/](../worship/) |
| The hand follows the top of the pole as it slides | todo | |
| Near the ground (below 2.5 units) the hand tips over towards the ground; higher up it lies at seven sixteenths of a turn | todo | (unconfirmed how this looks in game) |
| The hand faces away from the camera, half a turn from its heading | todo | |
| The hand holds the pole with its side hold, posed by how thick the pole is for the hand's size | todo | |
| The pointer stays pinned to the hand's place on screen as the pole slides | todo | |

## Letting go

| Behaviour | Status | Where / notes |
|-----------|--------|---------------|
| Letting go leaves the pole where it is and frees the pointer | todo | |
| Where the pole was left is sent to the other players | todo | see [../multiplayer/](../multiplayer/) |
| The raise sound ends | todo | |

## Puzzle totems

| Behaviour | Status | Where / notes |
|-----------|--------|---------------|
| The challenge puzzles have their own totems, which can be tapped | todo | see [../story/](../story/) |
| A puzzle totem slides by the mouse's movement divided by 1.5, between its top and its lowest | todo | |
| A puzzle totem slides in whole steps, sending each new step to the other players | todo | |
| Scripts set a puzzle totem's highest point | todo | see [hand_in_scripts.md](hand_in_scripts.md) |

## Others working totems

| Behaviour | Status | Where / notes |
|-----------|--------|---------------|
| The creature raises and lowers totems itself | todo | see [../creature/](../creature/) |
| Computer gods raise totems in their towns | todo | see [other_players_hands.md](other_players_hands.md) |
