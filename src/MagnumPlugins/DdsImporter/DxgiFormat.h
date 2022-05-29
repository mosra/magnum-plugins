/*
    _x() -- skipped (not supported)
    _i() -- invalid (missing enum value)
    _u() -- uncompressed format
    _s() -- uncompressed format needing a swizzle
    _c() -- compressed format
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
_u(R32G8X24_TYPELESS,       Depth32FStencil8UI) /* typeless treated as float/UI here */
_u(D32_FLOAT_S8X24_UINT,    Depth32FStencil8UI)
_u(R32_FLOAT_X8X24_TYPELESS,Depth32FStencil8UI) /* stencil unspecified, typeless treated as float here */
_u(X32_TYPELESS_G8X24_UINT, Depth32FStencil8UI) /* typeless treated as UI here */
_x(R10G10B10A2_TYPELESS)    /* no generic packed formats in Magnum yet */
_x(R10G10B10A2_UNORM)
_x(R10G10B10A2_UINT)
_x(R11G11B10_FLOAT)
_u(R8G8B8A8_TYPELESS,       RGBA8UI)    /* in Magnum, UI == typeless */
_u(R8G8B8A8_UNORM,          RGBA8Unorm)
_u(R8G8B8A8_UNORM_SRGB,     RGBA8Srgb)
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
_u(D32_FLOAT,               Depth32F)
_u(R32_FLOAT,               R32F)
_u(R32_UINT,                R32UI)
_u(R32_SINT,                R32I)
_u(R24G8_TYPELESS,          Depth24UnormStencil8UI) /* typeless treated as Unorm/UI here */
_u(D24_UNORM_S8_UINT,       Depth24UnormStencil8UI)
_u(R24_UNORM_X8_TYPELESS,   Depth24UnormStencil8UI) /* stencil unspecified */
_u(X24_TYPELESS_G8_UINT,    Depth24UnormStencil8UI) /* depth unspecified */
_u(R8G8_TYPELESS,           RG8UI)      /* in Magnum, UI == typeless */
_u(R8G8_UNORM,              RG8Unorm)
_u(R8G8_UINT,               RG8UI)
_u(R8G8_SNORM,              RG8Snorm)
_u(R8G8_SINT,               RG8I)
_u(R16_TYPELESS,            R16UI)      /* in Magnum, UI == typeless */
_u(R16_FLOAT,               R16F)
_u(D16_UNORM,               Depth16Unorm)
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
_c(BC1_TYPELESS,            Bc1RGBAUnorm) /* typeless treated as Unorm here */
_c(BC1_UNORM,               Bc1RGBAUnorm)
_c(BC1_UNORM_SRGB,          Bc1RGBASrgb)
_c(BC2_TYPELESS,            Bc2RGBAUnorm) /* typeless treated as Unorm here */
_c(BC2_UNORM,               Bc2RGBAUnorm)
_c(BC2_UNORM_SRGB,          Bc2RGBASrgb)
_c(BC3_TYPELESS,            Bc3RGBAUnorm) /* typeless treated as Unorm here */
_c(BC3_UNORM,               Bc3RGBAUnorm)
_c(BC3_UNORM_SRGB,          Bc3RGBASrgb)
_c(BC4_TYPELESS,            Bc4RUnorm)    /* typeless treated as Unorm here */
_c(BC4_UNORM,               Bc4RUnorm)
_c(BC4_SNORM,               Bc4RSnorm)
_c(BC5_TYPELESS,            Bc5RGUnorm)   /* typeless treated as Unorm here */
_c(BC5_UNORM,               Bc5RGUnorm)
_c(BC5_SNORM,               Bc5RGSnorm)
_x(B5G6R5_UNORM)            /* no generic packed formats in Magnum yet */
_x(B5G5R5A1_UNORM)
_s(B8G8R8A8_UNORM,          RGBA8Unorm, true)
_s(B8G8R8X8_UNORM,          RGBA8Unorm, true) /* alpha unspecified */
_x(R10G10B10_XR_BIAS_A2_UNORM) /* no XR formats in Magnum yet */
_s(B8G8R8A8_TYPELESS,       RGBA8Unorm, true) /* typeless treated as Unorm */
_s(B8G8R8A8_UNORM_SRGB,     RGBA8Srgb, true)
_s(B8G8R8X8_TYPELESS,       RGBA8Unorm, true) /* typeless treated as Unorm, alpha unspecified */
_s(B8G8R8X8_UNORM_SRGB,     RGBA8Srgb, true)  /* alpha unspecified */
_c(BC6H_TYPELESS,           Bc6hRGBUfloat) /* typeless treated as Ufloat here */
_c(BC6H_UF16,               Bc6hRGBUfloat)
_c(BC6H_SF16,               Bc6hRGBSfloat)
_c(BC7_TYPELESS,            Bc7RGBAUnorm) /* typeless treated as Unorm here */
_c(BC7_UNORM,               Bc7RGBAUnorm)
_c(BC7_UNORM_SRGB,          Bc7RGBASrgb)
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
_i()
_i()
_i()
_i()
_i()
_i()
_i()
_i()
_i()
_i()
_i()
_i()
_i()
_i()
_x(P208)
_x(V208)
_x(V408)

