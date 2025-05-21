#include <SHADERed/Objects/KeyboardShortcuts.h>
#include <SHADERed/Objects/Logger.h>
#include <SHADERed/Objects/Names.h>
#include <SHADERed/Objects/Settings.h>
#include <SHADERed/Objects/ShaderCompiler.h>
#include <SHADERed/Objects/ThemeContainer.h>
#include <SHADERed/UI/CodeEditorUI.h>
#include <SHADERed/UI/PreviewUI.h>
#include <SHADERed/UI/UIHelper.h>
#include <misc/ImFileDialog.h>

#include <imgui/imgui.h>
#include <imgui/imgui_internal.h>
#include <filesystem>
#include <fstream>
#include <iostream>

#if defined(_WIN32)
#include <windows.h>
#elif defined(__linux__) || defined(__unix__)
#include <fcntl.h>
#include <sys/inotify.h>
#include <sys/types.h>
#include <unistd.h>
#define EVENT_SIZE (sizeof(struct inotify_event))
#define EVENT_BUF_LEN (1024 * (EVENT_SIZE + 16))
#elif defined(__APPLE__)
#include <sys/types.h>
#include <unistd.h>
#endif

#define STATUSBAR_HEIGHT Settings::Instance().CalculateSize(30)

// TODO TOO long
namespace ed {
	CodeEditorUI::CodeEditorUI(GUIManager* ui, ed::InterfaceManager* objects, const std::string& name, const bool visible)
			: UIView(ui, objects, name, visible)
			, m_selectedItem(-1)
			, m_trackUpdatesNeeded(0)
	{
		const Settings& sets = Settings::Instance();

		if (std::filesystem::exists(sets.Editor.FontPath)) {
			// std::cout << "Loading code editor font: " << sets.Editor.FontPath << std::endl;
			m_font = ImGui::GetIO().Fonts->AddFontFromFileTTF(sets.Editor.FontPath, static_cast<float>(sets.Editor.FontSize));
			if (m_font == nullptr) {
				Logger::Get().Log("NPE: m_font", true);
			}
		} else {
			m_font = ImGui::GetIO().Fonts->AddFontDefault();
			Logger::Get().Log("Failed to load code editor font", true);
		}

		m_fontFilename = sets.Editor.FontPath;
		m_fontSize = sets.Editor.FontSize;
		m_fontNeedsUpdate = false;
		m_savePopupOpen = -1;
		m_focusWindow = false;
		m_trackFileChanges = false;
		m_trackThread = nullptr;
		m_contentChanged = false;

		RequestedProjectSave = false;

		m_setupShortcuts();
	}
	CodeEditorUI::~CodeEditorUI()
	{
		delete m_trackThread;
	}

	void CodeEditorUI::m_setupShortcuts()
	{
		KeyboardShortcuts::Instance().SetCallback("CodeUI.Compile", [=]() {
			if (m_selectedItem == -1)
				return;

			m_compile(m_selectedItem);
		});
		KeyboardShortcuts::Instance().SetCallback("CodeUI.Save", [=]() {
			if (m_selectedItem == -1)
				return;

			m_save(m_selectedItem);
		});
		KeyboardShortcuts::Instance().SetCallback("CodeUI.SwitchView", [=]() {
			if (m_selectedItem == -1 || m_selectedItem >= m_items.size())
				return;

			m_stats[m_selectedItem].Visible = !m_stats[m_selectedItem].Visible;

			if (m_stats[m_selectedItem].Visible)
				m_stats[m_selectedItem].Refresh(m_items[m_selectedItem], m_shaderStage[m_selectedItem]);
		});
		KeyboardShortcuts::Instance().SetCallback("CodeUI.ToggleStatusbar", [=]() {
			Settings::Instance().Editor.StatusBar = !Settings::Instance().Editor.StatusBar;
		});
	}
	void CodeEditorUI::m_compile(int id)
	{
		if (id >= m_editor.size() || m_items[id] == nullptr)
			return;

		if (m_trackerRunning) {
			std::lock_guard<std::mutex> lock(m_trackFilesMutex);
			m_trackIgnore.emplace_back(m_items[id]->Name);
		}

		m_save(id);

		std::string shaderFile;
		if (m_items[id]->Type == PipelineItem::ItemType::ShaderPass) {
			const auto* shader = static_cast<ed::pipe::ShaderPass*>(m_items[id]->Data);
			if (m_shaderStage[id] == ShaderStage::Vertex)
				shaderFile = shader->VSPath;
			else if (m_shaderStage[id] == ShaderStage::Pixel)
				shaderFile = shader->PSPath;
			else if (m_shaderStage[id] == ShaderStage::Geometry)
				shaderFile = shader->GSPath;
			else if (m_shaderStage[id] == ShaderStage::TessellationControl)
				shaderFile = shader->TCSPath;
			else if (m_shaderStage[id] == ShaderStage::TessellationEvaluation)
				shaderFile = shader->TESPath;
		} else if (m_items[id]->Type == PipelineItem::ItemType::ComputePass) {
			const auto* shader = static_cast<pipe::ComputePass*>(m_items[id]->Data);
			shaderFile = shader->Path;
		} else if (m_items[id]->Type == PipelineItem::ItemType::AudioPass) {
			const auto* shader = static_cast<pipe::AudioPass*>(m_items[id]->Data);
			shaderFile = shader->Path;
		} else if (m_items[id]->Type == PipelineItem::ItemType::PluginItem) {
			const auto* shader = static_cast<pipe::PluginItemData*>(m_items[id]->Data);
			shader->Owner->HandleRecompile(m_items[id]->Name);
		}

		m_data->Renderer.RecompileFile(shaderFile.c_str());
	}

	void CodeEditorUI::OnEvent(const SDL_Event& e) { }

