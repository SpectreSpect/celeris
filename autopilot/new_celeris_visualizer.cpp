#include "new_celeris_visualizer.h"

#include "../managers/material_instance_manager.h"
#include "../renderer/material_data_types.h"
#include "../voxel_grid_vulkan/voxel_grid.h"
// #include "../vulkan_self/vulkan_engine.h"
#include "../a_star/a_star_structures.h"
#include "../managers/mesh_manager.h"
#include "new_celeris.h"

NewCelerisVisualizer::NewCelerisVisualizer(
    VulkanEngine& engine,
    MeshManager& mesh_manager,
    MaterialInstanceManager& material_instance_manager,
    NewCeleris& celeris, 
    const VehicleGeometry& vehicle_geometry,
    float skybox_exposure,
    uint32_t max_path_line_count) 
    :   m_celeris(&celeris),
        m_max_path_line_count(max_path_line_count),
        m_gazelle_next(
            mesh_manager, 
            material_instance_manager,
            vehicle_geometry, 
            skybox_exposure),
        m_start_marker(
            mesh_manager,
            material_instance_manager,
            PBRMaterialData::create(1.0f, 0.7f, skybox_exposure, glm::vec4(1, 0, 0, 1))
        ),
        m_goal_marker(
            mesh_manager,
            material_instance_manager,
            PBRMaterialData::create(1.0f, 0.7f, skybox_exposure, glm::vec4(0, 0, 1, 1))
        ),
        m_path_line_cloud(
            engine,
            mesh_manager,
            material_instance_manager,
            max_path_line_count
        ),
        m_guide_path_line_cloud(
            engine,
            mesh_manager,
            material_instance_manager,
            max_path_line_count
        ),
        m_explored_paths_line_cloud(
            engine,
            mesh_manager,
            material_instance_manager,
            max_path_line_count
        ),
        m_unimpended_path_line_cloud(
            engine,
            mesh_manager,
            material_instance_manager,
            max_path_line_count
        ){
    m_path_line_cloud.set_material_data(LineMaterialData{
        .color = glm::vec4(1.0f, 1.0f, 1.0f, 1.0f),
        .line_width_pixels = 5
    });
    m_guide_path_line_cloud.set_material_data(LineMaterialData{
        .color = glm::vec4(1.0f, 1.0f, 1.0f, 1.0f),
        .line_width_pixels = 5
    });
    m_explored_paths_line_cloud.set_material_data(LineMaterialData{
        .color = glm::vec4(0.0f, 0.0f, 0.0f, 1.0f),
        .line_width_pixels = 3
    });
    m_unimpended_path_line_cloud.set_material_data(LineMaterialData{
        .color = glm::vec4(1.0f, 1.0f, 0.0f, 1.0f),
        .line_width_pixels = 5
    });
    
    add_child(m_gazelle_next);
    add_child(m_start_marker);
    add_child(m_goal_marker);

    add_child(m_path_line_cloud);
    add_child(m_guide_path_line_cloud);
    add_child(m_explored_paths_line_cloud);
    add_child(m_unimpended_path_line_cloud);
}

void NewCelerisVisualizer::update() {
    LOG_METHOD();

    logger().check(m_celeris, "Celeris was null");

    update_gazelle_next_transform();
    set_start(m_celeris->start_position());
    set_goal(m_celeris->goal_position());

    const PathPlanner::PathPlannerResult& path_planner_stapshot = m_celeris->path_planner_snapshot();

    std::vector<LineInstance> path_lines = get_line_instances(
        path_planner_stapshot.nonholonomic_astar_path,
        glm::vec4(1.0f, 0.0f, 0.0f, 1.0f),
        glm::vec4(0.0f, 0.0f, 1.0f, 1.0f)
    );
    m_path_line_cloud.set_lines(path_lines);

    std::vector<LineInstance> guide_path_lines = get_line_instances(
        path_planner_stapshot.plain_astar_path.path,
        glm::vec4(0.3f, 1.0f, 0.3f, 1.0f)
    );
    m_guide_path_line_cloud.set_lines(guide_path_lines);

    m_explored_paths_line_cloud.set_lines(path_planner_stapshot.explored_paths);

    std::vector<LineInstance> unimpended_path_lines = get_line_instances(
        path_planner_stapshot.unimpended_path,
        glm::vec4(1.0f, 1.0f, 0.0f, 1.0f),
        glm::vec4(1.0f, 1.0f, 0.0f, 1.0f),
        0.15f
    );
    m_unimpended_path_line_cloud.set_lines(unimpended_path_lines);
}

