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
			SDL_GetDisplayDPI(wndDisplayIndex, &dpi, NULL, NULL);
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

		CodeEditorUI* codeUI = ((CodeEditorUI*)Get(ViewID::Code));
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
	void GUIManager::m_tooltip(const std::string& text)
	{
		if (ImGui::IsItemHovered()) {
			ImGui::PopFont(); // TODO: remove this if its being used in non-toolbar places

			ImGui::BeginTooltip();
			ImGui::PushTextWrapPos(ImGui::GetFontSize() * 35.0f);
			ImGui::TextUnformatted(text.c_str());
			ImGui::PopTextWrapPos();
			ImGui::EndTooltip();

			ImGui::PushFont(m_iconFontLarge);
		}
	}
	void GUIManager::m_renderToolbar()
	{
		ImGuiWindowFlags window_flags = ImGuiWindowFlags_NoBringToFrontOnFocus | ImGuiWindowFlags_NoNavFocus | ImGuiWindowFlags_NoDocking | ImGuiWindowFlags_NoTitleBar | ImGuiWindowFlags_NoCollapse | ImGuiWindowFlags_NoResize | ImGuiWindowFlags_NoMove;
		ImGuiViewport* viewport = ImGui::GetMainViewport();
		ImGui::SetNextWindowPos(ImVec2(viewport->Pos.x, viewport->Pos.y + ImGui::GetFrameHeight()));
		ImGui::SetNextWindowSize(ImVec2(viewport->Size.x, Settings::Instance().CalculateSize(TOOLBAR_HEIGHT)));
		ImGui::SetNextWindowViewport(viewport->ID);
		ImGui::PushStyleVar(ImGuiStyleVar_WindowRounding, 0.0f);
		ImGui::PushStyleVar(ImGuiStyleVar_WindowBorderSize, 1.0f);
		ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(0.0f, 0.0f));
		ImGui::Begin("##toolbar", 0, window_flags);
		ImGui::PopStyleVar(3);

		float bHeight = TOOLBAR_HEIGHT / 2 + ImGui::GetStyle().FramePadding.y * 2;

		ImGui::SetCursorPosY(ImGui::GetWindowHeight() / 2 - Settings::Instance().CalculateSize(bHeight) / 2);
		ImGui::Indent(Settings::Instance().CalculateSize(15));

		/*
			project (open, open directory, save, empty, new shader) ||
			objects (new texture, new cubemap, new audio, new render texture) ||
			preview (screenshot, pause, zoom in, zoom out, maximize) ||
			random (settings)
		*/

		ImGui::PushFont(m_iconFontLarge);

		// TODO: maybe pack all these into functions such as m_open, m_createEmpty, etc... so that there are no code
		// repetitions
		ImGui::PushStyleColor(ImGuiCol_Button, ImVec4(0, 0, 0, 0));
		if (ImGui::Button(UI_ICON_DOCUMENT_FOLDER)) { // OPEN PROJECT
			bool cont = true;
			if (m_data->Parser.IsProjectModified()) {
				int btnID = this->AreYouSure();
				if (btnID == 2)
					cont = false;
			}

			if (cont)
				ifd::FileDialog::Instance().Open("OpenProjectDlg", "Open SHADERed project", "SHADERed project (*.sprj){.sprj},.*");
		}
		m_tooltip("Open a project");
		ImGui::SameLine();
		if (ImGui::Button(UI_ICON_SAVE)) // SAVE
			Save();
		m_tooltip("Save project");
		ImGui::SameLine();
		if (ImGui::Button(UI_ICON_FILE_FILE)) { // EMPTY PROJECT
			m_selectedTemplate = "?empty";
			m_createNewProject();
		}
		m_tooltip("New empty project");
		ImGui::SameLine();
		if (ImGui::Button(UI_ICON_FOLDER_OPEN)) { // OPEN DIRECTORY
			std::string prpath = m_data->Parser.GetProjectPath("");
#if defined(__APPLE__)
			system(("open " + prpath).c_str()); // [MACOS]
#elif defined(__linux__) || defined(__unix__)
			system(("xdg-open " + prpath).c_str());
#elif defined(_WIN32)
			ShellExecuteA(NULL, "open", prpath.c_str(), NULL, NULL, SW_SHOWNORMAL);
#endif
		}
		m_tooltip("Open project directory");

		ImGui::SameLine();
		ImGui::SeparatorEx(ImGuiSeparatorFlags_Vertical);
		ImGui::SameLine(0.0f, 32.0f);

		if (ImGui::Button(UI_ICON_PIXELS)) this->CreateNewShaderPass();
		m_tooltip("New shader pass");
		ImGui::SameLine();
		if (ImGui::Button(UI_ICON_FX)) this->CreateNewComputePass();
		m_tooltip("New compute pass");
		ImGui::SameLine();
		if (ImGui::Button(UI_ICON_FILE_IMAGE)) this->CreateNewTexture();
		m_tooltip("New texture");
		ImGui::SameLine();
		if (ImGui::Button(UI_ICON_IMAGE)) this->CreateNewImage();
		m_tooltip("New empty image");
		ImGui::SameLine();
		if (ImGui::Button(UI_ICON_FILE_WAVE)) this->CreateNewAudio();
		m_tooltip("New audio");
		ImGui::SameLine();
		if (ImGui::Button(UI_ICON_TRANSPARENT)) this->CreateNewRenderTexture();
		m_tooltip("New render texture");
		ImGui::SameLine();
		if (ImGui::Button(UI_ICON_CUBE)) this->CreateNewCubemap();
		m_tooltip("New cubemap");
		ImGui::SameLine();
		if (ImGui::Button(UI_ICON_FILE_TEXT)) this->CreateNewBuffer();
		m_tooltip("New buffer");

		ImGui::SameLine();
		ImGui::SeparatorEx(ImGuiSeparatorFlags_Vertical);
		ImGui::SameLine(0.0f, 32.0f);

		if (ImGui::Button(UI_ICON_REFRESH)) { // REBUILD PROJECT
			dynamic_cast<CodeEditorUI*>(Get(ViewID::Code))->SaveAll();
			std::vector<PipelineItem*> passes = m_data->Pipeline.GetList();
			for (PipelineItem*& pass : passes)
				m_data->Renderer.Recompile(pass->Name);
		}
		m_tooltip("Rebuild");
		ImGui::SameLine();
		if (ImGui::Button(UI_ICON_CAMERA)) m_savePreviewPopupOpened = true; // TAKE A SCREENSHOT
		m_tooltip("Render");
		ImGui::SameLine();
		if (ImGui::Button(m_data->Renderer.IsPaused() ? UI_ICON_PLAY : UI_ICON_PAUSE))
			m_data->Renderer.Pause(!m_data->Renderer.IsPaused());
		m_tooltip(m_data->Renderer.IsPaused() ? "Play preview" : "Pause preview");
		ImGui::SameLine();
		if (ImGui::Button(UI_ICON_MAXIMIZE)) m_perfModeFake = !m_perfModeFake;
		m_tooltip("Performance mode");

		ImGui::SameLine();
		ImGui::SeparatorEx(ImGuiSeparatorFlags_Vertical);
		ImGui::SameLine(0.0f, 32.0f);

		if (ImGui::Button(UI_ICON_GEAR)) { // SETTINGS BUTTON
			m_optionsOpened = true;
			*m_settingsBkp = Settings::Instance();
			m_shortcutsBkp = KeyboardShortcuts::Instance().GetMap();
		}
		m_tooltip("Settings");

		ImGui::PopStyleColor();
		ImGui::PopFont();

		ImGui::End();
	}
	void GUIManager::m_renderOptions()
	{
		OptionsUI* options = dynamic_cast<OptionsUI*>(m_options);
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

			CodeEditorUI* code = dynamic_cast<CodeEditorUI*>(Get(ViewID::Code));

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

	void GUIManager::m_renderFocusMode()
	{
		ImGui::SetCursorPos(ImVec2(10, Settings::Instance().CalculateSize(35.0f)));
		ImGui::PushStyleColor(ImGuiCol_ChildBg, 0xB5000000 | (ImGui::GetColorU32(ImGuiCol_WindowBg) & 0x00FFFFFF));
		ImGui::BeginChild("##focus_container", ImVec2((float)m_width / 2 - 50, -Settings::Instance().CalculateSize(10) - (float)Settings::Instance().Preview.StatusBar * 35.0f), false);

		if (ImGui::BeginTabBar("PassTabs")) {
			auto& passes = m_data->Pipeline.GetList();
			for (auto& pass : passes) {
				if (pass->Type == PipelineItem::ItemType::ShaderPass) {
					if (ImGui::BeginTabItem(pass->Name)) {
						pipe::ShaderPass* passData = static_cast<pipe::ShaderPass*>(pass->Data);

						// shader tabs for this pass
						if (ImGui::BeginTabBar("ShaderTabs")) {
							CodeEditorUI* codeEditor = dynamic_cast<CodeEditorUI*>(Get(ViewID::Code));

							// vertex shader
							if (ImGui::BeginTabItem("Vertex shader")) {
								m_renderTextEditorFocusMode("Vertex shader editor", pass, ShaderStage::Vertex);
								ImGui::EndTabItem();
							}

							// geometry shader if used
							if (passData->GSUsed) {
								// geometry shader
								if (ImGui::BeginTabItem("Geometry shader")) {
									m_renderTextEditorFocusMode("Geometry shader editor", pass, ShaderStage::Geometry);
									ImGui::EndTabItem();
								}
							}

							// tessellation shaders
							if (passData->TSUsed) {
								// tc shader
								if (ImGui::BeginTabItem("Tessellation control shader")) {
									m_renderTextEditorFocusMode("Tessellation control shader editor", pass, ShaderStage::TessellationControl);
									ImGui::EndTabItem();
								}

								// te shader
								if (ImGui::BeginTabItem("Tessellation evaluation shader")) {
									m_renderTextEditorFocusMode("Tessellation evaluation shader editor", pass, ShaderStage::TessellationEvaluation);
									ImGui::EndTabItem();
								}
							}

							// pixel shader
							if (ImGui::BeginTabItem("Pixel shader")) {
								m_renderTextEditorFocusMode("Pixel shader editor", pass, ShaderStage::Pixel);
								ImGui::EndTabItem();
							}

							ImGui::EndTabBar();
						}

						ImGui::EndTabItem();
					}
				} else if (pass->Type == PipelineItem::ItemType::ComputePass) {
					if (ImGui::BeginTabItem(pass->Name)) {
						pipe::ComputePass* passData = static_cast<pipe::ComputePass*>(pass->Data);

						// shader tabs for this pass
						if (ImGui::BeginTabBar("ShaderTabs")) {
							CodeEditorUI* codeEditor = static_cast<CodeEditorUI*>(Get(ViewID::Code));

							// vertex shader
							if (ImGui::BeginTabItem("Compute shader")) {
								m_renderTextEditorFocusMode("Compute shader editor", pass, ShaderStage::Compute);
								ImGui::EndTabItem();
							}

							ImGui::EndTabBar();
						}

						ImGui::EndTabItem();
					}
				}
			}
			ImGui::EndTabBar();
		}

		ImGui::EndChild();
		ImGui::PopStyleColor();
	}
	void GUIManager::m_renderTextEditorFocusMode(const std::string& name, void* item, ShaderStage stage)
	{
		CodeEditorUI* codeEditor = dynamic_cast<CodeEditorUI*>(Get(ViewID::Code));

		TextEditor* editor = codeEditor->Get(static_cast<PipelineItem*>(item), stage);
		if (editor == nullptr) {
			codeEditor->Open(static_cast<PipelineItem*>(item), stage);
			editor = codeEditor->Get(static_cast<PipelineItem*>(item), stage);
		}

		codeEditor->DrawTextEditor(name, editor);
	}

	void GUIManager::m_renderDAPMode(float delta)
	{
		// PIXEL INSPECT //
		ImGui::SetNextWindowPos(ImVec2((float)m_width - 310, (float)m_height - 420));
		ImGui::SetNextWindowSize(ImVec2(280, 390), ImGuiCond_Always);
		ImGui::PushStyleColor(ImGuiCol_WindowBg, 0xB5000000 | (ImGui::GetColorU32(ImGuiCol_WindowBg) & 0x00FFFFFF));
		ImGui::PushStyleColor(ImGuiCol_ChildBg, 0x55000000 | (ImGui::GetColorU32(ImGuiCol_WindowBg) & 0x00FFFFFF));
		ImGui::Begin("##dap_wnd_pixelinsp", 0, ImGuiWindowFlags_NoTitleBar | ImGuiWindowFlags_NoMove | ImGuiWindowFlags_NoResize);

		Get(ViewID::PixelInspect)->Update(delta); // is delta necessary for the PixelInspect window?

		ImGui::End();
		ImGui::PopStyleColor(2);

		if (m_data->Debugger.IsDebugging()) {
			// GS OUTPUT WINDOW //
			if (m_data->Debugger.GetStage() == ShaderStage::Geometry) {
				float gsWidth = ((float)m_width / (float)m_height) * 390.0f;

				ImGui::SetNextWindowPos(ImVec2((float)m_width - 310 - 30 - gsWidth, (float)m_height - 420));
				ImGui::SetNextWindowSize(ImVec2(gsWidth, 390), ImGuiCond_Always);
				ImGui::PushStyleColor(ImGuiCol_WindowBg, 0xB5000000 | (ImGui::GetColorU32(ImGuiCol_WindowBg) & 0x00FFFFFF));
				ImGui::PushStyleColor(ImGuiCol_ChildBg, 0x55000000 | (ImGui::GetColorU32(ImGuiCol_WindowBg) & 0x00FFFFFF));
				ImGui::Begin("##dap_wnd_gsoutput", 0, ImGuiWindowFlags_NoTitleBar | ImGuiWindowFlags_NoMove | ImGuiWindowFlags_NoResize);

				Get(ViewID::DebugGeometryOutput)->Update(delta);

				ImGui::End();
				ImGui::PopStyleColor(2);
			}

			// TS CONTROL OUTPUT //
			else if (m_data->Debugger.GetStage() == ShaderStage::TessellationControl) {
				float tsWidth = 280;

				ImGui::SetNextWindowPos(ImVec2((float)m_width - 310 - 30 - tsWidth, (float)m_height - 420));
				ImGui::SetNextWindowSize(ImVec2(tsWidth, 390), ImGuiCond_Always);
				ImGui::PushStyleColor(ImGuiCol_WindowBg, 0xB5000000 | (ImGui::GetColorU32(ImGuiCol_WindowBg) & 0x00FFFFFF));
				ImGui::PushStyleColor(ImGuiCol_ChildBg, 0x55000000 | (ImGui::GetColorU32(ImGuiCol_WindowBg) & 0x00FFFFFF));
				ImGui::Begin("##dap_wnd_tcsoutput", 0, ImGuiWindowFlags_NoTitleBar | ImGuiWindowFlags_NoMove | ImGuiWindowFlags_NoResize);

				Get(ViewID::DebugTessControlOutput)->Update(delta); // TODO: doesn't work for some reason

				ImGui::End();
				ImGui::PopStyleColor(2);
			}
		}
	}

	UIView* GUIManager::Get(ViewID view)
	{
		if (view == ViewID::Options)
			return m_options;
		if (view == ViewID::ObjectPreview)
			return m_objectPrev;
		if (view >= ViewID::DebugWatch && view <= ViewID::DebugImmediate)
			return m_debugViews[(int)view - (int)ViewID::DebugWatch];
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
	void GUIManager::SaveSettings() const
	{
		std::ofstream data("data/gui.dat");

		for (auto& view : m_views)
			data.put((char)view->Visible);
		for (auto& dview : m_debugViews)
			data.put((char)dview->Visible);

		data.close();
	}
	void GUIManager::LoadSettings()
	{
		std::ifstream data("data/gui.dat");

		if (data.is_open()) {
			for (auto& view : m_views)
				view->Visible = data.get();
			for (auto& dview : m_debugViews)
				dview->Visible = data.get();

			data.close();
		}

		Get(ViewID::Code)->Visible = false;

		dynamic_cast<OptionsUI*>(m_options)->ApplyTheme();
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
		PinnedUI* pinUI = dynamic_cast<PinnedUI*>(Get(ViewID::Pinned));
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
								for (int m = 0; m < mems.size(); m++) {
									std::string memName = std::string(name.c_str()) + "." + mems[m].Name; // hack for \0
									curType.push(mems[m]);
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
		PinnedUI* pinUI = dynamic_cast<PinnedUI*>(Get(ViewID::Pinned));
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
	void GUIManager::m_addProjectToRecents(const std::string& file)
	{
		std::string fileToBeOpened = file;
		for (int i = 0; i < m_recentProjects.size(); i++) {
			if (m_recentProjects[i] == fileToBeOpened) {
				m_recentProjects.erase(m_recentProjects.begin() + i);
				break;
			}
		}
		m_recentProjects.insert(m_recentProjects.begin(), fileToBeOpened);
		if (m_recentProjects.size() > 9)
			m_recentProjects.pop_back();
	}
	void GUIManager::m_createNewProject()
	{
		CodeEditorUI* codeUI = dynamic_cast<CodeEditorUI*>(Get(ViewID::Code));
		codeUI->SetTrackFileChanges(false);

		m_saveAsOldFile = m_data->Parser.GetOpenedFile();

		std::string outputPath = Settings::Instance().ConvertPath("temp/");

		// first clear the old data and create new directory
		std::error_code ec;
		std::filesystem::remove_all(outputPath, ec);
		std::filesystem::create_directory(outputPath);

		if (m_selectedTemplate == "?empty") {
			Settings::Instance().Project.FPCamera = false;
			Settings::Instance().Project.ClearColor = glm::vec4(0, 0, 0, 0);

			ResetWorkspace();
			m_data->Pipeline.New(false);

			SDL_SetWindowTitle(m_wnd, "SHADERed");
		} else {
			m_data->Parser.SetTemplate(m_selectedTemplate);

			ResetWorkspace();
			m_data->Pipeline.New();
			m_data->Parser.SetTemplate(Settings::Instance().General.StartUpTemplate);

			SDL_SetWindowTitle(m_wnd, ("SHADERed (" + m_selectedTemplate + ")").c_str());
		}

		std::string savePath = (std::filesystem::path(outputPath) / "project.sprj").generic_string();
		m_data->Parser.SaveAs(savePath, true);
		Open(savePath);
		m_data->Parser.SetOpenedFile("");

		codeUI->SetTrackFileChanges(Settings::Instance().General.RecompileOnFileChange);
	}
	bool GUIManager::Save()
	{
		if (m_data->Parser.GetOpenedFile().empty()) {
			if (!dynamic_cast<CodeEditorUI*>(Get(ViewID::Code))->RequestedProjectSave) {
				SaveAsProject(true);
				return true;
			}
			return false;
		}

		m_data->Parser.Save();

		dynamic_cast<CodeEditorUI*>(Get(ViewID::Code))->SaveAll();

		std::vector<PipelineItem*> passes = m_data->Pipeline.GetList();
		for (PipelineItem*& pass : passes)
			m_data->Renderer.Recompile(pass->Name);

		m_recompiledAll = true;

		return true;
	}
	void GUIManager::SaveAsProject(bool restoreCached, std::function<void(bool)> handle, std::function<void()> preHandle)
	{
		m_saveAsRestoreCache = restoreCached;
		m_saveAsHandle = std::move(handle);
		m_saveAsPreHandle = std::move(preHandle);
		ifd::FileDialog::Instance().Save("SaveProjectDlg", "Save project", "SHADERed project (*.sprj){.sprj},.*");
	}
	void GUIManager::Open(const std::string& file)
	{
		StopDebugging();

		glm::ivec2 curSize = m_data->Renderer.GetLastRenderSize();

		this->ResetWorkspace();
		m_data->Renderer.Pause(false); // unpause

		m_data->Parser.Open(file);

		// add to recents
		m_addProjectToRecents(file);

		std::string projName = m_data->Parser.GetOpenedFile();
		projName = projName.substr(projName.find_last_of("/\\") + 1);

		m_data->Renderer.Pause(Settings::Instance().Preview.PausedOnStartup);
		if (m_data->Renderer.IsPaused())
			m_data->Renderer.Render(curSize.x, curSize.y);

		SDL_SetWindowTitle(m_wnd, ("SHADERed (" + projName + ")").c_str());
	}

	void GUIManager::SetCommandLineOptions(CommandLineOptionParser& options)
	{
		m_cmdArguments = &options;
		SetMinimalMode(options.MinimalMode);

		if (options.Render) {
			m_savePreviewCachedTime = options.RenderTime;
			m_savePreviewTimeDelta = 1 / 60.0f;
			m_savePreviewFrameIndex = options.RenderFrameIndex;
			m_savePreviewSupersample = 0;

			switch (options.RenderSupersampling) {
			case 2: m_savePreviewSupersample = 1; break;
			case 4: m_savePreviewSupersample = 2; break;
			case 8: m_savePreviewSupersample = 3; break;
			}

			m_previewSavePath = options.RenderPath;
			m_previewSaveSize = glm::ivec2(options.RenderWidth, options.RenderHeight);
			m_savePreviewSeq = options.RenderSequence;
			m_savePreviewSeqDuration = options.RenderSequenceDuration;
			m_savePreviewSeqFPS = options.RenderSequenceFPS;
		}
	}

	void GUIManager::SavePreviewToFile()
	{
		int sizeMulti = 1;
		switch (m_savePreviewSupersample) {
		case 1: sizeMulti = 2; break;
		case 2: sizeMulti = 4; break;
		case 3: sizeMulti = 8; break;
		default:;
		}
		int actualSizeX = m_previewSaveSize.x * sizeMulti;
		int actualSizeY = m_previewSaveSize.y * sizeMulti;

		SystemVariableManager::Instance().SetSavingToFile(true);

		// normal render
		if (!m_savePreviewSeq) {
			if (actualSizeX > 0 && actualSizeY > 0) {
				SystemVariableManager::Instance().CopyState();

				SystemVariableManager::Instance().SetTimeDelta(m_savePreviewTimeDelta);
				SystemVariableManager::Instance().SetFrameIndex(m_savePreviewFrameIndex);
				SystemVariableManager::Instance().SetKeysWASD(m_savePreviewWASD[0], m_savePreviewWASD[1], m_savePreviewWASD[2], m_savePreviewWASD[3]);
				SystemVariableManager::Instance().SetMousePosition(m_savePreviewMouse.x, m_savePreviewMouse.y);
				SystemVariableManager::Instance().SetMouse(m_savePreviewMouse.x, m_savePreviewMouse.y, m_savePreviewMouse.z, m_savePreviewMouse.w);

				m_data->Renderer.Render(actualSizeX, actualSizeY);

				SystemVariableManager::Instance().AdvanceTimer(m_savePreviewCachedTime - m_savePreviewTime);
			}

			auto* pixels = static_cast<unsigned char*>(malloc(actualSizeX * actualSizeY * 4));
			unsigned char* outPixels = nullptr;

			if (sizeMulti != 1)
				outPixels = (unsigned char*)malloc(m_previewSaveSize.x * m_previewSaveSize.y * 4);
			else
				outPixels = pixels;

			GLuint tex = m_data->Renderer.GetTexture();
			glBindTexture(GL_TEXTURE_2D, tex);
			glGetTexImage(GL_TEXTURE_2D, 0, GL_RGBA, GL_UNSIGNED_BYTE, pixels);
			glBindTexture(GL_TEXTURE_2D, 0);

			// resize image
			if (sizeMulti != 1) {
				stbir_resize_uint8(pixels, actualSizeX, actualSizeY, actualSizeX * 4,
					outPixels, m_previewSaveSize.x, m_previewSaveSize.y, m_previewSaveSize.x * 4, 4);
			}

			std::string ext = m_previewSavePath.substr(m_previewSavePath.find_last_of('.') + 1);

			if (ext == "jpg" || ext == "jpeg")
				stbi_write_jpg(m_previewSavePath.c_str(), m_previewSaveSize.x, m_previewSaveSize.y, 4, outPixels, 100);
			else if (ext == "bmp")
				stbi_write_bmp(m_previewSavePath.c_str(), m_previewSaveSize.x, m_previewSaveSize.y, 4, outPixels);
			else if (ext == "tga")
				stbi_write_tga(m_previewSavePath.c_str(), m_previewSaveSize.x, m_previewSaveSize.y, 4, outPixels);
			else
				stbi_write_png(m_previewSavePath.c_str(), m_previewSaveSize.x, m_previewSaveSize.y, 4, outPixels, m_previewSaveSize.x * 4);

			if (sizeMulti != 1) free(outPixels);
			free(pixels);
		} else { // sequence render
			float seqDelta = 1.0f / (float)m_savePreviewSeqFPS;

			if (actualSizeX > 0 && actualSizeY > 0) {
				SystemVariableManager::Instance().SetKeysWASD(m_savePreviewWASD[0], m_savePreviewWASD[1], m_savePreviewWASD[2], m_savePreviewWASD[3]);
				SystemVariableManager::Instance().SetMousePosition(m_savePreviewMouse.x, m_savePreviewMouse.y);
				SystemVariableManager::Instance().SetMouse(m_savePreviewMouse.x, m_savePreviewMouse.y, m_savePreviewMouse.z, m_savePreviewMouse.w);

				float curTime = 0.0f;

				GLuint tex = m_data->Renderer.GetTexture();

				size_t lastDot = m_previewSavePath.find_last_of('.');
				std::string ext = lastDot == std::string::npos ? "png" : m_previewSavePath.substr(lastDot + 1);
				std::string filename = m_previewSavePath;

				// allow only one %??d
				bool inFormat = false;
				int lastFormatPos = -1;
				int formatCount = 0;
				for (int i = 0; i < filename.size(); i++) {
					if (filename[i] == '%') {
						inFormat = true;
						lastFormatPos = i;
						continue;
					}

					if (inFormat) {
						if (isdigit(filename[i])) {
						} else {
							if (filename[i] != '%' && ((filename[i] == 'd' && formatCount > 0) || (filename[i] != 'd'))) {
								filename.insert(lastFormatPos, 1, '%');
							}

							if (filename[i] == 'd')
								formatCount++;
							inFormat = false;
						}
					}
				}

				// no %d found? add one
				if (formatCount == 0) {
					int frameCountDigits = (int)log10((int)(m_savePreviewSeqDuration / seqDelta)) + 1;
					filename.insert(lastDot == std::string::npos ? filename.size() : lastDot, "%0" + std::to_string(frameCountDigits) + "d"); // frame%d
				}

				SystemVariableManager::Instance().AdvanceTimer(m_savePreviewCachedTime - m_savePreviewTimeDelta);
				SystemVariableManager::Instance().SetTimeDelta(seqDelta);

				stbi_write_png_compression_level = 5; // set to lowest compression level

				int tCount = (int)std::thread::hardware_concurrency();
				tCount = tCount == 0 ? 2 : tCount;

				auto** pixels = new unsigned char*[tCount];
				auto** outPixels = new unsigned char*[tCount];
				int* curFrame = new int[tCount];
				bool* needsUpdate = new bool[tCount];
				auto** threadPool = new std::thread*[tCount];
				std::atomic<bool> isOver = false;

				for (int i = 0; i < tCount; i++) {
					curFrame[i] = 0;
					needsUpdate[i] = true;
					pixels[i] = (unsigned char*)malloc(actualSizeX * actualSizeY * 4);

					if (sizeMulti != 1)
						outPixels[i] = (unsigned char*)malloc(m_previewSaveSize.x * m_previewSaveSize.y * 4);
					else
						outPixels[i] = nullptr;

					threadPool[i] = new std::thread([ext, filename, sizeMulti, actualSizeX, actualSizeY, &outPixels, &pixels, &needsUpdate, &curFrame, &isOver](int worker, int w, int h) {
						char prevSavePath[SHADERED_MAX_PATH];
						while (!isOver) {
							if (needsUpdate[worker])
								continue;

							// resize image
							if (sizeMulti != 1) {
								stbir_resize_uint8(pixels[worker], actualSizeX, actualSizeY, actualSizeX * 4,
									outPixels[worker], w, h, w * 4, 4);
							} else
								outPixels[worker] = pixels[worker];

							snprintf(prevSavePath, sizeof(prevSavePath), filename.c_str(), curFrame[worker]);

							if (ext == "jpg" || ext == "jpeg")
								stbi_write_jpg(prevSavePath, w, h, 4, outPixels[worker], 100);
							else if (ext == "bmp")
								stbi_write_bmp(prevSavePath, w, h, 4, outPixels[worker]);
							else if (ext == "tga")
								stbi_write_tga(prevSavePath, w, h, 4, outPixels[worker]);
							else
								stbi_write_png(prevSavePath, w, h, 4, outPixels[worker], w * 4);

							needsUpdate[worker] = true;
						}
					},
						i, m_previewSaveSize.x, m_previewSaveSize.y);
				}

				int globalFrame = 0;
				while (curTime < m_savePreviewSeqDuration) {
					int hasWork = -1;
					for (int i = 0; i < tCount; i++)
						if (needsUpdate[i]) {
							hasWork = i;
							break;
						}

					if (hasWork == -1)
						continue;

					SystemVariableManager::Instance().CopyState();
					SystemVariableManager::Instance().SetFrameIndex(m_savePreviewFrameIndex + globalFrame);

					m_data->Renderer.Render(actualSizeX, actualSizeY);

					glBindTexture(GL_TEXTURE_2D, tex);
					glGetTexImage(GL_TEXTURE_2D, 0, GL_RGBA, GL_UNSIGNED_BYTE, pixels[hasWork]);
					glBindTexture(GL_TEXTURE_2D, 0);

					SystemVariableManager::Instance().AdvanceTimer(seqDelta);

					curTime += seqDelta;
					curFrame[hasWork] = globalFrame;
					needsUpdate[hasWork] = false;
					globalFrame++;
				}
				isOver = true;

				for (int i = 0; i < tCount; i++) {
					if (threadPool[i]->joinable())
						threadPool[i]->join();
					free(pixels[i]);
					if (sizeMulti != 1)
						free(outPixels[i]);
					delete threadPool[i];
				}
				delete[] pixels;
				delete[] outPixels;
				delete[] curFrame;
				delete[] needsUpdate;
				delete[] threadPool;

				stbi_write_png_compression_level = 8; // set back to default compression level
			}
		}

		SystemVariableManager::Instance().SetSavingToFile(false);
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

	void GUIManager::m_renderPopups(float delta)
	{
		// open popup for creating items
		if (m_isCreateItemPopupOpened) {
			ImGui::OpenPopup("Create Item##main_create_item");
			m_isCreateItemPopupOpened = false;
		}

		// open popup for saving preview as image
		if (m_savePreviewPopupOpened) {
			ImGui::OpenPopup("Save Preview##main_save_preview");
			m_previewSavePath = "render.png";
			m_savePreviewPopupOpened = false;
			m_wasPausedPrior = m_data->Renderer.IsPaused();
			m_savePreviewCachedTime = m_savePreviewTime = SystemVariableManager::Instance().GetTime();
			m_savePreviewTimeDelta = SystemVariableManager::Instance().GetTimeDelta();
			m_savePreviewCachedFIndex = m_savePreviewFrameIndex = static_cast<int>(SystemVariableManager::Instance().GetFrameIndex());
			glm::ivec4 wasd = SystemVariableManager::Instance().GetKeysWASD();
			m_savePreviewWASD[0] = wasd.x;
			m_savePreviewWASD[1] = wasd.y;
			m_savePreviewWASD[2] = wasd.z;
			m_savePreviewWASD[3] = wasd.w;
			m_savePreviewMouse = SystemVariableManager::Instance().GetMouse();
			m_savePreviewSupersample = 0;

			m_savePreviewSeq = false;

			m_data->Renderer.Pause(true);
		}

		// open popup for creating cubemap
		if (m_isCreateCubemapOpened) {
			ImGui::OpenPopup("Create cubemap##main_create_cubemap");
			m_isCreateCubemapOpened = false;
		}

		// open popup for creating buffer
		if (m_isCreateBufferOpened) {
			ImGui::OpenPopup("Create buffer##main_create_buffer");
			m_isCreateBufferOpened = false;
		}

		// open popup for creating image
		if (m_isCreateImgOpened) {
			ImGui::OpenPopup("Create image##main_create_img");
			m_isCreateImgOpened = false;
		}

		// open popup for creating image3D
		if (m_isCreateImg3DOpened) {
			ImGui::OpenPopup("Create 3D image##main_create_img3D");
			m_isCreateImg3DOpened = false;
		}

		// open popup that shows the list of incompatible plugins
		if (m_isIncompatPluginsOpened) {
			ImGui::OpenPopup("Incompatible plugins##main_incompat_plugins");
			m_isIncompatPluginsOpened = false;
		}

		// open popup for creating camera snapshot
		if (m_isRecordCameraSnapshotOpened) {
			ImGui::OpenPopup("Camera snapshot name##main_camsnap_name");
			m_isRecordCameraSnapshotOpened = false;
		}

		// open popup for creating render texture
		if (m_isCreateRTOpened) {
			ImGui::OpenPopup("Create RT##main_create_rt");
			m_isCreateRTOpened = false;
		}

		// open popup for creating keyboard texture
		if (m_isCreateKBTxtOpened) {
			ImGui::OpenPopup("Create KeyboardTexture##main_create_kbtxt");
			m_isCreateKBTxtOpened = false;
		}

		// open about popup
		if (m_isAboutOpen) {
			ImGui::OpenPopup("About##main_about");
			m_isAboutOpen = false;
		}

		// open tips window
		if (m_isInfoOpened) {
			ImGui::OpenPopup("Information##main_info");
			m_isInfoOpened = false;
		}

		// open export as c++ app
		if (m_exportAsCPPOpened) {
			ImGui::OpenPopup("Export as C++ project##main_export_as_cpp");
			m_expcppError = false;
			m_exportAsCPPOpened = false;
		}

		// open changelog popup
		if (m_isChangelogOpened) {
			ImGui::OpenPopup("Changelog##main_upd_changelog");
			m_isChangelogOpened = false;
		}

		// open tips popup
		if (m_tipOpened) {
			ImGui::OpenPopup("###main_tips");
			m_tipOpened = false;
		}

		// open browse online window
		if (m_isBrowseOnlineOpened) {
			ImGui::OpenPopup("Browse online##browse_online");
			m_isBrowseOnlineOpened = false;

			dynamic_cast<BrowseOnlineUI*>(m_browseOnline)->Open();
		}

		// File dialogs (open project, create texture, create audio, pick cubemap face texture)
		if (ifd::FileDialog::Instance().IsDone("OpenProjectDlg")) {
			if (ifd::FileDialog::Instance().HasResult())
				Open(ifd::FileDialog::Instance().GetResult().u8string());

			ifd::FileDialog::Instance().Close();
		}
		if (ifd::FileDialog::Instance().IsDone("CreateTextureDlg")) {
			if (ifd::FileDialog::Instance().HasResult()) {
				const std::vector<std::filesystem::path>& results = ifd::FileDialog::Instance().GetResults();
				for (const auto& res : results)
					m_data->Objects.CreateTexture(res.u8string());
			}

			ifd::FileDialog::Instance().Close();
		}
		if (ifd::FileDialog::Instance().IsDone("CreateTexture3DDlg")) {
			if (ifd::FileDialog::Instance().HasResult()) {
				const std::vector<std::filesystem::path>& results = ifd::FileDialog::Instance().GetResults();
				for (const auto& res : results)
					m_data->Objects.CreateTexture3D(res.u8string());
			}

			ifd::FileDialog::Instance().Close();
		}
		if (ifd::FileDialog::Instance().IsDone("CreateAudioDlg")) {
			if (ifd::FileDialog::Instance().HasResult()) {
				std::string rfile = m_data->Parser.GetRelativePath(ifd::FileDialog::Instance().GetResult().u8string());
				if (!rfile.empty())
					m_data->Objects.CreateAudio(rfile);
			}

			ifd::FileDialog::Instance().Close();
		}
		if (ifd::FileDialog::Instance().IsDone("SaveProjectDlg")) {
			if (ifd::FileDialog::Instance().HasResult()) {
				if (m_saveAsPreHandle)
					m_saveAsPreHandle();

				std::string fileName = ifd::FileDialog::Instance().GetResult().u8string();
				m_data->Parser.SaveAs(fileName, true);

				// cache opened code editors
				CodeEditorUI* editor = dynamic_cast<CodeEditorUI*>(Get(ViewID::Code));
				std::vector<std::pair<std::string, ShaderStage>> files = editor->GetOpenedFiles();
				std::vector<std::string> filesData = editor->GetOpenedFilesData();

				// close all
				this->StopDebugging();
				this->ResetWorkspace();

				m_addProjectToRecents(fileName);
				m_data->Parser.Open(fileName);

				std::string projName = m_data->Parser.GetOpenedFile();
				projName = projName.substr(projName.find_last_of("/\\") + 1);

				SDL_SetWindowTitle(m_wnd, ("SHADERed (" + projName + ")").c_str());

				// return cached state
				if (m_saveAsRestoreCache) {
					for (auto& file : files) {
						PipelineItem* item = m_data->Pipeline.Get(file.first.c_str());
						editor->Open(item, file.second);
					}
					editor->SetOpenedFilesData(filesData);
					editor->SaveAll();
				}
			}

			if (m_saveAsHandle != nullptr)
				m_saveAsHandle(ifd::FileDialog::Instance().HasResult());

			ifd::FileDialog::Instance().Close();
		}

		// Create RT popup
		ImGui::SetNextWindowSize(ImVec2(Settings::Instance().CalculateSize(830), Settings::Instance().CalculateSize(550)), ImGuiCond_FirstUseEver);
		if (ImGui::BeginPopupModal("Browse online##browse_online")) {
			m_browseOnline->Update(delta);

			ImGui::SetCursorPosX(ImGui::GetWindowWidth() - Settings::Instance().CalculateSize(90));
			if (ImGui::Button("Close", ImVec2(Settings::Instance().CalculateSize(70), 0)))
				ImGui::CloseCurrentPopup();

			ImGui::EndPopup();
		}

		// Create Item popup
		ImGui::SetNextWindowSize(ImVec2(Settings::Instance().CalculateSize(530), Settings::Instance().CalculateSize(300)), ImGuiCond_Always);
		if (ImGui::BeginPopupModal("Create Item##main_create_item", 0, ImGuiWindowFlags_NoResize)) {
			m_createUI->Update(delta);

			if (ImGui::Button("Ok")) {
				if (m_createUI->Create())
					ImGui::CloseCurrentPopup();
			}
			ImGui::SameLine();
			if (ImGui::Button("Cancel")) {
				m_createUI->Reset();
				ImGui::CloseCurrentPopup();
			}
			ImGui::EndPopup();
		}

		// Create cubemap popup
		ImGui::SetNextWindowSize(ImVec2(Settings::Instance().CalculateSize(430), Settings::Instance().CalculateSize(275)), ImGuiCond_Always);
		if (ImGui::BeginPopupModal("Create cubemap##main_create_cubemap", 0, ImGuiWindowFlags_NoResize)) {
			static char buf[65] = { 0 };
			ImGui::InputText("Name", buf, 64);

			static std::string left, top, front, bottom, right, back;
			float btnWidth = Settings::Instance().CalculateSize(65.0f);

			ImGui::Text("Left: %s", std::filesystem::path(left).filename().string().c_str());
			ImGui::SameLine();
			ImGui::SetCursorPosX(ImGui::GetWindowWidth() - btnWidth);
			if (ImGui::Button("Change##left")) {
				m_cubemapPathPtr = &left;
				ifd::FileDialog::Instance().Open("CubemapFaceDlg", "Select cubemap face - left", "Image file (*.png;*.jpg;*.jpeg;*.bmp;*.tga;*.dds){.png,.jpg,.jpeg,.bmp,.tga,.dds},.*");
			}

			ImGui::Text("Top: %s", std::filesystem::path(top).filename().string().c_str());
			ImGui::SameLine();
			ImGui::SetCursorPosX(ImGui::GetWindowWidth() - btnWidth);
			if (ImGui::Button("Change##top")) {
				m_cubemapPathPtr = &top;
				ifd::FileDialog::Instance().Open("CubemapFaceDlg", "Select cubemap face - top", "Image file (*.png;*.jpg;*.jpeg;*.bmp;*.tga;*.dds){.png,.jpg,.jpeg,.bmp,.tga,.dds},.*");
			}

			ImGui::Text("Front: %s", std::filesystem::path(front).filename().string().c_str());
			ImGui::SameLine();
			ImGui::SetCursorPosX(ImGui::GetWindowWidth() - btnWidth);
			if (ImGui::Button("Change##front")) {
				m_cubemapPathPtr = &front;
				ifd::FileDialog::Instance().Open("CubemapFaceDlg", "Select cubemap face - front", "Image file (*.png;*.jpg;*.jpeg;*.bmp;*.tga;*.dds){.png,.jpg,.jpeg,.bmp,.tga,.dds},.*");
			}

			ImGui::Text("Bottom: %s", std::filesystem::path(bottom).filename().string().c_str());
			ImGui::SameLine();
			ImGui::SetCursorPosX(ImGui::GetWindowWidth() - btnWidth);
			if (ImGui::Button("Change##bottom")) {
				m_cubemapPathPtr = &bottom;
				ifd::FileDialog::Instance().Open("CubemapFaceDlg", "Select cubemap face - bottom", "Image file (*.png;*.jpg;*.jpeg;*.bmp;*.tga;*.dds){.png,.jpg,.jpeg,.bmp,.tga,.dds},.*");
			}

			ImGui::Text("Right: %s", std::filesystem::path(right).filename().string().c_str());
			ImGui::SameLine();
			ImGui::SetCursorPosX(ImGui::GetWindowWidth() - btnWidth);
			if (ImGui::Button("Change##right")) {
				m_cubemapPathPtr = &right;
				ifd::FileDialog::Instance().Open("CubemapFaceDlg", "Select cubemap face - right", "Image file (*.png;*.jpg;*.jpeg;*.bmp;*.tga;*.dds){.png,.jpg,.jpeg,.bmp,.tga,.dds},.*");
			}

			ImGui::Text("Back: %s", std::filesystem::path(back).filename().string().c_str());
			ImGui::SameLine();
			ImGui::SetCursorPosX(ImGui::GetWindowWidth() - btnWidth);
			if (ImGui::Button("Change##back")) {
				m_cubemapPathPtr = &back;
				ifd::FileDialog::Instance().Open("CubemapFaceDlg", "Select cubemap face - back", "Image file (*.png;*.jpg;*.jpeg;*.bmp;*.tga;*.dds){.png,.jpg,.jpeg,.bmp,.tga,.dds},.*");
			}

			if (ifd::FileDialog::Instance().IsDone("CubemapFaceDlg")) {
				if (ifd::FileDialog::Instance().HasResult() && m_cubemapPathPtr != nullptr)
					*m_cubemapPathPtr = m_data->Parser.GetRelativePath(ifd::FileDialog::Instance().GetResult().u8string());

				ifd::FileDialog::Instance().Close();
			}

			if (ImGui::Button("Ok") && strlen(buf) > 0 && !m_data->Objects.Exists(buf)) {
				if (m_data->Objects.CreateCubemap(buf, left, top, front, bottom, right, back))
					ImGui::CloseCurrentPopup();
			}
			ImGui::SameLine();
			if (ImGui::Button("Cancel")) ImGui::CloseCurrentPopup();
			ImGui::EndPopup();
		}

		// Create RT popup
		ImGui::SetNextWindowSize(ImVec2(Settings::Instance().CalculateSize(430), Settings::Instance().CalculateSize(155)), ImGuiCond_Always);
		if (ImGui::BeginPopupModal("Create RT##main_create_rt", 0, ImGuiWindowFlags_NoResize)) {
			static char buf[65] = { 0 };
			ImGui::InputText("Name", buf, 65);

			if (ImGui::Button("Ok")) {
				if (m_data->Objects.CreateRenderTexture(buf)) {
					((PropertyUI*)Get(ViewID::Properties))->Open(m_data->Objects.Get(buf));
					ImGui::CloseCurrentPopup();
				}
			}
			ImGui::SameLine();
			if (ImGui::Button("Cancel")) ImGui::CloseCurrentPopup();
			ImGui::EndPopup();
		}

		// Create KB texture
		ImGui::SetNextWindowSize(ImVec2(Settings::Instance().CalculateSize(430), Settings::Instance().CalculateSize(155)), ImGuiCond_Always);
		if (ImGui::BeginPopupModal("Create KeyboardTexture##main_create_kbtxt", 0, ImGuiWindowFlags_NoResize)) {
			static char buf[65] = { 0 };
			ImGui::InputText("Name", buf, 65);

			if (ImGui::Button("Ok")) {
				if (m_data->Objects.CreateKeyboardTexture(buf))
					ImGui::CloseCurrentPopup();
			}

			ImGui::SameLine();
			if (ImGui::Button("Cancel")) ImGui::CloseCurrentPopup();
			ImGui::EndPopup();
		}

		// Record cam snapshot
		ImGui::SetNextWindowSize(ImVec2(Settings::Instance().CalculateSize(430), Settings::Instance().CalculateSize(155)), ImGuiCond_Always);
		if (ImGui::BeginPopupModal("Camera snapshot name##main_camsnap_name", 0, ImGuiWindowFlags_NoResize)) {
			static char buf[65] = { 0 };
			ImGui::InputText("Name", buf, 64);

			if (ImGui::Button("Ok")) {
				bool exists = false;
				auto& names = CameraSnapshots::GetList();
				for (const auto& name : names) {
					if (strcmp(buf, name.c_str()) == 0) {
						exists = true;
						break;
					}
				}

				if (!exists) {
					CameraSnapshots::Add(buf, SystemVariableManager::Instance().GetViewMatrix());
					ImGui::CloseCurrentPopup();
				}
			}
			ImGui::SameLine();
			if (ImGui::Button("Cancel")) ImGui::CloseCurrentPopup();
			ImGui::EndPopup();
		}

		// Create buffer popup
		ImGui::SetNextWindowSize(ImVec2(Settings::Instance().CalculateSize(430), Settings::Instance().CalculateSize(155)), ImGuiCond_Always);
		if (ImGui::BeginPopupModal("Create buffer##main_create_buffer", 0, ImGuiWindowFlags_NoResize)) {
			static char buf[65] = { 0 };
			ImGui::InputText("Name", buf, 64);

			if (ImGui::Button("Ok")) {
				if (m_data->Objects.CreateBuffer(buf))
					ImGui::CloseCurrentPopup();
			}
			ImGui::SameLine();
			if (ImGui::Button("Cancel")) ImGui::CloseCurrentPopup();
			ImGui::EndPopup();
		}

		// Create empty image popup
		ImGui::SetNextWindowSize(ImVec2(Settings::Instance().CalculateSize(430), Settings::Instance().CalculateSize(175)), ImGuiCond_Always);
		if (ImGui::BeginPopupModal("Create image##main_create_img", 0, ImGuiWindowFlags_NoResize)) {
			static char buf[65] = { 0 };
			static glm::ivec2 size(0, 0);

			ImGui::InputText("Name", buf, 64);
			ImGui::DragInt2("Size", glm::value_ptr(size));
			if (size.x <= 0) size.x = 1;
			if (size.y <= 0) size.y = 1;

			if (ImGui::Button("Ok")) {
				if (m_data->Objects.CreateImage(buf, size))
					ImGui::CloseCurrentPopup();
			}
			ImGui::SameLine();
			if (ImGui::Button("Cancel"))
				ImGui::CloseCurrentPopup();
			ImGui::EndPopup();
		}

		// Create empty 3D image
		ImGui::SetNextWindowSize(ImVec2(Settings::Instance().CalculateSize(430), Settings::Instance().CalculateSize(175)), ImGuiCond_Always);
		if (ImGui::BeginPopupModal("Create 3D image##main_create_img3D", 0, ImGuiWindowFlags_NoResize)) {
			static char buf[65] = { 0 };
			static glm::ivec3 size(0, 0, 0);

			ImGui::InputText("Name", buf, 64);
			ImGui::DragInt3("Size", glm::value_ptr(size));
			if (size.x <= 0) size.x = 1;
			if (size.y <= 0) size.y = 1;
			if (size.z <= 0) size.z = 1;

			if (ImGui::Button("Ok")) {
				if (m_data->Objects.CreateImage3D(buf, size))
					ImGui::CloseCurrentPopup();
			}
			ImGui::SameLine();
			if (ImGui::Button("Cancel"))
				ImGui::CloseCurrentPopup();
			ImGui::EndPopup();
		}

		// Create about popup
		ImGui::SetNextWindowSize(ImVec2(Settings::Instance().CalculateSize(270), Settings::Instance().CalculateSize(220)), ImGuiCond_Always);
		if (ImGui::BeginPopupModal("About##main_about", 0, ImGuiWindowFlags_NoResize)) {
			ImGui::TextWrapped("(C) 2018 - 2021 dfranx");
			ImGui::TextWrapped("Version %s", WebAPI::Version);
			ImGui::TextWrapped("Internal version: %d", WebAPI::InternalVersion);
			ImGui::TextWrapped("Compute shaders: %s", GLEW_ARB_compute_shader ? "true" : "false");
			ImGui::TextWrapped("Tessellation shaders: %s", GLEW_ARB_tessellation_shader ? "true" : "false");
			if (GLEW_ARB_tessellation_shader)
				ImGui::TextWrapped("GL_MAX_PATCH_VERTICES: %d", m_data->Renderer.GetMaxPatchVertices());

			ImGui::NewLine();
			UIHelper::Markdown("SHADERed is [open source](https://www.github.com/dfranx/SHADERed)");
			ImGui::NewLine();

			ImGui::Separator();

			if (ImGui::Button("Ok"))
				ImGui::CloseCurrentPopup();
			ImGui::EndPopup();
		}

		// Create "Information" popup
		ImGui::SetNextWindowSize(ImVec2(Settings::Instance().CalculateSize(550), Settings::Instance().CalculateSize(460)), ImGuiCond_Once);
		if (ImGui::BeginPopupModal("Information##main_info")) {
			ImGui::TextWrapped("Here you can find random information about various features");

			ImGui::TextWrapped("System variables");
			ImGui::Separator();
			ImGui::TextWrapped("Time (float) - time elapsed since app start");
			ImGui::TextWrapped("TimeDelta (float) - render time");
			ImGui::TextWrapped("FrameIndex (uint) - current frame index");
			ImGui::TextWrapped("ViewportSize (vec2) - rendering window size");
			ImGui::TextWrapped("MousePosition (vec2) - normalized mouse position in the Preview window");
			ImGui::TextWrapped("View (mat4) - a built-in camera matrix");
			ImGui::TextWrapped("Projection (mat4) - a built-in projection matrix");
			ImGui::TextWrapped("ViewProjection (mat4) - View*Projection matrix");
			ImGui::TextWrapped("Orthographic (mat4) - an orthographic matrix");
			ImGui::TextWrapped("ViewOrthographic (mat4) - View*Orthographic");
			ImGui::TextWrapped("GeometryTransform (mat4) - applies Scale, Rotation and Position to geometry");
			ImGui::TextWrapped("IsPicked (bool) - check if current item is selected");
			ImGui::TextWrapped("CameraPosition (vec4) - current camera position");
			ImGui::TextWrapped("CameraPosition3 (vec3) - current camera position");
			ImGui::TextWrapped("CameraDirection3 (vec3) - camera view direction");
			ImGui::TextWrapped("KeysWASD (vec4) - W, A, S or D keys state");
			ImGui::TextWrapped("Mouse (vec4) - vec4(x,y,left,right) updated every frame");
			ImGui::TextWrapped("MouseButton (vec4) - vec4(viewX,viewY,clickX,clickY) updated only when left mouse button is down");

			ImGui::NewLine();
			ImGui::Separator();

			ImGui::TextWrapped("KeyboardTexture:");
			ImGui::PushFont(dynamic_cast<CodeEditorUI*>(Get(ViewID::Code))->GetImFont());
			m_kbInfo.Render("##kbtext_info", ImVec2(0, 600), true);
			ImGui::PopFont();

			ImGui::NewLine();
			ImGui::Separator();

			ImGui::TextWrapped("Do you have an idea for a feature? Suggest it ");
			ImGui::SameLine();
			if (ImGui::Button("here"))
				UIHelper::ShellOpen("https://github.com/dfranx/SHADERed/issues");
			ImGui::Separator();
			ImGui::NewLine();

			if (ImGui::Button("Ok"))
				ImGui::CloseCurrentPopup();
			ImGui::EndPopup();
		}

		// Create a new project
		ImGui::SetNextWindowSize(ImVec2(Settings::Instance().CalculateSize(450), Settings::Instance().CalculateSize(150)), ImGuiCond_Always);
		if (ImGui::BeginPopupModal("Are you sure?##main_new_proj", 0, ImGuiWindowFlags_NoResize)) {
			ImGui::TextWrapped("You will lose your unsaved progress if you create a new project. Are you sure you want to continue?");
			if (ImGui::Button("Yes")) {

				ImGui::CloseCurrentPopup();
			}
			ImGui::SameLine();
			if (ImGui::Button("No"))
				ImGui::CloseCurrentPopup();
			ImGui::EndPopup();
		}

		// Save preview
		ImGui::SetNextWindowSize(ImVec2(Settings::Instance().CalculateSize(450), Settings::Instance().CalculateSize(250)), ImGuiCond_Once);
		if (ImGui::BeginPopupModal("Save Preview##main_save_preview")) {
			ImGui::TextWrapped("Path: %s", m_previewSavePath.c_str());
			ImGui::SameLine();
			if (ImGui::Button("...##save_prev_path"))
				ifd::FileDialog::Instance().Save("SavePreviewDlg", "Save", "Image file (*.png;*.jpg;*.jpeg;*.bmp;*.tga){.png,.jpg,.jpeg,.bmp,.tga},.*");
			if (ifd::FileDialog::Instance().IsDone("SavePreviewDlg")) {
				if (ifd::FileDialog::Instance().HasResult())
					m_previewSavePath = ifd::FileDialog::Instance().GetResult().u8string();
				ifd::FileDialog::Instance().Close();
			}

			ImGui::Text("Width: ");
			ImGui::SameLine();
			ImGui::Indent(Settings::Instance().CalculateSize(110));
			ImGui::InputInt("##save_prev_sizew", &m_previewSaveSize.x);
			ImGui::Unindent(Settings::Instance().CalculateSize(110));

			ImGui::Text("Height: ");
			ImGui::SameLine();
			ImGui::Indent(Settings::Instance().CalculateSize(110));
			ImGui::InputInt("##save_prev_sizeh", &m_previewSaveSize.y);
			ImGui::Unindent(Settings::Instance().CalculateSize(110));

			ImGui::Text("Supersampling: ");
			ImGui::SameLine();
			ImGui::Indent(Settings::Instance().CalculateSize(110));
			ImGui::Combo("##save_prev_ssmp", &m_savePreviewSupersample, " 1x\0 2x\0 4x\0 8x\0");
			ImGui::Unindent(Settings::Instance().CalculateSize(110));

			ImGui::Separator();
			if (ImGui::CollapsingHeader("Sequence")) {
				ImGui::TextWrapped("Export a sequence of images");

				/* RECORD */
				ImGui::Text("Record:");
				ImGui::SameLine();
				ImGui::Checkbox("##save_prev_keyw", &m_savePreviewSeq);

				if (!m_savePreviewSeq) {
					ImGui::PushItemFlag(ImGuiItemFlags_Disabled, true);
					ImGui::PushStyleVar(ImGuiStyleVar_Alpha, ImGui::GetStyle().Alpha * 0.5f);
				}

				/* DURATION */
				ImGui::Text("Duration:");
				ImGui::SameLine();
				ImGui::PushItemWidth(-1);
				ImGui::DragFloat("##save_prev_seqdur", &m_savePreviewSeqDuration);
				ImGui::PopItemWidth();

				/* TIME DELTA */
				ImGui::Text("FPS:");
				ImGui::SameLine();
				ImGui::PushItemWidth(-1);
				ImGui::DragInt("##save_prev_seqfps", &m_savePreviewSeqFPS);
				ImGui::PopItemWidth();

				if (!m_savePreviewSeq) {
					ImGui::PopItemFlag();
					ImGui::PopStyleVar();
				}
			}
			ImGui::Separator();

			ImGui::Separator();
			if (ImGui::CollapsingHeader("Advanced")) {
				bool requiresPreviewUpdate = false;

				/* TIME */
				ImGui::Text("Time:");
				ImGui::SameLine();
				ImGui::PushItemWidth(-1);
				if (ImGui::DragFloat("##save_prev_time", &m_savePreviewTime)) {
					float timeAdvance = m_savePreviewTime - SystemVariableManager::Instance().GetTime();
					SystemVariableManager::Instance().AdvanceTimer(timeAdvance);
					requiresPreviewUpdate = true;
				}
				ImGui::PopItemWidth();

				/* TIME DELTA */
				ImGui::Text("Time delta:");
				ImGui::SameLine();
				ImGui::PushItemWidth(-1);
				if (ImGui::DragFloat("##save_prev_timed", &m_savePreviewTimeDelta))
					requiresPreviewUpdate = true;
				ImGui::PopItemWidth();

				/* FRAME INDEX */
				ImGui::Text("Frame index:");
				ImGui::SameLine();
				ImGui::PushItemWidth(-1);
				if (ImGui::DragInt("##save_prev_findex", &m_savePreviewFrameIndex))
					requiresPreviewUpdate = true;
				ImGui::PopItemWidth();

				/* WASD */
				ImGui::Text("WASD:");
				ImGui::SameLine();
				if (ImGui::Checkbox("##save_prev_keyw", &m_savePreviewWASD[0]))
					requiresPreviewUpdate = true;
				ImGui::SameLine();
				if (ImGui::Checkbox("##save_prev_keya", &m_savePreviewWASD[1]))
					requiresPreviewUpdate = true;
				ImGui::SameLine();
				if (ImGui::Checkbox("##save_prev_keys", &m_savePreviewWASD[2]))
					requiresPreviewUpdate = true;
				ImGui::SameLine();
				if (ImGui::Checkbox("##save_prev_keyd", &m_savePreviewWASD[3]))
					requiresPreviewUpdate = true;

				/* MOUSE */
				ImGui::Text("Mouse:");
				ImGui::SameLine();
				if (ImGui::DragFloat2("##save_prev_mpos", glm::value_ptr(m_savePreviewMouse))) {
					SystemVariableManager::Instance().SetMousePosition(m_savePreviewMouse.x, m_savePreviewMouse.y);
					requiresPreviewUpdate = true;
				}
				ImGui::SameLine();
				bool isLeftDown = m_savePreviewMouse.z >= 1.0f;
				if (ImGui::Checkbox("##save_prev_btnleft", &isLeftDown))
					requiresPreviewUpdate = true;
				ImGui::SameLine();
				m_savePreviewMouse.z = isLeftDown;
				bool isRightDown = m_savePreviewMouse.w >= 1.0f;
				if (ImGui::Checkbox("##save_prev_btnright", &isRightDown))
					requiresPreviewUpdate = true;
				m_savePreviewMouse.w = isRightDown;
				SystemVariableManager::Instance().SetMouse(m_savePreviewMouse.x, m_savePreviewMouse.y, m_savePreviewMouse.z, m_savePreviewMouse.w);

				if (requiresPreviewUpdate)
					m_data->Renderer.Render();
			}
			ImGui::Separator();

			bool rerenderPreview = false;
			glm::ivec2 rerenderSize = m_data->Renderer.GetLastRenderSize();

			if (ImGui::Button("Save")) {
				SavePreviewToFile();
				rerenderPreview = true;

				m_data->Renderer.Pause(m_wasPausedPrior);
				ImGui::CloseCurrentPopup();
			}
			ImGui::SameLine();
			if (ImGui::Button("Cancel")) {
				ImGui::CloseCurrentPopup();
				SystemVariableManager::Instance().AdvanceTimer(m_savePreviewCachedTime - m_savePreviewTime);
				m_data->Renderer.Pause(m_wasPausedPrior);
				rerenderPreview = true;
			}
			ImGui::EndPopup();

			if (rerenderPreview)
				m_data->Renderer.Render(rerenderSize.x, rerenderSize.y);

			m_recompiledAll = false;
		}

		// Export as C++ app
		ImGui::SetNextWindowSize(ImVec2(Settings::Instance().CalculateSize(450), Settings::Instance().CalculateSize(300)));
		if (ImGui::BeginPopupModal("Export as C++ project##main_export_as_cpp")) {
			// output file
			ImGui::TextWrapped("Output file: %s", m_expcppSavePath.c_str());
			ImGui::SameLine();
			if (ImGui::Button("...##expcpp_savepath"))
				ifd::FileDialog::Instance().Save("ExportCPPDlg", "Save", "C++ source file (*.cpp;*.cxx){.cpp,.cxx},.*");
			if (ifd::FileDialog::Instance().IsDone("ExportCPPDlg")) {
				if (ifd::FileDialog::Instance().HasResult())
					m_expcppSavePath = ifd::FileDialog::Instance().GetResult().u8string();
				ifd::FileDialog::Instance().Close();
			}

			// store shaders in files
			ImGui::Text("Store shaders in memory: ");
			ImGui::SameLine();
			ImGui::Checkbox("##expcpp_memory_shaders", &m_expcppMemoryShaders);

			// cmakelists
			ImGui::Text("Generate CMakeLists.txt: ");
			ImGui::SameLine();
			ImGui::Checkbox("##expcpp_cmakelists", &m_expcppCmakeFiles);

			// cmake project name
			ImGui::Text("CMake project name: ");
			ImGui::SameLine();
			ImGui::InputText("##expcpp_cmake_name", &m_expcppProjectName[0], 64);

			// copy cmake modules
			ImGui::Text("Copy CMake modules: ");
			ImGui::SameLine();
			ImGui::Checkbox("##expcpp_cmakemodules", &m_expcppCmakeModules);

			// copy stb_image
			ImGui::Text("Copy stb_image.h: ");
			ImGui::SameLine();
			ImGui::Checkbox("##expcpp_stb_image", &m_expcppImage);

			// copy images
			ImGui::Text("Copy images: ");
			ImGui::SameLine();
			ImGui::Checkbox("##expcpp_copy_images", &m_expcppCopyImages);

			// backend
			ImGui::Text("Backend: ");
			ImGui::SameLine();
			ImGui::BeginGroup();
			ImGui::RadioButton("OpenGL", &m_expcppBackend, 0);
			ImGui::SameLine();

			ImGui::PushItemFlag(ImGuiItemFlags_Disabled, true);
			ImGui::RadioButton("DirectX", &m_expcppBackend, 1);
			ImGui::PopItemFlag();
			ImGui::EndGroup();

			if (m_expcppError) ImGui::Text("An error occured. Possible cause: some files are missing.");

			// export || cancel
			if (ImGui::Button("Export")) {
				m_expcppError = !ExportCPP::Export(m_data, m_expcppSavePath, !m_expcppMemoryShaders, m_expcppCmakeFiles, m_expcppProjectName, m_expcppCmakeModules, m_expcppImage, m_expcppCopyImages);
				if (!m_expcppError)
					ImGui::CloseCurrentPopup();
			}
			ImGui::SameLine();
			if (ImGui::Button("Cancel"))
				ImGui::CloseCurrentPopup();
			ImGui::EndPopup();
		}

		// Changelog popup
		ImGui::SetNextWindowSize(ImVec2(Settings::Instance().CalculateSize(370), Settings::Instance().CalculateSize(420)), ImGuiCond_Always);
		if (ImGui::BeginPopupModal("Changelog##main_upd_changelog", 0, ImGuiWindowFlags_NoResize)) {
			UIHelper::Markdown(m_changelogText);

			ImGui::Separator();

			if (ImGui::Button("Ok"))
				ImGui::CloseCurrentPopup();

			ImGui::SameLine();

			if (!m_changelogBlogLink.empty() && ImGui::Button("Open the blogpost"))
				UIHelper::ShellOpen(m_changelogBlogLink);

			ImGui::EndPopup();
		}

		// Tips popup
		ImGui::SetNextWindowSize(ImVec2(Settings::Instance().CalculateSize(470), Settings::Instance().CalculateSize(420)), ImGuiCond_Once);
		if (ImGui::BeginPopupModal((m_tipTitle + "###main_tips").c_str(), 0)) {
			UIHelper::Markdown(m_tipText);

			ImGui::SetCursorPos(ImVec2(ImGui::GetWindowSize().x - Settings::Instance().CalculateSize(50), ImGui::GetWindowSize().y - Settings::Instance().CalculateSize(40)));
			if (ImGui::Button("Ok"))
				ImGui::CloseCurrentPopup();
			ImGui::EndPopup();
		}

		// Incompatible plugins
		ImGui::SetNextWindowSize(ImVec2(Settings::Instance().CalculateSize(470), 0), ImGuiCond_Appearing);
		if (ImGui::BeginPopupModal("Incompatible plugins##main_incompat_plugins")) {
			std::string text = "There's a mismatch between SHADERed's plugin API version and the following plugins' API version:\n";
			for (int i = 0; i < m_data->Plugins.GetIncompatiblePlugins().size(); i++)
				text += "  * " + m_data->Plugins.GetIncompatiblePlugins()[i] + "\n";
			text += "Either update your instance of SHADERed or update these plugins.";

			UIHelper::Markdown(text);

			ImGui::Separator();

			if (ImGui::Button("Ok"))
				ImGui::CloseCurrentPopup();
			ImGui::EndPopup();
		}
	}

	void GUIManager::m_setupShortcuts()
	{
		// PROJECT
		KeyboardShortcuts::Instance().SetCallback("Project.Rebuild", [=]() {
			dynamic_cast<CodeEditorUI*>(Get(ViewID::Code))->SaveAll();

			std::vector<PipelineItem*> passes = m_data->Pipeline.GetList();
			for (PipelineItem*& pass : passes)
				m_data->Renderer.Recompile(pass->Name);
		});
		KeyboardShortcuts::Instance().SetCallback("Project.Save", [=]() {
			Save();
		});
		KeyboardShortcuts::Instance().SetCallback("Project.SaveAs", [=]() {
			SaveAsProject(true);
		});
		KeyboardShortcuts::Instance().SetCallback("Workspace.ToggleToolbar", [=]() {
			Settings::Instance().General.Toolbar ^= 1;
		});
		KeyboardShortcuts::Instance().SetCallback("Project.Open", [=]() {
			bool cont = true;
			if (m_data->Parser.IsProjectModified()) {
				int btnID = this->AreYouSure();
				if (btnID == 2)
					cont = false;
			}

			if (cont)
				ifd::FileDialog::Instance().Open("OpenProjectDlg", "Open SHADERed project", "SHADERed project (*.sprj){.sprj},.*");
		});
		KeyboardShortcuts::Instance().SetCallback("Project.New", [=]() {
			this->ResetWorkspace();
			m_data->Pipeline.New();
		});
		KeyboardShortcuts::Instance().SetCallback("Project.NewRenderTexture", [=]() {
			CreateNewRenderTexture();
		});
		KeyboardShortcuts::Instance().SetCallback("Project.NewBuffer", [=]() {
			CreateNewBuffer();
		});
		KeyboardShortcuts::Instance().SetCallback("Project.NewImage", [=]() {
			CreateNewImage();
		});
		KeyboardShortcuts::Instance().SetCallback("Project.NewImage3D", [=]() {
			CreateNewImage3D();
		});
		KeyboardShortcuts::Instance().SetCallback("Project.NewKeyboardTexture", [=]() {
			CreateKeyboardTexture();
		});
		KeyboardShortcuts::Instance().SetCallback("Project.NewCubeMap", [=]() {
			CreateNewCubemap();
		});
		KeyboardShortcuts::Instance().SetCallback("Project.NewTexture", [=]() {
			CreateNewTexture();
		});
		KeyboardShortcuts::Instance().SetCallback("Project.NewTexture3D", [=]() {
			CreateNewTexture3D();
		});
		KeyboardShortcuts::Instance().SetCallback("Project.NewAudio", [=]() {
			CreateNewAudio();
		});
		KeyboardShortcuts::Instance().SetCallback("Project.NewShaderPass", [=]() {
			CreateNewShaderPass();
		});
		KeyboardShortcuts::Instance().SetCallback("Project.NewComputePass", [=]() {
			CreateNewComputePass();
		});
		KeyboardShortcuts::Instance().SetCallback("Project.NewAudioPass", [=]() {
			CreateNewAudioPass();
		});
		KeyboardShortcuts::Instance().SetCallback("Project.CameraSnapshot", [=]() {
			CreateNewCameraSnapshot();
		});

		// PREVIEW
		KeyboardShortcuts::Instance().SetCallback("Preview.ToggleStatusbar", [=]() {
			Settings::Instance().Preview.StatusBar = !Settings::Instance().Preview.StatusBar;
		});
		KeyboardShortcuts::Instance().SetCallback("Preview.SaveImage", [=]() {
			m_savePreviewPopupOpened = true;
		});

		// WORKSPACE
		KeyboardShortcuts::Instance().SetCallback("Workspace.PerformanceMode", [=]() {
			m_performanceMode = !m_performanceMode;
			m_perfModeFake = m_performanceMode;
		});
		KeyboardShortcuts::Instance().SetCallback("Workspace.FocusMode", [=]() {
			m_focusMode = !m_focusMode;
			m_focusModeTemp = m_focusMode;
		});
		KeyboardShortcuts::Instance().SetCallback("Workspace.HideOutput", [=]() {
			Get(ViewID::Output)->Visible = !Get(ViewID::Output)->Visible;
		});
		KeyboardShortcuts::Instance().SetCallback("Workspace.HideEditor", [=]() {
			Get(ViewID::Code)->Visible = !Get(ViewID::Code)->Visible;
		});
		KeyboardShortcuts::Instance().SetCallback("Workspace.HidePreview", [=]() {
			Get(ViewID::Preview)->Visible = !Get(ViewID::Preview)->Visible;
		});
		KeyboardShortcuts::Instance().SetCallback("Workspace.HidePipeline", [=]() {
			Get(ViewID::Pipeline)->Visible = !Get(ViewID::Pipeline)->Visible;
		});
		KeyboardShortcuts::Instance().SetCallback("Workspace.HidePinned", [=]() {
			Get(ViewID::Pinned)->Visible = !Get(ViewID::Pinned)->Visible;
		});
		KeyboardShortcuts::Instance().SetCallback("Workspace.HideProperties", [=]() {
			Get(ViewID::Properties)->Visible = !Get(ViewID::Properties)->Visible;
		});
		KeyboardShortcuts::Instance().SetCallback("Workspace.Help", [=]() {
			UIHelper::ShellOpen("https://github.com/dfranx/SHADERed/blob/master/TUTORIAL.md");
		});
		KeyboardShortcuts::Instance().SetCallback("Workspace.Options", [=]() {
			m_optionsOpened = true;
			*m_settingsBkp = Settings::Instance();
			m_shortcutsBkp = KeyboardShortcuts::Instance().GetMap();
		});
		KeyboardShortcuts::Instance().SetCallback("Workspace.ChangeThemeUp", [=]() {
			std::vector<std::string> themes = dynamic_cast<OptionsUI*>(m_options)->GetThemeList();

			std::string& theme = Settings::Instance().Theme;

			for (int i = 0; i < themes.size(); i++) {
				if (theme == themes[i]) {
					if (i != 0)
						theme = themes[i - 1];
					else
						theme = themes[themes.size() - 1];
					break;
				}
			}

			dynamic_cast<OptionsUI*>(m_options)->ApplyTheme();
		});
		KeyboardShortcuts::Instance().SetCallback("Workspace.ChangeThemeDown", [=]() {
			std::vector<std::string> themes = dynamic_cast<OptionsUI*>(m_options)->GetThemeList();

			std::string& theme = Settings::Instance().Theme;

			for (int i = 0; i < themes.size(); i++) {
				if (theme == themes[i]) {
					if (i != themes.size() - 1)
						theme = themes[i + 1];
					else
						theme = themes[0];
					break;
				}
			}

			dynamic_cast<OptionsUI*>(m_options)->ApplyTheme();
		});

		KeyboardShortcuts::Instance().SetCallback("Window.Fullscreen", [=]() {
			Uint32 wndFlags = SDL_GetWindowFlags(m_wnd);
			bool isFullscreen = wndFlags & SDL_WINDOW_FULLSCREEN_DESKTOP;
			SDL_SetWindowFullscreen(m_wnd, (!isFullscreen) * SDL_WINDOW_FULLSCREEN_DESKTOP);
		});
	}
	void GUIManager::m_checkChangelog()
	{
		const std::string currentVersionPath = Settings::Instance().ConvertPath("info.dat");

		std::ifstream verReader(currentVersionPath);
		int curVer = 0;
		if (verReader.is_open()) {
			verReader >> curVer;
			verReader.close();
		}

		if (curVer < WebAPI::InternalVersion) {
			m_data->API.FetchChangelog([&](const std::string& str) -> void {
				m_isChangelogOpened = true;

				size_t firstNewline = str.find_first_of('\n');
				if (firstNewline != std::string::npos) {
					m_changelogText = str.substr(firstNewline + 1);
					m_changelogBlogLink = str.substr(0, firstNewline);
				}
			});
		}
	}
	void GUIManager::m_loadTemplateList()
	{
		m_selectedTemplate = "";

		Logger::Get().Log("Loading template list");

		if (std::filesystem::exists("./templates/")) {
			for (const auto& entry : std::filesystem::directory_iterator("./templates/")) {
				std::string file = entry.path().filename().string();
				m_templates.push_back(file);

				if (file == Settings::Instance().General.StartUpTemplate)
					m_selectedTemplate = file;
			}
		}

		if (m_selectedTemplate.empty()) {
			if (!m_templates.empty()) {
				m_selectedTemplate = m_templates[0];
				Logger::Get().Log("Default template set to " + m_selectedTemplate);
			} else {
				m_selectedTemplate = "?empty";
				Logger::Get().Log("No templates found", true);
			}
		}

		m_data->Parser.SetTemplate(m_selectedTemplate);
	}
}