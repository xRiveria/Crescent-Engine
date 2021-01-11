#include "CrescentPCH.h";
#include "Renderer.h";
#include "RenderQueue.h"
#include "GLStateCache.h"
#include "MaterialLibrary.h"
#include "../Models/DefaultPrimitives.h"
#include "../Utilities/Camera.h"
#include "../Scene/SceneEntity.h"
#include "../Models/Model.h"
#include "../Shading/Shader.h"
#include "../Shading/Material.h"
#include "../Lighting/DirectionalLight.h"
#include "../Lighting/PointLight.h"
#include "../Rendering/Resources.h"
#include <glm/gtc/type_ptr.hpp>

//As of now, our renderer only supports Forward Pass Rendering.

namespace Crescent
{
	Renderer::Renderer()
	{
	}

	Renderer::~Renderer()
	{
	}

	void Renderer::InitializeRenderer(const int& renderWindowWidth, const int& renderWindowHeight)
	{
		if (glewInit() != GLEW_OK)
		{
			CrescentError("Failed to initialize GLEW.")
		}
		CrescentInfo("Successfully initialized GLEW.");

		m_DeviceRendererInformation = (char*)glGetString(GL_RENDERER);
		m_DeviceVendorInformation = (char*)glGetString(GL_VENDOR);
		m_DeviceVersionInformation = (char*)glGetString(GL_VERSION);
		
		m_RenderWindowSize = glm::vec2(renderWindowWidth, renderWindowHeight);
		glViewport(0.0f, 0.0f, renderWindowWidth, renderWindowHeight);
		glClearDepth(1.0f);
		glClearColor(0.0f, 0.0f, 0.0f, 1.0f);

		//Core Primitives
		m_NDCQuad = new Quad();
		m_DebugLightMesh = new Sphere(16, 16);
		m_DeferredPointLightMesh = new Sphere(16, 16);

		//Core Systems
		m_RenderQueue = new RenderQueue(this);
		m_GLStateCache = new GLStateCache();
		m_MaterialLibrary = new MaterialLibrary(m_GBuffer);

		//Configure Default OpenGL State
		m_GLStateCache->ToggleDepthTesting(true);
		m_GLStateCache->ToggleFaceCulling(true);

		//Render Targets
		m_MainRenderTarget = new RenderTarget(1, 1, GL_FLOAT, 1, true);
		m_GBuffer = new RenderTarget(1, 1, GL_HALF_FLOAT, 4, true);
		m_CustomRenderTarget = new RenderTarget(1, 1, GL_HALF_FLOAT, 1, true);
		m_PostProcessRenderTarget = new RenderTarget(1, 1, GL_UNSIGNED_BYTE, 1, false);

		//Shadows
		for (int i = 0; i < 4; i++) //Allow for up to a total of 4 directional/spot shadow casters.
		{
			RenderTarget* renderTarget = new RenderTarget(2048, 2048, GL_UNSIGNED_BYTE, 1, true);
			renderTarget->m_DepthAndStencilAttachment.BindTexture();
			renderTarget->m_DepthAndStencilAttachment.SetMinificationFilter(GL_NEAREST);
			renderTarget->m_DepthAndStencilAttachment.SetMagnificationFilter(GL_NEAREST);
			renderTarget->m_DepthAndStencilAttachment.SetWrappingMode(GL_CLAMP_TO_BORDER);
			float borderColor[] = { 1.0f, 1.0f, 1.0f, 1.0f };
			glTexParameterfv(GL_TEXTURE_2D, GL_TEXTURE_BORDER_COLOR, borderColor);
			m_ShadowRenderTargets.push_back(renderTarget);
		}

		//Temporary
		m_PostProcessShader = Resources::LoadShader("Post Processing", "Resources/Shaders/ScreenQuadVertex.shader", "Resources/Shaders/PostProcessingFragment.shader");
		m_PostProcessShader->UseShader();
		m_PostProcessShader->SetUniformInteger("TexSource", 0);
		m_PostProcessShader->SetUniformInteger("TexBloom1", 1);
		m_PostProcessShader->SetUniformInteger("TexBloom2", 2);
		m_PostProcessShader->SetUniformInteger("TexBloom3", 3);
		m_PostProcessShader->SetUniformInteger("TexBloom4", 4);
		m_PostProcessShader->SetUniformInteger("gMotion", 5);
		//Global Uniform Buffer Object
		//glGenBuffers(1, &m_GlobalUniformBufferID);
		//glBindBuffer(GL_UNIFORM_BUFFER, m_GlobalUniformBufferID);
		//glBufferData(GL_UNIFORM_BUFFER, 720, nullptr, GL_STREAM_DRAW);
		//glBindBufferBase(GL_UNIFORM_BUFFER, 0, m_GlobalUniformBufferID);
	}

