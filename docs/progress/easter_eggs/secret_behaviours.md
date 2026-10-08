# Secret behaviours

Odd things the creature and the world do that the manual never mentions. Each row is something the program really
builds; how often the creature picks it depends on its desires, which are owned by
[../creature/desires.md](../creature/desires.md) and [../creature/decision_making.md](../creature/decision_making.md).
The playful villagers (football, the Mexican wave, gossip) are in
[../villager/play_and_gossip.md](../villager/play_and_gossip.md); knocking on houses is in
[../hand/clicking_and_activating.md](../hand/clicking_and_activating.md); the rival gods casting on their own creature
"for a laugh" is in [../rival_gods/magic.md](../rival_gods/magic.md).

**Progress: 0/17 done, 2 partial — 6%**

## The creature

| Behaviour | Status | Where / notes |
|-----------|--------|---------------|
| To get the player's attention it picks something up and throws it at the camera, at the point the player is looking from | todo | |
| Out of curiosity it looks at its reflection: it walks to the water's edge, stares down at its feet, makes a gesture, moves to a second spot and does it again, then goes back to the first | todo | |
| At play it pulls silly faces at someone: it walks up, faces them, two times in three makes a gesture, then pulls three random faces | partial | openblack plays a playful face only (`src/Creature/CreaturePlanActions.cpp`) |
| It has a "desire to get high", which it satisfies by picking something up and eating it; magic mushrooms are the things that count, and its web page keeps a count of mushrooms eaten | todo | what raises this desire is unconfirmed |
| To be with the player it walks into the middle of the screen, following the camera, and turns to face the player | todo | |
| It can look straight at the camera during cinema scenes, and point at the camera or at the hand | todo | |
| It can mimic the player, copying what the hand does | todo | (unconfirmed exactly what it copies) |
| With a "mental illness" desire it behaves strangely | todo | (unconfirmed what it does) |
| It looks about: at the moon, at the sun, out to sea, down a cliff, at the mountains, at its temple | partial | openblack looks about at the moon (`src/Creature/CreaturePlanActions.cpp`); the others are todo |
| It plays games: throwing stones at a can, racing a friend, running round a race track, throwing a die it owns | todo | the die is one of its toys, see [../story/rewards.md](../story/rewards.md) |
| Angry at another creature, it tells it to clear off | todo | |
| To make friends with another creature it asks it to hold still for four seconds and kisses its backside | todo | creature friendships are mostly a Creature Isle matter (unconfirmed how often this happens in Black & White) |
| It can swap minds with another creature | todo | (unconfirmed whether anything in Black & White leads to it) |
| In a football match it can celebrate or mourn a goal, and play as goalkeeper, defender or attacker | todo | it joins by acting on a villager who is playing, and its side comes from its player number: [../town/football.md](../town/football.md#the-player-and-the-creature) |

## The world

| Behaviour | Status | Where / notes |
|-----------|--------|---------------|
| Putting a music CD in the drive lets the game play it, and the advisors remark: "So you wanna hear your own music, huh?", "What's the matter? Our in-game music not good enough?", "Don't mind him. But watch out for your Creature.", "He'll always associate this music with what you do to him when it's playing.", "Creatures are sensitive to music, you see.", "Good gracious. Pumpin' choonz." | todo | the CD player is n/a in [../audio/music.md](../audio/music.md); whether the creature really links CD music to how it is treated is unconfirmed in the code |
| When a villager is made, there is a 2 in 10 chance it becomes a named villager, often a Lionhead developer | todo | owned by [../villager/special_villagers.md](../villager/special_villagers.md) |
| Fireflies that come out at night hide in trees and rocks by day, and lifting the right tree or rock gives a random miracle seed (the "seeds under trees" players remember) | todo | owned by [../nature/fireflies.md](../nature/fireflies.md) |
