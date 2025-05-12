#pragma once
#include <SHADERed/UI/CodeEditorUI.h>
#include <SHADERed/UI/Tools/CubemapPreview.h>
#include <SHADERed/UI/Tools/Texture3DPreview.h>
#include <SHADERed/UI/UIView.h>

namespace ed {
	class PixelInspectUI final : public UIView {
	public:
		PixelInspectUI(GUIManager* ui, ed::InterfaceManager* objects, const std::string& name = "", const bool visible = true)
				: UIView(ui, objects, name, visible)
				, m_cacheHasColor(false)
				, m_cacheExists(false)
				, m_cacheColor()
		{
			m_cubePrev.Init(152, 114);
			m_tex3DPrev.Init();
		}

		void OnEvent(const SDL_Event& e) override;
		void Update(float delta) override;

		void RenderPixelInfo(PixelInformation& pixel, float& elementHeight);

		void StartDebugging(TextEditor* editor, const PluginShaderEditor& pluginEditor, PixelInformation* pixel);

	private:
		std::string m_cacheExpression;
		std::string m_cacheValue;
		bool m_cacheHasColor;
		bool m_cacheExists;
		glm::vec4 m_cacheColor;

		CubemapPreview m_cubePrev {};
		Texture3DPreview m_tex3DPrev {};

		std::vector<float> m_pixelHeights;
		std::vector<float> m_suggestionHeights;

		std::vector<std::string> m_editorStack;
	};
}