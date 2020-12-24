#include "CrescentPCH.h"
#include "Window.h"
#include "Editor.h"
#include "Rendering/Texture.h"
#include "Utilities/Timestep.h"
#include "Utilities/Camera.h"
#include "Rendering/Framebuffer.h";
#include "Models/Model.h"
#include "Rendering/Renderer.h"
#include "Rendering/RenderQueue.h"
#include "Object.h"
#include "Primitive.h"
#include "Rendering/Cubemap.h"
#include <stb_image/stb_image.h>
#include <imgui/imgui.h>

float animationTime = 0.0f;

struct CoreSystems
{
	CrescentEngine::Window m_Window; //Setups our Window.
	CrescentEngine::Editor m_Editor; //Setups our ImGui context.
	CrescentEngine::Renderer m_Renderer; //Setups our OpenGL context.
	CrescentEngine::Timestep m_Timestep; //Setups our Timestep.
	CrescentEngine::Camera m_Camera = { glm::vec3(0.0f, 0.0f, 3.0f) }; //Setups our Camera.

	float m_LastFrameTime = 0.0f;
};

struct RenderingComponents
{
	CrescentEngine::Framebuffer m_Framebuffer;
	CrescentEngine::DepthmapFramebuffer m_DepthMapFramebuffer;
	CrescentEngine::Cubemap m_Cubemap;

	bool m_LightingModel[2] = { true, false };  //[0] for Blinn Phong, [1] for Phong.
	float pcfSampleAmount = 15.0f;
	bool m_SoftOrHardShadows[2] = { true, false }; //[0] for Soft, [1] for Hard.
	bool m_WireframeRendering = false;
};

struct Renderables  //Currently our base scene objects.
{
	CrescentEngine::Model m_BackpackModel;
	CrescentEngine::Model m_StormTrooperModel;
	CrescentEngine::Model m_EggDogModel;
	CrescentEngine::Model m_GeodudeModel;

	glm::vec3 m_BackpackModelPosition = { 0.0f, 1.3f, 0.0f };
	glm::vec3 m_StormTrooperPosition = { -5.0f, -0.5f, -1.9f };
	glm::vec3 m_GeodudePosition = { -27.5f, -5.9f, 66.5f };
	glm::vec3 m_EggDogPosition = { 3.7f, -0.5f, -4.1f };

	CrescentEngine::Model m_RedstoneLampModel;
	CrescentEngine::Primitive m_Plane; //Our base plane.
	CrescentEngine::TransparentQuad m_TransparentQuad;

	CrescentEngine::DirectionalLight m_LightDirection;
	CrescentEngine::PointLight m_PointLight;

	CrescentEngine::RenderQueue m_RenderQueue;

	//Cubemap
	std::vector<std::string> m_OceanCubemap = {
		"Resources/Skybox/Ocean/right.jpg",
		"Resources/Skybox/Ocean/left.jpg",
		"Resources/Skybox/Ocean/top.jpg",
		"Resources/Skybox/Ocean/bottom.jpg",
		"Resources/Skybox/Ocean/front.jpg",
		"Resources/Skybox/Ocean/back.jpg"
	};

	//Vegetables
	std::vector<glm::vec3> m_VegetableLocations = {
		glm::vec3(-1.5f, 0.0f, -0.48f),
		glm::vec3(1.5f, 0.0f, 0.51f),
		glm::vec3(0.0f, 0.0f, 0.7f),
		glm::vec3(-0.3f, 0.0f, -2.3f),
		glm::vec3(0.5f, 0.0f, -0.6f)
	};
};

struct Shaders
{
	CrescentEngine::LearnShader m_StaticModelShader;
	CrescentEngine::LearnShader m_PointLightObjectShader;
	CrescentEngine::LearnShader m_OutlineObjectShader;
	CrescentEngine::LearnShader m_TransparentQuadShader;
	CrescentEngine::LearnShader m_DepthShader;
	CrescentEngine::LearnShader m_AnimationShader;
};

struct Textures
{
	CrescentEngine::Texture2D m_GrassTexture;
	CrescentEngine::Texture2D m_WindowTexture;
	CrescentEngine::Texture2D m_MarbleTexture;
	CrescentEngine::Texture2D m_WoodTexture;
};

//Our Systems	
CoreSystems g_CoreSystems; //Creates our core engine systems.
RenderingComponents g_RenderingComponents; //Creates our rendering components.
Renderables g_Renderables; //Creates our assets.
Shaders g_Shaders; //Creates our shaders.
Textures g_Textures; //Creates our textures.

