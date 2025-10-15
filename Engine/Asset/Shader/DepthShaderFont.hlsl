// Constant buffers for font depth shader
cbuffer WorldMatrixBuffer : register(b0)
{
	row_major matrix WorldMatrix;
};

cbuffer ViewProjectionBuffer : register(b1)
{
	row_major float4x4 View;		// View Matrix Calculation of MVP Matrix
	row_major float4x4 Projection;	// Projection Matrix Calculation of MVP Matrix
};

cbuffer FontDataBuffer : register(b2)
{
	float2 AtlasSize;		// 512.0, 512.0
	float2 GlyphSize;		// 16.0, 16.0
	float2 GridSize;		// 32.0, 32.0
	float2 Padding;
};

// Input structures
struct VSInput
{
	float3 position : POSITION;		// FVector (3 floats)
	float2 texCoord : TEXCOORD0;	// FVector2 (2 floats)
	uint charIndex : TEXCOORD1;		// uint32 character index
};

struct PSInput
{
	float4 position : SV_POSITION;
	float depth : TEXCOORD0;		// Linear depth for visualization
	float2 texCoord : TEXCOORD1;	// For alpha testing
	uint charIndex : TEXCOORD2;
};

// Texture and Sampler for alpha testing
Texture2D FontAtlas : register(t0);
SamplerState FontSampler : register(s0);

// Vertex shader
PSInput mainVS(VSInput Input)
{
	PSInput Output;

	// Transform to world space
	float4 worldPos = mul(float4(Input.position, 1.0f), WorldMatrix);

	// View transformation
	float4 viewPos = mul(worldPos, View);

	// Store view-space depth before projection
	Output.depth = viewPos.z;

	// Projection transformation
	Output.position = mul(viewPos, Projection);

	// Calculate UV coordinates for alpha testing
	// ASCII code to 16x16 grid mapping
	uint col = Input.charIndex % 16;	// Column (0-15)
	uint row = Input.charIndex / 16;	// Row (0-15)
	float2 gridPos = float2(float(col), float(row));

	// 16x16 grid cell size calculation
	float2 cellSize = float2(1.0f / 16.0f, 1.0f / 16.0f);

	// Final UV coordinate calculation: grid position + cell offset
	float2 atlasUV = (gridPos * cellSize) + (Input.texCoord * cellSize);
	Output.texCoord = atlasUV;

	Output.charIndex = Input.charIndex;

	return Output;
}

// Pixel shader
float4 mainPS(PSInput Input) : SV_TARGET
{
	// Sample font texture for alpha testing
	float4 AtlasColor = FontAtlas.Sample(FontSampler, Input.texCoord);

	// Discard transparent pixels (text background should not contribute to depth)
	if (AtlasColor.r < 0.01f)
		discard;

	// Normalize depth to 0-1 range
	// Assuming typical far plane is around 1000-10000 units
	float normalizedDepth = (Input.depth % 100.0f) / 100.0f;

	// Create color gradient from near (dark blue) to far (white)
	float3 nearColor = float3(0.0f, 0.0f, 0.0f);	// Dark blue
	float3 farColor = float3(1.0f, 1.0f, 1.0f);		// White

	float3 depthColor = lerp(nearColor, farColor, normalizedDepth);

	return float4(depthColor, 1.0f);
}
