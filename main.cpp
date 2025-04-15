#include <algorithm>
#include <cstdint>
#include <cstring>
#include <limits>
#include <optional>
#include <ostream>
#include <set>
#include <vector>
#include <vulkan/vk_platform.h>
#include <vulkan/vulkan_core.h>
#define GLFW_INCLUDE_VULKAN
#include <GLFW/glfw3.h>

#define GLM_FORCE_RADIANS
#define GLM_FORCE_DEPTH_ZERO_TO_ONE
#include <glm/vec4.hpp>
#include <glm/mat4x4.hpp>

#include <iostream>

#include <vulkan/vulkan.h>

#include <iostream>
#include <stdexcept>
#include <cstdlib>

struct QueueFamilyIndices {
    // queue with graphics capabilities
    std::optional<uint32_t> graphicsFamily;

    // queue with ability to present to window surface
    std::optional<uint32_t> presentFamily;

    bool isComplete() {
        return graphicsFamily.has_value() &&
               presentFamily.has_value();
    }
};

struct SwapChainSupportDetails {
    VkSurfaceCapabilitiesKHR capabilities;
    std::vector<VkSurfaceFormatKHR> formats;
    std::vector<VkPresentModeKHR> presentModes;
};

VkResult CreateDebugUtilsMessengerEXT(
    VkInstance instance,
    const VkDebugUtilsMessengerCreateInfoEXT* pCreateInfo,
    const VkAllocationCallbacks *pAllocator,
    VkDebugUtilsMessengerEXT *pDebugMessenger
) {
    auto func = (PFN_vkCreateDebugUtilsMessengerEXT)
        vkGetInstanceProcAddr(
            instance,
            "vkCreateDebugUtilsMessengerEXT"
        );
    
        if(func != nullptr) {
            return func(
                instance,
                pCreateInfo,
                pAllocator,
                pDebugMessenger
            );
        } else {
            return VK_ERROR_EXTENSION_NOT_PRESENT;
        }
}

void DestroyDebugUtilsMessengerEXT(
    VkInstance instance,
    VkDebugUtilsMessengerEXT debugMessenger,
    const VkAllocationCallbacks *pAllocator
) {
    auto func = (PFN_vkDestroyDebugUtilsMessengerEXT)
        vkGetInstanceProcAddr(
            instance,
            "vkDestroyDebugUtilsMessengerEXT"
        );
    if(func != nullptr) {
        func(
            instance,
            debugMessenger,
            pAllocator
        );
    }
}

class HelloTriangleApplication {
public:
    void run() {
        initWindow();
        initVulkan();
        mainLoop();
        cleanup();
    }

private:
    const uint32_t WIDTH = 800;
    const uint32_t HEIGHT = 600;
    /*
        Validation layers insert necessary checks for when things go wrong, and
        are intended for disabling in a release build.

        Without the application of the validation layers, errors including null
        pointer dereferences will not occur, and may silently fail (?) or crash
        (?)
    */
    const std::vector<const char*> validationLayers = {
        "VK_LAYER_KHRONOS_validation"
    };
    #ifdef NDEBUG
        const bool enableValidationLayers = false;
    #else
        const bool enableValidationLayers = true;
    #endif
    const std::vector<const char*> deviceExtensions = {
        VK_KHR_SWAPCHAIN_EXTENSION_NAME
    };

    GLFWwindow *window = nullptr;
    VkInstance instance;
    VkPhysicalDevice physicalDevice = VK_NULL_HANDLE;
    VkDevice device;
    VkQueue graphicsQueue;
    VkDebugUtilsMessengerEXT debugMessenger;

    // Window surface
    // note: creation relies on windowing system, not platform agnostic
    // using glfw to abstract over this
    VkSurfaceKHR surface;
    // queue for presentation
    VkQueue presentQueue;
    VkSwapchainKHR swapChain;
    std::vector<VkImage> swapChainImages;
    std::vector<VkImageView> swapChainImageViews;
    VkFormat swapChainImageFormat;
    VkExtent2D swapChainExtent;


