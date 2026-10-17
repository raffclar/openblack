#ifndef UNDERWATER_STREAMS_SH
#define UNDERWATER_STREAMS_SH

// EXPERIMENT (not for merge): the rivers' flow map (see underwater::streams). A shader that reads it includes this file
// and binds s_streamFlow at sampler stage 3.

// The flow map: xy the water's velocity across the ground in units a second, z the white water, w the distance to the
// nearest river's line in units
SAMPLER2D(s_streamFlow, 3);
// xy: the point at the low corner of the flow map's first texel; zw: 1 over the map's size in units; all 0 when the
// land has no rivers
uniform vec4 u_streamMap;

// Whether the land has rivers
bool UwStreamsShow()
{
	return u_streamMap.z > 0.0f;
}

// The flow map at a point. Outside the map: no flow and a far distance.
vec4 UwStreamFlowAt(vec2 xz)
{
	vec2 uv = (xz - u_streamMap.xy) * u_streamMap.zw;
	if (!UwStreamsShow() || uv.x <= 0.0f || uv.y <= 0.0f || uv.x >= 1.0f || uv.y >= 1.0f)
	{
		return vec4(0.0f, 0.0f, 0.0f, 16.0f);
	}
	return texture2DLod(s_streamFlow, uv, 0.0f);
}

// How much a point is in a river's water, 0 to 1, from the distance to the river's line: the original's channels are
// about two units wide
float UwStreamWaterShare(vec4 flow)
{
	return 1.0f - smoothstep(0.6f, 1.4f, flow.w);
}

#endif // UNDERWATER_STREAMS_SH
