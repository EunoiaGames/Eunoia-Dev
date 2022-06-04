#include "Engine.h"
#include "../Utils/Log.h"
#include "Application.h"
#include "../Rendering/MasterRenderer.h"
#include "../Rendering/Asset/AssetManager.h"
#include "Input.h"
#include "../Metadata/Metadata.h"
#include "../ECS/Systems/TransformHierarchy3DSystem.h"
#include "../ECS/Systems/TransformHierarchy2DSystem.h"
#include "../Rendering/GuiManager.h"
#include "../Physics/PhysicsEngine3D.h"

#define EU_WORLD0 0
#define EU_WORLD1 1

namespace Eunoia {

	struct EunoiaWorldData
	{
		Display* display;
		RenderContext* renderContext;
		MasterRenderer* renderer;
		PhysicsEngine3D* physicsEngine;

		b32 created;
		b32 active;
	};

	struct Engine_Data
	{
		List<Application*> applications;
		Application* activeApp;
		b32 running;
		r32 timeInSeconds;
		b32 editorAttached;
		EunoiaWorldData activeWorlds[MAX_EUNOIA_WORLDS];
	};

	static Engine_Data s_Data;

	void Engine::Init(Application* app, const String& title, u32 width, u32 height, RenderAPI api, b32 editorAttached)
	{
		Logger::Init();
		Metadata::Init();
		ECSLoader::Init();
		
		if (!app->GetECS())
		{
			app->InitECS();
		}

		s_Data.editorAttached = editorAttached;
		s_Data.applications.Push(app);
		s_Data.activeApp = app;

		for (u32 i = 0; i < MAX_EUNOIA_WORLDS; i++)
		{
			s_Data.activeWorlds[i].created = false;
			s_Data.activeWorlds[i].active = false;
		}
		
		DisplayInfo displayInfo;
		displayInfo.title = title;
		displayInfo.width = width;
		displayInfo.height = height;

		CreateWorld(displayInfo, api, EUNOIA_WORLD_FLAG_ALL, true, EUNOIA_WORLD_MAIN);
		AssetManager::Init();

		EUInput::InitInput();
		GuiManager::Init();
	}

	void Engine::Start()
	{
		s_Data.running = true;
		s_Data.activeApp->Init();

		r32 dt = 1.0f / 60.0f;
		while (s_Data.running)
		{
			for (u32 i = 0; i < MAX_EUNOIA_WORLDS; i++)
			{
				EunoiaWorldData* world = &s_Data.activeWorlds[i];

				if (!world->active)
					continue;

				if (world->display->CheckForEvent(DISPLAY_EVENT_CLOSE))
				{
					if (i == EUNOIA_WORLD_MAIN)
					{
						Stop();
					}
					else
					{
						DestroyWorld((EunoiaWorld)i);
						break;
					}
				}
			}

			Update(dt);
			Render();

			s_Data.timeInSeconds += dt;
		}
	}

	void Engine::Stop()
	{
		s_Data.running = false;
	}

	EngineApplicationHandle Engine::AddApplication(Application* app, b32 setActive)
	{
		EngineApplicationHandle handle = s_Data.applications.Size();
		s_Data.applications.Push(app);

		if (setActive)
			s_Data.activeApp = app;

		return handle;
	}

	void Engine::SetActiveApplication(EngineApplicationHandle handle)
	{
		s_Data.activeApp = s_Data.applications[handle];
	}

	EunoiaWorld Engine::CreateWorld(const DisplayInfo& displayInfo, RenderAPI renderAPI, EunoiaWorldFlags flags, b32 active, EunoiaWorld setHandle)
	{
		if (setHandle > EUNOIA_WORLD_INVALID)
		{
			EU_LOG_WARN("Engine::CreateWorld() Invalid set handle");
			return EUNOIA_WORLD_INVALID;
		}

		EunoiaWorld worldHandle = EUNOIA_WORLD_INVALID;
		if (setHandle == EUNOIA_WORLD_INVALID)
		{
			for (u32 i = 0; i < MAX_EUNOIA_WORLDS; i++)
			{
				if (!s_Data.activeWorlds[i].created)
				{
					worldHandle = (EunoiaWorld)i;
					break;
				}
			}
		}
		else
		{
			worldHandle = setHandle;
		}

		if (worldHandle == EUNOIA_WORLD_INVALID)
		{
			EU_LOG_ERROR("Engine::CreateWorld() Cannot create anymore worlds");
			return EUNOIA_WORLD_INVALID;
		}

		EunoiaWorldData* world = &s_Data.activeWorlds[worldHandle];

		if (world->created)
		{
			DestroyWorld(worldHandle);
		}

		world->display = Display::CreateDisplay();
		world->display->Create(displayInfo.title, displayInfo.width, displayInfo.height);
		world->renderContext = RenderContext::CreateRenderContext(renderAPI);
		world->renderContext->Init(world->display);
		world->renderer = new MasterRenderer(world->renderContext, world->display);
		world->renderer->Init();
		world->physicsEngine = new PhysicsEngine3D();
		world->physicsEngine->Init();
		world->created = true; 
		world->active = active;

		return worldHandle;
	}

