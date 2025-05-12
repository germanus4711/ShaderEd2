//
// Created by André Krützfeldt on 5/10/25.
//

#include "GuiManagerUpdate.h"
#include "GUIManager.h"
#include "Objects/CameraSnapshots.h"
#include "Objects/FunctionVariableManager.h"
#include "Objects/ShaderCompiler.h"
#include "Objects/SystemVariableManager.h"
#include "UI/CodeEditorUI.h"
#include "UI/ObjectPreviewUI.h"
#include "UI/OptionsUI.h"
#include "UI/PreviewUI.h"
#include "UI/UIHelper.h"
#include "misc/ImFileDialog.h"

namespace ed {
	void GUIManager::Update(float delta)
	{
		// add star to the titlebar if project was modified
		if (m_cacheProjectModified != m_data->Parser.IsProjectModified()) {
			std::string projName = m_data->Parser.GetOpenedFile();

			if (!projName.empty()) {
				projName = projName.substr(projName.find_last_of("/\\") + 1);

				if (m_data->Parser.IsProjectModified())
					projName = "*" + projName;

				SDL_SetWindowTitle(m_wnd, ("SHADERed (" + projName + ")").c_str());
			} else {
				if (m_data->Parser.IsProjectModified())
					SDL_SetWindowTitle(m_wnd, "SHADERed (*)");
				else
					SDL_SetWindowTitle(m_wnd, "SHADERed");
			}

			m_cacheProjectModified = m_data->Parser.IsProjectModified();
		}

		Settings& settings = Settings::Instance();
		m_performanceMode = m_perfModeFake;
		m_focusMode = m_focusModeTemp;

		// reset FunctionVariableManager
		FunctionVariableManager::Instance().ClearVariableList();

		// update editor & workspace font
		if (dynamic_cast<CodeEditorUI*>(Get(ViewID::Code))->NeedsFontUpdate() || ((m_cachedFont != settings.General.Font || m_cachedFontSize != settings.General.FontSize) && strcmp(settings.General.Font, "null") != 0) || m_fontNeedsUpdate) {
			Logger::Get().Log("Updating fonts...");

			std::pair<std::string, int> edFont = dynamic_cast<CodeEditorUI*>(Get(ViewID::Code))->GetFont();

			m_cachedFont = settings.General.Font;
			m_cachedFontSize = settings.General.FontSize;
			m_fontNeedsUpdate = false;

			ImFontAtlas* fonts = ImGui::GetIO().Fonts;
			fonts->Clear();

			ImFont* font = nullptr;
			if (std::filesystem::exists(m_cachedFont))
				font = fonts->AddFontFromFileTTF(m_cachedFont.c_str(), static_cast<float>(m_cachedFontSize) * Settings::Instance().DPIScale);

			// icon font
			static constexpr ImWchar icon_ranges[] = { 0xea5b, 0xf026, 0 };
			if (font && std::filesystem::exists("data/icofont.ttf")) {
				ImFontConfig config;
				config.MergeMode = true;
				fonts->AddFontFromFileTTF("data/icofont.ttf", static_cast<float>(m_cachedFontSize) * Settings::Instance().DPIScale, &config, icon_ranges);
			}

			ImFont* edFontPtr = nullptr;
			if (std::filesystem::exists(edFont.first))
				edFontPtr = fonts->AddFontFromFileTTF(edFont.first.c_str(), edFont.second * Settings::Instance().DPIScale);

			if (font == nullptr || edFontPtr == nullptr) {
				fonts->Clear();
				font = fonts->AddFontDefault();
				edFontPtr = fonts->AddFontDefault();

				Logger::Get().Log("Failed to load fonts", true);
			}

			// icon font large
			if (std::filesystem::exists("data/icofont.ttf")) {
				ImFontConfig configIconsLarge;
				m_iconFontLarge = ImGui::GetIO().Fonts->AddFontFromFileTTF("data/icofont.ttf", Settings::Instance().CalculateSize(TOOLBAR_HEIGHT / 2), &configIconsLarge, icon_ranges);
			}

			ImGui::GetIO().FontDefault = font;
			fonts->Build();

			ImGui_ImplOpenGL3_DestroyFontsTexture();
			ImGui_ImplOpenGL3_CreateFontsTexture();

			dynamic_cast<CodeEditorUI*>(Get(ViewID::Code))->UpdateFont();
			dynamic_cast<OptionsUI*>(Get(ViewID::Options))->ApplyTheme(); // reset paddings, etc...
		}

		// Start the Dear ImGui frame
		ImGui_ImplOpenGL3_NewFrame();
		ImGui_ImplSDL2_NewFrame(m_wnd);
		ImGui::NewFrame();

		// splash screen
		if (m_splashScreen) {
			m_splashScreenRender();
			return;
		}

		// toolbar
		static bool initializedToolbar = false;
		bool actuallyToolbar = settings.General.Toolbar && !m_performanceMode && !m_minimalMode && !m_focusMode;
		if (!initializedToolbar) { // some hacks ew
			m_renderToolbar();
			initializedToolbar = true;
		} else if (actuallyToolbar)
			m_renderToolbar();

		// create a fullscreen imgui panel that will host a dock space
		bool showMenu = !m_minimalMode && !(m_performanceMode && settings.Preview.HideMenuInPerformanceMode && m_perfModeClock.GetElapsedTime() > 2.5f);
		ImGuiWindowFlags window_flags = ImGuiWindowFlags_NoBringToFrontOnFocus | ImGuiWindowFlags_NoNavFocus | (ImGuiWindowFlags_MenuBar * showMenu) | ImGuiWindowFlags_NoDocking | ImGuiWindowFlags_NoTitleBar | ImGuiWindowFlags_NoCollapse | ImGuiWindowFlags_NoResize | ImGuiWindowFlags_NoMove;
		ImGuiViewport* viewport = ImGui::GetMainViewport();
		ImGui::SetNextWindowPos(ImVec2(viewport->Pos.x, viewport->Pos.y + actuallyToolbar * Settings::Instance().CalculateSize(TOOLBAR_HEIGHT)));
		ImGui::SetNextWindowSize(ImVec2(viewport->Size.x, viewport->Size.y - actuallyToolbar * Settings::Instance().CalculateSize(TOOLBAR_HEIGHT)));
		ImGui::SetNextWindowViewport(viewport->ID);
		ImGui::PushStyleVar(ImGuiStyleVar_WindowRounding, 0.0f);
		ImGui::PushStyleVar(ImGuiStyleVar_WindowBorderSize, 0.0f);
		ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(0.0f, 0.0f));
		ImGui::Begin("DockSpaceWnd", 0, window_flags);
		ImGui::PopStyleVar(3);

