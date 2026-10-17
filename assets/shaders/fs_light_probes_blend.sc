$input v_texcoord0, v_texcoord1

#include <bgfx_shader.sh>

// EXPERIMENT (not for merge): the grid of light probes that the scene reads: the old grid blended into the new one

SAMPLER2D(s_probesOld, 0);
SAMPLER2D(s_probesNew, 1);

// x: how much of the new grid shows, 0 to 1; yz: the size of the grid's texture; w: unused
uniform vec4 u_lightBlend;

void main()
{
	vec2 uv = (floor(gl_FragCoord.xy) + 0.5f) / u_lightBlend.yz;
	gl_FragColor = mix(texture2DLod(s_probesOld, uv, 0.0f), texture2DLod(s_probesNew, uv, 0.0f), u_lightBlend.x);
}
