#include <set>
#include <vector>
#include <array>
#include <iostream>
#include <optional>
#include <thread>
#include <vulkan/vulkan.hpp>
#include <GLFW/glfw3.h>

std::ostream& operator<<(std::ostream& os, vk::DebugUtilsMessageSeverityFlagsEXT flags) {
    if (flags & vk::DebugUtilsMessageSeverityFlagBitsEXT::eVerbose) {
        os << "(Verbose)";
    }
    if (flags & vk::DebugUtilsMessageSeverityFlagBitsEXT::eInfo) {
        os << "(Info)";
    }
    if (flags & vk::DebugUtilsMessageSeverityFlagBitsEXT::eWarning) {
        os << "(Warning)";
    }
    if (flags & vk::DebugUtilsMessageSeverityFlagBitsEXT::eError) {
        os << "(Error)";
    }
    return os;
}

std::ostream& operator<<(std::ostream& os, vk::DebugUtilsMessageTypeFlagsEXT flags) {
    if (flags & vk::DebugUtilsMessageTypeFlagBitsEXT::eGeneral) {
        os << "[General]";
    }
    if (flags & vk::DebugUtilsMessageTypeFlagBitsEXT::ePerformance) {
        os << "[Performance]";
    }
    if (flags & vk::DebugUtilsMessageTypeFlagBitsEXT::eValidation) {
        os << "[Validation]";
    }
    return os;
}

VkBool32 vkDebugUtilsMessengerCallbackEXT(
    VkDebugUtilsMessageSeverityFlagBitsEXT messageSeverity,
    VkDebugUtilsMessageTypeFlagsEXT messageTypes,
    const VkDebugUtilsMessengerCallbackDataEXT* pCallbackData,
    void* pUserData
) {
    std::cout 
        << vk::DebugUtilsMessageTypeFlagsEXT(messageTypes)
        << vk::DebugUtilsMessageSeverityFlagsEXT(messageSeverity)
        << " "
        << pCallbackData->pMessage
        << std::endl;
    return VK_FALSE;
}

struct Vertex {
    float x, y;

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
                offsetof(Vertex, x)
            }
        };

        return attributeDescriptions;
    }

    bool operator==(const Vertex& other) const {
        return x == other.x && y == other.y;
    }
};

