#ifndef CORE_COMPONENT_I_H
#define CORE_COMPONENT_I_H

#include <Windows.h>
#include <memory>

class CoreEngine;

class ICoreComponent
{
public:
	virtual ~ICoreComponent() = default;
	void SetCoreEnv(CoreEngine* engine) { core_engine_ = engine; }

protected:
	CoreEngine* core_engine_ = nullptr;
};

#endif