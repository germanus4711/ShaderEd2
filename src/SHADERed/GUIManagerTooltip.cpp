//
// Created by André Krützfeldt on 5/12/25.
//

#include "GUIManagerTooltip.h"

#include "GUIManager.h"
#include "InterfaceManager.h"
#include "Objects/Settings.h"
#include "UI/CodeEditorUI.h"
#include "UI/Icons.h"
#include "imgui/imgui_internal.h"
#include "misc/ImFileDialog.h"
namespace ed {
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
		ImGui::Begin("##toolbar", nullptr, window_flags);
		ImGui::PopStyleVar(3);

		float bHeight = static_cast<float>(TOOLBAR_HEIGHT) / 2 + ImGui::GetStyle().FramePadding.y * 2;

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
				if (this->AreYouSure() == 2)
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
}