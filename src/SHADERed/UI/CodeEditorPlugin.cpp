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
}