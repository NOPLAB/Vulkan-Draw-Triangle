#include <GLFW/glfw3.h>

#define VK_USE_PLATFORM_WIN32_KHR
#define GLFW_INCLUDE_VULKAN

#include <vulkan/vulkan.hpp>
#include <glm/glm.hpp>
#include <vector>
#include <iostream>
#include <fstream>
#include <filesystem>

#define STB_IMAGE_WRITE_IMPLEMENTATION
#include "stb_image_write.h"

constexpr uint32_t kScreenWidth = 1920;
constexpr uint32_t kScreenHeight = 1080;

static std::string AppName = "Vulkan Test";
static std::string EngineName = "Vulkan.hpp";

class App
{
public:
    void run()
    {
        InitVulkan();
        CleanUp();
    }

private:
	std::vector<const char*> required_layers_ = { "VK_LAYER_KHRONOS_validation" };

    // Vulkanインスタンス
	vk::UniqueInstance instance_;

    // すべての物理デバイス
	std::vector<vk::PhysicalDevice> physical_devices_;

    // Vulkanが使用する物理デバイス
	vk::PhysicalDevice physical_device_;

    vk::PhysicalDeviceMemoryProperties physical_device_mem_props_;

    // グラフィックスをサポートするキューファミリを持っている物理デバイスが存在するか
    bool exists_suitable_physical_device_ = false;

    // グラフィックスをサポートするキューファミリのインデックス（1つ）
    uint32_t graphics_queue_family_index_;

    // 論理デバイス
    vk::UniqueDevice device_;

    // グラフィックスキュー（1つ）
    vk::Queue graphics_queue_;

    vk::UniqueCommandPool cmd_pool_;

    std::vector<vk::UniqueCommandBuffer> cmd_bufs_;

    vk::UniqueImage image_;
    vk::UniqueImageView image_view_;
    // イメージが必要とするメモリ
    vk::MemoryRequirements image_mem_req_;

    vk::UniqueDeviceMemory image_mem_;

    vk::UniqueRenderPass renderpass_;

    vk::UniqueFramebuffer framebuffer_;

    vk::UniquePipeline pipeline_;

    vk::UniqueShaderModule vert_shader_;
    vk::UniqueShaderModule frag_shader_;

    vk::UniqueBuffer buffer_;
    vk::UniqueDeviceMemory buffer_mem_;
    vk::MemoryRequirements buffer_mem_req_;

    /**
     * @brief Vulkanインスタンスの作成
     */
    void CreateInstance()
    {
        // vk::ApplicationInfoのインスタンス化
        const vk::ApplicationInfo application_info(AppName.c_str(), 1, EngineName.c_str(), 1, VK_API_VERSION_1_1);

        // vk::InstanceCreateInfoのインスタンス化
        vk::InstanceCreateInfo instance_create_info({}, &application_info);

        instance_create_info.enabledLayerCount = required_layers_.size();
        instance_create_info.ppEnabledLayerNames = &required_layers_.front();

        // Vulkanのインスタンス化
        instance_ = vk::createInstanceUnique(instance_create_info);
    }

    /**
     * @brief 物理デバイスの取得
     */
    void GetPhysicalDevices()
    {
        // 物理デバイスの取得
        physical_devices_ = instance_->enumeratePhysicalDevices();
    }

    /**
     * @brief 物理デバイスのすべてのキューファミリの情報を表示
     */
    void PrintPhysicalDevices() const
    {
        VkPhysicalDeviceProperties physical_device_properties;
        for (size_t i = 0; i < physical_devices_.size(); i++)
        {
            physical_device_properties = physical_devices_[i].getProperties();
            std::cout << physical_device_properties.deviceName << std::endl;

            std::vector<vk::QueueFamilyProperties> queueProps = physical_devices_[i].getQueueFamilyProperties();

            // 物理デバイスのすべてのキューファミリの情報を表示
            std::cout << "queue family count: " << queueProps.size() << std::endl;
            std::cout << std::endl;
            for (size_t i = 0; i < queueProps.size(); i++)
            {
                std::cout << "queue family index: " << i << std::endl;
                std::cout << "  queue count: " << queueProps[i].queueCount << std::endl;
                std::cout << "  graphic support: " << (queueProps[i].queueFlags & vk::QueueFlagBits::eGraphics ? "True" : "False") << std::endl;
                std::cout << "  compute support: " << (queueProps[i].queueFlags & vk::QueueFlagBits::eCompute ? "True" : "False") << std::endl;
                std::cout << "  transfer support: " << (queueProps[i].queueFlags & vk::QueueFlagBits::eTransfer ? "True" : "False") << std::endl;
                std::cout << std::endl;
            }
        }
    }