		// DockSpace
		if (ImGui::GetIO().ConfigFlags & ImGuiConfigFlags_DockingEnable && !m_performanceMode && !m_minimalMode && !m_focusMode) {
			ImGuiID dockspace_id = ImGui::GetID("DockSpace");
			ImGui::DockSpace(dockspace_id, ImVec2(0.0f, 0.0f), ImGuiDockNodeFlags_None);
		}

		// rebuild
		if (((CodeEditorUI*)Get(ViewID::Code))->TrackedFilesNeedUpdate()) {
			if (!m_recompiledAll) {
				std::vector<bool> needsUpdate = ((CodeEditorUI*)Get(ViewID::Code))->TrackedNeedsUpdate();
				std::vector<PipelineItem*> passes = m_data->Pipeline.GetList();
				int ind = 0;
				if (needsUpdate.size() >= passes.size()) {
					for (PipelineItem*& pass : passes) {
						if (needsUpdate[ind])
							m_data->Renderer.Recompile(pass->Name);
						ind++;
					}
				}
			}

			static_cast<CodeEditorUI*>(Get(ViewID::Code))->EmptyTrackedFiles();
		}
		static_cast<CodeEditorUI*>(Get(ViewID::Code))->UpdateAutoRecompileItems();

		// parse
		if (!m_data->Renderer.SPIRVQueue.empty()) {
			auto& spvQueue = m_data->Renderer.SPIRVQueue;
			CodeEditorUI* codeEditor = ((CodeEditorUI*)Get(ViewID::Code));
			for (int i = 0; i < spvQueue.size(); i++) {
				bool hasDups = false;

				PipelineItem* spvItem = spvQueue[i];

				if (i + 1 < spvQueue.size())
					if (std::count(spvQueue.begin() + i + 1, spvQueue.end(), spvItem) > 0)
						hasDups = true;

				if (!hasDups) {
					SPIRVParser spvParser;
					if (spvItem->Type == PipelineItem::ItemType::ShaderPass) {
						auto pass = static_cast<pipe::ShaderPass*>(spvItem->Data);
						std::vector<std::string> allUniforms;

						bool deleteUnusedVariables = true;

						if (pass->PSSPV.size() > 0) {
							int langID = -1;
							IPlugin1* plugin = ShaderCompiler::GetPluginLanguageFromExtension(&langID, pass->PSPath, m_data->Plugins.Plugins());

							deleteUnusedVariables &= (plugin == nullptr || (plugin != nullptr && plugin->CustomLanguage_SupportsAutoUniforms(langID)));

							spvParser.Parse(pass->PSSPV);
							TextEditor* tEdit = codeEditor->Get(spvItem, ed::ShaderStage::Pixel);
							if (tEdit != nullptr) codeEditor->FillAutocomplete(tEdit, spvParser);
							if (settings.General.AutoUniforms && (plugin == nullptr || (plugin != nullptr && plugin->CustomLanguage_SupportsAutoUniforms(langID))))
								m_autoUniforms(pass->Variables, spvParser, allUniforms);
						}
						if (pass->VSSPV.size() > 0) {
							int langID = -1;
							IPlugin1* plugin = ShaderCompiler::GetPluginLanguageFromExtension(&langID, pass->VSPath, m_data->Plugins.Plugins());

							deleteUnusedVariables &= (plugin == nullptr || (plugin != nullptr && plugin->CustomLanguage_SupportsAutoUniforms(langID)));

							spvParser.Parse(pass->VSSPV);
							TextEditor* tEdit = codeEditor->Get(spvItem, ed::ShaderStage::Vertex);
							if (tEdit != nullptr) codeEditor->FillAutocomplete(tEdit, spvParser);
							if (settings.General.AutoUniforms && (plugin == nullptr || (plugin != nullptr && plugin->CustomLanguage_SupportsAutoUniforms(langID))))
								m_autoUniforms(pass->Variables, spvParser, allUniforms);
						}
						if (pass->GSSPV.size() > 0) {
							int langID = -1;
							IPlugin1* plugin = ShaderCompiler::GetPluginLanguageFromExtension(&langID, pass->GSPath, m_data->Plugins.Plugins());

							deleteUnusedVariables &= (plugin == nullptr || (plugin != nullptr && plugin->CustomLanguage_SupportsAutoUniforms(langID)));

							spvParser.Parse(pass->GSSPV);
							TextEditor* tEdit = codeEditor->Get(spvItem, ed::ShaderStage::Geometry);
							if (tEdit != nullptr) codeEditor->FillAutocomplete(tEdit, spvParser);
							if (settings.General.AutoUniforms && (plugin == nullptr || (plugin != nullptr && plugin->CustomLanguage_SupportsAutoUniforms(langID))))
								m_autoUniforms(pass->Variables, spvParser, allUniforms);
						}
						if (pass->TCSSPV.size() > 0) {
							int langID = -1;
							IPlugin1* plugin = ShaderCompiler::GetPluginLanguageFromExtension(&langID, pass->TCSPath, m_data->Plugins.Plugins());

							deleteUnusedVariables &= (plugin == nullptr || (plugin != nullptr && plugin->CustomLanguage_SupportsAutoUniforms(langID)));

							spvParser.Parse(pass->TCSSPV);
							TextEditor* tEdit = codeEditor->Get(spvItem, ed::ShaderStage::TessellationControl);
							if (tEdit != nullptr) codeEditor->FillAutocomplete(tEdit, spvParser);
							if (settings.General.AutoUniforms && (plugin == nullptr || (plugin != nullptr && plugin->CustomLanguage_SupportsAutoUniforms(langID))))
								m_autoUniforms(pass->Variables, spvParser, allUniforms);
						}
						if (pass->TESSPV.size() > 0) {
							int langID = -1;
							IPlugin1* plugin = ShaderCompiler::GetPluginLanguageFromExtension(&langID, pass->TESPath, m_data->Plugins.Plugins());

							deleteUnusedVariables &= (plugin == nullptr || (plugin != nullptr && plugin->CustomLanguage_SupportsAutoUniforms(langID)));

							spvParser.Parse(pass->TESSPV);
							TextEditor* tEdit = codeEditor->Get(spvItem, ed::ShaderStage::TessellationEvaluation);
							if (tEdit != nullptr) codeEditor->FillAutocomplete(tEdit, spvParser);
							if (settings.General.AutoUniforms && (plugin == nullptr || (plugin != nullptr && plugin->CustomLanguage_SupportsAutoUniforms(langID))))
								m_autoUniforms(pass->Variables, spvParser, allUniforms);
						}

						if (settings.General.AutoUniforms && deleteUnusedVariables && settings.General.AutoUniformsDelete && pass->VSSPV.size() > 0 && pass->PSSPV.size() > 0 && ((pass->GSUsed && pass->GSSPV.size() > 0) || !pass->GSUsed) && ((pass->TSUsed && pass->TCSSPV.size() > 0 && pass->TESSPV.size() > 0) || !pass->TSUsed))
							m_deleteUnusedUniforms(pass->Variables, allUniforms);
					} else if (spvItem->Type == PipelineItem::ItemType::ComputePass) {
						pipe::ComputePass* pass = (pipe::ComputePass*)spvItem->Data;
						std::vector<std::string> allUniforms;

						if (pass->SPV.size() > 0) {
							int langID = -1;
							IPlugin1* plugin = ShaderCompiler::GetPluginLanguageFromExtension(&langID, pass->Path, m_data->Plugins.Plugins());

							spvParser.Parse(pass->SPV);
							TextEditor* tEdit = codeEditor->Get(spvItem, ed::ShaderStage::Compute);
							if (tEdit != nullptr) codeEditor->FillAutocomplete(tEdit, spvParser);
							if (settings.General.AutoUniforms && (plugin == nullptr || (plugin != nullptr && plugin->CustomLanguage_SupportsAutoUniforms(langID)))) {
								m_autoUniforms(pass->Variables, spvParser, allUniforms);
								if (settings.General.AutoUniformsDelete)
									m_deleteUnusedUniforms(pass->Variables, allUniforms);
							}
						}
					} else if (spvItem->Type == PipelineItem::ItemType::PluginItem) {
						pipe::PluginItemData* pass = (pipe::PluginItemData*)spvItem->Data;
						std::vector<std::string> allUniforms;

						for (int k = 0; k < (int)ShaderStage::Count; k++) {
							TextEditor* tEdit = codeEditor->Get(spvItem, (ed::ShaderStage)k);
							if (tEdit == nullptr) continue;

							unsigned int spvSize = pass->Owner->PipelineItem_GetSPIRVSize(pass->Type, pass->PluginData, (plugin::ShaderStage)k);
							if (spvSize > 0) {
								unsigned int* spv = pass->Owner->PipelineItem_GetSPIRV(pass->Type, pass->PluginData, (plugin::ShaderStage)k);
								std::vector<unsigned int> spvVec(spv, spv + spvSize);

								spvParser.Parse(spvVec);
								codeEditor->FillAutocomplete(tEdit, spvParser);
							}
						}
					}
				}
			}
			spvQueue.clear();
		}

