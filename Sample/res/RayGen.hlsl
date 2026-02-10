#include "Common.hlsl"

// Raytracing output texture, accessed as a UAV
RWTexture2D<float4> gOutput : register(u0);

// Raytracing acceleration structure, accessed as a SRV
RaytracingAccelerationStructure SceneBVH : register(t0);

// #DXR Extra: Perspective Camera
cbuffer CameraParams : register(b0)
{
    float4x4 world;
    float4x4 view;
    float4x4 projection;
    float4x4 viewI;
    float4x4 projectionI;
    float4 CameraPos;
}

[shader("raygeneration")]void RayGen()
{
  // Initialize the ray payload
    HitInfo payload;
    payload.colorAndDistance = float4(0, 0, 0, 0);

  // Get the location within the dispatched 2D grid of work items
  // (often maps to pixels, so this could represent a pixel coordinate).
    uint2 launchIndex = DispatchRaysIndex().xy;
    float2 dims = float2(DispatchRaysDimensions().xy);
    float2 d = (((launchIndex.xy + 0.5f) / dims.xy) * 2.f - 1.f);
  // Define a ray, consisting of origin, direction, and the min-max distance
  // values
  // #DXR Extra: Perspective Camera
    float aspectRatio = dims.x / dims.y;
  // Perspective
    RayDesc ray;
    ray.Origin = mul(viewI, CameraPos/*float4(0, 0, 0, 1)*/);
    //ray.Origin = CameraPos;
  
    float4 target = mul(projectionI, float4(d.x, -d.y, 1, 1));
    //ray.Direction = mul(viewI, float4(target.xyz, 0));
    ray.Direction = normalize(mul(viewI, float4(target.xyz, 0)).xyz);

    ray.TMin = 0.001f;
    ray.TMax = 100000.f;

  // Trace the ray
    TraceRay(
      SceneBVH,
      RAY_FLAG_NONE,
      0xFF,
      0,
      0,
      0,
      ray,
      payload);
    gOutput[launchIndex] = float4(payload.colorAndDistance.rgb, 1.f);
    

    //uint2 launchIndex = DispatchRaysIndex().xy;
    
    //// projectionI のどこか一箇所でも 0 以外（有効な値）が入っているか？
    //if (any(projectionI[0]) || any(projectionI[1]))
    //{
    //    gOutput[launchIndex] = float4(0, 1, 0, 1); // 届いていれば「緑」
    //}
    //else
    //{
    //    gOutput[launchIndex] = float4(1, 0, 0, 1); // 届いていなければ「赤」
    //}
    
}
