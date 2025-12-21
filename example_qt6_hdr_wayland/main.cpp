// Minimal WebEngine overlay using Wayland subsurfaces
// mpv renders to a subsurface below, WebEngine renders on top

#define VK_USE_PLATFORM_WAYLAND_KHR
#include <vulkan/vulkan.h>

#include <QApplication>
#include <QWidget>
#include <QWindow>
#include <QWebEngineView>
#include <QVBoxLayout>
#include <qpa/qplatformnativeinterface.h>

#include <wayland-client.h>
#include <mpv/client.h>
#include <mpv/render_vk.h>

#include <cstdio>
#include <cstring>
#include <clocale>
#include <algorithm>
#include <vector>
#include <thread>
#include <atomic>

// Wayland globals
static struct wl_display *wl_display = nullptr;
static struct wl_compositor *wl_compositor = nullptr;
static struct wl_subcompositor *wl_subcompositor = nullptr;
static struct wl_surface *mpv_surface = nullptr;
static struct wl_subsurface *mpv_subsurface = nullptr;

// Vulkan for mpv
static VkInstance vk_instance = VK_NULL_HANDLE;
static VkPhysicalDevice vk_physical_device = VK_NULL_HANDLE;
static VkDevice vk_device = VK_NULL_HANDLE;
static VkQueue vk_queue = VK_NULL_HANDLE;
static uint32_t vk_queue_family = 0;
static VkSurfaceKHR vk_surface = VK_NULL_HANDLE;
static VkSwapchainKHR vk_swapchain = VK_NULL_HANDLE;
static std::vector<VkImage> swapchain_images;
static std::vector<VkImageView> swapchain_views;
static VkFormat swapchain_format;
static VkColorSpaceKHR swapchain_colorspace;
static int sw_width = 1280, sw_height = 720;

// mpv
static mpv_handle *mpv = nullptr;
static mpv_render_context *mpv_render = nullptr;
static std::atomic<bool> running{true};
static std::atomic<bool> needs_resize{false};
static std::atomic<int> pending_width{0};
static std::atomic<int> pending_height{0};

// Device extensions - match standalone test
static const char *device_exts[] = {
    VK_KHR_SWAPCHAIN_EXTENSION_NAME,
    VK_KHR_TIMELINE_SEMAPHORE_EXTENSION_NAME,
    VK_EXT_HDR_METADATA_EXTENSION_NAME,
    VK_KHR_EXTERNAL_MEMORY_EXTENSION_NAME,
    VK_KHR_EXTERNAL_MEMORY_FD_EXTENSION_NAME,
    VK_EXT_EXTERNAL_MEMORY_DMA_BUF_EXTENSION_NAME,
    VK_EXT_IMAGE_DRM_FORMAT_MODIFIER_EXTENSION_NAME,
    VK_KHR_IMAGE_FORMAT_LIST_EXTENSION_NAME,
    VK_KHR_SAMPLER_YCBCR_CONVERSION_EXTENSION_NAME,
    VK_KHR_BIND_MEMORY_2_EXTENSION_NAME,
    VK_KHR_GET_MEMORY_REQUIREMENTS_2_EXTENSION_NAME,
    VK_KHR_MAINTENANCE_1_EXTENSION_NAME,
};
static const int num_device_exts = sizeof(device_exts) / sizeof(device_exts[0]);
static VkPhysicalDeviceVulkan12Features vk12_features;
static VkPhysicalDeviceFeatures2 features2;

static void check_vk(VkResult result, const char *msg) {
    if (result != VK_SUCCESS) {
        fprintf(stderr, "Vulkan error %d: %s\n", result, msg);
        exit(1);
    }
}