    void initWindow() {
        assert(glfwInit() == GLFW_TRUE);

        /*
            We need to instruct GLFW to use no client API, or it will assume we
            intend to use OpenGL.
        */
        glfwWindowHint(GLFW_CLIENT_API, GLFW_NO_API);
        /*
            We want to disable resizing for now as there will be logic required
            on a resize.
        */
        glfwWindowHint(GLFW_RESIZABLE, GLFW_FALSE);

        this->window = glfwCreateWindow(
            WIDTH, 
            HEIGHT, 
            "Vulkan", 
            nullptr, // no monitor preference
            nullptr // only relevant for OpenGL, null under Vulkan
        );
        assert(this->window != nullptr);
    }

    static VKAPI_ATTR VkBool32 VKAPI_CALL debugCallback(
        VkDebugUtilsMessageSeverityFlagBitsEXT messageSeverity,
        VkDebugUtilsMessageTypeFlagsEXT messageType,
        const VkDebugUtilsMessengerCallbackDataEXT *pCallbackData,
        void* pUserData
    ) {
        std::cerr << "validation layer: " << pCallbackData->pMessage << std::endl;

        return VK_FALSE;
    }

    void createInstance() {
        // MARK: Enumerate required glfw extensions
        auto glfwExtensions = getRequiredExtensions();

        VkApplicationInfo appInfo{
            .sType = VK_STRUCTURE_TYPE_APPLICATION_INFO,
            .pApplicationName = "Hello Triangle",
            .applicationVersion = VK_MAKE_VERSION(1, 0, 0),
            .pEngineName = "No Engine",
            .engineVersion = VK_MAKE_VERSION(1, 0, 0),
            .apiVersion = VK_API_VERSION_1_0
        };

        VkInstanceCreateInfo createInfo {
            .sType = VK_STRUCTURE_TYPE_INSTANCE_CREATE_INFO,
            .pApplicationInfo = &appInfo,
            .enabledLayerCount = 0, // this will be modified later iirc
            .enabledExtensionCount = static_cast<uint32_t>(glfwExtensions.size()),
            .ppEnabledExtensionNames = glfwExtensions.data(),
        };

        VkDebugUtilsMessengerCreateInfoEXT debugCreateInfo{};
        if (enableValidationLayers) {
            createInfo.enabledLayerCount = static_cast<uint32_t>(validationLayers.size());
            createInfo.ppEnabledLayerNames = validationLayers.data();
    
            populateDebugMessengerCreateInfo(debugCreateInfo);
            createInfo.pNext = (VkDebugUtilsMessengerCreateInfoEXT*) &debugCreateInfo;
        } else {
            createInfo.enabledLayerCount = 0;
    
            createInfo.pNext = nullptr;
        }
    
        if (vkCreateInstance(&createInfo, nullptr, &instance) != VK_SUCCESS) {
            throw std::runtime_error("failed to create instance!");
        }

        // MARK: Query optional Vulkan extensions (optional)
        // May not be needed or desired for certain low-spec-requirement
        // situations, such as window managers.
        /*
            This is probably an example of what I remember reading about
            Vulkan's API design having a "call twice" approach, where the first
            pass is used to query for allocation counts, and the second pass is 
            used to actually copy the data into the allocated structure.
        */
        uint32_t extensionCount = 0;
        vkEnumerateInstanceExtensionProperties(
            nullptr, 
            &extensionCount,
            nullptr
        );
        std::vector<VkExtensionProperties> extensions(extensionCount);
        vkEnumerateInstanceExtensionProperties(
            nullptr, 
            &extensionCount,
            extensions.data()
        );

        std::cout << "available extensions:\n";
        for(const auto& extension: extensions) {
            std::cout << '\t' << extension.extensionName << '\n';
        }

        if(enableValidationLayers && !checkValidationLayerSupport()) {
            throw std::runtime_error("validation layers requested but unavailable");
        }
    }
    
