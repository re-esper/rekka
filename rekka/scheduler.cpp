#include "scheduler.h"

NS_REK_BEGIN

Scheduler* Scheduler::s_sharedScheduler = nullptr;
Scheduler* Scheduler::getInstance()
{
	if (!s_sharedScheduler) s_sharedScheduler = new (std::nothrow) Scheduler();
	return s_sharedScheduler;
}

Scheduler::Scheduler()
: _timerIdCounter(0)
{
	_frequencyReciprocal = 1000.f / SDL_GetPerformanceFrequency();	
}
Scheduler::~Scheduler()
{
}

int Scheduler::scheduleCallback(JS::HandleValue funcval, float interval, bool repeat)
{
	TimerJSCallback *timer = new TimerJSCallback(funcval, interval, repeat);
	int id = fetchTimerId();
	_timers[id] = timer;
	return id;
}

int Scheduler::scheduleCallback(const std::function<void()>& callback, float interval, bool repeat)
{
	Timer *timer = new Timer(callback, interval, repeat);
	int id = fetchTimerId();
	_timers[id] = timer;
	return id;
}

void Scheduler::cancel(int timerId)
{
	auto itr = _timers.find(timerId);
	if (itr != _timers.end()) {
		itr->second->_deleted = true;
	}
}

void Scheduler::performInMainThread(const std::function<void()> &function)
{
	_performMutex.lock();
	_functionsToPerform.push_back(function);
	_performMutex.unlock();
}

double Scheduler::performanceNow()
{
	return SDL_GetPerformanceCounter() * _frequencyReciprocal;
}

int Scheduler::fetchTimerId()
{
	_timerIdCounter = (_timerIdCounter + 1) & 0x7fffffff;
	return _timerIdCounter;
}

void Scheduler::update(float dt)
{	
	for (auto itr = _timers.begin(); itr != _timers.end(); ) {		
		if (itr->second->_deleted || !itr->second->update(dt)) {
			delete itr->second;
			itr = _timers.erase(itr);
		}
		else {
			itr++;
		}
	}
	if (!_functionsToPerform.empty()) {
		_performMutex.lock();		
		auto functions = _functionsToPerform;
		_functionsToPerform.clear();
		_performMutex.unlock();
		for (const auto &func : functions) {
			func();
		}
	}
}

Timer::Timer(const std::function<void()>& callback, float interval, bool repeat)
: _cooldown(interval), _callback(callback), _interval(interval), _repeat(repeat), _deleted(false)
{
}
bool Timer::update(float dt)
{
	_cooldown -= dt;
	if (_cooldown <= 0) {
		fire();
		if (_repeat) _cooldown += _interval;
		if (!_repeat) return false;
	}
	return true;
}
void Timer::fire()
{
	_callback();
}

TimerJSCallback::TimerJSCallback(JS::HandleValue funcval, float interval, bool repeat)
: Timer(nullptr, interval, repeat)
{
	_callbackJSFunc = funcval;
}
TimerJSCallback::~TimerJSCallback()
{
}
void TimerJSCallback::fire()
{
	auto ctx = ScriptCore::getInstance()->getGlobalContext();
	auto global = ScriptCore::getInstance()->getGlobalObject();
	JS::RootedValue rval(ctx);
	JS_CallFunctionValue(ctx, nullptr, _callbackJSFunc, JS::HandleValueArray::empty(), &rval);
}

NS_REK_END