		// menu
		if (showMenu && ImGui::BeginMainMenuBar()) {
			if (ImGui::BeginMenu("File")) {
				if (ImGui::BeginMenu("New")) {
					if (ImGui::MenuItem("Empty")) {
						bool cont = true;
						if (m_data->Parser.IsProjectModified()) {
							int btnID = this->AreYouSure();
							if (btnID == 2)
								cont = false;
						}

						if (cont) {
							m_selectedTemplate = "?empty";
							m_createNewProject();
						}
					}
					ImGui::Separator();

					for (int i = 0; i < m_templates.size(); i++)
						if (ImGui::MenuItem(m_templates[i].c_str())) {
							bool cont = true;
							if (m_data->Parser.IsProjectModified()) {
								int btnID = this->AreYouSure();
								if (btnID == 2)
									cont = false;
							}

							if (cont) {
								m_selectedTemplate = m_templates[i];
								m_createNewProject();
							}
						}

					m_data->Plugins.ShowMenuItems("newproject");

					ImGui::EndMenu();
				}
				if (ImGui::MenuItem("Open", KeyboardShortcuts::Instance().GetString("Project.Open").c_str())) {

					bool cont = true;
					if (m_data->Parser.IsProjectModified()) {
						int btnID = this->AreYouSure();
						if (btnID == 2)
							cont = false;
					}

					if (cont)
						ifd::FileDialog::Instance().Open("OpenProjectDlg", "Open SHADERed project", "SHADERed project (*.sprj){.sprj},.*");
				}
				if (ImGui::BeginMenu("Open Recent")) {
					int recentCount = 0;
					for (int i = 0; i < m_recentProjects.size(); i++) {
						std::filesystem::path path(m_recentProjects[i]);
						if (!std::filesystem::exists(path))
							continue;

						recentCount++;
						if (ImGui::MenuItem(path.filename().string().c_str())) {
							bool cont = true;
							if (m_data->Parser.IsProjectModified()) {
								int btnID = this->AreYouSure();
								if (btnID == 2)
									cont = false;
							}

							if (cont)
								this->Open(m_recentProjects[i]);
						}
					}

					if (recentCount == 0)
						ImGui::Text("No projects opened recently");

					ImGui::EndMenu();
				}
				if (ImGui::MenuItem("Browse online")) {
					m_isBrowseOnlineOpened = true;
				}
				if (ImGui::MenuItem("Save", KeyboardShortcuts::Instance().GetString("Project.Save").c_str()))
					Save();
				if (ImGui::MenuItem("Save As", KeyboardShortcuts::Instance().GetString("Project.SaveAs").c_str()))
					SaveAsProject(true);
				if (ImGui::MenuItem("Save Preview as Image", KeyboardShortcuts::Instance().GetString("Preview.SaveImage").c_str()))
					m_savePreviewPopupOpened = true;
				if (ImGui::MenuItem("Open project directory")) {
					std::string prpath = m_data->Parser.GetProjectPath("");
#if defined(__APPLE__)
					system(("open " + prpath).c_str()); // [MACOS]
#elif defined(__linux__) || defined(__unix__)
					system(("xdg-open " + prpath).c_str());
#elif defined(_WIN32)
					ShellExecuteA(NULL, "open", prpath.c_str(), NULL, NULL, SW_SHOWNORMAL);
#endif
				}

				if (ImGui::BeginMenu("Export")) {
					if (ImGui::MenuItem("as C++ project"))
						m_exportAsCPPOpened = true;

					ImGui::EndMenu();
				}

				m_data->Plugins.ShowMenuItems("file");

				ImGui::Separator();
				if (ImGui::MenuItem("Exit", KeyboardShortcuts::Instance().GetString("Window.Exit").c_str())) {
					SDL_Event event;
					event.type = SDL_QUIT;
					SDL_PushEvent(&event);
					return;
				}

				ImGui::EndMenu();
			}
			if (ImGui::BeginMenu("Project")) {
				if (ImGui::MenuItem("Rebuild project", KeyboardShortcuts::Instance().GetString("Project.Rebuild").c_str())) {
					((CodeEditorUI*)Get(ViewID::Code))->SaveAll();

					std::vector<PipelineItem*> passes = m_data->Pipeline.GetList();
					for (PipelineItem*& pass : passes)
						m_data->Renderer.Recompile(pass->Name);
				}
				if (ImGui::MenuItem("Render", KeyboardShortcuts::Instance().GetString("Preview.SaveImage").c_str()))
					m_savePreviewPopupOpened = true;
				if (ImGui::BeginMenu("Create")) {
					if (ImGui::MenuItem("Shader Pass", KeyboardShortcuts::Instance().GetString("Project.NewShaderPass").c_str()))
						this->CreateNewShaderPass();
					if (ImGui::MenuItem("Compute Pass", KeyboardShortcuts::Instance().GetString("Project.NewComputePass").c_str()))
						this->CreateNewComputePass();
					if (ImGui::MenuItem("Audio Pass", KeyboardShortcuts::Instance().GetString("Project.NewAudioPass").c_str()))
						this->CreateNewAudioPass();
					ImGui::Separator();
					if (ImGui::MenuItem("Texture", KeyboardShortcuts::Instance().GetString("Project.NewTexture").c_str()))
						this->CreateNewTexture();
					if (ImGui::MenuItem("Texture 3D", KeyboardShortcuts::Instance().GetString("Project.NewTexture3D").c_str()))
						this->CreateNewTexture3D();
					if (ImGui::MenuItem("Cubemap", KeyboardShortcuts::Instance().GetString("Project.NewCubeMap").c_str()))
						this->CreateNewCubemap();
					if (ImGui::MenuItem("Audio", KeyboardShortcuts::Instance().GetString("Project.NewAudio").c_str()))
						this->CreateNewAudio();
					if (ImGui::MenuItem("Render Texture", KeyboardShortcuts::Instance().GetString("Project.NewRenderTexture").c_str()))
						this->CreateNewRenderTexture();
					if (ImGui::MenuItem("Buffer", KeyboardShortcuts::Instance().GetString("Project.NewBuffer").c_str()))
						this->CreateNewBuffer();
					if (ImGui::MenuItem("Empty image", KeyboardShortcuts::Instance().GetString("Project.NewImage").c_str()))
						this->CreateNewImage();
					if (ImGui::MenuItem("Empty 3D image", KeyboardShortcuts::Instance().GetString("Project.NewImage3D").c_str()))
						this->CreateNewImage3D();

					bool hasKBTexture = m_data->Objects.HasKeyboardTexture();
					if (hasKBTexture) {
						ImGui::PushItemFlag(ImGuiItemFlags_Disabled, true);
						ImGui::PushStyleVar(ImGuiStyleVar_Alpha, ImGui::GetStyle().Alpha * 0.5f);
					}
					if (ImGui::MenuItem("KeyboardTexture", KeyboardShortcuts::Instance().GetString("Project.NewKeyboardTexture").c_str()))
						this->CreateKeyboardTexture();
					if (hasKBTexture) {
						ImGui::PopStyleVar();
						ImGui::PopItemFlag();
					}

					m_data->Plugins.ShowMenuItems("createitem");

					ImGui::EndMenu();
				}
				if (ImGui::BeginMenu("Camera snapshots")) {
					if (ImGui::MenuItem("Add", KeyboardShortcuts::Instance().GetString("Project.CameraSnapshot").c_str())) CreateNewCameraSnapshot();
					if (ImGui::BeginMenu("Delete")) {
						auto& names = CameraSnapshots::GetList();
						for (const auto& name : names)
							if (ImGui::MenuItem(name.c_str()))
								CameraSnapshots::Remove(name);
						ImGui::EndMenu();
					}
					ImGui::EndMenu();
				}
				if (ImGui::MenuItem("Reset time")) {
					SystemVariableManager::Instance().Reset();
					if (!m_data->Debugger.IsDebugging() && m_data->Debugger.GetPixelList().size() == 0)
						m_data->Renderer.Render();
				}
				if (ImGui::MenuItem("Reload textures")) {
					std::vector<ObjectManagerItem*>& objs = m_data->Objects.GetObjects();

					for (const auto& obj : objs) {
						if (obj->Type == ObjectType::Texture)
							m_data->Objects.ReloadTexture(obj, obj->Name);
					}
				}
				if (ImGui::MenuItem("Options")) {
					m_optionsOpened = true;
					((OptionsUI*)m_options)->SetGroup(ed::OptionsUI::Page::Project);
					m_optGroup = (int)OptionsUI::Page::Project;
					*m_settingsBkp = settings;
					m_shortcutsBkp = KeyboardShortcuts::Instance().GetMap();
				}

				m_data->Plugins.ShowMenuItems("project");

				ImGui::EndMenu();
			}
			if (ImGui::BeginMenu("Window")) {
				// casual window controls
				for (auto& view : m_views) {
					if (view->Name != "Code") // dont show the "Code" UI view in this menu
						ImGui::MenuItem(view->Name.c_str(), 0, &view->Visible);
				}

				// frame analysis control
				bool isFrameAnalyzed = ((PreviewUI*)Get(ViewID::Preview))->IsFrameAnalyzed();
				if (!isFrameAnalyzed) {
					ImGui::PushItemFlag(ImGuiItemFlags_Disabled, true);
					ImGui::PushStyleVar(ImGuiStyleVar_Alpha, ImGui::GetStyle().Alpha * 0.5f);
				}
				ImGui::MenuItem(m_frameAnalysis->Name.c_str(), 0, &m_frameAnalysis->Visible);
				if (!isFrameAnalyzed) {
					ImGui::PopStyleVar();
					ImGui::PopItemFlag();
				}

				// debug window controls
				if (ImGui::BeginMenu("Debug")) {
					if (!m_data->Debugger.IsDebugging()) {
						ImGui::PushItemFlag(ImGuiItemFlags_Disabled, true);
						ImGui::PushStyleVar(ImGuiStyleVar_Alpha, ImGui::GetStyle().Alpha * 0.5f);
					}

					for (auto& dview : m_debugViews)
						ImGui::MenuItem(dview->Name.c_str(), 0, &dview->Visible);

					if (!m_data->Debugger.IsDebugging()) {
						ImGui::PopStyleVar();
						ImGui::PopItemFlag();
					}

					ImGui::EndMenu();
				}

				m_data->Plugins.ShowMenuItems("window");

				ImGui::Separator();

				ImGui::MenuItem("Performance Mode", KeyboardShortcuts::Instance().GetString("Workspace.PerformanceMode").c_str(), &m_perfModeFake);
				ImGui::MenuItem("Focus mode", KeyboardShortcuts::Instance().GetString("Workspace.FocusMode").c_str(), &m_focusModeTemp);

				if (ImGui::MenuItem("Options", KeyboardShortcuts::Instance().GetString("Workspace.Options").c_str())) {
					m_optionsOpened = true;
					*m_settingsBkp = settings;
					m_shortcutsBkp = KeyboardShortcuts::Instance().GetMap();
				}

				ImGui::EndMenu();
			}
			if (ImGui::BeginMenu("Help")) {
				if (ImGui::BeginMenu("Support")) {
					if (ImGui::MenuItem("Patreon"))
						UIHelper::ShellOpen("https://www.patreon.com/dfranx");
					if (ImGui::MenuItem("PayPal"))
						UIHelper::ShellOpen("https://www.paypal.me/dfranx");
					ImGui::EndMenu();
				}
				ImGui::Separator();
				if (ImGui::MenuItem("Tutorial"))
					UIHelper::ShellOpen("http://docs.shadered.org");

				if (ImGui::MenuItem("Send feedback"))
					UIHelper::ShellOpen("https://www.github.com/dfranx/SHADERed/issues");

				if (ImGui::MenuItem("Information")) { m_isInfoOpened = true; }
				if (ImGui::MenuItem("About SHADERed")) { m_isAboutOpen = true; }

				ImGui::EndMenu();
			}
			if (ImGui::BeginMenu("Supporters")) {
				static const std::vector<std::pair<std::string, std::string>> slist = {
					std::make_pair("Hugo Locurcio", "https://hugo.pro"),
					std::make_pair("Vladimir Alyamkin", "https://alyamkin.com"),
					std::make_pair("Wogos Media", "http://theWogos.com"),
					std::make_pair("Snow Developments", "https://snow.llc"),
					std::make_pair("Adad Morales", "https://www.moralesfx.com/"),
					std::make_pair("Liam Don", "https://twitter.com/liamdon"),
					std::make_pair("Andrew Kerr", ""),
					std::make_pair("Chris Sprance", "https://csprance.com")
				};

				for (auto& sitem : slist)
					if (ImGui::MenuItem(sitem.first.c_str()) && !sitem.second.empty())
						UIHelper::ShellOpen(sitem.second);

				ImGui::EndMenu();
			}
			m_data->Plugins.ShowCustomMenu();
			ImGui::EndMainMenuBar();
		}

