#include <algorithm>
#include <cstddef>
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

#include<fstream>

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

static std::vector<char> readFile(const std::string& filename) {
    std::ifstream file(filename, std::ios::ate | std::ios::binary);

    if(!file.is_open()) {
        throw std::runtime_error("failed to open file");
    }

    size_t fileSize = (size_t)file.tellg();
    std::vector<char> buffer(fileSize);

    file.seekg(0);
    file.read(buffer.data(), fileSize);
    file.close();

    return buffer;
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

    // TODO: Implement uniforms
    VkRenderPass renderPass;
    VkPipelineLayout pipelineLayout;

    VkPipeline graphicsPipeline;

    std::vector<VkFramebuffer> swapChainFrameBuffers;
    VkCommandPool commandPool;
    VkCommandBuffer commandBuffer;

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

    VkShaderModule createShaderModule(const std::vector<char>& code) {
        VkShaderModuleCreateInfo createInfo {
            .sType = VK_STRUCTURE_TYPE_SHADER_MODULE_CREATE_INFO,
            .codeSize = code.size(),
            .pCode = reinterpret_cast<const uint32_t*>(code.data())
        };

        VkShaderModule shaderModule;
        VkResult createShaderResult = vkCreateShaderModule(
            device, 
            &createInfo, 
            nullptr, 
            &shaderModule
        );
        if(createShaderResult != VK_SUCCESS) {
            throw std::runtime_error("failed to create shader module!");
        }

        return shaderModule;
    }

    void createGraphicsPipeline() {
        auto vertShaderCode = readFile("shaders/vert.spv");
        auto fragShaderCode = readFile("shaders/frag.spv");

        // Create shader modules, which are thin wrappers around bytecode
        // Compilation/linking of SPIR-V doesn't occur until pipeline creation 
        // finished. We can destroy these once the pipeline is created.
        VkShaderModule vertShaderModule = createShaderModule(vertShaderCode);
        VkShaderModule fragShaderModule = createShaderModule(fragShaderCode);

        // Create pipeline stage in order to use shaders
        VkPipelineShaderStageCreateInfo vertShaderStageInfo {
            .sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO,
            .stage = VK_SHADER_STAGE_VERTEX_BIT,
            .module = vertShaderModule, // specify the code
            .pName = "main" // specify the entry point
            /*
                pSpecializationInfo not used but can be used to specify shader
                constants, which enables single shader module with behavior 
                configured at pipeline creation, more efficient than
                render-time variables since compiler can optimize out if
                statements which use values.

                if not needed, set to null (done automatically in C++)
           */
        };

        // Equivalent for fragment shader
        VkPipelineShaderStageCreateInfo fragShaderStageInfo {
            .sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO,
            .stage = VK_SHADER_STAGE_FRAGMENT_BIT,
            .module = fragShaderModule,
            .pName = "main"
        };

        VkPipelineShaderStageCreateInfo shaderStages[] = {
            vertShaderStageInfo,
            fragShaderStageInfo
        };

        // MARK: Dynamic State
        /*
            Most of the Vulkan pipeline is fixed on creation. Limited properties
            (VkDynamicState) may be mutable after creation.

            Examples:
            - Viewport size
            - Line width
            - Blend constants
        */
        std::vector<VkDynamicState> dynamicStates = {
            VK_DYNAMIC_STATE_VIEWPORT,
            VK_DYNAMIC_STATE_SCISSOR
        };

        VkPipelineDynamicStateCreateInfo dynamicState {
            .sType = VK_STRUCTURE_TYPE_PIPELINE_DYNAMIC_STATE_CREATE_INFO,
            .dynamicStateCount = static_cast<uint32_t>(dynamicStates.size()),
            .pDynamicStates = dynamicStates.data()
        };

        /*
            Create a vertex input state structure. Specifies the input format
            for incoming data into the vertex shaders.

            Configures:
            - Bindings -> Spacing between data and per-vertex vs per-instance
            data flag
            - Attribute descriptions: What kinds of data being passed, what
            binding to load from, and offset information
        */
        // TODO: Specify that we are not passing in any vertex data.
        // We will later specify this data when we are no longer hardcoding in
        // the shader.
        VkPipelineVertexInputStateCreateInfo vertexInputInfo {
            .sType = VK_STRUCTURE_TYPE_PIPELINE_VERTEX_INPUT_STATE_CREATE_INFO,
            .vertexBindingDescriptionCount = 0,
            // Structure with descriptions of bindings
            .pVertexBindingDescriptions = nullptr,
            .vertexAttributeDescriptionCount = 0,
            // Structure with descriptions of attributes
            .pVertexAttributeDescriptions = nullptr
        };

        // MARK: Input assembly
        /*
            Describes what geometry is being drawn given the vertices, and if
            primitive restat should be enabled.

            Topology describes the vertices:
            - Point List -> Points from vertices -> VK_PRIMITIVE_TOPOLOGY_POINT_LIST
            - Line List -> Line from every two vertices w/o reuse
            - Line Strip -> End of every vertex line used as start for next line
            - Triangle List -> Triangle from every three vertices, no reuse
            - Triangle Strip -> Second and third vertices for each triangle used
            as first two vertices of next triangle.

            Vertices normally loaded in sequential order, but element buffer can
            be defined to specify indices yourself, enabling reusing vertices
            for performance.

            Primitive restart enables breaking up strip topologies with
            0xFFFF or 0xFFFFFFFF addresses.

            This tutorial is a simple triangle list.
        */

        VkPipelineInputAssemblyStateCreateInfo inputAssembly {
            .sType = VK_STRUCTURE_TYPE_PIPELINE_INPUT_ASSEMBLY_STATE_CREATE_INFO,
            .topology = VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST,
            .primitiveRestartEnable = VK_FALSE
        };

        /*
            Viewport defines framebuffer output will be rendered to

            Usually (0, 0) to (width, height)

            Size of swapchain and images may differ from window height.
            Swap chain images are used for render so we want to use that.
            ? (What is the distinction in practice, if you do not sync?)

            minDepth and maxDepth must be specified. Standard is 0.0 to 1.0.

            Scissor rectangles act as filters for processing. If we create
            a scissor rectangle across the entire viewport, the entire viewport
            will be processed. If we create a smaller scissor rectangle, we 
            filter the processed pixels.
        */

        VkViewport viewport {
            .x = 0.0f,
            .y = 0.0f,
            .width = static_cast<float>(swapChainExtent.width),
            .height = static_cast<float>(swapChainExtent.height),
            .minDepth = 0.0f,
            .maxDepth = 1.0f
        };

        VkRect2D scissor {
            .offset = {
                .x = 0,
                .y = 0,
            },
            .extent = swapChainExtent
        };

        VkPipelineViewportStateCreateInfo viewportState {
            .sType = VK_STRUCTURE_TYPE_PIPELINE_VIEWPORT_STATE_CREATE_INFO,
            .viewportCount = 1,
            .pViewports = &viewport,
            .scissorCount = 1,
            .pScissors = &scissor
        };

        // MARK: Rasterizer
        /*
            The rasterizer converts vertex geometry into fragments. It performs
            depth testing, face culling, scissor testing, and may either output
            polygons or wireframes.
        */
        VkPipelineRasterizationStateCreateInfo rasterizerCreateInfo {
            .sType = VK_STRUCTURE_TYPE_PIPELINE_RASTERIZATION_STATE_CREATE_INFO,
            /*
                Fragments beyond near and far planes are clamped not discarded
                if true. Requires a GPU feature.
            */
            .depthClampEnable = false,
            
            /*
                Rasterizer discard enable disables framebuffer output by never
                passing any geometry through stage.
            */
            .rasterizerDiscardEnable = false,

            /*
                Polygon mode determines fragment generation.

                FILL -> Fill area with fragments
                LINE -> Polygon edges drawn as lines (wireframe)
                POINT -> Polygon edges drawn as points
            */
            .polygonMode = VK_POLYGON_MODE_FILL,

            /*
                Cull mode determines culling options, between no culling, front,
                back, or both faces.

                frontFace variable specifies vertex order for the above.
            */
            .cullMode = VK_CULL_MODE_BACK_BIT,
            .frontFace = VK_FRONT_FACE_CLOCKWISE,

            /*
                Depth value alterations which may be used for shadow mapping.
            */
            .depthBiasEnable = VK_FALSE,
            // below here for reference solely
            .depthBiasConstantFactor = 0.0f,
            .depthBiasClamp = 0.0f,
            .depthBiasSlopeFactor = 0.0f,

            /*
                Line width describes thickness of lines in terms of fragments.
                Requires wideLines GPU feature for non-1.0 widths.
            */
            .lineWidth = 1.0f
        };

        // MARK: Multisampling
        /*
            Method to perform antialiasing by combining fragment shaders across 
            multiple polygons rendering to same fragment, mainly along edges
            where AA artifacts tend to crop up. Cheap if only one polygon maps
            to a given pixel, versus rendering to higher resolution then
            downscaling. 

            Multisampling requires a GPU feature.
        */
        // TODO: Will revisit this in a later chapter, for now disabled.
        VkPipelineMultisampleStateCreateInfo multisampling {
            .sType = VK_STRUCTURE_TYPE_PIPELINE_MULTISAMPLE_STATE_CREATE_INFO,
            .rasterizationSamples = VK_SAMPLE_COUNT_1_BIT,
            .sampleShadingEnable = VK_FALSE,
            // Below are optional and solely for reference
            .minSampleShading = 1.0f,
            .pSampleMask = nullptr,
            .alphaToCoverageEnable = VK_FALSE,
            .alphaToOneEnable = VK_FALSE
        };

        // MARK: Depth testing
        // ! Depth testing requires depth and stencil test create info
        // TODO: Implement depth testing. (For now we pass nullptr, disabled)

        // MARK: Color blending
        /*
            Once a fragment shader has returned a color, we need to combine it
            with any existing color in the framebuffer. 

            We can either:
            - Mix new and old value to create some final color
            - Combine new and old via some bitwise operation

            We need two structs to configure color blending. 

            First struct AttachmentState is configuration per attached 
            framebuffer. 

            Second struct ColorBlendState contains global color blending 
            settings.

            We only have one framebuffer, so we TODO describe

            NOTE: We could implement alpha blending within color blending, a 
            common usage for merging colors based on the alpha value 
            (presumably for transparency?)
            https://docs.vulkan.org/tutorial/latest/03_Drawing_a_triangle/02_Graphics_pipeline_basics/02_Fixed_functions.html
        */
        VkPipelineColorBlendAttachmentState colorBlendAttachment {
            /*
                Equivalent psuedocode:

                if (blendEnable) {
                    finalColor.rgb = (srcColorBlendFactor * newColor.rgb) <colorBlendOp> (dstColorBlendFactor * oldColor.rgb);
                    finalColor.a = (srcAlphaBlendFactor * newColor.a) <alphaBlendOp> (dstAlphaBlendFactor * oldColor.a);
                } else {
                    finalColor = newColor;
                }

                finalColor = finalColor & colorWriteMask;
            */
            .blendEnable = VK_FALSE,
            // Below are solely for reference; we disable.
            .srcColorBlendFactor = VK_BLEND_FACTOR_ONE,
            .dstColorBlendFactor = VK_BLEND_FACTOR_ZERO,
            .colorBlendOp = VK_BLEND_OP_ADD,
            .srcAlphaBlendFactor = VK_BLEND_FACTOR_ONE,
            .dstAlphaBlendFactor = VK_BLEND_FACTOR_ZERO,
            .alphaBlendOp = VK_BLEND_OP_ADD,
            .colorWriteMask = 
                VK_COLOR_COMPONENT_R_BIT
                | VK_COLOR_COMPONENT_G_BIT
                | VK_COLOR_COMPONENT_B_BIT
                | VK_COLOR_COMPONENT_A_BIT,
        };

        VkPipelineColorBlendStateCreateInfo colorBlending {
            .sType = VK_STRUCTURE_TYPE_PIPELINE_COLOR_BLEND_STATE_CREATE_INFO,
            // NOTE: Set below to VK_TRUE to use bitwise combination.
            .logicOpEnable = VK_FALSE,
            // logicOp for reference, disabled above.
            .logicOp = VK_LOGIC_OP_COPY,
            .attachmentCount = 1,
            .pAttachments = &colorBlendAttachment,
            // Below optional for reference
            .blendConstants = {
                0.0f, 0.0f, 0.0f, 0.0f
            }
        };

        // MARK: Pipeline Layout
        /*
            Pipeline layout defines layout for uniform shaders. We need to 
            specify them, though we are not using them yet.
        */
        VkPipelineLayoutCreateInfo pipelineLayoutInfo {
            .sType = VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO,
            // below are optional
            .setLayoutCount = 0,
            .pSetLayouts = nullptr,
            .pushConstantRangeCount = 0,
            .pPushConstantRanges = nullptr
        };
        VkResult createPipelineLayoutResult = vkCreatePipelineLayout(
            device, 
            &pipelineLayoutInfo, 
            nullptr, 
            &pipelineLayout
        );
        if(createPipelineLayoutResult != VK_SUCCESS) {
            throw std::runtime_error("failed to create pipeline layout!");
        }

        // MARK: Graphics pipeline creation
        VkGraphicsPipelineCreateInfo pipelineInfo {
            .sType = VK_STRUCTURE_TYPE_GRAPHICS_PIPELINE_CREATE_INFO,
            .stageCount = 2,
            .pStages = shaderStages,

            // Input piopeline shader stage structs
            .pVertexInputState = &vertexInputInfo,
            .pInputAssemblyState = &inputAssembly,
            .pViewportState = &viewportState,
            .pRasterizationState = &rasterizerCreateInfo,
            .pMultisampleState = &multisampling,
            .pDepthStencilState = nullptr, // depth test disabled
            .pColorBlendState = &colorBlending,
            .pDynamicState = &dynamicState,

            // then fixed function
            .layout = pipelineLayout,

            // then pipeline layout (Vulkan handle rather than struct pointer)
            .renderPass = renderPass,
            .subpass = 0,

            /*
                We can derive pipelines if we share a lot of functionality in 
                common. Right now we only have one pipeline, so this is 
                not needed. These values only used if 
                VK_PIPELINE_CREATE_DERIVATIVE_IT is set in info.
                below for reference
            */
            .basePipelineHandle = VK_NULL_HANDLE,
            .basePipelineIndex = -1
        };

        VkResult createPipelineResult = vkCreateGraphicsPipelines(
            device,
            VK_NULL_HANDLE,
            1,
            &pipelineInfo,
            nullptr,
            &graphicsPipeline
        );
        if(createPipelineResult != VK_SUCCESS) {
            throw std::runtime_error("Could not create graphics pipeline!");
        }

        // clean up shader modules after creating pipeline
        vkDestroyShaderModule(
            device, 
            vertShaderModule, 
            nullptr
        );
        vkDestroyShaderModule(
            device, 
            fragShaderModule, 
            nullptr
        );
    }

    void initVulkan() {
        createInstance();
        setupDebugMessenger();
        createSurface();
        pickPhysicalDevice();
        createLogicalDevice();
        createSwapChain();
        createImageViews();
        createRenderPass();
        createGraphicsPipeline();
        createFramebuffers();
        createCommandPool();
        createCommandBuffer();
    }

    void recordCommandBuffer(
        VkCommandBuffer commandBuffer,
        // Swapchain image we wish to write to.
        uint32_t imageIndex
    ) {
        VkCommandBufferBeginInfo beginInfo {
            .sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO,
            // Below for reference
            /*
                Flags specifies how we use the command buffer.

                ONE_TIME_SUBMIT_BIT -> Command buffer will be rerecorded 
                immediately after executing it once.
                RENDER_PASS_CONTINUE_BIT -> Secondary command buffer that is
                entirely within a single render pass
                SIMULTANEOUS_USAGE_BIT -> Command buffer can be resubmitted if 
                it is pending execution

                None are applicable currently.
            */
            .flags = 0,
            /*
                Only relevant to secondary command buffers, specifying which
                state to inherit from primary command buffers.
            */
            .pInheritanceInfo = nullptr
        };

        VkResult beginCommandBufferResult = vkBeginCommandBuffer(
            commandBuffer,
            &beginInfo
        );
        if(beginCommandBufferResult != VK_SUCCESS) {
            throw std::runtime_error("failed to begin recording command buffer!");
        }

        // MARK: Starting render pass
        VkClearValue clearColor = {{{0.0f, 0.0f, 0.0f, 1.0f}}};

        VkRenderPassBeginInfo renderPassInfo {
            .sType = VK_STRUCTURE_TYPE_RENDER_PASS_BEGIN_INFO,
            // Get render pass
            .renderPass = renderPass,
            // Specify attachments to bind
            .framebuffer = swapChainFrameBuffers[imageIndex],

            /*
                Define render size. This defines where shader loads and stores
                take place. Anything outside this region is undefined.

                Should match size of attachments for best performance.
            */
            .renderArea = {
                .offset = { 0, 0 },
                .extent = swapChainExtent
            },

            /*
                Define clear color. In this case black.
            */
            .clearValueCount = 1,
            .pClearValues = &clearColor
        };

        vkCmdBeginRenderPass(
            commandBuffer,
            &renderPassInfo,
            /*
                Controls how the drawing commands are provided.

                INLINE -> Primary command buffer holds drawing commands and no
                secondary commands are to be executed.

                SECONDARY_COMMAND_BUFFERS -> Render pass commands to be executed
                by secondary command buffers.

                We use INLINE as we do not currently use secondary command
                buffers.
            */
            VK_SUBPASS_CONTENTS_INLINE
        );

        vkCmdBindPipeline(
            commandBuffer,
            VK_PIPELINE_BIND_POINT_GRAPHICS,
            graphicsPipeline
        );

        // ! Need to set viewport and scissor here since it's dynamic.
        VkViewport viewport {
            .x = 0.0f,
            .y = 0.0f,
            .width = static_cast<float>(swapChainExtent.width),
            .height = static_cast<float>(swapChainExtent.height),
            .minDepth = 0.0f,
            .maxDepth = 1.0f
        };
        vkCmdSetViewport(
            commandBuffer,
            0,
            1,
            &viewport
        );

        VkRect2D scissor {
            .offset = { 0, 0 },
            .extent = swapChainExtent
        };
        vkCmdSetScissor(
            commandBuffer,
            0,
            1,
            &scissor
        );

        // Issue draw call
        vkCmdDraw(
            commandBuffer,
            3, // 3 verts to draw even w/o vertex buffer
            1, // Set 1 for non-instanced rendering
            0,
            0
        );

        // End render pass
        vkCmdEndRenderPass(commandBuffer);

        // Finish recording command buffer
        VkResult recordResult = vkEndCommandBuffer(commandBuffer);
        if(recordResult != VK_SUCCESS) {
            throw std::runtime_error("failed to record command buffer!");
        }
    }

    // MARK: Command buffer creation
    /*
        Create a command buffer which resides on the pool.
    */
    void createCommandBuffer() {
        VkCommandBufferAllocateInfo allocInfo {
            .sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO,
            .commandPool = commandPool,
            /*
                Level parameter defines:
                - PRIMARY -> Can be submitted to a queue for execution, but
                cannot be called by other command buffers.
                - SECONDARY -> Cannot be submitted directly, but can be called 
                from primary command buffers

                Intended use is to reuse common operations from primary command
                buffers.
            */
            .level = VK_COMMAND_BUFFER_LEVEL_PRIMARY,
            .commandBufferCount = 1
        };

        VkResult createCommandBufferResult = vkAllocateCommandBuffers(
            device, 
            &allocInfo, 
            &commandBuffer
        );
        if(createCommandBufferResult != VK_SUCCESS) {
            throw std::runtime_error("failed to allocate command buffers!");
        }
    }

    // MARK: Command pool creation
    /*
        Drawing commands and memory transfers are not executed directly.
        We record all operations into a buffer
        All commands are submitted together.
        Command recording may occur across multiple threads if desired.
    */
    void createCommandPool() {
        QueueFamilyIndices queueFamilyIndices = findQueueFamilies(physicalDevice);

        VkCommandPoolCreateInfo poolInfo {
            .sType = VK_STRUCTURE_TYPE_COMMAND_POOL_CREATE_INFO,
            /*
                Two possible flags:
                TRANSIENT_BIT -> Hint that command buffers are rerecorded with
                new commands often.
                CREATE_RESET_COMMAND_BUFFER_BIT -> Buffers may be rerecorded
                individually (otherwise all must be reset together)

                We are recording buffer every frame so we want
                RESET_COMMAND_BUFFER_BIT. (Unsure why we don't use transient
                bit)
            */
            .flags = VK_COMMAND_POOL_CREATE_RESET_COMMAND_BUFFER_BIT,
            .queueFamilyIndex = queueFamilyIndices.graphicsFamily.value()
        };

        // NOTE: Must deallocate command pool at end of program.
        VkResult createCommandPoolResult = vkCreateCommandPool(
            device,
            &poolInfo,
            nullptr,
            &commandPool
        );
        if(createCommandPoolResult != VK_SUCCESS) {
            throw std::runtime_error("could not create command pool!");
        }
    }

    void createFramebuffers() {
        // MARK: Frame buffer creation
        swapChainFrameBuffers.resize(swapChainImageViews.size());

        for(size_t i = 0; i<swapChainImageViews.size(); i++) {
            VkImageView attachments = {
                swapChainImageViews[i]
            };

            VkFramebufferCreateInfo framebufferInfo {
                .sType = VK_STRUCTURE_TYPE_FRAMEBUFFER_CREATE_INFO,
                .renderPass = renderPass,
                .attachmentCount = 1,
                .pAttachments = &attachments,
                .width = swapChainExtent.width,
                .height = swapChainExtent.height,
                .layers = 1
            };

            VkResult createFramebufferResult = vkCreateFramebuffer(
                device,
                &framebufferInfo,
                nullptr,
                &swapChainFrameBuffers[i]
            );
            if(createFramebufferResult != VK_SUCCESS) {
                throw std::runtime_error("could not create framebuffer!");
            }
        }
    }

    /*
        Render pass instructs Vulkan about framebuffer attachments
        How many depth, color buffers there are, sample count for each,
        how their contents should be handled through render operations.
    */
    void createRenderPass() {
        std::cout << "Creating render pass" << std::endl;

        // MARK: Attachment Description
        VkAttachmentDescription colorAttachment {
            .format = swapChainImageFormat,
            // Use one sample - no multisampling being used
            .samples = VK_SAMPLE_COUNT_1_BIT,

            /*
                Set load and store operations.
                Load options
                Load -> Preserve existing attachment contents
                Clear -> Clear framebuffer values to zero at start
                Load Don't Care -> Existing contents undefined, don't care

                Store operations
                Store -> Rendered contents stored in memory, may be read later
                Don't Care -> Contents will be undefined after rendering

                We want to clear before the frame begins, and afterwards
                store (in order to see the output)
            */
            .loadOp = VK_ATTACHMENT_LOAD_OP_CLEAR,
            .storeOp = VK_ATTACHMENT_STORE_OP_STORE,

            // Not using stencil so ignore
            .stencilLoadOp = VK_ATTACHMENT_LOAD_OP_DONT_CARE,
            .stencilStoreOp = VK_ATTACHMENT_STORE_OP_DONT_CARE,

            /*
                Images are stored in some particular layout in VkImages, but
                the in-memory layout may change depending on the operations.

                Common layouts:
                COLOR_ATTACHMENT_OPTIONAL -> Image used as color attachment
                PRESENT_SRC_KHR -> Images are to be presented in swap chain
                TRANSFER_DST_OPTIMAL -> Images used as destination for memory 
                copy op

                Images need to be transitioned to specific layouts for the
                operations thye will be used with.

                initialLayout specifies what layout the image has before the 
                render pass begins. In this case, we use
                VK_IMAGE_LAYOUT_UNDEFINED as we do not care what the existing
                layout is. (With this mode contents not guaranteed stored;
                this is OK because we clear it)

                finalLayout specifies the layout to transition to when the
                render pass is complete. We want the image to be presentable
                to swap chain so choose VK_IMAGE_LAYOUT_PRESENT_SRC_KHR.
            */
            .initialLayout = VK_IMAGE_LAYOUT_UNDEFINED,
            .finalLayout = VK_IMAGE_LAYOUT_PRESENT_SRC_KHR
        };

        // MARK: Subpasses and Attachment References
        /*
            Single render pass may have multiple subpasses, in order to do some
            rendering operations in sequence which depend on prior rendering
            of the frame. Vulkan may optimize if you use this approach instead
            of several render passes.

            We are only performing a single render pass in this run.

            Subpasses reference color attachments, and we need to create
            those references.
        */
        VkAttachmentReference colorAttachmentRef {
            /*
                Attachment parameter specifies index in attachment descriptions
                array to reference. 
            */
            .attachment = 0,
            /* 
                Layout specifies the desired layout the attachment should have
                during subpass using this reference. Vulkan automatically
                transitions attachment to this when subpass started.
                We intend to use color attachment as color buffer so we use 
                COLOR_ATTACHMENT_OPTIONAL
            */
            .layout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL
        };

        /*
            Next we define the subpass itself
        */
        VkSubpassDescription subpass {
            // Compute subpasses not currently supported (?) but may be in
            // future. Thus we specify graphics.
            // TODO: Has this been added since writing of this tutorial?
            .pipelineBindPoint = VK_PIPELINE_BIND_POINT_GRAPHICS,
            
            // Define color attachment
            .colorAttachmentCount = 1,
            .pColorAttachments = &colorAttachmentRef
            /*
                The layout(location = 0) out vec4 outColor
                referers to THIS INDEX in the attachment reference.
            */
        };

        VkRenderPassCreateInfo renderPassInfo {
            .sType = VK_STRUCTURE_TYPE_RENDER_PASS_CREATE_INFO,
            .attachmentCount = 1,
            .pAttachments = &colorAttachment,
            .subpassCount = 1,
            .pSubpasses = &subpass
        };

        // NOTE: Must clean up render pass
        VkResult createRenderPassResult = vkCreateRenderPass(
            device,
            &renderPassInfo,
            nullptr,
            &renderPass
        );
        if(createRenderPassResult != VK_SUCCESS) {
            throw std::runtime_error("failed to create render pass!");
        }
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
        vkDestroyCommandPool(
            device,
            commandPool,
            nullptr
        );
        // Destroy framebuffers after we are finished rendering
        for(VkFramebuffer frameBuffer : swapChainFrameBuffers) {
            vkDestroyFramebuffer(
                device,
                frameBuffer,
                nullptr
            );
        }
        // Clean up pipeline. Should only be done at end of program as it is 
        // needed for all drawing operations.
        vkDestroyPipeline(
            device,
            graphicsPipeline,
            nullptr
        );

        // Clean up pipeline layout (uniforms)
        vkDestroyPipelineLayout(
            device, 
            pipelineLayout, 
            nullptr
        );

        // Clean up render pass
        vkDestroyRenderPass(
            device, 
            renderPass, 
            nullptr
        );

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
