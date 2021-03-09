#define NOMINMAX
#define GLFW_INCLUDE_VULKAN
#define VK_USE_PLATFORM_WIN32_KHR
#include <vulkan/vulkan.h>
#define GLFW_INCLUDE_VULKAN
#include <GLFW/glfw3.h>
#define GLFW_EXPOSE_NATIVE_WIN32
#include <GLFW/glfw3native.h>
#include <iostream>
#include <stdexcept>
#include <cstdlib>
#include <vector>
#include <algorithm> // Necessary for std::min/std::max
#include <cstdint> // Necessary for UINT32_MAX
#include <optional>
#include <set>
#include <fstream>
#include <cassert>
#include <cstdint>
#include <glm/glm.hpp>
#include <array>

const int g_MaxFramesInFlight = 2;
size_t g_CurrentFrameIndex = 0;

struct Vertex
{
	glm::vec2 m_Position;
	glm::vec3 m_Color;

	static VkVertexInputBindingDescription RetrieveBindingDescription()
	{
		//The next step is to tell Vulkan how to pass this data format to the vertex shader once its been uploaded into GPU memory. 
		//There are two types of structures we can use to convey this information. The first one is VkVertexInputBindingDescription.
		VkVertexInputBindingDescription bindingDescription{};
		/*
			A vertex binding describes at which rate to load data from memory throughout the vertices. It specifies the number of bytes between data entries and whether to move
			to the next data entry after each vertex or after each instance.

			All of our per-vertex data is packed together in one array, so we're only going to have one binding. The binding parameter specifies the index of the binding
			in the array of bindings. The stride parameter specifies the number of bytes from one entry to the next, and the inputRate parameter can have one of the following
			values:
			- VK_VERTEX_INPUT_RATE_VERTEX: Move to the next data entry after each vertex.
			- VK_VERTEX_INPUT_RATE_INSTANCE: Move to the next data entry after each instance.
		*/
		bindingDescription.binding = 0;
		bindingDescription.stride = sizeof(Vertex);
		bindingDescription.inputRate = VK_VERTEX_INPUT_RATE_VERTEX;

		return bindingDescription;
	}

	//The second structure that describes how to handle vertex input is VkVertexInputAttributeDescription. We will use a helper function to help populate the struct.
	static std::array<VkVertexInputAttributeDescription, 2> RetrieveAttributeDescriptions()
	{
		//An attribute description struct describes how to extract a vertex attribute from a chunk of vertex data originating from a binding description. We have
		//two attributes, a position and a color, so we need two attribute description structs. 
		std::array<VkVertexInputAttributeDescription, 2> attributeDescriptions{};
		attributeDescriptions[0].binding = 0; //Tells Vulkan from which binding the per-vertex data comes from.
		attributeDescriptions[0].location = 0; //References the location directive of the input in the vertex shader. The input in the vertex shader with location 0 is the position, which has two 32-bit float components. 
		/*
			Format describes the type of data of the attribute. A bit confusingly, the formats are specified using the same enumeration as color formats. The following
			shader types and formats are commonly used together:

			- float: VK_FORMAT_R32_SFLOAT
			- vec2: VK_FORMAT_R32G32_SFLOAT
			- vec3: VK_FORMAT_R32G32B32_SFLOAT
			- vec4: VK_FORMAT_R32G32B32A32_SFLOAT

			As you can see, you should use the format where the amount of color channels matches the number of components in the shader data type. It is allowed to use more channels
			than the number of components in the shader, but they will be silently discarded. If the number of channels is lower than the number of components, then the BGA components
			will use the default values of (0, 0, 1). The color type (SFLOAT, UINT, SINT) and bit width should also match the type of the shader input. See the following examples:

			- ivec2: VK_FORMAT_R32G32_SINT, a 2 component vector of 32-bit signed integers.
			- uvec4: VK_FORMAT_R32G32B32A32_UINT, a 4 component vector of 32-bit unsigned integers.
			- double: VK_FORMAT_R64_SFLOAT, a double precision (64-bit) float.

			The format parameter implictly defines the byte size of attribute data and the offset parameter specifies the number of bytes since the start of the per-vertex data to read from.
			The binding is loading one Vertex (our struct) at a time and the position attribute (m_Position) is at an offset of 0 bytes from the beginning of the struct. This is automatically
			calculated using the offsetof macro.

		*/
		attributeDescriptions[0].format = VK_FORMAT_R32G32_SFLOAT;
		attributeDescriptions[0].offset = offsetof(Vertex, m_Position);

		attributeDescriptions[1].binding = 0;
		attributeDescriptions[1].location = 1;
		attributeDescriptions[1].format = VK_FORMAT_R32G32B32_SFLOAT;
		attributeDescriptions[1].offset = offsetof(Vertex, m_Color);

		return attributeDescriptions;
	}
};

//As this is an extension function, it is not automatically loaded. We will thus look up its address ourselves by using vkGetInstanceProcAddr.
VkResult CreateDebugUtilsMessengerEXT(VkInstance instance, const VkDebugUtilsMessengerCreateInfoEXT* pCreateInfo, const VkAllocationCallbacks* pAllocator,
	VkDebugUtilsMessengerEXT* pDebugMessenger)
{
	PFN_vkCreateDebugUtilsMessengerEXT func = (PFN_vkCreateDebugUtilsMessengerEXT)vkGetInstanceProcAddr(instance, "vkCreateDebugUtilsMessengerEXT");
	if (func != nullptr)
	{
		return func(instance, pCreateInfo, pAllocator, pDebugMessenger);
	}
	else
	{
		return VK_ERROR_EXTENSION_NOT_PRESENT;
	}
}

void DestroyDebugUtilsMessengerEXT(VkInstance instance, VkDebugUtilsMessengerEXT debugMessenger, const VkAllocationCallbacks* pAllocator)
{
	PFN_vkDestroyDebugUtilsMessengerEXT func = (PFN_vkDestroyDebugUtilsMessengerEXT)vkGetInstanceProcAddr(instance, "vkDestroyDebugUtilsMessengerEXT");
	if (func != nullptr)
	{
		func(instance, debugMessenger, pAllocator);
	}
}

class VulkanApplication
{
public:
	void Run()
	{
		InitializeWindowLibrary();
		InitializeVulkan();
		MainLoop(); 
		CleanUp(); //Deallocate resources and end the program.
	}

private:
	void InitializeWindowLibrary()
	{
		glfwInit();
		glfwWindowHint(GLFW_CLIENT_API, GLFW_NO_API); //GLFW was originally designed to create an OpenGL context. We need to tell it to not create an OpenGL context.
		m_Window = glfwCreateWindow(m_WindowWidth, m_WindowHeight, "Crescent Engine", nullptr, nullptr);
		glfwSetWindowUserPointer(m_Window, this); //Sets a user-defined pointer of the specified window that can be retrieved. 
		glfwSetFramebufferSizeCallback(m_Window, FramebufferResizeCallback);
	}

	static void FramebufferResizeCallback(GLFWwindow* window, int width, int height)
	{
		VulkanApplication* ourApplication = reinterpret_cast<VulkanApplication*>(glfwGetWindowUserPointer(window));
		ourApplication->m_FramebufferResized = true;
	}

	void InitializeVulkan()
	{
		QueryValidationLayersSupport();
		CreateVulkanInstance();
		SetupDebugMessenger();
		CreateSurface();
		PickPhysicalDevice();
		CreateLogicalDevice();
		CreateSwapChain();
		CreateImageViews();
		CreateRenderPass();
		CreateGraphicsPipeline();
		CreateFramebuffers();
		CreateCommandPool();
		CreateVertexBuffer();
		CreateIndexBuffer();
		CreateCommandBuffers();
		CreateSyncObjects();
	}

	void MainLoop()
	{
		while (!glfwWindowShouldClose(m_Window))
		{
			glfwPollEvents();
			DrawFrame();
		}

		//Remember that all of the operations we have while drawing frames are asynchronous. That means that when we exit the loop in the main loop, drawing and presentation
		//operations may still be going on. Cleaning up resources while that is happening is a bad idea. To fix that, we should wait for the logical device to finish operations
		//before exiting the update function and destroying the window. Alternatively, you can also wait for operations in a specific command queue to be finished with vkQueueWaitIdle.
		//These functions can be used as a very rudimentary way to perform synchronization. You will see that the program will exit without problems while doing so. 
		vkDeviceWaitIdle(m_Device);
	}

	void CleanUpSwapChain()
	{
		for (size_t i = 0; i < m_SwapChainFramebuffers.size(); i++)
		{
			vkDestroyFramebuffer(m_Device, m_SwapChainFramebuffers[i], nullptr);
		}

		//We could recreate the command pool from scratch, but that is rather wasteful. Instead, we will opt to clean up the existing command buffers with the vkFreeCommandBuffers function.
		//This way, we can reuse the existing pool to allocate the new command buffers.
		vkFreeCommandBuffers(m_Device, m_CommandPool, static_cast<uint32_t>(m_CommandBuffers.size()), m_CommandBuffers.data());

		vkDestroyPipeline(m_Device, m_GraphicsPipeline, nullptr);
		vkDestroyPipelineLayout(m_Device, m_PipelineLayout, nullptr); //As the pipeline layout will be referenced throughout the program's lifetime, it should be destroyed.
		vkDestroyRenderPass(m_Device, m_RenderPass, nullptr);

		for (size_t i = 0; i < m_SwapChainImageViews.size(); i++)
		{
			vkDestroyImageView(m_Device, m_SwapChainImageViews[i], nullptr);
		}

		vkDestroySwapchainKHR(m_Device, m_SwapChain, nullptr);
	}

	void CleanUp()
	{
		//Every Vulkan object that we create needs to be explicitly destroyed when we no longer need it.
		//Vulkan's niche is to be explicit about every operation. Thus, its good to be about the lifetime of objects as well.
		CleanUpSwapChain();

		vkDestroyBuffer(m_Device, m_IndexBuffer, nullptr);
		vkFreeMemory(m_Device, m_IndexBufferMemory, nullptr);

		vkDestroyBuffer(m_Device, m_VertexBuffer, nullptr);
		vkFreeMemory(m_Device, m_VertexBufferMemory, nullptr); //Memory should be freed at some point. Memory that is bound to a buffer object may be freed once the buffer is no longer used. We can thus free it after the buffer is destroyed.

		for (size_t i = 0; i < g_MaxFramesInFlight; i++)
		{
			vkDestroySemaphore(m_Device, m_RenderFinishedSemaphores[i], nullptr);
			vkDestroySemaphore(m_Device, m_ImageAvaliableSemaphores[i], nullptr);
			vkDestroyFence(m_Device, m_InFlightFences[i], nullptr);
		}

		vkDestroyCommandPool(m_Device, m_CommandPool, nullptr);	

		vkDestroyDevice(m_Device, nullptr); //Logical devices don't interact directly with instances, which is why it isn't needed as a parameter.

		if (m_ValidationLayersEnabled)
		{
			DestroyDebugUtilsMessengerEXT(m_VulkanInstance, m_DebugMessenger, nullptr);
		}

		vkDestroySurfaceKHR(m_VulkanInstance, m_Surface, nullptr);
		vkDestroyInstance(m_VulkanInstance, nullptr); //All Vulkan resources should be destroyed before the instance is destroyed.

		glfwDestroyWindow(m_Window);
		glfwTerminate();
	}

	std::vector<const char*> RetrieveRequiredExtensions()
	{
		//As Vulkan is a platform agnostic API, extensions are needed to interface with the window system. GLFW has a handy function that returns the extension(s) it needs to do that.
		uint32_t glfwExtensionCount = 0;
		const char** glfwExtensions;
		glfwExtensions = glfwGetRequiredInstanceExtensions(&glfwExtensionCount); //Returns an array of Vulkan instance extensions and stores the count in a provided buffer.

		std::vector<const char*> extensions(glfwExtensions, glfwExtensions + glfwExtensionCount);
		for (const auto& extension : extensions)
		{
			std::cout << "GLFW Extension Required: " << extension << "\n";
		}

		if (m_ValidationLayersEnabled)
		{
			//Note that the extensions listed by GLFW in glfwGetRequiredInstanceExtensions are always required, but the debug messenger extension is conditionally added.
			//While the validation layers will print debug messages to the standard output by default, we can choose to handle this ourselves by providing an explicit 
			//callback in our program. This will also allow us to decide what kind of messages we want to see as not all are necessarily (fatal) errors. 
			//VK_EXT_debug_utils allow us to setup a debug messenger to handle debug messages and their associated details. 
			extensions.push_back(VK_EXT_DEBUG_UTILS_EXTENSION_NAME); //VK_EXT_DEBUG_UTILS_EXTENSION_NAME is equal to the literal string VK_EXT_debug_utils.
		}

		return extensions;
	}

	//How a debug callback function looks like. The VKAPI_ATTR and VKAPI_CALL ensures that the function has the right signature for Vulkan to call it.
	static VKAPI_ATTR VkBool32 VKAPI_CALL DebugCallback(VkDebugUtilsMessageSeverityFlagBitsEXT messageSeverity, VkDebugUtilsMessageTypeFlagsEXT messageType,
		const VkDebugUtilsMessengerCallbackDataEXT* pCallbackData, void* pUserData)
	{
		/*
			The first parameter specifies the severity of the message which is one of the following flags:
			- VK_DEBUG_UTILS_MESSAGE_SEVERITY_VERBOSE_BIT_EXT: Diagnostic messages.
			- VK_DEBUG_UTILS_MESSAGE_SEVERITY_INFO_BIT_EXT: Informational message like the creation of a resource.
			- VK_DEBUG_UTILS_MESSAGE_SEVERITY_WARNING_BIT_EXT: Message about behavior that may not be an error but very likely a bug in your application.
			- VK_DEBUG_UTILS_MESSAGE_SEVERITY_ERROR_BIT_EXT: Message about behavior that is invalid and may cause crashes.

			The second parameter specifies a message type filter for the logs you wish to see get printed in addition to the severity level.
			- VK_DEBUG_UTILS_MESSAGE_TYPE_GENERAL_BIT_EXT: Some event has happened that is unrelated to the specification or performance.
			- VK_DEBUG_UTILS_MESSAGE_TYPE_VALIDATION_BIT_EXT: Something has happened that violates the specification or indicates a possible mistake.
			- VK_DEBUG_UTILS_MESSAGE_TYPE_PERFORMANCE_BIT_EXT: Potential non-optimal use of Vulkan.

			The third parameter refers to a VkDebugUtilsMessengerCallbackDataEXT struct containing the details of the message itself with the most important ones being:
			- pMessage: The debug message as a null terminated string.
			- pObjects - Array of Vulkan object handles related to the message.
			- objectCount - Number of objects in said array.
		*/

		//The values of the enumerations are setup in such a way that we can use a comparison operator to check if a message is equal or worse compared to some level of severity.
		if (messageSeverity >= VK_DEBUG_UTILS_MESSAGE_SEVERITY_WARNING_BIT_EXT)
		{
			//Message is important enough to show.
		}

		std::cerr << "Validation Layer: " << pCallbackData->pMessage << "\n";

		//This boolean that we return indicates if the Vulkan call that triggered the validation layer messege should be aborted. If this returns true, then the call is
		//aborted with the VK_ERROR_VALIDATION_FAILED_EXT error. 
		return VK_FALSE;
	}


	bool QueryValidationLayersSupport()
	{
		uint32_t layerCount = 0;
		vkEnumerateInstanceLayerProperties(&layerCount, nullptr);
		std::vector<VkLayerProperties> avaliableLayers(layerCount);
		vkEnumerateInstanceLayerProperties(&layerCount, avaliableLayers.data());

		//Check if the validation layers we want are supported.
		for (const char* layerName : m_ValidationLayers)
		{
			bool layerFound = false;
			for (const auto& layerProperties : avaliableLayers)
			{
				if (strcmp(layerName, layerProperties.layerName) == 0) //If they are identical, 0 is returned.
				{
					layerFound = true;
					break;
				}
			}

			if (!layerFound)
			{
				return false;
			}
		}

		return true;
	}

