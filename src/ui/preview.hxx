#pragma once
#include <cstdint>
#include <d3d11.h>
#include <string>
#include "imgui.h"
#include <vector>

namespace egui {

    struct Model3DRenderer {

        ID3D11Texture2D* rt_tex = nullptr;
        ID3D11RenderTargetView* rtv = nullptr;
        ID3D11ShaderResourceView* srv = nullptr;
        ID3D11Texture2D* ds_tex = nullptr;
        ID3D11DepthStencilView* dsv = nullptr;
        ID3D11VertexShader* vs = nullptr;
        ID3D11PixelShader* ps = nullptr;
        ID3D11InputLayout* layout = nullptr;
        ID3D11Buffer* vb = nullptr;
        ID3D11Buffer* ib = nullptr;
        ID3D11Buffer* cb = nullptr;
        ID3D11ShaderResourceView* diffuse_srv = nullptr;
        ID3D11SamplerState* sampler_state = nullptr;
        ID3D11RasterizerState* rs = nullptr;
        ID3D11DepthStencilState* dss = nullptr;

        int   rt_w = 0, rt_h = 0;
        float yaw = 0.0f;
        float yaw_target = 0.0f;
        float pitch = 0.0f;
        float distance = 3.8f;
        float thickness = 1.0f;
        float offset_x = 0.0f, offset_y = 0.0f;
        bool  init_done = false;
        int   index_count = 0;

        float orbit_speed = 0.010f;
        float pitch_limit = 1.2f;

        bool  auto_fit = true;
        float fill = 0.88f;
        float center_x = 0.0f, center_y = 0.0f, center_z = 0.0f;
        float half_h = 1.0f;
        float radius_xz = 1.0f;

        float bg[4] = { 0.078f, 0.078f, 0.078f, 0.55f };

        std::string diffuse_path;

        struct Vtx {
            float px, py, pz;
            float nx, ny, nz;
            float tu, tv;
        };
        struct Submesh {
            uint32_t index_start = 0;
            uint32_t index_count = 0;
            std::string tex_path;
            ID3D11ShaderResourceView* srv = nullptr;
        };
        std::vector<Vtx>      cpu_verts;
        std::vector<uint32_t> cpu_inds;
        std::vector<Submesh>  submeshes;
        bool mesh_ready = false;

        bool  load_obj(const char* path);
        bool  load_obj_from_memory(const void* data, size_t size, const char* base_dir = nullptr);
        bool  load_texture(ID3D11Device* dev, const char* path);

        bool  init(ID3D11Device* dev);
        bool  ensure_rt(ID3D11Device* dev, int w, int h);
        void  render(ID3D11Device* dev, ID3D11DeviceContext* ctx,
            int w, int h, float mdx, float mdy, bool dragging);
        void  shutdown();

        ImTextureID texture() const { return (ImTextureID)srv; }

    private:
        void build_procedural();
        void compute_bounds();
        bool upload_mesh(ID3D11Device* dev);
        bool load_fbx(const char* path, const char* texture_dir);
    };

    inline Model3DRenderer g_model3d;

}
