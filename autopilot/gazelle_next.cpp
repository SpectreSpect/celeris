#include "gazelle_next.h"

#include "../managers/material_instance_manager.h"
#include "../managers/mesh_manager.h"
#include "../renderer/material_data_types.h"

#include <algorithm>
#include <glm/gtc/constants.hpp>
#include <glm/gtc/quaternion.hpp>

GazelleNext::GazelleNext(
    MeshManager& mesh_manager,
    MaterialInstanceManager& material_instance_manager,
    const VehicleGeometry& vehicle_geometry,
    float skybox_exposure)
    :   m_vehicle_geometry(vehicle_geometry),
        m_compartment_1(mesh_manager.cube, material_instance_manager.pbr),
        m_compartment_2(mesh_manager.cube, material_instance_manager.pbr),
        m_compartment_connector(mesh_manager.cube, material_instance_manager.pbr),
        m_compartment_3(mesh_manager.cube, material_instance_manager.pbr),
        m_left_rear_wheel(mesh_manager.cylinder, material_instance_manager.pbr),
        m_right_rear_wheel(mesh_manager.cylinder, material_instance_manager.pbr),
        m_left_front_wheel(mesh_manager.cylinder, material_instance_manager.pbr),
        m_right_front_wheel(mesh_manager.cylinder, material_instance_manager.pbr),
        m_lidar(mesh_manager.cylinder, material_instance_manager.pbr)
{
    update_layout();

    m_compartment_1.set_material_data(PBRMaterialData::create(0.0f, 0.95f, skybox_exposure, glm::vec4(1.0f), 1.0f));
    m_compartment_2.set_material_data(PBRMaterialData::create(0.0f, 0.95f, skybox_exposure, glm::vec4(0.25f, 0.005f, 0.005f, 1.0f), 1.0f));
    const PBRMaterialData body_red_material = PBRMaterialData::create(0.0f, 0.95f, skybox_exposure, glm::vec4(0.25f, 0.005f, 0.005f, 1.0f), 1.0f);
    m_compartment_connector.set_material_data(body_red_material);
    m_compartment_3.set_material_data(body_red_material);

    const PBRMaterialData wheel_material = PBRMaterialData::create(0.0f, 0.95f, skybox_exposure, glm::vec4(0.01f, 0.01f, 0.01f, 1.0f), 1.0f);
    m_left_rear_wheel.set_material_data(wheel_material);
    m_right_rear_wheel.set_material_data(wheel_material);
    m_left_front_wheel.set_material_data(wheel_material);
    m_right_front_wheel.set_material_data(wheel_material);

    m_lidar.set_material_data(PBRMaterialData::create(0.0f, 0.95f, skybox_exposure, glm::vec4(0.015f, 0.015f, 0.015f, 1.0f), 1.0f));

    add_child(m_compartment_1);
    add_child(m_compartment_2);
    add_child(m_compartment_connector);
    add_child(m_compartment_3);
    add_child(m_left_rear_wheel);
    add_child(m_right_rear_wheel);
    add_child(m_left_front_wheel);
    add_child(m_right_front_wheel);
    add_child(m_lidar_mount);
    m_lidar_mount.add_child(m_lidar);
}