	void CodeEditorUI::Update(float delta)
	{
		if (m_editor.empty()) return;

		m_selectedItem = -1;

		// counters for each shader type for window ids
		int wid[10] = { 0 }; // vs, ps, gs, cs, tes, tcs, pl

		// editor windows
		for (int i = 0; i < m_editor.size(); i++) {
			if (m_editorOpen[i]) {
				bool isSeparateFile = (m_items[i] == nullptr);
				bool isPluginItem = isSeparateFile ? false : (m_items[i]->Type == PipelineItem::ItemType::PluginItem);
				pipe::PluginItemData* plData = isSeparateFile ? nullptr : static_cast<pipe::PluginItemData*>(m_items[i]->Data);

				std::string stageAbbr = "VS";
				if (m_shaderStage[i] == ShaderStage::Pixel)
					stageAbbr = "PS";
				else if (m_shaderStage[i] == ShaderStage::Geometry)
					stageAbbr = "GS";
				else if (m_shaderStage[i] == ShaderStage::Compute)
					stageAbbr = "CS";
				else if (m_shaderStage[i] == ShaderStage::Audio)
					stageAbbr = "AS";
				else if (m_shaderStage[i] == ShaderStage::TessellationControl)
					stageAbbr = "TCS";
				else if (m_shaderStage[i] == ShaderStage::TessellationEvaluation)
					stageAbbr = "TES";
				else if (isSeparateFile)
					stageAbbr = "FILE";

				std::string shaderType = isPluginItem ? plData->Owner->LanguageDefinition_GetNameAbbreviation(static_cast<int>(m_shaderStage[i])) : stageAbbr;
				std::string windowName(std::string(isSeparateFile ? std::filesystem::path(m_paths[i]).filename().u8string() : m_items[i]->Name) + " (" + shaderType + ")");

				int pluginLanguageID = m_pluginEditor[i].LanguageID;
				int pluginEditorID = m_pluginEditor[i].ID;
				bool isPluginEditorChanged = m_pluginEditor[i].Plugin != nullptr && m_pluginEditor[i].Plugin->ShaderEditor_IsChanged(pluginLanguageID, pluginEditorID);
				bool isTextEditorChanged = m_editor[i] && m_editor[i]->IsTextChanged();
				if ((isTextEditorChanged || isPluginEditorChanged) && !m_data->Parser.IsProjectModified())
					m_data->Parser.ModifyProject();

				ImGui::SetNextWindowSizeConstraints(ImVec2(300, 300), ImVec2(10000, 10000));
				ImGui::SetNextWindowSize(ImVec2(400, 300), ImGuiCond_FirstUseEver);
				if (ImGui::Begin((std::string(windowName) + "###code_view" + shaderType + std::to_string(wid[isPluginItem ? 4 : static_cast<int>(m_shaderStage[i])])).c_str(), &m_editorOpen[i], (ImGuiWindowFlags_UnsavedDocument * (isTextEditorChanged || isPluginEditorChanged)) | ImGuiWindowFlags_MenuBar)) {
					if (ImGui::BeginMenuBar()) {
						if (ImGui::BeginMenu("File")) {
							if (ImGui::MenuItem("Save", KeyboardShortcuts::Instance().GetString("CodeUI.Save").c_str())) m_save(i);
							if (ImGui::MenuItem("Save SPIR-V binary")) m_saveAsSPV(i);
							if (ImGui::MenuItem("Save as GLSL")) m_saveAsGLSL(i);
							if (ImGui::MenuItem("Save as HLSL")) m_saveAsHLSL(i);
							ImGui::EndMenu();
						}
						if (ImGui::BeginMenu("Code")) {
							if (ImGui::MenuItem("Compile", KeyboardShortcuts::Instance().GetString("CodeUI.Compile").c_str(), false, m_items[i] != nullptr)) m_compile(i);

							if (m_items[i] != nullptr && (m_pluginEditor[i].Plugin == nullptr || (m_pluginEditor[i].Plugin && m_pluginEditor[i].Plugin->ShaderEditor_HasStats(pluginLanguageID, pluginEditorID)))) {
								if (!m_stats[i].Visible && ImGui::MenuItem("Stats", KeyboardShortcuts::Instance().GetString("CodeUI.SwitchView").c_str())) {
									m_stats[i].Visible = true;
									m_stats[i].Refresh(m_items[i], m_shaderStage[i]);
								}
								if (m_stats[i].Visible && ImGui::MenuItem("Code", KeyboardShortcuts::Instance().GetString("CodeUI.SwitchView").c_str())) m_stats[i].Visible = false;
							}

							if (m_editor[i]) {
								ImGui::Separator();
								if (ImGui::MenuItem("Undo", "CTRL+Z", nullptr, m_editor[i]->CanUndo())) m_editor[i]->Undo();
								if (ImGui::MenuItem("Redo", "CTRL+Y", nullptr, m_editor[i]->CanRedo())) m_editor[i]->Redo();
								ImGui::Separator();
								if (ImGui::MenuItem("Cut", "CTRL+X")) m_editor[i]->Cut();
								if (ImGui::MenuItem("Copy", "CTRL+C")) m_editor[i]->Copy();
								if (ImGui::MenuItem("Paste", "CTRL+V")) m_editor[i]->Paste();
								if (ImGui::MenuItem("Select All", "CTRL+A")) m_editor[i]->SelectAll();
							} else {
								ImGui::Separator();
								if (ImGui::MenuItem("Undo", "CTRL+Z", nullptr, m_pluginEditor[i].Plugin->ShaderEditor_CanUndo(pluginLanguageID, pluginEditorID))) m_pluginEditor[i].Plugin->ShaderEditor_Undo(pluginLanguageID, pluginEditorID);
								if (ImGui::MenuItem("Redo", "CTRL+Y", nullptr, m_pluginEditor[i].Plugin->ShaderEditor_CanRedo(pluginLanguageID, pluginEditorID))) m_pluginEditor[i].Plugin->ShaderEditor_Redo(pluginLanguageID, pluginEditorID);
								ImGui::Separator();
								if (ImGui::MenuItem("Cut", "CTRL+X")) m_pluginEditor[i].Plugin->ShaderEditor_Cut(pluginLanguageID, pluginEditorID);
								if (ImGui::MenuItem("Copy", "CTRL+C")) m_pluginEditor[i].Plugin->ShaderEditor_Copy(pluginLanguageID, pluginEditorID);
								if (ImGui::MenuItem("Paste", "CTRL+V")) m_pluginEditor[i].Plugin->ShaderEditor_Paste(pluginLanguageID, pluginEditorID);
								if (ImGui::MenuItem("Select All", "CTRL+A")) m_pluginEditor[i].Plugin->ShaderEditor_SelectAll(pluginLanguageID, pluginEditorID);
							}
							ImGui::EndMenu();
						}
						ImGui::EndMenuBar();
					}

					if (m_stats[i].Visible) {
						ImGui::PushFont(m_font);
						m_stats[i].Update(delta);
						ImGui::PopFont();
					} else {
						if (m_editor[i]) {
							DrawTextEditor(windowName, m_editor[i]);
						} else {
							PluginShaderEditor* pluginData = &m_pluginEditor[i];
							if (pluginData->Plugin) {
								ImGui::PushFont(m_font);
								pluginData->Plugin->ShaderEditor_Render(pluginData->LanguageID, pluginData->ID);
								ImGui::PopFont();
							}
						}
					}
					if (m_editor[i] && m_editor[i]->IsFocused())
						m_selectedItem = i;
				}
				if (m_focusWindow) {
					if (m_focusPath == m_paths[i]) {
						ImGui::SetWindowFocus();
						m_focusWindow = false;
					}
				}
				wid[isPluginItem ? 4 : static_cast<int>(m_shaderStage[i])]++;
				ImGui::End();
			}
		}

		// save popup
		for (int i = 0; i < m_editorOpen.size(); i++)
			if (!m_editorOpen[i] && (m_editor[i] && m_editor[i]->IsTextChanged())) {
				ImGui::OpenPopup("Save Changes##code_save");
				m_savePopupOpen = i;
				m_editorOpen[i] = true;
				break;
			}

		// Create Item popup
		if (ImGui::BeginPopupModal("Save Changes##code_save")) {
			ImGui::Text("Do you want to save changes?");
			if (ImGui::Button("Yes")) {
				m_save(m_savePopupOpen);
				m_editorOpen[m_savePopupOpen] = false;
				m_savePopupOpen = -1;
				ImGui::CloseCurrentPopup();
			}
			ImGui::SameLine();
			if (ImGui::Button("No")) {
				m_editorOpen[m_savePopupOpen] = false;
				m_savePopupOpen = -1;
				ImGui::CloseCurrentPopup();
			}
			ImGui::SameLine();
			if (ImGui::Button("Cancel")) {
				m_editorOpen[m_savePopupOpen] = true;
				m_savePopupOpen = -1;
				ImGui::CloseCurrentPopup();
			}
			ImGui::EndPopup();
		}

		// save spir-v binary dialog
		if (ifd::FileDialog::Instance().IsDone("SaveSPVBinaryDlg")) {
			if (ifd::FileDialog::Instance().HasResult()) SaveSPVBinary();
			ifd::FileDialog::Instance().Close();
		}

		// save glsl dialog
		if (ifd::FileDialog::Instance().IsDone("SaveGLSLDlg")) {
			if (ifd::FileDialog::Instance().HasResult()) SaveGLSL();
			ifd::FileDialog::Instance().Close();
		}

		// save hlsl dialog
		if (ifd::FileDialog::Instance().IsDone("SaveHLSLDlg")) {
			if (ifd::FileDialog::Instance().HasResult()) SaveHLSL();
			ifd::FileDialog::Instance().Close();
		}

		// delete closed editors
		if (m_savePopupOpen == -1) {
			for (int i = 0; i < m_editorOpen.size(); i++) {
				if (!m_editorOpen[i]) {
					if (m_items[i] && m_items[i]->Type == PipelineItem::ItemType::PluginItem) {
						auto* shader = static_cast<pipe::PluginItemData*>(m_items[i]->Data);
						shader->Owner->CodeEditor_CloseItem(m_paths[i].c_str());
					}

					if (IPlugin1* plugin = m_pluginEditor[i].Plugin)
						plugin->ShaderEditor_Close(m_pluginEditor[i].LanguageID, m_pluginEditor[i].ID);

					m_items.erase(m_items.begin() + i);
					delete m_editor[i];
					m_editor.erase(m_editor.begin() + i);
					m_pluginEditor.erase(m_pluginEditor.begin() + i);
					m_editorOpen.erase(m_editorOpen.begin() + i);
					m_stats.erase(m_stats.begin() + i);
					m_paths.erase(m_paths.begin() + i);
					m_shaderStage.erase(m_shaderStage.begin() + i);
					i--;
				}
			}
		}
	}

