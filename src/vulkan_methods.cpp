#include "vulkan_methods.hpp"

#include <iostream>

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

namespace game {
    void createInstance(
        vk::Instance& instance,
        vk::DispatchLoaderDynamic& dispatcher,
        vk::DebugUtilsMessengerEXT& debug_utils
    ) {
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

    void createSurface(vk::Instance instance, GLFWwindow* window, vk::SurfaceKHR& surface) {
        VkSurfaceKHR s;
        glfwCreateWindowSurface(instance, window, nullptr, &s);
        surface = s;
    }

    void createDevice(
        vk::Instance instance,
        vk::SurfaceKHR surface,
        vk::DispatchLoaderDynamic dispatcher,
        vk::PhysicalDevice& physical_device,
        vk::Device& device,
        Queue& graphics_queue,
        Queue& present_queue,
        Queue& compute_queue
    ) {
        physical_device = instance.enumeratePhysicalDevices()[0];

        auto queue_families = physical_device.getQueueFamilyProperties();
        for (uint32_t i = 0; i < queue_families.size(); i++) {
            vk::QueueFamilyProperties& qf = queue_families[i];
            bool is_graphics = qf.queueCount > 0 ? bool(qf.queueFlags & vk::QueueFlagBits::eGraphics) : false;
            bool is_present = qf.queueCount > 0 ? bool(physical_device.getSurfaceSupportKHR(i, surface, dispatcher)) : false;
            bool is_compute = qf.queueCount > 0 ? bool(qf.queueFlags & vk::QueueFlagBits::eCompute) : false;

            if (is_graphics && is_present && is_compute) {
                graphics_queue.index = i;
                present_queue.index = i;
                compute_queue.index = i;
                break;
            }
            if (is_graphics && !graphics_queue.index.has_value()) graphics_queue.index = i;
            if (is_present && !present_queue.index.has_value()) present_queue.index = i;
            if (is_compute && !compute_queue.index.has_value()) compute_queue.index = i;
        }

        std::vector<const char*> extensions = { VK_KHR_SWAPCHAIN_EXTENSION_NAME };
        std::vector<vk::DeviceQueueCreateInfo> queue_infos;
        std::set<uint32_t> unique_queue_indexes = {
            graphics_queue.index.value(),
            present_queue.index.value(),
            compute_queue.index.value()
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
        graphics_queue.queue = device.getQueue(graphics_queue.index.value(), 0);
        present_queue.queue = device.getQueue(present_queue.index.value(), 0);
        compute_queue.queue = device.getQueue(compute_queue.index.value(), 0);
    }

    void createSwapchain(
        vk::PhysicalDevice physical_device,
        vk::Device device,
        vk::SurfaceKHR surface,
        GLFWwindow* window,
        vk::DispatchLoaderDynamic dispatcher,
        std::set<uint32_t> queue_indexes,
        vk::SwapchainKHR& swapchain
    ) {
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

        if (queue_indexes.size() > 1) {
            std::vector<uint32_t> qi(queue_indexes.begin(), queue_indexes.end());
            swapchain_info
                .setImageSharingMode(vk::SharingMode::eConcurrent)
                .setQueueFamilyIndexCount(qi.size())
                .setPQueueFamilyIndices(qi.data());
        } else {
            swapchain_info.setImageSharingMode(vk::SharingMode::eExclusive);
        }

        swapchain_info
            .setPreTransform(vk::SurfaceTransformFlagBitsKHR::eIdentity)
            .setCompositeAlpha(vk::CompositeAlphaFlagBitsKHR::eOpaque)
            .setPresentMode(vk::PresentModeKHR::eFifo)
            .setClipped(VK_TRUE);

        swapchain = device.createSwapchainKHR(swapchain_info, nullptr, dispatcher);
    }

    void createDescriptorSetLayout(vk::Device device, vk::DescriptorSetLayout& descriptor_set_layout) {
        std::vector<vk::DescriptorSetLayoutBinding> bindings = {};

        vk::DescriptorSetLayoutCreateInfo descriptor_set_layout_info = vk::DescriptorSetLayoutCreateInfo()
            .setBindingCount(bindings.size())
            .setPBindings(bindings.data());
        descriptor_set_layout = device.createDescriptorSetLayout(descriptor_set_layout_info);
    }

    void createDescriptorPool(vk::Device device, vk::DescriptorPool& descriptor_pool) {
        std::vector<vk::DescriptorPoolSize> sizes = {};
        vk::DescriptorPoolCreateInfo descriptor_pool_info = vk::DescriptorPoolCreateInfo()
            .setMaxSets(2)
            .setPoolSizeCount(sizes.size())
            .setPPoolSizes(sizes.data());

        descriptor_pool = device.createDescriptorPool(descriptor_pool_info);
    }

    void createCommandPools(
        vk::Device device,
        uint32_t graphics_queue_index,
        uint32_t compute_queue_index,
        vk::CommandPool& graphics_command_pool,
        vk::CommandPool& compute_command_pool
    ) {
        vk::CommandPoolCreateInfo command_pool_info = vk::CommandPoolCreateInfo()
            .setQueueFamilyIndex(graphics_queue_index);
        graphics_command_pool = device.createCommandPool(command_pool_info);
        if (compute_queue_index != graphics_queue_index) {
            command_pool_info.setQueueFamilyIndex(compute_queue_index);
            compute_command_pool = device.createCommandPool(command_pool_info);
        } else {
            compute_command_pool = graphics_command_pool;
        }
    }

    void createBuffer(
        vk::Device device,
        vk::DeviceSize size,
        std::set<uint32_t> queue_indexes,
        vk::BufferUsageFlags usage,
        vk::Buffer& buffer
    ) {
        vk::BufferCreateInfo buffer_info = vk::BufferCreateInfo()
            .setSize(size)
            .setUsage(usage);
        if (queue_indexes.size() > 1) {
            std::vector<uint32_t> qi(queue_indexes.begin(), queue_indexes.end());
            buffer_info
                .setSharingMode(vk::SharingMode::eConcurrent)
                .setQueueFamilyIndexCount(qi.size())
                .setPQueueFamilyIndices(qi.data());
        } else {
            buffer_info.setSharingMode(vk::SharingMode::eExclusive);
        }

        buffer = device.createBuffer(buffer_info);
    }

    void createDeviceMemory(vk::Device device, vk::PhysicalDevice physical_device, vk::DeviceSize size, vk::DeviceMemory& device_memory) {
        auto memory_properties = physical_device.getMemoryProperties();
        uint32_t index;
        for (uint32_t i = 0; i < memory_properties.memoryTypeCount; i++) {
            if (memory_properties.memoryTypes[i].propertyFlags & (vk::MemoryPropertyFlagBits::eHostVisible | vk::MemoryPropertyFlagBits::eHostCoherent)) {
                index = i;
                break;
            }
        }
        vk::MemoryAllocateInfo memory_info = vk::MemoryAllocateInfo()
            .setAllocationSize(size)
            .setMemoryTypeIndex(index);

        device_memory = device.allocateMemory(memory_info);
    }
}