	void Renderer::PushToRenderQueue(SceneEntity* sceneEntity)
	{
		//Update transform before pushing node to render command buffer.
		sceneEntity->UpdateEntityTransform(true);

		m_RenderQueue->PushToRenderQueue(sceneEntity->m_Mesh, sceneEntity->m_Material, sceneEntity->RetrieveEntityTransform());
	}

	//Attach shader to material.
	void Renderer::RenderAllQueueItems()
	{
		glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);

		//Update Global Uniform Buffer Object
		UpdateGlobalUniformBufferObjects();

		//Set default OpenGL state.
		m_GLStateCache->ToggleBlending(false);
		m_GLStateCache->ToggleFaceCulling(true);
		m_GLStateCache->SetCulledFace(GL_BACK);
		m_GLStateCache->ToggleDepthTesting(true);
		m_GLStateCache->SetDepthFunction(GL_LESS);
		
		//1) Geometry Buffer
		std::vector<RenderCommand> deferredRenderCommands = m_RenderQueue->RetrieveDeferredRenderingCommands();
		glViewport(0, 0, m_RenderWindowSize.x, m_RenderWindowSize.y);
		glBindFramebuffer(GL_FRAMEBUFFER, m_GBuffer->m_FramebufferID);
		unsigned int attachments[4] = { GL_COLOR_ATTACHMENT0, GL_COLOR_ATTACHMENT1, GL_COLOR_ATTACHMENT2, GL_COLOR_ATTACHMENT3 };
		glDrawBuffers(4, attachments);
		glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
		m_GLStateCache->SetPolygonMode(m_WireframesEnabled ? GL_LINE : GL_FILL);

		for (int i = 0; i < deferredRenderCommands.size(); i++)
		{
			RenderCustomCommand(&deferredRenderCommands[i], nullptr, false);
		}
		m_GLStateCache->SetPolygonMode(GL_FILL);

		//Disable for next pass (shadow map generation).
		attachments[1] = GL_NONE;
		attachments[2] = GL_NONE;
		attachments[3] = GL_NONE;
		glDrawBuffers(4, attachments);

		//2) Render All Shadow Casters to Light Shadow Buffers
		if (m_ShadowsEnabled)
		{
			m_GLStateCache->SetCulledFace(GL_FRONT);
			std::vector<RenderCommand> shadowRenderCommands = m_RenderQueue->RetrieveShadowCastingRenderCommands();

			unsigned int shadowRenderTargetIndex = 0;
			for (int i = 0; i < m_DirectionalLights.size(); i++) //We usually have 1 directional light source.
			{
				DirectionalLight* directionalLight = m_DirectionalLights[i];
				if (directionalLight->m_ShadowCastingEnabled)
				{
					m_MaterialLibrary->m_DirectionalShadowShader->UseShader();

					glBindFramebuffer(GL_FRAMEBUFFER, m_ShadowRenderTargets[shadowRenderTargetIndex]->m_FramebufferID);
					glViewport(0, 0, m_ShadowRenderTargets[shadowRenderTargetIndex]->m_FramebufferWidth, m_ShadowRenderTargets[shadowRenderTargetIndex]->m_FramebufferHeight);
					glClear(GL_DEPTH_BUFFER_BIT);

					glm::mat4 lightProjectionMatrix = glm::ortho(-20.0f, 20.0f, -20.0f, 20.0f, -15.0f, 20.0f);
					glm::mat4 lightViewMatrix = glm::lookAt(-directionalLight->m_LightDirection * 10.0f, glm::vec3(0.0f), glm::vec3(0.0f, 1.0f, 0.0f));

					m_DirectionalLights[i]->m_LightSpaceViewProjectionMatrix = lightProjectionMatrix * lightViewMatrix;
					m_DirectionalLights[i]->m_ShadowMapRenderTarget = m_ShadowRenderTargets[shadowRenderTargetIndex];

					//This varies based on the amount of objects in our scene that can cast shadows on objects. This filtered whenever we submit commands into the render queue.
					//By default, all physical objects in the scene can cast and receive shadows.
					for (int j = 0; j < shadowRenderCommands.size(); j++) 
					{
						RenderShadowCastCommand(&shadowRenderCommands[j], lightProjectionMatrix, lightViewMatrix);
					}
					shadowRenderTargetIndex++;
				}
			}
			m_GLStateCache->SetCulledFace(GL_BACK);
		}
		attachments[0] = GL_COLOR_ATTACHMENT0;
		glDrawBuffers(4, attachments);

