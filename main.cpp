#include <cstdint>
#include <vector>
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

    GLFWwindow *window = nullptr;
    VkInstance instance;

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

    void createInstance() {
        // MARK: Enumerate required glfw extensions
        uint32_t glfwExtensionCount = 0;
        const char** glfwExtensions 
            = glfwGetRequiredInstanceExtensions(&glfwExtensionCount);

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
            .enabledExtensionCount = glfwExtensionCount,
            .ppEnabledExtensionNames = glfwExtensions,
        };

        // MARK: Create instance
        VkResult result = vkCreateInstance(
            &createInfo, 
            nullptr, 
            &instance
        );
        if(result != VK_SUCCESS) {
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
    }

    void initVulkan() {
        createInstance();
    }

    void mainLoop() {
        while(!glfwWindowShouldClose(window)) {
            // process events (including window close)
            glfwPollEvents();
        }
    }

    void cleanup() {
        // MARK: Vulkan deinstantiation
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
