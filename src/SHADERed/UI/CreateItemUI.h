#pragma once
#include <SHADERed/Objects/PipelineItem.h>
#include <SHADERed/Objects/ShaderVariable.h>
#include <SHADERed/UI/UIView.h>

namespace ed {
	class CreateItemUI final : public UIView {
	public:
		CreateItemUI(GUIManager* ui, InterfaceManager* objects, const std::string& name = "", bool visible = false);

		void OnEvent(const SDL_Event& e) override;
		void Update(float delta) override;

		void SetOwner(const char* shaderPass);
		void SetType(PipelineItem::ItemType type);
		bool Create();
		void Reset();

		void UpdateLanguageList();

	private:
		void m_createFile(const std::string& filename) const;
		void m_updateItemFilenames();

		std::vector<std::string> m_groups;
		int m_selectedGroup;

		bool m_errorOccured;

		char m_owner[PIPELINE_ITEM_NAME_LENGTH] {};
		PipelineItem m_item;

		char* m_dialogPath;
		bool* m_dialogShaderAuto;
		std::string m_dialogShaderType;

		int m_fileAutoExtensionSel;
		std::string m_fileAutoLanguages;
		std::vector<std::string> m_fileAutoExtensions;
		bool m_isShaderFileAuto[5] {};
	};
}