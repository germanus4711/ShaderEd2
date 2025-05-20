//
// Created by André Krützfeldt on 5/20/25.
//

#include "CodeEditorSettings.h"

#include "CodeEditorUI.h"

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
}