# Spatially indexed map hit testing

`Game::MapProjection` uses a uniform spatial grid to narrow the regions tested
for a pixel hit. The approach is inspired by
[`StonePotatoes/src/ai/SpatialGrid.h`](https://github.com/ArchaonHW/StonePotatoes/blob/main/src/ai/SpatialGrid.h)
(MIT). This implementation is local to ChinaWarlords and uses projected
integer pixel coordinates; it does not add a StonePotatoes build dependency.

For projections with at least 32 regions, the projection places each region
center into one 32-pixel cell; smaller maps keep the lower-overhead linear
scan. An indexed hit query visits only cells overlapped by the query circle's
bounding box, then applies the exact squared-distance test. The nearest center
wins and equal-distance ties select the lowest region index. Very broad
queries fall back to the canonical linear scan so an oversized radius cannot
cause an unbounded cell walk.

This index belongs to `Game/Render`, where coordinates are presentation pixels.
It does not affect `Gameplay` state, AI visibility, battle ticks, replay, or
save data. The headless overworld test compares indexed results to a linear
reference scan and covers cell-boundary ties and broad queries.
