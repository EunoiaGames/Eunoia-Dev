#pragma once

#include "EditorSettings.h"
#include <Eunoia\ECS\ECSTypes.h>
#include <Eunoia\Metadata\MetadataInfo.h>
#include <Eunoia\Rendering\Asset\LoadedMaterial.h>

namespace Eunoia_Editor {

	class EditorGUI
	{
	public:
		static void Init();
		static void Destroy();
		static void UpdateInput();
		static void BeginFrame();
		static void EndFrame();
		static void RenderGUI();

		static void CheckForShortcuts();
	private:
		static void InitImGui();
		static void InitDarkTheme();
		static void InitRenderPass();
		static void InitResources();
	private:
		static void DrawSelectProjectWindow();
		static void DrawMainMenuBar();
		static void DrawCompileWindow();
		static void DrawSceneHierarchyWindow();
		static void DrawSceneHierarchyEntity(Eunoia::EntityID entityID);
		static void DrawPropertiesWindow();
		static void DrawSystemsWindow();
		static void DrawMaterialEditorWindow();
		static void DrawRenderersWindow();
		static void DrawAssetBrowserWindow();
		static void DrawGameWindow();
		static void DoDragDropMaterialTexture(Eunoia::MaterialTextureType texType);
		static void DrawPopupWindows();
		static void DrawMetadata(const Eunoia::MetadataInfo& metadata, const u8* data, const Eunoia::String& idString = "");
		static void DrawMetadataHelper(const Eunoia::MetadataInfo& metadata, const u8* data, const Eunoia::String& idString = "");
		static void DrawMetadataMember(Eunoia::MetadataClass* cls, u32 memberIndex, const u8* data, const Eunoia::String& idString = "", u32 indent = 0);

		static void BlueText(const char* chars);
		static void EnableEngineCamera(r32 enabled);

		static b32 CheckForShortcut(const EditorShortcut& shortcut);
	};

}