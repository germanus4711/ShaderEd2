#pragma once
#include <GL/glew.h>
#if defined(__APPLE__)
#include <OpenGL/gl.h>
#else
#include <GL/gl.h>
#endif

namespace ed {
	class Texture3DPreview {
	public:
		~Texture3DPreview();

		void Init();
		void Draw(GLuint tex, int w, int h, float uvZ = 0.0f);
		[[nodiscard]] GLuint GetTexture() const { return m_color; }

	private:
		float m_w = 0, m_h = 0;

		GLuint m_shader = 0;
		GLuint m_fbo = 0, m_color = 0, m_depth = 0;
		GLuint m_vao = 0, m_vbo = 0;
		GLuint m_uMatWVPLoc = 0, m_uLevelLoc = 0;
	};
}