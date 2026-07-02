#pragma once

#include "../renderer/scene_object.h"
#include "../renderer/render_object.h"
#include "../renderer/transform.h"

#include <glm/vec3.hpp>
#include <glm/gtc/quaternion.hpp>

class MeshManager;
class MaterialInstanceManager;

class GazelleNext : public SceneObject {
public:
    float car_length = 6.6f;

    float compartment_1_weight = 3.41f;
    float compartment_2_weight = 2.0f;
    float compartment_3_weight = 1.0f;
    float compartment_3_length = 1.5f;

    float car_body_height = 2.75f;
    float compartment_1_top_height_from_rear_wheel_center = 2.30f;
    float compartment_2_top_height_from_rear_wheel_center = 1.70f;
    float compartment_3_top_height_from_rear_wheel_center = 0.70f;
    float lidar_height_from_wheel_bottom = 1.51f;

    float axle_length = 1.805f;
    float rear_axle_x_portion = 0.2f;
    float front_axle_x_portion = 0.85f;

    glm::vec3 wheel_scale = glm::vec3(0.78f, 0.2f, 0.78f);
    float connector_depth = 2.0f;
    float connector_thickness = 0.8f;
    float connector_end_offset_from_car_front_portion = 0.0f;

    float lidar_x_portion = 1.0f - 0.6f / 6.6f;
    glm::vec3 lidar_scale = glm::vec3(0.2f, 0.1f, 0.2f);

    GazelleNext(
        MeshManager& mesh_manager,
        MaterialInstanceManager& material_instance_manager,
        float skybox_exposure
    );

    void update_layout();
    void set_rear_axle_position(const glm::vec3& position);
    void set_rear_axle_rotation(const glm::quat& rotation);
    void set_rear_axle_transform(const Transform& rear_axle_transform);
    void set_lidar_position(const glm::vec3& position);
    void set_lidar_rotation(const glm::quat& rotation);
    void set_lidar_transform(const Transform& lidar_transform);

private:
    glm::vec3 rear_axle_bottom_offset() const;
    glm::vec3 rear_axle_world_offset() const;
    glm::vec3 lidar_world_offset() const;
    glm::quat parent_rotation_from_lidar_mount_rotation(const glm::quat& lidar_rotation) const;

    RenderObject m_compartment_1;
    RenderObject m_compartment_2;
    RenderObject m_compartment_connector;
    RenderObject m_compartment_3;

    RenderObject m_left_rear_wheel;
    RenderObject m_right_rear_wheel;
    RenderObject m_left_front_wheel;
    RenderObject m_right_front_wheel;

    SceneObject m_lidar_mount;
    RenderObject m_lidar;
};
