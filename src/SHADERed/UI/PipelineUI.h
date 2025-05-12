#pragma once
#include <SHADERed/UI/CreateItemUI.h>
#include <SHADERed/UI/Tools/VariableValueEdit.h>
#include <SHADERed/UI/UIView.h>

namespace ed {
	class PipelineUI final : public UIView {
	public:
		PipelineUI(GUIManager* ui, ed::InterfaceManager* objects, const std::string& name = "", bool visible = true)
				: UIView(ui, objects, name, visible)
				, m_isChangeVarsOpened(false)
				, m_isCreateViewOpened(false)
				, m_computeLang()
				, m_isVarManagerOpened(false)
				, m_modalItem(nullptr)
				, m_createUI(ui, objects)
				, m_valueEdit(objects)
		{
			m_itemMenuOpened = false;
			m_isMacroManagerOpened = false;
			m_isInpLayoutManagerOpened = false;
			m_isResourceManagerOpened = false;
			m_isComputeDebugOpen = false;
			m_isConfirmDeleteOpened = false;
		}

		void OnEvent(const SDL_Event& e) override;
		void Update(float delta) override;

		void UpdateLanguageList() { m_createUI.UpdateLanguageList(); }

		void Reset() { m_expandList.clear(); }

		void DeleteItem(PipelineItem* item) const;

		std::vector<pipe::ShaderPass*>& GetCollapsedItems() { return m_expandList; }
		void Collapse(pipe::ShaderPass* data) { m_expandList.push_back(data); }

	private:
		// for popups
		bool m_isMacroManagerOpened;
		bool m_isComputeDebugOpen;
		bool m_isVarManagerOpened;
		bool m_isInpLayoutManagerOpened;
		bool m_isResourceManagerOpened;
		bool m_isChangeVarsOpened;
		bool m_isCreateViewOpened;
		bool m_isConfirmDeleteOpened;
		bool m_itemMenuOpened;

		int m_localSizeX {}, m_localSizeY {}, m_localSizeZ {}; // local size defined in compute shader
		int m_groupsX {}, m_groupsY {}, m_groupsZ {};		   // numbers sent to glDispatch
		unsigned int m_thread[3] {};						   // user's selection
		ShaderLanguage m_computeLang;

		std::vector<pipe::ShaderPass*> m_expandList; // list of shader pass items that are collapsed

		CreateItemUI m_createUI;
		PipelineItem* m_modalItem; // item that we are editing in a popup modal
		void m_closePopup();

		// for variable value editor
		ed::VariableValueEditUI m_valueEdit;

		// various small components
		void m_renderItemUpDown(pipe::PluginItemData* owner, std::vector<PipelineItem*>& items, int index) const;
		bool m_renderItemContext(std::vector<PipelineItem*>& items, int index);
		void m_renderVariableManagerUI();
		void m_renderInputLayoutManagerUI() const;
		void m_renderResourceManagerUI() const;
		void m_renderChangeVariablesUI();
		void m_renderMacroManagerUI();

		static void m_tooltip(const std::string& text);
		void m_renderVarFlags(ShaderVariable* var, char flags) const;

		// adding items to pipeline UI
		void m_addShaderPass(PipelineItem* data);
		void m_addComputePass(PipelineItem* data) const;
		void m_addAudioPass(PipelineItem* data) const;
		void m_addPluginItem(PipelineItem* data) const;
		void m_addItem(PipelineItem* name) const;

		// handle object drop
		void m_handleObjectDrop(PipelineItem* item, ObjectManagerItem* object) const;
	};
}