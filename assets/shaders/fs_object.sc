$input v_position, v_texcoord0, v_normal, v_color0

#include <bgfx_shader.sh>

// u_objectClip and SeaPlaneDiscard: the plane of the pass under the sea, src/Graphics/SeaPass.h
#include "sea_plane.sh"

SAMPLER2D(s_diffuse, 0);
uniform vec4 u_skyAlphaThreshold; // x, z: unused (0), y: ALPHAREF / 255 (< 0: no alpha test),
                                  // w: stage 0 alpha (render_modes::PrimitiveAlpha): 0 none, 1 texture, 2 texture x diffuse
uniform vec4 u_materialColour;    // rgb: L3D material colour, w > 0: untextured primitive (Smooth*)

// The original lights models on the CPU (D3DTLVERTEX): the vertex diffuse is computed in vs_object and
// the D3D stage is COLOROP = MODULATE(TEXTURE, DIFFUSE) with the specular colour added afterwards (SPECULARENABLE).
void main()
{
	float alphaThreshold = u_skyAlphaThreshold.y;
	float opacity = v_color0.a;
	float alphaSource = u_skyAlphaThreshold.w;

	// the side of the sea's plane this draw keeps (KeepAbove: reflections and cuts over the water, KeepBelow: the cuts
	// under it), on the real position
	if (SeaPlaneDiscard(v_position.y))
	{
		discard;
	}
	vec4 diffuseTex = texture2D(s_diffuse, v_texcoord0.xy);
	if (u_materialColour.w > 0.0f)
	{
		// untextured primitive: material colour x object colour
		diffuseTex = vec4(u_materialColour.rgb, 1.0f);
	}

	if (alphaThreshold >= 0.0f && floor(diffuseTex.a * (alphaSource > 1.5f ? opacity : 1.0f) * 255.0f + 0.5f) <
	                                       floor(alphaThreshold * 255.0f + 0.5f))
	{
		// the alpha tested modes: ALPHAFUNC GREATEREQUAL against ALPHAREF (render_modes::AlphaRef: the
		// material's, scaled by the object's alpha - 5 only for modes 9 / 15 with their own table), on the
		// stage 0 output: texture x diffuse alpha in 10, 11, 15, 16 (ALPHAOP MODULATE), the texture's in
		// 9 and 18 (SELECTARG1) (inferred: the alpha compared as a byte, rounded)
		discard;
	}
	// The opaque modes' textures may carry no meaningful alpha: they are opaque before fading. (aproximado) with 1 the
	// original's alpha is the texture's alone (SELECTARG1): the same while the object's alpha is 255
	diffuseTex.a = (alphaSource > 0.5f ? diffuseTex.a : 1.0f) * opacity;
	vec3 specular = vec3(v_texcoord0.zw, v_position.w); // see vs_object
	vec3 light = v_color0.rgb;
	diffuseTex.rgb = diffuseTex.rgb * light + specular;
	gl_FragColor = diffuseTex;
}
