#if defined(__INTELLISENSE__) || !defined(USE_CPP20_MODULES)
#	include <vulkan/vulkan_raii.hpp>
#else
import vulkan_hpp;
#endif
#define GLFW_INCLUDE_VULKAN
#include <GLFW/glfw3.h>

#include <cstdlib>
#include <iostream>
#include <stdexcept>
#include <cstdint>
#include <limits>
#include <algorithm>

#define _CRTDBG_MAP_ALLOC
#include <stdlib.h>
#include <crtdbg.h>

constexpr uint32_t WIDTH  = 800;
constexpr uint32_t HEIGHT = 600;

#ifdef NDEBUG
constexpr bool enableValidationLayers = false;
#else
constexpr bool enableValidationLayers = true;
#endif

class HelloTriangleApplication
{
  public:
	void run()
	{
		initWindow();
		initVulkan();
		mainLoop();
		cleanup();
	}

  private:
	GLFWwindow* window = nullptr;

	// Vulkan API objects
	vk::raii::Context                context;
	vk::raii::Instance               instance       = nullptr;
	vk::raii::PhysicalDevice         physicalDevice = nullptr;
	vk::raii::Device                 device         = nullptr;
	vk::raii::DebugUtilsMessengerEXT debugMessenger = nullptr;
	vk::raii::SurfaceKHR             surface        = nullptr;
	vk::raii::Queue                  graphicsQueue  = nullptr;

	// Swapchain data
	vk::raii::SwapchainKHR           swapChain = nullptr;
	std::vector<vk::Image>           swapChainImages;
	vk::SurfaceFormatKHR             swapChainSurfaceFormat;
	vk::Extent2D                     swapChainExtent;
	std::vector<vk::raii::ImageView> swapChainImageViews;

	// These are needed in multiple places, so they go up here
	const std::vector<char const *> validationLayers  = {"VK_LAYER_KHRONOS_validation"};
	std::vector<const char *> requiredDeviceExtension = {vk::KHRSwapchainExtensionName};

	void initWindow()
	{
		// glfwInit should be called before anything else.
		glfwInit();

		// Set GLFW hints: 1.) don't create an OpenGL API client, and 2.) don't allow the window to be resized.
		glfwWindowHint(GLFW_CLIENT_API, GLFW_NO_API);
		glfwWindowHint(GLFW_RESIZABLE, GLFW_FALSE);

		// Create the window
		window = glfwCreateWindow(WIDTH, HEIGHT, "Vulkan", nullptr, nullptr);
	}

	void initVulkan()
	{
		createInstance();
		setupDebugMessenger();
		createSurface();
		pickPhysicalDevice();
		createLogicalDevice();
		createSwapChain();
		createImageViews();
	}

	void createInstance()
	{
		// App name, engine, and version info struct
		constexpr vk::ApplicationInfo appInfo{
		    .pApplicationName   = "Hello Triangle",
		    .applicationVersion = VK_MAKE_VERSION(1, 0, 0),
		    .pEngineName        = "No Engine",
		    .engineVersion      = VK_MAKE_VERSION(1, 0, 0),
		    .apiVersion         = vk::ApiVersion14};

		// Get the required layers.
		std::vector<char const *> requiredLayers;
		if (enableValidationLayers)
		{
			requiredLayers.assign(validationLayers.begin(), validationLayers.end());
		}

		// Check if the required layers are supported by the Vulkan implementation.
		auto layerProperties = context.enumerateInstanceLayerProperties();
		auto unsupportedLayerIt =
		    std::ranges::find_if(requiredLayers, [&layerProperties](auto const &requiredLayer) {
			    return std::ranges::none_of(layerProperties, [requiredLayer](auto const &layerProperty) { return strcmp(layerProperty.layerName, requiredLayer) == 0; });
		    });
		if (unsupportedLayerIt != requiredLayers.end())
		{
			throw std::runtime_error("Required layer not supported: " + std::string(*unsupportedLayerIt));
		}

		// Get the required extensions.
		auto requiredExtensions = getRequiredInstanceExtensions();

		// Check if the required extensions are supported by the Vulkan implementation.
		auto extensionProperties = context.enumerateInstanceExtensionProperties();
		auto unsupportedPropertyIt =
		    std::ranges::find_if(requiredExtensions, [&extensionProperties](auto const &requiredExtension) {
			    return std::ranges::none_of(extensionProperties, [requiredExtension](auto const &extensionProperty) {
				    return strcmp(extensionProperty.extensionName, requiredExtension) == 0;
			    });
		    });
		if (unsupportedPropertyIt != requiredExtensions.end())
		{
			throw std::runtime_error("Required extension not supported: " + std::string(*unsupportedPropertyIt));
		}

		vk::InstanceCreateInfo createInfo{
		    .pApplicationInfo        = &appInfo,
		    .enabledLayerCount       = static_cast<uint32_t>(requiredLayers.size()),
		    .ppEnabledLayerNames     = requiredLayers.data(),
		    .enabledExtensionCount   = static_cast<uint32_t>(requiredExtensions.size()),
		    .ppEnabledExtensionNames = requiredExtensions.data()};

		instance = vk::raii::Instance(context, createInfo);
	}

