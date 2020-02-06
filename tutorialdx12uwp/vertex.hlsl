#include "Header.hlsli"


VertexOut VS(VertexIn vin, uint instanceID : SV_InstanceID)
{
	VertexOut vout = (VertexOut)0.0f;
	InstanceData idata = gInstanceData[instanceID];
	vout.pos = mul(float4(vin.pos, 1.0f), idata.transform);
	vout.normal = mul(float4(vin.normal, 0.0f), idata.normaltransform);
	vout.color = vin.color;
	vout.uvcoord = vin.uvcoord;
	vout.mind = idata.matind;
	return vout;
}