/* From https://github.com/g-truc/gli/commit/e5ad4ae6233abfb29eecebfd247142f1b3ef7844
   No floating-point variants listed there, those would probably be the missing
   values (136, 140, ...). Ignoring those until I know about a tool that
   exports them. The only tool known to be using these is NVidia Texture Tools
   Exporter, but it apparently uses only the _UNORM variant:
   https://forums.developer.nvidia.com/t/nv-tt-exporter-astc-compression/122477 */
_c(ASTC_4X4_TYPELESS,       Astc4x4RGBAUnorm) /* typeless treated as Unorm here */
_c(ASTC_4X4_UNORM,          Astc4x4RGBAUnorm)
_c(ASTC_4X4_UNORM_SRGB,     Astc4x4RGBASrgb)
_i()
_c(ASTC_5X4_TYPELESS,       Astc5x4RGBAUnorm) /* typeless treated as Unorm here */
_c(ASTC_5X4_UNORM,          Astc5x4RGBAUnorm)
_c(ASTC_5X4_UNORM_SRGB,     Astc5x4RGBASrgb)
_i()
_c(ASTC_5X5_TYPELESS,       Astc5x5RGBAUnorm) /* typeless treated as Unorm here */
_c(ASTC_5X5_UNORM,          Astc5x5RGBAUnorm)
_c(ASTC_5X5_UNORM_SRGB,     Astc5x5RGBASrgb)
_i()
_c(ASTC_6X5_TYPELESS,       Astc6x5RGBAUnorm) /* typeless treated as Unorm here */
_c(ASTC_6X5_UNORM,          Astc6x5RGBAUnorm)
_c(ASTC_6X5_UNORM_SRGB,     Astc6x5RGBASrgb)
_i()
_c(ASTC_6X6_TYPELESS,       Astc6x6RGBAUnorm) /* typeless treated as Unorm here */
_c(ASTC_6X6_UNORM,          Astc6x6RGBAUnorm)
_c(ASTC_6X6_UNORM_SRGB,     Astc6x6RGBASrgb)
_i()
_c(ASTC_8X5_TYPELESS,       Astc8x5RGBAUnorm) /* typeless treated as Unorm here */
_c(ASTC_8X5_UNORM,          Astc8x5RGBAUnorm)
_c(ASTC_8X5_UNORM_SRGB,     Astc8x5RGBASrgb)
_i()
_c(ASTC_8X6_TYPELESS,       Astc8x6RGBAUnorm) /* typeless treated as Unorm here */
_c(ASTC_8X6_UNORM,          Astc8x6RGBAUnorm)
_c(ASTC_8X6_UNORM_SRGB,     Astc8x6RGBASrgb)
_i()
_c(ASTC_8X8_TYPELESS,       Astc8x8RGBAUnorm) /* typeless treated as Unorm here */
_c(ASTC_8X8_UNORM,          Astc8x8RGBAUnorm)
_c(ASTC_8X8_UNORM_SRGB,     Astc8x8RGBASrgb)
_i()
_c(ASTC_10X5_TYPELESS,      Astc10x5RGBAUnorm) /* typeless treated as Unorm here */
_c(ASTC_10X5_UNORM,         Astc10x5RGBAUnorm)
_c(ASTC_10X5_UNORM_SRGB,    Astc10x5RGBASrgb)
_i()
_c(ASTC_10X6_TYPELESS,      Astc10x6RGBAUnorm) /* typeless treated as Unorm here */
_c(ASTC_10X6_UNORM,         Astc10x6RGBAUnorm)
_c(ASTC_10X6_UNORM_SRGB,    Astc10x6RGBASrgb)
_i()
_c(ASTC_10X8_TYPELESS,      Astc10x8RGBAUnorm) /* typeless treated as Unorm here */
_c(ASTC_10X8_UNORM,         Astc10x8RGBAUnorm)
_c(ASTC_10X8_UNORM_SRGB,    Astc10x8RGBASrgb)
_i()
_c(ASTC_10X10_TYPELESS,     Astc10x10RGBAUnorm) /* typeless treated as Unorm here */
_c(ASTC_10X10_UNORM,        Astc10x10RGBAUnorm)
_c(ASTC_10X10_UNORM_SRGB,   Astc10x10RGBASrgb)
_i()
_c(ASTC_12X10_TYPELESS,     Astc12x10RGBAUnorm) /* typeless treated as Unorm here */
_c(ASTC_12X10_UNORM,        Astc12x10RGBAUnorm)
_c(ASTC_12X10_UNORM_SRGB,   Astc12x10RGBASrgb)
_i()
_c(ASTC_12X12_TYPELESS,     Astc12x12RGBAUnorm) /* typeless treated as Unorm here */
_c(ASTC_12X12_UNORM,        Astc12x12RGBAUnorm)
_c(ASTC_12X12_UNORM_SRGB,   Astc12x12RGBASrgb)
_i()
/* DXGI_FORMAT_FORCE_UINT is just an "expander", skipping */
#endif
