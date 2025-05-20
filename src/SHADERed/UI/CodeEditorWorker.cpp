//
// Created by André Krützfeldt on 5/20/25.
//

#include "CodeEditorWorker.h"

#include "CodeEditorUI.h"

namespace ed {

	void CodeEditorUI::StopThreads()
	{
		m_trackerRunning = false;
		if (m_trackThread)
			m_trackThread->detach();
	}

	void CodeEditorUI::CleanupTrackingThread()
	{
		if (m_trackThread != nullptr && m_trackThread->joinable())
			m_trackThread->join();
		delete m_trackThread;
		m_trackThread = nullptr;
	}

	void CodeEditorUI::SetTrackFileChanges(bool track)
	{
		if (m_trackFileChanges == track)
			return;

		m_trackFileChanges = track;
		m_trackerRunning = false;

		if (track) {
			Logger::Get().Log("Starting to track file changes...");

			CleanupTrackingThread();

			// start
			m_trackerRunning = true;
			m_trackThread = new std::thread(&CodeEditorUI::m_trackWorker, this);
		} else {
			Logger::Get().Log("Stopping file change tracking...");
			CleanupTrackingThread();
		}
	}

	void CodeEditorUI::m_trackWorker()
	{
		std::string curProject = m_data->Parser.GetOpenedFile();

		std::vector<PipelineItem*> passes = m_data->Pipeline.GetList();
		std::vector<bool> gsUsed(passes.size());
		std::vector<bool> tsUsed(passes.size());

		std::vector<std::string> allFiles;	// list of all files we care for
		std::vector<std::string> allPasses; // list of shader pass names that correspond to the file name
		std::vector<std::string> paths;		// list of all paths that we should have "notifications turned on"

		m_trackUpdatesNeeded = 0;

#if defined(__APPLE__)
		// TODO: implementation for macos (cant test)
#elif defined(__linux__) || defined(__unix__)

		int bufLength, bufIndex = 0;
		int notifyEngine = inotify_init1(IN_NONBLOCK);
		char buffer[EVENT_BUF_LEN];

		std::vector<int> notifyIDs;

		if (notifyEngine < 0) {
			// TODO: log!
			return;
		}

		// make sure that read() doesn't block on exit
		int flags = fcntl(notifyEngine, F_GETFL, 0);
		flags = (flags | O_NONBLOCK);
		fcntl(notifyEngine, F_SETFL, flags);

#elif defined(_WIN32)
		// variables for storing all the handles
		std::vector<HANDLE> events(paths.size());
		std::vector<HANDLE> hDirs(paths.size());
		std::vector<OVERLAPPED> pOverlap(paths.size());

		// buffer data
		const int bufferLen = 2048;
		char buffer[bufferLen];
		DWORD bytesReturned;
		char filename[SHADERED_MAX_PATH];
#endif

		// run this loop until we close the thread
		while (m_trackerRunning) {
			std::this_thread::sleep_for(std::chrono::milliseconds(25));

			for (auto&& j : m_trackedNeedsUpdate)
				j = false;

			// check if user added/changed shader paths
			std::vector<PipelineItem*> nPasses = m_data->Pipeline.GetList();
			bool needsUpdate = false;
			for (auto& pass : nPasses) {
				if (pass->Type == PipelineItem::ItemType::ShaderPass) {
					auto* data = static_cast<pipe::ShaderPass*>(pass->Data);

					bool foundVS = false, foundPS = false, foundGS = false, foundTCS = false, foundTES = false;

					std::string vsPath(m_data->Parser.GetProjectPath(data->VSPath));
					std::string psPath(m_data->Parser.GetProjectPath(data->PSPath));

					for (auto& f : allFiles) {
						if (f == vsPath) {
							foundVS = true;
							if (foundPS) break;
						} else if (f == psPath) {
							foundPS = true;
							if (foundVS) break;
						}
					}

					if (data->GSUsed) {
						std::string gsPath(m_data->Parser.GetProjectPath(data->GSPath));
						for (auto& f : allFiles)
							if (f == gsPath) {
								foundGS = true;
								break;
							}
					} else
						foundGS = true;

					if (data->TSUsed) {
						std::string tcsPath(m_data->Parser.GetProjectPath(data->TCSPath));
						std::string tesPath(m_data->Parser.GetProjectPath(data->TESPath));
						for (auto& f : allFiles) {
							if (f == tesPath) {
								foundTES = true;
								break;
							}
							if (f == tcsPath) {
								foundTCS = true;
								break;
							}
						}
					} else {
						foundTES = true;
						foundTCS = true;
					}

					if (!foundGS || !foundVS || !foundPS || !foundTES || !foundTCS) {
						needsUpdate = true;
						break;
					}
				} else if (pass->Type == PipelineItem::ItemType::ComputePass) {
					auto* data = static_cast<pipe::ComputePass*>(pass->Data);

					bool found = false;

					std::string path(m_data->Parser.GetProjectPath(data->Path));

					for (auto& f : allFiles)
						if (f == path)
							found = true;

					if (!found) {
						needsUpdate = true;
						break;
					}
				} else if (pass->Type == PipelineItem::ItemType::AudioPass) {
					auto* data = static_cast<pipe::AudioPass*>(pass->Data);

					bool found = false;

					std::string path(m_data->Parser.GetProjectPath(data->Path));

					for (auto& f : allFiles)
						if (f == path)
							found = true;

					if (!found) {
						needsUpdate = true;
						break;
					}
				} else if (pass->Type == PipelineItem::ItemType::PluginItem) {
					auto* data = static_cast<pipe::PluginItemData*>(pass->Data);

					if (data->Owner->ShaderFilePath_HasChanged()) {
						needsUpdate = true;
						data->Owner->ShaderFilePath_Update();
					}
				}
			}

			for (int i = 0; i < gsUsed.size() && i < nPasses.size() && i < tsUsed.size(); i++) {
				if (nPasses[i]->Type == PipelineItem::ItemType::ShaderPass) {
					bool newGSUsed = static_cast<pipe::ShaderPass*>(nPasses[i]->Data)->GSUsed;
					bool newTSUsed = static_cast<pipe::ShaderPass*>(nPasses[i]->Data)->TSUsed;
					if (gsUsed[i] != newGSUsed) {
						gsUsed[i] = newGSUsed;
						needsUpdate = true;
					}
					if (tsUsed[i] != newTSUsed) {
						tsUsed[i] = newTSUsed;
						needsUpdate = true;
					}
				}
			}

			// update our file collection if needed
			if (needsUpdate || nPasses.size() != passes.size() || curProject != m_data->Parser.GetOpenedFile() || paths.empty()) {
#if defined(__APPLE__)
				// TODO: implementation for macos
#elif defined(__linux__) || defined(__unix__)
				for (int i = 0; i < notifyIDs.size(); i++)
					inotify_rm_watch(notifyEngine, notifyIDs[i]);
				notifyIDs.clear();
#elif defined(_WIN32)
				for (int i = 0; i < paths.size(); i++) {
					CloseHandle(hDirs[i]);
					CloseHandle(events[i]);
				}
				events.clear();
				hDirs.clear();
#endif

				allFiles.clear();
				allPasses.clear();
				paths.clear();
				curProject = m_data->Parser.GetOpenedFile();

				// get all paths to all shaders
				passes = nPasses;
				gsUsed.resize(passes.size());
				tsUsed.resize(passes.size());
				m_trackedNeedsUpdate.resize(passes.size());
				for (const auto& pass : passes) {
					if (pass->Type == PipelineItem::ItemType::ShaderPass) {
						auto* data = static_cast<pipe::ShaderPass*>(pass->Data);

						std::string vsPath(m_data->Parser.GetProjectPath(data->VSPath));
						std::string psPath(m_data->Parser.GetProjectPath(data->PSPath));

						allFiles.push_back(vsPath);
						paths.push_back(vsPath.substr(0, vsPath.find_last_of("/\\") + 1));
						allPasses.emplace_back(pass->Name);

						allFiles.push_back(psPath);
						paths.push_back(psPath.substr(0, psPath.find_last_of("/\\") + 1));
						allPasses.emplace_back(pass->Name);

						// geometry shader
						if (data->GSUsed) {
							std::string gsPath(m_data->Parser.GetProjectPath(data->GSPath));

							allFiles.push_back(gsPath);
							paths.push_back(gsPath.substr(0, gsPath.find_last_of("/\\") + 1));
							allPasses.emplace_back(pass->Name);
						}

						// tessellation shader
						if (data->TSUsed) {
							// tessellation control shader
							std::string tcsPath(m_data->Parser.GetProjectPath(data->TCSPath));
							allFiles.push_back(tcsPath);
							paths.push_back(tcsPath.substr(0, tcsPath.find_last_of("/\\") + 1));
							allPasses.emplace_back(pass->Name);

							// tessellation evaluation shader
							std::string tesPath(m_data->Parser.GetProjectPath(data->TESPath));
							allFiles.push_back(tesPath);
							paths.push_back(tesPath.substr(0, tesPath.find_last_of("/\\") + 1));
							allPasses.emplace_back(pass->Name);
						}
					} else if (pass->Type == PipelineItem::ItemType::ComputePass) {
						auto* data = static_cast<pipe::ComputePass*>(pass->Data);

						std::string path(m_data->Parser.GetProjectPath(data->Path));

						allFiles.push_back(path);
						paths.push_back(path.substr(0, path.find_last_of("/\\") + 1));
						allPasses.emplace_back(pass->Name);
					} else if (pass->Type == PipelineItem::ItemType::AudioPass) {
						auto data = static_cast<pipe::AudioPass*>(pass->Data);

						std::string path(m_data->Parser.GetProjectPath(data->Path));

						allFiles.push_back(path);
						paths.push_back(path.substr(0, path.find_last_of("/\\") + 1));
						allPasses.emplace_back(pass->Name);
					} else if (pass->Type == PipelineItem::ItemType::PluginItem) {
						auto* data = static_cast<pipe::PluginItemData*>(pass->Data);

						int count = data->Owner->ShaderFilePath_GetCount();

						for (int i = 0; i < count; i++) {
							std::string path(m_data->Parser.GetProjectPath(data->Owner->ShaderFilePath_Get(i)));

							allFiles.push_back(path);
							paths.push_back(path.substr(0, path.find_last_of("/\\") + 1));
							allPasses.emplace_back(pass->Name);
						}
					}
				}

				// delete directories that appear twice or that are subdirectories
				{
					std::vector<bool> toDelete(paths.size(), false);

					for (int i = 0; i < paths.size(); i++) {
						if (toDelete[i]) continue;

						for (int j = 0; j < paths.size(); j++) {
							if (j == i || toDelete[j]) continue;

							if (paths[j].find(paths[i]) != std::string::npos)
								toDelete[j] = true;
						}
					}

					for (int i = 0; i < paths.size(); i++)
						if (toDelete[i]) {
							paths.erase(paths.begin() + i);
							toDelete.erase(toDelete.begin() + i);
							i--;
						}
				}

#if defined(__APPLE__)
				// TODO: implementation for macos
#elif defined(__linux__) || defined(__unix__)
				// create HANDLE to all tracked directories
				notifyIDs.resize(paths.size());
				for (int i = 0; i < paths.size(); i++)
					notifyIDs[i] = inotify_add_watch(notifyEngine, paths[i].c_str(), IN_MODIFY);
#elif defined(_WIN32)
				events.resize(paths.size());
				hDirs.resize(paths.size());
				pOverlap.resize(paths.size());

				// create HANDLE to all tracked directories
				for (int i = 0; i < paths.size(); i++) {
					hDirs[i] = CreateFileA(paths[i].c_str(), GENERIC_READ | FILE_LIST_DIRECTORY,
						FILE_SHARE_READ | FILE_SHARE_WRITE | FILE_SHARE_DELETE,
						NULL, OPEN_EXISTING, FILE_FLAG_BACKUP_SEMANTICS | FILE_FLAG_OVERLAPPED,
						NULL);

					if (hDirs[i] == INVALID_HANDLE_VALUE)
						return;

					pOverlap[i].OffsetHigh = 0;
					pOverlap[i].hEvent = CreateEvent(NULL, TRUE, FALSE, NULL);

					events[i] = pOverlap[i].hEvent;
				}
#endif
			}

			if (paths.empty()) {
				std::this_thread::sleep_for(std::chrono::milliseconds(500));
				continue;
			}

#if defined(__APPLE__)
			// TODO: implementation for macos
		}
#elif defined(__linux__) || defined(__unix__)
			fd_set rfds;
			int eCount = select(notifyEngine + 1, &rfds, NULL, NULL, NULL);

			if (eCount <= 0) continue;

			// check for changes
			bufLength = read(notifyEngine, buffer, EVENT_BUF_LEN);
			if (bufLength < 0) { /* TODO: error! */
			}

			// read all events
			while (bufIndex < bufLength) {
				struct inotify_event* event = (struct inotify_event*)&buffer[bufIndex];
				if (event->len) {
					if (event->mask & IN_MODIFY) {
						if (event->mask & IN_ISDIR) { /* it is a directory - do nothing */
						} else {
							// check if its our shader and push it on the update queue if it is
							char filename[SHADERED_MAX_PATH];
							strcpy(filename, event->name);

							int pathIndex = 0;
							for (int i = 0; i < notifyIDs.size(); i++) {
								if (event->wd == notifyIDs[i]) {
									pathIndex = i;
									break;
								}
							}

							std::lock_guard<std::mutex> lock(m_trackFilesMutex);
							std::string updatedFile(paths[pathIndex] + filename);

							for (int i = 0; i < allFiles.size(); i++)
								if (allFiles[i] == updatedFile) {
									// did we modify this file through "Compile" option?
									bool shouldBeIgnored = false;
									for (int j = 0; j < m_trackIgnore.size(); j++)
										if (m_trackIgnore[j] == allPasses[i]) {
											shouldBeIgnored = true;
											m_trackIgnore.erase(m_trackIgnore.begin() + j);
											break;
										}

									if (!shouldBeIgnored) {
										m_trackUpdatesNeeded++;

										for (int j = 0; j < passes.size(); j++)
											if (allPasses[i] == passes[j]->Name)
												m_trackedNeedsUpdate[j] = true;
									}
								}
						}
					}
				}
				bufIndex += EVENT_SIZE + event->len;
			}
			bufIndex = 0;
		}

