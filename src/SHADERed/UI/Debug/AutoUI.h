#pragma once
#include <SHADERed/UI/UIView.h>
#include <string>
#include <vector>

namespace ed {
	class DebugAutoUI final : public UIView {
	public:
		DebugAutoUI(GUIManager* ui, InterfaceManager* objects, const std::string& name = "", const bool visible = true)
				: UIView(ui, objects, name, visible)
		{
			m_update = false;
		}

		inline void SetExpressions(const std::vector<std::string>& exprs)
		{
			m_expr = exprs;
			m_value.clear();
			m_update = true;
		}

		void OnEvent(const SDL_Event& e) override;
		void Update(float delta) override;

	private:
		std::vector<std::string> m_value;
		std::vector<std::string> m_expr;
		bool m_update;
	};
}