	std::vector<const char *> getRequiredInstanceExtensions()
	{
		uint32_t glfwExtensionCount = 0;
		auto     glfwExtensions     = glfwGetRequiredInstanceExtensions(&glfwExtensionCount);

		std::vector extensions(glfwExtensions, glfwExtensions + glfwExtensionCount);
		if (enableValidationLayers)
		{
			extensions.push_back(vk::EXTDebugUtilsExtensionName);
		}

		return extensions;
	}

	void setupDebugMessenger()
	{
		if (!enableValidationLayers)
			return;

		// Select the flag severities that will be shown: warning and error.
		vk::DebugUtilsMessageSeverityFlagsEXT severityFlags(
		    vk::DebugUtilsMessageSeverityFlagBitsEXT::eWarning |
		    vk::DebugUtilsMessageSeverityFlagBitsEXT::eError);

		// Select the flag types that will be shown: general, performance, and validation.
		vk::DebugUtilsMessageTypeFlagsEXT messageTypeFlags(
		    vk::DebugUtilsMessageTypeFlagBitsEXT::eGeneral |
		    vk::DebugUtilsMessageTypeFlagBitsEXT::ePerformance |
		    vk::DebugUtilsMessageTypeFlagBitsEXT::eValidation);

		// Setup and create the debug messenger with the selected severities, flags, and a reference to the callback function.
		vk::DebugUtilsMessengerCreateInfoEXT debugUtilsMessengerCreateInfoEXT{
		    .messageSeverity = severityFlags,
		    .messageType     = messageTypeFlags,
		    .pfnUserCallback = &debugCallback};
		debugMessenger = instance.createDebugUtilsMessengerEXT(debugUtilsMessengerCreateInfoEXT);
	}

	static VKAPI_ATTR vk::Bool32 VKAPI_CALL debugCallback(
	    vk::DebugUtilsMessageSeverityFlagBitsEXT      severity,
	    vk::DebugUtilsMessageTypeFlagsEXT             type,
	    const vk::DebugUtilsMessengerCallbackDataEXT *pCallbackData,
	    void                                         *pUserData)
	{
		std::cerr << "Validation layer: type " << to_string(type) << " msg: " << pCallbackData->pMessage << std::endl;

		return vk::False;
	}

	void createSurface()
	{
		VkSurfaceKHR _surface;
		if (glfwCreateWindowSurface(*instance, window, nullptr, &_surface) != 0){
			throw std::runtime_error("Failed to create window surface!");
		}
		surface = vk::raii::SurfaceKHR(instance, _surface);
	}

	void pickPhysicalDevice()
	{
		// Get all GPUs that support Vulkan.
		std::vector<vk::raii::PhysicalDevice> physicalDevices = instance.enumeratePhysicalDevices();

		// Iterate through GPUs, find the first that meets our needs, and store a reference to it.
		auto const devIter = std::ranges::find_if(physicalDevices, [&](auto const &physicalDevice) { return isDeviceSuitable(physicalDevice); });
		if (devIter == physicalDevices.end())
		{
			throw std::runtime_error("Failed to find a suitable GPU!");
		}
		physicalDevice = *devIter;
		std::cout << "Selected GPU: " << physicalDevice.getProperties().deviceName << std::endl;
	}

