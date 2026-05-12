#pragma once
#include "point_cloud/point_cloud_video.h"
#include <algorithm>
#include <cmath>
#include <vector>
#include <glm/common.hpp>
#include <glm/gtc/constants.hpp>


class GICPTestClouds {
public:
    GICPTestClouds() = default;
    void create_roads(VulkanEngine* engine);
    void create_points(VulkanEngine* engine);
    void generate_three_points(VulkanEngine* engine, PointCloudFrame* frame, glm::vec4 color, const glm::vec3& normal);
    void generate_single_point(VulkanEngine* engine, PointCloudFrame* frame, glm::vec4 color, const glm::vec3& position, const glm::vec3& normal);
    void generate_road(VulkanEngine* engine, PointCloudFrame* frame, glm::vec4 color, std::size_t target_point_count);
    void road_with_trees(VulkanEngine* engine, PointCloudFrame* frame, glm::vec4 color, std::size_t target_point_count);
    void trees_and_spheres(VulkanEngine* engine, PointCloudFrame* frame, glm::vec4 color, std::size_t target_point_count);
    
    void add_point(PointCloudFrame* frame, const glm::vec4& pos, const glm::vec4& color, const glm::vec4& normal);

    PointCloudFrame target_frame;
    PointCloudFrame source_frame;
};