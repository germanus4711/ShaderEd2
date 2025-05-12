#pragma once
#include <SHADERed/InterfaceManager.h>

#include <utility>

namespace ed {
	class GUIManager;

	class UIView {
	public:
		UIView(GUIManager* ui, ed::InterfaceManager* objects, std::string name = "", const bool visible = true)
				: m_ui(ui)
				, m_data(objects)
				, Visible(visible)
				, Name(std::move(name))
		{
		}
		virtual ~UIView() = default;

		virtual void OnEvent(const SDL_Event& e) = 0;
		virtual void Update(float delta) = 0;

		bool Visible;
		std::string Name;

	protected:
		InterfaceManager* m_data;
		GUIManager* m_ui;
	};
}
