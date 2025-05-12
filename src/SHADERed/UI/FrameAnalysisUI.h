#pragma once
#include <SHADERed/UI/UIView.h>
#include <SDL2/SDL_events.h>

namespace ed {
	class FrameAnalysisUI final : public UIView {
	public:
		FrameAnalysisUI(GUIManager* ui, ed::InterfaceManager* objects, const std::string& name = "", bool visible = true)
				: UIView(ui, objects, name, visible)
				, m_histogramR {}
				, m_histogramG {}
				, m_histogramB {}
				, m_histogramRGB {}
		{
			m_histogramType = 0;
		}
		~FrameAnalysisUI() override = default;

		void Process();

		void OnEvent(const SDL_Event& e) override;
		void Update(float delta) override;

	private:
		int m_histogramType;
		float m_histogramR[256], m_histogramG[256], m_histogramB[256], m_histogramRGB[256];

		std::vector<float> m_pixelHeights;
	};
}