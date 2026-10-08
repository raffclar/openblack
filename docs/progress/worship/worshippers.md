# Worshippers and dancing

The villagers who leave their town to dance at the worship site. The player sets how many of a town worship; the
dancers chant prayer power, tire, get hungry and can die of it.

**Progress: 0/21 done, 1 partial — 2%**

## Going to worship

| Behaviour | Status | Where / notes |
|-----------|--------|---------------|
| Each town has a totem whose height sets the share of its people who worship; the player holds it with the hand and pulls it up or down | todo | |
| The totem eases smoothly to its new height, with a sound as it moves | todo | |
| A town's worshippers are drawn from its people who aren't busy, up to the share | todo | see `../villager/` |
| Worshippers walk from their town to the site, at the worship walking speed | todo | |
| A worshipper that can't reach the site doesn't go | todo | |
| Worshippers ask to go home when tired or hungry, and the site lets them go in turn | todo | |
| Worshippers go home to sleep, or sleep at the site (unconfirmed) | todo | |
| A villager dropped on the site by the hand becomes a worshipper disciple | todo | see `../villager/` for disciples |
| Scripts can make villagers dance and stop them | todo | |

## The dance

| Behaviour | Status | Where / notes |
|-----------|--------|---------------|
| Dancers move in group shapes and paths set out in the game's dance files | todo | the dance table loads with the info file (`src/InfoConstants.h`); nothing dances |
| Each dancer plays its tribe's dance animations | todo | |
| The dance goes faster the more prayer power is drawn from the site | partial | the intensity is worked out (`src/Magic/WorshipBattery.cpp`, test `test_worship_battery`) only in the debug sandbox |
| A dance takes a few turns to get going before it counts | todo | |
| Coloured lights follow the dancers | n/a | the game keeps them but never draws them; see [../rendering/light_beams.md](../rendering/light_beams.md) |
| The chanting is heard, louder with more worshippers | todo | |
| A strain sound plays when the miracles ask more than the dancers give | todo | the sound is listed in `src/Audio/Sound.h`, never played |

## What worship costs the dancers

| Behaviour | Status | Where / notes |
|-----------|--------|---------------|
| The more a dancer chants, the more life it loses | todo | |
| Dancers get hungry and eat the site's food | todo | |
| Worshippers starving or worn out die, and their town counts the deaths | todo | |
| Dancers rest at the altar to get their strength back | todo | |

## Other worship

| Behaviour | Status | Where / notes |
|-----------|--------|---------------|
| Villagers who are impressed enough by a creature worship it, dancing round it (unconfirmed exact trigger) | todo | |
| Villagers dance round an artefact in their town | todo | |

The camera that watches a dance is in `../camera/`.
