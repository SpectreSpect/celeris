#pragma once

#include <memory>

#include "../../vulkan_self/logger/logger_header.h"
#include "../instanced_render_object.h"

class InstanceBatch;
class VulkanEngine;
class Mesh;
class SlotPassInstance;
class LineInstance;

class LineCloud : public InstancedRenderObject {
public:
    _XCHILD_NAME(LineCloud);

    explicit LineCloud(VulkanEngine& engine, Mesh& mesh, SlotPassInstance& material, InstanceBatch&& instance_batch);
    explicit LineCloud(VulkanEngine& engine, Mesh& mesh, SlotPassInstance& material, InstanceBatch& instance_batch);
    explicit LineCloud(VulkanEngine& engine, Mesh& mesh, SlotPassInstance& material, uint32_t instance_count);
    explicit LineCloud(VulkanEngine& engine, Mesh& mesh, SlotPassInstance& material, const std::vector<LineInstance>& lines);
    ~LineCloud() noexcept override = default;

    LineCloud(const LineCloud&) = delete;
    LineCloud& operator=(const LineCloud&) = delete;

    LineCloud(LineCloud&&) noexcept = default;
    LineCloud& operator=(LineCloud&&) noexcept = default;

    void set_lines(const std::vector<LineInstance>& lines);

private:
    std::unique_ptr<InstanceBatch> m_instance_batch;
};
