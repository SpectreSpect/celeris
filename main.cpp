#include "vulkan_self/vulkan_engine.h"

int main() {
    GlfwContext glfw_context;
    Window window(glfw_context, 1280, 720, "Vulkan engine");

    QueueRequest queue_request;
    queue_request.graphics_count = 1;
    queue_request.present_count = 1;

    VulkanEngine engine(glfw_context, window, queue_request);
    engine.run();
}
