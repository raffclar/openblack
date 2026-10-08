# Plants and flowers

The small growing things on the land that aren't trees: flower patches, bushes, hedges and crops' look on the land.

**Progress: 6/8 done, 1 partial — 81%**

| Behaviour | Status | Where / notes |
|-----------|--------|---------------|
| The land scripts place flower patches of each kind, turned and sized | done | `FlowersArchetype` |
| Flowers are drawn unlit as the game draws them | done | port notes (flowers unlit) |
| Flowers catch fire and burn | partial | `FireSystem.cpp` counts them among things that burn; unconfirmed in the game |
| Bushes, copses and hedges are kinds of tree, growing and swaying as trees do | done | see [trees](trees.md) |
| Fields' crops sway once ripe, further than trees | done | `VegetationSystem::GetFieldMatrix` |
| A field's crop shows green while young and turns its own colour as it ripens | done | test `test_field_crop` |
| Plants count as their own kind when a miracle's alignment is worked out | done | `AlignmentWeight` (plants) |
| The creature eats plants and stomps on flowers (unconfirmed) | todo | see `../creature/` |