		if (m_performanceMode || m_minimalMode || m_focusMode) {
			static_cast<PreviewUI*>(Get(ViewID::Preview))->Update(delta);

			// focus mode
			if (m_focusMode)
				m_renderFocusMode();
		}

		ImGui::End();

		// DAP host mode
		if (m_minimalMode && m_data->DAP.IsStarted())
			m_renderDAPMode(delta);

		if (!m_performanceMode && !m_minimalMode && !m_focusMode) {
			m_data->Plugins.Update(delta);

			for (auto& view : m_views)
				if (view->Visible) {
					ImGui::SetNextWindowSizeConstraints(ImVec2(80, 80), ImVec2(m_width * 2, m_height * 2));
					if (ImGui::Begin(view->Name.c_str(), &view->Visible)) view->Update(delta);
					ImGui::End();
				}
			if (m_data->Debugger.IsDebugging()) {
				for (auto& dview : m_debugViews) {
					if (dview->Visible) {
						ImGui::SetNextWindowSizeConstraints(ImVec2(80, 80), ImVec2(m_width * 2, m_height * 2));
						if (ImGui::Begin(dview->Name.c_str(), &dview->Visible)) dview->Update(delta);
						ImGui::End();
					}
				}

				// geometry output window
				if (m_data->Debugger.GetStage() == ShaderStage::Geometry) {
					ImGui::SetNextWindowSizeConstraints(ImVec2(80, 80), ImVec2(m_width * 2, m_height * 2));
					if (ImGui::Begin(m_geometryOutput->Name.c_str())) m_geometryOutput->Update(delta);
					ImGui::End();
				}

				// tessellation control shader output window
				if (m_data->Debugger.GetStage() == ShaderStage::TessellationControl) {
					ImGui::SetNextWindowSizeConstraints(ImVec2(80, 80), ImVec2(m_width * 2, m_height * 2));
					if (ImGui::Begin(m_tessControlOutput->Name.c_str())) m_tessControlOutput->Update(delta);
					ImGui::End();
				}
			}

			// text editor windows
			Get(ViewID::Code)->Update(delta);

			// object preview
			if (((ed::ObjectPreviewUI*)m_objectPrev)->ShouldRun())
				m_objectPrev->Update(delta);

			// frame analysis window
			if (((PreviewUI*)Get(ViewID::Preview))->IsFrameAnalyzed() && m_frameAnalysis->Visible) {
				if (ImGui::Begin(m_frameAnalysis->Name.c_str(), &m_frameAnalysis->Visible))
					m_frameAnalysis->Update(delta);
				ImGui::End();
			}
		}

		// handle the "build occured" event
		if (settings.General.AutoOpenErrorWindow && m_data->Messages.BuildOccured) {
			size_t errors = m_data->Messages.GetErrorAndWarningMsgCount();
			if (errors > 0 && !Get(ViewID::Output)->Visible)
				Get(ViewID::Output)->Visible = true;
			m_data->Messages.BuildOccured = false;
		}

		// render options window
		if (m_optionsOpened) {
			ImGui::Begin("Options", &m_optionsOpened, ImGuiWindowFlags_NoDocking);
			m_renderOptions();
			ImGui::End();
		}

		// render all the popups
		m_renderPopups(delta);

		// notifications
		if (m_notifs.Has())
			m_notifs.Render();

		// render ImGUI
		ImGui::Render();
	}
}