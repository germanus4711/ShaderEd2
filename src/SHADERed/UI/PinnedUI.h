#pragma once
#include <SHADERed/UI/Tools/VariableValueEdit.h>
#include <SHADERed/UI/UIView.h>

namespace ed {
	class PinnedUI final : public UIView {
	public:
		PinnedUI(GUIManager* ui, ed::InterfaceManager* objects, const std::string& name = "", const bool visible = true)
				: UIView(ui, objects, name, visible)
				, m_editor(objects)
		{
		}

		void OnEvent(const SDL_Event& e) override;
		void Update(float delta) override;

		void CloseAll() { m_pinnedVars.clear(); }
		std::vector<ed::ShaderVariable*>& GetAll() { return m_pinnedVars; }

		void Add(ed::ShaderVariable* var);
		void Remove(const char* name);
		bool Contains(const char* name) const;

	private:
		std::vector<ed::ShaderVariable*> m_pinnedVars;
		VariableValueEditUI m_editor;
	};
}