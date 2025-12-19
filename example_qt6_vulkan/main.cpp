#include <QGuiApplication>
#include <QQmlApplicationEngine>
#include <QQmlContext>
#include <QQuickWindow>
#include <QQuickGraphicsDevice>
#include <QVulkanInstance>
#include <QVulkanFunctions>
#include <QtWebEngineQuick/QtWebEngineQuick>

#include <vulkan/vulkan.h>
#include <cstdio>
#include <vector>

int main(int argc, char* argv[])
{
    // Must initialize QtWebEngine before QGuiApplication
    QtWebEngineQuick::initialize();

    QGuiApplication app(argc, argv);

    if (argc < 2) {
        fprintf(stderr, "Usage: %s <video-file>\n", argv[0]);
        return 1;
    }

    QString videoFile = QString::fromUtf8(argv[1]);

    // Use Vulkan for mpv rendering
    QQuickWindow::setGraphicsApi(QSGRendererInterface::Vulkan);

    // Create QVulkanInstance with Vulkan 1.2 API
    QVulkanInstance vulkanInstance;
    vulkanInstance.setApiVersion(QVersionNumber(1, 2));
    if (!vulkanInstance.create()) {
        qFatal("Failed to create Vulkan instance");
        return 1;
    }

    // Get Vulkan functions from Qt
    QVulkanFunctions *vkFuncs = vulkanInstance.functions();

    // Get the native VkInstance
    VkInstance vkInstance = vulkanInstance.vkInstance();

    // Enumerate physical devices and pick the first one
    uint32_t deviceCount = 0;
    vkFuncs->vkEnumeratePhysicalDevices(vkInstance, &deviceCount, nullptr);
    if (deviceCount == 0) {
        qFatal("No Vulkan-capable GPU found");
        return 1;
    }

    std::vector<VkPhysicalDevice> physicalDevices(deviceCount);
    vkFuncs->vkEnumeratePhysicalDevices(vkInstance, &deviceCount, physicalDevices.data());
    VkPhysicalDevice physicalDevice = physicalDevices[0];

    // Find graphics queue family
    uint32_t queueFamilyCount = 0;
    vkFuncs->vkGetPhysicalDeviceQueueFamilyProperties(physicalDevice, &queueFamilyCount, nullptr);

    std::vector<VkQueueFamilyProperties> queueFamilies(queueFamilyCount);
    vkFuncs->vkGetPhysicalDeviceQueueFamilyProperties(physicalDevice, &queueFamilyCount, queueFamilies.data());

    uint32_t graphicsQueueFamily = 0;
    for (uint32_t i = 0; i < queueFamilyCount; i++) {
        if (queueFamilies[i].queueFlags & VK_QUEUE_GRAPHICS_BIT) {
            graphicsQueueFamily = i;
            break;
        }
    }

    // Set up queue creation info
    float queuePriority = 1.0f;
    VkDeviceQueueCreateInfo queueCreateInfo{};
    queueCreateInfo.sType = VK_STRUCTURE_TYPE_DEVICE_QUEUE_CREATE_INFO;
    queueCreateInfo.queueFamilyIndex = graphicsQueueFamily;
    queueCreateInfo.queueCount = 1;
    queueCreateInfo.pQueuePriorities = &queuePriority;

    // Query supported features first (into separate structures)
    VkPhysicalDeviceVulkan12Features queryVk12{};
    queryVk12.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_VULKAN_1_2_FEATURES;

    VkPhysicalDeviceFeatures2 queryFeatures{};
    queryFeatures.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_FEATURES_2;
    queryFeatures.pNext = &queryVk12;

    auto vkGetPhysicalDeviceFeatures2 = reinterpret_cast<PFN_vkGetPhysicalDeviceFeatures2>(
        vulkanInstance.getInstanceProcAddr("vkGetPhysicalDeviceFeatures2"));
    if (vkGetPhysicalDeviceFeatures2) {
        vkGetPhysicalDeviceFeatures2(physicalDevice, &queryFeatures);
    }

    qDebug() << "Device supports hostQueryReset:" << queryVk12.hostQueryReset;
    qDebug() << "Device supports timelineSemaphore:" << queryVk12.timelineSemaphore;

    // Now build our feature chain with the features we want enabled
    VkPhysicalDeviceVulkan12Features vulkan12Features{};
    vulkan12Features.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_VULKAN_1_2_FEATURES;
    vulkan12Features.hostQueryReset = VK_TRUE;
    vulkan12Features.timelineSemaphore = VK_TRUE;

    VkPhysicalDeviceFeatures2 features2{};
    features2.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_FEATURES_2;
    features2.pNext = &vulkan12Features;

    // Required extensions
    const char* deviceExtensions[] = {
        VK_KHR_SWAPCHAIN_EXTENSION_NAME,
    };

    // Create logical device
    VkDeviceCreateInfo deviceCreateInfo{};
    deviceCreateInfo.sType = VK_STRUCTURE_TYPE_DEVICE_CREATE_INFO;
    deviceCreateInfo.pNext = &features2;
    deviceCreateInfo.queueCreateInfoCount = 1;
    deviceCreateInfo.pQueueCreateInfos = &queueCreateInfo;
    deviceCreateInfo.enabledExtensionCount = sizeof(deviceExtensions) / sizeof(deviceExtensions[0]);
    deviceCreateInfo.ppEnabledExtensionNames = deviceExtensions;

    // Get vkCreateDevice function
    auto vkCreateDevice = reinterpret_cast<PFN_vkCreateDevice>(
        vulkanInstance.getInstanceProcAddr("vkCreateDevice"));
    if (!vkCreateDevice) {
        qFatal("Failed to get vkCreateDevice");
        return 1;
    }

    VkDevice device;
    VkResult result = vkCreateDevice(physicalDevice, &deviceCreateInfo, nullptr, &device);
    if (result != VK_SUCCESS) {
        qFatal("Failed to create Vulkan device: %d", result);
        return 1;
    }

    qDebug() << "Created Vulkan device with hostQueryReset enabled";

    int ret;
    {
        // Create QML engine
        QQmlApplicationEngine engine;

        // Pass video file to QML
        engine.rootContext()->setContextProperty("videoFile", videoFile);

        // Connect to window creation to set up custom graphics device
        QObject::connect(&engine, &QQmlApplicationEngine::objectCreated, [&](QObject *obj, const QUrl &) {
            if (!obj) return;
            QQuickWindow *window = qobject_cast<QQuickWindow*>(obj);
            if (window) {
                window->setVulkanInstance(&vulkanInstance);
                window->setGraphicsDevice(QQuickGraphicsDevice::fromDeviceObjects(
                    physicalDevice, device, graphicsQueueFamily, 0));
                qDebug() << "Set custom Vulkan device on window";
            }
        });

        // Load QML from module
        engine.loadFromModule("Example", "Main");

        ret = app.exec();
    } // engine destroyed here, before VkDevice cleanup

    // Cleanup - get vkDestroyDevice
    auto vkDestroyDevice = reinterpret_cast<PFN_vkDestroyDevice>(
        vulkanInstance.getInstanceProcAddr("vkDestroyDevice"));
    if (vkDestroyDevice) {
        vkDestroyDevice(device, nullptr);
    }

    return ret;
}