	void CreateVulkanInstance()
	{
		//Validation layers are optional components that hook into Vulkan function calls to apply operations such as parameter checking, Vulkan object tracking, call logging etc.
		if (m_ValidationLayersEnabled && !QueryValidationLayersSupport()) //If validation layers are enabled but our layers are not supported...
		{
			throw::std::runtime_error("Validation Layers Requested, but not avaliable!");
		}

		/*
			A Vulkan instance is used to initialize the Vulkan library. This instance is the connection between your application and the Vulkan library and involves 
			specifying details about your application to the driver. This is done through the Application Info struct. While this data is techically optional, 
			it allows the application to pass information about itself to the implementation. Compared to several APIs out there that use function parameters to pass 
			information, a lot of these information is passed through structs in Vulkan instead.
		*/
		VkApplicationInfo applicationInfo{};
		applicationInfo.sType = VK_STRUCTURE_TYPE_APPLICATION_INFO; //Many structs in Vulkan require you to explicitly specify the type of it in this sType member.
		applicationInfo.pApplicationName = "Application Demo";
		applicationInfo.applicationVersion = VK_MAKE_VERSION(1, 0, 0); //Supplied by us, the developers. This is the version of the application.
		applicationInfo.pEngineName = "Crescent Engine";
		applicationInfo.engineVersion = VK_MAKE_VERSION(1, 0, 0); //Supplied by us, the developers. This is the version of the engine.
		applicationInfo.apiVersion = VK_API_VERSION_1_0; //The highest version of Vulkan that the application is designed to use. 

		//The Instance Creation struct tells the Vulkan driver which global extensions and validation layers we want to use. These settings apply to the entire program.
		VkInstanceCreateInfo creationInfo{};
		creationInfo.sType = VK_STRUCTURE_TYPE_INSTANCE_CREATE_INFO;
		creationInfo.pApplicationInfo = &applicationInfo;

		std::vector<const char*> requiredExtensions = RetrieveRequiredExtensions();
		creationInfo.enabledExtensionCount = static_cast<uint32_t>(requiredExtensions.size());
		creationInfo.ppEnabledExtensionNames = requiredExtensions.data();
		
		//For custom debugging for our instance creation call. This isn't possible to do usually as our debugger is created after the instance itself is created. This is why
		//we populate the pNext struct member with our debug information so that it will be used automatically during vkCreateInstance and vkDestroyedInstance and cleaned up after that.
		VkDebugUtilsMessengerCreateInfoEXT debugCreationInfo; //This is placed outside the if statement so that it is not destroyed before the vkCreateInstance call below.
		if (m_ValidationLayersEnabled) 
		{
			creationInfo.enabledLayerCount = static_cast<uint32_t>(m_ValidationLayers.size()); 
			creationInfo.ppEnabledLayerNames = m_ValidationLayers.data(); //Determines the global validation layers to enable based on what we specified.

			PopulateDebugMessengerCreationInfo(debugCreationInfo);
			creationInfo.pNext = (VkDebugUtilsMessengerCreateInfoEXT*)&debugCreationInfo;
		}
		else
		{
			creationInfo.enabledLayerCount = 0;
			creationInfo.pNext = nullptr;
		}

		//Lastly, we can create our instance. Nearly all Vulkan functions return a value of type VkResult that either VK_SUCCESS or an error code. 
		//The general pattern of object creation parameters in Vulkan is as follows:
		//- Pointer to struct with its creation info.
		//- Pointer to custom allocator callbacks.
		//- Pointer to the variable that stores the handle to the new object.
		if (vkCreateInstance(&creationInfo, nullptr, &m_VulkanInstance) != VK_SUCCESS)
		{
			throw std::runtime_error("Failed to create instance.");
		}
		else
		{
			std::cout << "Successfully created Vulkan Instance." << "\n";
		}
	}

	void PopulateDebugMessengerCreationInfo(VkDebugUtilsMessengerCreateInfoEXT& creationInfo)
	{
		//Naturally, even a debug callback in Vulkan is managed with a handle that needs to be explicitly created and destroyed. Such a callback is part of a debug messenger
		//object and we can have as many of them as we want.
		creationInfo = {};
		creationInfo.sType = VK_STRUCTURE_TYPE_DEBUG_UTILS_MESSENGER_CREATE_INFO_EXT;
		//This field allows you to specify all the types of severities you would like your callback to be called for. We've specified all types except 
		//VK_DEBUG_UTILS_MESSAGE_SEVERITY_INFO_BIT_EXT and VK_DEBUG_UTILS_MESSAGE_SEVERITY_VERBOSE_BIT_EXT to receive notifications about possible problems while leaving out verbose general debug information.
		creationInfo.messageSeverity = VK_DEBUG_UTILS_MESSAGE_SEVERITY_WARNING_BIT_EXT | VK_DEBUG_UTILS_MESSAGE_SEVERITY_ERROR_BIT_EXT;
		//Similarly, the messageType field lets you filter which types of messages your callback is notified about. We've simply enabled all types here.
		creationInfo.messageType = VK_DEBUG_UTILS_MESSAGE_TYPE_GENERAL_BIT_EXT | VK_DEBUG_UTILS_MESSAGE_TYPE_VALIDATION_BIT_EXT | VK_DEBUG_UTILS_MESSAGE_TYPE_PERFORMANCE_BIT_EXT;
		//Specifies a pointer to the callback option. You can pass a pointer here that will be passed along to the callback function via the pUserData parameter.
		creationInfo.pfnUserCallback = DebugCallback;
		creationInfo.pUserData = nullptr; //Optional
	}

	void SetupDebugMessenger()
	{
		if (!m_ValidationLayersEnabled) { return; }

		VkDebugUtilsMessengerCreateInfoEXT creationInfo;
		PopulateDebugMessengerCreationInfo(creationInfo);

		if (CreateDebugUtilsMessengerEXT(m_VulkanInstance, &creationInfo, nullptr, &m_DebugMessenger) != VK_SUCCESS)
		{
			throw std::runtime_error("Failed to setup debug messenger.");
		}
	}

	void CreateSurface()
	{
		/*
			As Vulkan is a platform agnostic API, it cannot interface directly with the window system on its own. Thus, we need to establish this connection to present results
			to the screen. We will thus be using the WSI (Window System Integration) extensions. The first one is VK_KHR_surface. It exposes a VkSurfaceKHR object that represents
			an abstract type of surface to present rendered images to. The surface in our program will be backed by the window that we've already opened with GLFW. Note that the
			VK_KHR_surface extension is returned by glfwGetRequiredInstanceExtensions (thus instance level) that we have already enabled alongside some other WSI extensions. 
			The window surface needs to be created right after instance creation as it can actually influence the physical device selection. Note that window surfaces are entirely
			optional in Vulkan. If you just need off-screen rendering, Vulkan allows you to do that unlike OpenGL where a window is at least needed.

			Although the VkSurfaceKHR object and its usage is platform agnostic, its creation isn't as it depends on window system details. For example, it requires the
			HWND and HMODULE handles on Windows. There is also a platform-specific addition to the extension, which on Windows is called VK_KHR_win32_surface, once again included
			in the list from glfwGetRequiredInstanceExtensions. Lets demonstrate how this platform specific extension can be used to create a surface on Windows. However, we won't
			be using it. GLFW does this for us automatically which we will be using instead.

			VkWin32SurfaceCreateInfoKHR creationInfo{};
			creationInfo.sType = VK_STRUCTURE_TYPE_WIN32_SURFACE_CREATE_INFO_KHR;
			creationInfo.hwnd = glfwGetWin32Window(m_Window); //Retrieves the raw HWND from the GLFW window object.
			creationInfo.hinstance = GetModuleHandle(nullptr); //Returns the HINSTANCE handles of the current process.

			if (vkCreateWin32SurfaceKHR(m_VulkanInstance, &creationInfo, nullptr, &m_Surface) != VK_SUCCESS)
			{
				throw std::runtime_error("Failed to create Window Surface.");
			}
		*/

		//GLFW provides us with an implementation that does all of the above for us, with a different implementation avalaible for each platform.
		if (glfwCreateWindowSurface(m_VulkanInstance, m_Window, nullptr, &m_Surface) != VK_SUCCESS)
		{
			throw std::runtime_error("Failed to create window surface.");
		}
	}

	struct QueueFamilyIndices
	{
		//We use optional here to indicate whether a particular queue family was found. This is good as while we may *prefer* devices with a dedicated transfer queue family, but not require it.
		//It is likely that the queues end up being from the same queue family, but throughout our program we will teat them as if they were seperate queues for a uniform approach.
		//Nevertheless, you could add logic to explicitly prefer a physical device that supports drawing and presentation in the same queue for improved performance. 
		std::optional<uint32_t> m_GraphicsFamily;
		std::optional<uint32_t> m_PresentationFamily; //While Vulkan supports window system integration, it doesn't mean that every device in the system supports it.

		bool IsComplete()
		{
			return m_GraphicsFamily.has_value() && m_PresentationFamily.has_value();
		}
	};

	QueueFamilyIndices FindQueueFamilies(VkPhysicalDevice device)
	{
		//Almost every operation in Vulkan from drawing to texture uploads requires commands to be submitted to a queue. There are different queues that a device may support,
		//such as a graphics rendering queue, texture uploading queue etc. In Vulkan, queues are collected together into queue families. These queue families each contain
		//a set of queues which all support the same type of operations. We thus need to check which queue familes are supported by the device and which one supports the commands
		//that we want to use. 

		//Logic to find a graphics queue family.
		QueueFamilyIndices indices;

		uint32_t queueFamilyCount = 0;
		vkGetPhysicalDeviceQueueFamilyProperties(device, &queueFamilyCount, nullptr);
		std::vector<VkQueueFamilyProperties> queueFamilies(queueFamilyCount);
		vkGetPhysicalDeviceQueueFamilyProperties(device, &queueFamilyCount, queueFamilies.data());

		int i = 0;
		for (const VkQueueFamilyProperties& queueFamily : queueFamilies)
		{
			VkBool32 presentationSupport = false;
			vkGetPhysicalDeviceSurfaceSupportKHR(device, i, m_Surface, &presentationSupport);

			if (queueFamily.queueFlags & VK_QUEUE_GRAPHICS_BIT) //If anyone supports graphics queues...
			{
				indices.m_GraphicsFamily = i;
			}

			if (presentationSupport)
			{
				indices.m_PresentationFamily = i;
			}

			if (indices.IsComplete()) //If all conditions are already fulfilled, exit the loop.
			{
				break;
			}

			i++;
		}

		return indices;
	}

	struct SwapChainSupportDetails
	{
		VkSurfaceCapabilitiesKHR m_Capabilities; //Min/Max number of images in swapchain, min/max width and height of images etc.
		std::vector<VkSurfaceFormatKHR> m_Formats; //Pixel Format/Color Space
		std::vector<VkPresentModeKHR> m_PresentationModes; //Avaliable presentation modes.
	};

	SwapChainSupportDetails QuerySwapChainSupport(VkPhysicalDevice device)
	{
		/*
			Vulkan does not have the concept of a default framebuffer. Hence, it requires an infrastructure that will own the buffers we will render to before we visualize them
			on the screen. This infrastructure is known as the swapchain and must be created explicitly in Vulkan. The swapchain is essentially a queue of images that are waiting to be
			presented to the screen. Our application will acquire such an image to draw to it and then return it to the queue. How exactly the queue works and the conditions for presenting an image
			from the queue depends on how the swapchain is set up, but the general purpose of the swapchain is to synchronize the presentation of images with the refresh rate of the screen. 
		*/
		SwapChainSupportDetails details;
		vkGetPhysicalDeviceSurfaceCapabilitiesKHR(device, m_Surface, &details.m_Capabilities);

		uint32_t formatCount;
		vkGetPhysicalDeviceSurfaceFormatsKHR(device, m_Surface, &formatCount, nullptr);
		if (formatCount != 0)
		{
			details.m_Formats.resize(formatCount);
			vkGetPhysicalDeviceSurfaceFormatsKHR(device, m_Surface, &formatCount, details.m_Formats.data());
		}

		uint32_t presentModeCount;
		vkGetPhysicalDeviceSurfacePresentModesKHR(device, m_Surface, &presentModeCount, nullptr);
		if (presentModeCount != 0)
		{
			details.m_PresentationModes.resize(presentModeCount);
			vkGetPhysicalDeviceSurfacePresentModesKHR(device, m_Surface, &presentModeCount, details.m_PresentationModes.data());
		}

		return details;
	}

	//If the swapChainAdequete conditions were met, then the support is definitely sufficient. However, there may still be many modes with varying optimality.
	//We will now try to determine for the best possible swapchain based on 3 types of settings:
	//- Surface Format (Color Depth)
	//- Presentation Mode (Conditions for "swapping" images to the screen)
	//- Swap Extent (Resolution of images in the swapchain)
	VkSurfaceFormatKHR ChooseSwapchainSurfaceFormat(const std::vector<VkSurfaceFormatKHR>& avaliableFormats)
	{
		/*
			Each VkSurfaceFormatKHR entry contains a format and a colorspace member. The format member specifies color channels and types. For example, VK_FORMAT_B8G8R8A8_SRGB means
			that we store the B, R, G and Alpha channels in that order with an 8 bit unsigned integer for a total of 32 bits per pixel. The colorSpace member indicates if the SRGB color
			space is supported or not using the VK_COLOR_SPACE_SRGB_NONLINEAR_KHR flag. For the color space, we will use SRGB if it is avaliable, because it results in more accurately
			perceived colors. It is also pretty much the standard color space for images, like the textures we will use later on. Because of that, we should also use an SRGB color format, a common
			one being VK_FORMAT_B8G8R8A8_SRGB. 
		*/

		for (const VkSurfaceFormatKHR& avaliableFormat : avaliableFormats)
		{
			if (avaliableFormat.format == VK_FORMAT_B8G8R8A8_SRGB && avaliableFormat.colorSpace == VK_COLOR_SPACE_SRGB_NONLINEAR_KHR)
			{
				return avaliableFormat;
			}
		}
		//If the above fails, we could start ranking the avaliable formats based on how good they are. But in most cases, it is okay to settle with the first format specified.
		return avaliableFormats[0];
	}

	VkPresentModeKHR ChooseSwapchainPresentationMode(const std::vector<VkPresentModeKHR>& avaliablePresentationModes)
	{
		/*
			The presentation mode is arguably the most important setting for the swapchain, because it represents the actual conditions for showing images to the screen.
			There are 4 possible modes avaliable in Vulkan:

			- VK_PRESENT_MODE_IMMEDIATE_KHR: Images submitted by your application are transferred to the screen right away which made result in tearing.
			- VK_PRESENT_MODE_FIFO_KHR: The swapchain is a queue where the display takes an image from the front of the queue when the display is refreshed and the program inserts rendered
			images at the back of the queue. If the queue is full, then the program has to wait. This is most similar to vertical sync (VSync) as found in modern games. The moment that the display is refreshed is known as "vertical blank".
			- VK_PRESENT_MODE_FIFO_RELAXED_KHR: This mode only differs from the previous one if the application is late and the queue was empty at the last vertical blank. Instead of
			waiting for the next vertical blank, the image is transferred right away when it finally arrives. This may result in visible tearing.
			- VK_PRESENT_MODE_MAILBOX_KHR: This is another variation of the second mode. Instead of blocking the application when the queue is full, the images that are already
			queued are simply replaced with the newer ones. This mode can be used to implement triple buffering, which allows you to avoid tearing with significantly less latency issues than standard vertical
			sync that uses double buffering. 

			VSync is a graphics technology that synchronizes the frame rate of a game and the fresh rate of a gaming monitor. This tech was a way to deal with screen tearing,
			which is when your screen displays portions of multiple frames at one go, resulting in a display that appears split along a line, usually horizontally. 
			Tearing occurs when the refresh rate of the monitor (how many times it updates per second) is not in sync with the frames per second. 

			Only the VK_PRESENT_MODE_FIFO_KHR mode is guarenteed to be avaliable, so we will again have to write a function that looks for the best mode avaliable. 
		*/

		for (const VkPresentModeKHR& avaliablePresentationMode : avaliablePresentationModes)
		{
			//Triple buffering is a very nice tradeoff as it allow us to prevent tearing whilst still maintaining a fairly low latency by rendering new images that are as up-to-date as possible right until the vertical blank.
			if (avaliablePresentationMode == VK_PRESENT_MODE_MAILBOX_KHR)
			{
				return avaliablePresentationMode;
			}
		}

		//If our preferred option isn't avaliable, just return the default one. 
		return VK_PRESENT_MODE_FIFO_KHR;
	}

