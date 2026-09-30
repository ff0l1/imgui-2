#include "glass.hxx"
#include "imgui_internal.h"
#include <d3dcompiler.h>
#include <vector>

#pragma comment(lib, "d3dcompiler")

namespace egui {

static const char* k_blur_hlsl = R"(
Texture2D    tex : register(t0);
SamplerState smp : register(s0);

cbuffer Blur : register(b0) { float4 texel; };

struct VOut { float4 pos : SV_POSITION; float2 uv : TEXCOORD0; };

VOut VSFull(uint vid : SV_VertexID) {
    VOut o;
    o.uv  = float2(vid & 1, (vid >> 1) & 1);
    o.pos = float4(o.uv * float2(2.0, -2.0) + float2(-1.0, 1.0), 0.0, 1.0);
    return o;
}

float4 PSDown(VOut i) : SV_TARGET {
    float2 h = texel.xy;
    float4 s = tex.Sample(smp, i.uv) * 4.0;
    s += tex.Sample(smp, i.uv + float2(-h.x, -h.y));
    s += tex.Sample(smp, i.uv + float2( h.x, -h.y));
    s += tex.Sample(smp, i.uv + float2(-h.x,  h.y));
    s += tex.Sample(smp, i.uv + float2( h.x,  h.y));
    return s / 8.0;
}

float4 PSUp(VOut i) : SV_TARGET {
    float2 h = texel.xy;
    float4 s = tex.Sample(smp, i.uv + float2(-h.x * 2.0, 0.0));
    s += tex.Sample(smp, i.uv + float2(-h.x,  h.y)) * 2.0;
    s += tex.Sample(smp, i.uv + float2( 0.0,  h.y * 2.0));
    s += tex.Sample(smp, i.uv + float2( h.x,  h.y)) * 2.0;
    s += tex.Sample(smp, i.uv + float2( h.x * 2.0, 0.0));
    s += tex.Sample(smp, i.uv + float2( h.x, -h.y)) * 2.0;
    s += tex.Sample(smp, i.uv + float2( 0.0, -h.y * 2.0));
    s += tex.Sample(smp, i.uv + float2(-h.x, -h.y)) * 2.0;
    return s / 12.0;
}
)";

static const char* k_glass_hlsl = R"(
Texture2D    tex_sharp : register(t0);
Texture2D    tex_blur  : register(t1);
SamplerState smp       : register(s0);

cbuffer Pane : register(b0) {
    float4 rect;
    float4 screen;
    float4 style;
    float4 style2;
    float4 tint_col;
    float4 light;
    float4 clip;
};

struct VOut { float4 pos : SV_POSITION; float2 px : TEXCOORD0; };

VOut VSPane(uint vid : SV_VertexID) {
    float2 uv = float2(vid & 1, (vid >> 1) & 1);
    VOut o;
    o.px  = lerp(rect.xy, rect.zw, uv);
    o.pos = float4(o.px * screen.zw * float2(2.0, -2.0) + float2(-1.0, 1.0), 0.0, 1.0);
    return o;
}

float sd_box(float2 p, float2 b, float r) {
    float2 q = abs(p) - b + r;
    return min(max(q.x, q.y), 0.0) + length(max(q, 0.0)) - r;
}

