cbuffer constants : register(b0)
{
    row_major float4x4 World;
    row_major float4x4 WorldInverseTranspose;
}

cbuffer PerFrame : register(b1)
{
    row_major float4x4 View; // View Matrix Calculation of MVP Matrix
    row_major float4x4 Projection; // Projection Matrix Calculation of MVP Matrix
};

cbuffer DecalConstants : register(b2)
{
    row_major float4x4 DecalWorld;
    row_major float4x4 DecalWorldInverse;
    float SpotAngle;           // < 0: 박스 클리핑, >= 0: 원뿔 프러스텀
    float Padding1;
    float Padding2;
    float Padding3;
};

Texture2D DecalTexture : register(t0);
SamplerState DecalSampler : register(s0);

struct VS_INPUT
{
    float3 Position : POSITION;
    float3 Normal : NORMAL;
    float4 Color : COLOR;
    float2 Tex : TEXCOORD0;
};

struct PS_INPUT
{
    float4 Position : SV_POSITION;
    float4 WorldPos : TEXCOORD0;
    float4 Normal : TEXCOORD1;
    float2 Tex : TEXCOORD2;
};

PS_INPUT mainVS(VS_INPUT Input)
{
    PS_INPUT Output;

    float4 Pos = mul(float4(Input.Position, 1.0f), World);
    Output.Position = mul(mul(Pos, View), Projection);
    Output.WorldPos = Pos;
    Output.Normal = normalize(mul(float4(Input.Normal, 0.0f), WorldInverseTranspose));
    Output.Tex = Input.Tex;

    return Output;
}

float4 mainPS(PS_INPUT Input) : SV_TARGET
{
	// Normal Test
    float4 DecalForward = mul(float4(1.0f, 0.0f, 0.0f, 0.0f), DecalWorld);
    if (dot(DecalForward, Input.Normal) > 0.0f)
    {
		discard;
    }
	
	// Decal Local Transition
    float3 DecalLocalPos = mul(Input.WorldPos, DecalWorldInverse).xyz;

    // SpotAngle 기반 조건부 클리핑
    if (SpotAngle < 0.0f)
    {
        // 일반 박스 데칼: 박스 클리핑
        if (abs(DecalLocalPos.x) > 0.5f || abs(DecalLocalPos.y) > 0.5f || abs(DecalLocalPos.z) > 0.5f)
        {
            discard;
        }
    }
    else
    {
        // SemiLight 원뿔 프러스텀 클리핑
        // X 방향 (투사 방향) 체크: [0, 1] 범위
        if (DecalLocalPos.x < 0.0f || DecalLocalPos.x > 1.0f)
        {
            discard;
        }

        // 원뿔 프러스텀: 꼭짓점(0)에서 밑면(1)까지 반지름이 선형 증가
        float normalizedDepth = DecalLocalPos.x;
        float maxRadius = normalizedDepth * 0.5f;

        // 중심으로부터의 거리 계산 (YZ 평면)
        float distanceFromCenter = length(DecalLocalPos.yz);

        // 원뿔 프러스텀 밖이면 버림
        if (distanceFromCenter > maxRadius)
        {
            discard;
        }
    }

	// UV Transition ([-0.5~0.5], [-0.5~0.5]) -> ([0~1.0], [1.0~0])
    float2 DecalUV = DecalLocalPos.yz * float2(1, -1) + 0.5f;
    float4 DecalColor = DecalTexture.Sample(DecalSampler, DecalUV);
    if (DecalColor.a < 0.001f)
    {
        discard;
    }
	
    return DecalColor;
}