std::array<Vertex, 4> vertices = {
    Vertex { 0., 0. },
    Vertex { 0., 1. },
    Vertex { 1., 0. },
    Vertex { 1., 1. }
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
                0,
                0,
                vk::Format::eR32G32Uint,
                offsetof(Cell, x)
            },
            vk::VertexInputAttributeDescription {
                0,
                0,
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

int main(int argc, char** argv) {
    glfwInit();

    uint64_t grid_width = 1000, grid_height = 1000;
    if (argc == 3) {
        grid_width = std::atoll(argv[1]);
        grid_height = std::atoll(argv[2]);
    }

    vk::Instance instance;
    vk::DebugUtilsMessengerEXT debug_utils;
    vk::DispatchLoaderDynamic dispatcher;
    {
        vk::DebugUtilsMessengerCreateInfoEXT debug_utils_info = vk::DebugUtilsMessengerCreateInfoEXT()
            .setMessageSeverity(
                vk::DebugUtilsMessageSeverityFlagBitsEXT::eInfo |
                vk::DebugUtilsMessageSeverityFlagBitsEXT::eWarning |
                vk::DebugUtilsMessageSeverityFlagBitsEXT::eError
            )
            .setMessageType(
                vk::DebugUtilsMessageTypeFlagBitsEXT::ePerformance |
                vk::DebugUtilsMessageTypeFlagBitsEXT::eValidation
            )
            .setPfnUserCallback(vkDebugUtilsMessengerCallbackEXT);

        std::vector<const char*> layers = { "VK_LAYER_LUNARG_standard_validation" };
        std::vector<const char*> extensions = { VK_EXT_DEBUG_UTILS_EXTENSION_NAME };
        {
            uint32_t count;
            const char** exts = glfwGetRequiredInstanceExtensions(&count);
            for (uint32_t i = 0; i < count; i++) {
                extensions.push_back(exts[i]);
            }
        }

        vk::InstanceCreateInfo instance_info = vk::InstanceCreateInfo()
            .setEnabledLayerCount(layers.size())
            .setPpEnabledLayerNames(layers.data())
            .setEnabledExtensionCount(extensions.size())
            .setPpEnabledExtensionNames(extensions.data());

        instance = vk::createInstance(instance_info);
        dispatcher = vk::DispatchLoaderDynamic(instance);
        debug_utils = instance.createDebugUtilsMessengerEXT(debug_utils_info, nullptr, dispatcher);
    }

    glfwWindowHint(GLFW_CLIENT_API, GLFW_NO_API);
    GLFWwindow* window = glfwCreateWindow(800, 600, "Game of Life", nullptr, nullptr);
    vk::SurfaceKHR surface;
    {
        VkSurfaceKHR s;
        glfwCreateWindowSurface(instance, window, nullptr, &s);
        surface = s;
    }

    vk::PhysicalDevice physical_device;
    vk::Device device;
    vk::Queue graphics_queue, present_queue, compute_queue;
    std::optional<uint32_t> graphics_queue_index, present_queue_index, compute_queue_index;
    {
        physical_device = instance.enumeratePhysicalDevices()[0];

        auto queue_families = physical_device.getQueueFamilyProperties();
        for (uint32_t i = 0; i < queue_families.size(); i++) {
            vk::QueueFamilyProperties& qf = queue_families[i];
            bool is_graphics = qf.queueCount > 0 ? bool(qf.queueFlags & vk::QueueFlagBits::eGraphics) : false;
            bool is_present = qf.queueCount > 0 ? bool(physical_device.getSurfaceSupportKHR(i, surface, dispatcher)) : false;
            bool is_compute = qf.queueCount > 0 ? bool(qf.queueFlags & vk::QueueFlagBits::eCompute) : false;

            if (is_graphics && is_present && is_compute) {
                graphics_queue_index = i;
                present_queue_index = i;
                compute_queue_index = i;
                break;
            }
            if (is_graphics && !graphics_queue_index.has_value()) graphics_queue_index = i;
            if (is_present && !present_queue_index.has_value()) present_queue_index = i;
            if (is_compute && !compute_queue_index.has_value()) compute_queue_index = i;
        }

        std::vector<const char*> extensions = { VK_KHR_SWAPCHAIN_EXTENSION_NAME };
        std::vector<vk::DeviceQueueCreateInfo> queue_infos;
        std::set<uint32_t> unique_queue_indexes = {
            graphics_queue_index.value(),
            present_queue_index.value(),
            compute_queue_index.value()
        };

        float priorities = 1.f;
        for (uint32_t index : unique_queue_indexes) {
            queue_infos.push_back(
                vk::DeviceQueueCreateInfo(
                    {},
                    index,
                    1,
                    &priorities
                )
            );
        }

        vk::DeviceCreateInfo device_info = vk::DeviceCreateInfo()
            .setEnabledExtensionCount(extensions.size())
            .setPpEnabledExtensionNames(extensions.data())
            .setQueueCreateInfoCount(queue_infos.size())
            .setPQueueCreateInfos(queue_infos.data());

        device = physical_device.createDevice(device_info);
        dispatcher = vk::DispatchLoaderDynamic(instance, device);
        graphics_queue = device.getQueue(graphics_queue_index.value(), 0);
        present_queue = device.getQueue(present_queue_index.value(), 0);
        compute_queue = device.getQueue(compute_queue_index.value(), 0);
    }

    vk::SwapchainKHR swapchain;
    {
        int width, height;
        glfwGetFramebufferSize(window, &width, &height);

        auto surface_capabilities = physical_device.getSurfaceCapabilitiesKHR(surface, dispatcher);
        uint32_t min_image_count = surface_capabilities.minImageCount + 1;
        if (surface_capabilities.maxImageCount > 0 && min_image_count > surface_capabilities.maxImageCount)
            min_image_count = surface_capabilities.maxImageCount;

        vk::SurfaceFormatKHR surface_format = physical_device.getSurfaceFormatsKHR(surface)[0];

        vk::SwapchainCreateInfoKHR swapchain_info = vk::SwapchainCreateInfoKHR()
            .setSurface(surface)
            .setMinImageCount(min_image_count)
            .setImageFormat(surface_format.format)
            .setImageColorSpace(surface_format.colorSpace)
            .setImageExtent(vk::Extent2D { static_cast<uint32_t>(width), static_cast<uint32_t>(height) })
            .setImageArrayLayers(1)
            .setImageUsage(vk::ImageUsageFlagBits::eColorAttachment);

        if (graphics_queue == present_queue) {
            swapchain_info.setImageSharingMode(vk::SharingMode::eExclusive);
        } else {
            std::vector<uint32_t> queue_indexes = { graphics_queue_index.value(), present_queue_index.value() };
            swapchain_info
                .setImageSharingMode(vk::SharingMode::eConcurrent)
                .setQueueFamilyIndexCount(queue_indexes.size())
                .setPQueueFamilyIndices(queue_indexes.data());
        }

        swapchain_info
            .setPreTransform(vk::SurfaceTransformFlagBitsKHR::eIdentity)
            .setCompositeAlpha(vk::CompositeAlphaFlagBitsKHR::eOpaque)
            .setPresentMode(vk::PresentModeKHR::eFifo)
            .setClipped(VK_TRUE);

        swapchain = device.createSwapchainKHR(swapchain_info, nullptr, dispatcher);
    }
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
    {
        std::vector<vk::DescriptorSetLayoutBinding> bindings = {
            vk::DescriptorSetLayoutBinding {
                0,
                vk::DescriptorType::eUniformBuffer,
                1,
                vk::ShaderStageFlagBits::eVertex
            },
            vk::DescriptorSetLayoutBinding {
                1,
                vk::DescriptorType::eUniformBuffer,
                1,
                vk::ShaderStageFlagBits::eVertex
            }
        };

        vk::DescriptorSetLayoutCreateInfo descriptor_set_layout_info = vk::DescriptorSetLayoutCreateInfo()
            .setBindingCount(bindings.size())
            .setPBindings(bindings.data());
        descriptor_set_layout = device.createDescriptorSetLayout(descriptor_set_layout_info);
    }

    vk::DescriptorPool descriptor_pool;
    {
        std::vector<vk::DescriptorPoolSize> sizes = {
            vk::DescriptorPoolSize {
                vk::DescriptorType::eUniformBuffer,
                2
            }
        };
        vk::DescriptorPoolCreateInfo descriptor_pool_info = vk::DescriptorPoolCreateInfo()
            .setMaxSets(2)
            .setPoolSizeCount(sizes.size())
            .setPPoolSizes(sizes.data());

        descriptor_pool = device.createDescriptorPool(descriptor_pool_info);
    }

    vk::CommandPool graphics_command_pool;
    vk::CommandPool compute_command_pool;
    {
        vk::CommandPoolCreateInfo command_pool_info = vk::CommandPoolCreateInfo()
            .setQueueFamilyIndex(graphics_queue_index.value());
        graphics_command_pool = device.createCommandPool(command_pool_info);
        if (compute_queue != graphics_queue) {
            command_pool_info.setQueueFamilyIndex(compute_queue_index.value());
            compute_command_pool = device.createCommandPool(command_pool_info);
        } else {
            compute_command_pool = graphics_command_pool;
        }
    }

    std::vector<vk::Buffer> game_buffers(swapchain_images.size());
    {
        vk::BufferCreateInfo buffer_info = vk::BufferCreateInfo()
            .setSize(grid_width * grid_height * sizeof(Cell))
            .setUsage(vk::BufferUsageFlagBits::eUniformBuffer);
        if (graphics_queue != compute_queue) {
            std::vector<uint32_t> queue_indexes = { graphics_queue_index.value(), compute_queue_index.value() };
            buffer_info
                .setSharingMode(vk::SharingMode::eConcurrent)
                .setQueueFamilyIndexCount(queue_indexes.size())
                .setPQueueFamilyIndices(queue_indexes.data());
        } else {
            buffer_info.setSharingMode(vk::SharingMode::eExclusive);
        }

        for (uint32_t i = 0; i < game_buffers.size(); i++) {
            game_buffers[i] = device.createBuffer(buffer_info);
        }
    }
    // ###

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