#pragma once
#include <chrono>

namespace ed::eng {
	class Timer {
	public:
		Timer();

		float Restart();
		float GetElapsedTime() const;

		void Pause();
		void Resume();

		bool IsPaused() const { return m_pause; }

	private:
		std::chrono::time_point<std::chrono::system_clock> m_start;
		std::chrono::time_point<std::chrono::system_clock> m_pauseStart;
		float m_pauseTime;
		bool m_pause;
	};
}
