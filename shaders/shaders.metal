using namespace metal;

vertex float4 quad_vertex(
	uint vertex_id [[vertex_id]]
) {
	float2 unit_vertex = float2(float(vertex_id & 1u), 0.5 * float(vertex_id & 2u));

	float2 pos = unit_vertex * float2(400, 400) + float2(10, 10);
	float2 device_pos = pos / float2(2560, 1440) * float2(2, -2) + float2(-1, 1);

	return float4(device_pos, 0, 1);
}

fragment float4 quad_fragment() {
	return float4(1, 0, 0, 1);
}