float4 PSPane(VOut i) : SV_TARGET {
    float2 half_sz = (rect.zw - rect.xy) * 0.5;
    float2 p       = i.px - (rect.xy + rect.zw) * 0.5;

    float r = min(style.x, min(half_sz.x, half_sz.y));
    float d = sd_box(p, half_sz, r);

    float aa   = max(fwidth(d), 1e-4);
    float mask = saturate(0.5 - d / aa);
    if (mask <= 0.0) discard;

    float band  = max(style.y, 3.0 * aa);
    float bevel = saturate(1.0 + (d + aa) / band);
    float bend  = bevel * bevel;

    float rim = 1.0 - smoothstep(0.0, 1.6 * aa, -(d + aa));

    float nscale = max(r, 8.0 * aa);
    float2 k = (half_sz - abs(p)) / nscale;
    float2 n = normalize(sign(p) * exp2(-4.0 * k * k) + 1e-6);

    float2 sp = clamp(i.px - n * bend * style2.x, rect.xy + 1.0, rect.zw - 1.0);

    float2 uv  = i.px * screen.zw;
    float2 off = sp * screen.zw - uv;

    float3 blurred;
    blurred.r = tex_blur.Sample(smp, uv + off * (1.0 + style2.y)).r;
    blurred.g = tex_blur.Sample(smp, uv + off).g;
    blurred.b = tex_blur.Sample(smp, uv + off * (1.0 - style2.y)).b;

    float3 sharp = tex_sharp.Sample(smp, uv + off).rgb;
    float3 col   = lerp(lerp(sharp, blurred, style.z), tint_col.rgb, style.w);

    const float3 luma = float3(0.299, 0.587, 0.114);
    float need = saturate(dot(tint_col.rgb, luma) - dot(col, luma) - 0.25);
    col = lerp(col, tint_col.rgb, need);

    float lam = saturate(dot(n, normalize(light.xy + 1e-6)));
    col += pow(lam, 6.0) * bend * style2.w;
    col += rim * style2.z;

    float g = frac(sin(dot(i.px, float2(12.9898, 78.233))) * 43758.5453);
    col += (g - 0.5) * light.z;

    return float4(col, mask * tint_col.w);
}
)";

struct Level {
    ID3D11Texture2D*          tex = nullptr;
    ID3D11RenderTargetView*   rtv = nullptr;
    ID3D11ShaderResourceView* srv = nullptr;
    UINT w = 0, h = 0;
};

static const int k_levels = 3;

struct GlassGpu {
    ID3D11VertexShader* vs_full  = nullptr;
    ID3D11PixelShader*  ps_down  = nullptr;
    ID3D11PixelShader*  ps_up    = nullptr;
    ID3D11VertexShader* vs_pane  = nullptr;
    ID3D11PixelShader*  ps_pane  = nullptr;

    ID3D11Buffer*       cb_blur  = nullptr;
    ID3D11Buffer*       cb_pane  = nullptr;

    ID3D11SamplerState*      sampler = nullptr;
    ID3D11BlendState*        blend   = nullptr;
    ID3D11RasterizerState*   raster  = nullptr;
    ID3D11DepthStencilState* depth   = nullptr;

    ID3D11Texture2D*          scene     = nullptr;
    ID3D11ShaderResourceView* scene_srv = nullptr;
    Level level[k_levels];

    UINT w = 0, h = 0;
    bool shaders_ok = false;
    bool shaders_failed = false;
    bool captured = false;
};
static GlassGpu g;

struct CaptureSlot { const ImDrawList* dl; int layer; };
static CaptureSlot g_slots[8];
static int g_slots_frame = -1;
static int g_slots_used  = 0;

static void release_sized()
{
    if (g.scene_srv) { g.scene_srv->Release(); g.scene_srv = nullptr; }
    if (g.scene)     { g.scene->Release();     g.scene = nullptr; }
    for (int i = 0; i < k_levels; i++) {
        if (g.level[i].srv) { g.level[i].srv->Release(); g.level[i].srv = nullptr; }
        if (g.level[i].rtv) { g.level[i].rtv->Release(); g.level[i].rtv = nullptr; }
        if (g.level[i].tex) { g.level[i].tex->Release(); g.level[i].tex = nullptr; }
    }
    g.w = g.h = 0;
    g.captured = false;
}

void glass_invalidate() { release_sized(); }

