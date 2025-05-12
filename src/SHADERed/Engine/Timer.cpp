#include <SHADERed/Engine/Timer.h>

namespace ed::eng {
	Timer::Timer()
	{
		m_start = std::chrono::system_clock::now();
		m_pause = false;
		m_pauseTime = 0;
	}
	float Timer::Restart()
	{
		std::chrono::time_point<std::chrono::system_clock> end = std::chrono::system_clock::now();

		float ret = 0;
		if (m_pause)
			ret = static_cast<float>(std::chrono::duration_cast<std::chrono::microseconds>(m_pauseStart - m_start).count());
		else
			ret = static_cast<float>(std::chrono::duration_cast<std::chrono::microseconds>(end - m_start).count());
		ret = ret / 1000000.0f - m_pauseTime;

		m_start = end;
		m_pauseStart = end;
		m_pauseTime = 0;

		return ret;
	}
	float Timer::GetElapsedTime() const
	{
		std::chrono::time_point<std::chrono::system_clock> end = std::chrono::system_clock::now();

		float ret = 0;
		if (m_pause)
			ret = static_cast<float>(std::chrono::duration_cast<std::chrono::microseconds>(m_pauseStart - m_start).count());
		else
			ret = static_cast<float>(std::chrono::duration_cast<std::chrono::microseconds>(end - m_start).count());

		return ret / 1000000.0f - m_pauseTime;
	}
	void Timer::Pause()
	{
		if (!m_pause) {
			m_pause = true;
			m_pauseStart = std::chrono::system_clock::now();
		}
	}
	void Timer::Resume()
	{
		if (m_pause) {
			std::chrono::time_point<std::chrono::system_clock> end = std::chrono::system_clock::now();

			m_pauseTime += static_cast<float>(std::chrono::duration_cast<std::chrono::microseconds>(end - m_pauseStart).count()) / 1000000.0f;
			m_pause = false;
		}
	}
}
