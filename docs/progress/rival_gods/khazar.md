# Khazar

The good rival god of the second land: the player's ally, who brought them through the vortex, gives them builders and
scaffolds, comments on their deeds and is killed by Nemesis part-way through the land. He is the land's second player
and is run by the computer player's mind, steered by the land's challenge scripts.

**Progress: 2/46 done, 10 partial — 15%**

See [../story/land_2.md](../story/land_2.md) for the land's challenges, and [ai.md](ai.md), [magic.md](magic.md),
[towns_and_influence.md](towns_and_influence.md) and [creatures.md](creatures.md) for how every computer god works.

## On the map

| Behaviour | Status | Where / notes |
|-----------|--------|---------------|
| The land script makes the second player a computer player | partial | `TOGGLE_COMPUTER_PLAYER` makes a player entity (`FeatureScriptCommands::ToggleComputerPlayer`, `PlayerArchetype`), with no mind behind it |
| His temple stands on his own side of the land, owned by him | done | `CREATE_CITADEL` with its owner (`CitadelArchetype`) |
| He has a Norse worship site beside his temple | todo | `CREATE_WORSHIP_SITE` is ignored |
| He owns a Norse town with fire, nature, food and wood miracles | partial | the town and its owner are made (`TownArchetype`); `CREATE_NEW_TOWN_SPELL` is not implemented |
| He owns a Greek town with heal, teleport, physical shield and the itchy creature miracle | partial | as above |
| Each of his towns starts believing 0.65 in him | partial | `SET_TOWN_BELIEF` writes a map the belief rules don't read; see [../town/belief_and_conversion.md](../town/belief_and_conversion.md) |
| Belief in him and in Lethys is capped at 0.75 in every town of the land | todo | `SET_TOWN_BELIEF_CAP` is not implemented |
| His influence rings his temple and towns in his player colour | done | `InfluenceSystem`; see [../worship/influence.md](../worship/influence.md) |
| He is allied to the player at the full 100%, so the player can use his influence (unconfirmed what the alliance shares beyond that) | todo | `SET_PLAYER_ALLY` is a stub |
| His mind is told to expand his influence only a little (one fifth of full) | todo | personality stubs; see [ai.md](ai.md) |
| His attitude to Lethys is set to a quarter (kept doubled inside, as a half) | todo | `SET_COMPUTER_PLAYER_ATTITUDE` is a stub |

## His creature

| Behaviour | Status | Where / notes |
|-----------|--------|---------------|
| A tortoise loaded from his own mind file, set down near his temple | todo | `LOAD_CREATURE` is a stub; see [creatures.md](creatures.md) |
| It is named from the game's text (the script's note calls it Khalen) | todo | `SET_CREATURE_NAME` is a stub |
| It is made fully grown and drawn 1.2 times its auto-scaled size | todo | development and auto-scale script functions are stubs |
| It knows every everyday skill and almost every miracle, all body values set to 0.2 | todo | see [creatures.md](creatures.md) |
| Once the player has a creature, it takes on the same alignment and is made friends with it | todo | script waits 5 seconds at a time for the player's creature; `CREATURE_FORCE_FRIENDS` is a stub |
| While Nemesis kills Khazar, the script keeps his creature at full health until its own end | todo | |

## Arrival on the land

| Behaviour | Status | Where / notes |
|-----------|--------|---------------|
| His hand is put by his temple as the land opens, then flies to greet the player | todo | `SET_COMPUTER_PLAYER_POSITION`/`MOVE_COMPUTER_PLAYER_POSITION` are stubs |
| He introduces himself to his own music, then explains the threat from Lethys | partial | the music type is known (`MUSIC_TYPE_SCRIPT_KHAZAR`); the dialogue runs only if the hand can move |
| He places a town centre scaffold (size 5) from his workshop for the player, moving his hand at 40 then 150 | todo | |
| Later he gives a storage pit (size 3) and more scaffolds | todo | scaffolds: [../building/workshop_and_scaffolds.md](../building/workshop_and_scaffolds.md) |
| He picks up a new villager by his hand and drops it at the player's town, where it becomes a builder disciple | todo | forced "pick up and drop" action |
| He leaves food and wood one-shot miracles for the player | partial | the one-shot seeds can be made (see [../miracles/dispensers_and_seeds.md](../miracles/dispensers_and_seeds.md)); the script around them waits on his hand |
| He pours wood on the player's storage pit four times, his wood icon topped up with enough prayer power for six | todo | queued actions and `GAME_SET_MANA` are stubs |

## What he says

| Behaviour | Status | Where / notes |
|-----------|--------|---------------|
| To speak, his hand flies to 10 metres in front of the camera at speed 350, rechecking every 0.3 seconds | todo | |
| If it isn't there after 10 seconds it is put there at once | todo | |
| His lines are shown as dialogue text and he is released back to his mind after | todo | |
| He stays quiet while another script is using him | todo | the land's flag of him being in a script |
| Every 20 minutes (the first after 10) he urges the player to build, or once gestures are learnt to take over the land | todo | one of six or twelve lines at random |
| If the player attacked one of his two towns in the last 3 seconds he complains (one of eight lines), then waits 20 seconds; otherwise he checks again in 3 | partial | `GET_TIME_SINCE_OBJECT_ATTACKED` works; the rest waits on his hand |
| When his raw influence at the camera beats the player's he comes over, at most once a minute | todo | `GET_INFLUENCE` is a stub |
| Then half the time he says one of six "this is my land" lines, otherwise building or conquest advice | todo | |
| When the player wins one of Lethys's towns he praises them (one of four lines) | todo | see [towns_and_influence.md](towns_and_influence.md) |
| When the player wins one of his towns he complains (one of eight lines) | todo | |
| When someone else wins one of his towns he laments it (one of four lines) | todo | |
| When the player wins a neutral town he congratulates them (one of six lines) | todo | |
| He takes part in the land's shield and fireball challenges | todo | see [../story/land_2.md](../story/land_2.md) |

## His death

| Behaviour | Status | Where / notes |
|-----------|--------|---------------|
| The land checks every 9 seconds whether his death is due | partial | the land control script runs; town owners never change yet; the whole death scene: [../story/gold_scrolls/nemesis_no.md](../story/gold_scrolls/nemesis_no.md) |
| It is due once the player holds any of Lethys's three towns, or has more than five towns, or Khazar has none left, or Lethys has two or fewer | todo | `GET_PLAYER_TOWN_TOTAL` is a stub |
| It waits until Khazar isn't busy in another script | todo | |
| Nemesis's music starts and black storm clouds gather over the vortex and his temple | partial | the music and weather objects exist; see [nemesis.md](nemesis.md) |
| Volleys of fireballs fall on four places in his land, curling left, straight and right | partial | `SPELL_AT_POS` casts them, as the neutral player |
| His temple explodes | todo | |
| His creature is struck by an explosion from the cloud; Lethys's creature, made strong, comes to it, then both are taken away (Lethys's through the vortex) | todo | the vortex Nemesis opens at his death: [../story/portals_per_land.md](../story/portals_per_land.md#land-2-khazars-death-and-lethyss-vortex) |
| He is switched off and his alliance with the player ends | todo | |
| The land's final scroll is offered | todo | see [../story/land_2.md](../story/land_2.md) |