//Input Callbacks
void RenderScene(CrescentEngine::LearnShader& shader, bool renderShadowMap);
void DrawEditorContent();
void ProcessKeyboardEvents(GLFWwindow* window);
void FramebufferResizeCallback(GLFWwindow* window, int windowWidth, int windowHeight);
void CameraAllowEulerCallback(GLFWwindow* window, int button, int action, int mods);
void CameraMovementCallback(GLFWwindow* window, double xPos, double yPos);
void CameraZoomCallback(GLFWwindow* window, double xOffset, double yOffset);

glm::mat4 projectionMatrix = glm::mat4(1.0f);

int main(int argc, int argv[])
{
	//Initializes GLFW.
	g_CoreSystems.m_Window.CreateNewWindow("Crescent Engine", 1280.0f, 720.0f);

	//Callbacks
	g_CoreSystems.m_Window.SetFramebufferCallback(FramebufferResizeCallback);
	g_CoreSystems.m_Window.SetMouseCursorCallback(CameraMovementCallback);
	g_CoreSystems.m_Window.SetMouseButtonCallback(CameraAllowEulerCallback);
	g_CoreSystems.m_Window.SetMouseScrollCallback(CameraZoomCallback);

	//Initializes OpenGL.
	g_CoreSystems.m_Renderer.InitializeOpenGL();
	g_CoreSystems.m_Renderer.ToggleDepthTesting(true);
	g_CoreSystems.m_Renderer.ToggleBlending(true);
	g_CoreSystems.m_Renderer.ToggleFaceCulling(false);

	//Setups ImGui
	g_CoreSystems.m_Editor.SetApplicationContext(&g_CoreSystems.m_Window);
	g_CoreSystems.m_Editor.InitializeImGui();

	g_RenderingComponents.m_Framebuffer.InitializeFramebuffer(g_CoreSystems.m_Window.RetrieveWindowWidth(), g_CoreSystems.m_Window.RetrieveWindowHeight());
	g_RenderingComponents.m_DepthMapFramebuffer.SetupDepthMapFramebuffer();

	//Cubemaps
	g_RenderingComponents.m_Cubemap.LoadCubemap(g_Renderables.m_OceanCubemap);
	g_RenderingComponents.m_Cubemap.SetupCubemapBuffers();

	//Shaders
	g_Shaders.m_StaticModelShader.CreateShaders("Resources/Shaders/StaticModelVertex.shader", "Resources/Shaders/StaticModelFragment.shader");
	g_Shaders.m_PointLightObjectShader.CreateShaders("Resources/Shaders/LightVertexShader.shader", "Resources/Shaders/LightFragmentShader.shader");
	g_Shaders.m_OutlineObjectShader.CreateShaders("Resources/Shaders/OutlineVertex.shader", "Resources/Shaders/OutlineFragment.shader");
	g_Shaders.m_TransparentQuadShader.CreateShaders("Resources/Shaders/TransparentVertex.shader", "Resources/Shaders/TransparentFragment.shader");
	g_Shaders.m_DepthShader.CreateShaders("Resources/Shaders/DepthVertex.shader", "Resources/Shaders/DepthFragment.shader");
	g_Shaders.m_AnimationShader.CreateShaders("Resources/Shaders/AnimationVertex.shader", "Resources/Shaders/AnimationFragment.shader");

	//Objects
	g_Renderables.m_Plane.SetupPrimitiveBuffers(CrescentEngine::PrimitiveShape::PlanePrimitive);
	g_Renderables.m_TransparentQuad.SetupTransparentQuadBuffers();
	g_Textures.m_GrassTexture.LoadTexture("Resources/Textures/Grass.png");
	g_Textures.m_WindowTexture.LoadTexture("Resources/Textures/TransparentWindow.png");
	g_Textures.m_MarbleTexture.LoadTexture("Resources/Textures/Marble.jpg");
	g_Textures.m_WoodTexture.LoadTexture("Resources/Textures/Wood.png");

	stbi_set_flip_vertically_on_load(true);
	g_Renderables.m_BackpackModel.LoadModel("Resources/Models/Backpack/backpack.obj", g_CoreSystems.m_Window);
	g_Renderables.m_RedstoneLampModel.LoadModel("Resources/Models/RedstoneLamp/Redstone-lamp.obj", g_CoreSystems.m_Window);
	stbi_set_flip_vertically_on_load(false);
	g_Renderables.m_EggDogModel.LoadModel("Resources/Models/Eggdog/source/Eggdog/Eggdog.obj", g_CoreSystems.m_Window);
	g_Renderables.m_StormTrooperModel.LoadModel("Resources/Models/Stormtrooper/source/silly_dancing.fbx", g_CoreSystems.m_Window);
	g_Renderables.m_GeodudeModel.LoadModel("Resources/Models/Pokeball/source/RufflesDuchessVisual.fbx", g_CoreSystems.m_Window);

	while (!g_CoreSystems.m_Window.RetrieveWindowCloseStatus())
	{
		//Check Projection Matrix
		if (g_CoreSystems.m_Editor.RetrieveViewportWidth() > 0.0f && g_CoreSystems.m_Editor.RetrieveViewportHeight() > 0.0f && (g_RenderingComponents.m_Framebuffer.RetrieveFramebufferWidth() != g_CoreSystems.m_Editor.RetrieveViewportWidth() || g_RenderingComponents.m_Framebuffer.RetrieveFramebufferHeight() != g_CoreSystems.m_Editor.RetrieveViewportHeight()))
		{
			g_RenderingComponents.m_Framebuffer.ResizeFramebuffer(g_CoreSystems.m_Editor.RetrieveViewportWidth(), g_CoreSystems.m_Editor.RetrieveViewportHeight());
			projectionMatrix = glm::perspective(glm::radians(g_CoreSystems.m_Camera.m_MouseZoom), ((float)g_CoreSystems.m_Editor.RetrieveViewportWidth() / (float)g_CoreSystems.m_Editor.RetrieveViewportHeight()), 0.2f, 100.0f);
		}

		//Retrieve Delta Time
		float currentFrame = g_CoreSystems.m_Window.RetrieveCurrentTime();
		g_CoreSystems.m_Timestep = currentFrame - g_CoreSystems.m_LastFrameTime;
		animationTime += g_CoreSystems.m_Timestep.GetDeltaTimeInSeconds();
		if (animationTime > g_Renderables.m_StormTrooperModel.m_AnimationTime)
		{
			animationTime = 0.0f;
		}
		g_CoreSystems.m_LastFrameTime = currentFrame;

		//Poll Events
		g_CoreSystems.m_Window.PollEvents();
		ProcessKeyboardEvents(g_CoreSystems.m_Window.RetrieveWindow());

		g_CoreSystems.m_Renderer.ClearBuffers();

		//Bind our Depth Framebuffer.
		glm::mat4 lightProjectionMatrix = glm::mat4(1.0f), lightViewMatrix = glm::mat4(1.0f);
		glm::mat4 lightSpaceMatrix = glm::mat4(1.0f);
		float nearPlane = 1.0f, farPlane = 30.5f;
		lightProjectionMatrix = glm::ortho(-10.0f, 10.f, -10.0f, 10.0f, nearPlane, farPlane);
		lightViewMatrix = glm::lookAt(g_Renderables.m_PointLight.pointLightPosition, glm::vec3(0.0f, 0.0f, 0.0f), glm::vec3(0.0f, 1.0f, 0.0f));
		lightSpaceMatrix = lightProjectionMatrix * lightViewMatrix;

		//Render Scene from the light's point of view.
		g_Shaders.m_DepthShader.UseShader();
		g_Shaders.m_DepthShader.SetUniformMat4("lightSpaceMatrix", lightSpaceMatrix);
		g_RenderingComponents.m_DepthMapFramebuffer.BindDepthFramebuffer();
		glClear(GL_DEPTH_BUFFER_BIT);
		RenderScene(g_Shaders.m_DepthShader, true);

		g_Shaders.m_DepthShader.UnbindShader();
		g_RenderingComponents.m_DepthMapFramebuffer.UnbindDepthFramebuffer();

		//Bind Our Editor Window Framebuffer
		g_RenderingComponents.m_Framebuffer.BindFramebuffer();
		glViewport(0, 0, g_CoreSystems.m_Editor.RetrieveViewportWidth(), g_CoreSystems.m_Editor.RetrieveViewportHeight());
		g_CoreSystems.m_Renderer.ClearBuffers();

		//Draws our Scene
		g_Shaders.m_StaticModelShader.UseShader();
		g_Shaders.m_StaticModelShader.SetUniformInteger("shadowMap", 3);
		g_Shaders.m_StaticModelShader.SetUniformMat4("lightSpaceMatrix", lightSpaceMatrix);
		g_Shaders.m_StaticModelShader.SetUniformVector3("lightPos", g_Renderables.m_PointLight.pointLightPosition);
		glActiveTexture(GL_TEXTURE3);
		glBindTexture(GL_TEXTURE_2D, g_RenderingComponents.m_DepthMapFramebuffer.RetrieveDepthmapTextureID());
		RenderScene(g_Shaders.m_StaticModelShader, false);

		g_RenderingComponents.m_Framebuffer.UnbindFramebuffer();

		//We reset the framebuffer back to normal here for our Editor.
		DrawEditorContent();

		g_CoreSystems.m_Window.SwapBuffers();
	}

	g_CoreSystems.m_Window.TerminateWindow();
	return 0;
}

