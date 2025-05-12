#pragma once
#include <SHADERed/Engine/Timer.h>
#include <SHADERed/Objects/PluginAPI/Plugin.h>

#include <functional>
#include <string>
#include <vector>

namespace ed {
	class NotificationSystem {
	public:
		void Add(int id, const std::string& text, const std::string& buttonText, std::function<void(int, IPlugin1*)> fn, IPlugin1* owner = nullptr);

		[[nodiscard]] bool Has() const { return !m_notifs.empty(); }
		void Render();

	private:
		struct Entry {
			Entry()
			{
				ID = 0;
				Text = ButtonText = "";
				Timer.Restart();
				Owner = nullptr;
				Handler = nullptr;
			}
			Entry(const int id, const std::string& text, const std::string& btn, const std::function<void(int, IPlugin1*)>& fn, IPlugin1* owner = nullptr)
			{
				ID = id;
				Text = text;
				ButtonText = btn;
				Timer.Restart();
				Owner = owner;
				Handler = fn;
			}

			int ID;
			std::string Text;
			std::string ButtonText;
			eng::Timer Timer;
			IPlugin1* Owner;

			std::function<void(int, IPlugin1*)> Handler;
		};
		std::vector<Entry> m_notifs;
	};
}