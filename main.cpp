#define GLFW_INCLUDE_VULKAN
#include <GLFW/glfw3.h>
#include <algorithm>
#include <cstring>
#include <functional>
#include <iostream>
#include <set>
//#define VULKAN_HPP_NO_EXCEPTIONS
#include <vulkan/vulkan.hpp>

static VKAPI_ATTR VkBool32 VKAPI_CALL debugCallback(VkDebugUtilsMessageSeverityFlagBitsEXT messageSeverity,
                                                    VkDebugUtilsMessageTypeFlagsEXT messageType,
                                                    const VkDebugUtilsMessengerCallbackDataEXT* pCallbackData,
                                                    void* pUserData) {
    std::cerr << "validation layer: " << pCallbackData->pMessage << std::endl;
    return VK_FALSE;
}

class WindowUtil {
    public:
    WindowUtil() {
        if (!glfwInit()) {
            std::cout << "Initialization failed" << std::endl;
        } else {
            auto glfwExtensionCount = 0u;
            auto glfwExtensions = glfwGetRequiredInstanceExtensions(&glfwExtensionCount);
            glfwRequiredInstanceExtensions = { glfwExtensions, glfwExtensions + glfwExtensionCount };
        }
    }

    uint32_t getRequiredExtensionCount() { return glfwExtensionCount; }
    std::vector<const char*> getRequiredExtensions() { return glfwRequiredInstanceExtensions; }

    private:
    uint32_t glfwExtensionCount = 0;
    std::vector<const char*> glfwRequiredInstanceExtensions;
};

class Window {
    public:
    Window(const vk::Instance& instance)
    : window([&]() {
          glfwWindowHint(GLFW_CLIENT_API, GLFW_NO_API);
          window = glfwCreateWindow(640, 480, "My Title", nullptr, nullptr);

          if (glfwVulkanSupported()) {
              std::cout << "Vulkan is available, at least for compute" << std::endl;
          } else {
              std::cout << "Vulkan is NOT available" << std::endl;
          }
          glfwSetKeyCallback(window, key_callback);

          return window;

      }()),
      us([&]() {
          VkSurfaceKHR surface;
          VkResult err = glfwCreateWindowSurface(instance, window, nullptr, &surface);
          if (err) {
              std::cout << "Window surface creation failed" << std::endl;
          }
          return vk::UniqueSurfaceKHR{ surface, instance };
      }()) {}
    ~Window() {
        glfwDestroyWindow(window);
        glfwTerminate();
    }

    static void key_callback(GLFWwindow* window, int key, int scancode, int action, int mods) {
        if (key == GLFW_KEY_ESCAPE && action == GLFW_PRESS)
            glfwSetWindowShouldClose(window, GLFW_TRUE);
    }

    GLFWwindow* window = nullptr;
    vk::UniqueSurfaceKHR us;
};

int main() {
    bool enableDebugging = false;
    // Window window2;
    vk::ApplicationInfo appInfo("Hello Triangle", VK_MAKE_VERSION(1, 0, 0), "No Engine",
                                VK_MAKE_VERSION(1, 0, 0), VK_API_VERSION_1_0);

    // find out what layers are available
    auto installedLayers = vk::enumerateInstanceLayerProperties();
    for (auto& layer : installedLayers) {
        std::cout << layer.layerName << "\n";
    }
    auto layers = std::vector<const char*>{ "VK_LAYER_LUNARG_standard_validation" };

    WindowUtil windowUtil;
    auto requiredExtensions = windowUtil.getRequiredExtensions();

    std::cout << "required extensions"
              << "\n";
    for (auto& extension : requiredExtensions) {
        std::cout << extension << "\n";
    }

    // one that we want
    if (enableDebugging) {
        requiredExtensions.push_back("VK_EXT_debug_utils");
    }

    auto installedExtensions = vk::enumerateInstanceExtensionProperties();
    auto extensions = std::vector<const char*>();
    for (auto& extension : installedExtensions) {
        extensions.emplace_back(extension.extensionName);
        std::cout << extension.extensionName << "\n";
    }

    auto foundExtensions = std::vector<const char*>();
    std::set_intersection(requiredExtensions.begin(), requiredExtensions.end(), extensions.begin(),
                          extensions.end(), std::back_inserter(foundExtensions),
                          [&](const char* a, const char* b) { return strcmp(a, b) < 0; });

    if (foundExtensions.size() < requiredExtensions.size()) {
        std::cout << "required extensions not found\n";
        return 1;
    }

    vk::InstanceCreateInfo instanceCreateInfo(vk::InstanceCreateFlags(), &appInfo, layers.size(),
                                              layers.data(), foundExtensions.size(), foundExtensions.data());

    auto instance = vk::createInstanceUnique(instanceCreateInfo);

    Window window{ instance.get() };

    auto messengerL = [&]() {
        return instance->createDebugUtilsMessengerEXTUnique(
            vk::DebugUtilsMessengerCreateInfoEXT{ vk::DebugUtilsMessengerCreateFlagsEXT(),
                                                  vk::DebugUtilsMessageSeverityFlagBitsEXT::eError |
                                                      vk::DebugUtilsMessageSeverityFlagBitsEXT::eWarning |
                                                      vk::DebugUtilsMessageSeverityFlagBitsEXT::eVerbose |
                                                      vk::DebugUtilsMessageSeverityFlagBitsEXT::eInfo,
                                                  vk::DebugUtilsMessageTypeFlagBitsEXT::eGeneral |
                                                      vk::DebugUtilsMessageTypeFlagBitsEXT::eValidation |
                                                      vk::DebugUtilsMessageTypeFlagBitsEXT::ePerformance,
                                                  debugCallback },
            nullptr, vk::DispatchLoaderDynamic{ *instance });
    };

    using callBackType = decltype(messengerL());
    callBackType messenger = enableDebugging ? messengerL() : callBackType{};

    auto physicalDevices = instance->enumeratePhysicalDevices();
    std::cout << "number of devices  " << physicalDevices.size() << "\n";

    int deviceIndex = std::distance(physicalDevices.begin(),
                                    std::find_if(physicalDevices.begin(), physicalDevices.end(),
                                                 [](const vk::PhysicalDevice& physicalDevice) {
                                                     return strstr(physicalDevice.getProperties().deviceName, "Intel");
                                                 }));
    if (deviceIndex == physicalDevices.size()) {
        std::cout << "No devices found mathing criteria\n";
        return 1;
    }
    auto physicalDevice = physicalDevices[deviceIndex];
    std::cout << "using device at device Index " << deviceIndex << " with name "
              << physicalDevice.getProperties().deviceName << "\n";

    // auto extensionProperties = physicalDevices[0].enumerateDeviceExtensionProperties();

    // vk::UniqueSurfaceKHR surface(window.getSurface(instance));

    while (!glfwWindowShouldClose(window.window)) {
        // Keep running
        // glfwSwapBuffers(window.window);
        glfwPollEvents();
    }
}