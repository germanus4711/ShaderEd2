#pragma once
#include <SHADERed/UI/UIView.h>

namespace ed {
	class PropertyUI final : public UIView {
	public:
		PropertyUI(GUIManager* ui, ed::InterfaceManager* objects, const std::string& name = "", const bool visible = true)
				: UIView(ui, objects, name, visible)
		{
			m_init();
		}

		void OnEvent(const SDL_Event& e) override;
		void Update(float delta) override;

		void Open(PipelineItem* item);
		void Open(ObjectManagerItem* obj);
		void Close()
		{
			m_current = nullptr;
			m_currentObj = nullptr;
		}
		[[nodiscard]] std::string CurrentItemName() const { return m_current != nullptr ? m_current->Name : (m_currentObj != nullptr ? m_currentObj->Name : ""); }
		[[nodiscard]] bool HasItemSelected() const { return m_current != nullptr || m_currentObj != nullptr; }

		[[nodiscard]] bool IsPipelineItem() const { return m_current != nullptr; }
		[[nodiscard]] bool IsRenderTexture() const { return m_currentObj != nullptr && m_currentObj->Type == ObjectType::RenderTexture; }
		[[nodiscard]] bool IsTexture() const { return m_currentObj != nullptr && m_currentObj->Type == ObjectType::Texture; }
		[[nodiscard]] bool IsImage() const { return m_currentObj != nullptr && m_currentObj->Type == ObjectType::Image; }
		[[nodiscard]] bool IsImage3D() const { return m_currentObj != nullptr && m_currentObj->Type == ObjectType::Image3D; }
		[[nodiscard]] bool IsTexture3D() const { return m_currentObj != nullptr && m_currentObj->Type == ObjectType::Texture3D; }
		[[nodiscard]] bool IsPlugin() const { return m_currentObj != nullptr && m_currentObj->Type == ObjectType::PluginObject; }

	private:
		char m_itemName[2048] {};

		void m_init();

		PipelineItem* m_current {};
		ObjectManagerItem* m_currentObj {};

		char* m_dialogPath {};
		std::string m_dialogShaderType;

		glm::ivec3 m_cachedGroupSize {};
	};
}