static void registry_global(void *, struct wl_registry *registry, uint32_t name,
                            const char *interface, uint32_t) {
    if (strcmp(interface, wl_compositor_interface.name) == 0) {
        wl_compositor = (struct wl_compositor *)wl_registry_bind(registry, name, &wl_compositor_interface, 4);
    } else if (strcmp(interface, wl_subcompositor_interface.name) == 0) {
        wl_subcompositor = (struct wl_subcompositor *)wl_registry_bind(registry, name, &wl_subcompositor_interface, 1);
    }
}

static void registry_global_remove(void *, struct wl_registry *, uint32_t) {}

static const struct wl_registry_listener registry_listener = {
    .global = registry_global,
    .global_remove = registry_global_remove,
};

static void create_vulkan_for_mpv() {
    // Instance - match standalone test
    const char *instance_exts[] = {
        VK_KHR_SURFACE_EXTENSION_NAME,
        VK_KHR_WAYLAND_SURFACE_EXTENSION_NAME,
        VK_EXT_SWAPCHAIN_COLOR_SPACE_EXTENSION_NAME,
        VK_KHR_GET_PHYSICAL_DEVICE_PROPERTIES_2_EXTENSION_NAME,
        VK_KHR_EXTERNAL_MEMORY_CAPABILITIES_EXTENSION_NAME,
    };
    VkInstanceCreateInfo instance_info = {VK_STRUCTURE_TYPE_INSTANCE_CREATE_INFO};
    VkApplicationInfo app_info = {VK_STRUCTURE_TYPE_APPLICATION_INFO};
    app_info.apiVersion = VK_API_VERSION_1_3;
    instance_info.pApplicationInfo = &app_info;
    instance_info.enabledExtensionCount = 5;
    instance_info.ppEnabledExtensionNames = instance_exts;
    check_vk(vkCreateInstance(&instance_info, nullptr, &vk_instance), "vkCreateInstance");

    // Physical device
    uint32_t gpu_count = 0;
    vkEnumeratePhysicalDevices(vk_instance, &gpu_count, nullptr);
    std::vector<VkPhysicalDevice> gpus(gpu_count);
    vkEnumeratePhysicalDevices(vk_instance, &gpu_count, gpus.data());
    vk_physical_device = gpus[0];

    // Queue family
    uint32_t queue_family_count = 0;
    vkGetPhysicalDeviceQueueFamilyProperties(vk_physical_device, &queue_family_count, nullptr);
    std::vector<VkQueueFamilyProperties> queue_families(queue_family_count);
    vkGetPhysicalDeviceQueueFamilyProperties(vk_physical_device, &queue_family_count, queue_families.data());
    for (uint32_t i = 0; i < queue_family_count; i++) {
        if (queue_families[i].queueFlags & VK_QUEUE_GRAPHICS_BIT) {
            vk_queue_family = i;
            break;
        }
    }

    // Device
    float queue_priority = 1.0f;
    VkDeviceQueueCreateInfo queue_info = {VK_STRUCTURE_TYPE_DEVICE_QUEUE_CREATE_INFO};
    queue_info.queueFamilyIndex = vk_queue_family;
    queue_info.queueCount = 1;
    queue_info.pQueuePriorities = &queue_priority;

    static VkPhysicalDeviceVulkan11Features vk11_features = {VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_VULKAN_1_1_FEATURES};
    vk11_features.samplerYcbcrConversion = VK_TRUE;

    vk12_features = {VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_VULKAN_1_2_FEATURES};
    vk12_features.pNext = &vk11_features;
    vk12_features.timelineSemaphore = VK_TRUE;
    vk12_features.hostQueryReset = VK_TRUE;

    features2 = {VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_FEATURES_2};
    features2.pNext = &vk12_features;

    VkDeviceCreateInfo device_info = {VK_STRUCTURE_TYPE_DEVICE_CREATE_INFO};
    device_info.pNext = &features2;
    device_info.queueCreateInfoCount = 1;
    device_info.pQueueCreateInfos = &queue_info;
    device_info.enabledExtensionCount = num_device_exts;
    device_info.ppEnabledExtensionNames = device_exts;
    check_vk(vkCreateDevice(vk_physical_device, &device_info, nullptr, &vk_device), "vkCreateDevice");

    vkGetDeviceQueue(vk_device, vk_queue_family, 0, &vk_queue);

    // Create Vulkan surface from our wl_surface
    VkWaylandSurfaceCreateInfoKHR wayland_surface_info = {VK_STRUCTURE_TYPE_WAYLAND_SURFACE_CREATE_INFO_KHR};
    wayland_surface_info.display = wl_display;
    wayland_surface_info.surface = mpv_surface;
    check_vk(vkCreateWaylandSurfaceKHR(vk_instance, &wayland_surface_info, nullptr, &vk_surface), "vkCreateWaylandSurfaceKHR");

    fprintf(stderr, "*** Created Vulkan context for mpv subsurface ***\n");
}

