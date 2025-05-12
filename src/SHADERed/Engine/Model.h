#pragma once
#include <assimp/postprocess.h>
#include <assimp/scene.h>
#include <assimp/Importer.hpp>
#include <glm/glm.hpp>
#include <string>
#include <utility>
#include <vector>

namespace ed::eng {
	class Model {
	public:
		class Mesh {
		public:
			struct Vertex {
				glm::vec3 Position;
				glm::vec3 Normal;
				glm::vec2 TexCoords;
				glm::vec3 Tangent;
				glm::vec3 Binormal;
				glm::vec4 Color;
			};
			struct Texture {
				unsigned int ID;
				std::string Type;
			};

			std::string Name;

			std::vector<Vertex> Vertices;
			std::vector<unsigned int> Indices;
			std::vector<Texture> Textures;

			Mesh(const std::string& name, const std::vector<Vertex>& vertices, const std::vector<unsigned int>& indices, const std::vector<Texture>& textures);

			void Draw(bool instanced = false, int iCount = 0) const;

			unsigned int VAO{}, VBO{}, EBO{};

		private:
			void m_setup();
		};

		Model(const std::vector<Mesh>& meshes, std::string  directory, const glm::vec3& m_min_bound, const glm::vec3& m_max_bound)
				: Meshes(meshes)
				, Directory(std::move(directory))
				, m_minBound(m_min_bound)
				, m_maxBound(m_max_bound)
		{
		}
		~Model();

		std::vector<Mesh> Meshes;
		std::string Directory;

		[[nodiscard]] std::vector<std::string> GetMeshNames() const;
		bool LoadFromFile(const std::string& path);
		void Draw(bool instanced = false, int iCount = 0) const;
		void Draw(const std::string& mesh) const;

		[[nodiscard]] glm::vec3 GetMinBound() const { return m_minBound; }
		[[nodiscard]] glm::vec3 GetMaxBound() const { return m_maxBound; }

	private:
		void m_findBounds();

		glm::vec3 m_minBound, m_maxBound;
		void m_processNode(const aiNode* node, const aiScene* scene);
		static Mesh m_processMesh(aiMesh* mesh, const aiScene* scene);
	};
}
