Texture2D DepthIn : register(t0);
SamplerState LinearClamp : register(s0);

struct VSOut
{
    float4 Position : SV_POSITION;
    float2 UV : TEXCOORD0;
};

VSOut mainVS(uint id : SV_VertexID)
{
    float2 pos;
    if (id == 0) pos = float2(-1.0, -1.0);
    else if (id == 1) pos = float2(-1.0,  3.0);
    else pos = float2( 3.0, -1.0);

    VSOut o;
    o.Position = float4(pos, 0.0, 1.0);
    o.UV = pos * 0.5f + 0.5f;
    return o;
}

float4 mainPS(VSOut i) : SV_TARGET
{
    float d = DepthIn.SampleLevel(LinearClamp, i.UV, 0).r;
    return float4(d, d, d, 1.0);
}

