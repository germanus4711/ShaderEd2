//
// Created by André Krützfeldt on 5/20/25.
//

#include "CodeEditorSettings.h"

#include "CodeEditorUI.h"
#include "SHADERed/Objects/Names.h"
#include "SHADERed/Objects/ThemeContainer.h"

namespace ed {

	void CodeEditorUI::ApplySettings()
	{
		const std::vector<IPlugin1*>& plugins = m_data->Plugins.Plugins();
		for (int i = 0; i < m_editor.size(); i++) {
			TextEditor* editor = m_editor[i];
			if (editor == nullptr)
				continue;
			editor->SetTabSize(Settings::Instance().Editor.TabSize);
			editor->SetInsertSpaces(Settings::Instance().Editor.InsertSpaces);
			editor->SetSmartIndent(Settings::Instance().Editor.SmartIndent);
			editor->SetAutoIndentOnPaste(Settings::Instance().Editor.AutoIndentOnPaste);
			editor->SetShowWhitespaces(Settings::Instance().Editor.ShowWhitespace);
			editor->SetHighlightLine(Settings::Instance().Editor.HiglightCurrentLine);
			editor->SetShowLineNumbers(Settings::Instance().Editor.LineNumbers);
			editor->SetCompleteBraces(Settings::Instance().Editor.AutoBraceCompletion);
			editor->SetHorizontalScroll(Settings::Instance().Editor.HorizontalScroll);
			editor->SetSmartPredictions(Settings::Instance().Editor.SmartPredictions);
			editor->SetFunctionTooltips(Settings::Instance().Editor.FunctionTooltips);
			editor->SetFunctionDeclarationTooltip(Settings::Instance().Editor.FunctionDeclarationTooltips);
			editor->SetUIScale(Settings::Instance().TempScale);
			editor->SetUIFontSize(static_cast<float>(Settings::Instance().General.FontSize));
			editor->SetEditorFontSize(static_cast<float>(Settings::Instance().Editor.FontSize));
			editor->SetActiveAutocomplete(Settings::Instance().Editor.ActiveSmartPredictions);
			editor->SetColorizerEnable(Settings::Instance().Editor.SyntaxHighlighting);
			editor->SetScrollbarMarkers(Settings::Instance().Editor.ScrollbarMarkers);
			editor->SetHiglightBrackets(Settings::Instance().Editor.HighlightBrackets);
			editor->SetFoldEnabled(Settings::Instance().Editor.CodeFolding);

			editor->ClearAutocompleteEntries();

			const auto stage = static_cast<plugin::ShaderStage>(m_shaderStage[i]);
			for (IPlugin1* plugin : plugins) {
				for (int pluginIndex = 0; pluginIndex < plugin->Autocomplete_GetCount(stage); pluginIndex++) {
					const char* displayString = plugin->Autocomplete_GetDisplayString(stage, pluginIndex);
					const char* searchString = plugin->Autocomplete_GetSearchString(stage, pluginIndex);
					const char* value = plugin->Autocomplete_GetValue(stage, pluginIndex);

					editor->AddAutocompleteEntry(searchString, displayString, value);
				}
			}
			for (const auto& snippet : m_snippets)
				if (editor->GetLanguageDefinition().mName == snippet.Language || snippet.Language == "*")
					editor->AddAutocompleteEntry(snippet.Search, snippet.Display, snippet.Code);

			m_loadEditorShortcuts(editor);
		}

		SetFont(Settings::Instance().Editor.FontPath, Settings::Instance().Editor.FontSize);
		SetTrackFileChanges(Settings::Instance().General.RecompileOnFileChange);
	}
	void CodeEditorUI::SetTheme(const TextEditor::Palette& colors) const
	{
		for (TextEditor* editor : m_editor)
			if (editor)
				editor->SetPalette(colors);
	}
	void CodeEditorUI::SetFont(const std::string& filename, int size)
	{
		m_fontNeedsUpdate = m_fontFilename != filename || m_fontSize != size;
		m_fontFilename = filename;
		m_fontSize = size;
	}
	auto CodeEditorUI::m_loadEditorShortcuts(TextEditor* ed) -> void
	{
		const auto sMap = KeyboardShortcuts::Instance().GetMap();

		for (auto& it : sMap) {
			std::string id = it.first;

			if (id.size() > 8 && id.substr(0, 6) == "Editor") {
				std::string name = id.substr(7);

				auto sID = TextEditor::ShortcutID::Count;
				for (int i = 0; i < static_cast<int>(TextEditor::ShortcutID::Count); i++) {
					if (EDITOR_SHORTCUT_NAMES[i] == name) {
						sID = static_cast<TextEditor::ShortcutID>(i);
						break;
					}
				}

				if (sID != TextEditor::ShortcutID::Count)
					ed->SetShortcut(sID, TextEditor::Shortcut(it.second.Key1, it.second.Key2, it.second.Alt, it.second.Ctrl, it.second.Shift));
			}
		}
	}
	void CodeEditorUI::ConfigureTextEditor(TextEditor* editor, const std::string& path)
	{
		if (!editor) return;

		const auto& settings = Settings::Instance();

		editor->SetPalette(ThemeContainer::Instance().GetTextEditorStyle(settings.Theme));
		editor->SetTabSize(settings.Editor.TabSize);
		editor->SetInsertSpaces(settings.Editor.InsertSpaces);
		editor->SetSmartIndent(settings.Editor.SmartIndent);
		editor->SetAutoIndentOnPaste(settings.Editor.AutoIndentOnPaste);
		editor->SetShowWhitespaces(settings.Editor.ShowWhitespace);
		editor->SetHighlightLine(settings.Editor.HiglightCurrentLine);
		editor->SetShowLineNumbers(settings.Editor.LineNumbers);
		editor->SetCompleteBraces(settings.Editor.AutoBraceCompletion);
		editor->SetHorizontalScroll(settings.Editor.HorizontalScroll);
		editor->SetSmartPredictions(settings.Editor.SmartPredictions);
		editor->SetFunctionTooltips(settings.Editor.FunctionTooltips);
		editor->SetFunctionDeclarationTooltip(settings.Editor.FunctionDeclarationTooltips);
		editor->SetPath(path);
		editor->SetUIScale(settings.DPIScale);
		editor->SetUIFontSize(static_cast<float>(settings.General.FontSize));
		editor->SetEditorFontSize(static_cast<float>(settings.Editor.FontSize));
		editor->SetActiveAutocomplete(settings.Editor.ActiveSmartPredictions);
		editor->SetColorizerEnable(settings.Editor.SyntaxHighlighting);
		editor->SetScrollbarMarkers(settings.Editor.ScrollbarMarkers);
		editor->SetHiglightBrackets(settings.Editor.HighlightBrackets);
		editor->SetFoldEnabled(settings.Editor.CodeFolding);
	}
}