//
// Created by André Krützfeldt on 5/10/25.
//

#include "GUIManagerRender.h"
#include "GUIManager.h"
#include "InterfaceManager.h"
#include "Objects/Logger.h"
#include "Objects/Settings.h"
#include "UI/PipelineUI.h"
#include "UI/UIHelper.h"
#include "imgui/examples/imgui_impl_opengl3.h"
#include "misc/stb_image.h"
#include "misc/stb_image_resize.h"

namespace ed {
	void GUIManager::Render()
	{
		ImDrawData* drawData = ImGui::GetDrawData();
		if (drawData != nullptr) {
			// Save OpenGL state before rendering
			glPushAttrib(GL_ALL_ATTRIB_BITS);
			glPushClientAttrib(GL_CLIENT_ALL_ATTRIB_BITS);

			// Render ImGui draw data
			ImGui_ImplOpenGL3_RenderDrawData(drawData);

			if (ImGui::GetIO().ConfigFlags & ImGuiConfigFlags_ViewportsEnable) {
				ImGui::UpdatePlatformWindows();
				ImGui::RenderPlatformWindowsDefault();
			}

			// Restore state after rendering
			glPopClientAttrib();
			glPopAttrib();
		}
	}
	void GUIManager::m_splashScreenRender()
	{
		SDL_GetWindowSize(m_wnd, &m_width, &m_height);
		float wndRounding = ImGui::GetStyle().WindowRounding;
		float wndBorder = ImGui::GetStyle().WindowBorderSize;
		ImGui::GetStyle().WindowRounding = 0.0f;
		ImGui::GetStyle().WindowBorderSize = 0.0f;

		ImGui::SetNextWindowPos(ImVec2(0, 0), ImGuiCond_Always);
		ImGui::SetNextWindowSize(ImVec2(m_width, m_height));
		if (ImGui::Begin("##splash_screen", 0, ImGuiWindowFlags_NoMove | ImGuiWindowFlags_NoScrollbar | ImGuiWindowFlags_NoTitleBar | ImGuiWindowFlags_NoResize)) {
			ImGui::SetCursorPos(ImVec2((m_width - 350) / 2, (m_height - 324) / 6));
			ImGui::Image(reinterpret_cast<ImTextureID>(static_cast<uintptr_t>(m_splashScreenIcon)), ImVec2(350, 324));

			ImGui::SetCursorPos(ImVec2((m_width - 199) / 2, m_height - 202 - 48));
			// ImGui::Image((ImTextureID)m_splashScreenText, ImVec2(199, 46));
			ImGui::Image(reinterpret_cast<ImTextureID>(static_cast<uintptr_t>(m_splashScreenIcon)), ImVec2(199, 46));

			ImGui::SetCursorPos(ImVec2((m_width - 200) / 2 + 200, m_height - 202));
			// ImGui::Image((ImTextureID)m_sponsorDigitalOcean, ImVec2(200, 200));
			ImGui::Image(reinterpret_cast<ImTextureID>(static_cast<uintptr_t>(m_splashScreenIcon)), ImVec2(200, 200));

			ImGui::SetCursorPos(ImVec2((m_width - 284 * 2) / 2 - 40, m_height - 202 + (101 + 32) / 2));
			// ImGui::Image((ImTextureID)m_sponsorEmbark, ImVec2(284, 64));
			ImGui::Image(reinterpret_cast<ImTextureID>(static_cast<uintptr_t>(m_splashScreenIcon)), ImVec2(284, 64));
		}
		ImGui::End();

		ImGui::GetStyle().WindowRounding = wndRounding;
		ImGui::GetStyle().WindowBorderSize = wndBorder;

		// render ImGUI
		ImGui::Render();

		if (m_splashScreenTimer.GetElapsedTime() > 0.85f && m_splashScreenLoaded)
			m_splashScreen = false;

		// rather than moving everything on the other thread rn, load on this one but first render the splash screen (maybe TODO)
		if (m_splashScreenFrame < 5)
			m_splashScreenFrame++;
		else if (!m_splashScreenLoaded) {
			// check for updates
			if (Settings::Instance().General.CheckUpdates) {
				m_data->API.CheckForApplicationUpdates([&]() {
					m_notifs.Add(0, "A new version of SHADERed is available!", "UPDATE", [](int id, IPlugin1* pl) {
						UIHelper::ShellOpen("https://shadered.org/download.php");
					});
				});
			}

			// check for tips
			if (Settings::Instance().General.Tips) {
				m_data->API.FetchTips([&](int n, int i, const std::string& title, const std::string& text) {
					m_tipCount = n;
					m_tipIndex = i;
					m_tipTitle = title + " (tip " + std::to_string(m_tipIndex + 1) + "/" + std::to_string(m_tipCount) + ")";
					m_tipText = text;
					m_tipOpened = true;
				});
			}

			// check the changelog
			m_checkChangelog();

			// load plugins
			m_data->Plugins.Init(m_data, this);

			// once we loaded plugins, we can get the complete list of all languages
			m_createUI->UpdateLanguageList();
			dynamic_cast<PipelineUI*>(Get(ViewID::Pipeline))->UpdateLanguageList();

			// check for plugin updates
			if (Settings::Instance().General.CheckPluginUpdates) {
				const auto& pluginList = m_data->Plugins.GetPluginList();
				for (const auto& plugin : pluginList) {
					if (int version = m_data->API.GetPluginVersion(plugin); version > m_data->Plugins.GetPluginVersion(plugin)) {
						m_notifs.Add(1, "A new version of " + plugin + " plugin is available!", "UPDATE", [&](int id, IPlugin1* pl) {
							UIHelper::ShellOpen("https://shadered.org/plugin?id=" + plugin);
						});
					}
				}
			}

			// load a startup project (if given through arguments)
			if (m_cmdArguments && !m_cmdArguments->ProjectFile.empty()) {
				ed::Logger::Get().Log("Opening a file provided through argument " + m_cmdArguments->ProjectFile);
				this->Open(m_cmdArguments->ProjectFile);
			}

			m_splashScreenLoaded = true;
			m_isIncompatPluginsOpened = !m_data->Plugins.GetIncompatiblePlugins().empty();

			if (m_isIncompatPluginsOpened) {
				const std::vector<std::string>& incompat = m_data->Plugins.GetIncompatiblePlugins();
				std::vector<std::string>& notLoaded = Settings::Instance().Plugins.NotLoaded;

				for (const auto& plg : incompat)
					if (std::count(notLoaded.begin(), notLoaded.end(), plg) == 0)
						notLoaded.push_back(plg);
			}
		}
	}
	void GUIManager::m_splashScreenLoad()
	{
		Logger::Get().Log("Setting up the splash screen");

		stbi_set_flip_vertically_on_load(0);

		// logo
		int req_format = STBI_rgb_alpha;
		int width, height, orig_format;
		unsigned char* data = stbi_load("./data/splash_screen_logo.png", &width, &height, &orig_format, req_format);
		if (data == nullptr)
			ed::Logger::Get().Log("Failed to load splash screen icon", true);
		else {
			glGenTextures(1, &m_splashScreenIcon);
			glBindTexture(GL_TEXTURE_2D, m_splashScreenIcon);
			glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST_MIPMAP_LINEAR);
			glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
			glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, width, height, 0, GL_RGBA, GL_UNSIGNED_BYTE, data);
			glGenerateMipmap(GL_TEXTURE_2D);
			glBindTexture(GL_TEXTURE_2D, 0);
			stbi_image_free(data);
		}

		// check if we should use black or white text
		bool white = true;
		ImVec4 wndBg = ImGui::GetStyle().Colors[ImGuiCol_WindowBg];
		float avgWndBg = (wndBg.x + wndBg.y + wndBg.z) / 3.0f;
		if (avgWndBg >= 0.5f)
			white = false;

		// text
		data = stbi_load(white ? "./data/splash_screen_text_white.png" : "./data/splash_screen_text_black.png", &width, &height, &orig_format, req_format);
		if (data == nullptr)
			ed::Logger::Get().Log("Failed to load splash screen icon", true);
		else {
			glGenTextures(1, &m_splashScreenText);
			glBindTexture(GL_TEXTURE_2D, m_splashScreenText);
			glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST_MIPMAP_LINEAR);
			glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
			glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, width, height, 0, GL_RGBA, GL_UNSIGNED_BYTE, data);
			glGenerateMipmap(GL_TEXTURE_2D);
			glBindTexture(GL_TEXTURE_2D, 0);
			stbi_image_free(data);
			data = NULL;
		}

		// digital ocean logo
		data = stbi_load(white ? "./data/sponsors/DigitalOcean-white.png" : "./data/sponsors/DigitalOcean-black.png", &width, &height, &orig_format, req_format);
		if (data == NULL)
			ed::Logger::Get().Log("Failed to load DigitalOcean logo", true);
		else {
			glGenTextures(1, &m_sponsorDigitalOcean);
			glBindTexture(GL_TEXTURE_2D, m_sponsorDigitalOcean);
			glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST_MIPMAP_LINEAR);
			glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
			glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, width, height, 0, GL_RGBA, GL_UNSIGNED_BYTE, data);
			glGenerateMipmap(GL_TEXTURE_2D);
			glBindTexture(GL_TEXTURE_2D, 0);
			stbi_image_free(data);
		}

		// TODO: write some system so that I do not have to do this manually
		// embark studios
		data = stbi_load(white ? "./data/sponsors/Embark-white.png" : "./data/sponsors/Embark-black.png", &width, &height, &orig_format, req_format);
		if (data == NULL)
			ed::Logger::Get().Log("Failed to load Embark logo", true);
		else {
			auto outEmbark = static_cast<unsigned char*>(malloc(284 * 64 * 4));
			stbir_resize_uint8(data, width, height, width * 4, outEmbark, 284, 64, 284 * 4, 4);
			width = 284;
			height = 64;

			glGenTextures(1, &m_sponsorEmbark);
			glBindTexture(GL_TEXTURE_2D, m_sponsorEmbark);
			glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST_MIPMAP_LINEAR);
			glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
			glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, width, height, 0, GL_RGBA, GL_UNSIGNED_BYTE, outEmbark);
			glGenerateMipmap(GL_TEXTURE_2D);
			glBindTexture(GL_TEXTURE_2D, 0);
			stbi_image_free(data);
			free(outEmbark);
		}

		stbi_set_flip_vertically_on_load(1);

		m_splashScreenFrame = 0;
		m_splashScreenLoaded = false;
		m_splashScreenTimer.Restart();
	}
}