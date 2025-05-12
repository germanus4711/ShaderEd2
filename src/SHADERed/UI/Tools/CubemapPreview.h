#pragma once
#include <GL/glew.h>
#if defined(__APPLE__)
#include <OpenGL/gl.h>
#else
#include <GL/gl.h>
#endif

namespace ed {
	class CubemapPreview {
	public:
		~CubemapPreview();

		void Init(int w, int h);
		void Draw(GLuint tex) const;
		GLuint GetTexture() { return m_cubeTex; }

	private:
		float m_w = 0, m_h = 0;

		GLuint m_cubeShader = 0;
		GLuint m_cubeFBO = 0, m_cubeTex = 0, m_cubeDepth = 0;
		GLuint m_fsVAO = 0, m_fsVBO = 0;
		GLuint m_uMatWVPLoc = 0;
	};
}