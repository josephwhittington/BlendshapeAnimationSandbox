
// Use row major matrices
#pragma pack_matrix(row_major)

struct InputVertex
{
    float3 position : POSITION;
    float3 normal : NORMAL;
    float2 tex : TEXCOORD;
};

cbuffer SHADER_VARIABLES : register(b0)
{
    float4x4 worldMatrix;
    float4x4 viewMatrix;
    float4x4 projectionMatrix;
};

struct OutputVertex
{
    float4 position : SV_Position;
    float3 normal : NORMAL;
    float2 tex : TEXCOORD;
};

OutputVertex main(InputVertex input)
{
    OutputVertex output;
    
    output.position = mul(float4(input.position, 1), worldMatrix);
    output.position = mul(output.position, viewMatrix);
    output.position = mul(output.position, projectionMatrix);
    
    output.normal = input.normal;
    output.tex = input.tex;
    
	return output;
}