# Library

The temple's library (the credits room): seven scrolls of the game's help, the history of what was said, and the
people who made the game.

**Progress: 3/11 done, 5 partial — 50%**

## The scrolls

| Behaviour | Status | Where / notes |
|-----------|--------|---------------|
| The credits: the people who made the game | done | `src/3D/TempleScrolls.cpp` (the library's first scroll) |
| The creature: what the player has been told about creatures | partial | the scroll's facts are made up (`TempleScrolls::Facts::Mock`) until openblack keeps the game's statistics |
| Miracles | partial | the scroll's facts are made up (`TempleScrolls::Facts::Mock`) until openblack keeps the game's statistics |
| Navigation: moving the hand and camera | partial | the scroll's facts are made up (`TempleScrolls::Facts::Mock`) until openblack keeps the game's statistics |
| Village life | partial | the scroll's facts are made up (`TempleScrolls::Facts::Mock`) until openblack keeps the game's statistics |
| Miscellaneous help | partial | the scroll's facts are made up (`TempleScrolls::Facts::Mock`) until openblack keeps the game's statistics |
| The history of the story so far: up to five entries of what was said | todo | TODO in `TempleScrolls.cpp` |
| Each "did you know" read in the world is added to its scroll, once | todo | needs the bronze scrolls (../interface/scrolls_and_signs.md) |
| What has been seen is kept with the saved game | todo |  |
| Signs over the seven scrolls | done | `src/3D/TempleSigns.cpp` |
| The room's camera turns about where its path ends | done | see ../camera/temple_camera.md |
