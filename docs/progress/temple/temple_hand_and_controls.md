# Temple hand and controls

How the player acts inside the temple: the hand follows the cursor over the rooms' walls and floors, presses the rooms'
controls and scrolls, and the keys move between rooms.

**Progress: 8/11 done, 0 partial — 73%**

## The hand

| Behaviour | Status | Where / notes |
|-----------|--------|---------------|
| The hand is put where the cursor meets the room, turned to the surface | done | `TempleInterior::GetCursorHit` |
| The hand is drawn in the temple's light | done | `RenderingSystemTemple` |
| The hand holds a spell's seed or the creature's leash collar in the temple | todo | (unconfirmed what the hand can hold inside) |
| The tooltip is drawn by the hand | done | `TempleInterior::UpdateToolTips` |
| The hand's pose for pointing and pressing in the temple | todo | see ../hand/poses_and_states.md |

## Controls

| Behaviour | Status | Where / notes |
|-----------|--------|---------------|
| A press on a scroll or button takes the mouse until let go | done | `TempleInterior::HoldControl` |
| Escape goes back to the main room, and out from there | done | `TempleInterior::Escape` |
| Keys go straight to a room | done | see ../camera/temple_camera.md |
| The temple has its own game turn every 100 milliseconds | done | `TempleInterior.cpp` |
| Temple scripts run while the player is inside (temple help and specials) | todo | see ../engine/script_vm.md |
| The world room tab of the options, inside the temple | done | see ../interface/main_menu.md |
