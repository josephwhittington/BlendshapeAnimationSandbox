
struct OutputVertex
{
    float4 position : SV_Position;
    float3 normal : NORMAL;
    float2 tex : TEXCOORD;
};

float4 main(OutputVertex input) : SV_TARGET
{
    return float4(1, .1, .4, 1);
    return float4(input.normal * .8, 1);
}