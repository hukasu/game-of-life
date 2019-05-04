#include "vulkan_methods.hpp"

#include <thread>

std::array<game::Vertex, 6> vertices = {
    game::Vertex { 0, 0 },
    game::Vertex { 0, 1 },
    game::Vertex { 1, 0 },
    game::Vertex { 0, 1 },
    game::Vertex { 1, 1 },
    game::Vertex { 1, 0 }
};

int main(int argc, char** argv) {
    glfwInit();

    uint64_t grid_size = 1000;
    if (argc == 2) {
        grid_size = std::atoll(argv[1]);
    }

    vk::Instance instance;
    vk::DispatchLoaderDynamic dispatcher;
    vk::DebugUtilsMessengerEXT debug_utils;
    game::createInstance(instance, dispatcher, debug_utils);

    glfwWindowHint(GLFW_CLIENT_API, GLFW_NO_API);
    GLFWwindow* window = glfwCreateWindow(800, 600, "Game of Life", nullptr, nullptr);
    vk::SurfaceKHR surface;
    game::createSurface(instance, window, surface);

    vk::PhysicalDevice physical_device;
    vk::Device device;
    game::Queue graphics_queue, present_queue, compute_queue;
    game::createDevice(instance, surface, dispatcher, physical_device, device, graphics_queue, present_queue, compute_queue);

    vk::SwapchainKHR swapchain;
    game::createSwapchain(
        physical_device,
        device,
        surface,
        window,
        dispatcher,
        { graphics_queue.index.value(), present_queue.index.value() },
        swapchain
    );
    std::vector<vk::Image> swapchain_images = device.getSwapchainImagesKHR(swapchain, dispatcher);

    std::vector<vk::Semaphore> image_available(swapchain_images.size()), render_complete(swapchain_images.size()), compute_complete(swapchain_images.size());
    {
        vk::SemaphoreCreateInfo semaphore_info = vk::SemaphoreCreateInfo();

        for (uint64_t i = 0; i < swapchain_images.size(); i++) {
            image_available[i] = device.createSemaphore(semaphore_info);
            render_complete[i] = device.createSemaphore(semaphore_info);
            compute_complete[i] = device.createSemaphore(semaphore_info);
        }
    }

    vk::DescriptorSetLayout descriptor_set_layout;
    game::createDescriptorSetLayout(device, descriptor_set_layout);

    vk::DescriptorPool descriptor_pool;
    game::createDescriptorPool(device, descriptor_pool);

    vk::CommandPool graphics_command_pool;
    vk::CommandPool compute_command_pool;
    game::createCommandPools(
        device,
        graphics_queue.index.value(),
        compute_queue.index.value(),
        graphics_command_pool,
        compute_command_pool
    );

    std::vector<vk::Buffer> game_buffers(swapchain_images.size());
    for (uint32_t i = 0; i < game_buffers.size(); i++) {
        game::createBuffer(
            device,
            grid_size * grid_size * sizeof(game::Cell),
            { graphics_queue.index.value(), compute_queue.index.value() },
            vk::BufferUsageFlagBits::eVertexBuffer,
            game_buffers[i]
        );
    }
    vk::Buffer vertex_buffer;
    game::createBuffer(
        device,
        vertices.size() * sizeof(game::Vertex),
        { graphics_queue.index.value(), compute_queue.index.value() },
        vk::BufferUsageFlagBits::eVertexBuffer,
        vertex_buffer
    );
    vk::Buffer camera_buffer;
    game::createBuffer(
        device,
        sizeof(game::Camera),
        { graphics_queue.index.value(), compute_queue.index.value() },
        vk::BufferUsageFlagBits::eUniformBuffer,
        camera_buffer
    );

    vk::DeviceSize memory_req;
    std::vector<vk::MemoryRequirements> game_buffers_reqs(game_buffers.size());
    for (uint32_t i = 0; i < game_buffers.size(); i++) {
        game_buffers_reqs[i] = device.getBufferMemoryRequirements(game_buffers[i]);
        memory_req += game_buffers_reqs[i].size + game_buffers_reqs[i].alignment;
    }
    vk::MemoryRequirements vertex_buffer_reqs, camera_buffer_reqs;
    vertex_buffer_reqs = device.getBufferMemoryRequirements(vertex_buffer);
    memory_req += vertex_buffer_reqs.size + vertex_buffer_reqs.alignment;
    camera_buffer_reqs = device.getBufferMemoryRequirements(camera_buffer);
    memory_req += camera_buffer_reqs.size + camera_buffer_reqs.alignment;

    vk::DeviceMemory device_memory;
    game::createDeviceMemory(device, physical_device, memory_req, device_memory);
    // ###

    device.freeMemory(device_memory);
    device.destroyBuffer(camera_buffer);
    device.destroyBuffer(vertex_buffer);
    for (vk::Buffer b : game_buffers) {
        device.destroyBuffer(b);
    }
    if (compute_queue != graphics_queue) {
        device.destroyCommandPool(compute_command_pool);
    }
    device.destroyCommandPool(graphics_command_pool);
    device.destroyDescriptorPool(descriptor_pool);
    device.destroyDescriptorSetLayout(descriptor_set_layout);
    for (uint64_t i = 0; i < swapchain_images.size(); i++) {
        device.destroySemaphore(image_available[i]);
        device.destroySemaphore(render_complete[i]);
        device.destroySemaphore(compute_complete[i]);
    }
    device.destroySwapchainKHR(swapchain, nullptr, dispatcher);
    device.destroy();
    instance.destroySurfaceKHR(surface, nullptr, dispatcher);
    instance.destroyDebugUtilsMessengerEXT(debug_utils, nullptr, dispatcher);
    instance.destroy();

    glfwDestroyWindow(window);

    glfwTerminate();

    return 0;
}