#include "CrescentPCH.h"
#include "Mesh.h"
#include "GL/glew.h"

namespace Crescent
{
	Mesh::Mesh()
	{

	}

	Mesh::Mesh(std::vector<glm::vec3> positions, std::vector<unsigned int> indices)
	{
		m_Positions = positions;
		m_Indices = indices;
	}

	Mesh::Mesh(std::vector<glm::vec3> positions, std::vector<glm::vec2> uv, std::vector<unsigned int> indices)
	{
		m_Positions = positions;
		m_UV = uv;
		m_Indices = indices;
	}

	Mesh::Mesh(std::vector<glm::vec3> positions, std::vector<glm::vec2> uv, std::vector<glm::vec3> normals, std::vector<unsigned int> indices)
	{
		m_Positions = positions;
		m_UV = uv;
		m_Normals = normals;
		m_Indices = indices;
	}

	Mesh::Mesh(std::vector<glm::vec3> positions, std::vector<glm::vec2> uv, std::vector<glm::vec3> normals, std::vector<glm::vec3> tangents, std::vector<glm::vec3> bitangents, std::vector<unsigned int> indices)
	{
		m_Positions = positions;
		m_UV = uv;
		m_Normals = normals;
		m_Tangents = tangents;
		m_Bitangents = bitangents;
		m_Indices = indices;
	}

	void Mesh::FinalizeMesh(bool interleaved)
	{
		//Initialize IDs if not configured before.
		if (!m_VertexArrayID)
		{
			glGenVertexArrays(1, &m_VertexArrayID);
			glGenBuffers(1, &m_VertexBufferID);
			glGenBuffers(1, &m_IndexBufferID);
		}

		//Preprocess buffer data.
		std::vector<float> bufferData;

		if (interleaved)
		{
			for (int i = 0; i < m_Positions.size(); i++)
			{
				bufferData.push_back(m_Positions[i].x);
				bufferData.push_back(m_Positions[i].y);
				bufferData.push_back(m_Positions[i].z);
				if (m_UV.size() > 0)
				{
					bufferData.push_back(m_UV[i].x);
					bufferData.push_back(m_UV[i].y);
				}
				if (m_Normals.size() > 0)
				{
					bufferData.push_back(m_Normals[i].x);
					bufferData.push_back(m_Normals[i].y);
					bufferData.push_back(m_Normals[i].z);
				}
				if (m_Tangents.size() > 0)
				{
					bufferData.push_back(m_Tangents[i].x);
					bufferData.push_back(m_Tangents[i].y);
					bufferData.push_back(m_Tangents[i].z);
				}
				if (m_Bitangents.size() > 0)
				{
					bufferData.push_back(m_Bitangents[i].x);
					bufferData.push_back(m_Bitangents[i].z);
					bufferData.push_back(m_Bitangents[i].y);
				}
			}
		}
		else
		{
			//If any of the float arrays are empty, data won't be filled by them.
			for (int i = 0; i < m_Positions.size(); i++)
			{
				bufferData.push_back(m_Positions[i].x);
				bufferData.push_back(m_Positions[i].y);
				bufferData.push_back(m_Positions[i].z);
			}
			for (int i = 0; i < m_UV.size(); i++)
			{
				bufferData.push_back(m_UV[i].x);
				bufferData.push_back(m_UV[i].y);
			}
			for (int i = 0; i < m_Normals.size(); i++)
			{
				bufferData.push_back(m_Normals[i].x);
				bufferData.push_back(m_Normals[i].y);
				bufferData.push_back(m_Normals[i].z);
			}
			for (int i = 0; i < m_Tangents.size(); i++)
			{
				bufferData.push_back(m_Tangents[i].x);
				bufferData.push_back(m_Tangents[i].y);
				bufferData.push_back(m_Tangents[i].z);
			}
			for (int i = 0; i < m_Bitangents.size(); i++)
			{
				bufferData.push_back(m_Bitangents[i].x);
				bufferData.push_back(m_Bitangents[i].y);
				bufferData.push_back(m_Bitangents[i].z);
			}
		}

		//Configure vertex attributes only if vertex data size is more than 0.
		glBindVertexArray(m_VertexArrayID);
		glBindBuffer(GL_ARRAY_BUFFER, m_VertexBufferID);
		glBufferData(GL_ARRAY_BUFFER, bufferData.size() * sizeof(float), &bufferData[0], GL_STATIC_DRAW);
		//Only fill the index buffer if the index array is not empty.
		if (m_Indices.size() > 0)
		{
			glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, m_IndexBufferID);
			glBufferData(GL_ELEMENT_ARRAY_BUFFER, m_Indices.size(), &m_Indices[0], GL_STATIC_DRAW);
		}
		if (interleaved)
		{
			//Calculate stride from number of non-empty vertex attribute arrays. Remember that stride is the total amount of information owned by a single vertex.
			size_t stride = 3 * sizeof(float); //Positions
			if (m_UV.size() > 0) stride += 2 * sizeof(float);
			if (m_Normals.size() > 0) stride += 3 * sizeof(float);
			if (m_Tangents.size() > 0) stride += 3 * sizeof(float);
			if (m_Bitangents.size() > 0) stride += 3 * sizeof(float);

			size_t offset = 0;
			glEnableVertexAttribArray(0);
			glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, stride, (GLvoid*)offset);
			offset = 3 * sizeof(float);

