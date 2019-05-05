#include "vulkan_methods.hpp"

#include <iostream>
#include <fstream>

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
        vk::Extent2D image_extent,
        vk::DispatchLoaderDynamic dispatcher,
        std::set<uint32_t> queue_indexes,
        vk::SurfaceFormatKHR& surface_format,
        vk::SwapchainKHR& swapchain
    ) {
        auto surface_capabilities = physical_device.getSurfaceCapabilitiesKHR(surface, dispatcher);
        uint32_t min_image_count = surface_capabilities.minImageCount + 1;
        if (surface_capabilities.maxImageCount > 0 && min_image_count > surface_capabilities.maxImageCount)
            min_image_count = surface_capabilities.maxImageCount;

        surface_format = physical_device.getSurfaceFormatsKHR(surface)[0];

        vk::SwapchainCreateInfoKHR swapchain_info = vk::SwapchainCreateInfoKHR()
            .setSurface(surface)
            .setMinImageCount(min_image_count)
            .setImageFormat(surface_format.format)
            .setImageColorSpace(surface_format.colorSpace)
            .setImageExtent(image_extent)
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

    void createImageView(
        vk::Device device,
        vk::Image image,
        vk::Format format,
        vk::ImageView& image_view
    ) {
        vk::ImageViewCreateInfo image_view_info = vk::ImageViewCreateInfo(
            {},
            image,
            vk::ImageViewType::e2D,
            format,
            vk::ComponentMapping(),
            vk::ImageSubresourceRange(
                vk::ImageAspectFlagBits::eColor,
                0,
                1,
                0,
                1
            )
        );
        
        image_view = device.createImageView(image_view_info);
    }

    void createDescriptorSetLayout(vk::Device device, vk::DescriptorSetLayout& descriptor_set_layout) {
        std::vector<vk::DescriptorSetLayoutBinding> bindings = {
            vk::DescriptorSetLayoutBinding {
                0,
                vk::DescriptorType::eUniformBuffer,
                1,
                vk::ShaderStageFlagBits::eVertex,
                nullptr
            }
        };

        vk::DescriptorSetLayoutCreateInfo descriptor_set_layout_info = vk::DescriptorSetLayoutCreateInfo()
            .setBindingCount(bindings.size())
            .setPBindings(bindings.data());
        descriptor_set_layout = device.createDescriptorSetLayout(descriptor_set_layout_info);
    }

    void createDescriptorPool(
        vk::Device device,
        uint32_t frame_count,
        vk::DescriptorPool& descriptor_pool
    ) {
        std::vector<vk::DescriptorPoolSize> sizes = {
            vk::DescriptorPoolSize {
                vk::DescriptorType::eUniformBuffer,
                frame_count
            }
        };
        vk::DescriptorPoolCreateInfo descriptor_pool_info = vk::DescriptorPoolCreateInfo()
            .setMaxSets(frame_count)
            .setPoolSizeCount(sizes.size())
            .setPPoolSizes(sizes.data());

        descriptor_pool = device.createDescriptorPool(descriptor_pool_info);
    }

    void createDescriptorSets(
        vk::Device device,
        std::vector<vk::DescriptorSetLayout> descriptor_layouts,
        vk::DescriptorPool descriptor_pool,
        std::vector<game::Buffer> uniform_buffers,
        std::vector<vk::DescriptorSet>& descriptor_sets
    ) {
        vk::DescriptorSetAllocateInfo descriptor_set_info = vk::DescriptorSetAllocateInfo()
            .setDescriptorPool(descriptor_pool)
            .setDescriptorSetCount(descriptor_layouts.size())
            .setPSetLayouts(descriptor_layouts.data());

        descriptor_sets = device.allocateDescriptorSets(descriptor_set_info);

        for (uint32_t i = 0; i < descriptor_sets.size(); i++) {
            vk::DescriptorBufferInfo buffer_info = vk::DescriptorBufferInfo()
                .setBuffer(uniform_buffers[i].buffer)
                .setOffset(0)
                .setRange(sizeof(game::Camera));
            
            std::vector<vk::WriteDescriptorSet> descriptor_writes = {
                vk::WriteDescriptorSet(
                    descriptor_sets[i],
                    0,
                    0,
                    1,
                    vk::DescriptorType::eUniformBuffer,
                    nullptr,
                    &buffer_info,
                    nullptr
                )
            };

            device.updateDescriptorSets(
                descriptor_writes,
                {}
            );
        }
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

    void createDeviceMemory(
        vk::Device device,
        vk::PhysicalDevice physical_device,
        vk::DeviceSize size,
        uint32_t& device_memory_type_index,
        vk::DeviceMemory& device_memory
    ) {
        auto memory_properties = physical_device.getMemoryProperties();
        for (uint32_t i = 0; i < memory_properties.memoryTypeCount; i++) {
            if (memory_properties.memoryTypes[i].propertyFlags & (vk::MemoryPropertyFlagBits::eHostVisible | vk::MemoryPropertyFlagBits::eHostCoherent)) {
                device_memory_type_index = i;
                break;
            }
        }
        vk::MemoryAllocateInfo memory_info = vk::MemoryAllocateInfo()
            .setAllocationSize(size)
            .setMemoryTypeIndex(device_memory_type_index);

        device_memory = device.allocateMemory(memory_info);
    }

    void createRenderpass(
        vk::Device device,
        vk::Format format,
        vk::RenderPass& render_pass
    ) {
        std::vector<vk::AttachmentDescription> attachments = {
            vk::AttachmentDescription {
                {},
                format,
                vk::SampleCountFlagBits::e1,
                vk::AttachmentLoadOp::eClear,
                vk::AttachmentStoreOp::eStore,
                vk::AttachmentLoadOp::eDontCare,
                vk::AttachmentStoreOp::eDontCare,
                vk::ImageLayout::eUndefined,
                vk::ImageLayout::ePresentSrcKHR
            }
        };

        std::vector<vk::AttachmentReference> references = {
            vk::AttachmentReference {
                0,
                vk::ImageLayout::eColorAttachmentOptimal
            }
        };

        std::vector<vk::SubpassDescription> subpasses = {
            vk::SubpassDescription {
                {},
                vk::PipelineBindPoint::eGraphics,
                0,
                nullptr,
                static_cast<uint32_t>(references.size()),
                references.data(),
                nullptr,
                nullptr,
                0,
                nullptr
            }
        };

        std::vector<vk::SubpassDependency> dependencies = {
            vk::SubpassDependency {
                VK_SUBPASS_EXTERNAL,
                0,
                vk::PipelineStageFlagBits::eBottomOfPipe,
                vk::PipelineStageFlagBits::eColorAttachmentOutput,
                vk::AccessFlagBits(),
                vk::AccessFlagBits::eColorAttachmentRead | vk::AccessFlagBits::eColorAttachmentWrite
            },
            vk::SubpassDependency {
                0,
                VK_SUBPASS_EXTERNAL,
                vk::PipelineStageFlagBits::eColorAttachmentOutput,
                vk::PipelineStageFlagBits::eTopOfPipe,
                vk::AccessFlagBits::eColorAttachmentRead | vk::AccessFlagBits::eColorAttachmentWrite,
                vk::AccessFlagBits()
            }
        };

        vk::RenderPassCreateInfo render_pass_info = vk::RenderPassCreateInfo()
            .setAttachmentCount(attachments.size())
            .setPAttachments(attachments.data())
            .setSubpassCount(subpasses.size())
            .setPSubpasses(subpasses.data())
            .setDependencyCount(dependencies.size())
            .setPDependencies(dependencies.data());

        render_pass = device.createRenderPass(render_pass_info);
    }

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
    ) {
        vk::PipelineLayoutCreateInfo pipeline_layout_info = vk::PipelineLayoutCreateInfo()
            .setSetLayoutCount(set_layouts.size())
            .setPSetLayouts(set_layouts.data())
            .setPushConstantRangeCount(0)
            .setPPushConstantRanges(nullptr);
        graphics_pipeline_layout = device.createPipelineLayout(pipeline_layout_info);

        vk::SpecializationMapEntry vertex_specialization_map_entry = vk::SpecializationMapEntry()
            .setConstantID(0)
            .setOffset(0)
            .setSize(sizeof(uint32_t));

        vk::SpecializationInfo vertex_specialization = vk::SpecializationInfo()
            .setMapEntryCount(1)
            .setPMapEntries(&vertex_specialization_map_entry)
            .setDataSize(sizeof(uint32_t))
            .setPData(&grid_size);

        vk::ShaderModule vertex_shader;
        {
            std::ifstream vertex_shader_code("vertex.spv", std::ios::in | std::ios::binary);
            if (!vertex_shader_code.is_open()) {
                throw std::runtime_error("couldn't read vertex shader");
            }
            vertex_shader_code.seekg(0, std::ios::end);
            uint64_t vertex_shader_code_size = vertex_shader_code.tellg();
            vertex_shader_code.seekg(0, std::ios::beg);
            char* vertex_shader_code_c = new char[vertex_shader_code_size];
            vertex_shader_code.read(vertex_shader_code_c, vertex_shader_code_size);

            vk::ShaderModuleCreateInfo shader_info = vk::ShaderModuleCreateInfo()
                .setCodeSize(vertex_shader_code_size)
                .setPCode(reinterpret_cast<uint32_t*>(vertex_shader_code_c));
            vertex_shader = device.createShaderModule(shader_info);

            delete[] vertex_shader_code_c;
            vertex_shader_code.close();
        }

        vk::ShaderModule fragment_shader;
        {
            std::ifstream fragment_shader_code("fragment.spv", std::ios::in | std::ios::binary);
            if (!fragment_shader_code.is_open()) {
                throw std::runtime_error("couldn't read fragment shader");
            }
            fragment_shader_code.seekg(0, std::ios::end);
            uint64_t fragment_shader_code_size = fragment_shader_code.tellg();
            fragment_shader_code.seekg(0, std::ios::beg);
            char* fragment_shader_code_c = new char[fragment_shader_code_size];
            fragment_shader_code.read(fragment_shader_code_c, fragment_shader_code_size);

            vk::ShaderModuleCreateInfo shader_info = vk::ShaderModuleCreateInfo()
                .setCodeSize(fragment_shader_code_size)
                .setPCode(reinterpret_cast<uint32_t*>(fragment_shader_code_c));
            fragment_shader = device.createShaderModule(shader_info);

            delete[] fragment_shader_code_c;
            fragment_shader_code.close();
        }

        std::vector<vk::PipelineShaderStageCreateInfo> shader_stages_info = {
            vk::PipelineShaderStageCreateInfo {
                {},
                vk::ShaderStageFlagBits::eVertex,
                vertex_shader,
                "main",
                &vertex_specialization
            },
            vk::PipelineShaderStageCreateInfo {
                {},
                vk::ShaderStageFlagBits::eFragment,
                fragment_shader,
                "main",
                nullptr
            }
        };

        vk::PipelineVertexInputStateCreateInfo vertex_input_state_info = vk::PipelineVertexInputStateCreateInfo()
            .setVertexBindingDescriptionCount(vertex_input_bindings.size())
            .setPVertexBindingDescriptions(vertex_input_bindings.data())
            .setVertexAttributeDescriptionCount(vertex_input_attributes.size())
            .setPVertexAttributeDescriptions(vertex_input_attributes.data());

        vk::PipelineInputAssemblyStateCreateInfo input_assembly_info = vk::PipelineInputAssemblyStateCreateInfo()
            .setTopology(vk::PrimitiveTopology::eTriangleList)
            .setPrimitiveRestartEnable(VK_FALSE);

        vk::Viewport viewport {
            0., 0.,
            static_cast<float>(image_extent.width), static_cast<float>(image_extent.height),
            0., 1.
        };
        vk::Rect2D scissor {
            vk::Offset2D { 0, 0 },
            vk::Extent2D { image_extent.width, image_extent.height }
        };

        vk::PipelineViewportStateCreateInfo viewport_state = vk::PipelineViewportStateCreateInfo()
            .setViewportCount(1)
            .setPViewports(&viewport)
            .setScissorCount(1)
            .setPScissors(&scissor);

        vk::PipelineRasterizationStateCreateInfo rasterization_state = vk::PipelineRasterizationStateCreateInfo()
            .setDepthClampEnable(VK_FALSE)
            .setRasterizerDiscardEnable(VK_FALSE)
            .setPolygonMode(vk::PolygonMode::eFill)
            .setCullMode(vk::CullModeFlagBits::eNone)
            .setFrontFace(vk::FrontFace::eCounterClockwise)
            .setDepthBiasEnable(VK_FALSE)
            .setDepthBiasConstantFactor(0.)
            .setDepthBiasClamp(0.)
            .setDepthBiasSlopeFactor(0.)
            .setLineWidth(1.);

        vk::PipelineMultisampleStateCreateInfo multisampling_state = vk::PipelineMultisampleStateCreateInfo()
            .setRasterizationSamples(vk::SampleCountFlagBits::e1)
            .setSampleShadingEnable(VK_FALSE);

        vk::PipelineDepthStencilStateCreateInfo depth_stencil_state = vk::PipelineDepthStencilStateCreateInfo()
            .setDepthTestEnable(VK_FALSE)
            .setStencilTestEnable(VK_FALSE);

        vk::PipelineColorBlendAttachmentState color_blend_attachment = vk::PipelineColorBlendAttachmentState()
            .setColorWriteMask(vk::ColorComponentFlagBits::eR | vk::ColorComponentFlagBits::eG | vk::ColorComponentFlagBits::eB | vk::ColorComponentFlagBits::eA)
            .setBlendEnable(VK_FALSE);

        vk::PipelineColorBlendStateCreateInfo color_blend_state = vk::PipelineColorBlendStateCreateInfo()
            .setLogicOpEnable(VK_FALSE)
            .setLogicOp(vk::LogicOp::eCopy)
            .setAttachmentCount(1)
            .setPAttachments(&color_blend_attachment)
            .setBlendConstants(
                { 0., 0., 0., 0. }
            );

        vk::GraphicsPipelineCreateInfo graphics_pipeline_info = vk::GraphicsPipelineCreateInfo()
            .setStageCount(shader_stages_info.size())
            .setPStages(shader_stages_info.data())
            .setPVertexInputState(&vertex_input_state_info)
            .setPInputAssemblyState(&input_assembly_info)
            .setPViewportState(&viewport_state)
            .setPRasterizationState(&rasterization_state)
            .setPMultisampleState(&multisampling_state)
            .setPDepthStencilState(&depth_stencil_state)
            .setPColorBlendState(&color_blend_state)
            .setLayout(graphics_pipeline_layout)
            .setRenderPass(renderpass)
            .setSubpass(0);

        graphics_pipeline = device.createGraphicsPipeline(vk::PipelineCache(), graphics_pipeline_info);

        device.destroyShaderModule(vertex_shader);
        device.destroyShaderModule(fragment_shader);
    }

    void createFramebuffer(
        vk::Device device,
        vk::Extent2D frame_size,
        std::vector<vk::ImageView> image_views,
        vk::RenderPass render_pass,
        vk::Framebuffer& framebuffer
    ) {
        vk::FramebufferCreateInfo framebuffer_info = vk::FramebufferCreateInfo()
            .setAttachmentCount(image_views.size())
            .setPAttachments(image_views.data())
            .setWidth(frame_size.width)
            .setHeight(frame_size.height)
            .setLayers(1)
            .setRenderPass(render_pass);
        
        framebuffer = device.createFramebuffer(framebuffer_info);
    }

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
    ) {
        vk::CommandBufferAllocateInfo command_buffers_info = vk::CommandBufferAllocateInfo()
            .setCommandPool(command_pool)
            .setCommandBufferCount(count)
            .setLevel(vk::CommandBufferLevel::ePrimary);

        command_buffers = device.allocateCommandBuffers(command_buffers_info);

        for (uint32_t i = 0; i < count; i++) {
            vk::CommandBuffer& cmd = command_buffers[i];

            vk::CommandBufferBeginInfo command_buffer_begin = vk::CommandBufferBeginInfo()
                .setFlags(vk::CommandBufferUsageFlagBits::eSimultaneousUse);
            cmd.begin(command_buffer_begin);

            std::vector<vk::ClearValue> clear_colors = {
                vk::ClearValue {
                    vk::ClearColorValue {
                        std::array<float, 4> { .0f, .0f, .0f, 1.f }
                    }
                }
            };

            vk::RenderPassBeginInfo render_pass_begin = vk::RenderPassBeginInfo()
                .setFramebuffer(framebuffers[i])
                .setRenderPass(render_pass)
                .setRenderArea(
                    vk::Rect2D {
                        vk::Offset2D { 0, 0 },
                        render_area
                    }
                )
                .setClearValueCount(clear_colors.size())
                .setPClearValues(clear_colors.data());

            cmd.beginRenderPass(render_pass_begin, vk::SubpassContents::eInline);

            cmd.bindDescriptorSets(vk::PipelineBindPoint::eGraphics, graphics_pipeline_layout, 0, { descriptor_sets[i] }, {});
            cmd.bindPipeline(vk::PipelineBindPoint::eGraphics, graphics_pipeline);
            
            cmd.bindVertexBuffers(0, { vertex_buffer.buffer }, { 0 });
            cmd.bindVertexBuffers(1, { game_buffers[0].buffer }, { 0 });
            cmd.draw(6, grid_size * grid_size, 0, 0);

            cmd.endRenderPass();
            cmd.end();
        }
    }
}