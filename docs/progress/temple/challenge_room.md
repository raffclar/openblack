# Challenge room

The room of the challenges: a scroll listing every challenge found and done, and a picture of each, from which a
challenge can be replayed.

**Progress: 1/9 done, 2 partial — 22%**

## The room

| Behaviour | Status | Where / notes |
|-----------|--------|---------------|
| The room's camera comes in along its path and turns about the player | done | see ../camera/temple_camera.md |
| The scroll lists the challenges discovered and completed | partial | the scroll's facts are made up (`TempleScrolls::Facts::Mock`) until openblack keeps the game's statistics; the lands' scripts give no titles yet |
| Each challenge has a picture taken when it was recorded | todo | the snapshot command is a stub; see ../story/challenges_and_rewards.md |
| Pictures show low detail, and high detail when looked at | todo |  |
| Each challenge's picture shows its title and how well it went | todo |  |
| A challenge can be replayed from its picture | todo | TODO in `TempleToolTips.cpp` |
| The count of challenges and completed challenges | partial | the scroll's facts are made up (`TempleScrolls::Facts::Mock`) until openblack keeps the game's statistics |
| Tooltips for the pictures and the replay button | todo | TODO in `TempleToolTips.cpp` |
| The room's records are kept with the saved game | todo | see ../engine/saving_and_loading.md |
