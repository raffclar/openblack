# Villager play and social life

Villagers do more than work and sleep: they play football on the town's pitch when the town wants playtime, sit and
chill out in the evening, gossip, dance round the town's artefacts and celebrate.

**Progress: 1/20 done, 0 partial — 5%**

## Playtime and relaxation

| Behaviour | Status | Where / notes |
|-----------|--------|---------------|
| The town wants playtime once the game has run a while, and relaxation in the evening | done | `src/ECS/TownDesire.cpp` (worked out; test `test_town_desire.cpp`) |
| Villagers taking up relaxation sit and chill out outside their home or in town | todo | |
| Villagers kick a ball about when one lies near | todo | |
| Children play rather than work | todo | |

## Football

| Behaviour | Status | Where / notes |
|-----------|--------|---------------|
| Villagers walk to the football pitch when it is playtime and take positions in two teams | todo | the pitch is created as a building only (`src/ECS/Archetypes/AbodeArchetype.cpp`); the whole feature: [../town/football.md](../town/football.md) |
| Goalkeepers, defenders and attackers each play their part: dribble, pass, shoot, lob, clear, mark and save | todo | |
| The match waits for kick off, pauses, restarts after the ball goes out (dead ball) | todo | |
| A goal is celebrated by the scoring side and mourned by the other | todo | |
| Spectators watch the match and do a Mexican wave | todo | |
| A referee and the ball's own physics | todo | |
| The player or the creature can pick up the ball and throw it | todo | See ../hand/ |
| A pitch under construction draws villagers to build it | todo | |

## Social life

| Behaviour | Status | Where / notes |
|-----------|--------|---------------|
| Villagers stop to gossip with each other | todo | |
| Housewives gossip round the storage pit | todo | |
| Villagers tell others about something interesting they found, walking up to them | todo | |
| Villagers dance round the town's artefacts, which impresses the town | todo | See ../town/artefacts.md |
| Villagers congregate in town after an emergency | todo | See ../town/emergencies_and_aggression.md |
| Villagers celebrate when their town is won over | todo | |
| Villagers dance while reacting to something wonderful | todo | |
| Villagers sing and chant while worshipping | todo | See ../worship/ |