	void CodeEditorUI::DrawTextEditor(const std::string& windowName, TextEditor* tEdit)
	{
		if (!tEdit) return;

		int i = 0;
		for (; i < m_editor.size(); i++)
			if (m_editor[i] == tEdit)
				break;

		// add error markers if needed
		const auto msgs = m_data->Messages.GetMessages();
		// int groupMsg = 0;
		TextEditor::ErrorMarkers groupErrs;
		for (auto& j : msgs) {
			const MessageStack::Message* msg = &j;

			if (groupErrs.count(msg->Line))
				continue;

			if (m_items[i] != nullptr && msg->Line > 0 && msg->Group == m_items[i]->Name && msg->Shader == m_shaderStage[i])
				groupErrs[msg->Line] = msg->Text;
		}
		tEdit->SetErrorMarkers(groupErrs);

		bool statusbar = Settings::Instance().Editor.StatusBar;

		// render code
		ImGui::PushFont(m_font);
		m_editor[i]->Render(windowName.c_str(), ImVec2(0, static_cast<float>(-statusbar) * STATUSBAR_HEIGHT));
		if (ImGui::IsItemHovered() && ImGui::GetIO().KeyCtrl && ImGui::GetIO().MouseWheel != 0.0f) {
			Settings::Instance().Editor.FontSize = static_cast<int>(ImGui::GetIO().MouseWheel) + Settings::Instance().Editor.FontSize;
			this->SetFont(Settings::Instance().Editor.FontPath, Settings::Instance().Editor.FontSize);
		}
		ImGui::PopFont();

		// status bar
		if (statusbar) {
			const auto cursor = m_editor[i]->GetCursorPosition();

			ImGui::Separator();
			ImGui::Text("Line %d\tCol %d\tType: %s\tPath: %s", cursor.mLine, cursor.mColumn, m_editor[i]->GetLanguageDefinition().mName.c_str(), m_paths[i].c_str());
		}
	}

