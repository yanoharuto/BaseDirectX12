struct VSInput
{
    float3 position : POSITION;
    float4 color : COLOR;
    float2 uv : TEXCOORD;
};

struct PSInput
{
    float4 position : SV_POSITION;
    float4 color : COLOR;
};

PSInput BasicVS(VSInput input)
{
    PSInput output;

    output.position = float4(input.position, 1.0f);
    output.color = input.color;

    return output;
}