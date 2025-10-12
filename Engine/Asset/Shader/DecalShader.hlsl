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
    float4 DecalFadeParams; // x = FadeAlpha[0..1]
    float4 SubUVParams;     // (Rows, Cols, CurrentFrame, TotalFrames)
    float SpotAngle;           // < 0: 박스 클리핑, >= 0: 원뿔 프러스텀
    float BlendFactor;         // 블렌딩 강도 (0.0 ~ 1.0)
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

float2 CalculateSubUVCoord(float2 BaseUV, float4 Params)
{
    float Rows = max(1.0f, Params.x);
    float Cols = max(1.0f, Params.y);
    int Frame = (int)Params.z;

    int RowIndex = Frame / (int)Cols;
    int ColIndex = Frame - RowIndex * (int)Cols;

    float2 CellSize = float2(1.0f / Cols, 1.0f / Rows);
    float2 CellUV = BaseUV * CellSize;
    float2 CellOffset = float2((float)ColIndex, (float)RowIndex) * CellSize;

    return CellUV + CellOffset;
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

    // 박스 중심 좌표계 [-0.5, 0.5]를 [0, 1] 범위로 변환
    DecalLocalPos += 0.5f;

    // 거리 기반 감쇠 (기본값 1.0)
    float attenuation = 1.0f;

    // SpotAngle 기반 조건부 클리핑
    if (SpotAngle < 0.0f)
    {
        // 일반 박스 데칼: 박스 클리핑 [0, 1] 범위
        if (any(DecalLocalPos < 0.0f) || any(DecalLocalPos > 1.0f))
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

        // 원뿔 프러스텀: 꼭짓점(0)에서 밑면(1)까지 반지름이 SpotAngle에 따라 증가
        float normalizedDepth = DecalLocalPos.x;

        // SpotAngle을 기반으로 반지름 계산
        // 정규화된 좌표 [0, 1]에서:
        // - normalizedDepth: 광원(0)에서 최대거리(1)까지
        // - maxRadius: 중심(0.5)에서 가장자리(0~1)까지의 거리
        float halfAngleRad = radians(SpotAngle * 0.5f);
        float maxRadius = normalizedDepth * tan(halfAngleRad);

        // 중심으로부터의 거리 계산 (YZ 평면, 중심은 0.5)
        float distanceFromCenter = length(DecalLocalPos.yz - 0.5f);

        // 원뿔 프러스텀 밖이면 버림
        if (distanceFromCenter > maxRadius)
        {
            discard;
        }

        // 반경 방향 감쇠 (중심에서 멀어질수록 어두워짐)
        float radialDistanceRatio = distanceFromCenter / max(maxRadius, 0.001f);
        float radialAttenuation = 1.0f - (radialDistanceRatio * radialDistanceRatio);

        // 깊이 방향 감쇠 (광원에서 멀어질수록 어두워짐)
        // normalizedDepth는 0(광원)에서 1(최대 거리)까지
        float depthAttenuation = 1.0f - normalizedDepth;

        // 최종 감쇠: 두 감쇠를 모두 적용
        attenuation = radialAttenuation * depthAttenuation;
    }

	// UV Transition ([0~1], [0~1]) -> ([0~1.0], [1.0~0])
    float2 BaseUV = float2(DecalLocalPos.y, 1.0f - DecalLocalPos.z);
    float2 DecalUV = CalculateSubUVCoord(BaseUV, SubUVParams);
    float4 DecalColor = DecalTexture.Sample(DecalSampler, DecalUV);

    // BlendFactor와 attenuation으로 알파 값 조정 & fade 적용
    DecalColor.a *= BlendFactor * attenuation * DecalFadeParams.x;

    if (DecalColor.a < 0.001f)
    {
        discard;
    }

    return DecalColor;
}