void RenderScene(CrescentEngine::LearnShader& shader, bool renderShadowMap)
{
	//View/Projection Matrix
	glm::mat4 viewMatrix = g_CoreSystems.m_Camera.GetViewMatrix();

	//Backpack Model - To Be Further Abstracted ===========================================================================
	g_Shaders.m_StaticModelShader.UseShader();
	g_Shaders.m_StaticModelShader.SetUniformVector3("directionalLight.lightDirection", g_Renderables.m_LightDirection.lightDirection);
	g_Shaders.m_StaticModelShader.SetUniformVector3("directionalLight.ambientIntensity", g_Renderables.m_LightDirection.ambientIntensity);
	g_Shaders.m_StaticModelShader.SetUniformVector3("directionalLight.diffuseIntensity", g_Renderables.m_LightDirection.diffuseIntensity);
	g_Shaders.m_StaticModelShader.SetUniformVector3("directionalLight.specularIntensity", g_Renderables.m_LightDirection.specularIntensity);

	g_Shaders.m_StaticModelShader.SetUniformVector3("pointLight.lightPosition", g_Renderables.m_PointLight.pointLightPosition);
	g_Shaders.m_StaticModelShader.SetUniformFloat("pointLight.attenuationConstant", 1.0f);
	g_Shaders.m_StaticModelShader.SetUniformFloat("pointLight.attenuationLinear", 0.09f);
	g_Shaders.m_StaticModelShader.SetUniformFloat("pointLight.attenuationQuadratic", 0.032f);
	g_Shaders.m_StaticModelShader.SetUniformVector3("pointLight.ambientIntensity", g_Renderables.m_PointLight.ambientIntensity);
	g_Shaders.m_StaticModelShader.SetUniformVector3("pointLight.diffuseIntensity", g_Renderables.m_PointLight.diffuseIntensity);
	g_Shaders.m_StaticModelShader.SetUniformVector3("pointLight.specularIntensity", g_Renderables.m_PointLight.specularIntensity);

	g_Shaders.m_StaticModelShader.SetUniformMat4("projection", projectionMatrix);
	g_Shaders.m_StaticModelShader.SetUniformMat4("view", viewMatrix);
	g_Shaders.m_StaticModelShader.SetUniformVector3("viewPosition", g_CoreSystems.m_Camera.m_CameraPosition);
	g_Shaders.m_StaticModelShader.SetUniformInteger("shadowMap", 3);

	//Models
	if (renderShadowMap)
	{
		g_Renderables.m_BackpackModel.Draw(shader, renderShadowMap, g_RenderingComponents.m_DepthMapFramebuffer.RetrieveDepthmapTextureID(), 1.0f, g_Renderables.m_BackpackModelPosition);
		g_Renderables.m_EggDogModel.Draw(shader, renderShadowMap, g_RenderingComponents.m_DepthMapFramebuffer.RetrieveDepthmapTextureID(), 1.0f, g_Renderables.m_EggDogPosition);
	}
	else
	{
		g_Renderables.m_BackpackModel.Draw(shader, renderShadowMap, 0, 1.0f, g_Renderables.m_BackpackModelPosition);
		g_Renderables.m_EggDogModel.Draw(shader, renderShadowMap, 0, 1.0f, g_Renderables.m_EggDogPosition);

		g_Shaders.m_AnimationShader.UseShader();
		g_Shaders.m_AnimationShader.SetUniformVector3("pointLight.lightPosition", g_Renderables.m_PointLight.pointLightPosition);
		g_Shaders.m_AnimationShader.SetUniformFloat("pointLight.attenuationConstant", 1.0f);
		g_Shaders.m_AnimationShader.SetUniformFloat("pointLight.attenuationLinear", 0.09f);
		g_Shaders.m_AnimationShader.SetUniformFloat("pointLight.attenuationQuadratic", 0.032f);

		g_Shaders.m_AnimationShader.SetUniformVector3("pointLight.ambientIntensity", g_Renderables.m_PointLight.ambientIntensity);
		g_Shaders.m_AnimationShader.SetUniformVector3("pointLight.diffuseIntensity", g_Renderables.m_PointLight.diffuseIntensity);
		g_Shaders.m_AnimationShader.SetUniformVector3("pointLight.specularIntensity", g_Renderables.m_PointLight.specularIntensity);

		g_Shaders.m_AnimationShader.SetUniformVector3("directionalLight.lightDirection", g_Renderables.m_LightDirection.lightDirection);
		g_Shaders.m_AnimationShader.SetUniformVector3("directionalLight.ambientIntensity", g_Renderables.m_LightDirection.ambientIntensity);
		g_Shaders.m_AnimationShader.SetUniformVector3("directionalLight.diffuseIntensity", g_Renderables.m_LightDirection.diffuseIntensity);
		g_Shaders.m_AnimationShader.SetUniformVector3("directionalLight.specularIntensity", g_Renderables.m_LightDirection.specularIntensity);
		g_Shaders.m_AnimationShader.SetUniformMat4("projection", projectionMatrix);
		g_Shaders.m_AnimationShader.SetUniformMat4("view", viewMatrix);
		g_Shaders.m_AnimationShader.SetUniformVectorMat4("uBoneMatrices", g_Renderables.m_StormTrooperModel.m_BoneMatrices);
		g_Shaders.m_AnimationShader.SetUniformVector3("viewPosition", g_CoreSystems.m_Camera.m_CameraPosition);
		g_Renderables.m_StormTrooperModel.Draw(0, animationTime, true, g_Shaders.m_AnimationShader, 1.0f, g_Renderables.m_StormTrooperPosition);

		g_Shaders.m_AnimationShader.SetUniformVectorMat4("uBoneMatrices", g_Renderables.m_GeodudeModel.m_BoneMatrices);
		g_Renderables.m_GeodudeModel.Draw(0, animationTime, true, g_Shaders.m_AnimationShader, 5.0f, g_Renderables.m_GeodudePosition);
		g_Shaders.m_AnimationShader.UnbindShader();
	}

	//Our Plane
	g_Textures.m_MarbleTexture.BindTexture();
	g_Renderables.m_Plane.DrawPrimitive(g_Shaders.m_StaticModelShader);

	//Our Queues
	g_Renderables.m_RenderQueue.RenderAllQueueItems(shader);

	//Redstone Lamp Model
	g_Shaders.m_PointLightObjectShader.UseShader();
	g_Shaders.m_PointLightObjectShader.SetUniformMat4("projection", projectionMatrix);
	g_Shaders.m_PointLightObjectShader.SetUniformMat4("view", viewMatrix);

	//Rerouting through the object's editor settings.
	g_Renderables.m_RedstoneLampModel.Draw(g_Shaders.m_PointLightObjectShader, false, 0, 0.01f, g_Renderables.m_PointLight.pointLightPosition);

	//=======================================================================================================================

	//Calculates the front vector from the Camera's (updated) Euler Angles.
	g_CoreSystems.m_Camera.UpdateCameraVectors();

	//Draw Cubemap
	g_RenderingComponents.m_Cubemap.DrawCubemap(viewMatrix, projectionMatrix);

	//Grass Texture
	g_Shaders.m_TransparentQuadShader.UseShader();
	g_Shaders.m_TransparentQuadShader.SetUniformMat4("projection", projectionMatrix);
	g_Shaders.m_TransparentQuadShader.SetUniformMat4("view", viewMatrix);

	for (unsigned int i = 0; i < g_Renderables.m_VegetableLocations.size(); i++)
	{
		glm::mat4 grassQuadMatrix = glm::mat4(1.0f);
		grassQuadMatrix = glm::translate(grassQuadMatrix, g_Renderables.m_VegetableLocations[i]);
		g_Renderables.m_TransparentQuad.DrawTransparentQuad(g_Shaders.m_TransparentQuadShader, grassQuadMatrix, g_Textures.m_GrassTexture);
	}

	glm::mat4 windowMatrix = glm::mat4(1.0f);
	windowMatrix = glm::translate(windowMatrix, glm::vec3(0.0f, 0.0f, 3.0f));
	g_Renderables.m_TransparentQuad.DrawTransparentQuad(g_Shaders.m_TransparentQuadShader, windowMatrix, g_Textures.m_WindowTexture);
}

