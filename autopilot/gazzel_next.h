#pragma once

#include "../renderer/scene_object.h"
#include "../renderer/render_object.h"
#include "../renderer/transform.h"

#include <glm/vec3.hpp>
#include <glm/gtc/quaternion.hpp>

class MeshManager;
class MaterialInstanceManager;

class GazzelNext : public SceneObject {
public:
    float car_length = 6.6f;

    float compartment_1_weight = 3.41f;
    float compartment_2_weight = 2.0f;
    float compartment_3_weight = 1.0f;

    float car_body_height = 2.30f;
    float compartment_1_height_weight = 2.30f;
    float compartment_2_height_weight = 1.7f;
    float compartment_3_height_weight = 1.0f;

    float axle_length = 1.805f;
    float rear_axle_x_portion = 0.2f;
    float front_axle_x_portion = 0.85f;

    glm::vec3 wheel_scale = glm::vec3(0.75f, 0.2f, 0.75f);

    GazzelNext(
        MeshManager& mesh_manager,
        MaterialInstanceManager& material_instance_manager,
        float skybox_exposure
    );

    void update_layout();
    void set_rear_axle_position(const glm::vec3& position);
    void set_rear_axle_rotation(const glm::quat& rotation);
    void set_rear_axle_transform(const Transform& rear_axle_transform);

private:
    glm::vec3 rear_axle_bottom_offset() const;
    glm::vec3 rear_axle_world_offset() const;

    RenderObject m_compartment_1;
    RenderObject m_compartment_2;
    RenderObject m_compartment_3;

    RenderObject m_left_rear_wheel;
    RenderObject m_right_rear_wheel;
    RenderObject m_left_front_wheel;
    RenderObject m_right_front_wheel;
};