void glass_shutdown()
{
    release_sized();
    if (g.vs_full) { g.vs_full->Release(); g.vs_full = nullptr; }
    if (g.ps_down) { g.ps_down->Release(); g.ps_down = nullptr; }
    if (g.ps_up)   { g.ps_up->Release();   g.ps_up = nullptr; }
    if (g.vs_pane) { g.vs_pane->Release(); g.vs_pane = nullptr; }
    if (g.ps_pane) { g.ps_pane->Release(); g.ps_pane = nullptr; }
    if (g.cb_blur) { g.cb_blur->Release(); g.cb_blur = nullptr; }
    if (g.cb_pane) { g.cb_pane->Release(); g.cb_pane = nullptr; }
    if (g.sampler) { g.sampler->Release(); g.sampler = nullptr; }
    if (g.blend)   { g.blend->Release();   g.blend = nullptr; }
    if (g.raster)  { g.raster->Release();  g.raster = nullptr; }
    if (g.depth)   { g.depth->Release();   g.depth = nullptr; }
    g.shaders_ok = false;
    g.shaders_failed = false;
    g_slots_frame = -1;
    g_slots_used  = 0;
}

static bool fail_shaders()
{
    glass_shutdown();
    g.shaders_failed = true;
    return false;
}

static ID3DBlob* compile(const char* src, const char* entry, const char* target)
{
    ID3DBlob* code = nullptr;
    ID3DBlob* err  = nullptr;
    const UINT flags = D3DCOMPILE_OPTIMIZATION_LEVEL3 | D3DCOMPILE_ENABLE_STRICTNESS;
    if (FAILED(D3DCompile(src, strlen(src), nullptr, nullptr, nullptr, entry, target, flags, 0, &code, &err))) {
        if (err) err->Release();
        return nullptr;
    }
    if (err) err->Release();
    return code;
}

