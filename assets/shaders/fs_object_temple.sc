$input v_texcoord0, v_color0

#include <bgfx_shader.sh>

// The temple's rooms (src/Graphics/RendererTemple.cpp). The game draws the lit parts of the temple unlit, with the
// lightmap on the second texture stage: the texture times the lightmap, doubled, which saturates, and the texture's
// alpha. The rest are shaded by the game's one light (vs_object_temple_instanced).
SAMPLER2D(s_diffuse, 0);
#ifdef USE_LIGHTMAP
SAMPLER2D(s_lightmap, 3);
#endif // USE_LIGHTMAP
#ifdef USE_REFLECTION
SAMPLER2D(s_reflection, 4);
#endif // USE_REFLECTION
uniform vec4 u_skyAlphaThreshold; // y: ALPHAREF / 255 (< 0: no alpha test), w: the stage's alpha
                                  // (render_modes::PrimitiveAlpha): 0 none, else the texture's
uniform vec4 u_materialColour;    // rgb: the material's colour, w > 0: an untextured primitive
// rgb: the temple's light, which every colour is multiplied by
uniform vec4 u_templeLight;
// rgb: added after the texture stages, as the game adds the vertices' specular: the temple's light's colour added, or
// the glow of a control under the cursor in its place
uniform vec4 u_glow;
#ifndef USE_REFLECTION
// x: 1 = in its own colour and the temple's light alone, unshaded by the game's light (the pool's water, the map's
// markers), y: the opacity its alpha is multiplied by
uniform vec4 u_templeDraw;
#endif // USE_REFLECTION

void main()
{
	vec4 diffuseTex = texture2D(s_diffuse, v_texcoord0.xy);
	if (u_materialColour.w > 0.0f)
	{
		diffuseTex = vec4(u_materialColour.rgb, 1.0f);
	}
#ifdef USE_LIGHTMAP
	vec3 light = texture2D(s_lightmap, v_texcoord0.zw).rgb * 2.0f;
#elif defined(USE_REFLECTION)
	vec3 light = v_color0.rgb;
#else
	vec3 light = u_templeDraw.x > 0.5f ? vec3_splat(1.0f) : v_color0.rgb;
#endif // USE_LIGHTMAP
	vec3 colour = min(diffuseTex.rgb * u_templeLight.rgb * light, vec3_splat(1.0f));
	colour = min(colour + u_glow.rgb, vec3_splat(1.0f));
#ifdef USE_REFLECTION
	// The game draws the main room mirrored through its floor, then blends the floor over it by the floor's alpha.
	// The reflection pass is drawn from the mirrored camera with the same projection, so it lines up on the screen.
	vec3 reflection = texture2D(s_reflection, gl_FragCoord.xy * u_viewTexel.xy).rgb;
	gl_FragColor = vec4(mix(reflection, colour, diffuseTex.a), 1.0f);
#else
	// The alpha tested modes: ALPHAFUNC GREATEREQUAL against ALPHAREF, as fs_object
	float alphaThreshold = u_skyAlphaThreshold.y;
	if (alphaThreshold >= 0.0f && floor(diffuseTex.a * 255.0f + 0.5f) < floor(alphaThreshold * 255.0f + 0.5f))
	{
		discard;
	}
	gl_FragColor = vec4(colour, (u_skyAlphaThreshold.w > 0.5f ? diffuseTex.a : 1.0f) * u_templeDraw.y);
#endif // USE_REFLECTION
}