	VkExtent2D ChooseSwapExtent(const VkSurfaceCapabilitiesKHR& capabilities)
	{
		/*
			The swap extent is the resolution of the swapchain images and its almost always exactly equal to the resolution of the window that we're drawing to in pixels. The range
			of the possible resolutions is defined in the VkSurfaceCapabiltiesKHR structure. Vulkan tells us to match the resolution of the window by setting the
			width and height in the currentExtent member. However, some window managers do allow us to differ here and this is indicated by setting the width and height in
			currentExtent to a special value: the maximum value of uint32_t. In that case, we will pick the resolution that best matches the window within the minImageExtent and
			maxImageExtent bounds. We must specify the resolution in the correct unit.

			GLFW uses 2 units when measuring sizes: pixels and screen coordinates. For example, the resolution (width/height) that we specified earlier when creating the window
			is measured in screen coordinates. However, Vulkan works with pixels, so the swapchain extent must be specified in pixels as well. Unfortunately, if you are using
			a high DPI display (like Apple's Retina display), screen coordinates don't correspond to pixels. Instead, due to the higher pixel density, the resolution of the window
			in pixel will be larger than the resolution in screen coordinates. Thus, if Vulkan doesn't fix the swap extent for us, we can't just use the original width and height.
			Instead, we must use glfwGetFramebufferSize to query the resolution of the window in pixel before matching it against the minimum and maximum image extent. 
		*/

		if (capabilities.currentExtent.width != UINT32_MAX) //The maximum number that we can store with an unsigned 32bit integer.
		{
			return capabilities.currentExtent;
		}
		else //We pick the resolution that best matches the window.
		{
			int windowWidth, windowHeight;
			glfwGetFramebufferSize(m_Window, &windowWidth, &windowHeight);
			VkExtent2D actualExtent = { static_cast<uint32_t>(windowWidth), static_cast<uint32_t>(windowHeight) };

			//The max and min functions are used here to clamp the value of WIDTH and HEIGHT between the allowed minimum and maximum extents that are supported by the implementation.
			actualExtent.width = std::max(capabilities.minImageExtent.width, std::min(capabilities.maxImageExtent.width, actualExtent.width));
			actualExtent.height = std::max(capabilities.minImageExtent.height, std::min(capabilities.maxImageExtent.height, actualExtent.height));

			return actualExtent;
		}
	}

	bool CheckDeviceExtensionSupport(VkPhysicalDevice device)
	{
		//Not all graphic cards are capable of presenting images directly to a screen for various reasons, for example because they are designed for servers and don't have any display outputs.
		//Secondly, since image presentation is heavily tied to the window system and surfaces associated with windows, it is not actually part of the Vulkan core. We have to enable the
		//VK_KHR_swapchain device extension after querying for its support. 
		uint32_t extensionCount;
		vkEnumerateDeviceExtensionProperties(device, nullptr, &extensionCount, nullptr);
		std::vector<VkExtensionProperties> avaliableExtensions(extensionCount);
		vkEnumerateDeviceExtensionProperties(device, nullptr, &extensionCount, avaliableExtensions.data());

		std::set<std::string> requiredExtensions(m_DeviceExtensions.begin(), m_DeviceExtensions.end()); //Constructs a container with as many elements as the range.
		for (const VkExtensionProperties& extension : avaliableExtensions)
		{
			requiredExtensions.erase(extension.extensionName); 
		}

		if (requiredExtensions.empty()) //If empty, all extensions are supported. Else, return false indicating an extension is not supported.
		{
			std::cout << "All required device extensions supported." << "\n";
			return true;
		}
		else
		{
			//Error.
			return false;
		}
	}

	bool IsPhysicalDeviceSuitable(VkPhysicalDevice device)
	{
		//Our grading system to select a physical device. We can select graphic cards that support certain functionality that we want, such as geometry shaders,
		//or even create a score system to select the best graphic card, adding score increasingly with each supported feature. Alternatively, we may even let users select them themselves.
		VkPhysicalDeviceProperties deviceProperties; //This allows us to query basic device properties such as the name, type and supported Vulkan versions.
		vkGetPhysicalDeviceProperties(device, &deviceProperties);

		std::cout << "Found Physical Device: " << deviceProperties.deviceName << "\n";

		//This allows us to query for optional features like texture compression, 64 bit floats and multi viewport rendering.
		VkPhysicalDeviceFeatures deviceFeatures;
		vkGetPhysicalDeviceFeatures(device, &deviceFeatures);

		//==============================================================

		QueueFamilyIndices indices = FindQueueFamilies(device);
		bool extensionsSupported = CheckDeviceExtensionSupport(device);

		//Swapchain support is sufficient for our needs right now if at least one supported image format and one supported presentation mode is avaliable.
		bool swapChainAdequete = false;
		if (extensionsSupported) //It is important that we only try to query for swapchain support after verifying that the extension is avaliable.
		{
			SwapChainSupportDetails swapChainSupport = QuerySwapChainSupport(device);
			swapChainAdequete = !swapChainSupport.m_Formats.empty() && !swapChainSupport.m_PresentationModes.empty();
		}

		//We will accept it if all requirements were fulfilled.
		return indices.IsComplete() && extensionsSupported && swapChainAdequete;
	}

	void PickPhysicalDevice()
	{
		//After initializing the library, we need to look for and select a graphics card in the system that supports the features we need. In fact, we can select any number
		//of graphics cards and use them simultaneously, but for now, we will stick to the first one that suits our needs.
		uint32_t deviceCount;
		vkEnumeratePhysicalDevices(m_VulkanInstance, &deviceCount, nullptr);
		if (deviceCount == 0) //No point going further if there are 0 devices with Vulkan support.
		{
			throw std::runtime_error("Failed to find GPUs with Vulkan Support!");
		}
		std::vector<VkPhysicalDevice> devices(deviceCount);
		vkEnumeratePhysicalDevices(m_VulkanInstance, &deviceCount, devices.data());

		for (const VkPhysicalDevice& device : devices)
		{
			if (IsPhysicalDeviceSuitable(device)) 
			{
				m_PhysicalDevice = device;
				break;
			}
		}

		if (m_PhysicalDevice == VK_NULL_HANDLE)
		{
			throw std::runtime_error("Failed to find a suitable GPU!");
		}
	}

	void CreateLogicalDevice()
	{
		QueueFamilyIndices indices = FindQueueFamilies(m_PhysicalDevice);

		//Describes the number of queues we want for a single queue family. The queues are created along with the logical device which we must use a handle to interface with.
		std::vector<VkDeviceQueueCreateInfo> queueCreationInfos;
		std::set<uint32_t> uniqueQueueFamilies = { indices.m_GraphicsFamily.value(), indices.m_PresentationFamily.value() };

		//The currently avaliable drivers will only allow you to create a small number of queues for each queue family and you don't really need more than one.
		//That's because you can create all of the command buffers on multiple threads and then submit them all at once on the main thread with a single low-overhead call.
		//Vulkan allows you to assign priorities to queues to influence the scheduling of command buffer execution using floating point numbers between 0.0 and 1.0. This is required even if there is only a single queue.
		float queuePriority = 1.0f;
		
		for (uint32_t queueFamily : uniqueQueueFamilies)
		{
			VkDeviceQueueCreateInfo queueCreationInfo{};
			queueCreationInfo.sType = VK_STRUCTURE_TYPE_DEVICE_QUEUE_CREATE_INFO;
			queueCreationInfo.queueFamilyIndex = queueFamily;
			queueCreationInfo.queueCount = 1;
			queueCreationInfo.pQueuePriorities = &queuePriority;
			queueCreationInfos.push_back(queueCreationInfo);
		}
			

		//Specifies the set of device features that we will be using. These are the features that we queried support for with vkGetPhysicalDeviceFeatures.
		VkPhysicalDeviceFeatures deviceFeatures{};

		//Main device creation info struct.
		VkDeviceCreateInfo creationInfo{};
		creationInfo.sType = VK_STRUCTURE_TYPE_DEVICE_CREATE_INFO;

		creationInfo.queueCreateInfoCount = static_cast<uint32_t>(queueCreationInfos.size());
		creationInfo.pQueueCreateInfos = queueCreationInfos.data();

		creationInfo.pEnabledFeatures = &deviceFeatures;
		//We now have to specify extensions and validation layers similar to VkInstanceCreateInfo. However, the difference is that these are device specific this time.
		//An example of a device specific extension is VK_KHR_swapchain, which allows you to present rendered images from that device to windows. It is possible that there are
		//Vulkan devices in the system that lack this ability, for example because they only support compute operations.
		//Previous implementations made a distinction between instance and device specific validation layers, but this is no longer the case. This means that enabledLayerCount
		//and ppEnabledLayerNames fields of vkDeviceCreateInfo are ignored by up-to-date implementations. It is a good idea to set them anyway to be compatible with older implementations.
		creationInfo.enabledExtensionCount = static_cast<uint32_t>(m_DeviceExtensions.size());
		creationInfo.ppEnabledExtensionNames = m_DeviceExtensions.data();

		if (m_ValidationLayersEnabled)
		{
			creationInfo.enabledLayerCount = static_cast<uint32_t>(m_ValidationLayers.size());
			creationInfo.ppEnabledLayerNames = m_ValidationLayers.data();
		}
		else
		{
			creationInfo.enabledLayerCount = 0;
		}

		if (vkCreateDevice(m_PhysicalDevice, &creationInfo, nullptr, &m_Device) != VK_SUCCESS)
		{
			throw std::runtime_error("Failed to create logical device.");
		}

		//We can use vkGetDeviceQueue to retrieve queue handles for each queue family. In case the queue families are the same, the two handles will liely have the same value.
		vkGetDeviceQueue(m_Device, indices.m_GraphicsFamily.value(), 0, &m_GraphicsQueue);
		vkGetDeviceQueue(m_Device, indices.m_PresentationFamily.value(), 0, &m_PresentationQueue);
	}

	void CreateSwapChain()
	{
		SwapChainSupportDetails swapChainSupport = QuerySwapChainSupport(m_PhysicalDevice);

		VkSurfaceFormatKHR surfaceFormat = ChooseSwapchainSurfaceFormat(swapChainSupport.m_Formats);
		VkPresentModeKHR presentMode = ChooseSwapchainPresentationMode(swapChainSupport.m_PresentationModes);
		VkExtent2D swapExtent = ChooseSwapExtent(swapChainSupport.m_Capabilities);

		//Aside from these properties, we also have to decide how many images we would like to have in the swap chain. The implementation specifies the minimum number
		//that it requires to function. However, simply sticking to this minimum means that we may sometimes have to wait on the driver to complete internal operations before
		//we can acquire another image to render to. Therefore, it is recommended to request at least 1 more image than the minimum.
		uint32_t imageCount = swapChainSupport.m_Capabilities.minImageCount + 1;
		//We should also make sure to not exceed the maximum number of images while doing this, while 0 is a special value that means that there is no maximum. 
		if (swapChainSupport.m_Capabilities.maxImageCount > 0 && imageCount > swapChainSupport.m_Capabilities.maxImageCount)
		{
			imageCount = swapChainSupport.m_Capabilities.maxImageCount;
		}
		
		VkSwapchainCreateInfoKHR creationInfo{};
		creationInfo.sType = VK_STRUCTURE_TYPE_SWAPCHAIN_CREATE_INFO_KHR;
		creationInfo.surface = m_Surface; //Which surface the swapchain should be tied to.
		creationInfo.minImageCount = imageCount;
		creationInfo.imageFormat = surfaceFormat.format;
		creationInfo.imageColorSpace = surfaceFormat.colorSpace;
		creationInfo.imageExtent = swapExtent;
		//imageArrayLayers specifies the amount of layers each image consists of. This is always 1 unless you are developing a stereoscopic 3D application.
		//The imageUsage biut field specifies what klind of operations we will be using the images in the swapchain for. Here, we're going to render directly to them,
		//which means that they're used as color attachments. It is also possible that you will render images to a seperate image first to perform operations like post-processing.
		//In that case, you may use a value like VK_IMAGE_USAGE_TRANSFER_DST_BIT instead and use a memory operation to transfer the rendered image to a swapchain image.
		creationInfo.imageArrayLayers = 1;
		creationInfo.imageUsage = VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT;
		/*
			We now specify how to handle swap chain images that will be used across multiple queue families. That will be the case in our application if the graphics queue family
			is different from the presentation queue. We will be drawing on the images in the swap chain from the graphics queue and then submitting them on the presentation queue. 
			There are two ways to handle images that are accessed from multiple queues:
			- VK_SHARING_MODE_EXCLUSIVE: An image is owned by one queue family at a time and ownership must be explictly transferred before using it in another queue family. This offers the best performance.
			- VK_SHARING_MODE_CONCURRENT: Images can be used across multiple queue families without explicit ownership transfers.
			Concurrent mode requires you to specify in advance between which queue families ownership will be sharing using queueFamilyIndexCount and pQueueFamilyIdnices parameters.
			If the graphics queue family and presentation queue family are the same, which will be the case on most hardware, we should stick to exclusive mode, because concurrent mode requires you to specify at least 2 distinct queue families.
		*/
		QueueFamilyIndices indices = FindQueueFamilies(m_PhysicalDevice);
		uint32_t queueFamilyIndices[] = { indices.m_GraphicsFamily.value(), indices.m_PresentationFamily.value() };

		if (indices.m_GraphicsFamily != indices.m_PresentationFamily)
		{
			creationInfo.imageSharingMode = VK_SHARING_MODE_CONCURRENT;
			creationInfo.queueFamilyIndexCount = 2;
			creationInfo.pQueueFamilyIndices = queueFamilyIndices;
		}
		else
		{
			creationInfo.imageSharingMode = VK_SHARING_MODE_EXCLUSIVE;
			creationInfo.queueFamilyIndexCount = 0; //Optional
			creationInfo.pQueueFamilyIndices = nullptr;
		}

		//We can also specify that a certain transform be applied to images in the swapchain if it is supported (supportTransforms in capabilities), like a 90 degree clockwise roation or horizontal flip.
		//To specify that you do not want any transformation, simply specify the current transformatiohn.
		creationInfo.preTransform = swapChainSupport.m_Capabilities.currentTransform;

		//The compositeAlpha field specifies if the alpha channel should be used for blending with other windows in the window system. You will almost always want to simply ignore the alpha channel.
		//Hence, use VK_COMPOSITE_ALPHA_OPAQUE_BIT_KHR.
		creationInfo.compositeAlpha = VK_COMPOSITE_ALPHA_OPAQUE_BIT_KHR;

		//The presentMode member speaks for itself. If the clipped member is set to VK_TRUE, then that means that we don't care about the color of pixels that are obscured,
		//for example because another window is in front of them. Unless you really need to be able to read these pixels back and get predictable results, you will get the best performance by enabling clipping.
		creationInfo.presentMode = presentMode;
		creationInfo.clipped = VK_TRUE;

		//That leaves one last field, oldSwapChain. With Vulkan, it is possible that your swapchain becomes invalid or unoptimized while your application is running, for example
		//because the window was resized. In that case, the swapchain actually needs to be recreated from scratch and a reference to the old one must be specified in this field. 
		//This is rather complex, so for now we will assume that we will only ever create one swap chain.
		creationInfo.oldSwapchain = VK_NULL_HANDLE;

		if (vkCreateSwapchainKHR(m_Device, &creationInfo, nullptr, &m_SwapChain) != VK_SUCCESS)
		{
			throw std::runtime_error("Failed to create swapchain!");
		}
		else
		{
			std::cout << "Successfully created Swapchain." << "\n";
		}

		//Remember that we only specified a minimum number of images in the swapchain, so the implementation is allowed to create a swapchain with more.
		//Thus, we will first query the final number of images with vkGetSwapchainImagesKHR, then resize the container and finally call it again to retrieve the handles.
		vkGetSwapchainImagesKHR(m_Device, m_SwapChain, &imageCount, nullptr);
		m_SwapChainImages.resize(imageCount);

		vkGetSwapchainImagesKHR(m_Device, m_SwapChain, &imageCount, m_SwapChainImages.data());
		m_SwapChainImageFormat = surfaceFormat.format;
		m_SwapChainExtent = swapExtent;
	}

