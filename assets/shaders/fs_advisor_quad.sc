$input v_texcoord0, v_color0

#include <bgfx_shader.sh>

SAMPLER2D(s_diffuse, 0);
SAMPLER2D(s_alpha, 1);
// x: the alpha a fragment needs to be drawn, of 255, below 0 for no test
uniform vec4 u_quadAlphaRef;

// The advisors' halo, smoke and rainbow trail: world triangles textured in their vertices' colour, as opaque as both
// the texture's alpha and the vertices'
void main()
{
	vec3 colour = texture2D(s_diffuse, v_texcoord0.xy).rgb * v_color0.rgb;
	float alpha = texture2D(s_alpha, v_texcoord0.xy).r * v_color0.a;
	if (u_quadAlphaRef.x >= 0.0f && alpha * 255.0f < u_quadAlphaRef.x)
	{
		discard;
	}
	gl_FragColor = vec4(colour, alpha);
}
