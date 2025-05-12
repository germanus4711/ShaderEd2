#pragma once
#include <SDL2/SDL_events.h>
#include <SHADERed/UI/UIView.h>

namespace ed {
	class ProfilerUI final : public UIView {
	public:
		ProfilerUI(GUIManager* ui, ed::InterfaceManager* objects, const std::string& name = "", bool visible = true)
				: UIView(ui, objects, name, visible)
		{
		}
		~ProfilerUI() override = default;

		void OnEvent(const SDL_Event& e) override;
		void Update(float delta) override;

	private:
		static void m_renderRow(int index, const char* name, uint64_t time, uint64_t timeOffset, uint64_t totalTime);
	};
}