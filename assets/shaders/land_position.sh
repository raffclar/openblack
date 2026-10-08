#ifndef LAND_POSITION_SH
#define LAND_POSITION_SH

// The land block's vertex positions, shared by vs_terrain and vs_land_shadow: the shadows redraw the block over the very
// vertices it was drawn with (the block's transformed vertices) with Z LESSEQUAL, so
// both programs must compute the depth with the same expression.

// the block's corner + the vertex's local x, z; y as stored
vec3 LandWorldPosition(vec3 local, vec2 blockPosition)
{
	return vec3(local.x + blockPosition.x, local.y, local.z + blockPosition.y);
}

vec4 LandViewPosition(vec3 world)
{
	return mul(u_view, vec4(world, 1.0f));
}

vec4 LandClipPosition(vec4 view)
{
	return mul(u_proj, view);
}

#endif // LAND_POSITION_SH