static bool ensure_shaders()
{
    if (g.shaders_ok) return true;
    if (g.shaders_failed || !g_pd3dDevice) return false;

    struct { const char* src; const char* entry; const char* target; void** out; } jobs[] = {
        { k_blur_hlsl,  "VSFull", "vs_4_0", (void**)&g.vs_full },
        { k_blur_hlsl,  "PSDown", "ps_4_0", (void**)&g.ps_down },
        { k_blur_hlsl,  "PSUp",   "ps_4_0", (void**)&g.ps_up   },
        { k_glass_hlsl, "VSPane", "vs_4_0", (void**)&g.vs_pane },
        { k_glass_hlsl, "PSPane", "ps_4_0", (void**)&g.ps_pane },
    };

    for (int i = 0; i < IM_ARRAYSIZE(jobs); i++) {
        ID3DBlob* blob = compile(jobs[i].src, jobs[i].entry, jobs[i].target);
        if (!blob) { return fail_shaders(); }

        HRESULT hr;
        if (jobs[i].target[0] == 'v')
            hr = g_pd3dDevice->CreateVertexShader(blob->GetBufferPointer(), blob->GetBufferSize(),
                                                  nullptr, (ID3D11VertexShader**)jobs[i].out);
        else
            hr = g_pd3dDevice->CreatePixelShader(blob->GetBufferPointer(), blob->GetBufferSize(),
                                                 nullptr, (ID3D11PixelShader**)jobs[i].out);
        blob->Release();
        if (FAILED(hr)) { return fail_shaders(); }
    }

    D3D11_BUFFER_DESC bd = {};
    bd.Usage = D3D11_USAGE_DYNAMIC;
    bd.BindFlags = D3D11_BIND_CONSTANT_BUFFER;
    bd.CPUAccessFlags = D3D11_CPU_ACCESS_WRITE;
    bd.ByteWidth = sizeof(float) * 4;
    if (FAILED(g_pd3dDevice->CreateBuffer(&bd, nullptr, &g.cb_blur))) { return fail_shaders(); }
    bd.ByteWidth = sizeof(float) * 4 * 7;
    if (FAILED(g_pd3dDevice->CreateBuffer(&bd, nullptr, &g.cb_pane))) { return fail_shaders(); }

    D3D11_SAMPLER_DESC sd = {};
    sd.Filter = D3D11_FILTER_MIN_MAG_MIP_LINEAR;
    sd.AddressU = sd.AddressV = sd.AddressW = D3D11_TEXTURE_ADDRESS_CLAMP;
    sd.ComparisonFunc = D3D11_COMPARISON_ALWAYS;
    sd.MaxLOD = D3D11_FLOAT32_MAX;
    if (FAILED(g_pd3dDevice->CreateSamplerState(&sd, &g.sampler))) { return fail_shaders(); }

    D3D11_BLEND_DESC bl = {};
    bl.RenderTarget[0].BlendEnable = TRUE;
    bl.RenderTarget[0].SrcBlend = D3D11_BLEND_SRC_ALPHA;
    bl.RenderTarget[0].DestBlend = D3D11_BLEND_INV_SRC_ALPHA;
    bl.RenderTarget[0].BlendOp = D3D11_BLEND_OP_ADD;
    bl.RenderTarget[0].SrcBlendAlpha = D3D11_BLEND_ONE;
    bl.RenderTarget[0].DestBlendAlpha = D3D11_BLEND_INV_SRC_ALPHA;
    bl.RenderTarget[0].BlendOpAlpha = D3D11_BLEND_OP_ADD;
    bl.RenderTarget[0].RenderTargetWriteMask = D3D11_COLOR_WRITE_ENABLE_ALL;
    if (FAILED(g_pd3dDevice->CreateBlendState(&bl, &g.blend))) { return fail_shaders(); }

    D3D11_RASTERIZER_DESC rd = {};
    rd.FillMode = D3D11_FILL_SOLID;
    rd.CullMode = D3D11_CULL_NONE;
    rd.ScissorEnable = TRUE;
    rd.DepthClipEnable = TRUE;
    if (FAILED(g_pd3dDevice->CreateRasterizerState(&rd, &g.raster))) { return fail_shaders(); }

    D3D11_DEPTH_STENCIL_DESC dd = {};
    dd.DepthEnable = FALSE;
    dd.StencilEnable = FALSE;
    if (FAILED(g_pd3dDevice->CreateDepthStencilState(&dd, &g.depth))) { return fail_shaders(); }

    g.shaders_ok = true;
    return true;
}

static bool ensure_targets(UINT w, UINT h, DXGI_FORMAT fmt)
{
    if (g.scene && g.w == w && g.h == h) return true;
    release_sized();
    if (w == 0 || h == 0) return false;

    D3D11_TEXTURE2D_DESC td = {};
    td.Width = w; td.Height = h;
    td.MipLevels = 1; td.ArraySize = 1;
    td.Format = fmt;
    td.SampleDesc.Count = 1;
    td.Usage = D3D11_USAGE_DEFAULT;
    td.BindFlags = D3D11_BIND_SHADER_RESOURCE;
    if (FAILED(g_pd3dDevice->CreateTexture2D(&td, nullptr, &g.scene))) { release_sized(); return false; }
    if (FAILED(g_pd3dDevice->CreateShaderResourceView(g.scene, nullptr, &g.scene_srv))) { release_sized(); return false; }

    td.BindFlags = D3D11_BIND_SHADER_RESOURCE | D3D11_BIND_RENDER_TARGET;
    for (int i = 0; i < k_levels; i++) {
        const UINT div = (UINT)1 << (i + 1);
        Level& L = g.level[i];
        L.w = w / div > 0 ? w / div : 1;
        L.h = h / div > 0 ? h / div : 1;
        td.Width = L.w; td.Height = L.h;
        if (FAILED(g_pd3dDevice->CreateTexture2D(&td, nullptr, &L.tex))) { release_sized(); return false; }
        if (FAILED(g_pd3dDevice->CreateRenderTargetView(L.tex, nullptr, &L.rtv))) { release_sized(); return false; }
        if (FAILED(g_pd3dDevice->CreateShaderResourceView(L.tex, nullptr, &L.srv))) { release_sized(); return false; }
    }

    g.w = w; g.h = h;
    return true;
}