static void create_swapchain() {
    // Find HDR10 format
    uint32_t format_count = 0;
    vkGetPhysicalDeviceSurfaceFormatsKHR(vk_physical_device, vk_surface, &format_count, nullptr);
    std::vector<VkSurfaceFormatKHR> formats(format_count);
    vkGetPhysicalDeviceSurfaceFormatsKHR(vk_physical_device, vk_surface, &format_count, formats.data());

    swapchain_format = VK_FORMAT_B8G8R8A8_UNORM;
    swapchain_colorspace = VK_COLOR_SPACE_SRGB_NONLINEAR_KHR;

    for (const auto &fmt : formats) {
        if (fmt.colorSpace == VK_COLOR_SPACE_HDR10_ST2084_EXT) {
            swapchain_format = fmt.format;
            swapchain_colorspace = fmt.colorSpace;
            fprintf(stderr, "*** Found HDR10 format: %d ***\n", fmt.format);
            break;
        }
    }

    VkSurfaceCapabilitiesKHR caps;
    vkGetPhysicalDeviceSurfaceCapabilitiesKHR(vk_physical_device, vk_surface, &caps);

    VkSwapchainCreateInfoKHR swapchain_info = {VK_STRUCTURE_TYPE_SWAPCHAIN_CREATE_INFO_KHR};
    swapchain_info.surface = vk_surface;
    swapchain_info.minImageCount = caps.minImageCount + 1;
    swapchain_info.imageFormat = swapchain_format;
    swapchain_info.imageColorSpace = swapchain_colorspace;
    swapchain_info.imageExtent = {(uint32_t)sw_width, (uint32_t)sw_height};
    swapchain_info.imageArrayLayers = 1;
    swapchain_info.imageUsage = VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT | VK_IMAGE_USAGE_TRANSFER_DST_BIT;
    swapchain_info.preTransform = caps.currentTransform;
    swapchain_info.compositeAlpha = VK_COMPOSITE_ALPHA_OPAQUE_BIT_KHR;
    swapchain_info.presentMode = VK_PRESENT_MODE_FIFO_KHR;
    swapchain_info.clipped = VK_TRUE;
    check_vk(vkCreateSwapchainKHR(vk_device, &swapchain_info, nullptr, &vk_swapchain), "vkCreateSwapchainKHR");

    uint32_t image_count = 0;
    vkGetSwapchainImagesKHR(vk_device, vk_swapchain, &image_count, nullptr);
    swapchain_images.resize(image_count);
    vkGetSwapchainImagesKHR(vk_device, vk_swapchain, &image_count, swapchain_images.data());

    swapchain_views.resize(image_count);
    for (uint32_t i = 0; i < image_count; i++) {
        VkImageViewCreateInfo view_info = {VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO};
        view_info.image = swapchain_images[i];
        view_info.viewType = VK_IMAGE_VIEW_TYPE_2D;
        view_info.format = swapchain_format;
        view_info.subresourceRange = {VK_IMAGE_ASPECT_COLOR_BIT, 0, 1, 0, 1};
        check_vk(vkCreateImageView(vk_device, &view_info, nullptr, &swapchain_views[i]), "vkCreateImageView");
    }

    fprintf(stderr, "*** Swapchain created: %dx%d, HDR=%d ***\n", sw_width, sw_height,
            swapchain_colorspace == VK_COLOR_SPACE_HDR10_ST2084_EXT);

    // Set HDR metadata
    if (swapchain_colorspace == VK_COLOR_SPACE_HDR10_ST2084_EXT) {
        auto vkSetHdrMetadataEXT = (PFN_vkSetHdrMetadataEXT)vkGetDeviceProcAddr(vk_device, "vkSetHdrMetadataEXT");
        if (vkSetHdrMetadataEXT) {
            VkHdrMetadataEXT hdr_metadata = {VK_STRUCTURE_TYPE_HDR_METADATA_EXT};
            // BT.2020 primaries
            hdr_metadata.displayPrimaryRed = {0.708f, 0.292f};
            hdr_metadata.displayPrimaryGreen = {0.170f, 0.797f};
            hdr_metadata.displayPrimaryBlue = {0.131f, 0.046f};
            hdr_metadata.whitePoint = {0.3127f, 0.3290f};  // D65
            hdr_metadata.maxLuminance = 1000.0f;
            hdr_metadata.minLuminance = 0.001f;
            hdr_metadata.maxContentLightLevel = 1000.0f;
            hdr_metadata.maxFrameAverageLightLevel = 200.0f;
            vkSetHdrMetadataEXT(vk_device, 1, &vk_swapchain, &hdr_metadata);
            fprintf(stderr, "*** HDR metadata set ***\n");
        }
    }
}

