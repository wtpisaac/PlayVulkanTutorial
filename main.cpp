#include <cstdint>
#include <cstring>
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

    GLFWwindow *window = nullptr;
    VkInstance instance;
    VkDebugUtilsMessengerEXT debugMessenger;

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

    void initVulkan() {
        createInstance();
        setupDebugMessenger();
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
        if(enableValidationLayers) {
            DestroyDebugUtilsMessengerEXT(
                instance, 
                debugMessenger, 
                nullptr
            );
        }
        
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
