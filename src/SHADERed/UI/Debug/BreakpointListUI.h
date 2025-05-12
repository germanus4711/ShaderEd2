#pragma once
#include <SHADERed/UI/UIView.h>

namespace ed {
	class DebugBreakpointListUI final : public UIView {
	public:
		using UIView::UIView;

		void OnEvent(const SDL_Event& e) override;
		void Update(float delta) override;
	};
}