//
// Created by André Krützfeldt on 5/12/25.
//

#include "GUIManagerCLI.h"

#include "GUIManager.h"

namespace ed {
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
			default:;
			}

			m_previewSavePath = options.RenderPath;
			m_previewSaveSize = glm::ivec2(options.RenderWidth, options.RenderHeight);
			m_savePreviewSeq = options.RenderSequence;
			m_savePreviewSeqDuration = options.RenderSequenceDuration;
			m_savePreviewSeqFPS = options.RenderSequenceFPS;
		}
	}
}