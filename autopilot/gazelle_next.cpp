#include "gazelle_next.h"

#include "../managers/material_instance_manager.h"
#include "../managers/mesh_manager.h"
#include "../renderer/material_data_types.h"

#include <glm/gtc/constants.hpp>
#include <glm/gtc/quaternion.hpp>

GazelleNext::GazelleNext(
    MeshManager& mesh_manager,
    MaterialInstanceManager& material_instance_manager,
    float skybox_exposure)
    :   m_compartment_1(mesh_manager.cube, material_instance_manager.pbr),
        m_compartment_2(mesh_manager.cube, material_instance_manager.pbr),
        m_compartment_3(mesh_manager.cube, material_instance_manager.pbr),
        m_left_rear_wheel(mesh_manager.cylinder, material_instance_manager.pbr),
        m_right_rear_wheel(mesh_manager.cylinder, material_instance_manager.pbr),
        m_left_front_wheel(mesh_manager.cylinder, material_instance_manager.pbr),
        m_right_front_wheel(mesh_manager.cylinder, material_instance_manager.pbr)
{
    update_layout();

    m_compartment_1.set_material_data(PBRMaterialData::create(0.0f, 0.95f, skybox_exposure, glm::vec4(1.0f), 1.0f));
    m_compartment_2.set_material_data(PBRMaterialData::create(0.0f, 0.95f, skybox_exposure, glm::vec4(0.25f, 0.005f, 0.005f, 1.0f), 1.0f));
    m_compartment_3.set_material_data(PBRMaterialData::create(0.0f, 0.95f, skybox_exposure, glm::vec4(0.25f, 0.005f, 0.005f, 1.0f), 1.0f));

    const PBRMaterialData wheel_material = PBRMaterialData::create(0.0f, 0.95f, skybox_exposure, glm::vec4(0.01f, 0.01f, 0.01f, 1.0f), 1.0f);
    m_left_rear_wheel.set_material_data(wheel_material);
    m_right_rear_wheel.set_material_data(wheel_material);
    m_left_front_wheel.set_material_data(wheel_material);
    m_right_front_wheel.set_material_data(wheel_material);

    add_child(m_compartment_1);
    add_child(m_compartment_2);
    add_child(m_compartment_3);
    add_child(m_left_rear_wheel);
    add_child(m_right_rear_wheel);
    add_child(m_left_front_wheel);
    add_child(m_right_front_wheel);
}

