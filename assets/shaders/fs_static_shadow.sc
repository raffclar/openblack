$input v_texcoord0

#include <bgfx_shader.sh>

SAMPLER2D(s_diffuse, 0);
uniform vec4 u_shadowParams; // x: ALPHAREF / 255 (< 0: no alpha test), y: 1 when the primitive has a texture

// Coverage of the static shadow; chroma materials (tree leaves) are alpha tested like the original's textured
// shadow path
void main()
{
	if (u_shadowParams.x >= 0.0f && u_shadowParams.y > 0.0f)
	{
		float alpha = texture2D(s_diffuse, v_texcoord0.xy).a;
		// ALPHAFUNC GREATEREQUAL against the material's ALPHAREF, without the - 5 of the modes' own table
		// (inferred: the shadow path uses the normal table)
		if (floor(alpha * 255.0f + 0.5f) < floor(u_shadowParams.x * 255.0f + 0.5f))
		{
			discard;
		}
	}
	gl_FragColor = vec4_splat(1.0f);
}
