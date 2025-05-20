//
// Created by André Krützfeldt on 5/20/25.
//

#include "CodeEditorPlugin.h"

#include "CodeEditorUI.h"

namespace ed {
	PluginShaderEditor CodeEditorUI::GetPluginEditor(const PipelineItem* item, const ed::ShaderStage stage) const
	{
		for (int i = 0; i < m_items.size(); i++)
			if (m_items[i] == item && m_shaderStage[i] == stage)
				return m_pluginEditor[i];
		return {};
	}
	PluginShaderEditor CodeEditorUI::GetPluginEditor(const std::string& path) const
	{
		for (int i = 0; i < m_paths.size(); i++)
			if (m_paths[i] == path)
				return m_pluginEditor[i];
		return {};
	}
	std::string CodeEditorUI::GetPluginEditorPath(const PluginShaderEditor& editor)
	{
		for (int i = 0; i < m_pluginEditor.size(); i++)
			if (m_pluginEditor[i].ID == editor.ID && m_pluginEditor[i].LanguageID == editor.LanguageID && m_pluginEditor[i].Plugin == editor.Plugin)
				return m_paths[i];
		return "";
	}
	void CodeEditorUI::m_setupPlugin(ed::IPlugin1* plugin)
	{
		plugin->OnEditorContentChange = [](void* UI, void* plugin, int langID, int editorID) {
			auto* gui = static_cast<GUIManager*>(UI);
			auto* code = dynamic_cast<CodeEditorUI*>(gui->Get(ViewID::Code));
			code->ChangePluginShaderEditor(static_cast<IPlugin1*>(plugin), langID, editorID);
		};

		if (plugin->GetVersion() >= 3) {
			auto* plug3 = dynamic_cast<IPlugin3*>(plugin);
			plug3->GetEditorPipelineItem = [](void* UI, void* plugin, int langID, int editorID) -> void* {
				auto* gui = static_cast<GUIManager*>(UI);
				const auto code = dynamic_cast<CodeEditorUI*>(gui->Get(ViewID::Code));
				return code->GetPluginEditorPipelineItem(static_cast<IPlugin1*>(plugin), langID, editorID);
			};
		}
	}
}