void DrawEditorContent()
{
	g_CoreSystems.m_Editor.BeginEditorRenderLoop();
	g_CoreSystems.m_Editor.RenderDockingContext(); //This contains a Begin().

	ImGui::Begin("Global Settings");
	if (ImGui::Checkbox("Wireframe Rendering", &g_RenderingComponents.m_WireframeRendering))
	{
		g_CoreSystems.m_Renderer.ToggleWireframeRendering(g_RenderingComponents.m_WireframeRendering);
	}

	if (ImGui::Checkbox("Blinn Phong", &g_RenderingComponents.m_LightingModel[0]))
	{   //Perhaps it might be time for to pool our shaders together and update all their information in one go.
		g_Shaders.m_StaticModelShader.UseShader();
		g_Shaders.m_StaticModelShader.SetUniformBool("blinn", true);
		g_Shaders.m_StaticModelShader.UnbindShader();

		g_RenderingComponents.m_LightingModel[1] = false;
	}
	ImGui::SameLine();
	if (ImGui::Checkbox("Phong", &g_RenderingComponents.m_LightingModel[1]))
	{
		g_Shaders.m_StaticModelShader.UseShader();
		g_Shaders.m_StaticModelShader.SetUniformBool("blinn", false);
		g_Shaders.m_StaticModelShader.UnbindShader();

		g_RenderingComponents.m_LightingModel[0] = false;
	}

	if (ImGui::Checkbox("Soft Shadows", &g_RenderingComponents.m_SoftOrHardShadows[0]))
	{
		g_Shaders.m_StaticModelShader.UseShader();
		g_Shaders.m_StaticModelShader.SetUniformBool("softShadows", true);
		g_Shaders.m_StaticModelShader.UnbindShader();
		g_RenderingComponents.m_SoftOrHardShadows[1] = false;
	}
	ImGui::SameLine();
	if (ImGui::Checkbox("Hard Shadows", &g_RenderingComponents.m_SoftOrHardShadows[1]))
	{
		g_Shaders.m_StaticModelShader.UseShader();
		g_Shaders.m_StaticModelShader.SetUniformBool("softShadows", false);
		g_Shaders.m_StaticModelShader.UnbindShader();
		g_RenderingComponents.m_SoftOrHardShadows[0] = false;
	}

	if (ImGui::DragFloat("PCF Sample Amount", &g_RenderingComponents.pcfSampleAmount, 0.1f, 1.0f, 15.0f))
	{
		g_Shaders.m_StaticModelShader.UseShader();
		g_Shaders.m_StaticModelShader.SetUniformFloat("pcfSampleAmount", g_RenderingComponents.pcfSampleAmount);
		g_Shaders.m_StaticModelShader.UnbindShader();
	}

	ImGui::Image((void*)g_RenderingComponents.m_DepthMapFramebuffer.RetrieveDepthmapTextureID(), ImVec2{ 200.0f, 200.0f }, ImVec2{ 0, 1 }, ImVec2{ 1, 0 });

	ImGui::End();

	ImGui::Begin("Primitive Creation");
	if (ImGui::Button("Load 3D Model"))
	{

	}

	if (ImGui::Button("Create Plane"))
	{
		g_Renderables.m_RenderQueue.SubmitToRenderQueue(CrescentEngine::PrimitiveShape::PlanePrimitive);
	}

	if (ImGui::Button("Create Cube"))
	{
		g_Renderables.m_RenderQueue.SubmitToRenderQueue(CrescentEngine::PrimitiveShape::CubePrimitive);
	}
	ImGui::End();

	g_Renderables.m_RenderQueue.RenderAllQueueEditorSettings();
	g_Renderables.m_LightDirection.RenderSettingsInEditor();

	g_Renderables.m_PointLight.RenderSettingsInEditor();
	g_Renderables.m_BackpackModel.RenderSettingsInEditor(g_Renderables.m_BackpackModelPosition);
	g_Renderables.m_StormTrooperModel.RenderSettingsInEditor(g_Renderables.m_StormTrooperPosition);
	g_Renderables.m_GeodudeModel.RenderSettingsInEditor(g_Renderables.m_GeodudePosition);

	ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2{ 0, 0 });
	ImGui::Begin("Viewport");
	ImVec2 viewportPanelSize = ImGui::GetContentRegionAvail();
	g_CoreSystems.m_Editor.SetViewportSize(viewportPanelSize.x, viewportPanelSize.y); //The current size of our viewport.
	unsigned int colorAttachment = g_RenderingComponents.m_Framebuffer.RetrieveColorAttachment();
	ImGui::Image((void*)colorAttachment, { (float)g_CoreSystems.m_Editor.RetrieveViewportWidth(), (float)g_CoreSystems.m_Editor.RetrieveViewportHeight() }, ImVec2{ 0, 1 }, ImVec2{ 1, 0 });

	ImGui::End();
	ImGui::PopStyleVar(); //Pops the pushed style so other windows beyond this won't have the style's properties.

	ImGui::End(); //Closes the docking context.
	g_CoreSystems.m_Editor.EndEditorRenderLoop();
}

