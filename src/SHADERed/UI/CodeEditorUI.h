#pragma once
#include <ImGuiColorTextEdit/TextEditor.h>
#include <SHADERed/Objects/Logger.h>
#include <SHADERed/Objects/PipelineItem.h>
#include <SHADERed/Objects/Settings.h>
#include <SHADERed/Objects/ShaderLanguage.h>
#include <SHADERed/Objects/ShaderStage.h>
#include <SHADERed/Objects/SPIRVParser.h>
#include <SHADERed/UI/Tools/StatsPage.h>
#include <SHADERed/UI/UIView.h>
#include <imgui/examples/imgui_impl_opengl3.h>
#include <imgui/examples/imgui_impl_sdl.h>
#include <deque>
#include <future>
#include <shared_mutex>

namespace ed {
	struct CodeSnippet {
		std::string Language;
		std::string Display;
		std::string Search;
		std::string Code;
	};

	struct PluginShaderEditor {
		PluginShaderEditor()
		{
			Plugin = nullptr;
			LanguageID = -1;
			ID = -1;
		}
		PluginShaderEditor(IPlugin1* pl, int langID, int id)
		{
			Plugin = pl;
			LanguageID = langID;
			ID = id;
		}
		IPlugin1* Plugin;
		int LanguageID;
		int ID;
	};

	class CodeEditorUI final : public UIView {
	public:
		CodeEditorUI(GUIManager* ui, ed::InterfaceManager* objects, const std::string& name = "", bool visible = false);
		~CodeEditorUI() override;

		void OnEvent(const SDL_Event& e) override;
		void Update(float delta) override;

		void DrawTextEditor(const std::string& name, TextEditor* tEdit);

		void LoadSnippets();
		void SaveSnippets() const;
		void AddSnippet(const std::string& lang, const std::string& display, const std::string& search, const std::string& code);
		void RemoveSnippet(const std::string& lang, const std::string& display);
		const std::vector<CodeSnippet>& GetSnippets() { return m_snippets; }

		void UpdateAutoRecompileItems();

		void StopThreads();
		void CleanupTrackingThread();
		void StopDebugging() const;
		void CloseAll(PipelineItem* item = nullptr);
		void SaveAll();

		void Open(PipelineItem* item, ed::ShaderStage stage);
		void OpenFile(const std::string& path);
		void OpenPluginCode(PipelineItem* item, const char* filepath, int id);
		TextEditor* Get(const PipelineItem* item, ed::ShaderStage stage) const;
		TextEditor* Get(const std::string& path) const;
		PluginShaderEditor GetPluginEditor(const PipelineItem* item, ed::ShaderStage stage) const;
		PluginShaderEditor GetPluginEditor(const std::string& path) const;
		std::string GetPluginEditorPath(const PluginShaderEditor& editor);
		

		void SetTheme(const TextEditor::Palette& colors) const;
		void SetFont(const std::string& filename, int size = 15);
		void SetTrackFileChanges(bool track);
		void ApplySettings();

		void FillAutocomplete(TextEditor* tEdit, const SPIRVParser& spv, bool colorize = true);

		bool HasFocus() const { return m_selectedItem != -1; }
		bool NeedsFontUpdate() const { return m_fontNeedsUpdate; }
		std::pair<std::string, int> GetFont() { return std::make_pair(m_fontFilename, m_fontSize); }
		void UpdateFont()
		{
			m_fontNeedsUpdate = false;
			if (ImGui::GetIO().Fonts->Fonts.size() > 1)
				m_font = ImGui::GetIO().Fonts->Fonts[1];
			else
				m_font = ImGui::GetIO().Fonts->Fonts[0];
		}
		ImFont* GetImFont() { return m_font; }

		bool TrackedFilesNeedUpdate() { return m_trackUpdatesNeeded > 0; }
		void EmptyTrackedFiles() { m_trackUpdatesNeeded = 0; }
		std::vector<bool> TrackedNeedsUpdate() { return m_trackedNeedsUpdate; }

		void SetOpenedFilesData(const std::vector<std::string>& data) const;
		std::vector<std::string> GetOpenedFilesData() const;
		std::vector<std::pair<std::string, ShaderStage>> GetOpenedFiles();

		bool RequestedProjectSave;

		void ChangePluginShaderEditor(IPlugin1* plugin, int langID, int editorID);
		PipelineItem* GetPluginEditorPipelineItem(const IPlugin1* plugin, int langID, int editorID) const;


	private:
		void m_setupShortcuts();
		static void m_loadEditorShortcuts(TextEditor* editor);
		std::string m_findIncludedFile(const std::string& relativeTo, const std::string& path) const;

		static TextEditor::LanguageDefinition m_buildLanguageDefinition(IPlugin1* plugin, int languageID);
		void m_applyBreakpoints(TextEditor* editor, const std::string& path) const;

		static void m_setupPlugin(ed::IPlugin1* plugin);

		std::vector<CodeSnippet> m_snippets;

		// code editor font
		ImFont* m_font;

		// menu bar item actions
		void m_save(int id);
		void m_saveAsSPV(int id);
		void m_saveAsGLSL(int id);
		void m_saveAsHLSL(int id);
		void m_compile(int id);

		std::vector<PipelineItem*> m_items;
		std::vector<TextEditor*> m_editor;
		std::vector<StatsPage> m_stats;
		std::vector<std::string> m_paths;
		std::vector<ShaderStage> m_shaderStage;
		std::vector<PluginShaderEditor> m_pluginEditor;
		std::deque<bool> m_editorOpen;

		int m_editorSaveRequestID{};

		bool m_fontNeedsUpdate;
		std::string m_fontFilename;
		int m_fontSize;

		bool m_focusWindow;
		std::string m_focusPath;

		int m_savePopupOpen;
		int m_selectedItem;

		// auto recompile
		bool m_contentChanged;
		std::vector<TextEditor*> m_changedEditors;
		std::vector<PluginShaderEditor*> m_changedPluginEditors;
		eng::Timer m_lastAutoRecompile;

		// all the variables needed for the file change notifications
		void m_trackWorker();
		std::vector<bool> m_trackedNeedsUpdate;
		bool m_trackFileChanges;
		std::atomic<bool> m_trackerRunning{};
		std::atomic<int> m_trackUpdatesNeeded;
		std::vector<std::string> m_trackIgnore;
		std::thread* m_trackThread;
		std::mutex m_trackFilesMutex;
	};
}