    bool checkValidationLayerSupport() {
        std::cout << "Checking for validation layers\n";

        uint32_t layerCount;
        vkEnumerateInstanceLayerProperties(
            &layerCount,
            nullptr
        );

        std::vector<VkLayerProperties> availableLayers(layerCount);
        vkEnumerateInstanceLayerProperties(
            &layerCount,
            availableLayers.data()
        );

        for(const char* layerName: validationLayers)
        {
            bool layerFound = false;

            for(const auto& layerProperties: availableLayers) {
                if(strcmp(layerName, layerProperties.layerName) == 0) {
                    std::cout << "Validation layers found!\n";
                    layerFound = true;
                    break;
                }
            }

            return layerFound;
        }

        return false;
    }

    std::vector<const char*> getRequiredExtensions() {
        uint32_t glfwExtensionCount = 0;
        const char** glfwExtensions
            = glfwGetRequiredInstanceExtensions(&glfwExtensionCount);
        
        std::vector<const char*> extensions(
            glfwExtensions, 
            glfwExtensions + glfwExtensionCount
        );
        if(enableValidationLayers) {
            extensions.push_back(VK_EXT_DEBUG_UTILS_EXTENSION_NAME);
        }

        return extensions;
    }

    QueueFamilyIndices findQueueFamilies(VkPhysicalDevice device) {
        // different types of queues exist for different subsets of commands
        QueueFamilyIndices indices { .graphicsFamily = 0 };

        uint32_t queueFamilyCount = 0;
        vkGetPhysicalDeviceQueueFamilyProperties(
            device,
            &queueFamilyCount,
            nullptr
        );

        std::vector<VkQueueFamilyProperties> queueFamilies(queueFamilyCount);
        vkGetPhysicalDeviceQueueFamilyProperties(
            device,
            &queueFamilyCount,
            queueFamilies.data()
        );

        // iterate through and find the GRAPHICS_BIT queue
        int i = 0;
        for(const auto& queueFamily: queueFamilies) {
            if(queueFamily.queueFlags & VK_QUEUE_GRAPHICS_BIT) {
                indices.graphicsFamily = i;
            }

            VkBool32 presentSupport = false;
            vkGetPhysicalDeviceSurfaceSupportKHR(
                device, 
                i, 
                surface, 
                &presentSupport
            );

            if(presentSupport) {
                indices.presentFamily = i;
            }

            if(indices.isComplete()) {
                break;
            }

            i++;
        }

        return indices;
    }

    bool checkDeviceExtensionSupport(VkPhysicalDevice physicalDevice) {
        uint32_t extensionCount;
        vkEnumerateDeviceExtensionProperties(
            physicalDevice, 
            nullptr, 
            &extensionCount, 
            nullptr
        );

        std::vector<VkExtensionProperties> availableExtensions(extensionCount);
        vkEnumerateDeviceExtensionProperties(
            physicalDevice, 
            nullptr, 
            &extensionCount, 
            availableExtensions.data()
        );

        // enumerate through and check against required extensions
        std::set<std::string> requiredExtensions(
            deviceExtensions.begin(),
            deviceExtensions.end()
        );
        for(const auto& extension: availableExtensions) {
            requiredExtensions.erase(extension.extensionName);
        }

        return requiredExtensions.empty();
    }

    bool isDeviceSuitable(VkPhysicalDevice physicalDevice) {
        QueueFamilyIndices indices = findQueueFamilies(physicalDevice);

        bool extensionsSupported = checkDeviceExtensionSupport(physicalDevice);

        bool swapChainAdequate = false;
        if(extensionsSupported) {
            SwapChainSupportDetails swapChainSupport = querySwapChainSupport(
                physicalDevice
            );
            swapChainAdequate = !swapChainSupport.formats.empty() &&
                                !swapChainSupport.presentModes.empty();
        }

        return indices.isComplete() && extensionsSupported && swapChainAdequate;
    }

