//
// Created by André Krützfeldt on 5/20/25.
//

#include "CodeEditorUISaving.h"

#include "CodeEditorUI.h"
#include "SHADERed/Objects/ShaderCompiler.h"
#include "misc/ImFileDialog.h"

#include <fstream>
#include <iostream>

namespace ed {
	void CodeEditorUI::m_save(int editor_id)
	{
		if (editor_id >= m_editor.size())
			return;

		m_editorSaveRequestID = editor_id;

		const std::function saveFunc = [&](bool success) {
			if (success) {
				PluginShaderEditor* pluginEd = &m_pluginEditor[m_editorSaveRequestID];
				TextEditor* ed = m_editor[m_editorSaveRequestID];
				std::string text;
				if (ed == nullptr) {
					size_t textLength = 0;

					const char* tempText = pluginEd->Plugin->ShaderEditor_GetContent(pluginEd->LanguageID, pluginEd->ID, &textLength);
					text = std::string(tempText, textLength);
				} else
					text = ed->GetText();

				bool isSeparateFile = m_items[m_editorSaveRequestID] == nullptr;

				std::string path;

				// TODO: why not just m_paths[m_editorSaveRequestID] ? was this written before m_paths was added?
				if (!isSeparateFile) {
					if (m_items[m_editorSaveRequestID]->Type == PipelineItem::ItemType::ShaderPass) {
						auto* shader = static_cast<ed::pipe::ShaderPass*>(m_items[m_editorSaveRequestID]->Data);

						if (m_shaderStage[m_editorSaveRequestID] == ShaderStage::Vertex)
							path = shader->VSPath;
						else if (m_shaderStage[m_editorSaveRequestID] == ShaderStage::Pixel)
							path = shader->PSPath;
						else if (m_shaderStage[m_editorSaveRequestID] == ShaderStage::Geometry)
							path = shader->GSPath;
						else if (m_shaderStage[m_editorSaveRequestID] == ShaderStage::TessellationControl)
							path = shader->TCSPath;
						else if (m_shaderStage[m_editorSaveRequestID] == ShaderStage::TessellationEvaluation)
							path = shader->TESPath;
					} else if (m_items[m_editorSaveRequestID]->Type == PipelineItem::ItemType::ComputePass) {
						auto* shader = static_cast<ed::pipe::ComputePass*>(m_items[m_editorSaveRequestID]->Data);
						path = shader->Path;
					} else if (m_items[m_editorSaveRequestID]->Type == PipelineItem::ItemType::AudioPass) {
						auto* shader = static_cast<ed::pipe::AudioPass*>(m_items[m_editorSaveRequestID]->Data);
						path = shader->Path;
					}
				} else
					path = m_paths[m_editorSaveRequestID];

				if (ed)
					ed->ResetTextChanged();
				else
					pluginEd->Plugin->ShaderEditor_ResetChangeState(pluginEd->LanguageID, pluginEd->ID);

				if (!isSeparateFile && m_items[m_editorSaveRequestID]->Type == PipelineItem::ItemType::PluginItem) {
					auto* shader = static_cast<pipe::PluginItemData*>(m_items[m_editorSaveRequestID]->Data);
					std::string edsrc = m_editor[m_editorSaveRequestID]->GetText();
					shader->Owner->CodeEditor_SaveItem(edsrc.c_str(), static_cast<int>(edsrc.size()), m_paths[m_editorSaveRequestID].c_str()); // TODO: custom stages
				} else
					m_data->Parser.SaveProjectFile(path, text);
			}
		};

		// prompt user to choose a project location first
		if (m_data->Parser.GetOpenedFile().empty()) {
			RequestedProjectSave = true;
			m_ui->SaveAsProject(true, saveFunc);
		} else
			saveFunc(true);
	}
	void CodeEditorUI::m_saveAsSPV(int id)
	{
		if (id < m_items.size()) {
			m_editorSaveRequestID = id;
			ifd::FileDialog::Instance().Save("SaveSPVBinaryDlg", "Save SPIR-V binary", "SPIR-V binary (*.spv){.spv},.*");
		}
	}
	void CodeEditorUI::m_saveAsGLSL(int id)
	{
		if (id < m_items.size()) {
			m_editorSaveRequestID = id;
			ifd::FileDialog::Instance().Save("SaveGLSLDlg", "Save as GLSL", "GLSL source (*.glsl){.glsl},.*");
		}
	}
	void CodeEditorUI::m_saveAsHLSL(int id)
	{
		if (id < m_items.size()) {
			m_editorSaveRequestID = id;
			ifd::FileDialog::Instance().Save("SaveHLSLDlg", "Save as HLSL", "HLSL source (*.hlsl){.hlsl},.*");
		}
	}
	void CodeEditorUI::LoadSnippets()
	{
		Logger::Get().Log("Loading code snippets");

		const std::string snippetsFileLoc = Settings::Instance().ConvertPath("data/snippets.xml");

		pugi::xml_document doc;
		if (pugi::xml_parse_result result = doc.load_file(snippetsFileLoc.c_str()); !result)
			return;

		m_snippets.clear();

		const pugi::xml_node snippetsList = doc.child("snippets");
		for (pugi::xml_node snippetNode : snippetsList.children("snippet")) {
			CodeSnippet snippet;

			snippet.Language = snippetNode.child("language").text().as_string();
			snippet.Display = snippetNode.child("display").text().as_string();
			snippet.Search = snippetNode.child("search").text().as_string();
			snippet.Code = snippetNode.child("code").text().as_string();

			m_snippets.push_back(snippet);
		}
	}
	void CodeEditorUI::SaveSnippets() const
	{
		pugi::xml_document doc;
		pugi::xml_node snippetsList = doc.append_child("snippets");

		for (const auto& snippet : m_snippets) {
			pugi::xml_node snippetNode = snippetsList.append_child("snippet");

			snippetNode.append_child("language").text().set(snippet.Language.c_str());
			snippetNode.append_child("display").text().set(snippet.Display.c_str());
			snippetNode.append_child("search").text().set(snippet.Search.c_str());
			snippetNode.append_child("code").text().set(snippet.Code.c_str());
		}

		std::string snippetsFileLoc = Settings::Instance().ConvertPath("data/snippets.xml");
		if (!doc.save_file(snippetsFileLoc.c_str())) {
			std::cout << "Error saving the snippet!";
		}
		// auto did_save = doc.save_file(snippetsFileLoc.c_str());
	}
	void CodeEditorUI::AddSnippet(const std::string& lang, const std::string& display, const std::string& search, const std::string& code)
	{
		RemoveSnippet(lang, display);

		CodeSnippet snip;
		snip.Language = lang;
		snip.Display = display;
		snip.Search = search;
		snip.Code = code;
		m_snippets.push_back(snip);
	}
	void CodeEditorUI::RemoveSnippet(const std::string& lang, const std::string& display)
	{
		for (int i = 0; i < m_snippets.size(); i++)
			if (m_snippets[i].Language == lang && m_snippets[i].Display == display) {
				m_snippets.erase(m_snippets.begin() + i);
				i--;
			}
	}
	void CodeEditorUI::SaveAll()
	{
		for (int i = 0; i < m_items.size(); i++)
			m_save(i);
	}