	void CodeEditorUI::ChangePluginShaderEditor(IPlugin1* plugin, int langID, int editorID)
	{
		PluginShaderEditor* plEditor = nullptr;

		if (Settings::Instance().General.AutoRecompile) {
			for (auto& i : m_pluginEditor) {
				if (i.Plugin != nullptr)
					if (i.LanguageID == langID && i.ID == editorID && i.Plugin == plugin)
						plEditor = &i;
			}
		}

		if (plEditor) {
			if (std::count(m_changedPluginEditors.begin(), m_changedPluginEditors.end(), plEditor) == 0)
				m_changedPluginEditors.push_back(plEditor);
			m_contentChanged = true;
		}
	}
	PipelineItem* CodeEditorUI::GetPluginEditorPipelineItem(const IPlugin1* plugin, const int langID, const int editorID) const
	{
		PluginShaderEditor* plEditor = nullptr;

		for (int i = 0; i < m_pluginEditor.size(); i++) {
			if (m_pluginEditor[i].LanguageID == langID && m_pluginEditor[i].ID == editorID && m_pluginEditor[i].Plugin == plugin)
				return m_items[i];
		}

		return nullptr;
	}
	void CodeEditorUI::UpdateAutoRecompileItems()
	{
		if (m_contentChanged && m_lastAutoRecompile.GetElapsedTime() > 0.8f) {
			for (auto& m_changedEditor : m_changedEditors) {
				for (int j = 0; j < m_editor.size(); j++) {
					if (m_editor[j] == m_changedEditor && m_items[j] != nullptr) {
						if (m_items[j]->Type == PipelineItem::ItemType::ShaderPass) {
							std::string vs;
							std::string ps;
							std::string gs;
							std::string tcs;
							std::string tes;
							if (m_shaderStage[j] == ShaderStage::Vertex)
								vs = m_editor[j]->GetText();
							else if (m_shaderStage[j] == ShaderStage::Pixel)
								ps = m_editor[j]->GetText();
							else if (m_shaderStage[j] == ShaderStage::Geometry)
								gs = m_editor[j]->GetText();
							else if (m_shaderStage[j] == ShaderStage::TessellationControl)
								tcs = m_editor[j]->GetText();
							else if (m_shaderStage[j] == ShaderStage::TessellationEvaluation)
								tes = m_editor[j]->GetText();
							m_data->Renderer.RecompileFromSource(m_items[j]->Name, vs, ps, gs, tcs, tes);
						} else if ((m_items[j]->Type == PipelineItem::ItemType::ComputePass) || (m_items[j]->Type == PipelineItem::ItemType::AudioPass))
							m_data->Renderer.RecompileFromSource(m_items[j]->Name, m_editor[j]->GetText());
						else if (m_items[j]->Type == PipelineItem::ItemType::PluginItem) {
							std::string pluginCode = m_editor[j]->GetText();
							static_cast<pipe::PluginItemData*>(m_items[j]->Data)->Owner->HandleRecompileFromSource(m_items[j]->Name, static_cast<int>(m_shaderStage[j]), pluginCode.c_str(), static_cast<int>(pluginCode.size()));
						}

						break;
					}
				}
			}
			for (auto& m_changedPluginEditor : m_changedPluginEditors) {
				for (int j = 0; j < m_pluginEditor.size(); j++) {
					int langID = m_changedPluginEditor->LanguageID;
					int editorID = m_changedPluginEditor->ID;
					IPlugin1* plugin = m_changedPluginEditor->Plugin;

					if (langID == m_pluginEditor[j].LanguageID
						&& editorID == m_pluginEditor[j].ID
						&& plugin == m_pluginEditor[j].Plugin) {
						size_t contentLength = 0;
						const char* tempText = plugin->ShaderEditor_GetContent(langID, editorID, &contentLength);

						if (m_items[j]->Type == PipelineItem::ItemType::ShaderPass) {
							std::string vs, ps, gs, tcs, tes;
							if (m_shaderStage[j] == ShaderStage::Vertex)
								vs = std::string(tempText, contentLength);
							else if (m_shaderStage[j] == ShaderStage::Pixel)
								ps = std::string(tempText, contentLength);
							else if (m_shaderStage[j] == ShaderStage::Geometry)
								gs = std::string(tempText, contentLength);
							else if (m_shaderStage[j] == ShaderStage::TessellationControl)
								tcs = std::string(tempText, contentLength);
							else if (m_shaderStage[j] == ShaderStage::TessellationEvaluation)
								tes = std::string(tempText, contentLength);
							m_data->Renderer.RecompileFromSource(m_items[j]->Name, vs, ps, gs, tcs, tes);
						} else if ((m_items[j]->Type == PipelineItem::ItemType::ComputePass) || (m_items[j]->Type == PipelineItem::ItemType::AudioPass))
							m_data->Renderer.RecompileFromSource(m_items[j]->Name, std::string(tempText, contentLength));
						else if (m_items[j]->Type == PipelineItem::ItemType::PluginItem) {
							auto pluginCode = std::string(tempText, contentLength);
							static_cast<pipe::PluginItemData*>(m_items[j]->Data)->Owner->HandleRecompileFromSource(m_items[j]->Name, (int)m_shaderStage[j], pluginCode.c_str(), static_cast<int>(pluginCode.size()));
						}

						break;
					}
				}
			}

			m_changedPluginEditors.clear();
			m_changedEditors.clear();
			m_lastAutoRecompile.Restart();
			m_contentChanged = false;
		}
	}