    void createLogicalDevice() {
        // logical device derives from physical device
        // multiple logical devices can be created to a single physical device
        // this is *likely* the same queue and code could be written to assert
        // these are the same queue for performance reasons.
        // TODO: In real engine refactor this to do the above.
        QueueFamilyIndices indices = findQueueFamilies(physicalDevice);

        std::vector<VkDeviceQueueCreateInfo> queueCreateInfos;
        std::set<uint32_t> uniqueQueueFamilies = {
            indices.graphicsFamily.value(), 
            indices.presentFamily.value()
        };

        float queuePriority = 1.0f;
        for (uint32_t queueFamily : uniqueQueueFamilies) {
            VkDeviceQueueCreateInfo queueCreateInfo{};
            queueCreateInfo.sType = VK_STRUCTURE_TYPE_DEVICE_QUEUE_CREATE_INFO;
            queueCreateInfo.queueFamilyIndex = queueFamily;
            queueCreateInfo.queueCount = 1;
            queueCreateInfo.pQueuePriorities = &queuePriority;
            queueCreateInfos.push_back(queueCreateInfo);
        }

        VkPhysicalDeviceFeatures deviceFeatures{};

        VkDeviceCreateInfo createInfo{};
        createInfo.sType = VK_STRUCTURE_TYPE_DEVICE_CREATE_INFO;

        createInfo.queueCreateInfoCount = static_cast<uint32_t>(queueCreateInfos.size());
        createInfo.pQueueCreateInfos = queueCreateInfos.data();

        createInfo.pEnabledFeatures = &deviceFeatures;

        // createInfo.enabledExtensionCount = 0;
        // we can populate extensions from required extensions
        // precondition: assuming we have the extensions from suitability check
        createInfo.enabledExtensionCount = static_cast<uint32_t>(deviceExtensions.size());
        createInfo.ppEnabledExtensionNames = deviceExtensions.data();

        if (enableValidationLayers) {
            createInfo.enabledLayerCount = static_cast<uint32_t>(validationLayers.size());
            createInfo.ppEnabledLayerNames = validationLayers.data();
        } else {
            createInfo.enabledLayerCount = 0;
        }

        if (vkCreateDevice(physicalDevice, &createInfo, nullptr, &device) != VK_SUCCESS) {
            throw std::runtime_error("failed to create logical device!");
        }

        vkGetDeviceQueue(device, indices.graphicsFamily.value(), 0, &graphicsQueue);
        vkGetDeviceQueue(device, indices.presentFamily.value(), 0, &presentQueue);
    }

    void pickPhysicalDevice() {
        uint32_t deviceCount = 0;
        vkEnumeratePhysicalDevices(
            instance, 
            &deviceCount,
            nullptr
        );

        if(deviceCount == 0) {
            throw std::runtime_error("no gpus with vulkan available");
        }

        std::vector<VkPhysicalDevice> devices(deviceCount);
        vkEnumeratePhysicalDevices(
            instance,
            &deviceCount,
            devices.data() 
        );

        for(const auto& device : devices) {
            if(isDeviceSuitable(device)) {
                physicalDevice = device;
                break;
            }
        }
        if(physicalDevice == VK_NULL_HANDLE) {
            throw std::runtime_error("no suitable gpus");
        }
    }

    void createSurface() {
        VkResult result = glfwCreateWindowSurface(
            instance,
            window, 
            nullptr, 
            &surface
        );
        if(result != VK_SUCCESS) {
            throw std::runtime_error("failed to create window surface!");
        }
    }

