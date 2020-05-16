#pragma once

#include "rekka.h"
#include "script_core.h"

#include <map>
#include <functional>
#include <mutex>


NS_REK_BEGIN

class Timer;
class Scheduler {
private:
	static Scheduler* s_sharedScheduler;
public:
	Scheduler();
	~Scheduler();
	static Scheduler* getInstance();

	int scheduleCallback(JS::HandleValue funcval, float interval, bool repeat);
	int scheduleCallback(const std::function<void()>& callback, float interval, bool repeat);
	void cancel(int timerId);
	void performInMainThread(const std::function<void()> &function);

	void update(float dt);
	double performanceNow();
private:
	int _timerIdCounter;
	int fetchTimerId();

	std::map<int, Timer*> _timers;
	
	std::vector<std::function<void()>> _functionsToPerform;
	std::mutex _performMutex;
	double _frequencyReciprocal;
};


class Timer {
public:
	Timer(const std::function<void()>& callback, float interval, bool repeat);
	virtual ~Timer() {}
	bool update(float dt);
	bool _deleted;
private:	
	virtual void fire();
	std::function<void()> _callback;
	float _interval;
	float _cooldown;
	bool _repeat;	
};

class TimerJSCallback : public Timer {
public:
	TimerJSCallback(JS::HandleValue funcval, float interval, bool repeat);
	virtual ~TimerJSCallback();
private:
	virtual void fire();
	JS::PersistentRootedValue _callbackJSFunc;
};

NS_REK_END