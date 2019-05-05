#include "vulkan_methods.hpp"

#include <thread>
#include <random>
#include <algorithm>

#define MAX_FRAMES_IN_FLIGHT 2

std::array<game::Vertex, 6> vertices = {
    game::Vertex { 0, 0 },
    game::Vertex { 0, 1 },
    game::Vertex { 1, 0 },
    game::Vertex { 0, 1 },
    game::Vertex { 1, 0 },
    game::Vertex { 1, 1 }
};

struct GameData {
    game::Camera* camera;
    uint32_t grid_size;
};

void keyCallback(GLFWwindow * window, int key, int scancode, int action, int mods) {
    GameData* data = static_cast<GameData*>(glfwGetWindowUserPointer(window));
    if (key == GLFW_KEY_ESCAPE && action == GLFW_PRESS) {
        glfwSetWindowShouldClose(window, GLFW_TRUE);
    } else if (key == GLFW_KEY_LEFT && (action == GLFW_PRESS || action == GLFW_REPEAT)) {
        data->camera->x -= std::min(data->camera->x - 5.f, 0.f);
    } else if (key == GLFW_KEY_RIGHT && (action == GLFW_PRESS || action == GLFW_REPEAT)) {
        data->camera->x -= std::max(data->camera->x + 5.f, static_cast<float>(data->grid_size));
    } else if (key == GLFW_KEY_UP && (action == GLFW_PRESS || action == GLFW_REPEAT)) {
        data->camera->x -= std::max(data->camera->y + 5.f, static_cast<float>(data->grid_size));
    } else if (key == GLFW_KEY_DOWN && (action == GLFW_PRESS || action == GLFW_REPEAT)) {
        data->camera->x -= std::min(data->camera->y - 5.f, 0.f);
    }
}

