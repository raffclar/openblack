# Disciples

A villager the player picks up and puts down on something becomes a disciple of that work and keeps at it for good:
on a field a farmer, in a forest a forester, at the sea a fisherman, at a building site a builder, by another villager a
breeder, in another town a missionary, at the workshop a craftsman, at the storage pit a trader, at the worship site a
worshipper.

**Progress: 0/21 done, 1 partial — 2%**

## Making disciples

| Behaviour | Status | Where / notes |
|-----------|--------|---------------|
| Dropping a villager from the hand onto a place makes it a disciple of the work there | todo | the hand doesn't pick up villagers yet |
| The disciple's work is chosen from what the drop point is (field, forest, sea, building site, workshop, storage pit, worship site, villager, other town) | todo | |
| Only an adult in its own town (or a missionary in another) can be made a disciple | todo | |
| A disciple shows its icon above its head when the hand is near | todo | |
| The town counts its disciples of each kind | partial | `src/ECS/Components/TownDesire.h` keeps the counts, nothing raises them |
| Making a disciple moves the player's alignment as the town tables say | todo | See ../worship/ |
| Disciples interacted with by the hand ask what state they would take up | todo | |
| Disciples with nothing to do look for their work, then wait | todo | |
| Picking a disciple up and dropping it back in town frees it | todo | |
| The town takes back its disciples when it changes owner | todo | |

## Each discipline

| Behaviour | Status | Where / notes |
|-----------|--------|---------------|
| Farmer disciple: farms fields endlessly | todo | |
| Forester disciple: fells trees for the store endlessly | todo | |
| Fisherman disciple: fishes endlessly | todo | |
| Builder disciple: builds and repairs sites | todo | |
| Breeder disciple: wanders the town making babies with willing partners, the villagers reacting to him | todo | |
| Missionary disciple: goes to another town and preaches, winning belief for the player | todo | |
| Craftsman disciple: keeps the workshop supplied with wood | todo | see [../building/workshop_and_scaffolds.md](../building/workshop_and_scaffolds.md) |
| Trader disciple: carries goods between towns | todo | |
| Worship disciple: worships at the site endlessly | todo | |
| Protection disciple | n/a | in the table but never made in the game |
| Change-house disciple: moves to the abode it is dropped by | todo | |
| Villagers that come out of an arrival vortex given a flock become disciples from the vortex, standing in a crowd, until the script disbands the flock | todo | only Land 2's arrival gives one: [../story/portals_per_land.md](../story/portals_per_land.md#land-2-arrival); how a vortex throws villagers out: [../story/portals.md](../story/portals.md#arriving) |
