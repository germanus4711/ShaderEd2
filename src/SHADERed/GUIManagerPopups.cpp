//
// Created by André Krützfeldt on 5/12/25.
//

#include "GUIManagerPopups.h"
#include "GUIManager.h"
#include "InterfaceManager.h"
#include "Objects/CameraSnapshots.h"
#include "Objects/Export/ExportCPP.h"
#include "Objects/SystemVariableManager.h"
#include "UI/BrowseOnlineUI.h"
#include "UI/CodeEditorUI.h"
#include "UI/CreateItemUI.h"
#include "UI/PropertyUI.h"
#include "UI/UIHelper.h"
#include "imgui/imgui_internal.h"
#include "misc/ImFileDialog.h"

namespace ed {
	class PropertyUI;
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
				auto editor = dynamic_cast<CodeEditorUI*>(Get(ViewID::Code));
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
					static_cast<PropertyUI*>(Get(ViewID::Properties))->Open(m_data->Objects.Get(buf));
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
}