    /**
     * @brief グラフィックスをサポートするキューファミリを最低でも1つ持っている物理デバイスを選択する
     */
    void SelectPhysicalDevice()
    {
        for (size_t i = 0; i < physical_devices_.size(); i++)
        {
            std::vector<vk::QueueFamilyProperties> queueProps = physical_devices_[i].getQueueFamilyProperties();
            bool existsGraphicsQueue = false;

            for (size_t j = 0; j < queueProps.size(); j++)
            {
                if (queueProps[j].queueFlags & (vk::QueueFlagBits::eGraphics | vk::QueueFlagBits::eTransfer))
                {
                    existsGraphicsQueue = true;
                    graphics_queue_family_index_ = j;
                    break;
                }
            }

            if (existsGraphicsQueue)
            {
                physical_device_ = physical_devices_[i];
                exists_suitable_physical_device_ = true;
                break;
            }
        }

        if (!exists_suitable_physical_device_)
        {
            std::cerr << "使用可能な物理デバイスがありません" << std::endl;
            throw std::runtime_error("使用可能な物理デバイスがありません");
        }

        std::cout << "使用デバイス:" << physical_device_.getProperties().deviceName << std::endl;

        // デバイスのメモリ情報
        physical_device_mem_props_ = physical_device_.getMemoryProperties();
    }

    /**
     * @brief 論理デバイスの作成とキューの取得
     */
    void CreateLogicalDevice()
    {
        vk::DeviceCreateInfo device_create_info;

        // 論理デバイスにキューの使用を伝える
        vk::DeviceQueueCreateInfo queue_create_info[1];
        queue_create_info[0].queueFamilyIndex = graphics_queue_family_index_;
        queue_create_info[0].queueCount = 1;
        const float queue_priorities[1] = { 1.0f };
        queue_create_info[0].pQueuePriorities = queue_priorities;

        device_create_info.pQueueCreateInfos = queue_create_info;
        device_create_info.queueCreateInfoCount = 1;

        // バリデーションレイヤー
        device_create_info.enabledLayerCount = required_layers_.size();
        device_create_info.ppEnabledLayerNames = &required_layers_.front();

        // 論理デバイスの作成
        device_ = physical_device_.createDeviceUnique(device_create_info);

        // キューの取得
        graphics_queue_ = device_->getQueue(graphics_queue_family_index_, 0);
    }

    void CreateCommandPool()
    {
        vk::CommandPoolCreateInfo cmd_pool_create_info;

        // 後でそのコマンドバッファを送信するときに対象とするキュー
        cmd_pool_create_info.queueFamilyIndex = graphics_queue_family_index_;

        cmd_pool_ = device_->createCommandPoolUnique(cmd_pool_create_info);
    }

    void CreateCommandBuffers()
    {
        vk::CommandBufferAllocateInfo vk_cmd_buf_alloc_info;
        vk_cmd_buf_alloc_info.commandPool = cmd_pool_.get();
        vk_cmd_buf_alloc_info.commandBufferCount = 1; // 作るコマンドバッファの数
        vk_cmd_buf_alloc_info.level = vk::CommandBufferLevel::ePrimary;
        
        cmd_bufs_ = device_->allocateCommandBuffersUnique(vk_cmd_buf_alloc_info);
    }

    /**
     * @brief 画像やバッファの要求するメモリタイプと使用したい機能が使えるインデックスを返す
     * @param request_type_filter 要求するメモリタイプ
     * @param properties メモリの機能
     * @return メモリのインデックス
     */
    uint32_t FindMemoryType(const uint32_t request_type_filter, const vk::MemoryPropertyFlagBits properties) const
    {
	    const vk::PhysicalDeviceMemoryProperties physical_device_memory_properties = physical_device_.getMemoryProperties();

        for (uint32_t i = 0; i < physical_device_memory_properties.memoryTypeCount; i++) {
            if ((request_type_filter & (1 << i)) && (physical_device_memory_properties.memoryTypes[i].propertyFlags & properties) == properties) {
                return i;
            }
        }

        throw std::runtime_error("Failed to find suitable memory type!");
    }

