#include "point_cloud.h"

void PointCloud::create(VulkanEngine& engine, uint32_t num_points) {
    this->engine = &engine;

    num_points = num_points <= 0 ? 1 : num_points;

    instance_buffer = VideoBuffer(engine, num_points * sizeof(PointInstance));
    num_instances = num_points;
    external_instance_buffer = nullptr;
}

void PointCloud::set_points(const std::vector<PointInstance>& point_instances) {
    external_instance_buffer = nullptr;

    num_instances = static_cast<uint32_t>(point_instances.size());
    if (num_instances == 0) return;

    instance_buffer.update_data(
        point_instances.data(),
        point_instances.size() * sizeof(PointInstance)
    );
}

void PointCloud::set_points(VideoBuffer& point_instance_buffer, uint32_t num_instances) {
    this->num_instances = num_instances;
    external_instance_buffer = &point_instance_buffer;
}