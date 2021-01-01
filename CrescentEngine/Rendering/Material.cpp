#include "CrescentPCH.h"
#include "Material.h"

namespace CrescentEngine
{
	Material::Material()
	{

	}

	Material::Material(Shader* shader)
	{
		m_Shader = shader;
	}

	Material Material::Copy()
	{
		Material copiedMaterial(m_Shader);

		copiedMaterial.m_MaterialType = m_MaterialType;
		copiedMaterial.m_Color = m_Color;

		copiedMaterial.m_DepthTestEnabled = m_DepthTestEnabled;
		copiedMaterial.m_DepthWritingEnabled = m_DepthWritingEnabled;
		copiedMaterial.m_DepthTestComparisonFactor = m_DepthTestComparisonFactor;

		copiedMaterial.m_FaceCullingEnabled = m_FaceCullingEnabled;
		copiedMaterial.m_CulledFace = m_CulledFace;
		copiedMaterial.m_CullWindingOrder = m_CullWindingOrder;

		copiedMaterial.m_BlendingEnabled = m_BlendingEnabled;
		copiedMaterial.m_BlendSource = m_BlendSource;
		copiedMaterial.m_BlendDestination = m_BlendDestination;
		copiedMaterial.m_BlendEquation = m_BlendEquation;

		copiedMaterial.m_Uniforms = m_Uniforms;
		copiedMaterial.m_SamplerUniforms = m_SamplerUniforms;

		return copiedMaterial;
	}

	std::map<std::string, UniformValue>* Material::GetUniforms()
	{
		return &m_Uniforms;
	}

	std::map<std::string, UniformSamplerValue>* Material::GetSamplerUniforms()
	{
		return &m_SamplerUniforms;
	}

	void Material::SetShaderBool(const std::string& uniformName, const bool& value)
	{
		m_Uniforms[uniformName].m_UniformType = Shader_Type_Bool;
		m_Uniforms[uniformName].m_BoolValue = value;
	}

	void Material::SetShaderInt(const std::string& uniformName, const int& value)
	{
		m_Uniforms[uniformName].m_UniformType = Shader_Type_Int;
		m_Uniforms[uniformName].m_IntValue = value;
	}

	void Material::SetShaderFloat(const std::string& uniformName, const float& value)
	{
		m_Uniforms[uniformName].m_UniformType = Shader_Type_Float;
		m_Uniforms[uniformName].m_FloatValue = value;
	}

	void Material::SetShaderTexture(const std::string& uniformName, Texture2D* value, unsigned int textureUnit)
	{
		m_SamplerUniforms[uniformName].m_TextureUnit = textureUnit; 
		m_SamplerUniforms[uniformName].m_Texture = value;
		
		switch (value->m_TextureTarget)
		{
			case GL_TEXTURE_1D:
				m_SamplerUniforms[uniformName].m_UniformType = Shader_Type_Sampler1D;
				break;

			case GL_TEXTURE_2D:
				m_SamplerUniforms[uniformName].m_UniformType = Shader_Type_Sampler2D;
				break;

			case GL_TEXTURE_3D:
				m_SamplerUniforms[uniformName].m_UniformType = Shader_Type_Sampler3D;
				break;

			case GL_TEXTURE_CUBE_MAP:
				m_SamplerUniforms[uniformName].m_UniformType = Shader_Type_SamplerCube;
				break;
		}

		if (m_Shader) 
		{
			m_Shader->UseShader();
			m_Shader->SetUniformInteger(uniformName, textureUnit);
		}
	}

	void Material::SetShaderTextureCube(const std::string& uniformName, TextureCube* value, unsigned int textureUnit)
	{
		m_SamplerUniforms[uniformName].m_TextureUnit = textureUnit;
		m_SamplerUniforms[uniformName].m_UniformType = Shader_Type_SamplerCube;
		m_SamplerUniforms[uniformName].m_TextureCube = value;

		if (m_Shader)
		{
			m_Shader->UseShader();
			m_Shader->SetUniformInteger(uniformName, textureUnit);
		}
	}

	void Material::SetShaderVector2(const std::string& uniformName, const glm::vec2& value)
	{
		m_Uniforms[uniformName].m_UniformType = Shader_Type_Vec2;
		m_Uniforms[uniformName].m_Vector2Value = value;
	}

	void Material::SetShaderVector3(const std::string& uniformName, const glm::vec3& value)
	{
		m_Uniforms[uniformName].m_UniformType = Shader_Type_Vec3;
		m_Uniforms[uniformName].m_Vector3Value = value;
	}

	void Material::SetShaderVector3(const std::string& uniformName, const glm::vec4& value)
	{
		m_Uniforms[uniformName].m_UniformType = Shader_Type_Vec4;
		m_Uniforms[uniformName].m_Vector4Value = value;
	}

	void Material::SetShaderMat2(const std::string& uniformName, const glm::mat2& value)
	{
		m_Uniforms[uniformName].m_UniformType = Shader_Type_Mat2;
		m_Uniforms[uniformName].m_Mat2Value = value;
	}

	void Material::SetShaderMat3(const std::string& uniformName, const glm::mat3& value)
	{
		m_Uniforms[uniformName].m_UniformType = Shader_Type_Mat3;
		m_Uniforms[uniformName].m_Mat3Value = value;
	}

	void Material::SetShaderMat4(const std::string& uniformName, const glm::mat4& value)
	{
		m_Uniforms[uniformName].m_UniformType = Shader_Type_Mat4;
		m_Uniforms[uniformName].m_Mat4Value = value;
	}
}