static void set_blur_texel(float x, float y)
{
    D3D11_MAPPED_SUBRESOURCE m;
    if (FAILED(g_pd3dDeviceContext->Map(g.cb_blur, 0, D3D11_MAP_WRITE_DISCARD, 0, &m))) return;
    float* p = (float*)m.pData;
    p[0] = x; p[1] = y; p[2] = 0.0f; p[3] = 0.0f;
    g_pd3dDeviceContext->Unmap(g.cb_blur, 0);
}

static void blur_pass(ID3D11ShaderResourceView* src, UINT src_w, UINT src_h,
                      const Level& dst, ID3D11PixelShader* ps)
{
    ID3D11DeviceContext* ctx = g_pd3dDeviceContext;

    ID3D11RenderTargetView* null_rtv = nullptr;
    ctx->OMSetRenderTargets(1, &null_rtv, nullptr);

    set_blur_texel(1.0f / (float)src_w, 1.0f / (float)src_h);

    D3D11_VIEWPORT vp = { 0.0f, 0.0f, (float)dst.w, (float)dst.h, 0.0f, 1.0f };
    const D3D11_RECT sc = { 0, 0, (LONG)dst.w, (LONG)dst.h };
    ctx->RSSetViewports(1, &vp);
    ctx->RSSetScissorRects(1, &sc);
    ctx->OMSetRenderTargets(1, (ID3D11RenderTargetView* const*)&dst.rtv, nullptr);

    ctx->PSSetShader(ps, nullptr, 0);
    ctx->PSSetShaderResources(0, 1, &src);
    ctx->PSSetConstantBuffers(0, 1, &g.cb_blur);
    ctx->Draw(4, 0);

    ID3D11ShaderResourceView* null_srv = nullptr;
    ctx->PSSetShaderResources(0, 1, &null_srv);
}

static void capture_cb(const ImDrawList*, const ImDrawCmd*)
{
    ID3D11DeviceContext* ctx = g_pd3dDeviceContext;
    if (!ctx || !g_pSwapChain) return;

    ID3D11Texture2D* bb = nullptr;
    if (FAILED(g_pSwapChain->GetBuffer(0, IID_PPV_ARGS(&bb))) || !bb) return;

    D3D11_TEXTURE2D_DESC bd;
    bb->GetDesc(&bd);
    if (!ensure_targets(bd.Width, bd.Height, bd.Format)) { bb->Release(); return; }

    ID3D11RenderTargetView* null_rtv = nullptr;
    ctx->OMSetRenderTargets(1, &null_rtv, nullptr);
    ctx->CopyResource(g.scene, bb);
    bb->Release();

    const float blend_factor[4] = { 0, 0, 0, 0 };
    ctx->IASetInputLayout(nullptr);
    ctx->IASetPrimitiveTopology(D3D11_PRIMITIVE_TOPOLOGY_TRIANGLESTRIP);
    ctx->VSSetShader(g.vs_full, nullptr, 0);
    ctx->GSSetShader(nullptr, nullptr, 0);
    ctx->HSSetShader(nullptr, nullptr, 0);
    ctx->DSSetShader(nullptr, nullptr, 0);
    ctx->CSSetShader(nullptr, nullptr, 0);
    ctx->PSSetSamplers(0, 1, &g.sampler);
    ctx->OMSetBlendState(nullptr, blend_factor, 0xffffffff);
    ctx->OMSetDepthStencilState(g.depth, 0);
    ctx->RSSetState(g.raster);

    blur_pass(g.scene_srv,     g.w,             g.h,             g.level[0], g.ps_down);
    blur_pass(g.level[0].srv,  g.level[0].w,    g.level[0].h,    g.level[1], g.ps_down);
    blur_pass(g.level[1].srv,  g.level[1].w,    g.level[1].h,    g.level[2], g.ps_down);
    blur_pass(g.level[2].srv,  g.level[2].w,    g.level[2].h,    g.level[1], g.ps_up);
    blur_pass(g.level[1].srv,  g.level[1].w,    g.level[1].h,    g.level[0], g.ps_up);

    ctx->OMSetRenderTargets(1, &null_rtv, nullptr);
    ctx->OMSetRenderTargets(1, &g_mainRenderTargetView, nullptr);

    g.captured = true;
}

