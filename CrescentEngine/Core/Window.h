#pragma once
#include <GL/glew.h>
#include <GLFW/glfw3.h>
#include <string>
#include <functional>

namespace CrescentEngine
{
	class Window
	{
	public:
		Window() {}
		Window(const std::string& windowName, const float& windowWidth, const float& windowHeight);

		void CreateWindow(const std::string& windowName, const float& windowWidth, const float& windowHeight);
		void TerminateWindow() { glfwSetWindowShouldClose(m_ApplicationWindow, true); }
		void SwapBuffers() { glfwSwapBuffers(m_ApplicationWindow); }

		//Callbacks
		void PollEvents();
		void SetFramebufferCallback(GLFWframebuffersizefun callback);
		void SetMouseButtonCallback(GLFWmousebuttonfun callback);
		void SetMouseScrollCallback(GLFWscrollfun callback);
		void SetMouseCursorCallback(GLFWcursorposfun callback);
		void SetWindowDimensions(const float& windowWidth, const float& windowHeight) { m_WindowWidth = windowWidth; m_WindowHeight = windowHeight; }

		//Information Retrieval
		GLFWwindow* RetrieveWindow() const { return m_ApplicationWindow; }
		float RetrieveCurrentTime() const { return glfwGetTime(); }
		float RetrieveWindowWidth() const { return m_WindowWidth; }
		float RetrieveWindowHeight() const { return m_WindowHeight; }
		float RetrieveAspectRatio() { return (m_WindowWidth / m_WindowHeight); }
		bool RetrieveWindowCloseStatus() const { return glfwWindowShouldClose(m_ApplicationWindow); }

	private:
		void InitializeGLFW();

	private:
		GLFWwindow* m_ApplicationWindow = nullptr;
		std::string m_ApplicationName = "Application";

		float m_WindowWidth = 1280.0f;
		float m_WindowHeight = 720.0f;
	};
}