//Event Callbacks - To Be Further Abstracted.
void ProcessKeyboardEvents(GLFWwindow* window)
{
	if (glfwGetKey(window, GLFW_KEY_ESCAPE) == GLFW_PRESS)
	{
		g_CoreSystems.m_Window.TerminateWindow();
	}

	const float cameraSpeed = g_CoreSystems.m_Timestep.GetDeltaTimeInSeconds() * 2.5f;
	if (glfwGetKey(window, GLFW_KEY_W) == GLFW_PRESS)
	{
		g_CoreSystems.m_Camera.ProcessKeyboardEvents(CrescentEngine::CameraMovement::Forward, cameraSpeed);
	}
	if (glfwGetKey(window, GLFW_KEY_S) == GLFW_PRESS)
	{
		g_CoreSystems.m_Camera.ProcessKeyboardEvents(CrescentEngine::CameraMovement::Backward, cameraSpeed);
	}
	if (glfwGetKey(window, GLFW_KEY_A) == GLFW_PRESS)
	{
		g_CoreSystems.m_Camera.ProcessKeyboardEvents(CrescentEngine::CameraMovement::Left, cameraSpeed);
	}
	if (glfwGetKey(window, GLFW_KEY_D) == GLFW_PRESS)
	{
		g_CoreSystems.m_Camera.ProcessKeyboardEvents(CrescentEngine::CameraMovement::Right, cameraSpeed);
	}

	if (glfwGetKey(window, GLFW_KEY_SPACE) == GLFW_PRESS)
	{
		animationTime = 0.0f;
	}
}

