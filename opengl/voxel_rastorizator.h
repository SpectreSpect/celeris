#pragma once

#include <iostream>
#include <vector>
#include <algorithm>
#include <cmath>

#include "mesh_data.h"
#include "gridable.h"
#include "voxel_engine/voxel.h"


class VoxelRastorizator {
public:
    IGridable* gridable;

    VoxelRastorizator(IGridable* gridable);

    static float fmin3(float a, float b, float c);
    static float fmax3(float a, float b, float c);
    static bool plane_box_overlap(const glm::vec3& normal, const glm::vec3& vert, const glm::vec3& maxbox);
    static bool tri_box_overlap(const glm::vec3& boxcenter, const glm::vec3& boxhalf, const glm::vec3& v0,
                                const glm::vec3& v1, const glm::vec3& v2);
    static std::vector<glm::ivec3> rasterize_triangle_to_points(glm::vec3 v0, glm::vec3 v1, glm::vec3 v2, float voxel_size);

    // voxel_generator = Voxel F(glm::ivec3 point)
    template <class F>
    void rasterize_triangle(glm::vec3 v0, glm::vec3 v1, glm::vec3 v2, float voxel_size, F& voxel_generator) {
        std::vector<glm::ivec3> raster = rasterize_triangle_to_points(v0, v1, v2, voxel_size);
        
        std::vector<Voxel> voxels;
        voxels.reserve(raster.size());

        for (auto point : raster) {
            Voxel v = voxel_generator(point);
            voxels.push_back(v);
        }

        gridable->set_voxels(voxels, raster);
    }

    // voxel_generator = Voxel F(glm::ivec3 point)
    template <class F>
    void rasterize_mesh(MeshData& mesh_data, glm::mat4 transform, F& voxel_generator, float voxel_size, int position_offset, int vertex_stride) {
        int count_vertices = mesh_data.indices.size() / 3;

        std::vector<std::vector<glm::ivec3>> triangle_points_batch;

        size_t count_points = 0;
        for (size_t i = 0; i < count_vertices; i++) {
            size_t v_id1 = mesh_data.indices[i * 3];
            size_t v_id2 = mesh_data.indices[i * 3 + 1];
            size_t v_id3 = mesh_data.indices[i * 3 + 2];

            size_t base1 = v_id1 * vertex_stride + position_offset;
            size_t base2 = v_id2 * vertex_stride + position_offset;
            size_t base3 = v_id3 * vertex_stride + position_offset;

            glm::vec4 v0_vec4 = glm::vec4(mesh_data.vertices[base1 + 0], mesh_data.vertices[base1 + 1], mesh_data.vertices[base1 + 2], 1.0f);
            glm::vec4 v1_vec4 = glm::vec4(mesh_data.vertices[base2 + 0], mesh_data.vertices[base2 + 1], mesh_data.vertices[base2 + 2], 1.0f);
            glm::vec4 v2_vec4 = glm::vec4(mesh_data.vertices[base3 + 0], mesh_data.vertices[base3 + 1], mesh_data.vertices[base3 + 2], 1.0f);

            glm::vec3 v0 = glm::vec3(transform * v0_vec4);
            glm::vec3 v1 = glm::vec3(transform * v1_vec4);
            glm::vec3 v2 = glm::vec3(transform * v2_vec4);

            std::vector<glm::ivec3> triangle_points = rasterize_triangle_to_points(v0, v1, v2, voxel_size);
            count_points += triangle_points.size();

            triangle_points_batch.push_back(std::move(triangle_points));
        }

        std::vector<glm::ivec3> triangle_points_batch_flatten;
        triangle_points_batch_flatten.reserve(count_points);

        for (std::vector<glm::ivec3>& points : triangle_points_batch) {
            triangle_points_batch_flatten.insert(
                triangle_points_batch_flatten.end(),
                points.begin(),
                points.end()
            );
        }

        std::vector<Voxel> voxels;
        voxels.reserve(count_points);

        for (glm::ivec3& point : triangle_points_batch_flatten) {
            Voxel voxel = voxel_generator(point);
            voxels.push_back(voxel);
        }

        gridable->set_voxels(voxels, triangle_points_batch_flatten);
    }
};