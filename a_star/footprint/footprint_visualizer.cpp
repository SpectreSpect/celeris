#include "footprint_visualizer.h"

#include "../../autopilot/celeris.h"
#include "../../managers/material_instance_manager.h"
#include "../../managers/mesh_manager.h"
#include "../../renderer/material_data_types.h"

#include <algorithm>

#include <glm/vec4.hpp>

FootprintVisualizer::FootprintVisualizer(
    MeshManager& mesh_manager,
    MaterialInstanceManager& material_instance_manager,
    Celeris& celeris,
    Footprint& footprint,
    float skybox_exposure)
    : m_footprint(&footprint)
{
    const glm::vec3 voxel_size = celeris.voxel_size();
    const glm::vec3 marker_scale(
        static_cast<float>(m_footprint->horizontal_inflation_size()) * voxel_size.x * 2.0f,
        static_cast<float>(m_footprint->vertical_inflation_size()) * voxel_size.y,
        static_cast<float>(m_footprint->horizontal_inflation_size()) * voxel_size.z * 2.0f
    );

    for (uint32_t i = 0; i < m_footprint->sample_count(); i++) {
        m_markers.emplace_back(mesh_manager.cube, material_instance_manager.pbr);
        m_markers.back().set_material_data(
            PBRMaterialData::create(0.0f, 0.95f, skybox_exposure, glm::vec4(1.0f), 1.0f)
        );
        m_markers.back().transform.scale = marker_scale;

        add_child(m_markers.back());
    }
}

void FootprintVisualizer::update_footprint(const NonholonomicPos& rear_axle_pos) {
    Footprint::SampleResult sample = m_footprint->sample(rear_axle_pos);

    const glm::vec4 color = sample.is_passible ?
        glm::vec4(1, 1, 1, 1) :
        glm::vec4(1, 0, 0, 1);

    const size_t marker_count = std::min(m_markers.size(), sample.positions.size());
    for (size_t i = 0; i < marker_count; i++) {
        m_markers[i].edit_material_data<PBRMaterialData>([&](PBRMaterialData& data){
            data.color = color;
        });

        m_markers[i].transform.position = sample.positions[i] +
            glm::vec3(0, m_markers[i].transform.scale.y / 2.0f, 0);
    }
}
