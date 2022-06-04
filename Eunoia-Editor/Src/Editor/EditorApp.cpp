#include "EditorApp.h"
#include "EditorGUI.h"
#include "ProjectManager.h"
#include "EditorSettings.h"
#include <Eunoia\Core\Engine.h>
#include <Eunoia\ECS\Systems\TransformHierarchy2DSystem.h>
#include <Eunoia\ECS\Systems\TransformHierarchy3DSystem.h>
#include <Eunoia\ECS\Systems\ViewProjectionSystem.h>
#include <Eunoia\ECS\Components\CameraComponent.h>
#include <Eunoia\ECS\Components\Transform3DComponet.h>
#include <Eunoia\ECS\Components\KeyboardMovement3DComponent.h>
#include <Eunoia\ECS\Systems\KeyboardMovement3DSystem.h>
#include <Eunoia\ECS\Components\KeyboardLookAround3DComponent.h>
#include <Eunoia\ECS\Systems\KeyboardLookAround3DSystem.h>

namespace Eunoia_Editor
{
	void EditorApp::Init()
	{
		EditorSettings::LoadEditorSettingsFromFile();
		EditorGUI::Init();
		ProjectManager::Init();

		m_ECS->CreateScene("Scene", true);
		m_ECS->CreateSystem<Eunoia::ViewProjectionSystem>();
		m_ECS->CreateSystem<Eunoia::KeyboardMovement3DSystem>();
		m_ECS->CreateSystem<Eunoia::KeyboardLookAround3DSystem>();
		Eunoia::EntityID engineCamera = m_ECS->CreateEntity("Camera");
		m_ECS->CreateComponent<Eunoia::CameraComponent>(engineCamera, 70.0f);
		m_ECS->CreateComponent<Eunoia::Transform3DComponent>(engineCamera);
		m_ECS->CreateComponent<Eunoia::KeyboardMovement3DComponent>(engineCamera, Eunoia::KeyboardMovement3DComponent());
		m_ECS->CreateComponent<Eunoia::KeyboardLookAround3DComponent>(engineCamera, Eunoia::KeyboardLookAround3DComponent(Eunoia::EU_KEY_UP, Eunoia::EU_KEY_DOWN,
			Eunoia::EU_KEY_LEFT, Eunoia::EU_KEY_RIGHT, 20.0f));

		EU_LOG_INFO("Initialized Editor");
	}

	void EditorApp::OnClose()
	{
		EditorSettings::SaveEditorSettingsToFile();
	}

	void EditorApp::Update(r32 dt)
	{
		EunoiaProject* project = ProjectManager::GetProject();
		if (project->loaded)
		{
			EditorGUI::CheckForShortcuts();
			if (project->stepApplication)
			{
				project->application->Update(dt);
				project->application->BeginECS();
				project->application->UpdateECS(dt);
			}
			else
			{
				project->application->GetECS()->OnlyUpdateRequiredSystems(dt);
			}
		}
	}

	void EditorApp::Render()
	{
		EditorGUI::UpdateInput();
		EunoiaProject* project = ProjectManager::GetProject();
		if (project->loaded)
		{
			project->application->Render();
			project->application->RenderECS();
		}
	}

	void EditorApp::EndFrame()
	{
		EditorGUI::BeginFrame();
		EditorGUI::RenderGUI();
		EditorGUI::EndFrame();
	}

	void EditorApp::PrePhysicsSimulation(r32 dt)
	{
		EunoiaProject* project = ProjectManager::GetProject();
		if (project->loaded && project->stepApplication)
			project->application->GetECS()->PrePhysicsSystems(dt);
	}

	void EditorApp::PostPhysicsSimulation(r32 dt)
	{
		EunoiaProject* project = ProjectManager::GetProject();
		if (project->loaded && project->stepApplication)
			project->application->GetECS()->PostPhysicsSystems(dt);
	}
}

int main()
{
	Eunoia::Application* app = new Eunoia_Editor::EditorApp();
	Eunoia::Engine::Init(app, "Eunoia Editor - Select Project", 1280, 720, Eunoia::RENDER_API_VULKAN);
	Eunoia::Engine::Start();

	delete app;
}