	void CodeEditorUI::FillAutocomplete(TextEditor* tEdit, const SPIRVParser& spv, bool colorize)
	{
		bool changed = false;

		if (colorize) {
			// check if there are any function changes
			for (const auto& func : spv.Functions) {
				bool funcExists = false;
				for (const auto& editor : tEdit->GetAutocompleteFunctions()) {
					if (editor.first == func.first) {
						funcExists = true;

						// check for argument changes
						for (const auto& arg : func.second.Arguments) {
							bool argExists = false;
							for (const auto& editorArg : editor.second.Arguments) {
								if (arg.Name == editorArg.Name) {
									argExists = true;
									break;
								}
							}
							if (!argExists) changed = true;
						}

						// check for local variable changes
						for (const auto& loc : func.second.Locals) {
							bool locExists = false;
							for (const auto& editorLoc : editor.second.Locals) {
								if (loc.Name == editorLoc.Name) {
									locExists = true;
									break;
								}
							}
							if (!locExists) changed = true;
						}
						break;
					}
				}
				if (!funcExists) changed = true;
			}
			// check if there are any user type changes
			for (const auto& type : spv.UserTypes) {
				bool typeExists = false;
				for (const auto& editor : tEdit->GetAutocompleteUserTypes()) {
					if (type.first == editor.first) {
						typeExists = true;
						break;
					}
				}
				if (!typeExists) changed = true;
			}
			// check if there are any uniform var changes
			for (const auto& unif : spv.Uniforms) {
				bool unifExists = false;
				for (const auto& editor : tEdit->GetAutocompleteUniforms()) {
					if (unif.Name == editor.Name) {
						unifExists = true;
						break;
					}
				}
				if (!unifExists) changed = true;
			}
			// check if there are any global var changes
			for (const auto& glob : spv.Globals) {
				bool globExists = false;
				for (const auto& editor : tEdit->GetAutocompleteGlobals()) {
					if (glob.Name == editor.Name) {
						globExists = true;
						break;
					}
				}
				if (!globExists) changed = true;
			}
		}

		// pass the data to text editor
		tEdit->ClearAutocompleteData();
		tEdit->ClearAutocompleteEntries();

		// spirv parser
		tEdit->SetAutocompleteFunctions(spv.Functions);
		tEdit->SetAutocompleteUserTypes(spv.UserTypes);
		tEdit->SetAutocompleteUniforms(spv.Uniforms);
		tEdit->SetAutocompleteGlobals(spv.Globals);

		// plugins
		plugin::ShaderStage stage = plugin::ShaderStage::Vertex;
		for (int i = 0; i < m_editor.size(); i++)
			if (m_editor[i] == tEdit) {
				stage = static_cast<plugin::ShaderStage>(m_shaderStage[i]);
				break;
			}
		const std::vector<IPlugin1*>& plugins = m_data->Plugins.Plugins();
		for (IPlugin1* plugin : plugins) {
			for (int i = 0; i < plugin->Autocomplete_GetCount(stage); i++) {
				const char* displayString = plugin->Autocomplete_GetDisplayString(stage, i);
				const char* searchString = plugin->Autocomplete_GetSearchString(stage, i);
				const char* value = plugin->Autocomplete_GetValue(stage, i);

				tEdit->AddAutocompleteEntry(searchString, displayString, value);
			}
		}

		// snippets
		for (const auto& snippet : m_snippets)
			if (tEdit->GetLanguageDefinition().mName == snippet.Language || snippet.Language == "*")
				tEdit->AddAutocompleteEntry(snippet.Search, snippet.Display, snippet.Code);

		// colorize if needed
		if (changed)
			tEdit->Colorize();
	}