    SwapChainSupportDetails querySwapChainSupport(VkPhysicalDevice physicalDevice) {
        SwapChainSupportDetails details;
    
        // Query basic capabilities
        vkGetPhysicalDeviceSurfaceCapabilitiesKHR(
            physicalDevice,
            surface,
            &details.capabilities
        );

        // Query supported surface formats
        uint32_t formatCount;
        vkGetPhysicalDeviceSurfaceFormatsKHR(
            physicalDevice,
            surface,
            &formatCount,
            nullptr
        );

        if(formatCount != 0) {
            details.formats.resize(formatCount);
            vkGetPhysicalDeviceSurfaceFormatsKHR(
                physicalDevice,
                surface,
                &formatCount,
                details.formats.data()
            );
        }

        // Query supported presentation modes
        uint32_t presentModeCount;
        vkGetPhysicalDeviceSurfacePresentModesKHR(
            physicalDevice, 
            surface, 
            &presentModeCount, 
            nullptr
        );
        if(presentModeCount != 0) {
            details.presentModes.resize(presentModeCount);
            vkGetPhysicalDeviceSurfacePresentModesKHR(
                physicalDevice, 
                surface, 
                &presentModeCount, 
                details.presentModes.data()
            );
        }
    
        return details;
    }

    VkSurfaceFormatKHR chooseSwapSurfaceFormat(
        const std::vector<VkSurfaceFormatKHR>& availableFormats
    ) {
        for(const auto& availableFormat: availableFormats) {
            if(
                availableFormat.format == VK_FORMAT_B8G8R8_SRGB &&
                availableFormat.colorSpace == VK_COLOR_SPACE_SRGB_NONLINEAR_KHR
            ) {
                return availableFormat;
            }
        }

        // ? what are the implications of just going with the first format
        // ! I smell runtime bugs!
        return availableFormats[0];
    }

    VkPresentModeKHR chooseSwapPresentMode(
        const std::vector<VkPresentModeKHR>& availablePresentModes
    ) {
        // FIFO is guaranteed to be made available
        // return VK_PRESENT_MODE_FIFO_KHR;

        // We may also iterate and rank presentation modes.
        for(const auto& availablePresentMode: availablePresentModes) {
            if(availablePresentMode == VK_PRESENT_MODE_MAILBOX_KHR) {
                // Mailbox mode replaces upcoming frames instead of blocking
                // when queue is fuill, rendering as fast as possible but still
                // avoiding tearing. known as "triple buffering"

                return availablePresentMode;
            }
        }

        return VK_PRESENT_MODE_FIFO_KHR;
    }

    VkExtent2D chooseSwapExtent(
        const VkSurfaceCapabilitiesKHR& capabilities
    ) {
        // swap extent is resolution of swap chain images, usually window res
        // range of resolutions provided by capabilities
        // match res of window by setting width/height in currentExtent member
        if(capabilities.currentExtent.width != std::numeric_limits<uint32_t>::max()) {
            std::cout << "extents locked" << std::endl;
            return capabilities.currentExtent;
        } else {
            std::cout << "extents unlocked" << std::endl;

            int width, height;
            glfwGetFramebufferSize(
                window, 
                &width, 
                &height
            );

            VkExtent2D actualExtent = {
                static_cast<uint32_t>(width),
                static_cast<uint32_t>(height)
            };

            actualExtent.width = std::clamp(actualExtent.width, capabilities.minImageExtent.width, capabilities.maxImageExtent.width);
            actualExtent.height = std::clamp(actualExtent.height, capabilities.minImageExtent.height, capabilities.maxImageExtent.height);

            return actualExtent;
        }
    }

