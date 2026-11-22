$input v_texcoord0

#include <bgfx_shader.sh>

// The larger pictures of the debug menu's sky pack, a layer for each alignment and, in each, for each time of day
SAMPLER2DARRAY(s_diffuse, 0);
// x: the alignment's first layer. y, z: the times of day blended. w: how much of the second, of 255
uniform vec4 u_skyDome;

// A row of the sky's dome for an alignment, blended for a time of day. The weights are those of the game's own blend,
// which add up to 255 of 256, but the blend does not cut the result to whole 5-bit steps.
void main()
{
	vec3 lower = texture2DArrayLod(s_diffuse, vec3(v_texcoord0.xy, u_skyDome.x + u_skyDome.y), 0.0f).rgb;
	vec3 upper = texture2DArrayLod(s_diffuse, vec3(v_texcoord0.xy, u_skyDome.x + u_skyDome.z), 0.0f).rgb;
	vec3 blended = lower * ((255.0f - u_skyDome.w) / 256.0f) + upper * (u_skyDome.w / 256.0f);
	gl_FragColor = vec4(blended, 1.0f);
}
