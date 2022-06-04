#pragma once

#include "../Rendering/RenderContext.h"
#include "../Rendering/MasterRenderer.h"
#include "../Math/Math.h"
#include "../Rendering/DisplayInfo.h"
#include "../Physics/PhysicsEngine3D.h"

#define EU_MAIN_APPLICATION 0

namespace Eunoia {

	typedef u32 EngineApplicationHandle;

	EU_REFLECT()
	enum EunoiaWorldFlag_T
	{
		EUNOIA_WORLD_FLAG_RENDER_2D = 1,
		EUNOIA_WORLD_FLAG_RENDER_3D = 2,
		EUNOIA_WORLD_FLAG_DISPLAY = 4,
		EUNOIA_WORLD_FLAG_PHYSICS_ENGINE_3D = 8,
		EUNOIA_WORLD_FLAG_RENDER = EUNOIA_WORLD_FLAG_RENDER_2D | EUNOIA_WORLD_FLAG_RENDER_3D,
		EUNOIA_WORLD_FLAG_RENDER_DISPLAY = EUNOIA_WORLD_FLAG_RENDER | EUNOIA_WORLD_FLAG_DISPLAY,
		EUNOIA_WORLD_FLAG_ALL = EUNOIA_WORLD_FLAG_RENDER_DISPLAY | EUNOIA_WORLD_FLAG_PHYSICS_ENGINE_3D
	};

	typedef u32 EunoiaWorldFlags;

	EU_REFLECT()
	enum EunoiaWorld
	{
		EUNOIA_WORLD_0,
		EUNOIA_WORLD_1,
		EUNOIA_WORLD_2,

		MAX_EUNOIA_WORLDS = 1,
		EUNOIA_WORLD_INVALID,
		EUNOIA_WORLD_MAIN = EUNOIA_WORLD_0
	};

	class Application;
	class EU_API Engine
	{
	public:
		static void Init(Application* app, const String& title, u32 width, u32 height, RenderAPI api, b32 editorAttached = true);
		static void Start();
		static void Stop();

		static EngineApplicationHandle AddApplication(Application* app, b32 setActive = false);
		static void SetActiveApplication(EngineApplicationHandle handle);

		static EunoiaWorld CreateWorld(const DisplayInfo& displayInfo, RenderAPI renderAPI, EunoiaWorldFlags flags = EUNOIA_WORLD_FLAG_ALL, b32 active = false, EunoiaWorld setHandle = EUNOIA_WORLD_INVALID);
		static void DestroyWorld(EunoiaWorld world);
		static EunoiaWorld RecreateWorld(EunoiaWorld world, EunoiaWorldFlags flags = EUNOIA_WORLD_FLAG_ALL, b32 active = false);

		static RenderContext* GetRenderContext(EunoiaWorld world = EUNOIA_WORLD_MAIN);
		static MasterRenderer* GetRenderer(EunoiaWorld world = EUNOIA_WORLD_MAIN);
		static Display* GetDisplay(EunoiaWorld world = EUNOIA_WORLD_MAIN);
		static PhysicsEngine3D* GetPhysicsEngine(EunoiaWorld world = EUNOIA_WORLD_MAIN);

		static b32 IsWorldAvailable(EunoiaWorld world);
		static b32 IsWorldActive(EunoiaWorld world);
		static void SetWorldActive(EunoiaWorld world, b32 active);
		static void SetWorldActiveOpposite(EunoiaWorld world);

		static r32 GetTime();
		static b32 IsEditorAttached();

		static Application* GetActiveApplication();
		static Application* GetApplication(EngineApplicationHandle handle);
	private:
		static void Update(r32 dt);
		static void Render();
	};

}
