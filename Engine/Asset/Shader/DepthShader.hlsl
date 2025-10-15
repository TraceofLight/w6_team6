cbuffer constants : register(b0)
{
	row_major float4x4 world;
}

cbuffer PerFrame : register(b1)
{
	row_major float4x4 View;		// View Matrix Calculation of MVP Matrix
	row_major float4x4 Projection;	// Projection Matrix Calculation of MVP Matrix
};

struct VS_INPUT
{
	float3 position : POSITION;
	float3 normal : NORMAL;
	float4 color : COLOR;
	float2 tex : TEXCOORD0;
};

struct PS_INPUT
{
	float4 position : SV_POSITION;
	float depth : TEXCOORD0;		// Linear depth for visualization
};

PS_INPUT mainVS(VS_INPUT input)
{
	PS_INPUT output;

	float4 tmp = float4(input.position, 1.0f);
	tmp = mul(tmp, world);
	tmp = mul(tmp, View);

	// Store view-space depth before projection
	output.depth = tmp.z;

	tmp = mul(tmp, Projection);
	output.position = tmp;

	return output;
}

float4 mainPS(PS_INPUT input) : SV_TARGET
{
	// Normalize depth to 0-1 range
	// Assuming typical far plane is around 1000-10000 units
	float normalizedDepth = (input.depth % 100.0f) / 100.0f;

	// Create color gradient from near (dark blue) to far (white)
	float3 nearColor = float3(0.0f, 0.0f, 0.0f);	// Dark blue
	float3 farColor = float3(1.0f, 1.0f, 1.0f);		// White

	float3 depthColor = lerp(nearColor, farColor, normalizedDepth);

	return float4(depthColor, 1.0f);
}
