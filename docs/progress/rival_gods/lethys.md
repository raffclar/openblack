# Lethys

The evil rival god of the second and third lands. On the second land he is the third player, raining fireballs and
taunting the player while they win his towns; on the third he is the second player, holding the player's creature
prisoner on Nemesis's behalf until the player frees it and takes his last town, when his temple is destroyed.

**Progress: 3/50 done, 11 partial — 17%**

See [../story/land_2.md](../story/land_2.md) and [../story/land_3.md](../story/land_3.md) for the lands' challenges, and
[ai.md](ai.md), [magic.md](magic.md), [towns_and_influence.md](towns_and_influence.md) and
[creatures.md](creatures.md) for how every computer god works.

## On the second land

| Behaviour | Status | Where / notes |
|-----------|--------|---------------|
| The land script makes the third player a computer player | partial | `TOGGLE_COMPUTER_PLAYER` makes a player entity with no mind behind it |
| His temple and a Celtic worship site stand in the land's snowy corner, owned by him | partial | the temple is made with its owner (`CitadelArchetype`); worship sites are ignored |
| He owns three Celtic towns: one with lightning, one with heal, and his home town with food, shield, physical shield and water | partial | towns and owners made; town miracles not implemented |
| Each starts believing 0.65 in him, with belief in him and Khazar capped at 0.75 | todo | caps not implemented; see [../town/belief_and_conversion.md](../town/belief_and_conversion.md) |
| His influence rings his temple and towns in his player colour | done | `InfluenceSystem` |
| His mind is told to expand his influence only a little (one fifth of full) | todo | see [ai.md](ai.md) |
| His attitude to Khazar is set to a quarter | todo | |
| Lethys and the player are not allies | done | no alliance exists in openblack, which matches |

## His creature on the second land

| Behaviour | Status | Where / notes |
|-----------|--------|---------------|
| A wolf loaded from his mind file near his temple, drawn 1.5 times its auto-scaled size | todo | `LOAD_CREATURE` is a stub; see [creatures.md](creatures.md) |
| Named from the game's text (the script's note calls it Laetes), fully grown, every skill and nearly every miracle known | todo | |
| Once the player has a creature it takes the opposite alignment | todo | |
| When Khazar dies, his creature is made strong, goes to Khazar's creature and leaves through the vortex | todo | see [khazar.md](khazar.md); the vortices: [../story/portals_per_land.md](../story/portals_per_land.md#land-2-khazars-death-and-lethyss-vortex); [../story/gold_scrolls/nemesis_no.md](../story/gold_scrolls/nemesis_no.md#the-film-lethys-takes-the-creed) |

## Raids and taunts on the second land

| Behaviour | Status | Where / notes |
|-----------|--------|---------------|
| While the player arrives, his hand tours eight points of his land at speed 150, at a fixed height | todo | the hand can't be moved by scripts yet |
| Every 40 to 80 seconds (the first after 50) his hand flies to the first point and he fakes an attack | todo | |
| A fake attack is three level-two fireballs at one spot, 2, 5 and 2 seconds apart, thrown from his hand | partial | `SPELL_AT_POS` casts them, but from the land's origin (his hand's position is a stub) and as the neutral player |
| The tour stops and his mind takes over once the arrival ends | todo | |
| While he lives, whenever his influence at the camera beats the player's he comes to taunt them, at most once a minute | todo | `GET_INFLUENCE` is a stub |
| His hand flies to 10 metres in front of the camera at speed 350, rechecking every 0.3 seconds, and is put there after 10 seconds | todo | |
| His taunts are spoken in his own voice (one of six) rather than shown as text | todo | spoken lines come from the extra sound bank |
| He stays quiet while another script is using him | todo | |
| When the player wins one of his towns, Khazar praises it and 6 seconds later Lethys rages (one of four lines) | todo | see [towns_and_influence.md](towns_and_influence.md) |
| When he wins one of the player's towns he gloats (one of four lines) | todo | |

## Losing the second land

