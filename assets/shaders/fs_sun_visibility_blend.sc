$input v_texcoord0, v_texcoord1

#include <bgfx_shader.sh>

// EXPERIMENT (not for merge): the old and the new maps of the sun's visibility blended, so that the hills' shadows
// move on smoothly when the map is made again

SAMPLER2D(s_visibilityOld, 0);
SAMPLER2D(s_visibilityNew, 1);

// x: how much of the new map shows, 0 to 1; yzw: unused
uniform vec4 u_sunBlend;

void main()
{
	vec2 uv = gl_FragCoord.xy * u_viewTexel.xy;
	gl_FragColor = mix(texture2DLod(s_visibilityOld, uv, 0.0f), texture2DLod(s_visibilityNew, uv, 0.0f), u_sunBlend.x);
}
