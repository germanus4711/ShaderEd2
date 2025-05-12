#pragma once
#include <ImGuiColorTextEdit/TextEditor.h>
#include <SHADERed/InterfaceManager.h>
#include <SHADERed/Objects/SPIRVParser.h>
#include <SHADERed/UI/UIView.h>

namespace ed {
	class StatsPage final : public UIView {
	public:
		StatsPage(GUIManager* ui, ed::InterfaceManager* objects, const std::string& name = "", const bool visible = true)
				: UIView(ui, objects, name, visible)
				, m_info()
		{
			m_spirv.SetReadOnly(true);
		}

		void OnEvent(const SDL_Event& e) override;
		void Update(float delta) override;

		void Refresh(PipelineItem* item, ShaderStage stage);

		inline void ClearHighlights() { m_spirv.ClearHighlightedLines(); }
		void Highlight(int line);

	private:
		SPIRVParser m_info;
		TextEditor m_spirv;

		void m_parse(const std::string& spv);

		std::vector<unsigned int> m_spv;
		std::unordered_map<int, std::vector<int>> m_lineMap;
	};
}