| Behaviour | Status | Where / notes |
|-----------|--------|---------------|
| Taking any one of his three towns can bring about Khazar's death | todo | see [khazar.md](khazar.md) |
| After Khazar's death, once two of his three towns are lost or he holds one or none, the vortex out of the land opens | todo | checked every 9 seconds; Lethys takes the player's creature through it: [../story/gold_scrolls/lethys_has_taken_our_creature.md](../story/gold_scrolls/lethys_has_taken_our_creature.md) |
| Storm clouds gather over his temple: five monsoon clouds, 80 seconds, still and out of the wind | partial | weather objects exist; see [../weather/](../weather/) |
| When he holds no town and his temple is at a tenth of its health or less, the player can leave through the vortex | todo | temple health isn't modelled |
| The final scroll counts how many of his towns remain | todo | see [../story/land_2.md](../story/land_2.md); [../story/gold_scrolls/destroy_it.md](../story/gold_scrolls/destroy_it.md) |

## On the third land

| Behaviour | Status | Where / notes |
|-----------|--------|---------------|
| He is the land's second player, a computer player | partial | player entity only |
| His temple stands on a plateau, with Egyptian and Tibetan worship sites | partial | temple made; worship sites ignored |
| His Tibetan town at the foot of the plateau has lightning, heal, food and storm, and believes 1.9 in him, capped at 2 | partial | town and owner made; belief cap and miracles not implemented |
| His Egyptian town has fire, teleport, shield and physical shield, and believes about 0.73 in him | partial | as above |
| He is not allied to the player | done | matches |
| His mind ignores aggressive creatures and doesn't try to expand at all | todo | both set to nothing |
| His wolf is loaded again on this land at 1.5 times size, opposite in alignment to the player's creature | todo | |

## The captive creature

| Behaviour | Status | Where / notes |
|-----------|--------|---------------|
| Until the three prison statues are switched off, his creature stays by the prisoner and never tires | todo | see [../story/land_3.md](../story/land_3.md); [../story/gold_scrolls/so_you_couldnt_bear_to_be_without_your_creature.md](../story/gold_scrolls/so_you_couldnt_bear_to_be_without_your_creature.md#the-prison) |
| If cattle are within 50 metres it stamps on one and eats it | todo | |
| Within 75 metres of the player's creature it torments it in turn: a powered fireball, a powered lightning bolt, then an itchy spell, each at full energy, 3 seconds apart | todo | |
| Further away it walks back to its post | todo | |
| A frozen moan is heard from the prisoner every tenth round | todo | |
| When freed, it plays confused, frightened and angry four times over and both creatures can be leashed | todo | |

## His raids on the third land

| Behaviour | Status | Where / notes |
|-----------|--------|---------------|
| Five minutes in, unless freed already, a pack of 20 wolves gathers and walks at half speed towards an Indian town | todo | see [../animal/](../animal/); [../story/gold_scrolls/the_wolves_are_possessed.md](../story/gold_scrolls/the_wolves_are_possessed.md) |
| If a wolf reaches within 25 metres of the town, it loses faith: belief in Lethys rises by 1 and in the player is set to 0.3 | todo | relative belief functions are stubs |
| If the monk's quest is done, the monk appears and turns wolves into cows until 8 are left; the player must kill the rest before any reaches the town | todo | [../story/gold_scrolls/the_wolves_are_possessed.md](../story/gold_scrolls/the_wolves_are_possessed.md#the-monk-helps) |
| His hand appears near a town, he boasts and throws three level-one fireballs at a beach campfire, setting sixteen fishermen alight | partial | the fireballs can be cast; the hand and dialogue wait on stubs; [../story/gold_scrolls/fire_fire_im_on_fire.md](../story/gold_scrolls/fire_fire_im_on_fire.md) |
| The player is scored by how many burning fishermen they save: all 16 gives alignment +1, 6 to 15 nothing, 5 or fewer −0.8 | todo | [../story/gold_scrolls/fire_fire_im_on_fire.md](../story/gold_scrolls/fire_fire_im_on_fire.md); see [../story/land_3.md](../story/land_3.md) |

## Losing the third land

| Behaviour | Status | Where / notes |
|-----------|--------|---------------|
| Until the creature is freed, the player's belief in his Tibetan town is pushed back by 1 every 10 seconds | todo | |
| After that, the town can be won once his belief there leads the player's by less than 0.4 | todo | |
| When the player wins it, his hand flies over his temple at speed 300 and the temple explodes, to Nemesis's music | todo | [../story/gold_scrolls/so_you_couldnt_bear_to_be_without_your_creature.md](../story/gold_scrolls/so_you_couldnt_bear_to_be_without_your_creature.md#sparing-or-finishing-lethys) |
| A level-one explosion falls on his temple from 150 metres above | partial | the explosion can be cast by script |
| He is switched off for good | todo | |
