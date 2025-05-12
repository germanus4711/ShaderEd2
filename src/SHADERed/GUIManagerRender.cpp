//
// Created by André Krützfeldt on 5/10/25.
//

#include "GUIManagerRender.h"
#include "GUIManager.h"
#include "InterfaceManager.h"
#include "Objects/Logger.h"
#include "Objects/Settings.h"
#include "Objects/SystemVariableManager.h"
#include "UI/CodeEditorUI.h"
#include "UI/PinnedUI.h"
#include "UI/PipelineUI.h"
#include "UI/UIHelper.h"
#include "imgui/examples/imgui_impl_opengl3.h"
#include "misc/stb_image.h"
#include "misc/stb_image_resize.h"

#include <iostream>

namespace ed {
	class CodeEditorUI;
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
		ImGui::SetNextWindowSize(ImVec2(static_cast<float>(m_width), static_cast<float>(m_height)));
		if (ImGui::Begin("##splash_screen", 0, ImGuiWindowFlags_NoMove | ImGuiWindowFlags_NoScrollbar | ImGuiWindowFlags_NoTitleBar | ImGuiWindowFlags_NoResize)) {
			ImGui::SetCursorPos(ImVec2(static_cast<float>(m_width - 350) / 2, static_cast<float>(m_height - 324) / 6));
			ImGui::Image(reinterpret_cast<ImTextureID>(static_cast<uintptr_t>(m_splashScreenIcon)), ImVec2(350, 324));

			ImGui::SetCursorPos(ImVec2(static_cast<float>(m_width - 199) / 2, static_cast<float>(m_height) - 202 - 48));
			// ImGui::Image((ImTextureID)m_splashScreenText, ImVec2(199, 46));
			ImGui::Image(reinterpret_cast<ImTextureID>(static_cast<uintptr_t>(m_splashScreenIcon)), ImVec2(199, 46));

			ImGui::SetCursorPos(ImVec2(static_cast<float>(m_width - 200) / 2 + 200, static_cast<float>(m_height) - 202));
			// ImGui::Image((ImTextureID)m_sponsorDigitalOcean, ImVec2(200, 200));
			ImGui::Image(reinterpret_cast<ImTextureID>(static_cast<uintptr_t>(m_splashScreenIcon)), ImVec2(200, 200));

			ImGui::SetCursorPos(ImVec2(static_cast<float>(m_width - 284 * 2) / 2 - 40, static_cast<float>(m_height - 202 + (101 + 32) / 2)));
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
		if (data == nullptr) {
			ed::Logger::Get().Log("Failed to load splash screen logo", true);

			std::cout << "Current working directory: "
		  << std::filesystem::current_path() << std::endl;
			std::cout << "Failed to load splash screen icon";
		} else {
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
			data = nullptr;
		}

		// digital ocean logo
		data = stbi_load(white ? "./data/sponsors/DigitalOcean-white.png" : "./data/sponsors/DigitalOcean-black.png", &width, &height, &orig_format, req_format);
		if (data == nullptr)
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
		if (data == nullptr)
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
						auto* passData = static_cast<pipe::ShaderPass*>(pass->Data);

						// shader tabs for this pass
						if (ImGui::BeginTabBar("ShaderTabs")) {
							auto* codeEditor = dynamic_cast<CodeEditorUI*>(Get(ViewID::Code));

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
						auto* passData = static_cast<pipe::ComputePass*>(pass->Data);

						// shader tabs for this pass
						if (ImGui::BeginTabBar("ShaderTabs")) {
							auto* codeEditor = dynamic_cast<CodeEditorUI*>(Get(ViewID::Code));

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
		auto* codeEditor = dynamic_cast<CodeEditorUI*>(Get(ViewID::Code));

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
								for (const auto& mem : mems) {
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
}