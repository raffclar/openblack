# Footpaths and roads

Footpaths are invisible routes the land's script lays between buildings and places. Villagers walk them to get about a
town and between towns, and they bend round obstacles that block them. The worn paths and roads the player sees are
painted into the land's texture and the buildings' footprints.

**Progress: 3/12 done, 1 partial — 29%**

## Laying footpaths

| Behaviour | Status | Where / notes |
|-----------|--------|---------------|
| The land's script makes footpaths and gives them their nodes in order | done | `FeatureScriptCommands::CreateFootpath`, `CreateFootpathNode`; `src/ECS/Components/Footpath.h` |
| The land's script links footpaths to the building they serve | todo | `FeatureScriptCommands::LinkFootpath` logs not implemented |
| A building keeps the footpaths that lead from it and picks the nearest one towards a destination | todo | |
| Footpaths are saved and loaded with the game | partial | read from the land's files (`src/Serializer/GameThingSerializer.cpp`); saving todo, see ../engine/ |
| Footpaths are drawn for debugging as lines | n/a | openblack-only (`RenderingSystemCommon::PrepareDraw`, Gui "Footpaths" checkbox) |

## Walking the footpaths

| Behaviour | Status | Where / notes |
|-----------|--------|---------------|
| A villager going somewhere joins the nearest footpath when it helps, walks it node to node, and leaves it at the nearest node to the destination | todo | `src/Debug/PathFinding.cpp` "Move On Footpath" is a stub; see ../villager/ |
| Footpaths can be walked either way | todo | |
| Each node keeps the villagers following it, so they move in file | todo | |
| When something blocks a footpath (a dropped rock, a building), the path is sent round it and returns when it is cleared | todo | |
| New footpaths are made from the routes a creature or villager plans, over a few game turns | todo | |
| Hidden nodes can be skipped as a short cut | todo | (unconfirmed what hides a node) |

## Roads seen on the land

| Behaviour | Status | Where / notes |
|-----------|--------|---------------|
| Worn paths and roads are part of the land's painted texture | done | `src/3D/BlockTexture.cpp` paints whatever the land's materials hold |
| Buildings and civic pieces print their paved ground onto the land | done | footprints, see land_marks.md |