	void CodeEditorUI::SaveSPVBinary()
	{
		std::string filePathName = ifd::FileDialog::Instance().GetResult().u8string();

		std::vector<unsigned int> spv;

		PipelineItem* item = m_items[m_editorSaveRequestID];
		ShaderStage stage = m_shaderStage[m_editorSaveRequestID];

		if (item->Type == PipelineItem::ItemType::ShaderPass) {
			auto* pass = static_cast<pipe::ShaderPass*>(item->Data);
			if (stage == ShaderStage::Pixel)
				spv = pass->PSSPV;
			else if (stage == ShaderStage::Vertex)
				spv = pass->VSSPV;
			else if (stage == ShaderStage::Geometry)
				spv = pass->GSSPV;
			else if (stage == ShaderStage::TessellationControl)
				spv = pass->TCSSPV;
			else if (stage == ShaderStage::TessellationEvaluation)
				spv = pass->TESSPV;
		} else if (item->Type == PipelineItem::ItemType::ComputePass) {
			auto* pass = static_cast<pipe::ComputePass*>(item->Data);
			if (stage == ShaderStage::Pixel)
				spv = pass->SPV;
		} else if (item->Type == PipelineItem::ItemType::PluginItem) {
			auto* data = static_cast<pipe::PluginItemData*>(item->Data);
			unsigned int spvSize = data->Owner->PipelineItem_GetSPIRVSize(data->Type, data->PluginData, static_cast<plugin::ShaderStage>(stage));
			unsigned int* spvPtr = data->Owner->PipelineItem_GetSPIRV(data->Type, data->PluginData, static_cast<plugin::ShaderStage>(stage));

			if (spvPtr != nullptr && spvSize != 0)
				spv = std::vector(spvPtr, spvPtr + spvSize);
		}

		std::ofstream spvOut(filePathName, std::ios::out | std::ios::binary);
		spvOut.write(reinterpret_cast<char*>(spv.data()), static_cast<long>(sizeof(unsigned int)) * static_cast<std::streamsize>(spv.size()));
		spvOut.close();
	}
	void CodeEditorUI::SaveGLSL()
	{
		std::string filePathName = ifd::FileDialog::Instance().GetResult().u8string();

		std::vector<unsigned int> spv;
		bool gsUsed = false;
		bool tsUsed = false;

		PipelineItem* item = m_items[m_editorSaveRequestID];
		ShaderStage stage = m_shaderStage[m_editorSaveRequestID];

		ShaderLanguage lang = ShaderCompiler::GetShaderLanguageFromExtension(m_paths[m_editorSaveRequestID]);

		if (item->Type == PipelineItem::ItemType::ShaderPass) {
			auto* pass = static_cast<pipe::ShaderPass*>(item->Data);
			if (stage == ShaderStage::Pixel)
				spv = pass->PSSPV;
			else if (stage == ShaderStage::Vertex)
				spv = pass->VSSPV;
			else if (stage == ShaderStage::Geometry)
				spv = pass->GSSPV;
			else if (stage == ShaderStage::TessellationControl)
				spv = pass->TCSSPV;
			else if (stage == ShaderStage::TessellationEvaluation)
				spv = pass->TESSPV;

			gsUsed = pass->GSUsed;
			tsUsed = pass->TSUsed;
		} else if (item->Type == PipelineItem::ItemType::ComputePass) {
			auto* pass = static_cast<pipe::ComputePass*>(item->Data);
			if (stage == ShaderStage::Pixel)
				spv = pass->SPV;
		} else if (item->Type == PipelineItem::ItemType::PluginItem) {
			auto* data = static_cast<pipe::PluginItemData*>(item->Data);
			unsigned int spvSize = data->Owner->PipelineItem_GetSPIRVSize(data->Type, data->PluginData, static_cast<plugin::ShaderStage>(stage));
			unsigned int* spvPtr = data->Owner->PipelineItem_GetSPIRV(data->Type, data->PluginData, static_cast<plugin::ShaderStage>(stage));

			if (spvPtr != nullptr && spvSize != 0)
				spv = std::vector(spvPtr, spvPtr + spvSize);
		}

		std::string glslSource = ed::ShaderCompiler::ConvertToGLSL(spv, lang, stage, tsUsed, gsUsed, nullptr);

		std::ofstream spvOut(filePathName, std::ios::out | std::ios::binary);
		spvOut.write(glslSource.c_str(), static_cast<long>(glslSource.size()));
		spvOut.close();
	}
	void CodeEditorUI::SaveHLSL()
	{
		std::string filePathName = ifd::FileDialog::Instance().GetResult().u8string();

		std::vector<unsigned int> spv;

		PipelineItem* item = m_items[m_editorSaveRequestID];
		ShaderStage stage = m_shaderStage[m_editorSaveRequestID];

		if (item->Type == PipelineItem::ItemType::ShaderPass) {
			auto pass = static_cast<pipe::ShaderPass*>(item->Data);
			if (stage == ShaderStage::Pixel)
				spv = pass->PSSPV;
			else if (stage == ShaderStage::Vertex)
				spv = pass->VSSPV;
			else if (stage == ShaderStage::Geometry)
				spv = pass->GSSPV;
			else if (stage == ShaderStage::TessellationControl)
				spv = pass->TCSSPV;
			else if (stage == ShaderStage::TessellationEvaluation)
				spv = pass->TESSPV;
		} else if (item->Type == PipelineItem::ItemType::ComputePass) {
			auto* pass = static_cast<pipe::ComputePass*>(item->Data);
			if (stage == ShaderStage::Pixel)
				spv = pass->SPV;
		} else if (item->Type == PipelineItem::ItemType::PluginItem) {
			auto* data = static_cast<pipe::PluginItemData*>(item->Data);
			unsigned int spvSize = data->Owner->PipelineItem_GetSPIRVSize(data->Type, data->PluginData, static_cast<plugin::ShaderStage>(stage));
			unsigned int* spvPtr = data->Owner->PipelineItem_GetSPIRV(data->Type, data->PluginData, static_cast<plugin::ShaderStage>(stage));

			if (spvPtr != nullptr && spvSize != 0)
				spv = std::vector(spvPtr, spvPtr + spvSize);
		}

		std::string hlslSource = ed::ShaderCompiler::ConvertToHLSL(spv, stage);

		std::ofstream spvOut(filePathName, std::ios::out | std::ios::binary);
		spvOut.write(hlslSource.c_str(), static_cast<long>(hlslSource.size()));
		spvOut.close();
	}
}