    void createSwapChain() {
        SwapChainSupportDetails swapChainSupport = querySwapChainSupport(physicalDevice);

        VkSurfaceFormatKHR surfaceFormat = chooseSwapSurfaceFormat(swapChainSupport.formats);
        VkPresentModeKHR presentMode = chooseSwapPresentMode(swapChainSupport.presentModes);
        VkExtent2D extent = chooseSwapExtent(swapChainSupport.capabilities);

        // Determine swap chain image count
        uint32_t imageCount = swapChainSupport.capabilities.minImageCount;
        if(swapChainSupport.capabilities.maxImageCount > 0 && imageCount > swapChainSupport.capabilities.maxImageCount) {
            imageCount = swapChainSupport.capabilities.maxImageCount;
        }

        VkSwapchainCreateInfoKHR createInfo{
            .sType = VK_STRUCTURE_TYPE_SWAPCHAIN_CREATE_INFO_KHR,
            .surface = surface,

            .minImageCount = imageCount,
            .imageFormat = surfaceFormat.format,
            .imageColorSpace = surfaceFormat.colorSpace,
            .imageExtent = extent,
            // image array layers specifies number of layers of image, always 1
            // unless creating a stereoscopic 3d application
            .imageArrayLayers = 1,
            // this usage is for rendering directly
            .imageUsage = VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT
            // note: we could use vk_image_usage_transfer_dst_bit to do rendering
            // to a separate image, to do post processing.
        };

        // check if graphics family is the same as the present family
        // if they are equivalent, we can use exclusive sharing mode, which
        // is better performance. otherwise, we need to do concurrent sharing.
        QueueFamilyIndices indices = findQueueFamilies(physicalDevice);
        uint32_t queueFamilyIndices[] = {
            indices.graphicsFamily.value(),
            indices.presentFamily.value()
        };
        if(indices.graphicsFamily != indices.presentFamily) {
            /*
                Concurrent mode avoids having to do explicit ownership transfers
                we may end up doing this later, but for now the tutorial avoids
                it.

                Concurrent mode requires specifying the shared ownership,
                done below. 

                All of this should be avoided with the exclusive mode if the
                queues are shared.
            */
            createInfo.imageSharingMode = VK_SHARING_MODE_CONCURRENT;
            createInfo.queueFamilyIndexCount = 2;
            createInfo.pQueueFamilyIndices = queueFamilyIndices;
        } else {
            createInfo.imageSharingMode = VK_SHARING_MODE_EXCLUSIVE;
            createInfo.queueFamilyIndexCount = 0;
            createInfo.pQueueFamilyIndices = nullptr;
        }

        // map pre transform through to avoid doing transformations
        createInfo.preTransform = swapChainSupport.capabilities.currentTransform;

        // alpha channel used for blending *window* - usually ignore
        createInfo.compositeAlpha = VK_COMPOSITE_ALPHA_OPAQUE_BIT_KHR;

        createInfo.presentMode = presentMode;
        // ignore pixels obscured by other windows
        // we would want to disable this if we needed to read back the values
        createInfo.clipped = VK_TRUE; 

        // ! Assumes only one swapchain - will update this later
        createInfo.oldSwapchain = VK_NULL_HANDLE;

        VkResult result = vkCreateSwapchainKHR(
            device, 
            &createInfo, 
            nullptr, 
            &swapChain
        );
        if(result != VK_SUCCESS) {
            throw std::runtime_error("failed to create swap chain!");
        }

        // MARK: Swapchain images
        // Created for swap chain, retrieve array of however many in chain
        vkGetSwapchainImagesKHR(
            device,
            swapChain,
            &imageCount,
            nullptr
        );
        swapChainImages.resize(imageCount);
        vkGetSwapchainImagesKHR(
            device,
            swapChain,
            &imageCount,
            swapChainImages.data()
        );

        // store swap chain format and extent for later use
        swapChainImageFormat = surfaceFormat.format;
        swapChainExtent = extent;
    }

