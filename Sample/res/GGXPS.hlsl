//-----------------------------------------------------------------------------
// File : GGXPS.hlsl
// Desc : GGX Lighting Pixel Shader.
// Copyright(c) Pocol. All right reserved.
//-----------------------------------------------------------------------------

//-----------------------------------------------------------------------------
// Constant Values
//-----------------------------------------------------------------------------
static const float F_PI = 3.141596535f;

///////////////////////////////////////////////////////////////////////////////
// VSOutput structure
///////////////////////////////////////////////////////////////////////////////
struct VSOutput
{   
    float4  Position : SV_POSITION;     
    float2  TexCoord : TEXCOORD;        
    float3  Normal   : NORMAL;          
    float3x3  InvTangentBasis : INV_TANGENT_BASIS;
    float3  WorldPos : WORLD_POS;       
};

///////////////////////////////////////////////////////////////////////////////
// PSOutput structure
///////////////////////////////////////////////////////////////////////////////
struct PSOutput
{
    float4  Color : SV_TARGET0;     
};

///////////////////////////////////////////////////////////////////////////////
// LightBuffer 
///////////////////////////////////////////////////////////////////////////////
cbuffer LightBuffer : register( b1 )
{
    //float3 LightPosition  : packoffset( c0 );   
    //float3 LightColor     : packoffset( c1 );   
    //float3 CameraPosition : packoffset( c2 );   
    float3 LightPosition : packoffset(c0);
    float4 LightColor : packoffset(c1);
    float3 CameraPosition : packoffset(c2);
};

///////////////////////////////////////////////////////////////////////////////
// MaterialBuffer
///////////////////////////////////////////////////////////////////////////////
cbuffer MaterialBuffer : register( b2 )
{
    float3 BaseColor : packoffset( c0 );    
    float  Alpha     : packoffset( c1.w );  
    float  Roughness : packoffset( c1 );    
    float  Metallic  : packoffset( c1.y );  
};

//-----------------------------------------------------------------------------
// Textures and Samplers
//-----------------------------------------------------------------------------
SamplerState WrapSmp  : register( s0 );
Texture2D    BaseColorMap : register( t0 );
Texture2D    NormalMap : register( t1 );
Texture2D    RoughnessMap : register( t2 );
Texture2D    MetallicMap : register( t3 );

//-----------------------------------------------------------------------------
//      Schlick
//-----------------------------------------------------------------------------
float3 SchlickFresnel(float3 specular, float VH)
{
    return specular + (1.0f - specular) * pow((1.0f - VH), 5.0f);
}

//-----------------------------------------------------------------------------
//      GGX
//-----------------------------------------------------------------------------
float D_GGX(float m2, float NH)
{
    float f = (NH * m2 - NH) * NH + 1;
    return m2 / (F_PI * f * f);
}

//-----------------------------------------------------------------------------
//      Height Correlated Smith
//-----------------------------------------------------------------------------
float G2_Smith(float NL, float NV, float m2)
{
    
    float G_light = 2.0f * NL / (NL + sqrt(m2 + (1.0f + m2) * NL * NL));
    float G_view = 2.0f * NV / (NV + sqrt(m2 + (1.0f + m2) * NV * NV));
    return G_light * G_view;
}

//-----------------------------------------------------------------------------
//      main
//-----------------------------------------------------------------------------
PSOutput main(VSOutput input)
{
    PSOutput output = (PSOutput)0;
    float2 uv = float2(input.TexCoord.x, 1.0f - input.TexCoord.y);
    
    //float3 N = NormalMap.Sample(WrapSmp, uv /*input.TexCoord*/).xyz * 2.0f - 1.0f;
    //N = normalize(mul(input.InvTangentBasis, N));
    
    float3 N = normalize(input.Normal);
    float3 L = normalize(LightPosition  - input.WorldPos);
    float3 V = normalize(CameraPosition - input.WorldPos);
    float3 H = normalize(V + L);

    float NV = saturate(dot(N, V));
    float NH = saturate(dot(N, H));
    float NL = saturate(dot(N, L));
    float VH = saturate(dot(V, H));
    
    float4 basecolor = BaseColorMap.Sample(WrapSmp, uv);
    float roughness = RoughnessMap.Sample(WrapSmp, uv).r;
    float metallic = MetallicMap.Sample(WrapSmp, uv).r;
    float3 Kd = basecolor * (1.0f - metallic);
    
    float3 diffuse  = Kd * (1.0 / F_PI);

    float3 Ks = basecolor * metallic;
    float  a  = roughness * roughness;
    
    float  m2 = a * a;
    float  D  = D_GGX(m2, NH);
    float  G2 = G2_Smith(NL, NV, m2);
    float3 F0 = lerp(float3(0.04, 0.04, 0.04), basecolor.rgb, metallic);
    float3 Fr = SchlickFresnel(F0, VH);
    //float3 Fr = SchlickFresnel(Ks, VH);
    
    float3 specular = (D * G2 * Fr) / max(4.0f * NV * NL, 0.001f);
    
    // --- 距離減衰の計算 ---
    
    float d = distance(LightPosition, input.WorldPos); // 光源とピクセルの距離
    float range = 10000.0f; 
    float attenuation = saturate(1.0f - (d / range)); 
    attenuation *= attenuation; 
    
    float3 L_color = LightColor.rgb;
    float L_intensity = LightColor.a;
    
    output.Color = float4((diffuse + specular) * L_color * L_intensity * saturate(NL), basecolor.a * Alpha);

    return output;
}