static void recreate_swapchain(int new_width, int new_height) {
    vkDeviceWaitIdle(vk_device);

    // Destroy old views and swapchain
    for (auto view : swapchain_views) {
        vkDestroyImageView(vk_device, view, nullptr);
    }
    swapchain_views.clear();
    swapchain_images.clear();

    VkSwapchainKHR old_swapchain = vk_swapchain;

    VkSurfaceCapabilitiesKHR caps;
    vkGetPhysicalDeviceSurfaceCapabilitiesKHR(vk_physical_device, vk_surface, &caps);

    // Clamp to surface capabilities
    sw_width = std::max(caps.minImageExtent.width, std::min(caps.maxImageExtent.width, (uint32_t)new_width));
    sw_height = std::max(caps.minImageExtent.height, std::min(caps.maxImageExtent.height, (uint32_t)new_height));

    VkSwapchainCreateInfoKHR swapchain_info = {VK_STRUCTURE_TYPE_SWAPCHAIN_CREATE_INFO_KHR};
    swapchain_info.surface = vk_surface;
    swapchain_info.minImageCount = caps.minImageCount + 1;
    swapchain_info.imageFormat = swapchain_format;
    swapchain_info.imageColorSpace = swapchain_colorspace;
    swapchain_info.imageExtent = {(uint32_t)sw_width, (uint32_t)sw_height};
    swapchain_info.imageArrayLayers = 1;
    swapchain_info.imageUsage = VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT | VK_IMAGE_USAGE_TRANSFER_DST_BIT;
    swapchain_info.preTransform = caps.currentTransform;
    swapchain_info.compositeAlpha = VK_COMPOSITE_ALPHA_OPAQUE_BIT_KHR;
    swapchain_info.presentMode = VK_PRESENT_MODE_FIFO_KHR;
    swapchain_info.clipped = VK_TRUE;
    swapchain_info.oldSwapchain = old_swapchain;
    check_vk(vkCreateSwapchainKHR(vk_device, &swapchain_info, nullptr, &vk_swapchain), "vkCreateSwapchainKHR");

    vkDestroySwapchainKHR(vk_device, old_swapchain, nullptr);

    uint32_t image_count = 0;
    vkGetSwapchainImagesKHR(vk_device, vk_swapchain, &image_count, nullptr);
    swapchain_images.resize(image_count);
    vkGetSwapchainImagesKHR(vk_device, vk_swapchain, &image_count, swapchain_images.data());

    swapchain_views.resize(image_count);
    for (uint32_t i = 0; i < image_count; i++) {
        VkImageViewCreateInfo view_info = {VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO};
        view_info.image = swapchain_images[i];
        view_info.viewType = VK_IMAGE_VIEW_TYPE_2D;
        view_info.format = swapchain_format;
        view_info.subresourceRange = {VK_IMAGE_ASPECT_COLOR_BIT, 0, 1, 0, 1};
        check_vk(vkCreateImageView(vk_device, &view_info, nullptr, &swapchain_views[i]), "vkCreateImageView");
    }

    fprintf(stderr, "*** Swapchain resized: %dx%d ***\n", sw_width, sw_height);
}