			if (m_UV.size() > 0)
			{
				glEnableVertexAttribArray(1);
				glVertexAttribPointer(1, 2, GL_FLOAT, GL_FALSE, stride, (GLvoid*)offset);
				offset += 2 * sizeof(float);
			}
			if (m_Normals.size() > 0)
			{
				glEnableVertexAttribArray(2);
				glVertexAttribPointer(2, 3, GL_FLOAT, GL_FALSE, stride, (GLvoid*)offset);
				offset += 3 * sizeof(float);
			}
			if (m_Tangents.size() > 0)
			{
				glEnableVertexAttribArray(3);
				glVertexAttribPointer(3, 3, GL_FLOAT, GL_FALSE, stride, (GLvoid*)offset);
				offset += 3 * sizeof(float);
			}
			if (m_Bitangents.size() > 0)
			{
				glEnableVertexAttribArray(4);
				glVertexAttribPointer(4, 3, GL_FLOAT, GL_FALSE, stride, (GLvoid*)offset);
				offset += 3 * sizeof(float);
			}
		}
		else
		{
			size_t offset = 0;
			glEnableVertexAttribArray(0);
			glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, 0, (GLvoid*)offset);
			offset += m_Positions.size() * sizeof(float);

			if (m_UV.size() > 0)
			{
				glEnableVertexAttribArray(1);
				glVertexAttribPointer(1, 2, GL_FLOAT, GL_FALSE, 0, (GLvoid*)offset);
				offset += m_UV.size() * sizeof(float);
			}
			if (m_Normals.size() > 0)
			{
				glEnableVertexAttribArray(2);
				glVertexAttribPointer(2, 3, GL_FLOAT, GL_FALSE, 0, (GLvoid*)offset);
				offset += m_Normals.size() * sizeof(float);
			}
			if (m_Tangents.size() > 0)
			{
				glEnableVertexAttribArray(3);
				glVertexAttribPointer(3, 3, GL_FLOAT, GL_FALSE, 0, (GLvoid*)offset);
				offset += m_Tangents.size() * sizeof(float);
			}
			if (m_Bitangents.size() > 0)
			{
				glEnableVertexAttribArray(4);
				glVertexAttribPointer(4, 3, GL_FLOAT, GL_FALSE, 0, (GLvoid*)offset);
				offset += m_Bitangents.size() * sizeof(float);
			}
		}
		glBindVertexArray(0);
	}	

	//==================================================================================================================

	Mesh::Mesh(std::vector<Vertex> vertices, std::vector<unsigned int> indices, std::vector<MeshTexture> textures)
	{
		this->vertices = vertices;
		this->indices = indices;
		this->textures = textures;

		SetupMesh();
	}

	void Mesh::Draw(Shader& shader, bool renderShadowMap, unsigned int shadowMapTextureID)
	{
		//Bind appropriate textures.
		unsigned int diffuseNr = 1;
		unsigned int specularNr = 1;
		unsigned int normalNr = 1;
		unsigned int heightNr = 1;

		//Draw Mesh
		glBindVertexArray(vertexArrayObject);

		shader.UseShader();

		if (!renderShadowMap)
		{
			for (unsigned int i = 0; i < textures.size(); i++)
			{
				glActiveTexture(GL_TEXTURE0 + i); //Activate proper texture unit before binding.
				//Retrieve texture number (the N in diffuse_textureN)
				std::string number;
				std::string name = textures[i].type;

				if (name == "texture_diffuse")
				{
					number = std::to_string(diffuseNr++); //Convert to string and return before incrementing.
				}
				else if (name == "texture_specular")
				{
					number = std::to_string(specularNr++);
				}
				else if (name == "texture_normal")
				{
					number = std::to_string(normalNr++);
				}
				else if (name == "texture_height")
				{
					number = std::to_string(heightNr++);
				}

				shader.UseShader();
				glUniform1i(glGetUniformLocation(shader.GetShaderID(), (name + number).c_str()), i);
				glBindTexture(GL_TEXTURE_2D, textures[i].id);
			}
		}

		glDrawElements(GL_TRIANGLES, indices.size(), GL_UNSIGNED_INT, 0);
		for (int i = 0; i < 32; i++)
		{
			if (i == 3)
			{
				continue;
			}
			glActiveTexture(GL_TEXTURE0 + i);
			glBindTexture(GL_TEXTURE_2D, 0);
		}

		glBindVertexArray(0);
	}

	void Mesh::SetupMesh()
	{
		glGenVertexArrays(1, &vertexArrayObject);

		glGenBuffers(1, &vertexBufferObject);
		glGenBuffers(1, &indexBufferObject);

		glBindVertexArray(vertexArrayObject);

		glBindBuffer(GL_ARRAY_BUFFER, vertexBufferObject);
		glBufferData(GL_ARRAY_BUFFER, vertices.size() * sizeof(Vertex), &vertices[0], GL_STATIC_DRAW);

		glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, indexBufferObject);
		glBufferData(GL_ELEMENT_ARRAY_BUFFER, indices.size() * sizeof(unsigned int), &indices[0], GL_STATIC_DRAW);

		//Vertex Positions
		glEnableVertexAttribArray(0);
		glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, sizeof(Vertex), (void*)0);

		//Vertex Normals
		glEnableVertexAttribArray(1);
		glVertexAttribPointer(1, 3, GL_FLOAT, GL_FALSE, sizeof(Vertex), (void*)offsetof(Vertex, Normal));

		//Vertex Texture Coodinates
		glEnableVertexAttribArray(2);
		glVertexAttribPointer(2, 2, GL_FLOAT, GL_FALSE, sizeof(Vertex), (void*)offsetof(Vertex, TexCoords));

		//Vertex Tangents
		glEnableVertexAttribArray(3);
		glVertexAttribPointer(3, 3, GL_FLOAT, GL_FALSE, sizeof(Vertex), (void*)offsetof(Vertex, Tangent));

		//Vertex Bitangents
		glEnableVertexAttribArray(4);
		glVertexAttribPointer(4, 3, GL_FLOAT, GL_FALSE, sizeof(Vertex), (void*)offsetof(Vertex, Bitangent));
		
		glEnableVertexAttribArray(5);
		glVertexAttribIPointer(5, 4, GL_INT, sizeof(Vertex), (void*)(offsetof(Vertex, BoneIDs) + 0 * sizeof(int)));

		glEnableVertexAttribArray(6);
		glVertexAttribIPointer(6, 4, GL_INT, sizeof(Vertex), (void*)(offsetof(Vertex, BoneIDs) + 4 * sizeof(int)));

		glEnableVertexAttribArray(7);
		glVertexAttribPointer(7, 4, GL_FLOAT, false, sizeof(Vertex), (void*)(offsetof(Vertex, BoneWeights) + 0 * sizeof(float)));

		glEnableVertexAttribArray(8);
		glVertexAttribPointer(8, 4, GL_FLOAT, false, sizeof(Vertex), (void*)(offsetof(Vertex, BoneWeights) + 4 * sizeof(float)));
		
		glBindVertexArray(0);
	}
}