void rebuildSwapchain(
    vk::PhysicalDevice physical_device,
    vk::Device device,
    vk::SurfaceKHR surface,
    vk::DispatchLoaderDynamic dispatcher,
    uint32_t grid_size,
    vk::Extent2D window_extent,
    game::Queue graphics_queue,
    game::Queue present_queue,
    game::Queue compute_queue,
    vk::DeviceMemory device_memory,
    vk::DescriptorSetLayout descriptor_set_layout,
    vk::CommandPool graphics_command_pool,
    std::vector<game::Buffer> game_buffers,
    game::Buffer vertex_buffer,

    vk::SwapchainKHR& swapchain,
    vk::SurfaceFormatKHR surface_format,
    std::vector<vk::Image>& swapchain_images,
    std::vector<vk::ImageView>& swapchain_image_views,
    vk::DescriptorPool& descriptor_pool,
    std::vector<game::Buffer>& camera_buffers,
    vk::RenderPass& graphics_render_pass,
    vk::Pipeline& graphics_pipeline,
    vk::PipelineLayout& graphics_pipeline_layout,
    std::vector<vk::DescriptorSet>& uniform_sets,
    std::vector<vk::Framebuffer>& framebuffers,
    std::vector<vk::CommandBuffer>& command_buffers
) {
    vkDeviceWaitIdle(device);

    device.freeCommandBuffers(graphics_command_pool, command_buffers);
    for (auto fb : framebuffers) {
        device.destroyFramebuffer(fb);
    }
    device.destroyPipeline(graphics_pipeline);
    device.destroyPipelineLayout(graphics_pipeline_layout);
    device.destroyRenderPass(graphics_render_pass);
    for (game::Buffer b : camera_buffers) {
        device.destroyBuffer(b.buffer);
    }
    device.destroyDescriptorPool(descriptor_pool);
    for (auto siv : swapchain_image_views) {
        device.destroyImageView(siv);
    }
    device.destroySwapchainKHR(swapchain, nullptr, dispatcher);

    game::createSwapchain(
        physical_device,
        device,
        surface,
        window_extent,
        dispatcher,
        { graphics_queue.index.value(), present_queue.index.value() },
        surface_format,
        swapchain
    );
    swapchain_images = device.getSwapchainImagesKHR(swapchain, dispatcher);
    for (uint32_t i = 0; i < swapchain_image_views.size(); i++) {
        game::createImageView(
            device,
            swapchain_images[i],
            surface_format.format,
            swapchain_image_views[i]
        );
    }
    game::createDescriptorPool(
        device,
        swapchain_images.size(),
        descriptor_pool
    );
    for (uint32_t i = 0; i < camera_buffers.size(); i++) {
        game::createBuffer(
            device,
            sizeof(game::Camera),
            { graphics_queue.index.value(), compute_queue.index.value() },
            vk::BufferUsageFlagBits::eUniformBuffer,
            camera_buffers[i].buffer
        );
        device.bindBufferMemory(
            camera_buffers[i].buffer,
            device_memory,
            camera_buffers[i].offset
        );
    }
    game::createRenderpass(
        device,
        surface_format.format,
        graphics_render_pass
    );
    std::vector<vk::VertexInputAttributeDescription> vertex_attributes;
    {
        auto vertex_attr = game::Vertex::getAttributeDescriptions();
        vertex_attributes.insert(vertex_attributes.end(), vertex_attr.begin(), vertex_attr.end());
        auto cell_attr = game::Cell::getAttributeDescriptions();
        vertex_attributes.insert(vertex_attributes.end(), cell_attr.begin(), cell_attr.end());
    }
    game::createGraphicsPipeline(
        device,
        grid_size,
        window_extent,
        { descriptor_set_layout },
        { game::Vertex::getBindingDescription(), game::Cell::getBindingDescription() },
        vertex_attributes,
        graphics_render_pass,
        graphics_pipeline_layout,
        graphics_pipeline
    );
    game::createDescriptorSets(
        device,
        { descriptor_set_layout, descriptor_set_layout, descriptor_set_layout },
        descriptor_pool,
        camera_buffers,
        uniform_sets
    );
    for (uint32_t i = 0; i < framebuffers.size(); i++) {
        game::createFramebuffer(
            device,
            window_extent,
            { swapchain_image_views[i] },
            graphics_render_pass,
            framebuffers[i]
        );
    }
    game::createCommandBuffers(
        device,
        graphics_command_pool,
        swapchain_images.size(),
        graphics_render_pass,
        framebuffers,
        window_extent,
        graphics_pipeline,
        graphics_pipeline_layout,
        vertex_buffer,
        game_buffers,
        grid_size,
        uniform_sets,
        command_buffers
    );
}

