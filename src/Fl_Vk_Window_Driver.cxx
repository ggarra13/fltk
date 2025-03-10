
#include "Fl_Vk_Window_Driver.H"

namespace {
bool isDeviceSuitable(VkPhysicalDevice device)
{
     VkPhysicalDeviceProperties deviceProperties;
     vkGetPhysicalDeviceProperties(device, &deviceProperties);

     VkPhysicalDeviceFeatures deviceFeatures;
     vkGetPhysicalDeviceFeatures(device, &deviceFeatures);

     QueueFamilyIndices indices = findQueueFamilies(device);

     bool extensionsSupported = checkDeviceExtensionSupport(device);

     bool swapChainAdequate = false;
     if (extensionsSupported)
     {
         SwapChainSupportDetails swapChainSupport = querySwapChainSupport(device);
         swapChainAdequate = !swapChainSupport.formats.empty() && !swapChainSupport.presentModes.empty();
     }

     return indices.isComplete() && extensionsSupported && swapChainAdequate;
}

bool checkDeviceExtensionSupport(VkPhysicalDevice device) {
    uint32_t extensionCount;
    vkEnumerateDeviceExtensionProperties(device, nullptr, &extensionCount, nullptr);
    
    std::vector<VkExtensionProperties> availableExtensions(extensionCount);
    vkEnumerateDeviceExtensionProperties(device, nullptr, &extensionCount,
                                         availableExtensions.data());

    std::set<std::string> requiredExtensions(deviceExtensions.begin(), deviceExtensions.end());

    for (const auto& extension : availableExtensions)
    {
        requiredExtensions.erase(extension.extensionName);
    }

    return requiredExtensions.empty();
}

QueueFamilyIndices findQueueFamilies(VkPhysicalDevice device) {
    QueueFamilyIndices indices;

    uint32_t queueFamilyCount = 0;
    vkGetPhysicalDeviceQueueFamilyProperties(device, &queueFamilyCount, nullptr);

    std::vector<VkQueueFamilyProperties> queueFamilies(queueFamilyCount);
    vkGetPhysicalDeviceQueueFamilyProperties(device, &queueFamilyCount, queueFamilies.data());

    int i = 0;
    for (const auto& queueFamily : queueFamilies) {
        if (queueFamily.queueFlags & VK_QUEUE_GRAPHICS_BIT) {
            indices.graphicsFamily = i;
        }

        VkBool32 presentSupport = false;
        vkGetPhysicalDeviceSurfaceSupportKHR(device, i, surfaceKHR(), &presentSupport);

        if (presentSupport) {
            indices.presentFamily = i;
        }

        if (indices.isComplete()) {
            break;
        }

        i++;
    }

    return indices;
}

} // namespace


void Fl_Vk_Window_Driver::init_vulkan()
{
    create_instance();
    pick_physical_device();
    create_logical_dDevice();
    create_surface();
    create_swap_chain();
    create_command_pool();
    create_command_buffers();
    create_semaphores();
    get_queues();
}

void Fl_Vk_Window_Driver::cleanup_vulkan()
{
    vkDestroySemaphore(device, renderFinishedSemaphore(), nullptr);
    vkDestroySemaphore(device, imageAvailableSemaphore(), nullptr);
    vkFreeCommandBuffers(device, commandPool,
                         static_cast<uint32_t>(commandBuffers().size()),
                         commandBuffers().data());
    vkDestroyCommandPool(device, commandPool, nullptr);
    vkDestroySwapchainKHR(device, swapchainKHR(), nullptr);
    vkDestroySurfaceKHR(instance, surfaceKHR(), nullptr);
    vkDestroyDevice(device, nullptr);
    vkDestroyInstance(instance, nullptr);
}

void Fl_Vk_Window_Driver::create_instance()
{
    VkApplicationInfo appInfo = {};
    appInfo.sType = VK_STRUCTURE_TYPE_APPLICATION_INFO;
    appInfo.pApplicationName = "FLTK";
    appInfo.applicationVersion = VK_MAKE_VERSION(1, 0, 0);
    appInfo.pEngineName = "FLTK";
    appInfo.engineVersion = VK_MAKE_VERSION(1, 0, 0);
    appInfo.apiVersion = VK_API_VERSION_1_0;

    VkInstanceCreateInfo createInfo = {};
    createInfo.sType = VK_STRUCTURE_TYPE_INSTANCE_CREATE_INFO;
    createInfo.pApplicationInfo = &appInfo;

    uint32_t glfwExtensionCount = 0;
    const char** glfwExtensions = nullptr;
    // glfwExtensions = glfwGetRequiredInstanceExtensions(&glfwExtensionCount);

    // createInfo.enabledExtensionCount = glfwExtensionCount;
    // createInfo.ppEnabledExtensionNames = glfwExtensions;

    createInfo.enabledLayerCount = 0; // Validation layers could be added here

    if (vkCreateInstance(&createInfo, nullptr, &m_instance) != VK_SUCCESS) {
        throw std::runtime_error("failed to create instance!");
    }
}

