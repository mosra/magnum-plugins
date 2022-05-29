/*
    _x() -- skipped (not supported)
    _u() -- uncompressed format
    _s() -- uncompressed format needing a swizzle
*/
#ifdef _u
_x(UNKNOWN)
_u(R32G32B32A32_TYPELESS,   RGBA32UI)   /* in Magnum, UI == typeless */
_u(R32G32B32A32_FLOAT,      RGBA32F)
_u(R32G32B32A32_UINT,       RGBA32UI)
_u(R32G32B32A32_SINT,       RGBA32I)
_u(R32G32B32_TYPELESS,      RGB32UI)    /* in Magnum, UI == typeless */
_u(R32G32B32_FLOAT,         RGB32F)
_u(R32G32B32_UINT,          RGB32UI)
_u(R32G32B32_SINT,          RGB32I)
_u(R16G16B16A16_TYPELESS,   RGBA16UI)   /* in Magnum, UI == typeless */
_u(R16G16B16A16_FLOAT,      RGBA16F)
_u(R16G16B16A16_UNORM,      RGBA16Unorm)
_u(R16G16B16A16_UINT,       RGBA16UI)
_u(R16G16B16A16_SNORM,      RGBA16Snorm)
_u(R16G16B16A16_SINT,       RGBA16I)
_u(R32G32_TYPELESS,         RG32UI)     /* in Magnum, UI == typeless */
_u(R32G32_FLOAT,            RG32F)
_u(R32G32_UINT,             RG32UI)
_u(R32G32_SINT,             RG32I)
_x(R32G8X24_TYPELESS)
_x(D32_FLOAT_S8X24_UINT)
_x(R32_FLOAT_X8X24_TYPELESS)
_x(X32_TYPELESS_G8X24_UINT)
_x(R10G10B10A2_TYPELESS)    /* no generic packed formats in Magnum yet */
_x(R10G10B10A2_UNORM)
_x(R10G10B10A2_UINT)
_x(R11G11B10_FLOAT)
_u(R8G8B8A8_TYPELESS,       RGBA8UI)    /* in Magnum, UI == typeless */
_u(R8G8B8A8_UNORM,          RGBA8Unorm)
_u(R8G8B8A8_UNORM_SRGB,     RGBA8Unorm) /** @todo propagate sRGB property */
_u(R8G8B8A8_UINT,           RGBA8UI)
_u(R8G8B8A8_SNORM,          RGBA8Snorm)
_u(R8G8B8A8_SINT,           RGBA8I)
_u(R16G16_TYPELESS,         RG16UI)     /* in Magnum, UI == typeless */
_u(R16G16_FLOAT,            RG16F)
_u(R16G16_UNORM,            RG16Unorm)
_u(R16G16_UINT,             RG16UI)
_u(R16G16_SNORM,            RG16Snorm)
_u(R16G16_SINT,             RG16I)
_u(R32_TYPELESS,            R32UI)      /* in Magnum, UI == typeless */
_x(D32_FLOAT)
_u(R32_FLOAT,               R32F)
_u(R32_UINT,                R32UI)
_u(R32_SINT,                R32I)
_x(R24G8_TYPELESS)
_x(D24_UNORM_S8_UINT)
_x(R24_UNORM_X8_TYPELESS)
_x(X24_TYPELESS_G8_UINT)
_u(R8G8_TYPELESS,           RG8UI)      /* in Magnum, UI == typeless */
_u(R8G8_UNORM,              RG8Unorm)
_u(R8G8_UINT,               RG8UI)
_u(R8G8_SNORM,              RG8Snorm)
_u(R8G8_SINT,               RG8I)
_u(R16_TYPELESS,            R16UI)      /* in Magnum, UI == typeless */
_u(R16_FLOAT,               R16F)
_x(D16_UNORM)
_u(R16_UNORM,               R16Unorm)
_u(R16_UINT,                R16UI)
_u(R16_SNORM,               R16Snorm)
_u(R16_SINT,                R16I)
_u(R8_TYPELESS,             R8UI)       /* in Magnum, UI == typeless */
_u(R8_UNORM,                R8Unorm)
_u(R8_UINT,                 R8UI)
_u(R8_SNORM,                R8Snorm)
_u(R8_SINT,                 R8I)
_u(A8_UNORM,                R8Unorm)    /* only R as a single-channel format */
_x(R1_UNORM)                /* no single-bit formats in Magnum */
_x(R9G9B9E5_SHAREDEXP)      /* no generic packed formats in Magnum yet */
_x(R8G8_B8G8_UNORM)         /* no YUV formats in Magnum yet */
_x(G8R8_G8B8_UNORM)
_x(BC1_TYPELESS)
_x(BC1_UNORM)
_x(BC1_UNORM_SRGB)
_x(BC2_TYPELESS)
_x(BC2_UNORM)
_x(BC2_UNORM_SRGB)
_x(BC3_TYPELESS)
_x(BC3_UNORM)
_x(BC3_UNORM_SRGB)
_x(BC4_TYPELESS)
_x(BC4_UNORM)
_x(BC4_SNORM)
_x(BC5_TYPELESS)
_x(BC5_UNORM)
_x(BC5_SNORM)
_x(B5G6R5_UNORM)            /* no generic packed formats in Magnum yet */
_x(B5G5R5A1_UNORM)
_s(B8G8R8A8_UNORM,          RGBA8Unorm, true)
_s(B8G8R8X8_UNORM,          RGBA8Unorm, true) /* alpha unspecified */
_x(R10G10B10_XR_BIAS_A2_UNORM) /* no XR formats in Magnum yet */
_s(B8G8R8A8_TYPELESS,       RGBA8Unorm, true) /* typeless treated as Unorm */
_s(B8G8R8A8_UNORM_SRGB,     RGBA8Unorm, true) /** @todo propagate sRGB property */
_s(B8G8R8X8_TYPELESS,       RGBA8Unorm, true) /* typeless treated as Unorm, alpha unspecified */
_s(B8G8R8X8_UNORM_SRGB,     RGBA8Unorm, true) /* alpha unspecified */
                                              /** @todo propagate sRGB property */
_x(BC6H_TYPELESS)
_x(BC6H_UF16)
_x(BC6H_SF16)
_x(BC7_TYPELESS)
_x(BC7_UNORM)
_x(BC7_UNORM_SRGB)
_x(AYUV)                    /* no YUV formats in Magnum yet */
_x(Y410)
_x(Y416)
_x(NV12)
_x(P010)                    /* no (planar) YUV formats in Magnum yet */
_x(P016)
_x(420_OPAQUE)              /* no YUV formats in Magnum yet */
_x(YUY2)
_x(Y210)
_x(Y216)
_x(NV11)
_x(AI44)
_x(IA44)
_x(P8)                      /* no (planar) YUV formats in Magnum yet */
_x(A8P8)
_x(B4G4R4A4_UNORM)          /* no generic packed formats in Magnum yet */
/* DXGI_FORMAT_P208 is not linearly numbered, skipping */
/* DXGI_FORMAT_V208 is not linearly numbered, skipping */
/* DXGI_FORMAT_V408 is not linearly numbered, skipping */
/* DXGI_FORMAT_FORCE_UINT is just an "expander", skipping */
#endif
