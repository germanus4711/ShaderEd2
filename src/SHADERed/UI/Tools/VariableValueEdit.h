#pragma once
#include <SHADERed/InterfaceManager.h>
#include <SHADERed/Objects/ShaderVariable.h>

namespace ed {
	class VariableValueEditUI {
	public:
		explicit VariableValueEditUI(ed::InterfaceManager* data);

		void Open(ed::ShaderVariable* var) { m_var = var; }
		void Close() { m_var = nullptr; }
		[[nodiscard]] ShaderVariable* GetVariable() const { return m_var; }

		bool Update();

	private:
		bool m_drawRegular() const;
		bool m_drawFunction() const;

		InterfaceManager* m_data;
		ShaderVariable* m_var;
	};
}