	bool isDeviceSuitable(vk::raii::PhysicalDevice const& physicalDevice)
	{
		std::vector<bool> requirements;

		// Requirement #1: Must support Vulkan 1.3 or later.
		requirements.push_back(physicalDevice.getProperties().apiVersion >= vk::ApiVersion13);

		// Requirement #2: Must support graphics operations.
		auto queueFamilies = physicalDevice.getQueueFamilyProperties();
		requirements.push_back(std::ranges::any_of(queueFamilies, [](auto const &qfp) { return !!(qfp.queueFlags & vk::QueueFlagBits::eGraphics); }));

		// Requirement #3: Must support required extensions.
		auto availableDeviceExtensions = physicalDevice.enumerateDeviceExtensionProperties();
		requirements.push_back(
		    std::ranges::all_of(requiredDeviceExtension, [&availableDeviceExtensions](auto const &requiredDeviceExtension) {
			    return std::ranges::any_of(availableDeviceExtensions, [requiredDeviceExtension](auto const &availableDeviceExtension) {
				    return strcmp(availableDeviceExtension.extensionName, requiredDeviceExtension) == 0;});
		    }));

		// Requirement #4: Must support required optional features.
		auto features = physicalDevice.template getFeatures2<vk::PhysicalDeviceFeatures2,
		                                                     vk::PhysicalDeviceVulkan11Features,
		                                                     vk::PhysicalDeviceVulkan13Features,
		                                                     vk::PhysicalDeviceExtendedDynamicStateFeaturesEXT>();

		requirements.push_back(features.template get<vk::PhysicalDeviceVulkan11Features>().shaderDrawParameters &&
		                       features.template get<vk::PhysicalDeviceVulkan13Features>().dynamicRendering &&
		                       features.template get<vk::PhysicalDeviceExtendedDynamicStateFeaturesEXT>().extendedDynamicState);

		// Requirement #5: Must be a dedicated GPU.
		requirements.push_back(physicalDevice.getProperties().deviceType == vk::PhysicalDeviceType::eDiscreteGpu);

		// If all requirements are met, return true.
		return std::ranges::all_of(requirements, [](const bool requirement) { return requirement; });
	}

	void createLogicalDevice()
	{
		std::vector<vk::QueueFamilyProperties> queueFamilyProperties = physicalDevice.getQueueFamilyProperties();

		// Get the first queue that supports both graphics and presenting to a surface.
		uint32_t queueIndex = ~0;
		for (uint32_t qfpIndex = 0; qfpIndex < queueFamilyProperties.size(); qfpIndex++)
		{
			if ((queueFamilyProperties[qfpIndex].queueFlags & vk::QueueFlagBits::eGraphics) &&
				physicalDevice.getSurfaceSupportKHR(qfpIndex, *surface))
			{
				// Found a queue family that supports both graphics and present.
				queueIndex = qfpIndex;
				break;
			}
		}
		if (queueIndex == ~0)
		{
			throw std::runtime_error("Could not find a queue for graphics and present. Terminating.");
		}
		
		float queuePriority = 0.5f;
		vk::DeviceQueueCreateInfo deviceQueueCreateInfo{
			.queueFamilyIndex = queueIndex,
			.queueCount = 1,
			.pQueuePriorities = &queuePriority
		};
		
		// Come back to this later...
		vk::PhysicalDeviceFeatures deviceFeatures;

		vk::StructureChain<vk::PhysicalDeviceFeatures2,
		                   vk::PhysicalDeviceVulkan11Features,
		                   vk::PhysicalDeviceVulkan13Features,
		                   vk::PhysicalDeviceExtendedDynamicStateFeaturesEXT>
		    featureChain = {
		        {},								 // vk::PhysicalDeviceFeatures2 (empty for now)
		        {.shaderDrawParameters = true},	 // Enable shader draw parameters (from Vulkan 1.1)
		        {.dynamicRendering	   = true},	 // Enable dynamic rendering (from Vulkan 1.3)
		        {.extendedDynamicState = true}}; // Enable extended dynamic state (from the extension)

		vk::DeviceCreateInfo deviceCreateInfo{
		    .pNext                   = &featureChain.get<vk::PhysicalDeviceFeatures2>(),
		    .queueCreateInfoCount    = 1,
		    .pQueueCreateInfos       = &deviceQueueCreateInfo,
		    .enabledExtensionCount   = static_cast<uint32_t>(requiredDeviceExtension.size()),
		    .ppEnabledExtensionNames = requiredDeviceExtension.data()};

		device = vk::raii::Device(physicalDevice, deviceCreateInfo);
		graphicsQueue = vk::raii::Queue(device, queueIndex, 0);
	}
	