void GazelleNext::update_layout() {
    const float compartment_weight_sum = compartment_1_weight + compartment_2_weight + compartment_3_weight;
    const float compartment_1_portion = compartment_1_weight / compartment_weight_sum;
    const float compartment_2_portion = compartment_2_weight / compartment_weight_sum;
    const float compartment_3_portion = compartment_3_weight / compartment_weight_sum;

    const float compartment_1_length = car_length * compartment_1_portion;
    const float compartment_2_length = car_length * compartment_2_portion;
    const float compartment_3_length = car_length * compartment_3_portion;
    const float car_rear_x = -car_length / 2.0f;

    const float compartment_height_scale = car_body_height / compartment_1_height_weight;
    const float compartment_1_height = compartment_1_height_weight * compartment_height_scale;
    const float compartment_2_height = compartment_2_height_weight * compartment_height_scale;
    const float compartment_3_height = compartment_3_height_weight * compartment_height_scale;
    const float car_body_bottom_y = -car_body_height / 2.0f;
    const float wheel_radius_y = wheel_scale.x * 0.5f;
    const float car_model_bottom_y = car_body_bottom_y - wheel_radius_y;
    const float car_model_top_y = car_body_bottom_y + car_body_height;
    const float car_model_center_y = (car_model_bottom_y + car_model_top_y) / 2.0f;

    m_compartment_1.transform.position = glm::vec3(car_rear_x + compartment_1_length / 2.0f, car_body_bottom_y + compartment_1_height / 2.0f - car_model_center_y, 0.0f);
    m_compartment_1.transform.scale = glm::vec3(compartment_1_length, compartment_1_height, 2.0f);

    m_compartment_2.transform.position = glm::vec3(car_rear_x + compartment_1_length + compartment_2_length / 2.0f, car_body_bottom_y + compartment_2_height / 2.0f - car_model_center_y, 0.0f);
    m_compartment_2.transform.scale = glm::vec3(compartment_2_length, compartment_2_height, 2.0f);

    m_compartment_3.transform.position = glm::vec3(car_rear_x + compartment_1_length + compartment_2_length + compartment_3_length / 2.0f, car_body_bottom_y + compartment_3_height / 2.0f - car_model_center_y, 0.0f);
    m_compartment_3.transform.scale = glm::vec3(compartment_3_length, compartment_3_height, 2.0f);

    const float rear_axle_x = car_rear_x + car_length * rear_axle_x_portion;
    const float front_axle_x = car_rear_x + car_length * front_axle_x_portion;
    const float wheel_y = car_body_bottom_y - car_model_center_y;
    const float left_wheel_z = -axle_length / 2.0f;
    const float right_wheel_z = axle_length / 2.0f;
    const glm::quat wheel_rotation = glm::angleAxis(glm::half_pi<float>(), glm::vec3(1.0f, 0.0f, 0.0f));

    m_left_rear_wheel.transform.rotation = wheel_rotation;
    m_left_rear_wheel.transform.scale = wheel_scale;
    m_left_rear_wheel.transform.position = glm::vec3(rear_axle_x, wheel_y, left_wheel_z);

    m_right_rear_wheel.transform.rotation = wheel_rotation;
    m_right_rear_wheel.transform.scale = wheel_scale;
    m_right_rear_wheel.transform.position = glm::vec3(rear_axle_x, wheel_y, right_wheel_z);

    m_left_front_wheel.transform.rotation = wheel_rotation;
    m_left_front_wheel.transform.scale = wheel_scale;
    m_left_front_wheel.transform.position = glm::vec3(front_axle_x, wheel_y, left_wheel_z);

    m_right_front_wheel.transform.rotation = wheel_rotation;
    m_right_front_wheel.transform.scale = wheel_scale;
    m_right_front_wheel.transform.position = glm::vec3(front_axle_x, wheel_y, right_wheel_z);
}

void GazelleNext::set_rear_axle_position(const glm::vec3& position) {
    transform.position = position - rear_axle_world_offset();
}

void GazelleNext::set_rear_axle_rotation(const glm::quat& rotation) {
    const glm::vec3 rear_axle_position = transform.position + rear_axle_world_offset();
    transform.rotation = glm::normalize(rotation);
    set_rear_axle_position(rear_axle_position);
}

void GazelleNext::set_rear_axle_transform(const Transform& rear_axle_transform) {
    transform.rotation = rear_axle_transform.rotation;
    transform.scale = rear_axle_transform.scale;
    set_rear_axle_position(rear_axle_transform.position);
}

glm::vec3 GazelleNext::rear_axle_world_offset() const {
    return glm::normalize(transform.rotation) * (rear_axle_bottom_offset() * transform.scale);
}

glm::vec3 GazelleNext::rear_axle_bottom_offset() const {
    const float car_rear_x = -car_length / 2.0f;

    const float compartment_height_scale = car_body_height / compartment_1_height_weight;
    const float compartment_1_height = compartment_1_height_weight * compartment_height_scale;
    const float car_body_bottom_y = -car_body_height / 2.0f;
    const float wheel_radius_y = wheel_scale.x * 0.5f;
    const float car_model_bottom_y = car_body_bottom_y - wheel_radius_y;
    const float car_model_top_y = car_body_bottom_y + car_body_height;
    const float car_model_center_y = (car_model_bottom_y + car_model_top_y) / 2.0f;

    const float rear_axle_x = car_rear_x + car_length * rear_axle_x_portion;
    const float wheel_center_y = car_body_bottom_y - car_model_center_y;
    const float wheel_bottom_y = wheel_center_y - wheel_radius_y;

    return glm::vec3(rear_axle_x, wheel_bottom_y, 0.0f);
}
