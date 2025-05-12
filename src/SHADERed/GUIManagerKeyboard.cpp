//
// Created by André Krützfeldt on 5/12/25.
//

#include "GUIManagerKeyboard.h"

#include "GUIManager.h"
#include "InterfaceManager.h"
#include "Objects/Settings.h"
#include "UI/CodeEditorUI.h"
#include "UI/OptionsUI.h"
#include "UI/UIHelper.h"
#include "UI/UIView.h"
#include "misc/ImFileDialog.h"
namespace ed {

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

}