static void create_mpv_render() {
    // Qt resets locale, set it again before mpv
    setlocale(LC_NUMERIC, "C");

    mpv = mpv_create();
    mpv_set_option_string(mpv, "vo", "libmpv");
    mpv_set_option_string(mpv, "terminal", "yes");

    // HDR settings - explicit for HDR10 swapchain
    mpv_set_option_string(mpv, "target-trc", "pq");
    mpv_set_option_string(mpv, "target-prim", "bt.2020");
    mpv_set_option_string(mpv, "target-peak", "1000");

    mpv_initialize(mpv);

    // Create render context
    mpv_vulkan_init_params vk_params = {};
    vk_params.instance = vk_instance;
    vk_params.physical_device = vk_physical_device;
    vk_params.device = vk_device;
    vk_params.graphics_queue = vk_queue;
    vk_params.graphics_queue_family = vk_queue_family;
    vk_params.get_instance_proc_addr = vkGetInstanceProcAddr;
    vk_params.features = &features2;
    vk_params.extensions = device_exts;
    vk_params.num_extensions = num_device_exts;

    mpv_render_param params[] = {
        {MPV_RENDER_PARAM_API_TYPE, (void *)MPV_RENDER_API_TYPE_VULKAN},
        {MPV_RENDER_PARAM_BACKEND, (void *)"gpu-next"},
        {MPV_RENDER_PARAM_VULKAN_INIT_PARAMS, &vk_params},
        {MPV_RENDER_PARAM_INVALID, nullptr}
    };

    int result = mpv_render_context_create(&mpv_render, mpv, params);
    if (result < 0) {
        fprintf(stderr, "Failed to create mpv render context: %s\n", mpv_error_string(result));
        exit(1);
    }
    fprintf(stderr, "*** mpv render context created ***\n");
}