void NewCelerisVisualizer::gazelle_next_visible(bool visible) {
    m_gazelle_next.visible = visible;
}

void NewCelerisVisualizer::voxel_grid_visible(bool visible) {
    LOG_METHOD();

    logger().check(m_celeris->voxel_grid(), "Voxel grid was null");

    m_celeris->voxel_grid()->visible = visible;
}

void NewCelerisVisualizer::path_visibile(bool visible){
    m_path_line_cloud.visible = visible;
}

void NewCelerisVisualizer::guide_path_visible(bool visible) {
    m_guide_path_line_cloud.visible = visible;
}

void NewCelerisVisualizer::explored_paths_visible(bool visible) {
    m_explored_paths_line_cloud.visible = visible;
}

void NewCelerisVisualizer::unimpended_path_visible(bool visible) {
    m_unimpended_path_line_cloud.visible = visible;
}

void NewCelerisVisualizer::update_gazelle_next_transform() {
    Transform* lidar_transform = m_celeris->lidar_tranform();
    if (lidar_transform)
        m_gazelle_next.set_lidar_transform(*lidar_transform);
}

void NewCelerisVisualizer::set_marker_pose(
    SphericalPoseMarker& marker, 
    NonholonomicPos nonholonomic_position) 
{
    logger().check(m_celeris, "Celeris was null");

    glm::vec3 voxel_size = m_celeris->voxel_grid()->voxel_size();
    glm::vec3 vertical_offset = glm::vec3(0.0f, 0.5f * voxel_size.y, 0.0f);

    marker.transform.position = nonholonomic_position.pos + vertical_offset;
    marker.transform.rotation = glm::angleAxis(
        glm::pi<float>() - nonholonomic_position.theta,
        glm::vec3(0.0f, 1.0f, 0.0f)
    );
}

void NewCelerisVisualizer::set_start(const NonholonomicPos& position) {
    set_marker_pose(m_start_marker, position);
}

void NewCelerisVisualizer::set_goal(const NonholonomicPos& position) {
    set_marker_pose(m_goal_marker, position);
}

std::vector<LineInstance> NewCelerisVisualizer::get_line_instances(
    const std::vector<NonholonomicPos> path, 
    glm::vec4 forward_color,
    glm::vec4 backward_color,
    float y_offset,
    float forward_dir_y_offset) 
{
    std::vector<LineInstance> path_lines;
    path_lines.reserve(std::min<size_t>(path.size(), m_max_path_line_count));

    float backward_dir_y_offset = 0.02f;
    glm::vec3 voxel_size = m_celeris->voxel_grid()->voxel_size();
    for (uint32_t i = 1; i < path.size() && path_lines.size() < m_max_path_line_count; i++) {
        glm::vec3 p0 = path[i - 1].pos;
        glm::vec3 p1 = path[i].pos;

        float final_y_offset = y_offset;
        glm::vec4 color = backward_color;
        if (path[i].dir == 1) {
            color =forward_color;
            final_y_offset += forward_dir_y_offset;
        }

        p0.y += voxel_size.y * final_y_offset;
        p1.y += voxel_size.y * final_y_offset;        

        path_lines.push_back(LineInstance{
            .p0 = p0,
            .p1 = p1,
            .color = color
        });
    }

    return path_lines;
}

std::vector<LineInstance> NewCelerisVisualizer::get_line_instances(
    const std::vector<glm::ivec3> path,
    glm::vec4 color,
    float y_offset) 
{
    std::vector<LineInstance> path_lines;
    path_lines.reserve(std::min<size_t>(path.size(), m_max_path_line_count));

    glm::vec3 voxel_size = m_celeris->voxel_grid()->voxel_size();
    for (uint32_t i = 1; i < path.size() && path_lines.size() < m_max_path_line_count; i++) {
        glm::vec3 p0 = m_celeris->voxel_center_bottom_world_pos(path[i - 1]);
        glm::vec3 p1 = m_celeris->voxel_center_bottom_world_pos(path[i]);
        p0.y += voxel_size.y * y_offset;
        p1.y += voxel_size.y * y_offset;

        path_lines.push_back(LineInstance{
            .p0 = p0,
            .p1 = p1,
            .color = color
        });
    }

    return path_lines;
}