int main(int argc, char** argv) {
    glfwInit();

    uint32_t grid_size = 1000;
    if (argc == 2) {
        grid_size = std::atoll(argv[1]);
    }

    vk::Instance instance;
    vk::DispatchLoaderDynamic dispatcher;
    vk::DebugUtilsMessengerEXT debug_utils;
    game::createInstance(instance, dispatcher, debug_utils);

    glfwWindowHint(GLFW_CLIENT_API, GLFW_NO_API);
    GLFWwindow* window = glfwCreateWindow(800, 600, "Game of Life", nullptr, nullptr);
    int window_width, window_height;
    glfwGetFramebufferSize(window, &window_width, &window_height);
    vk::SurfaceKHR surface;
    game::createSurface(instance, window, surface);

    vk::PhysicalDevice physical_device;
    vk::Device device;
    game::Queue graphics_queue, present_queue, compute_queue;
    game::createDevice(instance, surface, dispatcher, physical_device, device, graphics_queue, present_queue, compute_queue);

    game::Camera camera {
        grid_size / 2.f,
        grid_size / 2.f,
        10.
    };

    vk::SwapchainKHR swapchain;
    vk::SurfaceFormatKHR surface_format;
    game::createSwapchain(
        physical_device,
        device,
        surface,
        vk::Extent2D { static_cast<uint32_t>(window_width), static_cast<uint32_t>(window_height) },
        dispatcher,
        { graphics_queue.index.value(), present_queue.index.value() },
        surface_format,
        swapchain
    );
    std::vector<vk::Image> swapchain_images = device.getSwapchainImagesKHR(swapchain, dispatcher);
    std::vector<vk::ImageView> swapchain_image_views(swapchain_images.size());
    for (uint32_t i = 0; i < swapchain_image_views.size(); i++) {
        game::createImageView(
            device,
            swapchain_images[i],
            surface_format.format,
            swapchain_image_views[i]
        );
    }

    std::vector<vk::Semaphore> image_available(MAX_FRAMES_IN_FLIGHT);
    std::vector<vk::Semaphore> render_complete(MAX_FRAMES_IN_FLIGHT);
    std::vector<vk::Semaphore> compute_complete(MAX_FRAMES_IN_FLIGHT);
    std::vector<vk::Fence> frame_in_flight(MAX_FRAMES_IN_FLIGHT);
    {
        vk::SemaphoreCreateInfo semaphore_info = vk::SemaphoreCreateInfo();
        vk::FenceCreateInfo fence_info = vk::FenceCreateInfo()
            .setFlags(vk::FenceCreateFlagBits::eSignaled);

        for (uint64_t i = 0; i < MAX_FRAMES_IN_FLIGHT; i++) {
            image_available[i] = device.createSemaphore(semaphore_info);
            render_complete[i] = device.createSemaphore(semaphore_info);
            compute_complete[i] = device.createSemaphore(semaphore_info);
            frame_in_flight[i] = device.createFence(fence_info);
        }
    }

    vk::DescriptorSetLayout descriptor_set_layout;
    game::createDescriptorSetLayout(device, descriptor_set_layout);

    vk::DescriptorPool descriptor_pool;
    game::createDescriptorPool(
        device,
        swapchain_images.size(),
        descriptor_pool
    );

    vk::CommandPool graphics_command_pool;
    vk::CommandPool compute_command_pool;
    game::createCommandPools(
        device,
        graphics_queue.index.value(),
        compute_queue.index.value(),
        graphics_command_pool,
        compute_command_pool
    );

    std::vector<game::Buffer> game_buffers(5);
    game::Buffer vertex_buffer;
    std::vector<game::Buffer> camera_buffers(swapchain_images.size());
    for (uint32_t i = 0; i < game_buffers.size(); i++) {
        game::createBuffer(
            device,
            grid_size * grid_size * sizeof(game::Cell),
            { graphics_queue.index.value(), compute_queue.index.value() },
            vk::BufferUsageFlagBits::eVertexBuffer,
            game_buffers[i].buffer
        );
    }
    game::createBuffer(
        device,
        vertices.size() * sizeof(game::Vertex),
        { graphics_queue.index.value(), compute_queue.index.value() },
        vk::BufferUsageFlagBits::eVertexBuffer,
        vertex_buffer.buffer
    );
    for (uint32_t i = 0; i < camera_buffers.size(); i++) {
        game::createBuffer(
            device,
            sizeof(game::Camera),
            { graphics_queue.index.value(), compute_queue.index.value() },
            vk::BufferUsageFlagBits::eUniformBuffer,
            camera_buffers[i].buffer
        );
    }

    vk::DeviceSize memory_req = 0;
    for (uint32_t i = 0; i < game_buffers.size(); i++) {
        game_buffers[i].mem_reqs = device.getBufferMemoryRequirements(game_buffers[i].buffer);
        memory_req += game_buffers[i].mem_reqs.size + game_buffers[i].mem_reqs.alignment;
    }
    vertex_buffer.mem_reqs = device.getBufferMemoryRequirements(vertex_buffer.buffer);
    memory_req += vertex_buffer.mem_reqs.size + vertex_buffer.mem_reqs.alignment;
    for (uint32_t i = 0; i < camera_buffers.size(); i++) {
        camera_buffers[i].mem_reqs = device.getBufferMemoryRequirements(camera_buffers[i].buffer);
        memory_req += camera_buffers[i].mem_reqs.size + camera_buffers[i].mem_reqs.alignment;
    }

    vk::DeviceMemory device_memory;
    uint32_t device_memory_type_index;
    game::createDeviceMemory(device, physical_device, memory_req, device_memory_type_index, device_memory);
    for (auto gbr : game_buffers) {
        assert(bool(gbr.mem_reqs.memoryTypeBits & (1 << (device_memory_type_index - 1))));
    }
    assert(bool(vertex_buffer.mem_reqs.memoryTypeBits & (1 << (device_memory_type_index - 1))));
    for (auto cbr : camera_buffers) {
        assert(bool(cbr.mem_reqs.memoryTypeBits & (1 << (device_memory_type_index - 1))));
    }

    vk::DeviceSize memory_offset = 0;
    for (uint32_t i = 0; i < game_buffers.size(); i++) {
        if (memory_offset % game_buffers[i].mem_reqs.alignment) {
            memory_offset += (game_buffers[i].mem_reqs.alignment - (memory_offset % game_buffers[i].mem_reqs.alignment));
        }
        device.bindBufferMemory(
            game_buffers[i].buffer,
            device_memory,
            memory_offset
        );
        game_buffers[i].offset = memory_offset;
        memory_offset += game_buffers[i].mem_reqs.size;
    }
    {
        // std::random_device rd;
        // std::mt19937 generator(rd());
        // std::bernoulli_distribution bernoulli(0.5);
        game::Cell* mapped_memory = static_cast<game::Cell*>(device.mapMemory(device_memory, 0, game_buffers[0].mem_reqs.size));
        // uint32_t tenth = grid_size/10;
        // for (uint32_t i = 0; i < tenth; i++) {
        //     for (uint32_t j = 0; j < tenth; j++) {
        //         mapped_memory[(((grid_size/2) - (tenth/2) + i) * grid_size) + ((grid_size/2) - (tenth/2) + j)].alive = bernoulli(generator);
        //     }
        // }
        for (uint32_t i = 0; i < grid_size; i++) {
            for (uint32_t j = 0; j < grid_size; j++) {
                mapped_memory[i * grid_size + j].x = i;
                mapped_memory[i * grid_size + j].y = j;
                mapped_memory[i * grid_size + j].alive = (i + j) % 2;
            }
        }
        device.unmapMemory(device_memory);
    }

    if (memory_offset % vertex_buffer.mem_reqs.alignment) {
        memory_offset += (vertex_buffer.mem_reqs.alignment - (memory_offset % vertex_buffer.mem_reqs.alignment));
    }
    device.bindBufferMemory(
        vertex_buffer.buffer,
        device_memory,
        memory_offset
    );
    vertex_buffer.offset = memory_offset;
    {
        game::Vertex* mapped_memory = static_cast<game::Vertex*>(device.mapMemory(device_memory, memory_offset, vertex_buffer.mem_reqs.size));
        for (uint32_t i = 0; i < vertices.size(); i++) {
            mapped_memory[i] = vertices[i];
        }
        device.unmapMemory(device_memory);
    }
    memory_offset += vertex_buffer.mem_reqs.size;

    for (uint32_t i = 0; i < camera_buffers.size(); i++) {
        if (memory_offset % camera_buffers[i].mem_reqs.alignment) {
            memory_offset += (camera_buffers[i].mem_reqs.alignment - (memory_offset % camera_buffers[i].mem_reqs.alignment));
        }
        device.bindBufferMemory(
            camera_buffers[i].buffer,
            device_memory,
            memory_offset
        );
        camera_buffers[i].offset = memory_offset;
        game::Camera* mapped_memory = reinterpret_cast<game::Camera*>(device.mapMemory(device_memory, memory_offset, camera_buffers[i].mem_reqs.size));
        *mapped_memory = camera;
        device.unmapMemory(device_memory);
        memory_offset += camera_buffers[i].mem_reqs.size;
    }

    vk::RenderPass graphics_render_pass;
    game::createRenderpass(
        device,
        surface_format.format,
        graphics_render_pass
    );

    std::vector<vk::VertexInputAttributeDescription> vertex_attributes;
    {
        auto vertex_attr = game::Vertex::getAttributeDescriptions();
        vertex_attributes.insert(vertex_attributes.end(), vertex_attr.begin(), vertex_attr.end());
        auto cell_attr = game::Cell::getAttributeDescriptions();
        vertex_attributes.insert(vertex_attributes.end(), cell_attr.begin(), cell_attr.end());
    }
    vk::Pipeline graphics_pipeline;
    vk::PipelineLayout graphics_pipeline_layout;
    game::createGraphicsPipeline(
        device,
        grid_size,
        vk::Extent2D { static_cast<uint32_t>(window_width), static_cast<uint32_t>(window_height) },
        { descriptor_set_layout },
        { game::Vertex::getBindingDescription(), game::Cell::getBindingDescription() },
        vertex_attributes,
        graphics_render_pass,
        graphics_pipeline_layout,
        graphics_pipeline
    );

    std::vector<vk::DescriptorSet> uniform_sets;
    game::createDescriptorSets(
        device,
        { descriptor_set_layout, descriptor_set_layout, descriptor_set_layout },
        descriptor_pool,
        camera_buffers,
        uniform_sets
    );

    std::vector<vk::Framebuffer> framebuffers(swapchain_images.size());
    for (uint32_t i = 0; i < framebuffers.size(); i++) {
        game::createFramebuffer(
            device,
            { static_cast<uint32_t>(window_width), static_cast<uint32_t>(window_height) },
            { swapchain_image_views[i] },
            graphics_render_pass,
            framebuffers[i]
        );
    }

    std::vector<vk::CommandBuffer> command_buffers;
    game::createCommandBuffers(
        device,
        graphics_command_pool,
        swapchain_images.size(),
        graphics_render_pass,
        framebuffers,
        vk::Extent2D { static_cast<uint32_t>(window_width), static_cast<uint32_t>(window_height) },
        graphics_pipeline,
        graphics_pipeline_layout,
        vertex_buffer,
        game_buffers,
        grid_size,
        uniform_sets,
        command_buffers
    );

    GameData* game_data = new GameData {
        &camera,
        grid_size
    };
    glfwSetWindowUserPointer(window, game_data);
    glfwSetKeyCallback(window, keyCallback);
    glfwShowWindow(window);

    bool running = true;
    uint32_t current_frame = 0;
    while (running) {
        glfwPollEvents();

        // ###
        device.waitForFences({ frame_in_flight[current_frame] }, VK_TRUE, std::numeric_limits<uint64_t>::max());

        vk::ResultValue<uint32_t> result = device.acquireNextImageKHR(swapchain, std::numeric_limits<uint64_t>::max(), image_available[current_frame], vk::Fence());

        bool rebuild_swapchain = false;
        if (result.result == vk::Result::eErrorOutOfDateKHR) {
            window_width = 0;
            window_height = 0;
            while (window_width == 0 || window_height == 0) {
                glfwGetFramebufferSize(window, &window_width, &window_height);
                glfwWaitEvents();
            }
            rebuildSwapchain(
                physical_device,
                device,
                surface,
                dispatcher,
                grid_size,
                vk::Extent2D { static_cast<uint32_t>(window_width), static_cast<uint32_t>(window_height) },
                graphics_queue,
                present_queue,
                compute_queue,
                device_memory,
                descriptor_set_layout,
                graphics_command_pool,
                game_buffers,
                vertex_buffer,
                swapchain,
                surface_format,
                swapchain_images,
                swapchain_image_views,
                descriptor_pool,
                camera_buffers,
                graphics_render_pass,
                graphics_pipeline,
                graphics_pipeline_layout,
                uniform_sets,
                framebuffers,
                command_buffers
            );
        } else if (result.result != vk::Result::eSuccess && result.result != vk::Result::eSuboptimalKHR) {
            break;
        }

        uint32_t image_index = result.value;
        game::Camera* mapped_memory = static_cast<game::Camera*>(device.mapMemory(
            device_memory,
            camera_buffers[image_index].offset,
            camera_buffers[image_index].mem_reqs.size
        ));
        *mapped_memory = camera;
        device.unmapMemory(device_memory);

        std::vector<vk::Semaphore> wait_semaphores = { image_available[current_frame] };
        std::vector<vk::Semaphore> signal_semaphores = { render_complete[current_frame] };
        std::vector<vk::PipelineStageFlags> wait_stages = { vk::PipelineStageFlagBits::eColorAttachmentOutput };
        std::vector<vk::CommandBuffer> commands = { command_buffers[image_index] };

        vk::SubmitInfo submit_info = vk::SubmitInfo()
            .setWaitSemaphoreCount(wait_semaphores.size())
            .setPWaitSemaphores(wait_semaphores.data())
            .setPWaitDstStageMask(wait_stages.data())
            .setCommandBufferCount(commands.size())
            .setPCommandBuffers(commands.data())
            .setSignalSemaphoreCount(signal_semaphores.size())
            .setPSignalSemaphores(signal_semaphores.data());

        device.resetFences({ frame_in_flight[current_frame] });
        graphics_queue.queue.submit({ submit_info }, frame_in_flight[current_frame]);

        std::vector<vk::SwapchainKHR> swapchains = { swapchain };
        vk::PresentInfoKHR present_info = vk::PresentInfoKHR()
            .setSwapchainCount(swapchains.size())
            .setPSwapchains(swapchains.data())
            .setWaitSemaphoreCount(signal_semaphores.size())
            .setPWaitSemaphores(signal_semaphores.data())
            .setPImageIndices(&image_index);
        vk::Result present_result = present_queue.queue.presentKHR(present_info);
        if (present_result == vk::Result::eErrorOutOfDateKHR || present_result == vk::Result::eSuboptimalKHR) {
            window_width = 0;
            window_height = 0;
            while (window_width == 0 || window_height == 0) {
                glfwGetFramebufferSize(window, &window_width, &window_height);
                glfwWaitEvents();
            }
            rebuildSwapchain(
                physical_device,
                device,
                surface,
                dispatcher,
                grid_size,
                vk::Extent2D { static_cast<uint32_t>(window_width), static_cast<uint32_t>(window_height) },
                graphics_queue,
                present_queue,
                compute_queue,
                device_memory,
                descriptor_set_layout,
                graphics_command_pool,
                game_buffers,
                vertex_buffer,
                swapchain,
                surface_format,
                swapchain_images,
                swapchain_image_views,
                descriptor_pool,
                camera_buffers,
                graphics_render_pass,
                graphics_pipeline,
                graphics_pipeline_layout,
                uniform_sets,
                framebuffers,
                command_buffers
            );
        } else if (present_result != vk::Result::eSuccess) {
            break;
        }

        current_frame = (current_frame + 1) % MAX_FRAMES_IN_FLIGHT;

        if (glfwWindowShouldClose(window)) {
            running = false;
        }
    }

    vkDeviceWaitIdle(device);

    delete game_data;

    device.freeCommandBuffers(graphics_command_pool, command_buffers);
    for (auto fb : framebuffers) {
        device.destroyFramebuffer(fb);
    }
    device.destroyPipeline(graphics_pipeline);
    device.destroyPipelineLayout(graphics_pipeline_layout);
    device.destroyRenderPass(graphics_render_pass);
    for (game::Buffer b : camera_buffers) {
        device.destroyBuffer(b.buffer);
    }
    device.destroyBuffer(vertex_buffer.buffer);
    for (game::Buffer b : game_buffers) {
        device.destroyBuffer(b.buffer);
    }
    device.freeMemory(device_memory);
    if (compute_queue != graphics_queue) {
        device.destroyCommandPool(compute_command_pool);
    }
    device.destroyCommandPool(graphics_command_pool);
    device.destroyDescriptorPool(descriptor_pool);
    device.destroyDescriptorSetLayout(descriptor_set_layout);
    for (uint64_t i = 0; i < MAX_FRAMES_IN_FLIGHT; i++) {
        device.destroySemaphore(image_available[i]);
        device.destroySemaphore(render_complete[i]);
        device.destroySemaphore(compute_complete[i]);
        device.destroyFence(frame_in_flight[i]);
    }
    for (auto siv : swapchain_image_views) {
        device.destroyImageView(siv);
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