    void CreateImage()
    {
        const vk::Format image_format = vk::Format::eR8G8B8A8Unorm;
        const vk::FormatProperties format_properties = physical_device_.getFormatProperties(image_format);

        const vk::ImageTiling image_tiling = vk::ImageTiling::eOptimal;
        if (format_properties.linearTilingFeatures & vk::FormatFeatureFlagBits::eColorAttachment)
        {
            std::cout << "Linearに対応" << std::endl;
        }
        else if (format_properties.optimalTilingFeatures & vk::FormatFeatureFlagBits::eColorAttachment)
        {
            std::cout << "Linearに非対応" << std::endl;
        }

        /* イメージの作成 */

        vk::ImageCreateInfo image_create_info;
        image_create_info.imageType = vk::ImageType::e2D;
        image_create_info.extent = vk::Extent3D(kScreenWidth, kScreenHeight, 1);
        image_create_info.mipLevels = 1;
        image_create_info.arrayLayers = 1;
        image_create_info.format = image_format;
        image_create_info.tiling = image_tiling;
        image_create_info.initialLayout = vk::ImageLayout::eUndefined;
        image_create_info.usage = vk::ImageUsageFlagBits::eTransferSrc | vk::ImageUsageFlagBits::eColorAttachment;
    	image_create_info.sharingMode = vk::SharingMode::eExclusive;
        image_create_info.samples = vk::SampleCountFlagBits::e1;
        
        image_ = device_->createImageUnique(image_create_info);

        /* イメージのメモリ確保 */

        image_mem_req_ = device_->getImageMemoryRequirements(image_.get());
        
        vk::MemoryAllocateInfo image_mem_alloc_info_;
    	image_mem_alloc_info_.allocationSize = image_mem_req_.size;
        image_mem_alloc_info_.memoryTypeIndex = FindMemoryType(image_mem_req_.memoryTypeBits, vk::MemoryPropertyFlagBits::eDeviceLocal);
        
        image_mem_ = device_->allocateMemoryUnique(image_mem_alloc_info_);
        
    	device_->bindImageMemory(image_.get(), image_mem_.get(), 0);

        /* バッファの作成 */

        vk::BufferCreateInfo buffer_create_info;
        buffer_create_info.size = image_mem_req_.size;
        buffer_create_info.usage = vk::BufferUsageFlagBits::eTransferDst;

        buffer_ = device_->createBufferUnique(buffer_create_info);

        /* バッファのメモリ確保 */

        buffer_mem_req_ = device_->getBufferMemoryRequirements(buffer_.get());

        vk::MemoryAllocateInfo buffer_allocate_info;
        buffer_allocate_info.allocationSize = buffer_mem_req_.size;
        buffer_allocate_info.memoryTypeIndex = FindMemoryType(buffer_mem_req_.memoryTypeBits, vk::MemoryPropertyFlagBits::eHostVisible);

        buffer_mem_ = device_->allocateMemoryUnique(buffer_allocate_info);

        device_->bindBufferMemory(buffer_.get(), buffer_mem_.get(), 0);
    }

    void CreateImageView()
    {
        vk::ImageViewCreateInfo image_view_create_info;
        image_view_create_info.image = image_.get();
        image_view_create_info.viewType = vk::ImageViewType::e2D;
        image_view_create_info.format = vk::Format::eR8G8B8A8Unorm;
        image_view_create_info.components.r = vk::ComponentSwizzle::eIdentity;
        image_view_create_info.components.g = vk::ComponentSwizzle::eIdentity;
        image_view_create_info.components.b = vk::ComponentSwizzle::eIdentity;
        image_view_create_info.components.a = vk::ComponentSwizzle::eIdentity;
        image_view_create_info.subresourceRange.aspectMask = vk::ImageAspectFlagBits::eColor;
        image_view_create_info.subresourceRange.baseMipLevel = 0;
        image_view_create_info.subresourceRange.levelCount = 1;
        image_view_create_info.subresourceRange.baseArrayLayer = 0;
        image_view_create_info.subresourceRange.layerCount = 1;

        image_view_ = device_->createImageViewUnique(image_view_create_info);

        
    }