void Fl_Vk_Window_Driver::pick_physical_device()
{
    uint32_t deviceCount = 0;
    vkEnumeratePhysicalDevices(instance, &deviceCount, nullptr);

    if (deviceCount == 0) {
        throw std::runtime_error("failed to find GPUs with Vulkan support!");
    }
    
    std::vector<VkPhysicalDevice> devices(deviceCount);
    vkEnumeratePhysicalDevices(instance, &deviceCount, devices.data());

    for (const auto& device : devices) {
        if (isDeviceSuitable(device)) {
            physicalDevice = device;
            break;
        }
    }

    if (physicalDevice == VK_NULL_HANDLE) {
        throw std::runtime_error("failed to find a suitable GPU!");
    }
}

void Fl_Vk_Window_Driver::create_logical_device()
{
}

void Fl_Vk_Window_Driver::create_swap_chain()
{
}

void Fl_Vk_Window_Driver::create_command_pool()
{
    VkCommandPoolCreateInfo poolInfo = {};
    poolInfo.sType = VK_STRUCTURE_TYPE_COMMAND_POOL_CREATE_INFO;
    poolInfo.queueFamilyIndex = graphicsQueueFamilyIndex;
    poolInfo.flags = VK_COMMAND_POOL_CREATE_RESET_COMMAND_BUFFER_BIT;
    if (vkCreateCommandPool(device(), &poolInfo, nullptr, &commandPool) != VK_SUCCESS) {
        throw std::runtime_error("failed to create command pool!");
    }
}

void Fl_Vk_Window_Driver::create_cCommand_Buffers()
{
    commandBuffers().resize(swapchainImages.size());

    VkCommandBufferAllocateInfo allocInfo = {};
    allocInfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO;
    allocInfo.commandPool = commandPool;
    allocInfo.level = VK_COMMAND_BUFFER_LEVEL_PRIMARY;
    allocInfo.commandBufferCount = (uint32_t) commandBuffers().size();

    if (vkAllocateCommandBuffers(device(), &allocInfo, commandBuffers().data()) !=
        VK_SUCCESS)
    {
        throw std::runtime_error("failed to allocate command buffers!");
    }
}

void Fl_Vk_Window_Driver::createSemaphores()
{
    VkSemaphoreCreateInfo semaphoreInfo = {};
    semaphoreInfo.sType = VK_STRUCTURE_TYPE_SEMAPHORE_CREATE_INFO;

    if (vkCreateSemaphore(device(), &semaphoreInfo, nullptr, &imageAvailableSemaphore()) != VK_SUCCESS ||
        vkCreateSemaphore(device(), &semaphoreInfo, nullptr, &renderFinishedSemaphore()) != VK_SUCCESS) {

        throw std::runtime_error("failed to create semaphores!");
    }
}

void Fl_Vk_Window_Driver::getQueues()
{
    vkGetDeviceQueue(device(), graphicsQueueFamilyIndex, 0, &graphicsQueue());
    vkGetDeviceQueue(device(), presentQueueFamilyIndex, 0, &presentQueue());
}

void Fl_Vk_Window_Driver::swap_buffers()
{
    // Acquire an image
    uint32_t imageIndex;
    vkAcquireNextImageKHR(device, swapchain, UINT64_MAX,
                          imageAvailableSemaphore, VK_NULL_HANDLE,
                          &imageIndex);

    // Submit rendering commands
    VkSubmitInfo submitInfo = {};
    submitInfo.sType = VK_STRUCTURE_TYPE_SUBMIT_INFO;

    VkSemaphore waitSemaphores[] = {imageAvailableSemaphore};
    VkPipelineStageFlags waitStages[] = {VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT};
    submitInfo.waitSemaphoreCount = 1;
    submitInfo.pWaitSemaphores = waitSemaphores;
    submitInfo.pWaitDstStageMask = waitStages;

    submitInfo.commandBufferCount = 1;
    submitInfo.pCommandBuffers = &commandBuffers[imageIndex];

    VkSemaphore signalSemaphores[] = {renderFinishedSemaphore};
    submitInfo.signalSemaphoreCount = 1;
    submitInfo.pSignalSemaphores = signalSemaphores;

    vkQueueSubmit(graphicsQueue, 1, &submitInfo, VK_NULL_HANDLE);

    // Present the image
    VkPresentInfoKHR presentInfo = {};
    presentInfo.sType = VK_STRUCTURE_TYPE_PRESENT_INFO_KHR;

    presentInfo.waitSemaphoreCount = 1;
    presentInfo.pWaitSemaphores = signalSemaphores;

    VkSwapchainKHR swapchains[] = {swapchain};
    presentInfo.swapchainCount = 1;
    presentInfo.pSwapchains = swapchains;

    presentInfo.pImageIndices = &imageIndex;

    vkQueuePresentKHR(presentQueue, &presentInfo);

    //Wait for the present queue to complete.
    vkQueueWaitIdle(presentQueue);
}