	void CreateImageViews()
	{
		//An image view is literally a view into image and is required to use any vkImage. It describes how to access the image and which part of the image to access.
		//For example, if it should be treated as a 2D texture depth texture without any mipmapping levels. 
		//An image view is sufficient to start using an image as a texture, but its not quite ready to be used as a render target just yet. We need just one more step of indirection, known as a framebuffer.
		m_SwapChainImageViews.resize(m_SwapChainImages.size());

		for (size_t i = 0; i < m_SwapChainImages.size(); i++)
		{
			VkImageViewCreateInfo creationInfo{};
			creationInfo.sType = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO;
			creationInfo.image = m_SwapChainImages[i];
			//The viewType and format fields specifies how the image data should be intepreted. The viewType parameter allows you to treat images as 1D textures, 2D textures, 3D textures and cubemaps.
			creationInfo.viewType = VK_IMAGE_VIEW_TYPE_2D;
			creationInfo.format = m_SwapChainImageFormat;
			//The components field allow you to swizzle the color channels around. For example, you can map all of the channels to the red channel for a monochrome texture.
			//You can also map constant values of 0 and 1 to a channel. In this case, we will stick to default mapping.
			creationInfo.components.r = VK_COMPONENT_SWIZZLE_IDENTITY;
			creationInfo.components.g = VK_COMPONENT_SWIZZLE_IDENTITY;
			creationInfo.components.b = VK_COMPONENT_SWIZZLE_IDENTITY;
			creationInfo.components.a = VK_COMPONENT_SWIZZLE_IDENTITY;
			//The subresourceRange fields describes what the image's purpose is and which part of the image should be accessed. Our images will be used as color targets
			//without any mipmapping levels or multiple layers.
			creationInfo.subresourceRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
			creationInfo.subresourceRange.baseMipLevel = 0;
			creationInfo.subresourceRange.levelCount = 1;
			creationInfo.subresourceRange.baseArrayLayer = 0;
			//If this was a stereographic 3D application, we would create a swapchain with multiple layers. We can then create multiple image views for each image representing the views for the
			//left and right eyes by accessing different layers. 
			creationInfo.subresourceRange.layerCount = 1; 

			if (vkCreateImageView(m_Device, &creationInfo, nullptr, &m_SwapChainImageViews[i]) != VK_SUCCESS)
			{
				throw std::runtime_error("Failed to create image views.");
			}
			else
			{
				std::cout << "Successfully create image view." << "\n";
			}
		}
	}

	static std::vector<char> ReadFile(const std::string& fileName)
	{
		//The ate flag will start reading at the end of the file, and binary indicates that we are reading the file as a binary file to avoid text transformations.
		//The advantage of starting to read at the end of the file is that we can use the read position to determine the size of the file and allocate a buffer.
		std::ifstream file(fileName, std::ios::ate | std::ios::binary); 

		if (!file.is_open())
		{
			throw std::runtime_error("Failed to open file " + fileName);
		}

		size_t fileSize = (size_t)file.tellg(); //Retrieves the position of the current character in the input stream. Since its at the end, hence the size of the buffer.
		std::vector<char> buffer(fileSize);

		file.seekg(0);
		file.read(buffer.data(), fileSize);

		file.close();

		//Make sure the shaders are loaded correctly by printing the size of the buffers and checking if they match the actual file size in bytes. 
		//Note that the code doesn't need to be null-terminated since its binary code and will later be explicit about its size.
		std::cout << buffer.size() << "\n";
		return buffer;
	}

	VkShaderModule CreateShaderModule(const std::vector<char>& code) 
	{
		//Before we can pass our shader code to the pipeline, we have to wrap it in a VkShaderModule object. This function will take a buffer with the bytecode as parameter and create a VkShaderModule from it. 
		VkShaderModuleCreateInfo createInfo{};
		createInfo.sType = VK_STRUCTURE_TYPE_SHADER_MODULE_CREATE_INFO;
		createInfo.codeSize = code.size();
		/*
			The catch here is that the size of the bytecode is specified in bytes, but the bytecode pointer is a uint32_t pointer rather than a char pointer.
			Therefore, we will need to cast the pointer with a reintepret cast as shown below. When you a perform a cast like this, you also need to ensure that the data
			satisfies the alignment requirements of uint32_t. Lucky for us, the data is stored in an std::vector where the default allocator already ensures that the data satisfies
			the worse case alignment requirements.
		*/
		//reinterpret_cast is used to convert one pointer to another pointer of any type, no matter whether the class is related to each other or not.
		//It does not check if the pointer type and data pointed by the pointer is the same or not.
		createInfo.pCode = reinterpret_cast<const uint32_t*>(code.data());

		VkShaderModule shaderModule;
		if (vkCreateShaderModule(m_Device, &createInfo, nullptr, &shaderModule) != VK_SUCCESS) {
			throw std::runtime_error("failed to create shader module!");
		}
		else
		{
			std::cout << "Successfully created Shader Module." << "\n";
		}

		return shaderModule;
	}

	void CreateRenderPass()
	{
		//We need to tell Vulkan about the framebuffer attachments that will be used while rendering. We need to specify how many color and depth buffers there will be,
		//how many samples to use for each of them and how their contents should be handled throughout the rendering operations. All of this is information is wrapped in a 
		//render pass object, for while we'll create a new CreateRenderPass function. Call this function from InitializedVulkan before CreateGraphicsPipeline.
		//In our case, we will only need to have a single color buffer attachment represented by one of the images from the swapchain.
		VkAttachmentDescription colorAttachmentInfo{};
		colorAttachmentInfo.format = m_SwapChainImageFormat; //The format of the color attachment should match the format of the swapchain images, and we're doing anything with multisampling yet, so we will stick with 1 sample.
		colorAttachmentInfo.samples = VK_SAMPLE_COUNT_1_BIT;
		/*
			The loadOp and storeOp determine what to do with the data in the attachment before rendering and after rendering. We have the following choices for loadOp:
			- VK_ATTACHMENT_LOAD_OP_LOAD: Preserve the existing contents of the attachment.
			- VK_ATTACHMENT_LOAD_OP_CLEAR: Clear the values to a constant at the start.
			- VK_ATTACHMENT_LOAD_OP_DONT_CARE: Existing contents are undefined; we don't care about them.
			In our case, we're going to use the clear operation to clear the framebuffer to black before drawing a new frame.  There are only two possibilities for storeOp:
		
			- VK_ATTACHMENT_STORE_OP_STORE: Rendered contents will be stored in memory and can be read later.
			- VK_ATTACHMENT_STORE_OP_DONT_CARE: Contents of the framebuffer will be undefined after the rendering operation.
			As we're only interested in seeing the rendered triangle on the screen, we're going with the store operation.
		*/
		colorAttachmentInfo.loadOp = VK_ATTACHMENT_LOAD_OP_CLEAR;
		colorAttachmentInfo.storeOp = VK_ATTACHMENT_STORE_OP_STORE;
		//The loadOp and storeOp apply to color and depth data, and stencilLoadOp/stencilStoreOp apply to stencil data. For now, we won't be doing anything with the stencil buffer, so they are irrelevant for now.
		colorAttachmentInfo.stencilLoadOp = VK_ATTACHMENT_LOAD_OP_DONT_CARE;
		colorAttachmentInfo.stencilStoreOp = VK_ATTACHMENT_STORE_OP_DONT_CARE;
		
		/*
			Textures and framebuffers in Vulkan are represented by VkImage objects with a certain pixel format, however the layout of the pixels in memory can change based
			on what you're trying to do with an image. The most important thing to know right now is that images need to be transitioned to certain layouts that are suitable for
			the operation they're going to be involved in next. Some of the most common layouts are:
			- VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL: Images used as color attachment.
			- VK_IMAGE_LAYOUT_PRESENT_SRC_KHR: Images to be presented in the swapchain.
			- VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL: Images to be used as destination for a memory copy operation.
			The initialLayout specifies which layout the image will have before the render pass begins. THe finalLayout specifies the layout to automatically transition to
			when the render pass finishes. Using VK_IMAGE_LAYOUT_UNDEFINED for initialLayout means that we don't care what previous layout the image was in. The caveat of this
			special value is that the contents of the image are not guarenteed to be preserved, but that doesn't matter since we're going to clear it anyway. We want the image
			to be ready for presentation using the swapchain after rendering, which is why we use the VK_IMAGE_LAYOUT_PRESENT_SRC_KHR as finalLayout. 
		*/
		colorAttachmentInfo.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;
		colorAttachmentInfo.finalLayout = VK_IMAGE_LAYOUT_PRESENT_SRC_KHR;

		/*
			A single renderpass can consist of multiple subpasses. Subpasses are subsequent rendering operations that depend on the contents of framebuffers in previous
			passes, for example a sequence of post-processing effects that are applied one after another. If you group these rendering operations into one render pass, then
			Vulkan is able to reorder the operations and conserve memory bandwidth for possibly better performance. For our very first triangle, however, we will stick to a 
			single subpass. Every subpass references one or more of the attachments that we've described using the structure in the previous sections. These references are
			themselves VkAttachmentReference structs that look like this:
		*/
		VkAttachmentReference colorAttachmentReference{};
		colorAttachmentReference.attachment = 0; //This parameter specifies which attachment to reference by its index in the attachment descriptions array. Our array consists of a single VkAttachmentDescription, so its index is 0.
		//The layout parameter specifies which layout we would like the attachment to have during a subpass that uses this reference. Vulkan will automatically transition the
		//attachment to the layout when the subpass is started. We intend to use the attachment to function as a color buffer and the VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL layout will give us the best performance as the name implies. 
		colorAttachmentReference.layout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL; 

		//The subpass is described using a VkSubpassDescriptionStructure.
		VkSubpassDescription subpassInfo{};
		subpassInfo.pipelineBindPoint = VK_PIPELINE_BIND_POINT_GRAPHICS; //As compute subpasses may become avaliable, we have to be explict about this being a graphics subpass.
		//The index of the attachment in this array is directly referenced from the fragment shader with the "layout(location = 0) out vec4 outColor" directive. The
		//following other types of attachments can be refered by a subpass:
		//- pInputAttachments: Attachments that are read from a shader.
		//- pResolveAttachments: Attachments used for multisampling color attachments.
		//- pDepthStencilAttachment: Attachment for depth and stencil data.
		//- pPreserveAttachments: Attachments that are not used by this subpass, but for which the data must be preserved. 
		subpassInfo.colorAttachmentCount = 1;
		subpassInfo.pColorAttachments = &colorAttachmentReference;

		//Now that the attachment and a basic subpass referencing it have been described, we can create the render pass itself. The render pass object can be created
		//by filling in the VkRenderPassCreateInfo structure with an array of attachments and subpasses. The VkAttachmentReference objects reference attachments using the indices of this array.
		VkRenderPassCreateInfo renderPassInfo{};
		renderPassInfo.sType = VK_STRUCTURE_TYPE_RENDER_PASS_CREATE_INFO;
		renderPassInfo.attachmentCount = 1;
		renderPassInfo.pAttachments = &colorAttachmentInfo;
		renderPassInfo.subpassCount = 1;
		renderPassInfo.pSubpasses = &subpassInfo;

		if (vkCreateRenderPass(m_Device, &renderPassInfo, nullptr, &m_RenderPass) != VK_SUCCESS)
		{
			throw std::runtime_error("Failed to create render pass.");
		}
		else
		{
			std::cout << "Successfully created Render Pass." << "\n";
		}

		/*
			Remember that the subpasses in a render pass automatically take care of image layout transitions. These transitions are controlled by subpass dependencies,
			which specify memory and execution dependencies between subpasses. We have only a single subpass right now, but the operations right before and right after
			this subpass also count as implicit "subpasses". There are two built-in dependencies that take care of the transition at the start of the render pass and
			at the end of the render pass, but the former does not occur at the right time. It assumes that the transition occurs at the start of the pipeline,
			but we haven't acquired the image yet at that point. There are two ways to deal with this problem.
			We could change the waitStages for the m_ImageAvaliableSemaphore to VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT to ensure that the render passes don't begin until the
			image is avaliable, or we can make the render pass wait for the VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT stage.
			Lets go wih the second option here as it is a good excuse to have a look at subpass dependencies and how they work.

			Subpass dependencies are specified in VkSubpassDependency structs.
		*/

		//The first two fields specify the indices of the dependency and the dependent subpass. The special value VK_SUBPASS_EXTERNAL refers to the implicit subpass before
		//or after the render pass on n

		VkSubpassDependency dependency{};
		dependency.srcSubpass = VK_SUBPASS_EXTERNAL;
		dependency.dstSubpass = 0;
		dependency.srcStageMask = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT;
		dependency.srcAccessMask = 0;
		dependency.dstStageMask = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT;
		dependency.dstAccessMask = VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT;
		renderPassInfo.dependencyCount = 1;
		renderPassInfo.pDependencies = &dependency;
	}