    void CreateRenderPass()
    {
        vk::AttachmentDescription attachments[1];
        attachments[0].format = vk::Format::eR8G8B8A8Unorm;
        attachments[0].samples = vk::SampleCountFlagBits::e1;
        attachments[0].loadOp = vk::AttachmentLoadOp::eClear;
        attachments[0].storeOp = vk::AttachmentStoreOp::eStore;
        attachments[0].stencilLoadOp = vk::AttachmentLoadOp::eDontCare;
        attachments[0].stencilStoreOp = vk::AttachmentStoreOp::eDontCare;
        attachments[0].initialLayout = vk::ImageLayout::eUndefined;
        attachments[0].finalLayout = vk::ImageLayout::eGeneral;

        vk::AttachmentReference subpass0_attachmentRefs[1];
        subpass0_attachmentRefs[0].attachment = 0;
        subpass0_attachmentRefs[0].layout = vk::ImageLayout::eColorAttachmentOptimal;

        vk::SubpassDescription subpasses[1];
        subpasses[0].pipelineBindPoint = vk::PipelineBindPoint::eGraphics;
        subpasses[0].colorAttachmentCount = 1;
        subpasses[0].pColorAttachments = subpass0_attachmentRefs;

        vk::RenderPassCreateInfo renderpass_create_info;
        renderpass_create_info.attachmentCount = 1;
        renderpass_create_info.pAttachments = attachments;
        renderpass_create_info.subpassCount = 1;
        renderpass_create_info.pSubpasses = subpasses;
        renderpass_create_info.dependencyCount = 0;
        renderpass_create_info.pDependencies = nullptr;

        renderpass_ = device_->createRenderPassUnique(renderpass_create_info);
    }

    void CreateFrameBuffer()
    {
        vk::ImageView vk_frame_buf_attachments[1];
        vk_frame_buf_attachments[0] = image_view_.get();

        vk::FramebufferCreateInfo framebuffer_create_info;
        framebuffer_create_info.width = kScreenWidth;
        framebuffer_create_info.height = kScreenHeight;
        framebuffer_create_info.layers = 1;
        framebuffer_create_info.renderPass = renderpass_.get();
        framebuffer_create_info.attachmentCount = 1;
        framebuffer_create_info.pAttachments = vk_frame_buf_attachments;

        framebuffer_ = device_->createFramebufferUnique(framebuffer_create_info);
    }

