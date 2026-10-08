# Future room

The temple room kept for the game's future: in the released game it shows only "The future is still uncertain...".
Network play itself is reached from the front end (../multiplayer/).

**Progress: 2/4 done, 0 partial — 50%**

## The room

| Behaviour | Status | Where / notes |
|-----------|--------|---------------|
| The room's camera comes in along its path | done | see ../camera/temple_camera.md |
| "The future is still uncertain..." fades in to half over two seconds, each time the camera comes in | done | `TempleInterior::UpdateOptionsAndFutureRooms` |
| While the sessions are shown the room turns slowly by itself | todo | TODO in `TempleCameraModel.cpp` (unconfirmed when the game shows sessions here) |
| Whether the room is open to the player | todo | (unconfirmed what decides it) |
