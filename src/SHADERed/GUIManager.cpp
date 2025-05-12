#include <SDL2/SDL_messagebox.h>
#include <SHADERed/GUIManager.h>
#include <SHADERed/InterfaceManager.h>
#include <SHADERed/Objects/CameraSnapshots.h>
#include <SHADERed/Objects/Export/ExportCPP.h>
#include <SHADERed/Objects/FunctionVariableManager.h>
#include <SHADERed/Objects/KeyboardShortcuts.h>
#include <SHADERed/Objects/Logger.h>
#include <SHADERed/Objects/Names.h>
#include <SHADERed/Objects/SPIRVParser.h>
#include <SHADERed/Objects/Settings.h>
#include <SHADERed/Objects/ShaderCompiler.h>
#include <SHADERed/Objects/SystemVariableManager.h>
#include <SHADERed/Objects/ThemeContainer.h>
#include <SHADERed/UI/BrowseOnlineUI.h>
#include <SHADERed/UI/CodeEditorUI.h>
#include <SHADERed/UI/CreateItemUI.h>
#include <SHADERed/UI/Debug/AutoUI.h>
#include <SHADERed/UI/Debug/BreakpointListUI.h>
#include <SHADERed/UI/Debug/FunctionStackUI.h>
#include <SHADERed/UI/Debug/GeometryOutputUI.h>
#include <SHADERed/UI/Debug/ImmediateUI.h>
#include <SHADERed/UI/Debug/TessellationControlOutputUI.h>
#include <SHADERed/UI/Debug/ValuesUI.h>
#include <SHADERed/UI/Debug/VectorWatchUI.h>
#include <SHADERed/UI/Debug/WatchUI.h>
#include <SHADERed/UI/FrameAnalysisUI.h>
#include <SHADERed/UI/Icons.h>
#include <SHADERed/UI/MessageOutputUI.h>
#include <SHADERed/UI/ObjectListUI.h>
#include <SHADERed/UI/ObjectPreviewUI.h>
#include <SHADERed/UI/OptionsUI.h>
#include <SHADERed/UI/PinnedUI.h>
#include <SHADERed/UI/PipelineUI.h>
#include <SHADERed/UI/PixelInspectUI.h>
#include <SHADERed/UI/PreviewUI.h>
#include <SHADERed/UI/ProfilerUI.h>
#include <SHADERed/UI/PropertyUI.h>
#include <SHADERed/UI/UIHelper.h>
#include <imgui/examples/imgui_impl_opengl3.h>
#include <imgui/examples/imgui_impl_sdl.h>
#include <imgui/imgui.h>
#include <misc/ImFileDialog.h>

#include <filesystem>
#include <fstream>

#include <misc/stb_image.h>
#define STB_IMAGE_WRITE_IMPLEMENTATION
#include <misc/stb_image_write.h>

extern "C" {
#include <misc/dds.h>
}

#define STBIR_DEFAULT_FILTER_DOWNSAMPLE STBIR_FILTER_CATMULLROM
#define STB_IMAGE_RESIZE_IMPLEMENTATION
#include <misc/stb_image_resize.h>

#if defined(__APPLE__)
// no includes on mac os
#elif defined(__linux__) || defined(__unix__)
// no includes on linux
#elif defined(_WIN32)
#include <windows.h>
#endif
#include <queue>