    void CreatePipeline()
    {
        vk::Viewport vk_viewports[1];
        vk_viewports[0].x = 0.0;
        vk_viewports[0].y = 0.0;
        vk_viewports[0].minDepth = 0.0;
        vk_viewports[0].maxDepth = 1.0;
        vk_viewports[0].width = kScreenWidth;
        vk_viewports[0].height = kScreenHeight;

        vk::Rect2D vk_scissors[1];
        vk_scissors[0].offset = VkOffset2D{ 0, 0 };
        vk_scissors[0].extent = VkExtent2D{ kScreenWidth, kScreenHeight };

        vk::PipelineViewportStateCreateInfo vk_pipeline_viewport_state_create_info;
        vk_pipeline_viewport_state_create_info.viewportCount = 1;
        vk_pipeline_viewport_state_create_info.pViewports = vk_viewports;
        vk_pipeline_viewport_state_create_info.scissorCount = 1;
        vk_pipeline_viewport_state_create_info.pScissors = vk_scissors;

        vk::PipelineVertexInputStateCreateInfo vk_pipeline_vertex_input_state_create_info;
        vk_pipeline_vertex_input_state_create_info.vertexAttributeDescriptionCount = 0;
        vk_pipeline_vertex_input_state_create_info.pVertexAttributeDescriptions = nullptr;
        vk_pipeline_vertex_input_state_create_info.vertexBindingDescriptionCount = 0;
        vk_pipeline_vertex_input_state_create_info.pVertexBindingDescriptions = nullptr;

        vk::PipelineInputAssemblyStateCreateInfo vk_pipeline_input_assembly_state_create_info;
        vk_pipeline_input_assembly_state_create_info.topology = vk::PrimitiveTopology::eTriangleList;
        vk_pipeline_input_assembly_state_create_info.primitiveRestartEnable = false;

        vk::PipelineRasterizationStateCreateInfo vk_pipeline_rasterization_state_create_info;
        vk_pipeline_rasterization_state_create_info.depthClampEnable = false;
        vk_pipeline_rasterization_state_create_info.rasterizerDiscardEnable = false;
        vk_pipeline_rasterization_state_create_info.polygonMode = vk::PolygonMode::eFill;
        vk_pipeline_rasterization_state_create_info.lineWidth = 1.0f;
        vk_pipeline_rasterization_state_create_info.cullMode = vk::CullModeFlagBits::eBack;
        vk_pipeline_rasterization_state_create_info.frontFace = vk::FrontFace::eClockwise;
        vk_pipeline_rasterization_state_create_info.depthBiasEnable = false;

        vk::PipelineMultisampleStateCreateInfo vk_pipeline_multisample_state_create_info;
        vk_pipeline_multisample_state_create_info.sampleShadingEnable = false;
        vk_pipeline_multisample_state_create_info.rasterizationSamples = vk::SampleCountFlagBits::e1;

        vk::PipelineColorBlendAttachmentState vk_pipeline_color_blend_attachment_states[1];
        vk_pipeline_color_blend_attachment_states[0].colorWriteMask =
            vk::ColorComponentFlagBits::eA |
            vk::ColorComponentFlagBits::eR |
            vk::ColorComponentFlagBits::eG |
            vk::ColorComponentFlagBits::eB;
        vk_pipeline_color_blend_attachment_states[0].blendEnable = false;

        vk::PipelineColorBlendStateCreateInfo blend;
        blend.logicOpEnable = false;
        blend.attachmentCount = 1;
        blend.pAttachments = vk_pipeline_color_blend_attachment_states;

        vk::PipelineLayoutCreateInfo vk_pipeline_layout_create_info;
        vk_pipeline_layout_create_info.setLayoutCount = 0;
        vk_pipeline_layout_create_info.pSetLayouts = nullptr;

        vk::UniquePipelineLayout vk_pipeline_layout = device_->createPipelineLayoutUnique(vk_pipeline_layout_create_info);

        vk::PipelineShaderStageCreateInfo vk_pipeline_shader_stage_create_infos[2];
        vk_pipeline_shader_stage_create_infos[0].stage = vk::ShaderStageFlagBits::eVertex;
        vk_pipeline_shader_stage_create_infos[0].module = vert_shader_.get();
        vk_pipeline_shader_stage_create_infos[0].pName = "main";
        vk_pipeline_shader_stage_create_infos[1].stage = vk::ShaderStageFlagBits::eFragment;
        vk_pipeline_shader_stage_create_infos[1].module = frag_shader_.get();
        vk_pipeline_shader_stage_create_infos[1].pName = "main";

        vk::GraphicsPipelineCreateInfo vk_graphics_pipeline_create_info;
        vk_graphics_pipeline_create_info.pViewportState = &vk_pipeline_viewport_state_create_info;
        vk_graphics_pipeline_create_info.pVertexInputState = &vk_pipeline_vertex_input_state_create_info;
        vk_graphics_pipeline_create_info.pInputAssemblyState = &vk_pipeline_input_assembly_state_create_info;
        vk_graphics_pipeline_create_info.pRasterizationState = &vk_pipeline_rasterization_state_create_info;
        vk_graphics_pipeline_create_info.pMultisampleState = &vk_pipeline_multisample_state_create_info;
        vk_graphics_pipeline_create_info.pColorBlendState = &blend;
        vk_graphics_pipeline_create_info.layout = vk_pipeline_layout.get();
        vk_graphics_pipeline_create_info.renderPass = renderpass_.get();
        vk_graphics_pipeline_create_info.subpass = 0;
        vk_graphics_pipeline_create_info.stageCount = 2;
        vk_graphics_pipeline_create_info.pStages = vk_pipeline_shader_stage_create_infos;

        pipeline_ = device_->createGraphicsPipelineUnique(nullptr, vk_graphics_pipeline_create_info).value;
    }

    void LoadVertShader()
    {
        const size_t vert_spv_file_sz = std::filesystem::file_size("SampleShader\\VertexSample.spv");

        std::ifstream vert_spv_file("SampleShader\\VertexSample.spv", std::ios_base::binary);

        std::vector<char> vert_spv_file_data(vert_spv_file_sz);
        vert_spv_file.read(vert_spv_file_data.data(), vert_spv_file_sz);

        vk::ShaderModuleCreateInfo vk_vert_shader_create_info;
        vk_vert_shader_create_info.codeSize = vert_spv_file_sz;
        vk_vert_shader_create_info.pCode = reinterpret_cast<const uint32_t*>(vert_spv_file_data.data());

        vert_shader_ = device_->createShaderModuleUnique(vk_vert_shader_create_info);
    }

