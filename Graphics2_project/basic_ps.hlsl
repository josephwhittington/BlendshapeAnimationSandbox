
struct OutputVertex
{
    float4 position : SV_POSITION;
    float4 color : COLOR;
};

float4 main(OutputVertex inputPixel) : SV_TARGET
{
    return inputPixel.color;
}