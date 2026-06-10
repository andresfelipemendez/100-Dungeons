#include "shaders.h"
#include <SDL3_shadercross/SDL_shadercross.h>

/* HLSL register spaces follow the SDL GPU convention:
   vertex   - uniform buffers in space1
   fragment - sampled textures/samplers in space2 */

static const char *VERTEX_HLSL =
    "cbuffer UBO : register(b0, space1)\n"
    "{\n"
    "    row_major float4x4 u_mvp;\n"
    "    row_major float4x4 u_model;\n"
    "};\n"
    "struct VSInput\n"
    "{\n"
    "    float3 position : TEXCOORD0;\n"
    "    float3 normal   : TEXCOORD1;\n"
    "    float2 uv       : TEXCOORD2;\n"
    "};\n"
    "struct VSOutput\n"
    "{\n"
    "    float4 position : SV_Position;\n"
    "    float3 normal   : TEXCOORD0;\n"
    "    float2 uv       : TEXCOORD1;\n"
    "};\n"
    "VSOutput main(VSInput input)\n"
    "{\n"
    "    VSOutput output;\n"
    "    output.position = mul(u_mvp, float4(input.position, 1.0f));\n"
    "    output.normal   = mul((float3x3)u_model, input.normal);\n"
    "    output.uv       = input.uv;\n"
    "    return output;\n"
    "}\n";

static const char *FRAGMENT_HLSL =
    "Texture2D<float4> u_tex : register(t0, space2);\n"
    "SamplerState u_smp      : register(s0, space2);\n"
    "struct PSInput\n"
    "{\n"
    "    float4 position : SV_Position;\n"
    "    float3 normal   : TEXCOORD0;\n"
    "    float2 uv       : TEXCOORD1;\n"
    "};\n"
    "float4 main(PSInput input) : SV_Target0\n"
    "{\n"
    "    float3 n = normalize(input.normal);\n"
    "    float3 light_dir = normalize(float3(0.4f, 0.9f, 0.5f));\n"
    "    float diffuse = max(dot(n, light_dir), 0.0f);\n"
    "    float ambient = 0.3f;\n"
    "    float4 tex = u_tex.Sample(u_smp, input.uv);\n"
    "    float3 rgb = tex.rgb * (ambient + 0.8f * diffuse);\n"
    "    return float4(rgb, 1.0f);\n"
    "}\n";

bool shaders_init(void) {
    if (!SDL_ShaderCross_Init()) {
        SDL_Log("SDL_ShaderCross_Init failed: %s", SDL_GetError());
        return false;
    }
    return true;
}

void shaders_quit(void) {
    SDL_ShaderCross_Quit();
}

SDL_GPUShader *shader_compile(SDL_GPUDevice *device, bool is_vertex) {
    SDL_ShaderCross_ShaderStage stage = is_vertex
        ? SDL_SHADERCROSS_SHADERSTAGE_VERTEX
        : SDL_SHADERCROSS_SHADERSTAGE_FRAGMENT;

    SDL_ShaderCross_HLSL_Info hlsl = {
        .source = is_vertex ? VERTEX_HLSL : FRAGMENT_HLSL,
        .entrypoint = "main",
        .include_dir = NULL,
        .defines = NULL,
        .shader_stage = stage,
        .props = 0,
    };

    size_t spirv_size = 0;
    void *spirv = SDL_ShaderCross_CompileSPIRVFromHLSL(&hlsl, &spirv_size);
    if (!spirv) {
        SDL_Log("HLSL -> SPIRV failed (%s): %s",
                is_vertex ? "vertex" : "fragment", SDL_GetError());
        return NULL;
    }

    SDL_ShaderCross_GraphicsShaderMetadata *meta =
        SDL_ShaderCross_ReflectGraphicsSPIRV((const Uint8 *)spirv, spirv_size, 0);
    if (!meta) {
        SDL_Log("SPIRV reflection failed: %s", SDL_GetError());
        SDL_free(spirv);
        return NULL;
    }

    SDL_ShaderCross_SPIRV_Info spirv_info = {
        .bytecode = (const Uint8 *)spirv,
        .bytecode_size = spirv_size,
        .entrypoint = "main",
        .shader_stage = stage,
        .props = 0,
    };
    SDL_GPUShader *shader = SDL_ShaderCross_CompileGraphicsShaderFromSPIRV(
        device, &spirv_info, &meta->resource_info, 0);

    SDL_free(meta);
    SDL_free(spirv);

    if (!shader) {
        SDL_Log("SPIRV -> GPU shader failed (%s): %s",
                is_vertex ? "vertex" : "fragment", SDL_GetError());
    }
    return shader;
}
