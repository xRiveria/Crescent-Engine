#include "CrescentPCH.h"
#include "Mesh.h"
#include "GL/glew.h"

namespace CrescentEngine
{
	Mesh::Mesh()
	{

	}

	Mesh::Mesh(std::vector<Vertex> vertices, std::vector<unsigned int> indices, std::vector<Texture> textures)
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
	void Mesh::Finalize(bool interleaved)
	{
	}
}