	void CodeEditorUI::Open(PipelineItem* item, ShaderStage stage)
	{
		std::string shaderPath;
		std::string shaderContent;
		bool externalEditor = Settings::Instance().General.UseExternalEditor;
		SPIRVParser spvData;

		if (item->Type == PipelineItem::ItemType::ShaderPass) {
			auto* shader = static_cast<ed::pipe::ShaderPass*>(item->Data);

			if (stage == ShaderStage::Vertex) {
				shaderPath = shader->VSPath;
				if (!externalEditor) spvData.Parse(shader->VSSPV);
			} else if (stage == ShaderStage::Pixel) {
				shaderPath = shader->PSPath;
				if (!externalEditor) spvData.Parse(shader->PSSPV);
			} else if (stage == ShaderStage::Geometry) {
				shaderPath = shader->GSPath;
				if (!externalEditor) spvData.Parse(shader->GSSPV);
			} else if (stage == ShaderStage::TessellationControl) {
				shaderPath = shader->TCSPath;
				if (!externalEditor) spvData.Parse(shader->TCSSPV);
			} else if (stage == ShaderStage::TessellationEvaluation) {
				shaderPath = shader->TESPath;
				if (!externalEditor) spvData.Parse(shader->TESSPV);
			}
		} else if (item->Type == PipelineItem::ItemType::ComputePass) {
			auto* shader = static_cast<ed::pipe::ComputePass*>(item->Data);
			shaderPath = shader->Path;
			if (!externalEditor) spvData.Parse(shader->SPV);
		} else if (item->Type == PipelineItem::ItemType::AudioPass) {
			auto* shader = static_cast<ed::pipe::AudioPass*>(item->Data);
			shaderPath = shader->Path;
		}

		shaderPath = m_data->Parser.GetProjectPath(shaderPath);

		if (externalEditor) {
			UIHelper::ShellOpen(shaderPath);
			return;
		}

		// check if a file is already opened
		for (const auto& m_path : m_paths) {
			if (m_path == shaderPath) {
				m_focusWindow = true;
				m_focusPath = shaderPath;
				return;
			}
		}

		int langID = 0;
		IPlugin1* plugin = nullptr;
		ShaderLanguage sLang = ShaderCompiler::GetShaderLanguageFromExtension(shaderPath);
		if (sLang == ShaderLanguage::Plugin)
			plugin = ShaderCompiler::GetPluginLanguageFromExtension(&langID, shaderPath, m_data->Plugins.Plugins());

		shaderContent = m_data->Parser.LoadFile(shaderPath);

		m_items.push_back(item);
		m_editorOpen.push_back(true);
		m_stats.emplace_back(m_ui, m_data, "", false);
		m_shaderStage.push_back(stage);
		m_paths.push_back(shaderPath);
		m_pluginEditor.emplace_back();

		if (plugin != nullptr && plugin->ShaderEditor_Supports(langID))
			m_editor.push_back(nullptr);
		else
			m_editor.push_back(new TextEditor());

		TextEditor* editor = m_editor[m_editor.size() - 1];

		if (editor != nullptr) {
			editor->OnContentUpdate = [&](TextEditor* chEditor) {
				if (Settings::Instance().General.AutoRecompile) {
					if (std::count(m_changedEditors.begin(), m_changedEditors.end(), chEditor) == 0)
						m_changedEditors.push_back(chEditor);
					m_contentChanged = true;
				}
			};

			ConfigureTextEditor(editor, shaderPath);

			editor->RequestOpen = [&](TextEditor* tEdit, const std::string& tEditPath, const std::string& path) {
				OpenFile(m_findIncludedFile(tEditPath, path));
			};
			editor->OnCtrlAltClick = [&](TextEditor* tEdit, const std::string& keyword, TextEditor::Coordinates coords) {
				for (int t = 0; t < m_editor.size(); t++)
					if (m_editor[t] == tEdit) {
						dynamic_cast<PreviewUI*>(m_ui->Get(ViewID::Preview))->SetVariableValue(m_items[t], keyword, coords.mLine);
						break;
					}
			};
			m_loadEditorShortcuts(editor);

			if (sLang == ShaderLanguage::HLSL)
				editor->SetLanguageDefinition(TextEditor::LanguageDefinition::HLSL());
			else if (sLang == ShaderLanguage::Plugin) {
				if (plugin->LanguageDefinition_Exists(langID))
					editor->SetLanguageDefinition(m_buildLanguageDefinition(plugin, langID));
			} else
				editor->SetLanguageDefinition(TextEditor::LanguageDefinition::GLSL());

			// apply breakpoints
			m_applyBreakpoints(editor, shaderPath);

			editor->SetText(shaderContent);
			editor->ResetTextChanged();

			FillAutocomplete(editor, spvData, false);
		} else {
			int idMax = -1;
			for (auto& i : m_pluginEditor)
				idMax = std::max<int>(i.ID, idMax);

			PluginShaderEditor* pluginEditor = &m_pluginEditor[m_pluginEditor.size() - 1];
			pluginEditor->LanguageID = langID;
			pluginEditor->Plugin = plugin;
			pluginEditor->ID = idMax + 1;

			m_setupPlugin(pluginEditor->Plugin);

			pluginEditor->Plugin->ShaderEditor_Open(pluginEditor->LanguageID, pluginEditor->ID, shaderContent.c_str(), static_cast<int>(shaderContent.size()));
		}
	}
	void CodeEditorUI::OpenFile(const std::string& path)
	{
		std::string shaderContent;

		if (Settings::Instance().General.UseExternalEditor) {
			UIHelper::ShellOpen(path);
			return;
		}

		// check if a file is already opened
		for (const auto& m_path : m_paths) {
			if (m_path == path) {
				m_focusWindow = true;
				m_focusPath = path;
				return;
			}
		}

		int langID = 0;
		IPlugin1* plugin = nullptr;
		ShaderLanguage sLang = ShaderCompiler::GetShaderLanguageFromExtension(path);
		if (sLang == ShaderLanguage::Plugin)
			plugin = ShaderCompiler::GetPluginLanguageFromExtension(&langID, path, m_data->Plugins.Plugins());

		shaderContent = m_data->Parser.LoadFile(path);

		m_items.push_back(nullptr);
		m_editorOpen.push_back(true);
		m_stats.emplace_back(m_ui, m_data, "", false);
		m_shaderStage.push_back(ShaderStage::Count);
		m_paths.push_back(path);
		m_pluginEditor.emplace_back();

		if (plugin != nullptr && plugin->ShaderEditor_Supports(langID))
			m_editor.push_back(nullptr);
		else
			m_editor.push_back(new TextEditor());

		TextEditor* editor = m_editor[m_editor.size() - 1];

		if (editor != nullptr) {
			editor->OnContentUpdate = [&](TextEditor* chEditor) {
				if (Settings::Instance().General.AutoRecompile) {
					if (std::count(m_changedEditors.begin(), m_changedEditors.end(), chEditor) == 0)
						m_changedEditors.push_back(chEditor);
					m_contentChanged = true;
				}
			};

			ConfigureTextEditor(editor, path);

			editor->RequestOpen = [&](TextEditor* tEdit, const std::string& tEditPath, const std::string& file_path) {
				OpenFile(m_findIncludedFile(tEditPath, file_path));
			};
			editor->OnCtrlAltClick = [&](const TextEditor* tEdit, const std::string& keyword, const TextEditor::Coordinates coords) {
				for (int t = 0; t < m_editor.size(); t++)
					if (m_editor[t] == tEdit) {
						dynamic_cast<PreviewUI*>(m_ui->Get(ViewID::Preview))->SetVariableValue(m_items[t], keyword, coords.mLine);
						break;
					}
			};
			m_loadEditorShortcuts(editor);

			if (sLang == ShaderLanguage::HLSL)
				editor->SetLanguageDefinition(TextEditor::LanguageDefinition::HLSL());
			else if (sLang == ShaderLanguage::Plugin) {
				if (plugin->LanguageDefinition_Exists(langID))
					editor->SetLanguageDefinition(m_buildLanguageDefinition(plugin, langID));
			} else
				editor->SetLanguageDefinition(TextEditor::LanguageDefinition::GLSL());

			editor->SetText(shaderContent);
			editor->ResetTextChanged();
		} else {
			int idMax = -1;
			for (auto& i : m_pluginEditor)
				idMax = std::max<int>(i.ID, idMax);

			PluginShaderEditor* pluginEditor = &m_pluginEditor[m_pluginEditor.size() - 1];
			pluginEditor->LanguageID = langID;
			pluginEditor->Plugin = plugin;
			pluginEditor->ID = idMax + 1;

			m_setupPlugin(pluginEditor->Plugin);

			pluginEditor->Plugin->ShaderEditor_Open(pluginEditor->LanguageID, pluginEditor->ID, shaderContent.c_str(), static_cast<int>(shaderContent.size()));
		}
	}
	void CodeEditorUI::OpenPluginCode(PipelineItem* item, const char* filepath, int id)
	{
		if (item->Type != PipelineItem::ItemType::PluginItem)
			return;

		const auto* shader = static_cast<pipe::PluginItemData*>(item->Data);

		if (Settings::Instance().General.UseExternalEditor) {
			const auto path = m_data->Parser.GetProjectPath(filepath);
			UIHelper::ShellOpen(path);
			return;
		}

		// check if already opened
		for (const auto& m_path : m_paths) {
			if (m_path == filepath) {
				m_focusWindow = true;
				m_focusPath = filepath;
				return;
			}
		}

		auto shaderStage = static_cast<ShaderStage>(id);

		int langID = 0;
		IPlugin1* plugin = nullptr;
		const ShaderLanguage sLang = ShaderCompiler::GetShaderLanguageFromExtension(filepath);
		if (sLang == ShaderLanguage::Plugin)
			plugin = ShaderCompiler::GetPluginLanguageFromExtension(&langID, filepath, m_data->Plugins.Plugins());

		m_items.push_back(item);
		m_editorOpen.push_back(true);
		m_stats.emplace_back(m_ui, m_data, "", false);
		m_paths.emplace_back(filepath);
		m_shaderStage.push_back(shaderStage);
		m_pluginEditor.emplace_back();

		if (plugin != nullptr && plugin->ShaderEditor_Supports(langID))
			m_editor.push_back(nullptr);
		else
			m_editor.push_back(new TextEditor());

		TextEditor* editor = m_editor[m_editor.size() - 1];
		std::string shaderContent = m_data->Parser.LoadProjectFile(filepath);

		if (editor != nullptr) {
			editor->OnContentUpdate = [&](TextEditor* chEditor) {
				if (Settings::Instance().General.AutoRecompile) {
					if (std::count(m_changedEditors.begin(), m_changedEditors.end(), chEditor) == 0)
						m_changedEditors.push_back(chEditor);
					m_contentChanged = true;
				}
			};

			TextEditor::LanguageDefinition defPlugin = m_buildLanguageDefinition(shader->Owner, id);

			ConfigureTextEditor(editor, filepath);

			editor->RequestOpen = [&](TextEditor* tEdit, const std::string& tEditPath, const std::string& path) {
				OpenFile(m_findIncludedFile(tEditPath, path));
			};
			editor->OnCtrlAltClick = [&](TextEditor* tEdit, const std::string& keyword, TextEditor::Coordinates coords) {
				for (int t = 0; t < m_editor.size(); t++)
					if (m_editor[t] == tEdit) {
						dynamic_cast<PreviewUI*>(m_ui->Get(ViewID::Preview))->SetVariableValue(m_items[t], keyword, coords.mLine);
						break;
					}
			};
			m_loadEditorShortcuts(editor);

			unsigned int spvSize = shader->Owner->PipelineItem_GetSPIRVSize(shader->Type, shader->PluginData, static_cast<plugin::ShaderStage>(shaderStage));
			if (spvSize > 0) {
				unsigned int* spv = shader->Owner->PipelineItem_GetSPIRV(shader->Type, shader->PluginData, static_cast<plugin::ShaderStage>(shaderStage));
				std::vector<unsigned int> spvVec(spv, spv + spvSize);

				SPIRVParser spvData;
				spvData.Parse(spvVec);
				FillAutocomplete(editor, spvData, false);
			}

			// apply breakpoints
			m_applyBreakpoints(editor, filepath);

			editor->SetLanguageDefinition(defPlugin);

			editor->SetText(shaderContent);
			editor->ResetTextChanged();
		} else {
			int idMax = -1;
			for (auto& i : m_pluginEditor)
				idMax = std::max<int>(i.ID, idMax);

			PluginShaderEditor* pluginEditor = &m_pluginEditor[m_pluginEditor.size() - 1];
			pluginEditor->LanguageID = langID;
			pluginEditor->Plugin = plugin;
			pluginEditor->ID = idMax + 1;

			m_setupPlugin(pluginEditor->Plugin);

			pluginEditor->Plugin->ShaderEditor_Open(pluginEditor->LanguageID, pluginEditor->ID, shaderContent.c_str(), static_cast<int>(shaderContent.size()));
		}
	}
	TextEditor* CodeEditorUI::Get(const PipelineItem* item, const ShaderStage stage) const
	{
		for (int i = 0; i < m_items.size(); i++)
			if (m_items[i] == item && m_shaderStage[i] == stage)
				return m_editor[i];
		return nullptr;
	}
	TextEditor* CodeEditorUI::Get(const std::string& path) const
	{
		for (int i = 0; i < m_items.size(); i++)
			if (m_paths[i] == path)
				return m_editor[i];
		return nullptr;
	}