namespace ed {
	GUIManager::GUIManager(ed::InterfaceManager* objs, SDL_Window* wnd, SDL_GLContext* gl)
	{
		m_data = objs;
		m_wnd = wnd;
		m_gl = gl;
		m_settingsBkp = new Settings();
		m_previewSaveSize = glm::ivec2(1920, 1080);
		m_savePreviewPopupOpened = false;
		m_optGroup = 0;
		m_optionsOpened = false;
		m_cachedFont = "null";
		m_cachedFontSize = 15;
		m_performanceMode = false;
		m_perfModeFake = false;
		m_fontNeedsUpdate = false;
		m_isCreateItemPopupOpened = false;
		m_isCreateCubemapOpened = false;
		m_isCreateRTOpened = false;
		m_isCreateKBTxtOpened = false;
		m_isCreateBufferOpened = false;
		m_isRecordCameraSnapshotOpened = false;
		m_exportAsCPPOpened = false;
		m_isCreateImgOpened = false;
		m_isAboutOpen = false;
		m_wasPausedPrior = true;
		m_savePreviewSeq = false;
		m_cacheProjectModified = false;
		m_isCreateImg3DOpened = false;
		m_isInfoOpened = false;
		m_isChangelogOpened = false;
		m_savePreviewSeqDuration = 5.5f;
		m_savePreviewSeqFPS = 30;
		m_savePreviewSupersample = 0;
		m_iconFontLarge = nullptr;
		m_expcppBackend = 0;
		m_expcppCmakeFiles = true;
		m_expcppCmakeModules = true;
		m_expcppImage = true;
		m_expcppMemoryShaders = true;
		m_expcppCopyImages = true;
		memset(&m_expcppProjectName[0], 0, 64 * sizeof(char));
		strcpy(m_expcppProjectName, "ShaderProject");
		m_expcppSavePath = "./export.cpp";
		m_expcppError = false;
		m_tipOpened = false;
		m_splashScreen = true;
		m_splashScreenLoaded = false;
		m_recompiledAll = false;
		m_isIncompatPluginsOpened = false;
		m_minimalMode = false;
		m_focusMode = false;
		m_focusModeTemp = false;
		m_cubemapPathPtr = nullptr;
		m_cmdArguments = nullptr;

		m_isBrowseOnlineOpened = false;

		m_uiIniFile = Settings::Instance().ConvertPath("data/workspace.dat");

		Settings::Instance().Load();
		m_loadTemplateList();

		SDL_GetWindowSize(m_wnd, &m_width, &m_height);

		// set vsync on startup
		SDL_GL_SetSwapInterval(Settings::Instance().General.VSync);

		// Initialize imgui
		Logger::Get().Log("Initializing Dear ImGUI");
		ImGui::CreateContext();

		ImGuiIO& io = ImGui::GetIO();
		io.Fonts->AddFontDefault();
		io.IniFilename = m_uiIniFile.c_str();
		io.ConfigFlags |= ImGuiConfigFlags_NavEnableKeyboard | ImGuiConfigFlags_DockingEnable /*| ImGuiConfigFlags_ViewportsEnable TODO: allow this on windows? test on linux?*/;
		io.ConfigDockingWithShift = false;

		if (!ed::Settings::Instance().LinuxHomeDirectory.empty()) {
			if (!std::filesystem::exists(m_uiIniFile) && std::filesystem::exists("data/workspace.dat"))
				ImGui::LoadIniSettingsFromDisk("data/workspace.dat");
		}

		ImGuiStyle& style = ImGui::GetStyle();
		style.WindowMenuButtonPosition = ImGuiDir_Right;

		ImGui_ImplOpenGL3_Init(SDL_GLSL_VERSION);
		ImGui_ImplSDL2_InitForOpenGL(m_wnd, *m_gl);

		ImGui::StyleColorsDark();

		Logger::Get().Log("Creating various UI view objects");

		m_views.push_back(new PreviewUI(this, objs, "Preview"));
		m_views.push_back(new PinnedUI(this, objs, "Pinned"));
		m_views.push_back(new CodeEditorUI(this, objs, "Code"));
		m_views.push_back(new MessageOutputUI(this, objs, "Output"));
		m_views.push_back(new ObjectListUI(this, objs, "Objects"));
		m_views.push_back(new PipelineUI(this, objs, "Pipeline"));
		m_views.push_back(new PropertyUI(this, objs, "Properties"));
		m_views.push_back(new PixelInspectUI(this, objs, "Pixel Inspect"));
		m_views.push_back(new ProfilerUI(this, objs, "Profiler"));

		m_debugViews.push_back(new DebugWatchUI(this, objs, "Watches"));
		m_debugViews.push_back(new DebugValuesUI(this, objs, "Variables"));
		m_debugViews.push_back(new DebugFunctionStackUI(this, objs, "Function stack"));
		m_debugViews.push_back(new DebugBreakpointListUI(this, objs, "Breakpoints"));
		m_debugViews.push_back(new DebugVectorWatchUI(this, objs, "Vector watch"));
		m_debugViews.push_back(new DebugAutoUI(this, objs, "Auto", false));
		m_debugViews.push_back(new DebugImmediateUI(this, objs, "Immediate"));

		KeyboardShortcuts::Instance().Load();
		m_setupShortcuts();

		m_browseOnline = new BrowseOnlineUI(this, objs, "Browse online");
		m_options = new OptionsUI(this, objs, "Options");
		m_createUI = new CreateItemUI(this, objs);
		m_objectPrev = new ObjectPreviewUI(this, objs, "Object Preview");
		m_geometryOutput = new DebugGeometryOutputUI(this, objs, "Geometry Shader Output");
		m_tessControlOutput = new DebugTessControlOutputUI(this, objs, "Tessellation control shader output");
		m_frameAnalysis = new FrameAnalysisUI(this, objs, "Frame Analysis");

		// turn on the tracker on startup
		dynamic_cast<CodeEditorUI*>(Get(ViewID::Code))->SetTrackFileChanges(Settings::Instance().General.RecompileOnFileChange);

		dynamic_cast<OptionsUI*>(m_options)->SetGroup(OptionsUI::Page::General);

		// enable dpi awareness
		if (Settings::Instance().General.AutoScale) {
			float dpi = 0.0f;
			int wndDisplayIndex = SDL_GetWindowDisplayIndex(wnd);
			SDL_GetDisplayDPI(wndDisplayIndex, &dpi, nullptr, nullptr);
			dpi /= 96.0f;

			if (dpi <= 0.0f) dpi = 1.0f;

			Settings::Instance().DPIScale = dpi;
			Logger::Get().Log("Setting DPI to " + std::to_string(dpi));
		}
		Settings::Instance().TempScale = Settings::Instance().DPIScale;

		ImGui::GetStyle().ScaleAllSizes(Settings::Instance().DPIScale);

		dynamic_cast<OptionsUI*>(m_options)->ApplyTheme();

		FunctionVariableManager::Instance().Initialize(&objs->Pipeline, &objs->Debugger, &objs->Renderer);
		m_data->Renderer.Pause(Settings::Instance().Preview.PausedOnStartup);

		m_kbInfo.SetText(std::string(KEYBOARD_KEYCODES_TEXT));
		m_kbInfo.SetPalette(ThemeContainer::Instance().GetTextEditorStyle(Settings::Instance().Theme));
		m_kbInfo.SetHighlightLine(true);
		m_kbInfo.SetShowLineNumbers(true);
		m_kbInfo.SetLanguageDefinition(TextEditor::LanguageDefinition::GLSL());
		m_kbInfo.SetReadOnly(true);

		// load snippets
		dynamic_cast<CodeEditorUI*>(Get(ViewID::Code))->LoadSnippets();

		// setup file dialog
		ifd::FileDialog::Instance().CreateTexture = [](uint8_t* data, int w, int h, char fmt) -> void* {
			GLuint tex;

			glGenTextures(1, &tex);
			glBindTexture(GL_TEXTURE_2D, tex);
			glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
			glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
			glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
			glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
			glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, w, h, 0, (fmt == 0) ? GL_BGRA : GL_RGBA, GL_UNSIGNED_BYTE, data);
			glGenerateMipmap(GL_TEXTURE_2D);
			glBindTexture(GL_TEXTURE_2D, 0);

			// return (void*)tex;
			return reinterpret_cast<void*>(static_cast<uintptr_t>(tex));
		};
		ifd::FileDialog::Instance().DeleteTexture = [](void* tex) {
			const auto texID = static_cast<GLuint>(reinterpret_cast<uintptr_t>(tex));
			glDeleteTextures(1, &texID);
		};