	void CreateGraphicsPipeline()
	{
		std::vector<char> vertexShaderCode = ReadFile("Shaders/vertex.spv");
		std::vector<char> fragmentShaderCode = ReadFile("Shaders/fragment.spv");

		//Shader modules are just a thin wrapper around the shader bytecode that we previously loaded from a file and the functions defined in it. The compilation and
		//linking of the SPIR-V bytecode to machine code for execution by the GPU doesn't happen until the graphics pipeline is created. That means that we are allowed
		//to destroy the shader modules again as soon as the pipeline creation is finished, which is why we will make them local variables in the CreateGraphicsPipeline function instead of class members. 
		VkShaderModule vertexShaderModule = CreateShaderModule(vertexShaderCode);
		VkShaderModule fragmentShaderModule = CreateShaderModule(fragmentShaderCode);

		VkPipelineShaderStageCreateInfo vertexShaderStageInfo{};
		//The first step besides the obligatory stype member is telling Vulkan in which pipeline stage the shader is going to be used. There is an enum value for each of the programmable shader stages.
		vertexShaderStageInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO;
		vertexShaderStageInfo.stage = VK_SHADER_STAGE_VERTEX_BIT;

		//The next two members specify the shader module containing the code, and the function to invoke known as the entry point. 
		//That means that its possible to combine multiple fragment shaders into a single shader module, and use different entry points to differentiate between
		//their behaviors. In this case, we will stick to the standard "main", however. There is one more optional member, pSpecializationInfo, which we won't be using here.
		//It allows you to specify values for shader constants. You can use a single shader module where its behavior can be configured at pipeline creation by specifying
		//different values for the constants used in it. This is more efficient than confugirng the shader using variables at render time, because the compiler can do
		//optimizations like eliminating if statements that depend on these values. If you do not have any constants like this, then you can set the member to nullptr, which our struct initialization does automatically.
		vertexShaderStageInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO;
		vertexShaderStageInfo.module = vertexShaderModule;
		vertexShaderStageInfo.pName = "main";
		
		//Modifying the structure to suit the fragment shader is easy.
		VkPipelineShaderStageCreateInfo fragmentShaderStageInfo{};
		fragmentShaderStageInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO;
		fragmentShaderStageInfo.stage = VK_SHADER_STAGE_FRAGMENT_BIT;
		fragmentShaderStageInfo.module = fragmentShaderModule;
		fragmentShaderStageInfo.pName = "main";

		VkPipelineShaderStageCreateInfo shaderStages[] = { vertexShaderStageInfo, fragmentShaderStageInfo };

		/*
			The older graphics APIs provided default state for most of the stages of the graphics pipeline. In Vulkan, you will have to be explicit about everything,
			from viewport size to color blending functions. We will thus have to create each of these ourselves.
			The VkPipelineVertexInputStateCreateInfo structure describes the format of the vertex data that will be passed to the vertex shader. It describes this in roughly two ways:
				- Bindings: Spacing between data and whether the data is per-vertex or per-instance (Geometry Instancing, where multiple copies of the same mesh are rendered at once in a scene).
				- Attribute Descriptions: Type of the attributes passed to the vertex shader, which binding to load them from and at which offset.
		*/

		VkVertexInputBindingDescription bindingDescription = Vertex::RetrieveBindingDescription();
		auto attributeDescription = Vertex::RetrieveAttributeDescriptions();

		VkPipelineVertexInputStateCreateInfo vertexInputInfo{};
		vertexInputInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_VERTEX_INPUT_STATE_CREATE_INFO;
		vertexInputInfo.vertexBindingDescriptionCount = 1;
		vertexInputInfo.pVertexBindingDescriptions = &bindingDescription; //This points to an array of structs that describe the aforementioned details for loading vertex data.
		
		vertexInputInfo.vertexAttributeDescriptionCount = static_cast<uint32_t>(attributeDescription.size());
		vertexInputInfo.pVertexAttributeDescriptions = attributeDescription.data(); //Optional. This points to an array of structs that describe the aforementioned details for loading vertex data.

		/*
			The VkPipelineInputAssemblyStateCreateInfo struct describes two things: what kind of geometry will be drawn from the vertices and if primitive restart should be enabled.
			The former is specified in to the "topology" member and can have values like:
			- VK_PRIMITIVE_TOPOLOGY_POINT_LIST: Points from vertices.
			- VK_PRIMITIVE_TOPOLOGY_LINE_LIST: Line from every 2 vertices without reuse.
			- VK_PRIMITIVE_TOPOLOGY_LINE_STRIP: The end vertex of every line is used as start vertex for the next line.
			- VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST: Triangle from every 3 vertices without reuse.
			- VK_PRIMITIVE_TOPOLOGY_TRIANGLE_STRIP: The second and third vertex of every triangle are used as first two vertices of the next triangle.

			Normally, the vertices are loaded from the vertex buffer by index in sequential order, but with an element buffer, you can specify the indices to use yourself.
			This allows you to perform optimizations like reusing vertices. If you set the primitiveRestartEnable to VK_TRUE, then its possible to break up lines and triangles in the
			_STRIP topology modes by using a special index of 0xFFFF or 0xFFFFFFFF. 
		*/

		//As we intend to draw triangles throughout this tutorial, we will stick to the following data for the structure.
		VkPipelineInputAssemblyStateCreateInfo inputAssembly{};
		inputAssembly.sType = VK_STRUCTURE_TYPE_PIPELINE_INPUT_ASSEMBLY_STATE_CREATE_INFO;
		inputAssembly.topology = VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST;
		inputAssembly.primitiveRestartEnable = VK_FALSE;

		//A viewport basically describes the region of the framebuffer than the output will be rendered to. This will almost always be (0, 0) to (wdith, height).
		//Remember that the size of the swapchain and its images may differ from the WIDTH/HEIGHT of the window. The swapchain images will be used as framebuffers later on,
		//so we should stick to their size. 

		VkViewport viewportInfo{};
		viewportInfo.x = 0.0f;
		viewportInfo.y = 0.0f;
		viewportInfo.width = (float)m_SwapChainExtent.width;
		viewportInfo.height = (float)m_SwapChainExtent.height;
		viewportInfo.minDepth = 0.0f; //The minDepthand maxDepth values specify the range of depth values to use for the framebuffer.
		viewportInfo.maxDepth = 1.0f; //These values must be within the [0.0f, 1.0f] range, but minDepth may be higer than maxDepth. If you aren't doing anything special, then you should stick to the standard values of 0.0f and 1.0f.

		//While viewports define the transformation from the image to the framebuffer, scissor rectangles define in which regions pixels will actually be stored.
		//Any pixels outside the scissor rectangles will be discarded by the rasterizer  They function like a filter rather than a transformation. 
		//In this tutorial, we simply want to draw to the entire framebuffer, so we will specify a scissor rectangle that covers it entirely.
		VkRect2D scissorInfo{};
		scissorInfo.offset = { 0, 0 };
		scissorInfo.extent = m_SwapChainExtent;

		//Now, this viewport and scissor rectangle need to be combined into a viewport state using the VkPipelineViewportStateCreateInfo struct. 
		//It is possible to use multiple viewports and scissor rectangles on some graphics cards, so its members reference an array of them. Using multiple requires enabling a GPU feature.
		VkPipelineViewportStateCreateInfo viewportState{};
		viewportState.sType = VK_STRUCTURE_TYPE_PIPELINE_VIEWPORT_STATE_CREATE_INFO;
		viewportState.viewportCount = 1;
		viewportState.pViewports = &viewportInfo;
		viewportState.scissorCount = 1;
		viewportState.pScissors = &scissorInfo;
		 
		//The rasterizer takes the geometry that is shaped by the vertices from the vertex shader and turns it into fragments to be colored by the fragment shader. 
		//It also performs depth testing, face culling and the scissor test, and it can be configured to output fragments that fill entire polygons or just the edges (wireframe rendering).
		//All this is configuring using the VkPipelineRasterizationStateCreateInfo structure. 
		VkPipelineRasterizationStateCreateInfo rasterizerInfo{};
		rasterizerInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_RASTERIZATION_STATE_CREATE_INFO;
		rasterizerInfo.depthClampEnable = VK_FALSE; //If this is true, then fragments that are beyond the near and far planes are clamped to them as opposed to discarding them.  This is useful in special cases like shadow maps. This requires a GPU feature as well.
		//The polygonMode determines how fragments are generated for geometry. The following modes are avaliable and using any other mode other than fill requires enabling a GPU feature.
		//- VK_POLYGON_MODE_FILL: Fills the area of the polygon with fragments.
		//- VK_POLYGON_MODE_LINE: Polygon edges are drawn as lines.
		//- VK_POLYGON_MODE_POINT: Polygon vertices are drawn as points.
		rasterizerInfo.polygonMode = VK_POLYGON_MODE_FILL; 
		//The lineWidth member is straightforward. It describes the thickness of lines in terms of number of fragments. The maximum line width that is supported depends on the hardware
		//and any line thicker than 1.0f requires you to enable the wideLines GPU feature.
		rasterizerInfo.lineWidth = 1.0f;
		//The cullMode variable determines the type of face culling to use. You can disable culling, cull the front faces, cull the back faces or both. The frontFace variable describes the
		//vertex order for faces to be considered front-facing and can be clockwise or counter-clockwise.
		rasterizerInfo.cullMode = VK_CULL_MODE_BACK_BIT;
		rasterizerInfo.frontFace = VK_FRONT_FACE_CLOCKWISE;
		//The rasterizer can alter the depth values by adding a constant value or biasing them based on a fragment's slope. This is sometimes used for shadow mapping, but we won't be using it.
		rasterizerInfo.depthBiasEnable = VK_FALSE;
		rasterizerInfo.depthBiasConstantFactor = 0.0f; // Optional
		rasterizerInfo.depthBiasClamp = 0.0f; // Optional
		rasterizerInfo.depthBiasSlopeFactor = 0.0f; // Optional

		//The VkPipelineMultisampleStateCreateInfo struct configures multisampling, which is one of the ways to perform anti-aliasing. It works by combining the fragment
		//shader results of multiple polygons that rasterize to the same pixel. This mainly occurs along edges, which is also where the most noticeable aliasing artifacts occur.
		//Because it doesn't need to run the fragment shader multiple times if only one polygon maps to a pixel, it is signicantly less expensive than simply rendering to a higher resolution and then downscaling.
		//Enabling it requires enabling a GPU feature. 
		VkPipelineMultisampleStateCreateInfo multisampleInfo{};
		multisampleInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_MULTISAMPLE_STATE_CREATE_INFO;
		multisampleInfo.sampleShadingEnable = VK_FALSE;
		multisampleInfo.rasterizationSamples = VK_SAMPLE_COUNT_1_BIT;
		multisampleInfo.minSampleShading = 1.0f; //Optional;
		multisampleInfo.pSampleMask = nullptr; //Optional
		multisampleInfo.alphaToCoverageEnable = VK_FALSE; //Optional
		multisampleInfo.alphaToOneEnable = VK_FALSE; //Optional

		//If you are using a depth/stencil buffer, then you also need to configure the depth and stencil tests using VkPipelineDepthStencilStateCreateInfo. As we don't have
		//one right now, so we can simply pass a nullptr instead of a pointer to such a struct. 

		/*
			After a fragment shader has returned a color, its needs to be combined with the color that is already in the framebuffer. This transformation is known as color
			blending and there are two ways to do it:
			- Mix the old and new value to produce a final colkor.
			- Combine the old and new value using a bitwise operation.
			There are two types of structs to configure color blending. The first struct, VkPipelineColorBlendAttachmentState contains the configuration per attached framebuffer
			and the second struct VkPipelineColorBlendStateCreateInfo contains the global color blending settings. In our case, we only have 1 framebuffer. 
		*/
		VkPipelineColorBlendAttachmentState colorBlendAttachmentInfo{};
		colorBlendAttachmentInfo.colorWriteMask = VK_COLOR_COMPONENT_R_BIT | VK_COLOR_COMPONENT_G_BIT | VK_COLOR_COMPONENT_B_BIT | VK_COLOR_COMPONENT_A_BIT;
		colorBlendAttachmentInfo.blendEnable = VK_FALSE;
		colorBlendAttachmentInfo.srcColorBlendFactor = VK_BLEND_FACTOR_ONE; //Optional
		colorBlendAttachmentInfo.dstColorBlendFactor = VK_BLEND_FACTOR_ZERO; //Optional
		colorBlendAttachmentInfo.colorBlendOp = VK_BLEND_OP_ADD; //Optional
		colorBlendAttachmentInfo.srcAlphaBlendFactor = VK_BLEND_FACTOR_ONE; //Optional
		colorBlendAttachmentInfo.dstAlphaBlendFactor = VK_BLEND_FACTOR_ZERO; //Optional
		colorBlendAttachmentInfo.alphaBlendOp = VK_BLEND_OP_ADD; //Optional

		//The second struct references an array of structures for all of the framebuffers and allows you to set blend constants that you can use as blend factors in the aforementioned calculations.
		VkPipelineColorBlendStateCreateInfo colorBlending{};
		colorBlending.sType = VK_STRUCTURE_TYPE_PIPELINE_COLOR_BLEND_STATE_CREATE_INFO;
		//Set this to true if you wish to use the second method of blending (bitwise combination). This will automatically disable the first method, as if you have set blendEnable to VK_FALSE for every attached framebuffer.
		//Of course, it is possible to disable both modes as we've done here, in which case the fragment colors will be written tot he framebuffer unmodified.
		colorBlending.logicOpEnable = VK_FALSE; 
		colorBlending.logicOp = VK_LOGIC_OP_COPY; //This is where the bitwise operation can be specified. 
		colorBlending.attachmentCount = 1;
		colorBlending.pAttachments = &colorBlendAttachmentInfo;
		colorBlending.blendConstants[0] = 0.0f; //Optional
		colorBlending.blendConstants[1] = 0.0f;
		colorBlending.blendConstants[2] = 0.0f;
		colorBlending.blendConstants[3] = 0.0f;

		/*
			You can use uniform values in shaders, which are globals similar to dynamic state variables that can be changed at drawing time to alter the behavior of your shaders
			without having to recreate them. They are commonly used to pass the transformation matrix to the vertex shader, or to create texture samplers in the fragment shader.
			These uniform values need to be specified during pipeline creation by creating a vkPipelineLayout object. Even though we won't be using them, we are still required
			to create an empty pipeline layout. 
		*/

		VkPipelineLayoutCreateInfo pipelineLayoutInfo{};
		pipelineLayoutInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO;
		pipelineLayoutInfo.setLayoutCount = 0; //Optional
		pipelineLayoutInfo.pSetLayouts = nullptr; //Optional
		pipelineLayoutInfo.pushConstantRangeCount = 0; //Optional
		pipelineLayoutInfo.pPushConstantRanges = nullptr; //Optional. Push Constants are another way of passing dynamic values to shaders. 

		if (vkCreatePipelineLayout(m_Device, &pipelineLayoutInfo, nullptr, &m_PipelineLayout))
		{
			throw std::runtime_error("Failed to create pipeline layout.");
		}

		//Finally, we can create our graphics pipeline.
		VkGraphicsPipelineCreateInfo pipelineCreationInfo{};
		pipelineCreationInfo.sType = VK_STRUCTURE_TYPE_GRAPHICS_PIPELINE_CREATE_INFO;
		//We start by referencing the array of VkPipelineShaderStageCreateInfo structs.
		pipelineCreationInfo.stageCount = 2; 
		pipelineCreationInfo.pStages = shaderStages;
		//Then, we reference all of the structures describing the fixed-function stage.
		pipelineCreationInfo.pVertexInputState = &vertexInputInfo;
		pipelineCreationInfo.pInputAssemblyState = &inputAssembly;
		pipelineCreationInfo.pViewportState = &viewportState;
		pipelineCreationInfo.pRasterizationState = &rasterizerInfo;
		pipelineCreationInfo.pMultisampleState = &multisampleInfo;
		pipelineCreationInfo.pDepthStencilState = nullptr; //Optional
		pipelineCreationInfo.pColorBlendState = &colorBlending;
		pipelineCreationInfo.pDynamicState = nullptr;
		//After that comes the pipeline layout, which is a Vulkan handle rather than a struct pointer.
		pipelineCreationInfo.layout = m_PipelineLayout;
		//And finally, we have a reference to the render pass and the index of the subpass where this graphics pipeline will be used. It is also possible to use other render passes
		//with this pipeline instead of this specific instance, but they have to be compatible with the renderPass parameter. The requirements for compatibility are described in the specification, but we won't be using that feature.
		pipelineCreationInfo.renderPass = m_RenderPass;
		pipelineCreationInfo.subpass = 0;
		/*
			There are two more parameters: basedPipelineHandle and basePipelineIndex. Vulkan allows you to create a new graphics pipeline by deriving from an existing pipeline.
			The idea of pipeline derivatives is that it is less expensive to set up pipelines when they have much functionality in common with an existing pipeline and switching
			between pipelines from the same parent can also be done much quicker. You can either specify the handle of an existing pipeline with basePipelineHandle or reference another
			pipeline that is about to be created by index with basePipelineIndex. Right now, there is only a single pipeline, so we will simply specify a null handle and an invalid index.
			These values are only used if the VK_PIPELINE_CREATE_DERIVATIVE_BIT flag is also specified in the flags field of VkGraphicsPipelineCreateInfo.
		*/

		//The vkCreateGraphicsPipelines function actually has more parameters than the usual object creation functions in Vulkan. It is designed to take multiple VkGraphicsPipelineCreateInfo objects and
		//and create multiple VkPipeline objects in a single call. The second parameter, in which we've passed VK_NULL_HANDLE as argument, references an optional VkPipelineCache object.
		//A pipeline cache can be used to store and reuse data relevant to pipeline creation across multiple calls to VkCreateGraphicsPipelines and even across program executions if the cache is stored to a file.
		//This makes it possible to significantly speed up pipeline creation at a later time. We will get into this eventually. 
		if (vkCreateGraphicsPipelines(m_Device, VK_NULL_HANDLE, 1, &pipelineCreationInfo, nullptr, &m_GraphicsPipeline) != VK_SUCCESS)
		{
			throw std::runtime_error("Failed to create graphics pipeline.");
		}
		else
		{
			std::cout << "Successfully created Graphics Pipeline.";
		}

		vkDestroyShaderModule(m_Device, fragmentShaderModule, nullptr);
		vkDestroyShaderModule(m_Device, vertexShaderModule, nullptr);
	}

