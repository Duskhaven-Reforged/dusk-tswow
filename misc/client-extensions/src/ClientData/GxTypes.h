#ifndef CLIENT_DATA_GX_TYPES_H
#define CLIENT_DATA_GX_TYPES_H

namespace ClientData
{

enum EGxPrim {
    GxPrim_Points = 0,
    GxPrim_Lines = 1,
    GxPrim_LineStrip = 2,
    GxPrim_Triangles = 3,
    GxPrim_TriangleStrip = 4,
    GxPrim_TriangleFan = 5,
    GxPrims_Last = 6
};

enum EGxRenderState {
    GxRs_PolygonOffset = 0,
    GxRs_MatDiffuse = 1,
    GxRs_MatEmissive = 2,
    GxRs_MatSpecular = 3,
    GxRs_MatSpecularExp = 4,
    GxRs_NormalizeNormals = 5,
    GxRs_BlendingMode = 6,
    GxRs_AlphaRef = 7,
    GxRs_FogStart = 8,
    GxRs_FogEnd = 9,
    GxRs_FogColor = 10,
    GxRs_Lighting = 11,
    GxRs_Fog = 12,
    GxRs_DepthTest = 13,
    GxRs_DepthFunc = 14,
    GxRs_DepthWrite = 15,
    GxRs_ColorWrite = 16,
    GxRs_Culling = 17,
    GxRs_ClipPlaneMask = 18,
    GxRs_Multisample = 19,
    GxRs_ScissorTest = 20,
    GxRs_Texture0 = 21,
    GxRs_Texture1 = 22,
    GxRs_Texture2 = 23,
    GxRs_Texture3 = 24,
    GxRs_Texture4 = 25,
    GxRs_Texture5 = 26,
    GxRs_Texture6 = 27,
    GxRs_Texture7 = 28,
    GxRs_Texture8 = 29,
    GxRs_Texture9 = 30,
    GxRs_Texture10 = 31,
    GxRs_Texture11 = 32,
    GxRs_Texture12 = 33,
    GxRs_Texture13 = 34,
    GxRs_Texture14 = 35,
    GxRs_Texture15 = 36,
    GxRs_ColorOp0 = 37,
    GxRs_ColorOp1 = 38,
    GxRs_ColorOp2 = 39,
    GxRs_ColorOp3 = 40,
    GxRs_ColorOp4 = 41,
    GxRs_ColorOp5 = 42,
    GxRs_ColorOp6 = 43,
    GxRs_ColorOp7 = 44,
    GxRs_AlphaOp0 = 45,
    GxRs_AlphaOp1 = 46,
    GxRs_AlphaOp2 = 47,
    GxRs_AlphaOp3 = 48,
    GxRs_AlphaOp4 = 49,
    GxRs_AlphaOp5 = 50,
    GxRs_AlphaOp6 = 51,
    GxRs_AlphaOp7 = 52,
    GxRs_TexGen0 = 53,
    GxRs_TexGen1 = 54,
    GxRs_TexGen2 = 55,
    GxRs_TexGen3 = 56,
    GxRs_TexGen4 = 57,
    GxRs_TexGen5 = 58,
    GxRs_TexGen6 = 59,
    GxRs_TexGen7 = 60,
    GxRs_TextureShader0 = 61,
    GxRs_TextureShader1 = 62,
    GxRs_TextureShader2 = 63,
    GxRs_TextureShader3 = 64,
    GxRs_TextureShader4 = 65,
    GxRs_TextureShader5 = 66,
    GxRs_TextureShader6 = 67,
    GxRs_TextureShader7 = 68,
    GxRs_TextureCoord0 = 69,
    GxRs_TextureCoord1 = 70,
    GxRs_TextureCoord2 = 71,
    GxRs_TextureCoord3 = 72,
    GxRs_TextureCoord4 = 73,
    GxRs_TextureCoord5 = 74,
    GxRs_TextureCoord6 = 75,
    GxRs_TextureCoord7 = 76,
    GxRs_VertexShader = 77,
    GxRs_PixelShader = 78,
    GxRs_PointScale = 79,
    GxRs_PointScaleAttenuation = 80,
    GxRs_PointScaleMin = 81,
    GxRs_PointScaleMax = 82,
    GxRs_PointSprite = 83,
    GxRs_BlendFactor = 84,
    GxRs_ColorMaterial = 85,
    GxRenderStates_Last = 86
};

enum EGxXform {
    GxXform_Tex0 = 0,
    GxXform_Tex1 = 1,
    GxXform_Tex2 = 2,
    GxXform_Tex3 = 3,
    GxXform_Tex4 = 4,
    GxXform_Tex5 = 5,
    GxXform_Tex6 = 6,
    GxXform_Tex7 = 7,
    GxXform_World = 8,
    GxXform_Projection = 9,
    GxXform_View = 10,
    GxXforms_Last = 11
};

enum EGxApi
{
    GxApi_OpenGl = 0x0,
    GxApi_D3d9 = 0x1,
    GxApi_D3d9Ex = 0x2,
    GxApi_D3d10 = 0x3,
    GxApi_D3d11 = 0x4,
    GxApi_GLL = 0x5,
    GxApis_Last = 0x6,
};

enum CGxFormat__Format
{
    Fmt_Rgb565 = 0x0,
    Fmt_ArgbX888 = 0x1,
    Fmt_Argb8888 = 0x2,
    Fmt_Argb2101010 = 0x3,
    Fmt_Ds160 = 0x4,
    Fmt_Ds24X = 0x5,
    Fmt_Ds248 = 0x6,
    Fmt_Ds320 = 0x7,
    Formats_Last = 0x8,
};

enum EGxColorFormat
{
    GxCF_argb = 0x0,
    GxCF_rgba = 0x1,
    GxColorFormats_Last = 0x2,
};

} // namespace ClientData

#endif
