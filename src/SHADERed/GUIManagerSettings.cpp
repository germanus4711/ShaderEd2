//
// Created by André Krützfeldt on 5/12/25.
//

#include "GUIManagerSettings.h"

#include "GUIManager.h"
#include "UI/OptionsUI.h"
#include "UI/UIView.h"

#include <fstream>
namespace ed {

	void GUIManager::SaveSettings() const
	{
		std::ofstream data("data/gui.dat");

		for (auto& view : m_views)
			data.put((char)view->Visible);
		for (auto& dview : m_debugViews)
			data.put((char)dview->Visible);

		data.close();
	}
	void GUIManager::LoadSettings()
	{
		std::ifstream data("data/gui.dat");

		if (data.is_open()) {
			for (auto& view : m_views)
				view->Visible = data.get();
			for (auto& dview : m_debugViews)
				dview->Visible = data.get();

			data.close();
		}

		Get(ViewID::Code)->Visible = false;

		dynamic_cast<OptionsUI*>(m_options)->ApplyTheme();
	}

}