		for (int i = 0; i < notifyIDs.size(); i++)
			inotify_rm_watch(notifyEngine, notifyIDs[i]);
		close(notifyEngine);
#elif defined(_WIN32)
			// notification data
			FILE_NOTIFY_INFORMATION* notif;
			int bufferOffset;

			// check for changes
			for (int i = 0; i < paths.size(); i++) {
				ReadDirectoryChangesW(
					hDirs[i],
					&buffer,
					bufferLen * sizeof(char),
					TRUE,
					FILE_NOTIFY_CHANGE_SIZE | FILE_NOTIFY_CHANGE_LAST_WRITE,
					&bytesReturned,
					&pOverlap[i],
					NULL);
			}

			DWORD dwWaitStatus = WaitForMultipleObjects(paths.size(), events.data(), false, 1000);

			// check if we got any info
			if (dwWaitStatus != WAIT_TIMEOUT) {
				bufferOffset = 0;
				do {
					// get notification data
					notif = (FILE_NOTIFY_INFORMATION*)((char*)buffer + bufferOffset);
					strcpy_s(filename, "");
					int filenamelen = WideCharToMultiByte(CP_ACP, 0, notif->FileName, notif->FileNameLength / 2, filename, sizeof(filename), NULL, NULL);
					if (filenamelen == 0)
						continue;
					filename[notif->FileNameLength / 2] = 0;

					if (notif->Action == FILE_ACTION_MODIFIED) {
						std::lock_guard<std::mutex> lock(m_trackFilesMutex);

						std::string updatedFile(paths[dwWaitStatus - WAIT_OBJECT_0] + std::string(filename));

						for (int i = 0; i < allFiles.size(); i++)
							if (allFiles[i] == updatedFile) {
								// did we modify this file through "Compile" option?
								bool shouldBeIgnored = false;
								for (int j = 0; j < m_trackIgnore.size(); j++)
									if (m_trackIgnore[j] == allPasses[i]) {
										shouldBeIgnored = true;
										m_trackIgnore.erase(m_trackIgnore.begin() + j);
										break;
									}

								if (!shouldBeIgnored) {
									m_trackUpdatesNeeded++;

									for (int j = 0; j < passes.size(); j++)
										if (allPasses[i] == passes[j]->Name)
											m_trackedNeedsUpdate[j] = true;
								}
							}
					}

					bufferOffset += notif->NextEntryOffset;
				} while (notif->NextEntryOffset);
			}
		}
		for (int i = 0; i < hDirs.size(); i++)
			CloseHandle(hDirs[i]);
		for (int i = 0; i < events.size(); i++)
			CloseHandle(events[i]);
		events.clear();
		hDirs.clear();
#endif
	}
}
