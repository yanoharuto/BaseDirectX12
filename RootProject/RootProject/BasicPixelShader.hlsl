#include "BasicShaderHeader.hlsli"



float4 BasicPS(Output vertOutput) : SV_TARGET
{
    return float4(tex.Sample(smp, vertOutput.uv));
}