	void CreateFramebuffers()
	{
		/*
			We've set up the render pass to expect a single framebuffer with the same format as the swap chain images, but we haven't actually created any yet.
			The attachments specified during render pass creation are bound by wrapping them into a VkFramebuffer object. A framebuffer object references all of the
			VkImageView objects that represent the attachments. In our case, there will only be a single one: the color attachment. However, the image that we have to use for
			the attachment depends on which image the swapchain returns when we retrieve one for presentation. That means that we have to create a framebuffer for all of the images
			in the swapchaim and use the one that corresponds to the retrieved image at drawing time.
		*/
		m_SwapChainFramebuffers.resize(m_SwapChainImageViews.size());
		//We will then iterate through the image views and create framebuffers from them.

		/*
			As seen, the creation of framebuffers is quite straigtforward. We first need to specify with which renderPass the framebuffer needs to be compatible. You only
			use a framebuffer with the render passes that it is compatible with, which roughly means that they use the same number and type of attachments. 
			The attachmentCount and pAttachments parameters specify the VkImageView objects that should be bound to the respective attachment descriptions in the render pass pAttachment array.
			The width and height parameters are self explainatory and layers refer to the number of layers in image arrays. Our swap chain images are single images, so the number of layers is 1. 
		*/
		for (size_t i = 0; i < m_SwapChainImageViews.size(); i++)
		{
			VkImageView attachments[] = { m_SwapChainImageViews[i] };
			VkFramebufferCreateInfo framebufferInfo{};
			framebufferInfo.sType = VK_STRUCTURE_TYPE_FRAMEBUFFER_CREATE_INFO;
			framebufferInfo.renderPass = m_RenderPass;
			framebufferInfo.attachmentCount = 1;
			framebufferInfo.pAttachments = attachments;
			framebufferInfo.width = m_SwapChainExtent.width;
			framebufferInfo.height = m_SwapChainExtent.height;
			framebufferInfo.layers = 1;

			if (vkCreateFramebuffer(m_Device, &framebufferInfo, nullptr, &m_SwapChainFramebuffers[i]) != VK_SUCCESS)
			{
				throw std::runtime_error("Failed to create framebuffer.");
			}
			else
			{
				std::cout << "Successfully created Framebuffer." << "\n";
			}
		}
	}
		
	void CreateCommandPool()
	{
		/*
			Commands in Vulkan, like drawing operations and memory transfers are not executed directly using function calls. You have to record all of the operations
			you want to perform in command buffer objects. The advantage of this is that all of the hard work of setting up drawing commands can be done in advance and in multiple
			threads. After that, you just have to tell Vulkan to execute the commands in the main loop.

			We have to create a command pool before we can create command buffers. Command pools manage the memory that is used to store the buffers and command buffers are allocated from them.
			Command buffers are executed by submitting them on one of the device queues, like the graphics and presentation queues we've retrieved. Each command pool can only
			allocate command buffers that are submitted on a single type of queue. We're going to record commands for drawing, which is why we've chosen the graphics queue family.
			There are two possible flags for command pools:
			- VK_COMMAND_POOL_CREATE_TRANSIENT_BIT: Hint that command buffers are rerecorded with new commands very often (may change memory allocated behavior).
			- VK_COMMAND_POOL_CREATE_RESET_COMMAND_BUFFER_BIT: Allow command buffers to be rerecorded individually, without this flag they all have to be reset together.
			We will only record the command buffers at the beginning of the program and then execute them many times in the main loop, so we don't have to use any of these flags.
		*/
		QueueFamilyIndices queueFamilyIndices = FindQueueFamilies(m_PhysicalDevice);
		VkCommandPoolCreateInfo poolInfo{};
		poolInfo.sType = VK_STRUCTURE_TYPE_COMMAND_POOL_CREATE_INFO;
		poolInfo.queueFamilyIndex = queueFamilyIndices.m_GraphicsFamily.value();
		poolInfo.flags = 0;

		if (vkCreateCommandPool(m_Device, &poolInfo, nullptr, &m_CommandPool) != VK_SUCCESS)
		{
			throw std::runtime_error("Failed to create command pool.");
		}
		else
		{
			std::cout << "Successfully created Command Pool." << "\n";
		}
	}

	uint32_t FindMemoryType(uint32_t typeFilter, VkMemoryPropertyFlags properties)
	{
		/*
			Graphics cards can offer different types of memory to allocate from. Each type of memory varies in terms of allowed operations and performance characteristics.
			We need to combine the requirements of the buffer and our own application requirements to find the right type of memory to use. 
		*/
		VkPhysicalDeviceMemoryProperties memoryProperties;
		/*
			The VkPhysicalDeviceMemoryProperties structure has 2 arrays memoryTypes and memoryHeaps. Memory heaps are distinct memory resources like dedicated VRAM and swap space
			in RAM for when VRAM runs out. The different types of memory exist within these heaps. Right now, we will only concern ourselves with the type of memory and not the heap
			it comes from, but you can imagine that this can affect performance. 
		*/
		vkGetPhysicalDeviceMemoryProperties(m_PhysicalDevice, &memoryProperties);
		for (uint32_t i = 0; i < memoryProperties.memoryTypeCount; i++)
		{
			/*
				The typeFilter parameter will be used to specify the bit field of memory types that are suitable. That means that we can find the index of a suitable memory
				type by simply iterating over them and checking if the corresponding bit is set to 1. However, we're not just interested in a memory type that is suitable for
				the vertex buffer. We also need to be able to write our vertex data to that memory. The memoryTypes array consists of VkMemoryType structs that specify the
				heap and properties of each type of memory. The properties define special features of the memory, like being able to map it so we can write to it from the CPU. 
				This property is indicated with VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT, but we also need to use the VK_MEMORY_PROPERTY_HOST_COHERENT_BIT property. We will see why when we map the memory.
			
				We may have more than one desirable property, so we should check if the result of the bitwise AND is not just non-zero, but equal to the desired properties bit field.
				If there is a memory type suitable for the buffer that also has all of the properties we need, then we return its index. Otherwise, we throw an exception.
			*/

			if ((typeFilter & (1 << i)) && (memoryProperties.memoryTypes[i].propertyFlags & properties) == properties)
			{
				return i;
			}
		}

		throw std::runtime_error("Failed to find a suitable memory type.");
	}

	void CreateBuffer(VkDeviceSize size, VkBufferUsageFlags usage, VkMemoryPropertyFlags properties, VkBuffer& buffer, VkDeviceMemory& bufferMemory)
	{
		/*
			Buffers in Vulkan are regions of memory for storing arbitrary data that can be read by the graphics card. They can be used to store vertex data, but also many other purposes.
			Unlike the Vulkan object that we have been dealing with so far, buffers do not automatically allocate memory for themselves. Like many things in Vulkan, the API puts the
			programmer in control of almost everything and memory management is one of those things.
		*/

		VkBufferCreateInfo bufferInfo{};
		bufferInfo.sType = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO;
		bufferInfo.size = size; //Size of the buffer in bytes. This is straightforward with sizeof().
		bufferInfo.usage = usage; //Indicates for which purposes the data in the buffer is going to be used. It is possible to specify multiple purposes using a bitwise or operator.
		//Just like images in the swapchain, buffers can be owned by a specific queue family or be shared between multiple at the same time. As the buffer will only be used from the graphics queue, we can stick with exclusive access.
		bufferInfo.sharingMode = VK_SHARING_MODE_EXCLUSIVE; 
		//There is a flags parameter that is used to configure sparse buffer memory, which is not relevant at the moment. We will leave it at a default value of 0.

		if (vkCreateBuffer(m_Device, &bufferInfo, nullptr, &buffer) != VK_SUCCESS)
		{
			throw std::runtime_error("Failed to create buffer.");
		}
		else
		{
			std::cout << "Successfully created Vertex Buffer." << "\n";
		}

		//While the buffer has been created, it doesn't actually have any memory assigned to it yet. The first step of allocating memory for the buffer is to query its memory
		//requirements using the aptly named vkGetBufferMemoryRequirements function. 
		VkMemoryRequirements memoryRequirements;
		vkGetBufferMemoryRequirements(m_Device, buffer, &memoryRequirements);
		/*
			The VkMemoryRequirements struct has 3 fields:
			- size: The size of the required amount of memory in bytes, may differ from bufferInfo.size().
			- alignment: The offset in bytes where the buffer begins in the allocated region of memory. This depends on bufferInfo.usage and bufferInfo.flags.
			- memoryTypeBits: Bit field of the memory types that are suitable for the buffer.
		*/

		/*
			Memory allocation is now as simply as specifying the size and type, both which are derived from the memory requirements of the vertex buffer and the desired property.
			It should be noted that in a real world application, you're not supposed to actually call vkAllocateMemory for every individual buffer. The maximum number of
			simultaneous memory allocations is limited by the maxMemoryAllocationCount physical device limit, which may be as low as 4096 even on high end hardware like an
			NVIDEA GTX 1080. The right way to allocate memory for a large number of objects at the same time is to create a custom allocator that splits up a single allocation
			among many different objects using the offset parameters that we have seen in many functions. We can either implement such an allocator ourselves, or use the 
			VulkanMemoryAllocator library. However, its okay for us to use a seperate allocation for every resource as we won't come close to hitting any of those limits for now.
		*/
		VkMemoryAllocateInfo allocateInfo{};
		allocateInfo.sType = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO;
		allocateInfo.allocationSize = memoryRequirements.size;
		allocateInfo.memoryTypeIndex = FindMemoryType(memoryRequirements.memoryTypeBits, properties);

		if (vkAllocateMemory(m_Device, &allocateInfo, nullptr, &bufferMemory) != VK_SUCCESS)
		{
			throw std::runtime_error("Failed to allocate vertex buffer memory.");
		}

		//Once successful, we can associate this memory with the buffer directly.
		//The first three parameters are straightforward and the fourth parameter is the offset within the region of memory. Since this memory is allocated
		//specifically for this: the vertex buffer, the offset is simply 0. If the offset is non-zero, then it is required to be divisible by memoryRequirements.alignment.
		vkBindBufferMemory(m_Device, buffer, bufferMemory, 0);
	}

	void CopyBuffer(VkBuffer sourceBuffer, VkBuffer destinationBuffer, VkDeviceSize size)
	{
		/*
			Memory transfer operations are executed using command buffers, just like drawing commands. Therefore, we must allocate a temporary command buffer. You may
			wish to create a seperate command pool for these kinds of short-lived buffers, because the implementation may be able to apply memory allocation optimizations.
			You should use the VK_COMMAND_POOL_CREATE_TRANSIENT_BIT flag during command pool generation in that case. This indicates that the command buffers allocated from said pool will have short lived (reset/freed in a short timeframe).
		*/
		VkCommandBufferAllocateInfo allocationInfo{};
		allocationInfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO;
		allocationInfo.level = VK_COMMAND_BUFFER_LEVEL_PRIMARY;
		allocationInfo.commandPool = m_CommandPool;
		allocationInfo.commandBufferCount = 1;
		
		VkCommandBuffer commandBuffer;
		vkAllocateCommandBuffers(m_Device, &allocationInfo, &commandBuffer);
		
		//We're only going to use the command buffer once and wait with returning from the function until the copy operation has finished executing. Thus, its good practice
		//to tell the driver about our intent using VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT.
		VkCommandBufferBeginInfo beginInfo{};
		beginInfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO;
		beginInfo.flags = VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT;

		vkBeginCommandBuffer(commandBuffer, &beginInfo);

		//Content of buffers are transferred using the vkCmdCopyBuffer command. It takes the source and destination buffers as arguments, and an array of regions to copy.
		//The regions are defined in VkBufferCopy structs and consist of a source buffer offset, destination buffer offset and size. It is not possible to specify VK_WHOLE_SIZE
		//here, unlike the vkMapMemory command.
		VkBufferCopy copyRegion{};
		copyRegion.srcOffset = 0; //Optional
		copyRegion.dstOffset = 0;
		copyRegion.size = size;
		vkCmdCopyBuffer(commandBuffer, sourceBuffer, destinationBuffer, 1, &copyRegion);
		//As the command buffer only contains the copy command, we can stop recording after that.
		vkEndCommandBuffer(commandBuffer);
		//Now, execute thhe command buffer to complete the transfer.

		VkSubmitInfo submitInfo{};
		submitInfo.sType = VK_STRUCTURE_TYPE_SUBMIT_INFO;
		submitInfo.commandBufferCount = 1;
		submitInfo.pCommandBuffers = &commandBuffer;

		/*
			Unlike the draw commands, there are no events we need to wait on this time. We just want to execute the transfer on the buffers immediately. There are again two
			possible ways to wait on this transfer to complete. We could use a fence and wait with vkWaitForFences, or simply wait for the transfer queue to become idle with vkQueueWaitIdle.
			A fence would allow you to schedule multiple transfers simultaneously and wait for all of them to complete, instead of executing one at a time. That may give the driver more opportunities to optimize.
		*/
		vkQueueSubmit(m_GraphicsQueue, 1, &submitInfo, VK_NULL_HANDLE);
		vkQueueWaitIdle(m_GraphicsQueue);

		//Don't forget to clean up the command buffer used for the transfer operation.
		vkFreeCommandBuffers(m_Device, m_CommandPool, 1, &commandBuffer);
	}