void GazelleNext::update_layout() {
    const float compartment_weight_sum = compartment_1_weight + compartment_2_weight + compartment_3_weight;
    const float compartment_1_portion = compartment_1_weight / compartment_weight_sum;
    const float car_length = m_vehicle_geometry.size.z;
    const float car_width = m_vehicle_geometry.size.x;
    const float car_body_height = m_vehicle_geometry.size.y;

    const float compartment_1_length = car_length * compartment_1_portion;
    const float compartment_3_actual_length = std::clamp(compartment_3_length, 0.0f, car_length - compartment_1_length);
    const float compartment_2_length = car_length - compartment_1_length - compartment_3_actual_length;
    const float car_rear_x = -car_length / 2.0f;

    const float wheel_radius_y = m_vehicle_geometry.wheel_radius;
    const glm::vec3 wheel_scale(
        m_vehicle_geometry.wheel_radius * 2.0f,
        wheel_depth,
        m_vehicle_geometry.wheel_radius * 2.0f
    );
    const float car_model_bottom_y = 0.0f;
    const float wheel_center_y = car_model_bottom_y + wheel_radius_y;
    const float compartment_1_height = std::max(0.0f, compartment_1_top_height_from_rear_wheel_center);
    const float compartment_2_height = std::max(0.0f, compartment_2_top_height_from_rear_wheel_center);
    const float compartment_3_actual_height = std::max(0.0f, compartment_3_top_height_from_rear_wheel_center);
    const float car_model_top_y = car_model_bottom_y + car_body_height;
    const float car_model_center_y = (car_model_bottom_y + car_model_top_y) / 2.0f;

    m_compartment_1.transform.position = glm::vec3(car_rear_x + compartment_1_length / 2.0f, wheel_center_y + compartment_1_height / 2.0f - car_model_center_y, 0.0f);
    m_compartment_1.transform.scale = glm::vec3(compartment_1_length, compartment_1_height, car_width);

    m_compartment_2.transform.position = glm::vec3(car_rear_x + compartment_1_length + compartment_2_length / 2.0f, wheel_center_y + compartment_2_height / 2.0f - car_model_center_y, 0.0f);
    m_compartment_2.transform.scale = glm::vec3(compartment_2_length, compartment_2_height, car_width);

    m_compartment_3.transform.position = glm::vec3(car_rear_x + compartment_1_length + compartment_2_length + compartment_3_actual_length / 2.0f, wheel_center_y + compartment_3_actual_height / 2.0f - car_model_center_y, 0.0f);
    m_compartment_3.transform.scale = glm::vec3(compartment_3_actual_length, compartment_3_actual_height, car_width);

    const float car_front_x = car_rear_x + car_length;
    const float compartment_2_front_x = car_rear_x + compartment_1_length + compartment_2_length;
    const float compartment_2_top_y = wheel_center_y + compartment_2_height - car_model_center_y;
    const float compartment_3_top_y = wheel_center_y + compartment_3_actual_height - car_model_center_y;
    const float connector_end_x =
        car_front_x - compartment_3_actual_length * connector_end_offset_from_car_front_portion;
    const glm::vec2 connector_start(compartment_2_front_x, compartment_2_top_y);
    const glm::vec2 connector_end(connector_end_x, compartment_3_top_y);
    const glm::vec2 connector_delta = connector_end - connector_start;
    const float connector_angle = std::atan2(connector_delta.y, connector_delta.x);
    const float connector_span = glm::length(connector_delta);

    const glm::vec2 connector_center = (connector_start + connector_end) / 2.0f;
    const glm::vec2 connector_inset(
        std::sin(connector_angle),
        -std::cos(connector_angle)
    );

    m_compartment_connector.transform.position = glm::vec3(
        connector_center + connector_inset * (connector_thickness / 2.0f),
        0.0f
    );
    m_compartment_connector.transform.rotation = glm::angleAxis(
        connector_angle,
        glm::vec3(0.0f, 0.0f, 1.0f)
    );
    m_compartment_connector.transform.scale = glm::vec3(
        connector_span,
        connector_thickness,
        car_width
    );

    const float rear_axle_x = car_rear_x + m_vehicle_geometry.rear_axle_from_rear;
    const float front_axle_x = car_rear_x + m_vehicle_geometry.front_axle_from_rear;
    const float wheel_y = wheel_center_y - car_model_center_y;
    const float left_wheel_z = -m_vehicle_geometry.axle_track_width / 2.0f;
    const float right_wheel_z = m_vehicle_geometry.axle_track_width / 2.0f;
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

    const float lidar_x = car_rear_x + m_vehicle_geometry.lidar_from_rear;
    const float lidar_y = car_model_bottom_y + m_vehicle_geometry.lidar_height - car_model_center_y;
    const float lidar_z = m_vehicle_geometry.lidar_from_left - car_width / 2.0f;

    m_lidar_mount.transform.position = glm::vec3(lidar_x, lidar_y + lidar_scale.y / 2.0f, lidar_z);
    m_lidar_mount.transform.rotation = glm::angleAxis(
        glm::pi<float>(),
        glm::vec3(0.0f, 1.0f, 0.0f)
    );
    m_lidar_mount.transform.scale = glm::vec3(1.0f);

    m_lidar.transform.position = glm::vec3(0.0f);
    m_lidar.transform.rotation = glm::quat(1.0f, 0.0f, 0.0f, 0.0f);
    m_lidar.transform.scale = lidar_scale;
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

void GazelleNext::set_lidar_position(const glm::vec3& position) {
    transform.position = position - lidar_world_offset();
}

void GazelleNext::set_lidar_rotation(const glm::quat& rotation) {
    const glm::vec3 lidar_position = transform.position + lidar_world_offset();
    transform.rotation = parent_rotation_from_lidar_mount_rotation(rotation);
    set_lidar_position(lidar_position);
}

void GazelleNext::set_lidar_transform(const Transform& lidar_transform) {
    transform.rotation = parent_rotation_from_lidar_mount_rotation(lidar_transform.rotation);
    transform.scale = lidar_transform.scale;
    set_lidar_position(lidar_transform.position);
}

glm::vec3 GazelleNext::rear_axle_world_offset() const {
    return glm::normalize(transform.rotation) * (rear_axle_bottom_offset() * transform.scale);
}

glm::vec3 GazelleNext::lidar_world_offset() const {
    return glm::normalize(transform.rotation) * (m_lidar_mount.transform.position * transform.scale);
}

glm::quat GazelleNext::parent_rotation_from_lidar_mount_rotation(const glm::quat& lidar_rotation) const {
    return glm::normalize(lidar_rotation * glm::inverse(glm::normalize(m_lidar_mount.transform.rotation)));
}

glm::vec3 GazelleNext::rear_axle_bottom_offset() const {
    const float car_length = m_vehicle_geometry.size.z;
    const float car_body_height = m_vehicle_geometry.size.y;
    const float car_rear_x = -car_length / 2.0f;

    const float car_model_bottom_y = 0.0f;
    const float car_model_top_y = car_model_bottom_y + car_body_height;
    const float car_model_center_y = (car_model_bottom_y + car_model_top_y) / 2.0f;

    const float rear_axle_x = car_rear_x + m_vehicle_geometry.rear_axle_from_rear;
    const float wheel_bottom_y = car_model_bottom_y - car_model_center_y;

    return glm::vec3(rear_axle_x, wheel_bottom_y, 0.0f);
}