static void render_loop() {
    while (running) {
        // Check for resize
        if (needs_resize) {
            int w = pending_width;
            int h = pending_height;
            if (w > 0 && h > 0) {
                recreate_swapchain(w, h);
            }
            needs_resize = false;
        }

        // Check mpv events
        while (1) {
            mpv_event *event = mpv_wait_event(mpv, 0);
            if (event->event_id == MPV_EVENT_NONE) break;
            if (event->event_id == MPV_EVENT_SHUTDOWN || event->event_id == MPV_EVENT_END_FILE)
                running = false;
        }

        if (!running) break;

        // Acquire swapchain image
        VkFence fence;
        VkFenceCreateInfo fence_info = {VK_STRUCTURE_TYPE_FENCE_CREATE_INFO};
        vkCreateFence(vk_device, &fence_info, nullptr, &fence);

        uint32_t image_idx;
        VkResult result = vkAcquireNextImageKHR(vk_device, vk_swapchain, 1000000000, VK_NULL_HANDLE, fence, &image_idx);
        if (result != VK_SUCCESS && result != VK_SUBOPTIMAL_KHR) {
            vkDestroyFence(vk_device, fence, nullptr);
            std::this_thread::sleep_for(std::chrono::milliseconds(16));
            continue;
        }

        vkWaitForFences(vk_device, 1, &fence, VK_TRUE, UINT64_MAX);
        vkDestroyFence(vk_device, fence, nullptr);

        // Render with mpv
        mpv_vulkan_fbo fbo = {};
        fbo.image = swapchain_images[image_idx];
        fbo.image_view = swapchain_views[image_idx];
        fbo.width = sw_width;
        fbo.height = sw_height;
        fbo.format = swapchain_format;
        fbo.current_layout = VK_IMAGE_LAYOUT_UNDEFINED;
        fbo.target_layout = VK_IMAGE_LAYOUT_PRESENT_SRC_KHR;

        int flip_y = 0;
        mpv_render_param render_params[] = {
            {MPV_RENDER_PARAM_VULKAN_FBO, &fbo},
            {MPV_RENDER_PARAM_FLIP_Y, &flip_y},
            {MPV_RENDER_PARAM_INVALID, nullptr}
        };

        mpv_render_context_render(mpv_render, render_params);

        // Present
        VkPresentInfoKHR present_info = {VK_STRUCTURE_TYPE_PRESENT_INFO_KHR};
        present_info.swapchainCount = 1;
        present_info.pSwapchains = &vk_swapchain;
        present_info.pImageIndices = &image_idx;
        vkQueuePresentKHR(vk_queue, &present_info);
    }

    mpv_render_context_free(mpv_render);
    mpv_terminate_destroy(mpv);
}

