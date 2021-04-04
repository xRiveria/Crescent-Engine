#pragma once
#include <vulkan/vulkan.h>
#include <vector>
#include <GLFW/glfw3.h>

namespace Crescent
{
	class VulkanSwapchain
	{
	public:
		VulkanSwapchain(VkPhysicalDevice* physicalDevice, VkDevice* logicalDevice, VkSurfaceKHR* presentationSurface, GLFWwindow* window);

	private:
		VkSurfaceFormatKHR SelectSwapchainSurfaceFormat(const std::vector<VkSurfaceFormatKHR>& avaliableFormats);
		VkPresentModeKHR SelectSwapchainPresentationMode(const std::vector<VkPresentModeKHR>& avaliablePresentationModes);
		VkExtent2D SelectSwapExtent(const VkSurfaceCapabilitiesKHR& capabilities);

		void CreateSwapchain();

	private:
		VkSwapchainKHR m_Swapchain;
		
		GLFWwindow* m_Window;
		VkSurfaceKHR* m_Surface;
		VkPhysicalDevice* m_PhysicalDevice;
		VkDevice* m_LogicalDevice;
	};
}