	void CodeEditorUI::CloseAll(PipelineItem* item)
	{
		for (int i = 0; i < m_items.size(); i++) {
			if (m_items[i] == item || item == nullptr) {
				if (m_items[i]->Type == PipelineItem::ItemType::PluginItem) {
					const auto* shader = static_cast<pipe::PluginItemData*>(m_items[i]->Data);
					shader->Owner->CodeEditor_CloseItem(m_paths[i].c_str());
				}

				delete m_editor[i];

				m_items.erase(m_items.begin() + i);
				m_editor.erase(m_editor.begin() + i);
				m_pluginEditor.erase(m_pluginEditor.begin() + i);
				m_editorOpen.erase(m_editorOpen.begin() + i);
				m_stats.erase(m_stats.begin() + i);
				m_paths.erase(m_paths.begin() + i);
				m_shaderStage.erase(m_shaderStage.begin() + i);
				i--;
			}
		}
	}

	std::vector<std::pair<std::string, ShaderStage>> CodeEditorUI::GetOpenedFiles()
	{
		std::vector<std::pair<std::string, ShaderStage>> ret;
		for (int i = 0; i < m_items.size(); i++)
			if (m_items[i] != nullptr)
				ret.emplace_back(std::string(m_items[i]->Name), m_shaderStage[i]);
		return ret;
	}
	std::vector<std::string> CodeEditorUI::GetOpenedFilesData() const
	{
		std::vector<std::string> ret;
		for (int i = 0; i < m_items.size(); i++)
			ret.push_back(m_editor[i]->GetText());
		return ret;
	}
	void CodeEditorUI::SetOpenedFilesData(const std::vector<std::string>& data) const
	{
		for (int i = 0; i < m_items.size() && i < data.size(); i++)
			m_editor[i]->SetText(data[i]);
	}