int main(int argc, char *argv[]) {
    if (argc < 2) {
        fprintf(stderr, "Usage: %s <video-file>\n", argv[0]);
        return 1;
    }

    // mpv requires C locale
    setlocale(LC_NUMERIC, "C");

    QApplication app(argc, argv);

    // Get Wayland display from Qt
    QPlatformNativeInterface *native = QGuiApplication::platformNativeInterface();
    wl_display = (struct wl_display *)native->nativeResourceForIntegration("wl_display");
    if (!wl_display) {
        fprintf(stderr, "Failed to get Wayland display from Qt\n");
        return 1;
    }
    fprintf(stderr, "*** Got wl_display from Qt: %p ***\n", (void*)wl_display);

    // Get compositor and subcompositor
    struct wl_registry *registry = wl_display_get_registry(wl_display);
    wl_registry_add_listener(registry, &registry_listener, nullptr);
    wl_display_roundtrip(wl_display);

    if (!wl_compositor || !wl_subcompositor) {
        fprintf(stderr, "Missing Wayland globals (compositor=%p, subcompositor=%p)\n",
                (void*)wl_compositor, (void*)wl_subcompositor);
        return 1;
    }

    // Create main widget with transparent background
    QWidget *mainWidget = new QWidget();
    mainWidget->setAttribute(Qt::WA_TranslucentBackground);
    mainWidget->setAttribute(Qt::WA_NoSystemBackground);
    mainWidget->setWindowTitle("MPV + WebEngine Overlay");
    mainWidget->resize(1280, 720);
    mainWidget->setStyleSheet("background: transparent;");

    // WebEngine overlay - transparent except for content
    QWebEngineView *webView = new QWebEngineView(mainWidget);
    webView->setAttribute(Qt::WA_TranslucentBackground);
    webView->setStyleSheet("background: transparent;");
    webView->page()->setBackgroundColor(Qt::transparent);
    webView->setHtml(R"(
<!DOCTYPE html>
<html>
<head>
    <meta charset="utf-8">
    <style>
        * {
            box-sizing: border-box;
        }
        html, body {
            margin: 0;
            padding: 0;
            width: 100%;
            height: 100%;
            overflow: hidden;
            font-family: sans-serif;
            color: #fff;
            user-select: none;
        }

        /* Demo overlay box */
        .overlay-box {
            position: absolute;
            top: 50%;
            left: 50%;
            transform: translate(-50%, -50%);
            background: rgba(0, 0, 0, 0.8);
            color: white;
            padding: 40px 60px;
            border-radius: 10px;
            border: 2px solid rgba(255, 255, 255, 0.3);
            text-align: center;
            box-shadow: 0 4px 20px rgba(0, 0, 0, 0.5);
        }

        h1 {
            margin: 0 0 15px 0;
            font-size: 28px;
            font-weight: 400;
        }

        p {
            margin: 8px 0;
            font-size: 14px;
            color: #ccc;
        }

        .tech-note {
            margin-top: 20px;
            padding-top: 15px;
            border-top: 1px solid rgba(255, 255, 255, 0.2);
            font-size: 12px;
            color: #888;
        }
    </style>
</head>
<body>
    <div class="overlay-box">
        <h1>mpv with Qt WebEngine Overlay<br>Qt6<br>HDR Wayland</h1>
        <p>This box is rendered by Qt WebEngine</p>
        <p>The video underneath is rendered by mpv (libmpv)</p>
        <p class="tech-note">
            Technique: Wayland subsurface for mpv (HDR10)<br>
            WebEngine on transparent parent surface
        </p>
    </div>
</body>
</html>
    )");

    QVBoxLayout *layout = new QVBoxLayout(mainWidget);
    layout->addWidget(webView);
    layout->setContentsMargins(0, 0, 0, 0);

    mainWidget->show();

    // Need to process events to get window created
    app.processEvents();

    // Get the native window
    QWindow *window = mainWidget->windowHandle();
    if (!window) {
        fprintf(stderr, "No QWindow\n");
        return 1;
    }

    // Get Qt's wl_surface
    struct wl_surface *qt_surface = (struct wl_surface *)native->nativeResourceForWindow("surface", window);
    if (!qt_surface) {
        fprintf(stderr, "Failed to get Qt's wl_surface\n");
        return 1;
    }
    fprintf(stderr, "*** Got Qt wl_surface: %p ***\n", (void*)qt_surface);

    // Create mpv's surface and make it a subsurface of Qt's
    mpv_surface = wl_compositor_create_surface(wl_compositor);
    mpv_subsurface = wl_subcompositor_get_subsurface(wl_subcompositor, mpv_surface, qt_surface);

    // Position at 0,0 relative to parent
    wl_subsurface_set_position(mpv_subsurface, 0, 0);

    // Place BELOW the parent surface (video behind, WebEngine on top)
    wl_subsurface_place_below(mpv_subsurface, qt_surface);

    // Desync mode - subsurface updates independently
    wl_subsurface_set_desync(mpv_subsurface);

    wl_surface_commit(mpv_surface);
    wl_display_roundtrip(wl_display);

    fprintf(stderr, "*** Created mpv subsurface below Qt ***\n");

    // Get window size
    sw_width = mainWidget->width();
    sw_height = mainWidget->height();
    fprintf(stderr, "*** Window size: %dx%d ***\n", sw_width, sw_height);

    // Create Vulkan context and swapchain for mpv surface
    create_vulkan_for_mpv();
    create_swapchain();
    create_mpv_render();

    // Load video
    const char *cmd[] = {"loadfile", argv[1], nullptr};
    mpv_command(mpv, cmd);

    // Handle window resize - always get both dimensions from window
    QObject::connect(window, &QWindow::widthChanged, [window](int) {
        pending_width = window->width();
        pending_height = window->height();
        needs_resize = true;
    });
    QObject::connect(window, &QWindow::heightChanged, [window](int) {
        pending_width = window->width();
        pending_height = window->height();
        needs_resize = true;
    });

    // Start render thread
    std::thread render_thread(render_loop);

    // Handle window close
    QObject::connect(&app, &QGuiApplication::aboutToQuit, [&]() {
        running = false;
    });

    int ret = app.exec();

    running = false;
    render_thread.join();

    delete mainWidget;
    return ret;
}
