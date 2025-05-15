#pragma once
#include <SHADERed/GUIManager.h>
#include <SHADERed/InterfaceManager.h>

#include <SDL2/SDL_events.h>
#include <SDL2/SDL_video.h>

namespace ed {
	class EditorEngine {
	public:
		EditorEngine(SDL_Window* wnd, SDL_GLContext* gl);

		void OnEvent(const SDL_Event& e);
		void Update(float delta);
		void Render();

		void Create();
		void Destroy();

		InterfaceManager& Interface() { return m_interface; }
		GUIManager& UI() { return m_ui; }

	private:
		InterfaceManager m_interface;
		GUIManager m_ui;
	};
}