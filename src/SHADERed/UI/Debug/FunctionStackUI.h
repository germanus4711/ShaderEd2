#pragma once
#include <SHADERed/UI/UIView.h>

namespace ed {
	class DebugFunctionStackUI final : public UIView {
	public:
		using UIView::UIView;

		void Refresh();

		void OnEvent(const SDL_Event& e) override;
		void Update(float delta) override;

	private:
		std::vector<std::string> m_stack;
	};
}