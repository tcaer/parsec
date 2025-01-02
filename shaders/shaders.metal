using namespace metal;

struct Globals {
	float2 viewport_size;
};

struct Quad {
	float2 origin;
	float2 size;
	float4 background_color;
};

struct QuadVertex {
	float4 position [[position]];
	float4 background_color;
};

vertex QuadVertex quad_vertex(
	uint vertex_id [[vertex_id]],
	uint quad_id [[instance_id]],
	constant Globals &globals [[buffer(0)]],
	constant Quad *quads [[buffer(1)]]
) {
	float2 unit_vertex = float2(float(vertex_id & 1u), 0.5 * float(vertex_id & 2u));
	Quad quad = quads[quad_id];

	float2 pos = unit_vertex * quad.size + quad.origin;
	float2 device_pos = pos / globals.viewport_size * float2(2, -2) + float2(-1, 1);

	return QuadVertex{
		float4(device_pos, 0, 1),
		quad.background_color
	};
}

fragment float4 quad_fragment(QuadVertex in [[stage_in]]) {
	return in.background_color;
}