void CameraAllowEulerCallback(GLFWwindow* window, int button, int action, int mods)
{
	if (button == GLFW_MOUSE_BUTTON_RIGHT && action == GLFW_PRESS)
	{
		CrescentEngine::g_CameraMode = true;
	}
	else
	{
		CrescentEngine::g_CameraMode = false;
	}
}

void CameraMovementCallback(GLFWwindow* window, double xPos, double yPos)
{
	if (CrescentEngine::g_IsCameraFirstMove)
	{
		CrescentEngine::g_CameraLastXPosition = xPos;
		CrescentEngine::g_CameraLastYPosition = yPos;
		CrescentEngine::g_IsCameraFirstMove = false;
	}

	float xOffset = xPos - CrescentEngine::g_CameraLastXPosition;
	float yOffset = CrescentEngine::g_CameraLastYPosition - yPos; //Reversed since Y Coordinates go from bottom to top.
	CrescentEngine::g_CameraLastXPosition = xPos;

	CrescentEngine::g_CameraLastYPosition = yPos;

	if (CrescentEngine::g_CameraMode)
	{
		g_CoreSystems.m_Camera.ProcessMouseMovement(xOffset, yOffset);
	}
}

void CameraZoomCallback(GLFWwindow* window, double xOffset, double yOffset)
{
	g_CoreSystems.m_Camera.ProcessMouseScroll(yOffset);
}

//This is a callback function that is called whenever a window is resized.
void FramebufferResizeCallback(GLFWwindow* window, int windowWidth, int windowHeight)
{
	//g_RenderingComponents.m_Framebuffer.ResizeFramebuffer(windowWidth, windowHeight);
	g_CoreSystems.m_Window.ResizeWindow((float)windowWidth, (float)windowHeight);
}
