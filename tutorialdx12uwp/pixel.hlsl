#include "Header.hlsli"

float4 PS(VertexOut pin) : SV_Target
{
	float4 la = {1.0,1.0,1.0,1.0};
	float4	 ld = { 1.0,1.0,1.0,1.0 };
	float3 ldir = { 1.0,1.0,-1.5 };
	float cl = max(dot(ldir, pin.normal.xyz), 0);
	float4 color1 = textcolor.Sample(textsampler, pin.uvcoord)*pin.color;
	float4 newcolor = la * color1  + float4(cl * (ld.rgb * color1.rgb), ld.a * color1.a);
	return newcolor;
}