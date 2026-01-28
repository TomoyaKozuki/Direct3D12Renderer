//-----------------------------------------------------------------------------
// File : GGXVS.hlsl
// Desc : GGX Lighting Vertex Shader.
// Copyright(c) Pocol. All right reserved.
//-----------------------------------------------------------------------------

///////////////////////////////////////////////////////////////////////////////
// VSInput structure
///////////////////////////////////////////////////////////////////////////////
struct VSInput
{
    float3  Position : POSITION;    // �ʒu���W�ł�.
    float3  Normal   : NORMAL;      // �@���x�N�g���ł�.
    float2  TexCoord : TEXCOORD;    // �e�N�X�`�����W�ł�.
    float3  Tangent  : TANGENT;     // �ڐ��x�N�g���ł�.
};

///////////////////////////////////////////////////////////////////////////////
// VSOutput structure
///////////////////////////////////////////////////////////////////////////////
struct VSOutput
{
    float4  Position : SV_POSITION;     // �ʒu���W�ł�.
    float2  TexCoord : TEXCOORD;        // �e�N�X�`�����W�ł�.
    float3  Normal   : NORMAL;          // �@���x�N�g���ł�.
    float3x3    InvTangentBasis : INV_TANGENT_BASIS;
    float3  WorldPos : WORLD_POS;       // ���[���h��Ԃ̈ʒu���W�ł�.
};

///////////////////////////////////////////////////////////////////////////////
// Transform constant buffer
///////////////////////////////////////////////////////////////////////////////
cbuffer Transform : register( b0 )
{
    float4x4 World : packoffset( c0 ); // ���[���h�s��ł�.
    float4x4 View  : packoffset( c4 ); // �r���[�s��ł�.
    float4x4 Proj  : packoffset( c8 ); // �ˉe�s��ł�.
    //float4x4 InvView : packoffset( c12 );
    //float4x4 InvProj : packoffset( c16 );
}

//-----------------------------------------------------------------------------
//      main(TBN)
//-----------------------------------------------------------------------------
VSOutput main( VSInput input )
{
    VSOutput output = (VSOutput)0;

    float4 localPos = float4( input.Position, 1.0f );
    float4 worldPos = mul( World, localPos );
    float4 viewPos  = mul( View,  worldPos );
    float4 projPos  = mul( Proj,  viewPos );

    output.Position = projPos;
    output.TexCoord = input.TexCoord;
    output.WorldPos = worldPos.xyz;
    output.Normal   = normalize( mul((float3x3)World, input.Normal ) );
    
    float3 N = normalize(mul((float3x3)World, input.Normal));
    float3 T = normalize(mul((float3x3)World, input.Tangent));
    float3 B = normalize(cross(N, T));
    output.InvTangentBasis = transpose(float3x3(T, B, N));

    return output;
}