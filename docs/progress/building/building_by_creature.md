# Building by the creature

A creature that has learnt to help a town can do its building work: carry scaffolds from the workshop to the town,
bring wood to building sites and the workshop, and build or repair with its own hands, copying what it has seen the
player do. It can also wreck buildings. This file covers the buildings' side; how the creature learns is in
../creature/.

**Progress: 0/13 done, 4 partial — 15%**

## Helping

| Behaviour | Status | Where / notes |
|-----------|--------|---------------|
| The creature picks up a scaffold and carries it to a town | todo | |
| It drops the scaffold where the town has room, making a building site | todo | |
| It brings wood to a building site or the storage pit | todo | |
| It brings wood to the workshop so it makes scaffolds | todo | |
| It builds a building site up with its hands, much faster than villagers | todo | |
| It repairs damaged buildings | todo | |
| It copies the player's adding of food and wood to stores and workshops | todo | |
| A town believes in the creature's player for its help | todo | See ../town/belief_and_conversion.md |
| It can steal scaffolds from other towns | todo | |

## Wrecking

| Behaviour | Status | Where / notes |
|-----------|--------|---------------|
| The creature kicks or stomps on buildings, damaging them | partial | the kick and stomp actions exist (`src/Creature/CreaturePlanActions.cpp`), see ../creature/; the buildings' damage from them is not checked |
| It throws objects at buildings | partial | See ../creature/ and damage_and_repair.md |
| It can burn buildings with a fireball or by setting things alight | partial | See ../creature/ |
| A town hurt by the creature fears it and wants protection | partial | aggression is kept (`src/ECS/TownAggression.cpp`); attitude to the creature isn't |