		// load file dialog bookmarks
		std::string bookmarksFileLoc = Settings::Instance().ConvertPath("data/filedialog.dat");
		std::ifstream bookmarksFile(bookmarksFileLoc);
		int fdlgFileVersion = 0;
		float fdlgZoom = 1.0f;
		std::string bookmark;
		bookmarksFile >> fdlgFileVersion;
		bookmarksFile >> fdlgZoom;
		while (std::getline(bookmarksFile, bookmark))
			ifd::FileDialog::Instance().AddFavorite(bookmark);
		ifd::FileDialog::Instance().SetZoom(fdlgZoom);

		// setup splash screen
		m_splashScreenLoad();

		// load recents
		std::string currentInfoPath = Settings::Instance().ConvertPath("info.dat");
		int recentsSize = 0;
		std::ifstream infoReader(currentInfoPath);
		infoReader.ignore(128, '\n');
		infoReader >> recentsSize;
		infoReader.ignore(128, '\n');
		m_recentProjects = std::vector<std::string>(recentsSize);
		for (int i = 0; i < recentsSize; i++) {
			std::getline(infoReader, m_recentProjects[i]);
			if (infoReader.eof())
				break;
		}
		infoReader.close();
	}
	GUIManager::~GUIManager()
	{
		glDeleteShader(Magnifier::Shader);

		std::string currentInfoPath = Settings::Instance().ConvertPath("info.dat");

		std::ofstream verWriter(currentInfoPath);
		verWriter << WebAPI::InternalVersion << std::endl;
		verWriter << m_recentProjects.size() << std::endl;
		for (const auto& m_recentProject : m_recentProjects)
			verWriter << m_recentProject << std::endl;
		verWriter.close();

		Logger::Get().Log("Shutting down UI");

		for (auto& view : m_views)
			delete view;
		for (auto& dview : m_debugViews)
			delete dview;
		delete m_geometryOutput;
		delete m_tessControlOutput;

		static_cast<BrowseOnlineUI*>(m_browseOnline)->FreeMemory();
		delete m_browseOnline;

		delete m_options;
		delete m_objectPrev;
		delete m_createUI;
		delete m_settingsBkp;

		ImGui_ImplSDL2_Shutdown();
		ImGui_ImplOpenGL3_Shutdown();
		ImGui::DestroyContext();
	}

	void GUIManager::OnEvent(const SDL_Event& e)
	{
		ImGui_ImplSDL2_ProcessEvent(&e);

		if (m_splashScreen) {

			return;
		}

		// check for shortcut presses
		if (e.type == SDL_KEYDOWN) {
			if (!(m_optionsOpened && static_cast<OptionsUI*>(m_options)->IsListening())) {
				const bool codeHasFocus = static_cast<CodeEditorUI*>(Get(ViewID::Code))->HasFocus();

				if (!(ImGui::GetIO().WantTextInput && !codeHasFocus)) {
					KeyboardShortcuts::Instance().Check(e, codeHasFocus);
					dynamic_cast<CodeEditorUI*>(Get(ViewID::Code))->RequestedProjectSave = false;
				}
			}
		} else if (e.type == SDL_MOUSEMOTION)
			m_perfModeClock.Restart();
		else if (e.type == SDL_DROPFILE) {

			char* droppedFile = e.drop.file;

			std::string file = m_data->Parser.GetRelativePath(droppedFile);
			size_t dotPos = file.find_last_of('.');

			if (!file.empty() && dotPos != std::string::npos) {
				std::string ext = file.substr(dotPos + 1);

				const std::vector<std::string> imgExt = { "png", "jpeg", "jpg", "bmp", "gif", "psd", "pic", "pnm", "hdr", "tga" };
				const std::vector<std::string> sndExt = { "ogg", "wav", "flac", "aiff", "raw" }; // TODO: more file ext
				const std::vector<std::string> projExt = { "sprj" };
				const std::vector<std::string> shaderExt = { "hlsl", "glsl", "vert", "frag", "geom", "tess", "shader" };

				if (std::count(projExt.begin(), projExt.end(), ext) > 0) {
					bool cont = true;
					if (m_data->Parser.IsProjectModified()) {
						const int btnID = this->AreYouSure();
						if (btnID == 2)
							cont = false;
					}

					if (cont)
						Open(m_data->Parser.GetProjectPath(file));
				} else if (std::count(imgExt.begin(), imgExt.end(), ext) > 0)
					m_data->Objects.CreateTexture(file);
				else if (std::count(sndExt.begin(), sndExt.end(), ext) > 0)
					m_data->Objects.CreateAudio(file);
				else if (std::count(shaderExt.begin(), shaderExt.end(), ext) > 0)
					((CodeEditorUI*)Get(ed::ViewID::Code))->OpenFile(m_data->Parser.GetProjectPath(file));
				else if (ext == "dds") {
					const auto actualFileLoc = m_data->Parser.GetProjectPath(file);

					// this makes the load time 2x slower, but it's not like everyones gonna be dropping dds files non stop
					dds_image_t ddsImage = dds_load_from_file(actualFileLoc.c_str());
					bool is3D = ddsImage->header.caps2 & DDSCAPS2_VOLUME;
					dds_image_free(ddsImage);

					if (is3D)
						m_data->Objects.CreateTexture3D(actualFileLoc);
					else
						m_data->Objects.CreateTexture(actualFileLoc);
				} else
					m_data->Plugins.HandleDropFile(file.c_str());
			}

			SDL_free(droppedFile);
		} else if (e.type == SDL_WINDOWEVENT) {
			if (e.window.event == SDL_WINDOWEVENT_MOVED || e.window.event == SDL_WINDOWEVENT_MAXIMIZED || e.window.event == SDL_WINDOWEVENT_RESIZED) {
				SDL_GetWindowSize(m_wnd, &m_width, &m_height);
			}
		}

		if (m_optionsOpened)
			m_options->OnEvent(e);

		if (dynamic_cast<ed::ObjectPreviewUI*>(m_objectPrev)->ShouldRun())
			m_objectPrev->OnEvent(e);

		for (auto& view : m_views)
			view->OnEvent(e);

		if (m_data->Debugger.IsDebugging()) {
			for (auto& dview : m_debugViews)
				dview->OnEvent(e);
			if (m_data->Debugger.GetStage() == ShaderStage::Geometry)
				m_geometryOutput->OnEvent(e);
			if (m_data->Debugger.GetStage() == ShaderStage::TessellationControl)
				m_tessControlOutput->OnEvent(e);
		}

		m_data->Plugins.OnEvent(e);
	}

	void GUIManager::AddNotification(int id, const char* text, const char* btnText, std::function<void(int, IPlugin1*)> fn, IPlugin1* plugin)
	{
		m_notifs.Add(id, text, btnText, fn, plugin);
	}

	void GUIManager::StopDebugging()
	{
		auto* codeUI = dynamic_cast<CodeEditorUI*>(Get(ViewID::Code));
		codeUI->StopDebugging();
		m_data->Debugger.SetDebugging(false);
		m_data->DAP.StopDebugging();
	}
	void GUIManager::Destroy()
	{
		std::string bookmarksFileLoc = Settings::Instance().ConvertPath("data/filedialog.dat");
		std::ofstream bookmarksFile(bookmarksFileLoc);
		int fdlgFileVersion = 0;
		float fdlgZoom = ifd::FileDialog::Instance().GetZoom();
		std::string bookmark;
		bookmarksFile << fdlgFileVersion << std::endl;
		bookmarksFile << fdlgZoom << std::endl;
		for (const auto& fav : ifd::FileDialog::Instance().GetFavorites())
			bookmarksFile << fav << std::endl;
		bookmarksFile.close();

		auto* codeUI = dynamic_cast<CodeEditorUI*>(Get(ViewID::Code));
		codeUI->SaveSnippets();
		codeUI->SetTrackFileChanges(false);
		codeUI->StopThreads();
	}

	int GUIManager::AreYouSure()
	{
		int buttonid = UIHelper::MessageBox_YesNoCancel(m_wnd, "Save changes to the project before quitting?");
		if (buttonid == 0) // save
			Save();
		return buttonid;
	}
	void GUIManager::m_renderOptions()
	{
		auto* options = dynamic_cast<OptionsUI*>(m_options);
		static const char* optGroups[8] = {
			"General",
			"Editor",
			"Code snippets",
			"Debugger",
			"Shortcuts",
			"Preview",
			"Plugins",
			"Project"
		};

		float height = abs(ImGui::GetWindowContentRegionMax().y - ImGui::GetWindowContentRegionMin().y - ImGui::GetStyle().WindowPadding.y * 2) / ImGui::GetTextLineHeightWithSpacing() - 1;

		ImGui::Columns(2);

		// TODO: this is only a temporary fix for non-resizable columns
		static bool isColumnWidthSet = false;
		if (!isColumnWidthSet) {
			ImGui::SetColumnWidth(0, Settings::Instance().CalculateSize(100) + ImGui::GetStyle().WindowPadding.x * 2);
			isColumnWidthSet = true;
		}

		ImGui::PushStyleColor(ImGuiCol_FrameBg, ImVec4(0, 0, 0, 0));
		ImGui::PushItemWidth(-1);
		if (ImGui::ListBox("##optiongroups", &m_optGroup, optGroups, std::size(optGroups), (int)height))
			options->SetGroup(static_cast<OptionsUI::Page>(m_optGroup));
		ImGui::PopStyleColor();

		ImGui::NextColumn();

		options->Update(0.0f);

		ImGui::Columns();

		ImGui::SetCursorPosX(ImGui::GetWindowWidth() - Settings::Instance().CalculateSize(160));
		if (ImGui::Button("OK", ImVec2(Settings::Instance().CalculateSize(70), 0))) {
			Settings::Instance().Save();
			KeyboardShortcuts::Instance().Save();

			auto* code = dynamic_cast<CodeEditorUI*>(Get(ViewID::Code));

			code->ApplySettings();

			if (Settings::Instance().TempScale != Settings::Instance().DPIScale) {
				dynamic_cast<ed::OptionsUI*>(m_options)->ApplyTheme();
				Settings::Instance().DPIScale = Settings::Instance().TempScale;
				ImGui::GetStyle().ScaleAllSizes(Settings::Instance().DPIScale);
				m_fontNeedsUpdate = true;
			}

			m_optionsOpened = false;
		}

		ImGui::SameLine();

		ImGui::SetCursorPosX(ImGui::GetWindowWidth() - Settings::Instance().CalculateSize(80));
		if (ImGui::Button("Cancel", ImVec2(-1, 0))) {
			Settings::Instance() = *m_settingsBkp;
			KeyboardShortcuts::Instance().SetMap(m_shortcutsBkp);
			dynamic_cast<OptionsUI*>(m_options)->ApplyTheme();
			m_optionsOpened = false;
		}
	}

	UIView* GUIManager::Get(ViewID view)
	{
		if (view == ViewID::Options)
			return m_options;
		if (view == ViewID::ObjectPreview)
			return m_objectPrev;
		if (view >= ViewID::DebugWatch && view <= ViewID::DebugImmediate)
			return m_debugViews[static_cast<int>(view) - static_cast<int>(ViewID::DebugWatch)];
		if (view == ViewID::DebugGeometryOutput)
			return m_geometryOutput;
		if (view == ViewID::DebugTessControlOutput)
			return m_tessControlOutput;
		if (view == ViewID::FrameAnalysis)
			return m_frameAnalysis;

		return m_views[(int)view];
	}
	void GUIManager::ResetWorkspace()
	{
		m_data->Renderer.FlushCache();
		dynamic_cast<CodeEditorUI*>(Get(ViewID::Code))->CloseAll();
		dynamic_cast<PinnedUI*>(Get(ViewID::Pinned))->CloseAll();
		dynamic_cast<PreviewUI*>(Get(ViewID::Preview))->Reset();
		dynamic_cast<PropertyUI*>(Get(ViewID::Properties))->Close();
		dynamic_cast<PipelineUI*>(Get(ViewID::Pipeline))->Reset();
		dynamic_cast<ObjectPreviewUI*>(Get(ViewID::ObjectPreview))->CloseAll();
		CameraSnapshots::Clear();
	}

	ShaderVariable::ValueType getTypeFromSPV(SPIRVParser::ValueType valType)
	{
		switch (valType) {
		case SPIRVParser::ValueType::Bool:
			return ShaderVariable::ValueType::Boolean1;
		case SPIRVParser::ValueType::Int:
			return ShaderVariable::ValueType::Integer1;
		case SPIRVParser::ValueType::Float:
			return ShaderVariable::ValueType::Float1;
		default:
			return ShaderVariable::ValueType::Count;
		}
		return ShaderVariable::ValueType::Count;
	}
	ShaderVariable::ValueType formVectorType(ShaderVariable::ValueType valType, int compCount)
	{
		if (valType == ShaderVariable::ValueType::Boolean1) {
			if (compCount == 2) return ShaderVariable::ValueType::Boolean2;
			if (compCount == 3) return ShaderVariable::ValueType::Boolean3;
			if (compCount == 4) return ShaderVariable::ValueType::Boolean4;
		} else if (valType == ShaderVariable::ValueType::Integer1) {
			if (compCount == 2) return ShaderVariable::ValueType::Integer2;
			if (compCount == 3) return ShaderVariable::ValueType::Integer3;
			if (compCount == 4) return ShaderVariable::ValueType::Integer4;
		} else if (valType == ShaderVariable::ValueType::Float1) {
			if (compCount == 2) return ShaderVariable::ValueType::Float2;
			if (compCount == 3) return ShaderVariable::ValueType::Float3;
			if (compCount == 4) return ShaderVariable::ValueType::Float4;
		}

		return ShaderVariable::ValueType::Count;
	}
	ShaderVariable::ValueType formMatrixType(ShaderVariable::ValueType valType, int compCount)
	{
		if (compCount == 2) return ShaderVariable::ValueType::Float2x2;
		if (compCount == 3) return ShaderVariable::ValueType::Float3x3;
		if (compCount == 4) return ShaderVariable::ValueType::Float4x4;

		return ShaderVariable::ValueType::Count;
	}
	void GUIManager::m_autoUniforms(ShaderVariableContainer& varManager, SPIRVParser& spv, std::vector<std::string>& uniformList)
	{
		auto* pinUI = dynamic_cast<PinnedUI*>(Get(ViewID::Pinned));
		std::vector<ShaderVariable*> vars = varManager.GetVariables();

		// add variables
		for (const auto& unif : spv.Uniforms) {
			bool exists = false;
			for (ShaderVariable* var : vars)
				if (strcmp(var->Name, unif.Name.c_str()) == 0) {
					exists = true;
					break;
				}

			uniformList.push_back(unif.Name);

			// add it
			if (!exists) {
				// type
				ShaderVariable::ValueType valType = getTypeFromSPV(unif.Type);
				if (valType == ShaderVariable::ValueType::Count) {
					if (unif.Type == SPIRVParser::ValueType::Vector)
						valType = formVectorType(getTypeFromSPV(unif.BaseType), unif.TypeComponentCount);
					else if (unif.Type == SPIRVParser::ValueType::Matrix)
						valType = formMatrixType(getTypeFromSPV(unif.BaseType), unif.TypeComponentCount);
				}

				if (valType == ShaderVariable::ValueType::Count) {
					std::queue<std::string> curName;
					std::queue<SPIRVParser::Variable> curType;

					curType.push(unif);
					curName.push(unif.Name);

					while (!curType.empty()) {
						SPIRVParser::Variable type = curType.front();
						std::string name = curName.front();

						curType.pop();
						curName.pop();

						if (type.Type != SPIRVParser::ValueType::Struct) {
							for (ShaderVariable* var : vars)
								if (strcmp(var->Name, name.c_str()) == 0) {
									exists = true;
									break;
								}

							uniformList.push_back(name);

							if (!exists) {
								// add variable
								valType = getTypeFromSPV(type.Type);
								if (valType == ShaderVariable::ValueType::Count) {
									if (type.Type == SPIRVParser::ValueType::Vector)
										valType = formVectorType(getTypeFromSPV(type.BaseType), type.TypeComponentCount);
									else if (type.Type == SPIRVParser::ValueType::Matrix)
										valType = formMatrixType(getTypeFromSPV(type.BaseType), type.TypeComponentCount);
								}

								if (valType != ShaderVariable::ValueType::Count) {
									ShaderVariable newVariable = ShaderVariable(valType, name.c_str(), SystemShaderVariable::None);
									ShaderVariable* ptr = varManager.AddCopy(newVariable);
									if (Settings::Instance().General.AutoUniformsPin)
										pinUI->Add(ptr);
								}
							}
						} else {
							// branch
							if (spv.UserTypes.count(type.TypeName) > 0) {
								const std::vector<SPIRVParser::Variable>& mems = spv.UserTypes[type.TypeName];
								for (const auto & mem : mems) {
									std::string memName = std::string(name.c_str()) + "." + mem.Name; // hack for \0
									curType.push(mem);
									curName.push(memName);
								}
							}
						}
					}
				} else {
					// usage
					SystemShaderVariable usage = SystemShaderVariable::None;
					if (Settings::Instance().General.AutoUniformsFunction)
						usage = SystemVariableManager::GetTypeFromName(unif.Name);

					// add and pin
					if (valType != ShaderVariable::ValueType::Count) {
						ShaderVariable newVariable = ShaderVariable(valType, unif.Name.c_str(), usage);
						ShaderVariable* ptr = varManager.AddCopy(newVariable);
						if (Settings::Instance().General.AutoUniformsPin && usage == SystemShaderVariable::None)
							pinUI->Add(ptr);
					}
				}
			}
		}
	}
	void GUIManager::m_deleteUnusedUniforms(ShaderVariableContainer& varManager, const std::vector<std::string>& spv)
	{
		auto* pinUI = dynamic_cast<PinnedUI*>(Get(ViewID::Pinned));
		std::vector<ShaderVariable*> vars = varManager.GetVariables();

		for (ShaderVariable* var : vars) {
			bool exists = false;
			for (const auto& unif : spv)
				if (strcmp(var->Name, unif.c_str()) == 0) {
					exists = true;
					break;
				}

			if (!exists) {
				pinUI->Remove(var->Name);
				varManager.Remove(var->Name);
			}
		}
	}

	void GUIManager::CreateNewShaderPass()
	{
		m_createUI->SetType(PipelineItem::ItemType::ShaderPass);
		m_isCreateItemPopupOpened = true;
	}
	void GUIManager::CreateNewComputePass()
	{
		m_createUI->SetType(PipelineItem::ItemType::ComputePass);
		m_isCreateItemPopupOpened = true;
	}
	void GUIManager::CreateNewAudioPass()
	{
		m_createUI->SetType(PipelineItem::ItemType::AudioPass);
		m_isCreateItemPopupOpened = true;
	}
	void GUIManager::CreateNewTexture()
	{
		ifd::FileDialog::Instance().Open("CreateTextureDlg", "Select texture(s)", "Image file (*.png;*.jpg;*.jpeg;*.bmp;*.tga;*.dds){.png,.jpg,.jpeg,.bmp,.tga,.dds},.*", true);
	}
	void GUIManager::CreateNewTexture3D()
	{
		ifd::FileDialog::Instance().Open("CreateTexture3DDlg", "Select texture(s)", "DDS file (*.dds){.dds},.*", true);
	}
	void GUIManager::CreateNewAudio()
	{
		ifd::FileDialog::Instance().Open("CreateAudioDlg", "Select audio file", "Audio file (*.wav;*.flac;*.ogg;*.midi){.wav,.flac,.ogg,.midi},.*");
	}

}