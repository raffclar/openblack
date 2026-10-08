# Save game room

The room where the game is saved and loaded: a scroll of the saves, a picture for each, and statistics of saving.

**Progress: 1/10 done, 3 partial — 25%**

## The room

| Behaviour | Status | Where / notes |
|-----------|--------|---------------|
| The room's camera comes in along its path | done | see ../camera/temple_camera.md |
| The scroll lists the saved games with their date and land | partial | the scroll's facts are made up (`TempleScrolls::Facts::Mock`) until openblack keeps the game's statistics |
| The time played since the game started | partial | counted from when the temple was made (`TempleInterior.cpp`) |
| How many times the game has been saved and loaded | partial | the scroll's facts are made up (`TempleScrolls::Facts::Mock`) until openblack keeps the game's statistics |
| Saving into a slot asks to confirm and takes a picture of the land | todo | see ../engine/saving_and_loading.md |
| Loading a slot asks to confirm and loads the game | todo | see ../engine/saving_and_loading.md |
| Deleting a save asks to confirm | todo |  |
| Each save's picture in low and high detail | todo |  |
| Quick save and quick load from the keys | todo | see ../engine/saving_and_loading.md |
| Scripts save the game into a slot | todo | stub in `src/CHLApi.cpp` |