static void schedule_capture(ImDrawList* dl, int layer)
{
    const int frame = ImGui::GetFrameCount();
    if (g_slots_frame != frame) { g_slots_frame = frame; g_slots_used = 0; }

    for (int i = 0; i < g_slots_used; i++)
        if (g_slots[i].dl == dl && g_slots[i].layer == layer) return;

    if (g_slots_used >= IM_ARRAYSIZE(g_slots)) return;
    g_slots[g_slots_used].dl = dl;
    g_slots[g_slots_used].layer = layer;
    g_slots_used++;

    dl->AddCallback(capture_cb, nullptr);
    dl->AddCallback(ImDrawCallback_ResetRenderState, nullptr);
}

void glass_capture()
{
    if (!glass.enabled || !g_pd3dDevice || !g_pSwapChain) return;
    if (!ensure_shaders()) return;
    schedule_capture(ImGui::GetBackgroundDrawList(), 0);
}

struct PaneData {
    float rect[4];
    float screen[4];
    float style[4];
    float style2[4];
    float tint[4];
    float light[4];
    float clip[4];
};
static std::vector<PaneData> g_panes;
static int g_panes_frame = -1;

static void pane_cb(const ImDrawList*, const ImDrawCmd* cmd)
{
    ID3D11DeviceContext* ctx = g_pd3dDeviceContext;
    if (!ctx || !g.captured) return;

    const size_t idx = (size_t)(uintptr_t)cmd->UserCallbackData;
    if (idx >= g_panes.size()) return;
    const PaneData& pd = g_panes[idx];

    D3D11_MAPPED_SUBRESOURCE m;
    if (FAILED(ctx->Map(g.cb_pane, 0, D3D11_MAP_WRITE_DISCARD, 0, &m))) return;
    memcpy(m.pData, &pd, sizeof(PaneData));
    ctx->Unmap(g.cb_pane, 0);

    const D3D11_RECT sc = { (LONG)pd.clip[0], (LONG)pd.clip[1],
                            (LONG)pd.clip[2], (LONG)pd.clip[3] };
    if (sc.right <= sc.left || sc.bottom <= sc.top) return;
    const D3D11_VIEWPORT vp = { 0.0f, 0.0f, pd.screen[0], pd.screen[1], 0.0f, 1.0f };
    const float blend_factor[4] = { 0, 0, 0, 0 };

    ID3D11ShaderResourceView* srvs[2] = { g.scene_srv, g.level[0].srv };

    ctx->IASetInputLayout(nullptr);
    ctx->IASetPrimitiveTopology(D3D11_PRIMITIVE_TOPOLOGY_TRIANGLESTRIP);
    ctx->RSSetState(g.raster);
    ctx->RSSetViewports(1, &vp);
    ctx->RSSetScissorRects(1, &sc);
    ctx->VSSetShader(g.vs_pane, nullptr, 0);
    ctx->VSSetConstantBuffers(0, 1, &g.cb_pane);
    ctx->GSSetShader(nullptr, nullptr, 0);
    ctx->HSSetShader(nullptr, nullptr, 0);
    ctx->DSSetShader(nullptr, nullptr, 0);
    ctx->CSSetShader(nullptr, nullptr, 0);
    ctx->PSSetShader(g.ps_pane, nullptr, 0);
    ctx->PSSetConstantBuffers(0, 1, &g.cb_pane);
    ctx->PSSetShaderResources(0, 2, srvs);
    ctx->PSSetSamplers(0, 1, &g.sampler);
    ctx->OMSetBlendState(g.blend, blend_factor, 0xffffffff);
    ctx->OMSetDepthStencilState(g.depth, 0);
    ctx->Draw(4, 0);

    ID3D11ShaderResourceView* null_srv[2] = { nullptr, nullptr };
    ctx->PSSetShaderResources(0, 2, null_srv);
}