    void createImageViews() {
        swapChainImageViews.resize(swapChainImages.size());
        for(size_t i = 0; i < swapChainImages.size(); i++) {
            VkImageViewCreateInfo createInfo {
                .sType = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO,
                .image = swapChainImages[i],
                // use 2D texture and swapchain format
                .viewType = VK_IMAGE_VIEW_TYPE_2D,
                .format = swapChainImageFormat,
                // default "swizzle" (swapping color channels) config
                .components = VkComponentMapping {
                    .r = VK_COMPONENT_SWIZZLE_IDENTITY,
                    .g = VK_COMPONENT_SWIZZLE_IDENTITY,
                    .b = VK_COMPONENT_SWIZZLE_IDENTITY,
                    .a = VK_COMPONENT_SWIZZLE_IDENTITY
                },
                // subresource configuration describes part of image and purpose
                // use these images as color target without mipmap or layering
                .subresourceRange = VkImageSubresourceRange {
                    .aspectMask = VK_IMAGE_ASPECT_COLOR_BIT,
                    .baseMipLevel = 0,
                    .levelCount = 1,
                    .baseArrayLayer = 0,
                    // stereo 3D would use swapchain with multiple layers
                    // with multiple image views for left and right eyes for 
                    // different layers
                    .layerCount = 1
                }
            };

            VkResult result = vkCreateImageView(
                device, 
                &createInfo, 
                nullptr, 
                &swapChainImageViews[i]
            );
            if(result != VK_SUCCESS) {
                throw std::runtime_error("failed to create image view");
            }
        };
    }

    void initVulkan() {
        createInstance();
        setupDebugMessenger();
        createSurface();
        pickPhysicalDevice();
        createLogicalDevice();
        createSwapChain();
        createImageViews();
    }

    void populateDebugMessengerCreateInfo(VkDebugUtilsMessengerCreateInfoEXT& createInfo) {
        createInfo = {};
        createInfo.sType = VK_STRUCTURE_TYPE_DEBUG_UTILS_MESSENGER_CREATE_INFO_EXT;
        createInfo.messageSeverity = VK_DEBUG_UTILS_MESSAGE_SEVERITY_VERBOSE_BIT_EXT | VK_DEBUG_UTILS_MESSAGE_SEVERITY_WARNING_BIT_EXT | VK_DEBUG_UTILS_MESSAGE_SEVERITY_ERROR_BIT_EXT;
        createInfo.messageType = VK_DEBUG_UTILS_MESSAGE_TYPE_GENERAL_BIT_EXT | VK_DEBUG_UTILS_MESSAGE_TYPE_VALIDATION_BIT_EXT | VK_DEBUG_UTILS_MESSAGE_TYPE_PERFORMANCE_BIT_EXT;
        createInfo.pfnUserCallback = debugCallback;
    }    

    void setupDebugMessenger() {
        if(!enableValidationLayers) return;

        VkDebugUtilsMessengerCreateInfoEXT createInfo{};
        populateDebugMessengerCreateInfo(createInfo);

        if(
            CreateDebugUtilsMessengerEXT(
                instance, 
                &createInfo, 
                nullptr, 
                &debugMessenger
            ) != VK_SUCCESS
        ) {
            throw std::runtime_error("failed to set up debug messenger!");
        }
    }

    void mainLoop() {
        while(!glfwWindowShouldClose(window)) {
            // process events (including window close)
            glfwPollEvents();
        }
    }

    void cleanup() {
        // MARK: Vulkan deinstantiation
        // We are responsible for removing image views
        for(VkImageView imageView: swapChainImageViews) {
            vkDestroyImageView(
                device, 
                imageView, 
                nullptr
            );
        }
        vkDestroySwapchainKHR(
            device,
            swapChain,
            nullptr
        );
        
        // queues implicitly cleaned up
        vkDestroyDevice(device, nullptr);

        if(enableValidationLayers) {
            DestroyDebugUtilsMessengerEXT(
                instance, 
                debugMessenger, 
                nullptr
            );
        }

        // must destroy surface before instance
        vkDestroySurfaceKHR(
            instance, 
            surface, 
            nullptr
        );
        
        vkDestroyInstance(
            instance,
            nullptr
        );
        // MARK: glfw deinstantiation
        glfwDestroyWindow(window);
        glfwTerminate();
    }
};

int main() {
    HelloTriangleApplication app;

    try {
        app.run();
    } catch (const std::exception& e) {
        std::cerr << e.what() << std::endl;
        return EXIT_FAILURE;
    }

    return EXIT_SUCCESS;
}
