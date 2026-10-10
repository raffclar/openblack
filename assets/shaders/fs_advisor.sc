$input v_position, v_texcoord0, v_color0, v_haze

#include <bgfx_shader.sh>

SAMPLER2D(s_diffuse, 0);
// x: 1 when the texture's alpha is multiplied by the advisor's, 0 when it is the texture's alone; y: the alpha a fragment
// needs to be drawn, of 255, below 0 for no test
uniform vec4 u_advisorMode;

// The advisors: their texture in their light, the land's colour added, the alpha as their material's mode takes it
void main()
{
	vec4 texel = texture2D(s_diffuse, v_texcoord0.xy);
	float alpha = u_advisorMode.x > 0.5f ? texel.a * v_color0.a : texel.a;
	if (u_advisorMode.y >= 0.0f && alpha * 255.0f < u_advisorMode.y)
	{
		discard;
	}
	gl_FragColor = vec4(min(texel.rgb * v_color0.rgb + v_haze.rgb, vec3_splat(1.0f)), alpha);
}
