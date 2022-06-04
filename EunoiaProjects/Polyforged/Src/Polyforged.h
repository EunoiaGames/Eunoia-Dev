#pragma once
#include <Eunoia\Eunoia.h>
#include "Components\Components.h"

using namespace Eunoia;

class EU_PROJ Polyforged : public Application
{
public:
	void Init() override;
	void OnClose() override;
	void RegisterMetadata() override;

};

extern "C" EU_PROJ Application* CreateApplication(ApplicationInfo* appInfo);