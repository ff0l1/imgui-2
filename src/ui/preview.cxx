#define NOMINMAX

#include <Windows.h>
#include "preview.hxx"
#include "egui.hxx"
#include "layout.hxx"
#include "assets/mesh.hxx"
#include "imgui.h"
#include "imgui_internal.h"
#include <d3dcompiler.h>
#pragma comment(lib, "d3dcompiler.lib")
#include <DirectXMath.h>
#include <vector>
#include <string>
#include <sstream>
#include <fstream>
#include <unordered_map>
#include <algorithm>
#include <cmath>
#include <cstring>
#include <cstdio>
#include <filesystem>
#include "ufbx.h"
namespace fs = std::filesystem;

using namespace DirectX;

namespace egui {

    static const char* k_shader = R"(
cbuffer CB : register(b0) {
    float4x4 gMVP;
    float4x4 gWorld;
    float3 gKeyLight;   float pad0;
    float3 gFillLight;  float pad1;
    float3 gCamPos;     float pad2;
};

Texture2D gDiffuse : register(t0);
SamplerState gSampler : register(s0);

struct VSIn { float3 pos : POSITION; float3 nor : NORMAL; float2 uv : TEXCOORD; };
struct PSIn { float4 pos : SV_POSITION; float3 nor : TEXCOORD0; float3 wp : TEXCOORD1; float2 uv : TEXCOORD2; };

PSIn VS(VSIn v) {
    PSIn o;
    o.pos = mul(float4(v.pos, 1), gMVP);
    o.nor = normalize(mul(float4(v.nor, 0), gWorld).xyz);
    o.wp  = mul(float4(v.pos, 1), gWorld).xyz;
    o.uv  = v.uv;
    return o;
}

float4 PS(PSIn p) : SV_Target {
    float3 N = normalize(p.nor);
    float3 V = normalize(gCamPos - p.wp);

    float  key  = saturate(dot(N, normalize(gKeyLight)));
    float3 kcol = float3(0.92, 0.90, 0.86) * (key * 0.80 + 0.35);

    float  fill  = saturate(dot(N, normalize(gFillLight)));
    float3 fcol  = float3(0.45, 0.50, 0.60) * fill * 0.30;

    float  rim   = pow(1.0 - saturate(dot(N, V)), 3.2) * 0.25;
    float3 rcol  = float3(0.65, 0.70, 0.90) * rim;

    float3 H    = normalize(normalize(gKeyLight) + V);
    float  spec = pow(saturate(dot(N, H)), 22.0) * 0.08;

    float3 lit  = (kcol + fcol) + rcol + spec;

    float4 tex = gDiffuse.Sample(gSampler, p.uv);
    return float4(tex.rgb * lit, 1.0f);
}
)";

    static void append_ellipsoid(std::vector<Model3DRenderer::Vtx>& verts,
        std::vector<uint32_t>& inds,
        float tx, float ty, float tz,
        float sx, float sy, float sz,
        int stacks = 14, int slices = 20)
    {
        uint32_t base = (uint32_t)verts.size();
        for (int i = 0; i <= stacks; ++i) {
            float phi = XM_PI * i / stacks;
            float sp = sinf(phi), cp = cosf(phi);
            for (int j = 0; j <= slices; ++j) {
                float theta = 2.0f * XM_PI * j / slices;
                float nx = sp * cosf(theta), ny = cp, nz = sp * sinf(theta);
                XMVECTOR nv = XMVector3Normalize(XMVectorSet(nx / sx, ny / sy, nz / sz, 0));
                XMFLOAT3 fn; XMStoreFloat3(&fn, nv);
                Model3DRenderer::Vtx v;
                v.px = nx * sx + tx; v.py = ny * sy + ty; v.pz = nz * sz + tz;
                v.nx = fn.x;       v.ny = fn.y;       v.nz = fn.z;
                v.tu = (float)j / slices; v.tv = (float)i / stacks;
                verts.push_back(v);
            }
        }
        for (int i = 0; i < stacks; ++i)
            for (int j = 0; j < slices; ++j) {
                uint32_t a = base + i * (slices + 1) + j, b = a + 1;
                uint32_t c = base + (i + 1) * (slices + 1) + j, d = c + 1;
                inds.insert(inds.end(), { a,c,b, b,c,d });
            }
    }

    void Model3DRenderer::build_procedural() {
        cpu_verts.clear(); cpu_inds.clear();

        append_ellipsoid(cpu_verts, cpu_inds, 0.000f, 0.580f, 0.000f, 0.215f, 0.215f, 0.215f);
        append_ellipsoid(cpu_verts, cpu_inds, 0.000f, 0.215f, 0.000f, 0.165f, 0.225f, 0.150f);
        append_ellipsoid(cpu_verts, cpu_inds, 0.000f, 0.005f, 0.000f, 0.145f, 0.090f, 0.140f);

        append_ellipsoid(cpu_verts, cpu_inds, -0.225f, 0.300f, 0.000f, 0.070f, 0.135f, 0.070f);
        append_ellipsoid(cpu_verts, cpu_inds, -0.250f, 0.095f, 0.000f, 0.063f, 0.115f, 0.063f);
        append_ellipsoid(cpu_verts, cpu_inds, -0.248f, -0.060f, 0.000f, 0.092f, 0.072f, 0.082f);
        append_ellipsoid(cpu_verts, cpu_inds, 0.225f, 0.300f, 0.000f, 0.070f, 0.135f, 0.070f);
        append_ellipsoid(cpu_verts, cpu_inds, 0.250f, 0.095f, 0.000f, 0.063f, 0.115f, 0.063f);
        append_ellipsoid(cpu_verts, cpu_inds, 0.248f, -0.060f, 0.000f, 0.092f, 0.072f, 0.082f);

        append_ellipsoid(cpu_verts, cpu_inds, -0.092f, -0.115f, 0.000f, 0.077f, 0.140f, 0.077f);
        append_ellipsoid(cpu_verts, cpu_inds, -0.092f, -0.320f, 0.000f, 0.070f, 0.125f, 0.070f);
        append_ellipsoid(cpu_verts, cpu_inds, -0.092f, -0.465f, 0.018f, 0.080f, 0.062f, 0.112f);
        append_ellipsoid(cpu_verts, cpu_inds, 0.092f, -0.115f, 0.000f, 0.077f, 0.140f, 0.077f);
        append_ellipsoid(cpu_verts, cpu_inds, 0.092f, -0.320f, 0.000f, 0.070f, 0.125f, 0.070f);
        append_ellipsoid(cpu_verts, cpu_inds, 0.092f, -0.465f, 0.018f, 0.080f, 0.062f, 0.112f);

        compute_bounds();
        mesh_ready = true;
    }

    void Model3DRenderer::compute_bounds() {
        if (cpu_verts.empty()) return;

        float mn_x = 1e9f, mn_y = 1e9f, mn_z = 1e9f;
        float mx_x = -1e9f, mx_y = -1e9f, mx_z = -1e9f;
        for (auto& v : cpu_verts) {
            mn_x = (std::min)(mn_x, v.px); mx_x = (std::max)(mx_x, v.px);
            mn_y = (std::min)(mn_y, v.py); mx_y = (std::max)(mx_y, v.py);
            mn_z = (std::min)(mn_z, v.pz); mx_z = (std::max)(mx_z, v.pz);
        }

        center_x = (mn_x + mx_x) * 0.5f;
        center_y = (mn_y + mx_y) * 0.5f;
        center_z = (mn_z + mx_z) * 0.5f;

        half_h = (std::max)((mx_y - mn_y) * 0.5f, 0.0001f);
        radius_xz = (std::max)({ (mx_x - mn_x) * 0.5f, (mx_z - mn_z) * 0.5f, 0.0001f });
    }

    bool Model3DRenderer::load_obj(const char* path) {
        std::ifstream disk_file(path, std::ios::binary | std::ios::ate);
        if (!disk_file.is_open()) {
            char buf[256];
            snprintf(buf, sizeof(buf), "[model3d] Cannot open OBJ: %s\n", path);
            OutputDebugStringA(buf);
            return false;
        }
        std::streamsize size = disk_file.tellg();
        disk_file.seekg(0, std::ios::beg);

        std::vector<char> buffer(size);
        if (!disk_file.read(buffer.data(), size)) return false;

        std::string base_dir;
        auto p = fs::path(path);
        auto parent = p.parent_path();
        if (!parent.empty()) base_dir = parent.string() + "\\";

        bool success = load_obj_from_memory(buffer.data(), size, base_dir.empty() ? nullptr : base_dir.c_str());
        if (success) {
            char buf[256];
            snprintf(buf, sizeof(buf), "[model3d] Loaded %s — %zu verts, %zu tris\n",
                path, cpu_verts.size(), cpu_inds.size() / 3);
            OutputDebugStringA(buf);
        }
        return success;
    }

    bool Model3DRenderer::load_obj_from_memory(const void* data, size_t size, const char* base_dir) {
        if (!data || size == 0) return false;
        std::string str_data((const char*)data, size);
        std::istringstream file(str_data);

        std::vector<XMFLOAT3> pos_arr, nor_arr;
        std::vector<XMFLOAT2> uv_arr;

        struct FaceVtx { int p, t, n; };

        auto parse_fv = [](const char* s, FaceVtx& fv) {
            fv = { 0,0,0 };
            int p = 0, t = 0, n = 0;
            if (sscanf(s, "%d/%d/%d", &p, &t, &n) == 3) { fv = { p,t,n }; }
            else if (sscanf(s, "%d//%d", &p, &n) == 2) { fv = { p,0,n }; }
            else if (sscanf(s, "%d/%d", &p, &t) == 2) { fv = { p,t,0 }; }
            else if (sscanf(s, "%d", &p) == 1) { fv = { p,0,0 }; }
            };

        struct FVHash {
            size_t operator()(const FaceVtx& f) const {
                return std::hash<long long>()(((long long)f.p << 20) | ((long long)f.t << 10) | f.n);
            }
        };
        struct FVEq {
            bool operator()(const FaceVtx& a, const FaceVtx& b) const {
                return a.p == b.p && a.t == b.t && a.n == b.n;
            }
        };
        std::unordered_map<FaceVtx, uint32_t, FVHash, FVEq> cache;

        cpu_verts.clear(); cpu_inds.clear();
        diffuse_path.clear();

        std::string mtllib_name;

        std::string line;
        while (std::getline(file, line)) {
            if (line.empty() || line[0] == '#') continue;
            std::istringstream ss(line);
            std::string tok; ss >> tok;

            if (tok == "v") {
                XMFLOAT3 p; ss >> p.x >> p.y >> p.z; pos_arr.push_back(p);
            }
            else if (tok == "vn") {
                XMFLOAT3 n; ss >> n.x >> n.y >> n.z; nor_arr.push_back(n);
            }
            else if (tok == "vt") {
                XMFLOAT2 t; ss >> t.x >> t.y; uv_arr.push_back(t);
            }
            else if (tok == "mtllib") {
                std::string mtl; std::getline(ss, mtl);
                mtl.erase(0, mtl.find_first_not_of(" \t"));
                mtllib_name = mtl;
            }
            else if (tok == "f") {
                std::vector<FaceVtx> fverts;
                std::string word;
                while (ss >> word) {
                    FaceVtx fv; parse_fv(word.c_str(), fv); fverts.push_back(fv);
                }

                for (int i = 1; i + 1 < (int)fverts.size(); ++i) {
                    FaceVtx tri[3] = { fverts[0], fverts[i], fverts[i + 1] };
                    for (auto& fv : tri) {
                        auto it = cache.find(fv);
                        if (it != cache.end()) {
                            cpu_inds.push_back(it->second);
                        }
                        else {
                            uint32_t idx = (uint32_t)cpu_verts.size();
                            cache[fv] = idx;
                            cpu_inds.push_back(idx);

                            Vtx v = {};
                            if (fv.p > 0 && fv.p <= (int)pos_arr.size()) {
                                auto& p = pos_arr[fv.p - 1];
                                v.px = p.x; v.py = p.y; v.pz = p.z;
                            }
                            if (fv.n > 0 && fv.n <= (int)nor_arr.size()) {
                                auto& n = nor_arr[fv.n - 1];
                                v.nx = n.x; v.ny = n.y; v.nz = n.z;
                            }
                            if (fv.t > 0 && fv.t <= (int)uv_arr.size()) {
                                auto& t = uv_arr[fv.t - 1];
                                v.tu = t.x; v.tv = 1.0f - t.y;
                            }
                            cpu_verts.push_back(v);
                        }
                    }
                }
            }
        }

        if (cpu_verts.empty()) return false;

        bool has_normals = false;
        for (auto& v : cpu_verts) if (v.nx || v.ny || v.nz) { has_normals = true; break; }
        if (!has_normals) {

            std::vector<XMFLOAT3> acc(cpu_verts.size(), { 0,0,0 });
            for (int i = 0; i + 2 < (int)cpu_inds.size(); i += 3) {
                auto& A = cpu_verts[cpu_inds[i]];
                auto& B = cpu_verts[cpu_inds[i + 1]];
                auto& C = cpu_verts[cpu_inds[i + 2]];
                XMVECTOR ab = XMVectorSet(B.px - A.px, B.py - A.py, B.pz - A.pz, 0);
                XMVECTOR ac = XMVectorSet(C.px - A.px, C.py - A.py, C.pz - A.pz, 0);
                XMVECTOR fn = XMVector3Normalize(XMVector3Cross(ab, ac));
                XMFLOAT3 n; XMStoreFloat3(&n, fn);
                for (int k = 0; k < 3; ++k) {
                    auto& a = acc[cpu_inds[i + k]];
                    a.x += n.x; a.y += n.y; a.z += n.z;
                }
            }
            for (int i = 0; i < (int)cpu_verts.size(); ++i) {
                XMVECTOR nv = XMVector3Normalize(XMLoadFloat3(&acc[i]));
                XMFLOAT3 fn; XMStoreFloat3(&fn, nv);
                cpu_verts[i].nx = fn.x; cpu_verts[i].ny = fn.y; cpu_verts[i].nz = fn.z;
            }
        }

        float mn_y = 1e9f, mx_y = -1e9f, mn_x = 1e9f, mx_x = -1e9f, mn_z = 1e9f, mx_z = -1e9f;
        float cx = 0, cy = 0, cz = 0;
        for (auto& v : cpu_verts) {
            mn_y = (std::min)(mn_y, v.py);
            mx_y = (std::max)(mx_y, v.py);
            mn_x = (std::min)(mn_x, v.px);
            mx_x = (std::max)(mx_x, v.px);
            mn_z = (std::min)(mn_z, v.pz);
            mx_z = (std::max)(mx_z, v.pz);
        }
        cx = (mn_x + mx_x) * 0.5f; cy = (mn_y + mx_y) * 0.5f; cz = (mn_z + mx_z) * 0.5f;
        float extent = (std::max)({ mx_y - mn_y, mx_x - mn_x, mx_z - mn_z }) * 0.5f;
        float inv = extent > 0.0f ? 1.0f / extent : 1.0f;
        for (auto& v : cpu_verts) {
            v.px = (v.px - cx) * inv;
            v.py = (v.py - cy) * inv;
            v.pz = (v.pz - cz) * inv;
        }

        if (cpu_verts.empty() || cpu_inds.empty()) return false;

        if (base_dir && !mtllib_name.empty()) {
            std::string mtl_path = std::string(base_dir) + mtllib_name;
            std::ifstream mtl_file(mtl_path);
            if (mtl_file.is_open()) {
                std::string mtl_line;
                while (std::getline(mtl_file, mtl_line)) {
                    if (mtl_line.compare(0, 7, "map_Kd ") == 0 || mtl_line.compare(0, 7, "map_kd ") == 0) {
                        std::string tex_name = mtl_line.substr(7);
                        size_t pos = tex_name.find_first_not_of(" \t");
                        if (pos != std::string::npos) tex_name = tex_name.substr(pos);
                        pos = tex_name.find_last_not_of(" \t\r\n");
                        if (pos != std::string::npos) tex_name = tex_name.substr(0, pos + 1);
                        diffuse_path = std::string(base_dir) + tex_name;
                        break;
                    }
                }
            }
        }

        compute_bounds();
        mesh_ready = true;
        return true;
    }

    bool Model3DRenderer::load_fbx(const char* path, const char* texture_dir) {
        ufbx_load_opts opts = {};
        opts.generate_missing_normals = true;
        opts.target_axes = ufbx_axes_left_handed_y_up;
        ufbx_error error;
        ufbx_scene* scene = ufbx_load_file(path, &opts, &error);
        if (!scene)
            return false;

        ufbx_scene* posed = nullptr;
        ufbx_anim* pose_anim = nullptr;
        {
            auto same_name = [](const ufbx_string& a, const char* b) {
                const size_t n = strlen(b);
                return a.data && a.length == n && memcmp(a.data, b, n) == 0;
            };
            ufbx_node* left = nullptr;
            ufbx_node* right = nullptr;
            for (size_t i = 0; i < scene->nodes.count; ++i) {
                ufbx_node* n = scene->nodes.data[i];
                if (same_name(n->name, "upperarm_l")) left = n;
                else if (same_name(n->name, "upperarm_r")) right = n;
            }
            ufbx_transform_override ovs[2] = {};
            size_t ovn = 0;
            auto add_drop = [&](ufbx_node* node, double deg) {
                if (!node || ovn >= 2) return;
                ufbx_transform t = node->local_transform;
                const ufbx_quat extra = ufbx_euler_to_quat(ufbx_vec3{ 0.0, 0.0, deg }, node->rotation_order);
                t.rotation = ufbx_quat_normalize(ufbx_quat_mul(t.rotation, extra));
                ovs[ovn].node_id = node->element_id;
                ovs[ovn].transform = t;
                ++ovn;
            };
            add_drop(left, -58.0);
            add_drop(right, 58.0);
            if (ovn) {
                ufbx_anim_opts ao = {};
                ao.transform_overrides.data = ovs;
                ao.transform_overrides.count = ovn;
                ufbx_error aerr = {};
                pose_anim = ufbx_create_anim(scene, &ao, &aerr);
                if (pose_anim) {
                    ufbx_evaluate_opts eo = {};
                    eo.evaluate_skinning = true;
                    ufbx_error eerr = {};
                    posed = ufbx_evaluate_scene(scene, pose_anim, 0.0, &eo, &eerr);
                }
            }
        }
        ufbx_scene* draw = posed ? posed : scene;

        cpu_verts.clear();
        cpu_inds.clear();
        submeshes.clear();

        auto leaf_name = [](const ufbx_string& fn) -> std::string {
            if (!fn.data || !fn.length) return {};
            std::string s(fn.data, fn.length);
            const size_t slash = s.find_last_of("/\\");
            return slash == std::string::npos ? s : s.substr(slash + 1);
        };

        auto resolve_tex = [&](const ufbx_material* mat) -> std::string {
            if (!mat) return {};
            ufbx_texture* tex = mat->fbx.diffuse_color.texture;
            if (!tex) tex = mat->pbr.base_color.texture;
            std::string leaf;
            if (tex)
                leaf = leaf_name(tex->relative_filename.length ? tex->relative_filename : tex->filename);
            if (leaf.empty()) {
                std::string n(mat->name.data, mat->name.length);
                for (char& c : n) c = (char)tolower((unsigned char)c);
                if (n.find("mask") != std::string::npos) leaf = "T_HerbalFist_Mask_D.png";
                else if (n.find("body") != std::string::npos) leaf = "T_HerbalFist_Body_D.png";
            }
            if (leaf.empty() || !texture_dir) return {};
            const std::string full = std::string(texture_dir) + leaf;
            return fs::exists(full) ? full : std::string();
        };

        for (size_t mi = 0; mi < draw->meshes.count; ++mi) {
            ufbx_mesh* mesh = draw->meshes.data[mi];
            if (!mesh || !mesh->instances.count) continue;
            ufbx_node* node = mesh->instances.data[0];
            const ufbx_matrix geom = node->geometry_to_world;
            const size_t part_count = mesh->material_parts.count ? mesh->material_parts.count : 1;

            for (size_t pi = 0; pi < part_count; ++pi) {
                const bool has_part = mesh->material_parts.count != 0;
                const ufbx_mesh_part* part = has_part ? &mesh->material_parts.data[pi] : nullptr;
                const ufbx_material* mat = (pi < mesh->materials.count) ? mesh->materials.data[pi] : nullptr;

                Submesh sm;
                sm.index_start = (uint32_t)cpu_inds.size();
                sm.tex_path = resolve_tex(mat);
                if (sm.tex_path.empty())
                    continue;

                auto emit = [&](uint32_t ix) {
                    const bool use_skin = mesh->skinned_position.exists;
                    ufbx_vec3 local_p = use_skin
                        ? ufbx_get_vertex_vec3(&mesh->skinned_position, ix)
                        : ufbx_get_vertex_vec3(&mesh->vertex_position, ix);
                    ufbx_vec3 p = (!use_skin || mesh->skinned_is_local)
                        ? ufbx_transform_position(&geom, local_p)
                        : local_p;
                    const bool use_sn = mesh->skinned_normal.exists;
                    ufbx_vec3 local_n = use_sn
                        ? ufbx_get_vertex_vec3(&mesh->skinned_normal, ix)
                        : (mesh->vertex_normal.exists
                            ? ufbx_get_vertex_vec3(&mesh->vertex_normal, ix)
                            : ufbx_vec3{ 0.0, 1.0, 0.0 });
                    ufbx_vec3 n = (!use_sn || mesh->skinned_is_local)
                        ? ufbx_transform_direction(&geom, local_n)
                        : local_n;
                    ufbx_vec2 uv = mesh->vertex_uv.exists
                        ? ufbx_get_vertex_vec2(&mesh->vertex_uv, ix)
                        : ufbx_vec2{ 0.0, 0.0 };
                    const float nx = (float)n.x, ny = (float)n.y, nz = (float)n.z;
                    const float len = sqrtf(nx * nx + ny * ny + nz * nz);
                    Vtx v;
                    v.px = (float)p.x; v.py = (float)p.y; v.pz = (float)p.z;
                    v.nx = len > 1e-6f ? nx / len : 0.0f;
                    v.ny = len > 1e-6f ? ny / len : 1.0f;
                    v.nz = len > 1e-6f ? nz / len : 0.0f;
                    v.tu = (float)uv.x;
                    v.tv = 1.0f - (float)uv.y;
                    cpu_inds.push_back((uint32_t)cpu_verts.size());
                    cpu_verts.push_back(v);
                };

                auto emit_face = [&](ufbx_face face) {
                    if (face.num_indices < 3) return;
                    std::vector<uint32_t> tri((face.num_indices - 2) * 3);
                    const uint32_t n = ufbx_triangulate_face(tri.data(), tri.size(), mesh, face);
                    for (uint32_t t = 0; t < n; ++t) {
                        emit(tri[t * 3 + 0]);
                        emit(tri[t * 3 + 1]);
                        emit(tri[t * 3 + 2]);
                    }
                };

                if (part) {
                    for (size_t fi = 0; fi < part->face_indices.count; ++fi)
                        emit_face(mesh->faces.data[part->face_indices.data[fi]]);
                } else {
                    for (size_t fi = 0; fi < mesh->faces.count; ++fi)
                        emit_face(mesh->faces.data[fi]);
                }

                sm.index_count = (uint32_t)cpu_inds.size() - sm.index_start;
                if (sm.index_count)
                    submeshes.push_back(std::move(sm));
            }
        }

        if (posed) ufbx_free_scene(posed);
        if (pose_anim) ufbx_free_anim(pose_anim);
        ufbx_free_scene(scene);
        if (cpu_inds.empty())
            return false;
        compute_bounds();
        mesh_ready = true;
        return true;
    }

    bool Model3DRenderer::upload_mesh(ID3D11Device* dev) {
        if (vb) { vb->Release(); vb = nullptr; }
        if (ib) { ib->Release(); ib = nullptr; }

        if (cpu_verts.empty()) return false;

        D3D11_BUFFER_DESC bd = {};
        D3D11_SUBRESOURCE_DATA sd = {};

        bd.Usage = D3D11_USAGE_IMMUTABLE;
        bd.BindFlags = D3D11_BIND_VERTEX_BUFFER;
        bd.ByteWidth = (UINT)(cpu_verts.size() * sizeof(Vtx));
        sd.pSysMem = cpu_verts.data();
        if (FAILED(dev->CreateBuffer(&bd, &sd, &vb))) return false;

        bd.BindFlags = D3D11_BIND_INDEX_BUFFER;
        bd.ByteWidth = (UINT)(cpu_inds.size() * sizeof(uint32_t));
        sd.pSysMem = cpu_inds.data();
        if (FAILED(dev->CreateBuffer(&bd, &sd, &ib))) return false;

        index_count = (int)cpu_inds.size();
        return true;
    }

    bool Model3DRenderer::load_texture(ID3D11Device* dev, const char* path) {
        if (diffuse_srv) { diffuse_srv->Release(); diffuse_srv = nullptr; }
        FILE* f = nullptr;
        if (fopen_s(&f, path, "rb") != 0 || !f) return false;
        fseek(f, 0, SEEK_END); long len = ftell(f); fseek(f, 0, SEEK_SET);
        std::vector<unsigned char> buf(len);
        if (fread(buf.data(), 1, len, f) != (size_t)len) { fclose(f); return false; }
        fclose(f);
        return create_texture_from_memory(dev, buf.data(), buf.size(), &diffuse_srv);
    }

    bool Model3DRenderer::init(ID3D11Device* dev) {
        if (init_done) return true;

        if (!mesh_ready) {
            if (!load_fbx("C:\\Users\\Lian\\Downloads\\3d\\source\\Spiderman Brand New Day Mask.fbx",
                          "C:\\Users\\Lian\\Downloads\\3d\\textures\\")) {
                if (!egui_model_embedded() ||
                    !load_obj_from_memory(egui_model_obj, sizeof(egui_model_obj)))
                    build_procedural();
            }
        }
        if (!upload_mesh(dev)) return false;

        ID3DBlob* vsb = nullptr, * psb = nullptr, * err = nullptr;
        if (FAILED(D3DCompile(k_shader, strlen(k_shader), nullptr, nullptr, nullptr,
            "VS", "vs_5_0", 0, 0, &vsb, &err))) {
            if (err) { OutputDebugStringA((char*)err->GetBufferPointer()); err->Release(); }
            return false;
        }
        if (FAILED(D3DCompile(k_shader, strlen(k_shader), nullptr, nullptr, nullptr,
            "PS", "ps_5_0", 0, 0, &psb, &err))) {
            if (err) { OutputDebugStringA((char*)err->GetBufferPointer()); err->Release(); }
            vsb->Release(); return false;
        }
        if (FAILED(dev->CreateVertexShader(vsb->GetBufferPointer(), vsb->GetBufferSize(), nullptr, &vs))) {
            vsb->Release(); psb->Release(); return false;
        }
        if (FAILED(dev->CreatePixelShader(psb->GetBufferPointer(), psb->GetBufferSize(), nullptr, &ps))) {
            vs->Release(); vs = nullptr; vsb->Release(); psb->Release(); return false;
        }

        D3D11_INPUT_ELEMENT_DESC ied[] = {
            {"POSITION",0,DXGI_FORMAT_R32G32B32_FLOAT,0, 0,D3D11_INPUT_PER_VERTEX_DATA,0},
            {"NORMAL",  0,DXGI_FORMAT_R32G32B32_FLOAT,0,12,D3D11_INPUT_PER_VERTEX_DATA,0},
            {"TEXCOORD",0,DXGI_FORMAT_R32G32_FLOAT,    0,24,D3D11_INPUT_PER_VERTEX_DATA,0},
        };
        if (FAILED(dev->CreateInputLayout(ied, 3, vsb->GetBufferPointer(), vsb->GetBufferSize(), &layout))) {
            ps->Release(); ps = nullptr; vs->Release(); vs = nullptr;
            vsb->Release(); psb->Release(); return false;
        }
        vsb->Release(); psb->Release();

        {
            D3D11_BUFFER_DESC bd = {};
            bd.Usage = D3D11_USAGE_DYNAMIC; bd.BindFlags = D3D11_BIND_CONSTANT_BUFFER;
            bd.ByteWidth = 176; bd.CPUAccessFlags = D3D11_CPU_ACCESS_WRITE;
            if (FAILED(dev->CreateBuffer(&bd, nullptr, &cb))) {
                layout->Release(); layout = nullptr; ps->Release(); ps = nullptr;
                vs->Release(); vs = nullptr; return false;
            }
        }

        {
            D3D11_RASTERIZER_DESC rd = {};
            rd.FillMode = D3D11_FILL_SOLID; rd.CullMode = D3D11_CULL_NONE;
            rd.DepthClipEnable = TRUE;
            if (FAILED(dev->CreateRasterizerState(&rd, &rs))) {
                cb->Release(); cb = nullptr; layout->Release(); layout = nullptr;
                ps->Release(); ps = nullptr; vs->Release(); vs = nullptr; return false;
            }
        }

        {
            D3D11_DEPTH_STENCIL_DESC dsd = {};
            dsd.DepthEnable = TRUE;
            dsd.DepthWriteMask = D3D11_DEPTH_WRITE_MASK_ALL;
            dsd.DepthFunc = D3D11_COMPARISON_LESS;
            if (FAILED(dev->CreateDepthStencilState(&dsd, &dss))) {
                rs->Release(); rs = nullptr; cb->Release(); cb = nullptr;
                layout->Release(); layout = nullptr; ps->Release(); ps = nullptr;
                vs->Release(); vs = nullptr; return false;
            }
        }

        {
            D3D11_SAMPLER_DESC sd = {};
            sd.Filter = D3D11_FILTER_MIN_MAG_MIP_LINEAR;
            sd.AddressU = D3D11_TEXTURE_ADDRESS_WRAP;
            sd.AddressV = D3D11_TEXTURE_ADDRESS_WRAP;
            sd.AddressW = D3D11_TEXTURE_ADDRESS_WRAP;
            sd.MaxAnisotropy = 1;
            sd.MinLOD = 0; sd.MaxLOD = D3D11_FLOAT32_MAX;
            if (FAILED(dev->CreateSamplerState(&sd, &sampler_state))) {
                dss->Release(); dss = nullptr; rs->Release(); rs = nullptr;
                cb->Release(); cb = nullptr; layout->Release(); layout = nullptr;
                ps->Release(); ps = nullptr; vs->Release(); vs = nullptr;
                return false;
            }
        }

        {
            D3D11_SUBRESOURCE_DATA init = {};
            unsigned int white_pixel = 0xFFFFFFFF;
            init.pSysMem = &white_pixel;
            init.SysMemPitch = 4;
            D3D11_TEXTURE2D_DESC td = {};
            td.Width = 1; td.Height = 1; td.MipLevels = 1; td.ArraySize = 1;
            td.Format = DXGI_FORMAT_R8G8B8A8_UNORM;
            td.SampleDesc.Count = 1;
            td.Usage = D3D11_USAGE_DEFAULT;
            td.BindFlags = D3D11_BIND_SHADER_RESOURCE;
            ID3D11Texture2D* white_tex = nullptr;
            ID3D11ShaderResourceView* white_srv = nullptr;
            if (SUCCEEDED(dev->CreateTexture2D(&td, &init, &white_tex)) &&
                SUCCEEDED(dev->CreateShaderResourceView(white_tex, nullptr, &white_srv))) {
                if (!diffuse_srv) diffuse_srv = white_srv; else white_srv->Release();
                white_tex->Release();
            }
        }

        if (!diffuse_path.empty()) {
            load_texture(dev, diffuse_path.c_str());
        }
        for (Submesh& part : submeshes) {
            if (!part.tex_path.empty())
                load_texture(dev, part.tex_path.c_str());
            part.srv = diffuse_srv;
            diffuse_srv = nullptr;
        }

        init_done = true;
        return true;
    }

    bool Model3DRenderer::ensure_rt(ID3D11Device* dev, int w, int h) {
        if (rt_w == w && rt_h == h && rtv && dsv) return true;
        auto rel = [](auto*& p) {if (p) { p->Release(); p = nullptr; }};
        rel(rtv); rel(srv); rel(rt_tex); rel(dsv); rel(ds_tex);
        rt_w = w; rt_h = h;

        D3D11_TEXTURE2D_DESC td = {};
        td.Width = (UINT)w; td.Height = (UINT)h; td.MipLevels = 1; td.ArraySize = 1;
        td.SampleDesc.Count = 1; td.Usage = D3D11_USAGE_DEFAULT;

        td.Format = DXGI_FORMAT_R8G8B8A8_UNORM;
        td.BindFlags = D3D11_BIND_RENDER_TARGET | D3D11_BIND_SHADER_RESOURCE;
        if (FAILED(dev->CreateTexture2D(&td, nullptr, &rt_tex))) { rt_w = rt_h = 0; return false; }
        if (FAILED(dev->CreateRenderTargetView(rt_tex, nullptr, &rtv))) { rel(rt_tex); rt_w = rt_h = 0; return false; }
        if (FAILED(dev->CreateShaderResourceView(rt_tex, nullptr, &srv))) { rel(rtv); rel(rt_tex); rt_w = rt_h = 0; return false; }

        td.Format = DXGI_FORMAT_D24_UNORM_S8_UINT;
        td.BindFlags = D3D11_BIND_DEPTH_STENCIL;
        if (FAILED(dev->CreateTexture2D(&td, nullptr, &ds_tex))) { rel(srv); rel(rtv); rel(rt_tex); rt_w = rt_h = 0; return false; }
        if (FAILED(dev->CreateDepthStencilView(ds_tex, nullptr, &dsv))) { rel(ds_tex); rel(srv); rel(rtv); rel(rt_tex); rt_w = rt_h = 0; return false; }

        return true;
    }

    struct alignas(16) CBData {
        XMFLOAT4X4 mvp;
        XMFLOAT4X4 world;
        XMFLOAT4   key;
        XMFLOAT4   fill;
        XMFLOAT4   cam;
    };

    void Model3DRenderer::render(ID3D11Device* dev, ID3D11DeviceContext* ctx,
        int w, int h, float mdx, float mdy, bool dragging)
    {
        if (w <= 0 || h <= 0) return;
        if (!init(dev)) return;
        if (!ensure_rt(dev, w, h)) return;

        if (dragging) {
            yaw_target -= mdx * orbit_speed;
            pitch = ImClamp(pitch - mdy * orbit_speed, -pitch_limit, pitch_limit);
        }
        yaw = ImLerp(yaw, yaw_target, ImGui::GetIO().DeltaTime * 12.0f);

        ID3D11RenderTargetView* prev_rtv = nullptr;
        ID3D11DepthStencilView* prev_dsv = nullptr;
        ctx->OMGetRenderTargets(1, &prev_rtv, &prev_dsv);
        UINT nvp = 1; D3D11_VIEWPORT prev_vp = {};
        ctx->RSGetViewports(&nvp, &prev_vp);

        if (!rtv || !dsv) { if (prev_rtv) prev_rtv->Release(); if (prev_dsv) prev_dsv->Release(); return; }

        ctx->ClearRenderTargetView(rtv, bg);
        ctx->ClearDepthStencilView(dsv, D3D11_CLEAR_DEPTH, 1.0f, 0);
        ctx->OMSetRenderTargets(1, &rtv, dsv);
        D3D11_VIEWPORT vp = { 0,0,(float)w,(float)h,0,1 };
        ctx->RSSetViewports(1, &vp);

        const float k_fov = XMConvertToRadians(38.0f);
        const float aspect = (float)w / h;

        XMMATRIX world = XMMatrixTranslation(-center_x, -center_y, -center_z) *
                         XMMatrixScaling(1.0f, 1.0f, thickness) *
                         XMMatrixRotationX(pitch) *
                         XMMatrixRotationY(yaw) *
                         XMMatrixTranslation(offset_x, offset_y, 0.0f);

        float dist = distance;
        if (auto_fit) {
            const float tan_half = tanf(k_fov * 0.5f);
            const float dv = half_h / ((std::max)(fill, 0.05f) * tan_half);
            const float dh = radius_xz / ((std::max)(fill, 0.05f) * tan_half * aspect);
            dist = (std::max)(dv, dh);
        }

        XMVECTOR cam_pos = XMVectorSet(0.0f, 0.0f, -dist, 1);
        XMMATRIX view = XMMatrixLookAtLH(
            cam_pos,
            XMVectorSet(0.0f, 0.0f, 0.0f, 1),
            XMVectorSet(0.0f, 1.0f, 0.0f, 0));
        const float far_z = dist * 8.0f + 1.0f;
        const float near_z = (std::max)(dist * 0.01f, 0.01f);
        XMMATRIX proj = XMMatrixPerspectiveFovLH(k_fov, aspect, near_z, far_z);
        XMMATRIX mvp = world * view * proj;

        D3D11_MAPPED_SUBRESOURCE ms = {};
        ctx->Map(cb, 0, D3D11_MAP_WRITE_DISCARD, 0, &ms);
        CBData* d = (CBData*)ms.pData;
        XMStoreFloat4x4(&d->mvp, XMMatrixTranspose(mvp));
        XMStoreFloat4x4(&d->world, XMMatrixTranspose(world));
        d->key = XMFLOAT4(0.55f, 1.0f, -0.30f, 0);
        d->fill = XMFLOAT4(-0.40f, 0.3f, 0.80f, 0);
        XMStoreFloat4(&d->cam, cam_pos);
        ctx->Unmap(cb, 0);

        ID3D11SamplerState* samp = sampler_state;
        ctx->PSSetSamplers(0, 1, &samp);

        ctx->IASetPrimitiveTopology(D3D11_PRIMITIVE_TOPOLOGY_TRIANGLELIST);
        ctx->IASetInputLayout(layout);
        UINT stride = sizeof(Vtx), offset = 0;
        ctx->IASetVertexBuffers(0, 1, &vb, &stride, &offset);
        ctx->IASetIndexBuffer(ib, DXGI_FORMAT_R32_UINT, 0);
        ctx->VSSetShader(vs, nullptr, 0);
        ctx->PSSetShader(ps, nullptr, 0);
        ctx->VSSetConstantBuffers(0, 1, &cb);
        ctx->PSSetConstantBuffers(0, 1, &cb);
        ctx->RSSetState(rs);
        ctx->OMSetDepthStencilState(dss, 0);
        ctx->OMSetBlendState(nullptr, nullptr, 0xffffffff);
        if (submeshes.empty()) {
            ID3D11ShaderResourceView* tex_srv = diffuse_srv;
            ctx->PSSetShaderResources(0, 1, &tex_srv);
            ctx->DrawIndexed((UINT)index_count, 0, 0);
        } else {
            for (const Submesh& part : submeshes) {
                ID3D11ShaderResourceView* tex_srv = part.srv ? part.srv : diffuse_srv;
                ctx->PSSetShaderResources(0, 1, &tex_srv);
                ctx->DrawIndexed(part.index_count, part.index_start, 0);
            }
        }

        ID3D11ShaderResourceView* unbound = nullptr;
        ctx->PSSetShaderResources(0, 1, &unbound);

        ctx->OMSetRenderTargets(1, &prev_rtv, prev_dsv);
        ctx->RSSetViewports(1, &prev_vp);
        if (prev_rtv) prev_rtv->Release();
        if (prev_dsv) prev_dsv->Release();
    }

    void Model3DRenderer::shutdown() {
        auto rel = [](auto*& p) {if (p) { p->Release(); p = nullptr; }};
        rel(rtv); rel(srv); rel(rt_tex); rel(dsv); rel(ds_tex);
        rel(vs); rel(ps); rel(layout); rel(vb); rel(ib); rel(cb);
        for (Submesh& part : submeshes) rel(part.srv);
        submeshes.clear();
        rel(rs); rel(dss); rel(diffuse_srv); rel(sampler_state);
        init_done = false; rt_w = rt_h = 0; mesh_ready = false;
    }

}
