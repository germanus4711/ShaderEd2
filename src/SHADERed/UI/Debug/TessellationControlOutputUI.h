#pragma once
#include <SHADERed/UI/UIView.h>

namespace ed {
	class DebugTessControlOutputUI final : public UIView {
	public:
		DebugTessControlOutputUI(GUIManager* ui, ed::InterfaceManager* objects, const std::string& name = "", bool visible = true);
		~DebugTessControlOutputUI() override;

		void OnEvent(const SDL_Event& e) override;
		void Update(float delta) override;

		void Refresh();

	private:
		void m_render() const;

		void m_buildShaders();

		glm::vec2 m_inner{};
		glm::vec4 m_outer{};
		bool m_error; // is any of the levels below 1.0?

		GLuint m_fbo, m_color, m_depth;
		ImVec2 m_lastSize;

		GLuint m_triangleVAO, m_triangleVBO{};
		GLuint m_shader;
	};
}