# Livestock

The animals that belong to towns: sheep, cows, horses, pigs and goats. They graze near their town, breed, are tended by
shepherds and breeders and are slaughtered for food.

**Progress: 1/16 done, 3 partial — 16%**

| Behaviour | Status | Where / notes |
|-----------|--------|---------------|
| The land scripts place livestock in flocks near towns | todo | animal and flock commands are stubs |
| An animal belongs to the town it is near when made, and to that town's player | todo | the owner is kept (`Animal::owner`), never set from a town |
| Livestock graze the land around their town, moving between grazing places | todo | |
| Livestock drink and sleep | todo | |
| A flock keeps together round its leader | todo | |
| Animals grow up from young, growing by age | partial | born at their age's size (`BirthScale`); they don't age |
| Animals breed when their need to breed is met, giving birth to young | todo | |
| A breeder disciple breeds a town's livestock | todo | see `../villager/` |
| Shepherds tend a town's flocks and take animals to be slaughtered for food | todo | see `../villager/` |
| A slaughtered animal becomes food for its town | todo | see `../resources/food.md` |
| Animals of another player's town count as theirs (unconfirmed what that changes) | todo | |
| The hand picks up livestock, gives them to a town or throws them | done | Picked up, thrown, and given to a storage pit as food (`HandGrabSystem`, `ResourceStoreSystem`) |
| The creature eats livestock, plays with them, or herds them (unconfirmed herding) | todo | see `../creature/` |
| Each kind plays its own clips: stand, move, eat, sleep, in hand, thrown, landed, dying, dead | partial | the dying and dead clips are known per kind (`src/Animals/AnimalRules.cpp`); nothing else plays them |
| Livestock are prey for wild hunters | partial | the miracles' wolves hunt any animal |
| Scripts make, move and read flocks | todo | the challenge script's flock functions are stubs (`src/CHLApi.cpp`) |