	void Engine::DestroyWorld(EunoiaWorld worldID)
	{
		EunoiaWorldData* world = &s_Data.activeWorlds[worldID];

		if (world->created)
		{
			world->display->Destroy();
			world->physicsEngine->Destroy();
			delete world->display;
			delete world->renderContext;
			delete world->renderer;
			delete world->physicsEngine;
			world->created = false;
			world->active = false;
		}
	}

	EunoiaWorld Engine::RecreateWorld(EunoiaWorld world, EunoiaWorldFlags flags, b32 active)
	{
		DisplayInfo displayInfo;
		displayInfo.title = GetDisplay(world)->GetTitle();
		displayInfo.width = GetDisplay(world)->GetWidth();
		displayInfo.height = GetDisplay(world)->GetHeigth();
		RenderAPI renderAPI = GetRenderContext(world)->GetRenderAPI();

		DestroyWorld(world);
		CreateWorld(displayInfo, renderAPI, flags, active, world);
		return world;
	}

	RenderContext* Engine::GetRenderContext(EunoiaWorld world)
	{
		return s_Data.activeWorlds[world].renderContext;
	}

	MasterRenderer* Engine::GetRenderer(EunoiaWorld world)
	{
		return s_Data.activeWorlds[world].renderer;
	}

	Display* Engine::GetDisplay(EunoiaWorld world)
	{
		return s_Data.activeWorlds[world].display;
	}

	PhysicsEngine3D* Engine::GetPhysicsEngine(EunoiaWorld world)
	{
		return s_Data.activeWorlds[world].physicsEngine;
	}

	b32 Engine::IsWorldAvailable(EunoiaWorld world)
	{
		return s_Data.activeWorlds[world].created;
	}

	b32 Engine::IsWorldActive(EunoiaWorld world)
	{
		return s_Data.activeWorlds[world].active;
	}

	void Engine::SetWorldActive(EunoiaWorld worldID, b32 active)
	{
		EunoiaWorldData* world = &s_Data.activeWorlds[worldID];

		if (!world->created)
		{
			EU_LOG_WARN("Engine::SetWorldActive() world has not been created yet");
			return;
		}

		world->active = active;
	}

	void Engine::SetWorldActiveOpposite(EunoiaWorld worldID)
	{
		EunoiaWorldData* world = &s_Data.activeWorlds[worldID];

		if (!world->created)
		{
			EU_LOG_WARN("Engine::SetWorldActiveOpposite() world has not been created yet");
			return;
		}

		world->active = !world->active;
	}

	r32 Engine::GetTime()
	{
		return s_Data.timeInSeconds;
	}

	b32 Engine::IsEditorAttached()
	{
		return s_Data.editorAttached;
	}

	Application* Engine::GetActiveApplication()
	{
		return s_Data.activeApp;
	}

	Application* Engine::GetApplication(EngineApplicationHandle handle)
	{
		return s_Data.applications[handle];
	}

	void Engine::Update(r32 dt)
	{
		EUInput::BeginInput();
		s_Data.activeApp->BeginECS();
		s_Data.activeApp->UpdateECS(dt);
		s_Data.activeApp->Update(dt);

		for (u32 i = 0; i < s_Data.applications.Size(); i++)
			if (s_Data.applications[i] != s_Data.activeApp)
				s_Data.applications[i]->GetECS()->OnlyUpdateRequiredSystems(dt);

		s_Data.activeApp->GetECS()->PrePhysicsSystems(dt);
		s_Data.activeApp->PrePhysicsSimulation(dt);
		
		for (u32 i = 0; i < MAX_EUNOIA_WORLDS; i++)
		{
			if (!s_Data.activeWorlds[i].active)
				continue;

			s_Data.activeWorlds[i].physicsEngine->StepSimulation(dt);
		}
		
		s_Data.activeApp->GetECS()->PostPhysicsSystems(dt);
		s_Data.activeApp->PostPhysicsSimulation(dt);
		
		EUInput::UpdateInput();
	}

	void Engine::Render()
	{
		for (u32 i = 0; i < MAX_EUNOIA_WORLDS; i++)
		{
			EunoiaWorldData* world = &s_Data.activeWorlds[i];
			if (!world->active)
				continue;

			world->renderContext->BeginFrame();
			world->renderer->BeginFrame();
		}

		
		s_Data.activeApp->RenderECS();
		s_Data.activeApp->Render();
		

		for (u32 i = 0; i < MAX_EUNOIA_WORLDS; i++)
		{
			EunoiaWorldData* world = &s_Data.activeWorlds[i];
			if (!world->active)
				continue;

			world->renderer->EndFrame();
			world->renderer->RenderFrame();
			s_Data.activeApp->EndFrame();

			world->renderContext->Present();
			world->display->Update();
		}
	}

}