    void LoadFragmentShader()
    {
	    const size_t frag_spv_file_sz = std::filesystem::file_size("SampleShader\\FragmentSample.spv");

        std::ifstream frag_spv_file("SampleShader\\FragmentSample.spv", std::ios_base::binary);

        std::vector<char> frag_spv_file_data(frag_spv_file_sz);
        frag_spv_file.read(frag_spv_file_data.data(), frag_spv_file_sz);

        vk::ShaderModuleCreateInfo frag_shader_create_info;
        frag_shader_create_info.codeSize = frag_spv_file_sz;
        frag_shader_create_info.pCode = reinterpret_cast<const uint32_t*>(frag_spv_file_data.data());

        frag_shader_ = device_->createShaderModuleUnique(frag_shader_create_info);
    }

    void WriteImage()
    {
        void* image_data = device_->mapMemory(buffer_mem_.get(), 0, buffer_mem_req_.size);

        stbi_write_bmp("image.bmp", kScreenWidth, kScreenHeight, 4, image_data);

        device_->unmapMemory(buffer_mem_.get());
    }

    void InitVulkan()
    {
        // Vulkanインスタンスの作成
        CreateInstance();

        // 物理デバイスの取得
        GetPhysicalDevices();

#ifndef NDEBUG
        // 物理デバイスの表示
        PrintPhysicalDevices();
#endif

        // 物理デバイスの選択
        SelectPhysicalDevice();

        // 論理デバイスの作成と、キューの取得
        CreateLogicalDevice();

        // コマンドプールの作成
        CreateCommandPool();

        // コマンドバッファの作成
        CreateCommandBuffers();

        // イメージとイメージビューの作成
        CreateImage();
        CreateImageView();

        // バーテックスシェーダーとフラグメントシェーダーの読み込み
        LoadVertShader();
        LoadFragmentShader();

        // レンダーパスの作成
        CreateRenderPass();

        // フレームバッファの作成
        CreateFrameBuffer();

        // パイプラインの作成
        CreatePipeline();

        vk::CommandBufferBeginInfo cmd_begin_info;
        cmd_bufs_[0]->begin(cmd_begin_info);

        vk::ClearValue clear_val[1];
        clear_val[0].color.float32[0] = 0.0f;
        clear_val[0].color.float32[1] = 1.0f;
        clear_val[0].color.float32[2] = 0.0f;
        clear_val[0].color.float32[3] = 1.0f;

        vk::RenderPassBeginInfo vk_render_pass_begin;
        vk_render_pass_begin.renderPass = renderpass_.get();
        vk_render_pass_begin.framebuffer = framebuffer_.get();
        vk_render_pass_begin.renderArea = vk::Rect2D({ 0,0 }, { kScreenWidth, kScreenHeight });
        vk_render_pass_begin.clearValueCount = 1;
        vk_render_pass_begin.pClearValues = clear_val;

        cmd_bufs_[0]->beginRenderPass(vk_render_pass_begin, vk::SubpassContents::eInline);

        // ここでサブパス0番の処理

        cmd_bufs_[0]->bindPipeline(vk::PipelineBindPoint::eGraphics, pipeline_.get());

        cmd_bufs_[0]->draw(3, 1, 0, 0);

        cmd_bufs_[0]->endRenderPass();

        cmd_bufs_[0]->copyImageToBuffer(
            image_.get(), 
            vk::ImageLayout::eGeneral, 
            buffer_.get(), 
            vk::BufferImageCopy{ 0, kScreenWidth, kScreenHeight, vk::ImageSubresourceLayers{vk::ImageAspectFlagBits::eColor, 0, 0, 1}, vk::Offset3D{0, 0, 0}, vk::Extent3D{kScreenWidth, kScreenHeight, 1} }
        );


        cmd_bufs_[0]->end();

        const vk::CommandBuffer submit_cmd_buf[1] = { cmd_bufs_[0].get() };
        vk::SubmitInfo submitInfo;
        submitInfo.commandBufferCount = 1;
        submitInfo.pCommandBuffers = submit_cmd_buf;

        graphics_queue_.submit({ submitInfo }, nullptr);
        
        graphics_queue_.waitIdle();

        // 書き出し
        WriteImage();
    }

    /**
     * @brief オブジェクトのクリーンアップ
     */
    void CleanUp()
    {
    }
};

int main() {
	try
	{
		App app;
		app.run();
    }
    catch (vk::SystemError& err)
    {
        std::cerr << "vk::SystemError: " << err.what() << std::endl;
        return EXIT_FAILURE;
    }
    catch (std::exception& err)
    {
        std::cerr << "std::exception: " << err.what() << std::endl;
        return EXIT_FAILURE;
    }
    catch (...)
    {
        std::cerr << "unknown error\n";
        return EXIT_FAILURE;
    }

    return EXIT_SUCCESS;
}