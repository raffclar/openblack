#ifndef SEA_PLANE_SH
#define SEA_PLANE_SH
// The pass under the sea, GPU side of src/Graphics/SeaPass.h (graphics::sea_pass; the two must stay the same).
// The user clip plane of the original's renderer (default (0, 1, 0, 0) from a static initialiser),
// the only plane any sea draw uses, read on the REAL world y.
// x: sea_pass::SeaPlane (1 KeepAbove, -1 KeepBelow, 0 None); y: 1 = unmirror (a draw cut by the plane, which never mirrors
// itself, inside openblack's mirrored Reflection pass); zw unused (sea_pass::PackClip)
uniform vec4 u_objectClip;

// Code 0x800: the underwater draw's "d > 0 out" on the mirrored point and the cut-by-plane draw's
// "d < 0 out" both become "keep the side the plane says" on the real y
// (sea_pass::Kept). (aproximado) strict per pixel: y = 0 is kept by both; the original clips triangles
bool SeaPlaneDiscard(float worldY)
{
	return (u_objectClip.x > 0.5f && worldY < 0.0f) || (u_objectClip.x < -0.5f && worldY > 0.0f);
}

// Back to the real position of a vertex drawn through the mirrored reflection camera (ReflectionXZCamera): the mirror
// of the underwater draw undone (sea_pass::Unmirror)
vec4 SeaUnmirror(vec4 worldPos)
{
	return u_objectClip.y > 0.5f ? vec4(worldPos.x, -worldPos.y, worldPos.zw) : worldPos;
}
#endif // SEA_PLANE_SH
