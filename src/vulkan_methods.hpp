#ifndef __VULKAN__METHODS__HPP__
#define __VULKAN__METHODS__HPP__

#include <set>
#include <vector>
#include <array>
#include <iostream>
#include <optional>

#include <vulkan/vulkan.hpp>
#include <GLFW/glfw3.h>

namespace game {
    struct Vertex {
        struct {
            float x, y;
        };

        static vk::VertexInputBindingDescription getBindingDescription() {
            vk::VertexInputBindingDescription bindingDescription = {
                0,
                sizeof(Vertex),
                vk::VertexInputRate::eVertex
            };

            return bindingDescription;
        }

        static std::array<vk::VertexInputAttributeDescription, 1> getAttributeDescriptions() {
            std::array<vk::VertexInputAttributeDescription, 1> attributeDescriptions = {
                vk::VertexInputAttributeDescription {
                    0,
                    0,
                    vk::Format::eR32G32Sfloat,
                    0
                }
            };

            return attributeDescriptions;
        }

        bool operator==(const Vertex& other) const {
            return x == other.x && y == other.y;
        }
    };

    struct Cell {
        struct {
            float x, y;
        };
        uint32_t alive;

        static vk::VertexInputBindingDescription getBindingDescription() {
            vk::VertexInputBindingDescription bindingDescription = {
                1,
                sizeof(Cell),
                vk::VertexInputRate::eInstance
            };

            return bindingDescription;
        }

        static std::array<vk::VertexInputAttributeDescription, 2> getAttributeDescriptions() {
            std::array<vk::VertexInputAttributeDescription, 2> attributeDescriptions = {
                vk::VertexInputAttributeDescription {
                    1,
                    1,
                    vk::Format::eR32G32Sfloat,
                    offsetof(Cell, x)
                },
                vk::VertexInputAttributeDescription {
                    2,
                    1,
                    vk::Format::eR32Uint,
                    offsetof(Cell, alive)
                },
            };

            return attributeDescriptions;
        }

        bool operator==(const Cell& other) const {
            return x == other.x && y == other.y && alive == other.alive;
        }
    };

    struct Camera {
        float x, y;
        float zoom;
    };

    struct Queue {
        std::optional<uint32_t> index;
        vk::Queue queue;

        bool operator==(const Queue& other) const {
            return index.has_value() && other.index.has_value() && index.value() == other.index.value() && queue == other.queue;
        }

        bool operator!=(const Queue& other) const {
            return index.has_value() && other.index.has_value() && (index.value() != other.index.value() || queue != other.queue);
        }
    };

    struct Buffer {
        vk::Buffer buffer;
        vk::MemoryRequirements mem_reqs;
        uint32_t offset;
    };

    void createInstance(vk::Instance& instance, vk::DispatchLoaderDynamic& dispatcher, vk::DebugUtilsMessengerEXT& debug_utils);
    void createSurface(vk::Instance instance, GLFWwindow* window, vk::SurfaceKHR& surface);
    void createDevice(
        vk::Instance instance,
        vk::SurfaceKHR surface,
        vk::DispatchLoaderDynamic dispatcher,
        vk::PhysicalDevice& physical_device,
        vk::Device& device,
        Queue& graphics_queue,
        Queue& present_queue,
        Queue& compute_queue
    );
    void createSwapchain(
        vk::PhysicalDevice physical_device,
        vk::Device device,
        vk::SurfaceKHR surface,
        vk::Extent2D image_extent,
        vk::DispatchLoaderDynamic dispatcher,
        std::set<uint32_t> queue_indexes,
        vk::SurfaceFormatKHR& surface_format,
        vk::SwapchainKHR& swapchain
    );
    void createImageView(
        vk::Device device,
        vk::Image image,
        vk::Format format,
        vk::ImageView& image_view
    );
    void createDescriptorSetLayout(vk::Device device, vk::DescriptorSetLayout& descriptor_set_layout);
    void createDescriptorPool(
        vk::Device device,
        uint32_t frame_count,
        vk::DescriptorPool& descriptor_pool
    );
    void createDescriptorSets(
        vk::Device device,
        std::vector<vk::DescriptorSetLayout> descriptor_layouts,
        vk::DescriptorPool descriptor_pool,
        std::vector<game::Buffer> uniform_buffers,
        std::vector<vk::DescriptorSet>& descriptor_sets
    );
    void createCommandPools(
        vk::Device device,
        uint32_t graphics_queue_index,
        uint32_t compute_queue_index,
        vk::CommandPool& graphics_command_pool,
        vk::CommandPool& compute_command_pool
    );
    void createBuffer(
        vk::Device device,
        vk::DeviceSize size,
        std::set<uint32_t> queue_indexes,
        vk::BufferUsageFlags usage,
        vk::Buffer& buffer
    );
    void createDeviceMemory(
        vk::Device device,
        vk::PhysicalDevice physical_device,
        vk::DeviceSize size,
        uint32_t& device_memory_type_index,
        vk::DeviceMemory& device_memory
    );
    void createRenderpass(
        vk::Device device,
        vk::Format format,
        vk::RenderPass& render_pass
    );
    void createGraphicsPipeline(
        vk::Device device,
        uint32_t grid_size,
        vk::Extent2D image_extent,
        std::vector<vk::DescriptorSetLayout> set_layouts,
        std::vector<vk::VertexInputBindingDescription> vertex_input_bindings,
        std::vector<vk::VertexInputAttributeDescription> vertex_input_attributes,
        vk::RenderPass renderpass,
        vk::PipelineLayout& graphics_pipeline_layout,
        vk::Pipeline& graphics_pipeline
    );
    void createFramebuffer(
        vk::Device device,
        vk::Extent2D frame_size,
        std::vector<vk::ImageView> image_views,
        vk::RenderPass render_pass,
        vk::Framebuffer& framebuffer
    );
    void createCommandBuffers(
        vk::Device device,
        vk::CommandPool command_pool,
        uint32_t count,
        vk::RenderPass render_pass,
        std::vector<vk::Framebuffer> framebuffers,
        vk::Extent2D render_area,
        vk::Pipeline graphics_pipeline,
        vk::PipelineLayout graphics_pipeline_layout,
        Buffer vertex_buffer,
        std::vector<Buffer> game_buffers,
        uint32_t grid_size,
        std::vector<vk::DescriptorSet> descriptor_sets,
        std::vector<vk::CommandBuffer>& command_buffers
    );
}

#endif // __VULKAN__METHODS__HPP__