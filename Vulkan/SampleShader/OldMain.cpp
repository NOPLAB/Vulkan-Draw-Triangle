#include <GLFW/glfw3.h>

#define VK_USE_PLATFORM_WIN32_KHR
#define GLFW_INCLUDE_VULKAN

#include <vulkan/vulkan.hpp>
#include <vector>
#include <iostream>

const uint32_t WIDTH = 800;
const uint32_t HEIGHT = 600;

static std::string AppName = "Vulkan Test";
static std::string EngineName = "Vulkan.hpp";

int main() {
    try
    {
        const auto required_layers = { "VK_LAYER_KHRONOS_validation" };

        // vk::ApplicationInfoのインスタンス化
        const vk::ApplicationInfo vk_application_info(AppName.c_str(), 1, EngineName.c_str(), 1, VK_API_VERSION_1_1);

        // vk::InstanceCreateInfoのインスタンス化
        vk::InstanceCreateInfo instanceCreateInfo({}, &vk_application_info);

        instanceCreateInfo.enabledLayerCount = required_layers.size();
        instanceCreateInfo.ppEnabledLayerNames = required_layers.begin();

        // Vulkanのインスタンス化
        const vk::Instance vk_instance = vk::createInstance(instanceCreateInfo);

        std::vector<vk::PhysicalDevice> physical_devices = vk_instance.enumeratePhysicalDevices();

        // デバイス名とすべてのキューファミリの情報を列挙
        VkPhysicalDeviceProperties physical_device_properties;
        for (size_t i = 0; i < physical_devices.size(); i++)
        {
            physical_device_properties = physical_devices[i].getProperties();
            std::cout << physical_device_properties.deviceName << std::endl;

            std::vector<vk::QueueFamilyProperties> queueProps = physical_devices[i].getQueueFamilyProperties();

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

        // グラフィックスをサポートするキューファミリを最低でも1つ持っている物理デバイスを選択する

        // 物理デバイス
        vk::PhysicalDevice vk_physical_device;

        // グラフィックスをサポートするキューファミリを持っている物理デバイスが存在するか
        bool exists_suitable_physical_device = false;

        // グラフィックスをサポートするキューファミリのインデックス（1つ）
        uint32_t graphics_queue_family_index;

        for (size_t i = 0; i < physical_devices.size(); i++)
        {
            std::vector<vk::QueueFamilyProperties> queueProps = physical_devices[i].getQueueFamilyProperties();
            bool existsGraphicsQueue = false;

            for (size_t j = 0; j < queueProps.size(); j++)
            {
                if (queueProps[j].queueFlags & vk::QueueFlagBits::eGraphics)
                {
                    existsGraphicsQueue = true;
                    graphics_queue_family_index = j;
                    break;
                }
            }

            if (existsGraphicsQueue)
            {
                vk_physical_device = physical_devices[i];
                exists_suitable_physical_device = true;
                break;
            }
        }

        if (!exists_suitable_physical_device)
        {
            std::cerr << "使用可能な物理デバイスがありません" << std::endl;
            return -1;
        }

        vk::DeviceCreateInfo vk_dev_create_info;

        // 論理デバイスにキューの使用を伝える
        vk::DeviceQueueCreateInfo queueCreateInfo[1];
        queueCreateInfo[0].queueFamilyIndex = graphics_queue_family_index;
        queueCreateInfo[0].queueCount = 1;
        const float queue_priorities[1] = { 1.0f };
        queueCreateInfo[0].pQueuePriorities = queue_priorities;

        vk_dev_create_info.pQueueCreateInfos = queueCreateInfo;
        vk_dev_create_info.queueCreateInfoCount = 1;

        // バリデーションレイヤー
        vk_dev_create_info.enabledLayerCount = required_layers.size();
        vk_dev_create_info.ppEnabledLayerNames = required_layers.begin();

        // 論理デバイスの作成
        const vk::Device vk_device = vk_physical_device.createDevice(vk_dev_create_info);

        // キューの取得
        vk::Queue graphicsQueue = vk_device.getQueue(graphics_queue_family_index, 0);

        // destroy it again
        vk_device.destroy();
        vk_instance.destroy();
    }
    catch (vk::SystemError& err)
    {
        std::cerr << "vk::SystemError: " << err.what() << std::endl;
        return -1;
    }
    catch (std::exception& err)
    {
        std::cerr << "std::exception: " << err.what() << std::endl;
        return -1;
    }
    catch (...)
    {
        std::cerr << "unknown error\n";
        return -1;
    }

    return 0;
}