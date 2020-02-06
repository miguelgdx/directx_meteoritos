struct VertexIn {
	float3 pos : POSITION;
	float4 color : COLOR;
	float3 normal : NORMAL;
	float2 uvcoord : UV;
};

struct VertexOut {
	float4 pos : SV_POSITION;
	float4 color : COLOR;
	float4 normal : NORMAL;
	float2 uvcoord : UV;
	nointerpolation uint  mind : MATINDEX;
};

struct InstanceData
{
	float4x4 transform;
	float4x4 normaltransform;
	uint matind;
	uint pad0;
	uint pad1;
	uint pad2; // 
};

cbuffer cb : register(b0)
{
	float4x4 gPassTransform;
};

Texture2D textcolor : register(t0);
SamplerState textsampler : register(s0);

StructuredBuffer<InstanceData> gInstanceData : register(t0, space1);