$input a_position, a_texcoord0, a_texcoord1
$output v_world

#include <bgfx_shader.sh>

// EXPERIMENT (not for merge): the sea bed past the land's blocks, out to the map's edge. Each corner takes its height
// from the shore map, as the corners of the land's blocks do, so the two have the same height where they meet. xz of
// a_position is where the corner is, y how far it hangs down (a skirt). The corner's height is on the line between
// two corners of the map (a_texcoord0 and a_texcoord1, the same for most corners): a corner on the side of a fine tile
// next to a coarse tile lies on the coarse tile's side, so the two tiles meet with no step.

// The shore map: at each cell corner of the whole map, w is the height of the ground
SAMPLER2D(s_shore, 2);

float SeabedCornerHeight(vec2 corner)
{
	return texture2DLod(s_shore, (corner + 0.5f) / 513.0f, 0.0f).w;
}

void main()
{
	vec2 along = a_texcoord1 - a_texcoord0;
	float span = dot(along, along);
	float share = span > 0.0f ? clamp(dot(a_position.xz / 10.0f - a_texcoord0, along) / span, 0.0f, 1.0f) : 0.0f;
	float height = mix(SeabedCornerHeight(a_texcoord0), SeabedCornerHeight(a_texcoord1), share);
	v_world = vec3(a_position.x, height - a_position.y, a_position.z);
	gl_Position = mul(u_viewProj, vec4(v_world, 1.0f));
}
