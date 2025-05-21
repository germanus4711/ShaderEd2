//
// Created by André Krützfeldt on 5/12/25.
//

#include "GUIManagerFIles.h"

#include "GUIManager.h"
#include "InterfaceManager.h"
#include "Objects/SystemVariableManager.h"
#include "UI/CodeEditorUI.h"
#include "misc/ImFileDialog.h"
#include "misc/stb_image_resize.h"
#include "misc/stb_image_write.h"

#include <fstream>
namespace ed {

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
				outPixels = static_cast<unsigned char*>(malloc(m_previewSaveSize.x * m_previewSaveSize.y * 4));
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
			auto seqDelta = 1.0f / static_cast<float>(m_savePreviewSeqFPS);

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
					int frameCountDigits = static_cast<int>(log10(static_cast<int>(m_savePreviewSeqDuration / seqDelta))) + 1;
					filename.insert(lastDot == std::string::npos ? filename.size() : lastDot, "%0" + std::to_string(frameCountDigits) + "d"); // frame%d
				}

				SystemVariableManager::Instance().AdvanceTimer(m_savePreviewCachedTime - m_savePreviewTimeDelta);
				SystemVariableManager::Instance().SetTimeDelta(seqDelta);

				stbi_write_png_compression_level = 5; // set to the lowest compression level

				int tCount = static_cast<int>(std::thread::hardware_concurrency());
				tCount = tCount == 0 ? 2 : tCount;

				auto** pixels = new unsigned char*[tCount];
				auto** outPixels = new unsigned char*[tCount];
				auto curFrame = new int[tCount];
				auto needsUpdate = new bool[tCount];
				auto** threadPool = new std::thread*[tCount];
				std::atomic<bool> isOver = false;

				for (int i = 0; i < tCount; i++) {
					curFrame[i] = 0;
					needsUpdate[i] = true;
					pixels[i] = static_cast<unsigned char*>(malloc(actualSizeX * actualSizeY * 4));

					if (sizeMulti != 1)
						outPixels[i] = static_cast<unsigned char*>(malloc(m_previewSaveSize.x * m_previewSaveSize.y * 4));
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
				Logger::Get().Log("No templates available. Setting to default empty project.", false);
			}
		}

		m_data->Parser.SetTemplate(m_selectedTemplate);
	}

	void GUIManager::m_createNewProject()
	{
		auto* codeUI = dynamic_cast<CodeEditorUI*>(Get(ViewID::Code));
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
	void GUIManager::m_addProjectToRecents(const std::string& file)
	{
		const std::string& fileToBeOpened = file;
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

}