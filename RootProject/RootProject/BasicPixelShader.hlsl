float4 BasicPS(float4 pos : SV_POSITION) : SV_TARGET
{
    return float4(pos.x / 1720.0f, pos.y / 900.0f, 1.0f, 1.0f);
}