	void createSwapChain()
	{
		vk::SurfaceCapabilitiesKHR surfaceCapabilities = physicalDevice.getSurfaceCapabilitiesKHR(*surface);
		swapChainExtent                                = chooseSwapExtent(surfaceCapabilities);
		uint32_t minImageCount                         = chooseSwapMinImageCount(surfaceCapabilities);

		std::vector<vk::SurfaceFormatKHR> availableFormats = physicalDevice.getSurfaceFormatsKHR(*surface);
		swapChainSurfaceFormat                             = chooseSwapSurfaceFormat(availableFormats);

		std::vector<vk::PresentModeKHR> availablePresentModes = physicalDevice.getSurfacePresentModesKHR(*surface);
		vk::PresentModeKHR              presentMode           = chooseSwapPresentMode(availablePresentModes);

		vk::SwapchainCreateInfoKHR swapChainCreateInfo{
		    .surface          = *surface,
		    .minImageCount    = minImageCount,
		    .imageFormat      = swapChainSurfaceFormat.format,
		    .imageColorSpace  = swapChainSurfaceFormat.colorSpace,
		    .imageExtent      = swapChainExtent,
		    .imageArrayLayers = 1,
		    .imageUsage       = vk::ImageUsageFlagBits::eColorAttachment,
		    .imageSharingMode = vk::SharingMode::eExclusive,
		    .preTransform     = surfaceCapabilities.currentTransform,
		    .compositeAlpha   = vk::CompositeAlphaFlagBitsKHR::eOpaque,
		    .presentMode      = chooseSwapPresentMode(availablePresentModes),
		    .clipped          = true};

		swapChain       = vk::raii::SwapchainKHR(device, swapChainCreateInfo);
		swapChainImages = swapChain.getImages();
	}

	vk::Extent2D chooseSwapExtent(vk::SurfaceCapabilitiesKHR const &capabilities)
	{
		if (capabilities.currentExtent.width != std::numeric_limits<uint32_t>::max())
		{
			return capabilities.currentExtent;
		}
		int width, height;
		glfwGetFramebufferSize(window, &width, &height);

		return {
		    std::clamp<uint32_t>(width, capabilities.minImageExtent.width, capabilities.maxImageExtent.width),
		    std::clamp<uint32_t>(height, capabilities.minImageExtent.height, capabilities.maxImageExtent.height)};
	}

	uint32_t chooseSwapMinImageCount(vk::SurfaceCapabilitiesKHR const &surfaceCapabilities)
	{
		auto minImageCount = std::max(3u, surfaceCapabilities.minImageCount);
		if ((0 < surfaceCapabilities.maxImageCount) && (surfaceCapabilities.maxImageCount < minImageCount))
		{
			minImageCount = surfaceCapabilities.maxImageCount;
		}
		return minImageCount;
	}

	vk::SurfaceFormatKHR chooseSwapSurfaceFormat(std::vector<vk::SurfaceFormatKHR> const &availableFormats)
	{
		const auto formatIt = std::ranges::find_if(availableFormats, [](const auto &format) { return format.format == vk::Format::eB8G8R8A8Srgb && format.colorSpace == vk::ColorSpaceKHR::eSrgbNonlinear; });
		return formatIt != availableFormats.end() ? *formatIt : availableFormats[0];
	}

	vk::PresentModeKHR chooseSwapPresentMode(std::vector<vk::PresentModeKHR> const &availablePresentModes)
	{
		// Check available swapchain modes. If Mailbox is available, use it. Otherwise, use first-in/first-out (FIFO).
		assert(std::ranges::any_of(availablePresentModes, [](auto presentMode) { return presentMode == vk::PresentModeKHR::eFifo; }));
		return std::ranges::any_of(availablePresentModes, [](const vk::PresentModeKHR value) { return vk::PresentModeKHR::eMailbox == value; }) ?
		           vk::PresentModeKHR::eMailbox :
		           vk::PresentModeKHR::eFifo;
	}

	void createImageViews()
	{
		assert(swapChainImageViews.empty());

		vk::ImageViewCreateInfo imageViewCreateInfo {
		    .viewType         = vk::ImageViewType::e2D,
		    .format           = swapChainSurfaceFormat.format,
		    .subresourceRange = {vk::ImageAspectFlagBits::eColor, 0, 1, 0, 1}};

		for (auto& image : swapChainImages)
		{
			imageViewCreateInfo.image = image;
			swapChainImageViews.emplace_back(device, imageViewCreateInfo);
		}
	}

	void mainLoop()
	{
		while (!glfwWindowShouldClose(window))
		{
			glfwPollEvents();
		}
	}

	void cleanup()
	{
		// GLFW resources
		glfwDestroyWindow(window);
		glfwTerminate();
	}
};

int main()
{
	try
	{
		HelloTriangleApplication app;
		app.run();
	}
	catch (const std::exception &e)
	{
		std::cerr << e.what() << std::endl;
		return EXIT_FAILURE;
	}

	_CrtDumpMemoryLeaks();
	return EXIT_SUCCESS;
}