	void CreateVertexBuffer()
	{
		/*
			While using the vertex buffer directly works just fine, the memory type that allows us to access it from the CPU may not be the most optimal memory type
			for the graphics carsd itself to read from. The most optimal memory has the VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT flag and is usually not accessible by the CPU
			on dedicated graphics cards. We will thus create 2 vertex buffers. One staging buffer in CPU accessible memory to upload the data from the vertex array to, and
			the final vertex buffer in device local memory. We will then use a buffer copy command to move the data from the staging buffer to the actual vertex buffer.
		
			We will now be using a stagingBuffer with stagingBufferMemory for mapping and copying the vertex data. We have two new buffer flags here to use:

			- VK_BUFFER_USAGE_TRANSFER_SRC_BIT: Buffer can be used as source in a memory transfer operation.
			- VK_BUFFER_USAGE_TRANSFER_DST_BIT: Buffer can be used as destination in a memory transfer operation.

			With this, the vertexBuffer is now allocated from a memory type that is device local, which generally means that we're not able to use vkMapMemory. However,
			we can copy data from the stagingBuffer to our vertexBuffer. We have to indicate that we intend to do that by specifying the transfer source flag for the stagingBuffer
			and the destination flag for the vertexBuffer, along with the vertex buffer usage flag. 
		*/
		VkDeviceSize bufferSize = sizeof(m_Vertices[0]) * m_Vertices.size();
		VkBuffer stagingBuffer;
		VkDeviceMemory stagingBufferMemory;
		CreateBuffer(bufferSize, VK_BUFFER_USAGE_TRANSFER_SRC_BIT, VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT, stagingBuffer, stagingBufferMemory);

		//It is now time to copy the vertex data to the buffer. This is done by mapping the buffer memory into CPU accessible memory with VkMapMemory.
		void* data; //Pointer to the mapped memory. 
		/*
			This function allows us to access a region of the specified memory resource defined by an offset and size. The offset and size here are 0 and bufferInfo.size respectively.
			It is also possible to specify the special value VK_WHOLE_SIZE to map all of the memory. The second to large parameter can be used to specify flags, but there aren't any
			yet avaliable in the current API. It must be set to the value 0. The last parameter specifies the output for the pointer to the mapped memory.
		*/
		vkMapMemory(m_Device, stagingBufferMemory, 0, bufferSize, 0, &data);

		/*
			We can now simply memcpy the vertex data to the mapped memory and unmap it again using vkUnmapMemory. Unfortunately, the driver may not immediately copy the data into
			the buffer memory, for example because of caching. It is also possible that writes to the buffer are not visible in the mapped memory yet. There are two ways to deal with that problem:

			- Use a memory heap that is coherent, indicated with VK_MEMORY_PROPERTY_HOST_COHERENT_BIT.
			- Call vkFlushMappedMemoryRanges after writing to the mapped memory, and call vkInvalidateMappedMemoryRanges before reading to the mapped memory.

			We went for the first approach, which ensures that the mapped memory always matches the contents of the allocated memory. Do keep in mind that this may lead to slightly
			worse performance than explicit flushing, but we will see why that doesn't matter eventually.

			Flushing memory ranges or using a coherent memory heap means that the driver will be aware of our writes to the buffer, but it doesn't mean that they are actually visible to the GPU yet.
			The transfer of data to the GPU is an operation that happens in the background and the specification simply tells us that it is guarenteed to be complete as of the next call to vkQueueSubmit.
		*/

		memcpy(data, m_Vertices.data(), (size_t)bufferSize);
		vkUnmapMemory(m_Device, stagingBufferMemory);

		CreateBuffer(bufferSize, VK_BUFFER_USAGE_TRANSFER_DST_BIT | VK_BUFFER_USAGE_VERTEX_BUFFER_BIT, VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT, m_VertexBuffer, m_VertexBufferMemory);
		CopyBuffer(stagingBuffer, m_VertexBuffer, bufferSize);
	}

	void CreateIndexBuffer()
	{
		/*
			There are only two notable differences between the creation of the Index and Vertex buffer. The bufferSize is now equal to the number of indices times the size of
			the index type. either uint16_t or uint32_t. The usage of the indexBuffer should be VK_BUFFER_USAGE_INDEX_BUFFER_BIT instead of VK_BUFFER_USAGE_VERTEX_BUFFER_BIT.
			We will create a staging buffer to copy the contents of indices to and then copy it to the final device local index buffer.
		*/

		VkDeviceSize bufferSize = sizeof(m_Indices[0]) * m_Indices.size();

		VkBuffer stagingBuffer;
		VkDeviceMemory stagingBufferMemory;
		//Note that VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT specifies that memory allocated with this type can be mapped for host access using vkMapMemory. 
		CreateBuffer(bufferSize, VK_BUFFER_USAGE_TRANSFER_SRC_BIT, VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT, stagingBuffer, stagingBufferMemory);
		
		void* data;
		vkMapMemory(m_Device, stagingBufferMemory, 0, bufferSize, 0, &data);
		memcpy(data, m_Indices.data(), (size_t)bufferSize);
		vkUnmapMemory(m_Device, stagingBufferMemory);

		CreateBuffer(bufferSize, VK_BUFFER_USAGE_TRANSFER_DST_BIT | VK_BUFFER_USAGE_INDEX_BUFFER_BIT, VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT, m_IndexBuffer, m_IndexBufferMemory);
		CopyBuffer(stagingBuffer, m_IndexBuffer, bufferSize);

		vkDestroyBuffer(m_Device, stagingBuffer, nullptr);
		vkFreeMemory(m_Device, stagingBufferMemory, nullptr);
	}

	void CreateCommandBuffers()
	{
		//With the command pool created, we can now start allocating command buffers and recording drawing commands in them. Because one of the drawing commands involves
		//binding the right vkFramebuffer, we will actually have to record a command buffer for every image in the swap chain once again. Thus, we have to create a list of vkCommandBuffer
		//objects as a class member. These command buffers will be automatically freed when their command pool is destroyed, so we don't need an explicit cleanup.
		//Command buffers are allocated with the vkAllocateCommandBuffers function, which takes a VkCommandBufferAllocateInfo struct as parameter that specifies the command pool and number of buffers to allocate.
		m_CommandBuffers.resize(m_SwapChainFramebuffers.size());
		
		VkCommandBufferAllocateInfo allocateInfo{};
		allocateInfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO;
		allocateInfo.commandPool = m_CommandPool;
		//The level parameter specifies if the allocated command buffers are primary or secondary command buffers.
		//- VK_COMMAND_BUFFER_LEVEL_PRIMARY: Can be submitted to a queue for execution, but cannot be called from other command buffers.
		//- VK_COMMAND_BUFFER_LEVEL_SECONDARY: Cannot be submitted directly, but can be called from primary command buffers. 
		//While we won't make use of secondary command buffers functionalities, you can imagine that its helpful to reuse common operations from primary command buffers.
		allocateInfo.level = VK_COMMAND_BUFFER_LEVEL_PRIMARY;
		allocateInfo.commandBufferCount = (uint32_t)m_CommandBuffers.size();

		if (vkAllocateCommandBuffers(m_Device, &allocateInfo, m_CommandBuffers.data()) != VK_SUCCESS)
		{
			throw std::runtime_error("Failed to allocate command buffers.");
		}

		//We begin recording a command buffer by calling vkBeginCommandBuffer with a small VkCommandBufferBeginInfo struct as argument that specifies some details about the usage of this specific command buffer.
		for (size_t i = 0; i < m_CommandBuffers.size(); i++)
		{
			VkCommandBufferBeginInfo beginInfo{};
			beginInfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO;
			/*
				The flags parameter specifies how we're going to use the command buffer. The following values are avaliable:
				- VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT: The command buffer will be rerecorded right after executing it once.
				- VK_COMMAND_BUFFER_USAGE_RENDER_PASS_CONTINUE_BIT: This is a secondary command buffer that will be entirely within a single render pass.
				- VK_COMMAND_BUFFER_USAGE_SIMULTANEOUS_USE_BIT: The command buffer can be resubmitted while it is also already pending execution.
				None of these flags are applicable for us at the moment. The pInheritenceInfo parameter is only relevant for secondary command buffers. It specifies which
				state to inherit from the calling primary command buffers. If the command buffer was already recorded once, then a call to vkBeginCommandBuffer will implictly
				reset it. Its not possible to append commands to a buffer at a later time.
			*/
			beginInfo.flags = 0; //Optional
			beginInfo.pInheritanceInfo = nullptr; //Optional

			if (vkBeginCommandBuffer(m_CommandBuffers[i], &beginInfo) != VK_SUCCESS)
			{
				throw std::runtime_error("Failed to begin recording command buffer.");
			}

			//Drawing starts by beginning the render pass with vkCmdBeginRenderPass. The render pass is configured using some parameters in a VkRenderPassBeginInfo struct.
			VkRenderPassBeginInfo renderPassInfo{};
			renderPassInfo.sType = VK_STRUCTURE_TYPE_RENDER_PASS_BEGIN_INFO;
			//The first parameters are the render pass itself and the attachments to bind. We created a framebuffer for each swapchain image that specifies it as color attachment.
			renderPassInfo.renderPass = m_RenderPass;
			renderPassInfo.framebuffer = m_SwapChainFramebuffers[i];
			//The next two parameters define the size of the render area. The render area defines where shaders load and stores will take place. The pixels outside this region will have undefined values.
			//It should match the size of the attachments for best performance.
			renderPassInfo.renderArea.offset = { 0, 0 };
			renderPassInfo.renderArea.extent = m_SwapChainExtent;
			//The last two parameters define the clear values to use for VK_ATTACHMENT_LOAD_OP_CLEAR, which we used as load operation for the color attachment. Here, we define it to be simply black with 100% opacity. 
			VkClearValue clearColor = { 0.0f, 0.0f, 0.0f, 0.0f };
			renderPassInfo.clearValueCount = 1;
			renderPassInfo.pClearValues = &clearColor;

			//The render pass can now begin. All of the functions that record commands can be recognized by their vkCmd prefix. They all return void, so there will be no error handling until we've finished recording.
			//The first parameter for every command is always the command buffer to record the command to. The second parameter specifies the details of the render pass we've just provided. The
			//final parameter controls how the drawing commands within the render pass will be provided. It can have one of two values:
			//- VK_SUBPASS_CONTENTS_INLINE: The render pass commands will be embedded in the primary command buffer itself and no secondary command buffers will be executed.
			//- VK_SUBPASS_CONTENTS_SECONDARY_COMMAND_BUFFERS: The render pass contents will be executed from secondary command buffers. 
			//We will not be using secondary command buffers, so we will go with the first option.
			vkCmdBeginRenderPass(m_CommandBuffers[i], &renderPassInfo, VK_SUBPASS_CONTENTS_INLINE);

			//We can now bind the graphics pipeline. The second parameter specifies if the pipeline object is a graphics or compute pipeline. 
			vkCmdBindPipeline(m_CommandBuffers[i], VK_PIPELINE_BIND_POINT_GRAPHICS, m_GraphicsPipeline);

			//We've now told Vulkan which operations to execute in the graphics pipeline and which attachment to use in the fragment shader, so all that remains is telling it to draw the triangle.
			//The actual vkCmdDraw function is a bit anticlimatic, but its so simple because of all the information we specified in advance. It has the following parameters, aside from the command buffer:
			//- vertexCount: The number of vertices we have in whatever we're drawing.
			//- instanceCount: Used for instanced rendering, use 1 if you're not doing that.
			//- firstVertex: Used as an offset into the vertex buffer, defines the lowest value of gl_VertexIndex.
			//- firstInstance: Used as an offset for instanced rendering, defines the lowest value of gl_InstanceIndex.
			VkBuffer vertexBuffers[] = { m_VertexBuffer };
			VkDeviceSize offsets[] = { 0 };
			//The vkCmdBindVertexBuffers function is used to bind vertex buffers to bindings. The first two parameters, besides the command buffer, specify the offset and the number of bindings
			//we're going to specify vertex buffers for. The last two parameters specify the array of vertex buffers to bind and the byte offsets to start reading the vertex data from. 
			vkCmdBindVertexBuffers(m_CommandBuffers[i], 0, 1, vertexBuffers, offsets);

			/*
				Using an index buffer requires us to first bind it, just like we did for the vertex buffer. The difference is that we can only have a single index buffer. Its unfortunately
			    not possible to use different indices for each vertex attribute, so we do still have to completely duplicate vertex data even if just one attribute varies.
				An index buffer is bound with vkCmdBindIndexBuffer, which has the index buffer, a byte offset into it and type of index data as parameters. As mentioned before,
				the possible types are VK_INDEX_TYPE_UINT16 and VK_INDEX_TYPE_UINT32.  
			 */	
			vkCmdBindIndexBuffer(m_CommandBuffers[i], m_IndexBuffer, 0, VK_INDEX_TYPE_UINT16);

			//We now need to tell Vulkan to change the drawing command to tell Vulkan to use the index buffer.
			//vkCmdDraw(m_CommandBuffers[i], static_cast<uint32_t>(m_Vertices.size()), 1, 0, 0);
			vkCmdDrawIndexed(m_CommandBuffers[i], static_cast<uint32_t>(m_Indices.size()), 1, 0, 0, 0);

			//The render pass can now be ended.
			vkCmdEndRenderPass(m_CommandBuffers[i]);
			//And we've finished recording the command buffer.
			if (vkEndCommandBuffer(m_CommandBuffers[i]) != VK_SUCCESS)
			{
				throw std::runtime_error("Failed to record command buffer.");
			}
		}
	}

	void CreateSyncObjects()
	{
		//We will need one semaphore to signal that an image has been acquired and is ready for rendering, and another one to signal that rendering has finished and presentation can happen.
		m_ImageAvaliableSemaphores.resize(g_MaxFramesInFlight);
		m_RenderFinishedSemaphores.resize(g_MaxFramesInFlight);
		m_InFlightFences.resize(g_MaxFramesInFlight);
		m_ImagesInFlight.resize(m_SwapChainImages.size(), VK_NULL_HANDLE); //Initially, not a single frame is using an image so we explictly initialize it to no fence. 

		VkSemaphoreCreateInfo semaphoreInfo{};
		semaphoreInfo.sType = VK_STRUCTURE_TYPE_SEMAPHORE_CREATE_INFO;

		VkFenceCreateInfo fenceInfo{};
		fenceInfo.sType = VK_STRUCTURE_TYPE_FENCE_CREATE_INFO;
		fenceInfo.flags = VK_FENCE_CREATE_SIGNALED_BIT;

		for (size_t i = 0; i < g_MaxFramesInFlight; i++)
		{
			if (vkCreateSemaphore(m_Device, &semaphoreInfo, nullptr, &m_ImageAvaliableSemaphores[i]) != VK_SUCCESS ||
				vkCreateSemaphore(m_Device, &semaphoreInfo, nullptr, &m_RenderFinishedSemaphores[i]) != VK_SUCCESS ||
				vkCreateFence(m_Device, &fenceInfo, nullptr, &m_InFlightFences[i]) != VK_SUCCESS)
			{
				throw std::runtime_error("Failed to create semaphores for a frame.");
			}
		}
	}

	void RecreateSwapChain()
	{
		//Minimizaton will result in a framebuffer size of 0. We will handle that by pausing the window until the window is in the foreground again.
		//The initial call to glfwGetFramebufferSize handles the case where the size is already correct and glfwWaitEvents will thus have nothing to wait on.
		int width = 0, height = 0;
		glfwGetFramebufferSize(m_Window, &width, &height);
		while (width == 0 || height == 0)
		{
			glfwGetFramebufferSize(m_Window, &width, &height);
			glfwWaitEvents(); //Await events by putting the current thread to sleep. Once a event is handled, the function returns instantly. 
		}

		/*
			Even though the application has successfully rendered your stuff, there are still some circumstances that we will have to handle. It is possible, for example, for the
			window surface to change such that the swapchain is no longer compatible with it. One of the reasons that could cause this to happen is the size of the window changing.
			We have to catch these events and recreate the swapchain. We will call for the CreateSwapchain function along with all of its dependencies here.

			We first call vkDeviceWaitIdle as we usually do as we shouldn't touch resources that may still be in use. Obviously the first thing we will have to do is recreate
			the swapchain itself. The image views need to be recreated as they are based directly on the swapchain images. The render pass needs to be recreated because
			because it depends on the format of the swapchain images. It is rare of the swapchain image format to change during an operation like a window resize, but it should
			still be handled. Viewport and scissor rectangle size is specified during graphics pipeline creation, so the pipeline also needs to be rebuilt. It is possible to
			avoid this by using dynamic state for the viewports and scissor rectangles. Finally, the framebuffers and command buffers also directly depend on the swapchain images.

			To make sure that the old versions of these objects are cleaned up before recreating them, we should move some of the cleanup code to a seperate function that we can call from here.
			Note that while calling ChooseSwapExtent within CreateSwapChain(), we already query the new window resolution to make sure that the swapchain images have the (new)
			right size, so there's no need to modify ChooseSwapExtent (remember that we already had to use glfwGetFramebufferSize get the resolution of the surface in pixels when creating the swapchain.			
		
			That's all it takes to recreate the swapchain. However, the disadvantage of this approach is that we need to stop all rendering before creating the new swapchain.
			It is possible to create a new swapchain while drawing commands on an image from the old swapchain are still in-flight. You need to pass the previous swapchain
			to the oldSwapChain field in the VkSwapchainCreateInfoKHR struct and destroy the old swapchain as soon as you've finished using it.
		*/

		vkDeviceWaitIdle(m_Device); //Wait for all ongoing operations to be complete before recreating the swapchain.
		CleanUpSwapChain();

		CreateSwapChain();
		CreateImageViews();
		CreateRenderPass();
		CreateGraphicsPipeline();
		CreateFramebuffers();
		CreateCommandBuffers();
	}