	std::string CodeEditorUI::m_findIncludedFile(const std::string& relativeTo, const std::string& path) const
	{
		for (const auto& p : Settings::Instance().Project.IncludePaths) {
			auto ret = std::filesystem::path(p) / path;

			if (!ret.is_absolute()) {
				auto projectPath = m_data->Parser.GetProjectPath(ret.u8string());
				if (std::filesystem::exists(projectPath))
					return projectPath;
			}

			if (std::filesystem::exists(ret))
				return ret.u8string();
		}

		auto ret = std::filesystem::path(relativeTo);
		if (ret.has_parent_path())
			ret = ret.parent_path() / path;
		if (std::filesystem::exists(ret))
			return ret.u8string();

		return path;
	}
	void CodeEditorUI::m_applyBreakpoints(TextEditor* editor, const std::string& path) const
	{
		const std::vector<dbg::Breakpoint>& bkpts = m_data->Debugger.GetBreakpointList(path);
		const std::vector<bool>& states = m_data->Debugger.GetBreakpointStateList(path);

		for (size_t i = 0; i < bkpts.size(); i++)
			editor->AddBreakpoint(bkpts[i].Line, bkpts[i].IsConditional, bkpts[i].Condition, states[i]);

		editor->OnBreakpointRemove = [&](TextEditor* ed, int line) {
			m_data->Debugger.RemoveBreakpoint(ed->GetPath(), line);
		};
		editor->OnBreakpointUpdate = [&](TextEditor* ed, int line, bool useCond, const std::string& cond, bool enabled) {
			m_data->Debugger.AddBreakpoint(ed->GetPath(), line, useCond, cond, enabled);
		};
	}

	void CodeEditorUI::StopDebugging() const
	{
		for (int i = 0; i < m_editor.size(); i++) {
			if (m_editor[i])
				m_editor[i]->SetCurrentLineIndicator(-1);
			else {
				if (m_pluginEditor[i].Plugin->GetVersion() >= 3)
					dynamic_cast<IPlugin3*>(m_pluginEditor[i].Plugin)->ShaderEditor_SetLineIndicator(m_pluginEditor[i].LanguageID, m_pluginEditor[i].ID, -1);
			}
		}
	}
	TextEditor::LanguageDefinition CodeEditorUI::m_buildLanguageDefinition(IPlugin1* plugin, int languageID)
	{
		TextEditor::LanguageDefinition langDef;

		const int keywordCount = plugin->LanguageDefinition_GetKeywordCount(languageID);
		const char** keywords = plugin->LanguageDefinition_GetKeywords(languageID);

		for (int i = 0; i < keywordCount; i++)
			langDef.mKeywords.insert(keywords[i]);

		const int identifierCount = plugin->LanguageDefinition_GetIdentifierCount(languageID);
		for (int i = 0; i < identifierCount; i++) {
			const char* ident = plugin->LanguageDefinition_GetIdentifier(i, languageID);
			const char* identDesc = plugin->LanguageDefinition_GetIdentifierDesc(i, languageID);
			langDef.mIdentifiers.insert(std::make_pair(ident, TextEditor::Identifier(identDesc)));
		}
		// m_GLSLDocumentation(langDef.mIdentifiers);

		int tokenRegexs = plugin->LanguageDefinition_GetTokenRegexCount(languageID);
		for (int i = 0; i < tokenRegexs; i++) {
			plugin::TextEditorPaletteIndex palIndex;
			const char* regStr = plugin->LanguageDefinition_GetTokenRegex(i, palIndex, languageID);
			langDef.mTokenRegexStrings.push_back(std::make_pair<std::string, TextEditor::PaletteIndex>(regStr, (TextEditor::PaletteIndex)palIndex));
		}

		langDef.mCommentStart = plugin->LanguageDefinition_GetCommentStart(languageID);
		langDef.mCommentEnd = plugin->LanguageDefinition_GetCommentEnd(languageID);
		langDef.mSingleLineComment = plugin->LanguageDefinition_GetLineComment(languageID);

		langDef.mCaseSensitive = plugin->LanguageDefinition_IsCaseSensitive(languageID);
		langDef.mAutoIndentation = plugin->LanguageDefinition_GetAutoIndent(languageID);

		langDef.mName = plugin->LanguageDefinition_GetName(languageID);

		return langDef;
	}
}