bool glass_pane(ImDrawList* dl, ImVec2 p_min, ImVec2 p_max, float rounding,
                const ImVec4& tint_col, float alpha, bool inner)
{
    if (!glass.enabled || !dl || alpha <= 0.001f) return false;
    if (inner && !glass.cards) return false;
    if (p_max.x - p_min.x < 2.0f || p_max.y - p_min.y < 2.0f) return false;
    if (!g_pd3dDevice || !g_pSwapChain) return false;

    const ImVec2 c_min = dl->GetClipRectMin();
    const ImVec2 c_max = dl->GetClipRectMax();
    if (p_max.x <= c_min.x || p_min.x >= c_max.x ||
        p_max.y <= c_min.y || p_min.y >= c_max.y) return false;

    if (!ensure_shaders()) return false;

    if (inner)                                     schedule_capture(dl, 1);
    else if (dl == ImGui::GetForegroundDrawList()) schedule_capture(dl, 0);
    else                                           schedule_capture(ImGui::GetBackgroundDrawList(), 0);

    const int frame = ImGui::GetFrameCount();
    if (g_panes_frame != frame) { g_panes.clear(); g_panes_frame = frame; }

    const ImGuiViewport* vp = ImGui::GetMainViewport();
    const ImVec2 org = vp->Pos;
    const ImVec2 sz  = vp->Size;
    if (sz.x < 1.0f || sz.y < 1.0f) return false;

    PaneData pd;
    pd.clip[0] = ImMax(c_min.x - org.x, 0.0f);
    pd.clip[1] = ImMax(c_min.y - org.y, 0.0f);
    pd.clip[2] = ImMin(c_max.x - org.x, sz.x);
    pd.clip[3] = ImMin(c_max.y - org.y, sz.y);

    pd.rect[0] = p_min.x - org.x; pd.rect[1] = p_min.y - org.y;
    pd.rect[2] = p_max.x - org.x; pd.rect[3] = p_max.y - org.y;
    pd.screen[0] = sz.x; pd.screen[1] = sz.y;
    pd.screen[2] = 1.0f / sz.x; pd.screen[3] = 1.0f / sz.y;

    pd.style[0] = rounding;
    pd.style[1] = dpi(glass.thickness);
    pd.style[2] = ImClamp(glass.blur, 0.0f, 1.0f);

    const float lum = tint_col.x * 0.299f + tint_col.y * 0.587f + tint_col.z * 0.114f;
    pd.style[3] = ImClamp(glass.tint, 0.0f, 1.0f) * ImLerp(1.0f, glass.tint_light, lum * lum);

    pd.style2[0] = dpi(glass.refraction);
    pd.style2[1] = glass.chroma;
    pd.style2[2] = glass.sheen;
    pd.style2[3] = glass.specular;

    pd.tint[0] = tint_col.x; pd.tint[1] = tint_col.y; pd.tint[2] = tint_col.z;
    pd.tint[3] = ImClamp(alpha, 0.0f, 1.0f);

    pd.light[0] = glass.light_x; pd.light[1] = glass.light_y;
    pd.light[2] = glass.grain;   pd.light[3] = 0.0f;

    g_panes.push_back(pd);

    dl->AddCallback(pane_cb, (void*)(uintptr_t)(g_panes.size() - 1));
    dl->AddCallback(ImDrawCallback_ResetRenderState, nullptr);
    return true;
}

}
