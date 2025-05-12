#pragma once
#include <SHADERed/Engine/Timer.h>
#include <SHADERed/UI/UIView.h>

namespace ed {
	class DebugValuesUI final : public UIView {
	public:
		using UIView::UIView;

		void Refresh();

		void OnEvent(const SDL_Event& e) override;
		void Update(float delta) override;

	private:
		std::unordered_map<std::string, std::string> m_cachedGlobals;
		std::unordered_map<std::string, std::string> m_cachedLocals;
	};
}