
// Three things have to match
// 1. C++ Vertex Struct
// 2. Input Layout
// 3. HLSL Vertex Struct

// Use row major matrices
#pragma pack_matrix(row_major)

struct InputVertex
{
    float4 position : POSITION;
    float4 color : COLOR;
};

cbuffer SHADER_VARIABLES : register(b0)
{
    float4x4 worldMatrix;
    float4x4 viewMatrix;
    float4x4 projectionMatrix;
};

struct OutputVertex
{
    float4 position : SV_POSITION; // System value
    float4 color : COLOR;
};

OutputVertex main( InputVertex input )
{
    OutputVertex output = (OutputVertex) 0;
    output.position = input.position;
    output.color = input.color;
    
    // Do math here
    output.position = mul(output.position, worldMatrix);
    output.position = mul(output.position, viewMatrix);
    output.position = mul(output.position, projectionMatrix);
    
	return output;
}