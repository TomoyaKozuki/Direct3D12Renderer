#pragma once

enum class RENDER_TYPE
{
    RASTERIZE,
    RAYTRACE,
    //NORMAL,
    //BASECOLOR,
    //ROUGHNESS,
    //METALLIC,
    count,
};

// ‚±‚ê‚ÅˆêŠ‡”z—ñ‰»
static const char* RENDER_TYPE_NAMES[] = {
    "RASTERIZE",
    "RAYTRACE",
    //"NORMAL",
    //"BASECOLOR",
    //"ROUGHNESS",
    //"METALLIC"
};

class RenderTypeWrapper {
public:
    RenderTypeWrapper() :m_RenderType(RENDER_TYPE::RASTERIZE) {}
    ~RenderTypeWrapper() = default;

    RENDER_TYPE Get() const { return m_RenderType; }
    void Set(RENDER_TYPE type) {
        if (static_cast<int>(type) < static_cast<int>(RENDER_TYPE::count))
            m_RenderType = type;
    }
protected:
    RENDER_TYPE                 m_RenderType;
};