	void DrawFrame()
	{
		/*
			Wait for the previous operations/frame to finish before continuing with drawing operations. The vkWaitForFences function takes an array of fences and waits
			for either any or all of them to be signalled before returning. The VK_TRUE we pass in here signifies that we want to wait for all fences, but in the case of a single
			one, it obviously doesn't really matter. Just like vkAcquireNextImageKHR, this function also takes a timeout. Unlike the semaphores, we manually need to restore the
			fence to the unsignaled state by resetting it with the vkResetFences call. 

			However, if you simply run the program, nothing will render. That is because by default, fences are created in the unsignaled state. That means that vkWaitForFences
			will wait forever if we haven't used the fence before. To solve that, we can change the fence creation to initialize it in signalled state as if we had rendered an initial frame that finished. 
		*/

		vkWaitForFences(m_Device, 1, &m_InFlightFences[g_CurrentFrameIndex], VK_TRUE, UINT64_MAX);

		/*
			We will perform the following operations:
			- Acquire an image from the swapchain.
			- Execute the command buffer with that image as attachment in the framebuffer.
			- Return the image to the swapchain for presentation.
			Each of these events is set in motion using a single function call, but they are executed asynchronously. The function calls will return before the operations
			are actually finished and the order of execution is also undefined. That is unfortunate, because each of the operations depend on the previous one finishing. 
			There are two ways of synchronizing swapchain events: fences and semaphores. They're both objects that can be used to coordinating operations by having one operation signal and another
			operation wait for a fence or semaphore to go from the unsignaled state to signaled state. The difference is that the state of fences can be accessed from
			your program using calls like vkWaitForFences and semaphores cannot be. Fences are mainly designed to synchronize your application itself with rendering operations,
			whereas semaphores are used to synchronize operations within or across command queues. We want to synchronize the queue operations of drawing commands and presentations, which makes semaphores the best fit.
		*/

		//As mentioned before, the first thing we need to do in the DrawFrame function is acquire an image from the swap chain. Recall that the swap chain is an extension feature,
		//so we must use a function with the vhKHR naming convention.
		uint32_t imageIndex;

		/*
			We have to figure out when swap chain recreation is necessary and call our RecreateSwapChain function accordingly. Luckily, Vulkan will usually just tell us
			that the swap chain is no longer adequate during presentation. The vkAcquireNextImageKHR and vkQueuePresentKHR functions can return the following special values to
			indicate this:
			- VK_ERROR_OUT_OF_DATE_KHR: The swapchain has become incompatible with the surface and can no longer be used for rendering. Usually happens after a window resize.
			- VK_SUBOPTIMAL_KHR: The swapchain can still be used to successfully present to the surface, but the surface properties are no longer matched directly. 

			The first two parameters of vkAcquireNextImageKHR are the logical device and the swapchain from which we wish to acquire an image. The third parameter
			specifies a timeout in nanoseconds for an image to become avaliable. Using a maximum value of a 64 bit unsigned integer disables the timeout.
			The next two parameters specify synchronization objects that are to be signaled when the presentation engine is finished using the image. 
			The last parameter specifies a variable to output the index of the swap chain image that has become avaliable. The index refers to the VkImage in our m_SwapchainImages array.
			We're going to use that index to pick the right command buffer. 
		*/
		
		VkResult result = vkAcquireNextImageKHR(m_Device, m_SwapChain, UINT64_MAX, m_ImageAvaliableSemaphores[g_CurrentFrameIndex], VK_NULL_HANDLE, &imageIndex);
		//If the swapchain turns out to be out of date when attempting to acquire an image, then it is no longer possible to present to it.
		//Thus, we should immediately recreate the swapchain and try again in the next DrawFrame call.
		//We should also decide to do so if the swapchain is suboptimal, but we've chosen to proceed anyway in that case as we've already acquired an image. Both VK_SUCCESS
		//and VK_SUBOPTIMAL_KHR are considered "success" return codes. 
		if (result == VK_ERROR_OUT_OF_DATE_KHR) 
		{
			RecreateSwapChain();
			return;
		}
		else if (result != VK_SUCCESS && result != VK_SUBOPTIMAL_KHR)
		{
			throw std::runtime_error("Failed to acquire swapchain image.");
		}

		//Now, we will wait on any previous frame that is using the image that we've just assigned for the new frame.
		//Check if a previous frame is using this image (i.e. there is its fence to wait on)
		if (m_ImagesInFlight[imageIndex] != VK_NULL_HANDLE)
		{
			vkWaitForFences(m_Device, 1, &m_ImagesInFlight[imageIndex], VK_TRUE, UINT64_MAX);
		}

		//Once done, mark the image as now being in use by this frame.
		m_ImagesInFlight[imageIndex] = m_InFlightFences[g_CurrentFrameIndex];

		//Queue submission and synchronization is configured through parameters in the VkSubmitInfo structure.
		VkSubmitInfo submitInfo{};
		submitInfo.sType = VK_STRUCTURE_TYPE_SUBMIT_INFO;

		/*
			The first three parameters specify which semaphores to wait on before execution begins and in which stage(s) of the pipeline to wait. We want to prevent from
			writing colors to the image until its avaliable, so we're specifying the start of the graphics pipeline that write to the color attachment. That means that
			thereotically the implementation can already start executing our vertex shader and such while the image is not yet avaliable. We don't want that to happen.
			Note that each entry in the waitStages array corresponds to the semaphore with the same index in pWaitSemaphores. 
		*/
		VkSemaphore waitSemaphores[] = { m_ImageAvaliableSemaphores[g_CurrentFrameIndex] };
		VkPipelineStageFlags waitStages[] = { VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT };
		submitInfo.waitSemaphoreCount = 1;
		submitInfo.pWaitSemaphores = waitSemaphores;
		submitInfo.pWaitDstStageMask = waitStages;

		//The next two parameters specify which command buffers to actually submit for execution. As mentioned earlier, we should submit the command buffer that
		//binds the swapchain image we just acquired as color attachment. 
		submitInfo.commandBufferCount = 1;
		submitInfo.pCommandBuffers = &m_CommandBuffers[imageIndex];

		//The signalSemaphoreCount and pSignalSemaphores parameters specify which semaphores to signal once the command buffer(s) have finished execution. In our case, we're
		//using the renderFinishedSemaphore for that purpose. 
		VkSemaphore signalSemaphores[] = { m_RenderFinishedSemaphores[g_CurrentFrameIndex] };
		submitInfo.signalSemaphoreCount = 1;
		submitInfo.pSignalSemaphores = signalSemaphores;
		
		//Because we now have more calls to vkWaitForFences, the vkResetFences call should be moved. Its best so imply call it right before actually using the fence. 
		vkResetFences(m_Device, 1, &m_InFlightFences[g_CurrentFrameIndex]);

		//We can now submit the command buffer to the graphics queue using vkQueueSubmit. The function takes an array of VkSubmitInfo structures as arugment for efficiency when the workload is much larger.
		//The last parameter references an optional fence that will be signalled when the command buffers finish execution. We will pass our fence in to signal when a frame has finished.
		if (vkQueueSubmit(m_GraphicsQueue, 1, &submitInfo, m_InFlightFences[g_CurrentFrameIndex]) != VK_SUCCESS)
		{
			throw std::runtime_error("Failed to submit draw command buffers.");
		}

		//The last step of drawing a frame is submitting the result back to the swapchain to have it eventually show up on the screen. Presentation is configured
		//through a VkPresentInfoKHR structure at the end of DrawFrame function. 
		VkPresentInfoKHR presentInfo{};
		presentInfo.sType = VK_STRUCTURE_TYPE_PRESENT_INFO_KHR;
		//The first two parameter specify which semaphores to wait on before presentation can happen, just like VkSubmitInfo. We just thus await for the signal before presentation.
		presentInfo.waitSemaphoreCount = 1;
		presentInfo.pWaitSemaphores = signalSemaphores;

		//The next two parameter specify the swapchains to present images to and the index of the image for each swapchain. This will almost always be a single one.
		VkSwapchainKHR swapchains[] = { m_SwapChain };
		presentInfo.swapchainCount = 1;
		presentInfo.pSwapchains = swapchains;
		presentInfo.pImageIndices = &imageIndex;
		//There is one last optional parameter called pResults. It allows you to specify an array of VkResult values to check for every individual swapchain if 
		//presentation was successful. Its not necessary if you're only using a single swapchain, because you can simply use the return value of the present function.
		presentInfo.pResults = nullptr; //Optional

		//The vkQueuePresentKHR function submits the request to present an image to the swapchain. Similar to vkAcquireNextImageKHR, we will check if the swapchain is still valid.
		result = vkQueuePresentKHR(m_PresentationQueue, &presentInfo);

		//It is important to do this after vkQueuePresentKHR to ensure that the semaphores are in a consistent state, otherwise a signalled semaphore may never be properly waited upon.
		//Now, to actually detect resizes, we can use the glfwSetFramebufferSizeCallback function in the GLFW framework to setup a callback.
		if (result == VK_ERROR_OUT_OF_DATE_KHR || result == VK_SUBOPTIMAL_KHR || m_FramebufferResized) 
		{
			m_FramebufferResized = false;
			RecreateSwapChain();
		}
		else if (result != VK_SUCCESS) 
		{
			throw std::runtime_error("Failed to present swapchain image.");
		}

		/*
			If you run your application with validation layers enabled, you may get either errors or notice that the memory usage usually slowly grows. The reason for this is that
			the application is rapidly submitting work in the DrawFrame function, but doesn't actually check if any of it finishes. If the CPU is submitting work faster than the GPU
			can keep up,then the queue will slowly fill up with work. Worse, even, is that we are reusing the m_ImageAvaliableSemaphore and m_RenderFinishedSemaphore semaphores along with command buffers
			for multiple frames at the same time. The easy way to solve this is to wait for work to finish right after submitting it, for example by using vkQueueWaitIdle.
			
			However, we are likely not optimally using the GPU in this way, because the whole graphics pipeline is only used for one frame at a time right now. The stages that the
			current frame has already progressed through are idle and could already be used for a next frame. We will now extend our application to allow for multiple frames to be
			in-flight while still bounding the amount of work that piles up. Start by adding a constant at the top of the program that specifies how many frames should be processed concurrently. 
			
			Although we've now set up the required objects to faciliate processing of multiple frames simultaneously, we still don't acrtually prevent more than g_MaxFramesInFlight from being submitted.
			Right now, there is only GPU-GPU synchronization and no CPU-GPU synchronization going on to keep track of how the work is going. We may be using the frame #0 objects while frame #0 is still in-flight.

			To perform CPU-GPU synchronization, Vulkan offers a second tpye of synchronization primitive called fences. Fences are similar to semaphores in the sense that they can be signalled and waited for,
			but this time, we actually wait for them in our own code. We will first create a fence for each frame. Then, we will adjust our present function to signmal the fence
			that rendering is complete, which the fence is waiting for at the start of this function. This allows for operations to complete before a new frame is drawn.

			The memory leak problem is gone now, but the program is still not working quite correctly yet. If g_MaxFramesInFlight is higher than the number of swapchain images or
			vkAcquireNextImageKHR returns images-out-of-order, then its possible that we may start rendering to a swap chain image that is already in flight. To avoid this, we need
			to track for each swapchain image if a frame in flight is currently using it. This mapping will refer to frames in flight by their fences so we will
			immediately have a synchronization object to wait on before a new frame can use that image.

			We have now implemented all the needed synchronmization to ensure that there are no more than 2 frames of work enqueued and that these frames are not accidentally using the same image.
		*/
		vkQueueWaitIdle(m_PresentationQueue); //Wait for work to finish after submitting it.

		//By using the modulo operator, we ensure that the frame index loops around every g_MaxFramesInFlight enqueued frames.
		g_CurrentFrameIndex = (g_CurrentFrameIndex + 1) % g_MaxFramesInFlight;
	}

private:
	GLFWwindow* m_Window;
	const uint32_t m_WindowWidth = 1280;
	const uint32_t m_WindowHeight = 720;
	bool m_FramebufferResized = false;

	VkInstance m_VulkanInstance;
	VkSurfaceKHR m_Surface;
	VkDebugUtilsMessengerEXT m_DebugMessenger;
	VkPhysicalDevice m_PhysicalDevice; //Implictly destroyed when the VkInstance is destroyed.
	VkDevice m_Device; //Used to interface with the physical device. Multiple logical devices can be created from the same physical device.
	VkQueue m_GraphicsQueue; //Used to interface with the queues in our logical device. This is implicitly destroyed with the device. 
	VkQueue m_PresentationQueue;
	VkRenderPass m_RenderPass;
	VkPipelineLayout m_PipelineLayout;
	VkPipeline m_GraphicsPipeline;
	std::vector<VkFramebuffer> m_SwapChainFramebuffers; 
	VkCommandPool m_CommandPool;
	std::vector<VkCommandBuffer> m_CommandBuffers;
	std::vector<VkSemaphore> m_ImageAvaliableSemaphores; //Each frame should have its own set of semaphores.
	std::vector<VkSemaphore> m_RenderFinishedSemaphores;
	std::vector<VkFence> m_InFlightFences;
	std::vector<VkFence> m_ImagesInFlight;

	//Buffers
	VkBuffer m_VertexBuffer;
	VkDeviceMemory m_VertexBufferMemory;
	VkBuffer m_IndexBuffer;
	VkDeviceMemory m_IndexBufferMemory;

	//Swapchain
	VkSwapchainKHR m_SwapChain;
	std::vector<VkImage> m_SwapChainImages; //Images are created by the swapchain and will be automatically cleaned up as well when it is destroyed. 
	std::vector<VkImageView> m_SwapChainImageViews;		
	VkFormat m_SwapChainImageFormat;
	VkExtent2D m_SwapChainExtent;

	const std::vector<const char*> m_ValidationLayers = { "VK_LAYER_KHRONOS_validation" };
	const std::vector<const char*> m_DeviceExtensions = { VK_KHR_SWAPCHAIN_EXTENSION_NAME };

#ifdef NDEBUG
	const bool m_ValidationLayersEnabled = false;
#else
	const bool m_ValidationLayersEnabled = true;
#endif
	
	//Data
	const std::vector<Vertex> m_Vertices =
	{
		{{-0.5f, -0.5f}, {1.0f, 0.0f, 0.0f}},
		{{0.5f, -0.5f}, {0.0f, 1.0f, 0.0f}},
		{{0.5f, 0.5f}, {0.0f, 0.0f, 1.0f}},
		{{-0.5f, 0.5f}, {1.0f, 1.0f, 1.0f}}
	};

	//It is possible to use either uint16_t or uint32_t for your index buffer depending on the number of entries in m_Vertices. We can stick to uint16_t for now as we
	//are using less than 65535 unique vertices. Just like the vertex data, the indices need to be uploaded into a VkBuffer for the GPU to be able to access them.
	const std::vector<uint16_t> m_Indices =
	{
		0, 1, 2, 2, 3, 0
	};
};

int main(int argc, int argv[])
{
	VulkanApplication application;

	try
	{
		application.Run();
	}
	catch (const std::exception& error)
	{
		std::cerr << error.what() << std::endl;
		return EXIT_FAILURE; //Unsuccessful program execution.
	}

	return EXIT_SUCCESS; //Successful program execution.
}