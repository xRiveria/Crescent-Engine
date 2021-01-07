#pragma once
#include <GL/glew.h>
#include <map>
#include <vector>

namespace Crescent
{
	class Texture;
	class Shader;
	/*
		Global resource manager. This class manages and maintains all resource memory used throughout the rendering application.
		New resources are loaded from here, and duplicate resouce loads are prevented. Every resource is referenced by a hashed string ID.
	*/

	class Resources
	{
	public:
		static void InitializeResourceManager();
		static void Clean();

		//Shader Resources
		static Shader* LoadShader(const std::string& name, const std::string& vertexShaderPath, const std::string& fragmentShaderPath);
		static Shader* RetrieveShader(const std::string& name);

		//Texture Resources
		static Texture* LoadTexture(const std::string& name, const std::string& filePath, GLenum textureTarget = GL_TEXTURE_2D, GLenum textureFormat = GL_RGBA, bool srgb = false);

	private:
		//Disallow creation of any Resources object. This is a static object.
		Resources();

	private:
		//We index all resources with a hashed string ID.
		static std::map<unsigned int, Shader> m_Shaders;
		static std::map<unsigned int, Texture> m_Textures;

	};
}