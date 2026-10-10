$input v_texcoord0, v_texcoord1, v_lightColour, v_smallBumpFade, v_shadowCoord, v_haze, v_world

#include <bgfx_shader.sh>

#include "vortex_ground.sh"

// The colour of an open vortex's hole texture, added to what is drawn before the land on the block under its middle,
// wherever the land is drawn over it: the land's coast alpha lets it show where the land is under water
void main()
{
	vec4 hole = texture2D(s6_vortexHole, VortexGroundUv(v_texcoord1.zw));
	if (InVortexHole(hole))
	{
		discard;
	}
	gl_FragColor = vec4(VortexHoleColour(hole, v_lightColour, v_haze), hole.a);
}