		//3) Do post-processing steps before lighting stage.

		//4) Render deferred shader for each light (full quad for directional, spheres for point lights).
		glBindFramebuffer(GL_FRAMEBUFFER, m_CustomRenderTarget->m_FramebufferID);
		glViewport(0, 0, m_CustomRenderTarget->m_FramebufferWidth, m_CustomRenderTarget->m_FramebufferHeight);
		glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT | GL_STENCIL_BUFFER_BIT);

		m_GLStateCache->ToggleDepthTesting(false);
		m_GLStateCache->ToggleBlending(true);
		m_GLStateCache->SetBlendingFunction(GL_ONE, GL_ONE);

		//Binds our color buffers to the respective texture slots. Remember that Texture Slot 0 (Position), 1 (Normals) and 2 (Albedo) are always used for our GBuffer outputs. 
		m_GBuffer->RetrieveColorAttachment(0)->BindTexture(0);
		m_GBuffer->RetrieveColorAttachment(1)->BindTexture(1);
		m_GBuffer->RetrieveColorAttachment(2)->BindTexture(2);

		///Ambient Lighting

		if (m_LightsEnabled)
		{
			//Directional Lights
			for (auto iterator = m_DirectionalLights.begin(); iterator != m_DirectionalLights.end(); iterator++)
			{
				RenderDeferredDirectionalLight(*iterator);
			}
			
			//Point Lights
			m_GLStateCache->SetCulledFace(GL_FRONT);
			for (auto iterator = m_PointLights.begin(); iterator != m_PointLights.end(); iterator++) //Remember that our objects are stored as pointers, thus the dereference.
			{
				///Frustrum Check.
				RenderDeferredPointLight(*iterator);
			}
			m_GLStateCache->SetCulledFace(GL_BACK);
		}

		m_GLStateCache->ToggleDepthTesting(true);
		m_GLStateCache->SetBlendingFunction(GL_ONE, GL_ONE_MINUS_SRC_ALPHA);
		m_GLStateCache->ToggleBlending(false);

		//5) Blit Depth Framebuffer to Default for Rendering
		glBindFramebuffer(GL_READ_FRAMEBUFFER, m_GBuffer->m_FramebufferID);
		glBindFramebuffer(GL_DRAW_FRAMEBUFFER, m_CustomRenderTarget->m_FramebufferID); //Write to our default framebuffer.
		glBlitFramebuffer(0, 0, m_GBuffer->m_FramebufferWidth, m_GBuffer->m_FramebufferHeight, 0, 0, m_RenderWindowSize.x, m_RenderWindowSize.y, GL_DEPTH_BUFFER_BIT, GL_NEAREST);
		
		//6) Custom Forward Render Pass
		m_RenderTargetsCustom.push_back(nullptr);
		for (unsigned int targetIndex = 0; targetIndex < m_RenderTargetsCustom.size(); targetIndex++)
		{
			RenderTarget* renderTarget = m_RenderTargetsCustom[targetIndex];
			if (renderTarget)
			{
				glViewport(0, 0, renderTarget->m_FramebufferWidth, renderTarget->m_FramebufferHeight);
				glBindFramebuffer(GL_FRAMEBUFFER, renderTarget->m_FramebufferID);
				if (renderTarget->m_HasDepthAndStencilAttachments)
				{
					glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT | GL_STENCIL_BUFFER_BIT);
				}
				else
				{
					glClear(GL_COLOR_BUFFER_BIT);
				}
				m_Camera->m_ProjectionMatrix = glm::perspective(glm::radians(m_Camera->m_MouseZoom), (float)renderTarget->m_FramebufferWidth / (float)renderTarget->m_FramebufferHeight, 0.2f, 100.0f);
			}
			else
			{
				//Don't render to default framebuffer, but to custom target framebuffer which we will use for postprocessing.
				glViewport(0, 0, m_RenderWindowSize.x, m_RenderWindowSize.y);
				glBindFramebuffer(GL_FRAMEBUFFER, m_CustomRenderTarget->m_FramebufferID);
				m_Camera->m_ProjectionMatrix = glm::perspective(glm::radians(m_Camera->m_MouseZoom), (float)m_RenderWindowSize.x / (float)m_RenderWindowSize.y, 0.2f, 100.0f);
			}

			///Render custom commands here. (Things with custom material).
			//std::vector<RenderCommand> renderCommands = m_RenderQueue->
		}

		//7) Alpha Material Pass
		//glViewport(0, 0, m_RenderWindowSize.x, m_RenderWindowSize.y);
		//glBindFramebuffer(GL_FRAMEBUFFER, m_CustomRenderTarget->m_FramebufferID);
		///Alpha Render Commands Here.

		//Render Light Mesh (as visual cue), if requested.
		for (auto iterator = m_PointLights.begin(); iterator != m_PointLights.end(); iterator++)
		{
			if ((*iterator)->m_RenderMesh)
			{
				m_MaterialLibrary->m_DebugLightMaterial->SetShaderVector3("lightColor", (*iterator)->m_LightColor * (*iterator)->m_LightIntensity * 0.25f);
			
				RenderCommand renderCommand;
				renderCommand.m_Material = m_MaterialLibrary->m_DebugLightMaterial;
				renderCommand.m_Mesh = m_DebugLightMesh;

				glm::mat4 lightModelMatrix = glm::mat4(1.0f); //Not the light space matrix. Just our model matrix for the light itself.
				lightModelMatrix = glm::translate(lightModelMatrix, (*iterator)->m_LightPosition);
				lightModelMatrix = glm::scale(lightModelMatrix, glm::vec3(0.25f));
				renderCommand.m_Transform = lightModelMatrix;

				RenderCustomCommand(&renderCommand, nullptr);
			}
		}

		//8) Pody-Processing Stage after all lighting calculations.

		//9) Render Debug Visuals
		glViewport(0, 0, m_RenderWindowSize.x, m_RenderWindowSize.y);
		glBindFramebuffer(GL_FRAMEBUFFER, m_CustomRenderTarget->m_FramebufferID);
		if (m_ShowDebugLightVolumes)
		{
			m_GLStateCache->SetPolygonMode(GL_LINE);
			m_GLStateCache->SetCulledFace(GL_FRONT);
			for (auto iterator = m_PointLights.begin(); iterator != m_PointLights.end(); iterator++)
			{
				m_MaterialLibrary->m_DebugLightMaterial->SetShaderVector3("lightColor", (*iterator)->m_LightColor);

				RenderCommand renderCommand;
				renderCommand.m_Material = m_MaterialLibrary->m_DebugLightMaterial;
				renderCommand.m_Mesh = m_DebugLightMesh;

				glm::mat4 lightDebugModelMatrix = glm::mat4(1.0f); //Not the light space matrix. Just our model matrix for the light debug mesh itself.
				lightDebugModelMatrix = glm::translate(lightDebugModelMatrix, (*iterator)->m_LightPosition);
				lightDebugModelMatrix = glm::scale(lightDebugModelMatrix, glm::vec3((*iterator)->m_LightRadius));
				renderCommand.m_Transform = lightDebugModelMatrix;

				RenderCustomCommand(&renderCommand, nullptr);
			}
			m_GLStateCache->SetPolygonMode(GL_FILL);
			m_GLStateCache->SetCulledFace(GL_BACK);
		}

		//10) Custom Post Processing Pass


		//11) Finally, Blit everything to our framebuffer for rendering.
		std::vector<RenderCommand> postProcessingCommands = m_RenderQueue->RetrievePostProcessingRenderCommands();
		BlitToMainFramebuffer(m_CustomRenderTarget->RetrieveColorAttachment(0));

		m_RenderQueue->ClearQueuedCommands();

		glBindFramebuffer(GL_FRAMEBUFFER, 0);
	}

	void Renderer::BlitToMainFramebuffer(Texture* sourceRenderTarget)
	{
		glBindFramebuffer(GL_FRAMEBUFFER, m_MainRenderTarget->m_FramebufferID);
		glViewport(0, 0, m_RenderWindowSize.x, m_RenderWindowSize.y);
		glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT | GL_STENCIL_BUFFER_BIT);

		//Bind Input Texture Data
		sourceRenderTarget->BindTexture(0);
		//Bloom
		m_GBuffer->RetrieveColorAttachment(3)->BindTexture(5);

		m_PostProcessShader->UseShader();
		RenderMesh(m_NDCQuad);
	}

	//Renders from the light's point of view. 
	void Renderer::RenderShadowCastCommand(RenderCommand* renderCommand, const glm::mat4& lightSpaceProjectionMatrix, const glm::mat4& lightSpaceViewMatrix)
	{
		Shader* shadowShader = m_MaterialLibrary->m_DirectionalShadowShader;
		shadowShader->UseShader();

		shadowShader->SetUniformMat4("lightSpaceProjection", lightSpaceProjectionMatrix);
		shadowShader->SetUniformMat4("lightSpaceView", lightSpaceViewMatrix);
		shadowShader->SetUniformMat4("model", renderCommand->m_Transform);

		RenderMesh(renderCommand->m_Mesh);
	}

	void Renderer::RenderDeferredDirectionalLight(DirectionalLight* directionalLight)
	{
		//We also have to update the global uniform buffer for this.
		Shader* directionalShader = m_MaterialLibrary->m_DeferredDirectionalLightShader;

		directionalShader->UseShader();
		directionalShader->SetUniformVector3("cameraPosition", m_Camera->m_CameraPosition);
		directionalShader->SetUniformVector3("lightDirection", directionalLight->m_LightDirection);
		directionalShader->SetUniformVector3("lightColor", glm::normalize(directionalLight->m_LightColor) * directionalLight->m_LightIntensity);
		directionalShader->SetUniformBool("ShadowsEnabled", m_ShadowsEnabled);

		if (directionalLight->m_ShadowMapRenderTarget)
		{
			directionalShader->SetUniformMat4("lightShadowViewProjection", directionalLight->m_LightSpaceViewProjectionMatrix);
			directionalLight->m_ShadowMapRenderTarget->RetrieveDepthAndStencilAttachment()->BindTexture(3); //In our material library, we set the shadow map sampler to be in texture slot 3.
		}

		RenderMesh(m_NDCQuad);
	}

	void Renderer::RenderDeferredPointLight(PointLight* pointLight)
	{
		Shader* pointLightShader = m_MaterialLibrary->m_DeferredPointLightShader;

		pointLightShader->UseShader();
		pointLightShader->SetUniformVector3("cameraPosition", m_Camera->m_CameraPosition);
		pointLightShader->SetUniformVector3("lightPosition", pointLight->m_LightPosition);
		pointLightShader->SetUniformFloat("lightRadius", pointLight->m_LightRadius);
		pointLightShader->SetUniformVector3("lightColor", glm::normalize(pointLight->m_LightColor) * pointLight->m_LightIntensity);

		glm::mat4 pointLightModelMatrix = glm::mat4(1.0f);
		pointLightModelMatrix = glm::translate(pointLightModelMatrix, pointLight->m_LightPosition);
		pointLightModelMatrix = glm::scale(pointLightModelMatrix, glm::vec3(pointLight->m_LightRadius));

		pointLightShader->SetUniformMat4("projection", m_Camera->m_ProjectionMatrix);
		pointLightShader->SetUniformMat4("view", m_Camera->GetViewMatrix());
		pointLightShader->SetUniformMat4("model", pointLightModelMatrix);

		RenderMesh(m_DeferredPointLightMesh);
	}

	void Renderer::RenderCustomCommand(RenderCommand* renderCommand, Camera* customRenderCamera, bool updateGLStates)
	{
		Mesh* mesh = renderCommand->m_Mesh;
		Material* material = renderCommand->m_Material;

		//Update global OpenGL states based on the material.
		if (updateGLStates)
		{
			m_GLStateCache->ToggleBlending(material->m_BlendingEnabled);
			if (material->m_BlendingEnabled)
			{
				m_GLStateCache->SetBlendingFunction(material->m_BlendSource, material->m_BlendDestination);
			}
			m_GLStateCache->ToggleDepthTesting(material->m_DepthTestEnabled);
			m_GLStateCache->SetDepthFunction(material->m_DepthTestFunction);

			m_GLStateCache->ToggleFaceCulling(material->m_FaceCullingEnabled);
			m_GLStateCache->SetCulledFace(material->m_CulledFace);
		}

		//Default uniforms that are always configured regardless of shader configuration. See these as a set of default shader variables that are always there.
		///To implement with Uniform Buffer Objects.
		material->RetrieveMaterialShader()->UseShader();
		material->RetrieveMaterialShader()->SetUniformMat4("projection", m_Camera->m_ProjectionMatrix);
		material->RetrieveMaterialShader()->SetUniformMat4("view", m_Camera->GetViewMatrix());
		material->RetrieveMaterialShader()->SetUniformVector3("cameraPosition", m_Camera->m_CameraPosition);

		if (customRenderCamera) //If a custom camera is defined, we will update our shader uniforms with its information as needed.
		{

		}

		//==============================================
		material->RetrieveMaterialShader()->SetUniformMat4("model", renderCommand->m_Transform);

		///Shadow Related Stuff. Create Shaders for relevant stuff in Material Library.
		material->RetrieveMaterialShader()->SetUniformBool("ShadowsEnabled", m_ShadowsEnabled); //If global shadows are enabled.
		if (m_ShadowsEnabled && material->m_MaterialType == Material_Custom && material->m_ShadowReceiving) //If the mesh in question should receive shadows...
		{
			for (int i = 0; i < m_DirectionalLights.size(); i++)
			{
				if (m_DirectionalLights[i]->m_ShadowMapRenderTarget != nullptr)
				{
					material->RetrieveMaterialShader()->SetUniformMat4("lightShadowViewProjection" + std::to_string(i + 1), m_DirectionalLights[i]->m_LightSpaceViewProjectionMatrix);
					m_DirectionalLights[i]->m_ShadowMapRenderTarget->RetrieveDepthAndStencilAttachment()->BindTexture(10 + i);
				}
			}
		}

		//Bind and set active uniform sampler/texture objects.
		auto* samplers = material->GetSamplerUniforms(); //Returns a map of a string (uniform name) and its corresponding uniform information.
		for (auto iterator = samplers->begin(); iterator != samplers->end(); iterator++)
		{
			iterator->second.m_Texture->BindTexture(iterator->second.m_TextureUnit);
		}

		//Set uniform states of material.
		auto* uniforms = material->GetUniforms(); //Returns a map of a string (uniform name) and its corresponding uniform information.
		for (auto iterator = uniforms->begin(); iterator != uniforms->end(); iterator++)
		{
			switch (iterator->second.m_UniformType)
			{
			case Shader_Type_Boolean:
				material->RetrieveMaterialShader()->SetUniformBool(iterator->first, iterator->second.m_BoolValue);
				break;
			case Shader_Type_Integer:
				material->RetrieveMaterialShader()->SetUniformInteger(iterator->first, iterator->second.m_IntValue);
				break;
			case Shader_Type_Float:
				material->RetrieveMaterialShader()->SetUniformFloat(iterator->first, iterator->second.m_FloatValue);
				break;
			//case Shader_Type_Vec2:
				//material->RetrieveMaterialShader()->SetUniformVector2(it->first, it->second.Vec2);
				//break;
			case Shader_Type_Vector3:
				material->RetrieveMaterialShader()->SetUniformVector3(iterator->first, iterator->second.m_Vector3Value);
				break;
			//case Shader_Type_Vec4:
				//material->RetrieveMaterialShader()->SetVector(it->first, it->second.Vec4);
				//break;
			//case Shader_Type_Mat2:
				//material->RetrieveMaterialShader()->SetMatrix(it->first, it->second.Mat2);
				//break;
			//case Shader_Type_Mat3:
				//material->RetrieveMaterialShader()->SetMatrix(it->first, it->second.Mat3);
				//break;
			case Shader_Type_Matrix4:
				material->RetrieveMaterialShader()->SetUniformMat4(iterator->first, iterator->second.m_Mat4Value);
				break;
			default:
				CrescentError("You tried to set an unidentified uniform data type and value. Please check."); //Include which shader.
				break;
			}
		}

		RenderMesh(renderCommand->m_Mesh);
	}

	void Renderer::RenderMesh(Mesh* mesh)
	{
		glBindVertexArray(mesh->RetrieveVertexArrayID()); //Binding will automatically fill the vertex array with the attributes allocated during its time. 
		if (mesh->m_Indices.size() > 0)
		{
			glDrawElements(mesh->m_Topology == TriangleStrips ? GL_TRIANGLE_STRIP : GL_TRIANGLES, mesh->m_Indices.size(), GL_UNSIGNED_INT, 0);
		}
		else
		{
			glDrawArrays(mesh->m_Topology == TriangleStrips ? GL_TRIANGLE_STRIP : GL_TRIANGLES, 0, mesh->m_Positions.size());
		}
	}

	void Renderer::SetRenderingWindowSize(const int& newWidth, const int& newHeight)
	{
		m_RenderWindowSize = glm::vec2((float)newWidth, (float)newHeight);
		m_GBuffer->ResizeRenderTarget(newWidth, newHeight);
		m_CustomRenderTarget->ResizeRenderTarget(newWidth, newHeight);
		m_PostProcessRenderTarget->ResizeRenderTarget(newWidth, newHeight);
		m_MainRenderTarget->ResizeRenderTarget(newWidth, newHeight);
	}

	RenderTarget* Renderer::RetrieveMainRenderTarget()
	{
		return m_MainRenderTarget;
	}

	RenderTarget* Renderer::RetrieveGBuffer()
	{
		return m_GBuffer;
	}

	RenderTarget* Renderer::RetrieveShadowRenderTarget(int index)
	{
		return m_ShadowRenderTargets[index];
	}

	RenderTarget* Renderer::RetrieveCustomRenderTarget()
	{
		return m_CustomRenderTarget;
	}

	void Renderer::SetSceneCamera(Camera* sceneCamera)
	{
		m_Camera = sceneCamera;
	}

	Camera* Renderer::RetrieveSceneCamera()
	{
		return m_Camera;
	}

	Material* Renderer::CreateMaterial(std::string shaderName)
	{
		return m_MaterialLibrary->CreateMaterial(shaderName);
	}

	void Renderer::AddLightSource(DirectionalLight* directionalLight)
	{
		m_DirectionalLights.push_back(directionalLight);
	}

	void Renderer::AddLightSource(PointLight* pointLight)
	{
		m_PointLights.push_back(pointLight);
	}

	void Renderer::UpdateGlobalUniformBufferObjects()
	{
		//glBindBuffer(GL_UNIFORM_BUFFER, m_GlobalUniformBufferID);
		//For now, we will update the global uniforms here. 
		//glBufferSubData(GL_UNIFORM_BUFFER, sizeof(glm::mat4), sizeof(glm::mat4), glm::value_ptr(m_Camera->m_ProjectionMatrix);
		//glBufferSubData(GL_UNIFORM_BUFFER, sizeof(glm::mat4), sizeof(glm::mat4), glm::value_ptr(m_Camera->GetViewMatrix()));
	}
}