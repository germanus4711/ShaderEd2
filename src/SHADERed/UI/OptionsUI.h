#pragma once
#include <ImGuiColorTextEdit/TextEditor.h>
#include <SHADERed/Objects/KeyboardShortcuts.h>
#include <SHADERed/Objects/Settings.h>
#include <SHADERed/UI/UIView.h>

namespace ed {
	class OptionsUI final : public UIView {
	public:
		enum class Page {
			General,
			Editor,
			CodeSnippets,
			Debug,
			Shortcuts,
			Preview,
			Plugins,
			Project
		};

		OptionsUI(GUIManager* ui, ed::InterfaceManager* objects, const std::string& name = "", bool visible = true)
				: UIView(ui, objects, name, visible)
				, m_page(Page::General)
				, m_selectedShortcut(-1)
		{
			memset(m_shortcutSearch, 0, 256);
			memset(m_pluginSearch, 0, 256);
			memset(m_snippetDisplay, 0, 32);
			memset(m_snippetSearch, 0, 32);
			memset(m_snippetLanguage, 0, 32);
			m_pluginNotLoadedLB = 0;
			m_pluginLoadedLB = 0;
			m_overwriteShortcutOpened = false;
			m_pluginRequiresRestart = false;

			m_initSnippetEditor();
		}
		// using UIView::UIView;

		void OnEvent(const SDL_Event& e) override;
		void Update(float delta) override;

		[[nodiscard]] bool IsListening() const { return m_page == Page::Shortcuts && m_selectedShortcut != -1; }

		void SetGroup(const Page grp)
		{
			m_page = grp;
			if (m_page == Page::General)
				m_loadThemeList();
			else if (m_page == Page::Preview) {
				switch (Settings::Instance().Preview.MSAA) {
				case 1: m_msaaChoice = 0; break;
				case 2: m_msaaChoice = 1; break;
				case 4: m_msaaChoice = 2; break;
				case 8: m_msaaChoice = 3; break;
				default: m_msaaChoice = 0; break;
				}
			} else if (m_page == Page::Plugins)
				m_loadPluginList();
			else if (m_page == Page::CodeSnippets)
				m_initSnippetEditor();
		}
		[[nodiscard]] Page GetGroup() const { return m_page; }

		void ApplyTheme() const;

		const std::vector<std::string>& GetThemeList() { return m_themes; }
		void RefreshThemeList() { m_loadThemeList(); }

	private:
		Page m_page;

		bool m_overwriteShortcutOpened;
		std::string m_exisitingShortcut;
		char m_shortcutSearch[256] {};
		int m_selectedShortcut;
		KeyboardShortcuts::Shortcut m_newShortcut;
		[[nodiscard]] std::string m_getShortcutString() const;

		char* m_dialogPath {};

		int m_msaaChoice {};

		bool m_pluginRequiresRestart;

		char m_snippetLanguage[32] {};
		char m_snippetDisplay[32] {};
		char m_snippetSearch[32] {};
		TextEditor m_snippetCode;

		char m_pluginSearch[256] {};
		std::vector<std::string> m_pluginsNotLoaded, m_pluginsLoaded;
		int m_pluginNotLoadedLB,
			m_pluginLoadedLB;
		void m_loadPluginList();

		void m_initSnippetEditor();

		std::vector<std::string> m_themes;
		void m_loadThemeList();

		void m_renderGeneral();
		void m_renderEditor();
		void m_renderShortcuts();
		void m_renderPreview();
		static void m_renderDebug();
		void m_renderPlugins();
		void m_renderProject();
		void m_renderCodeSnippets();
	};
}