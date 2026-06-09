#pragma once

#include <fstream>
#include <filesystem>
#include <string_view>

#include "mesh_loader.h"
#include "vertex_layout.h"


namespace fs = std::filesystem;

class VtkMeshLoader : MeshLoader {
public:
    // std::vector<float> default_vertex_data;
    // int vertex_position_offset;
    // int vertex_normal_offset;

    VertexLayout vertex_layout;
    std::vector<std::byte> default_vertex_data;
    size_t vertex_position_offset;
    size_t vertex_normal_offset;

    // VtkMeshLoader(VertexLayout vertex_layout, const std::vector<float>& default_vertex_data, int vertex_position_offset, int vertex_normal_offset);
    VtkMeshLoader(VertexLayout& vertex_layout);
    template<class... Ts>
    static inline bool read(std::istream& in, Ts&... xs) {
        return static_cast<bool>((in >> ... >> xs));
    };
    static void expect(bool cond, const std::string& msg);
    virtual MeshData load_mesh(std::string& file_path) const override;
};