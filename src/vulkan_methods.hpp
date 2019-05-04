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
        uint32_t x, y;

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
                    vk::Format::eR32G32Uint,
                    offsetof(Vertex, x)
                }
            };

            return attributeDescriptions;
        }

        bool operator==(const Vertex& other) const {
            return x == other.x && y == other.y;
        }
    };

    struct Cell {
        uint32_t x, y, alive;

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
                    2,
                    vk::Format::eR32G32Uint,
                    offsetof(Cell, x)
                },
                vk::VertexInputAttributeDescription {
                    1,
                    3,
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
        uint32_t x, y;
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
        GLFWwindow* window,
        vk::DispatchLoaderDynamic dispatcher,
        std::set<uint32_t> queue_indexes,
        vk::SwapchainKHR& swapchain
    );
    void createDescriptorSetLayout(vk::Device device, vk::DescriptorSetLayout& descriptor_set_layout);
    void createDescriptorPool(vk::Device device, vk::DescriptorPool& descriptor_pool);
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
    void createDeviceMemory(vk::Device device, vk::PhysicalDevice physical_device, vk::DeviceSize size, vk::DeviceMemory& device_memory);
}

#endif // __VULKAN__METHODS__HPP__