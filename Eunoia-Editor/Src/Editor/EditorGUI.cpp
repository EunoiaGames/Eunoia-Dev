#include "EditorGUI.h"

#include <Eunoia\Core\Engine.h>
#include <Eunoia\Core\Input.h>
#include <Eunoia\Math\Math.h>
#include <Eunoia\Rendering\RenderContext.h>
#include <Eunoia\Rendering\Asset\AssetManager.h>
#include <Eunoia\Rendering\Renderer2D.h>
#include <Eunoia\Rendering\Renderer3D.h>
#include <Eunoia\Rendering\MasterRenderer.h>
#include <Eunoia\Rendering\Asset\MaterialLoader.h>
#include <Eunoia\Utils\Log.h>
#include <Eunoia\ECS\Components\Components.h>
#include <Eunoia\ECS\Events\RigidBodyTransformModifiedEvent.h>
#include <Eunoia\Physics\PhysicsEngine3D.h>
#include <Eunoia\ECS\Systems\ViewProjectionSystem.h>

#include "../Vendor/imgui/imgui.h"
#include "../Vendor/imgui/imgui_internal.h"
#include "../Vendor/imgui/ImGuizmo.h"

#ifdef EU_PLATFORM_WINDOWS
#include <Eunoia\Platform\Win32\DisplayWin32.h>
#include "../Vendor/imgui/imgui_impl_win32.h"
#endif

#include "ProjectManager.h"

IMGUI_IMPL_API LRESULT  ImGui_ImplWin32_WndProcHandler(HWND hWnd, UINT msg, WPARAM wParam, LPARAM lParam);

#define EU_PROJECT_APPLICATION 1

namespace Eunoia_Editor {

	using namespace Eunoia;

	enum EditorContentTab
	{
		CONTENT_TAB_INSPECTOR,
		CONTENT_TAB_RENDERERS,
		NUM_CONTENT_TABS
	};

	enum AssetModify
	{
		ASSET_MODIFY_RENAME,
		ASSET_MODIFY_DELETE,
		ASSET_MODIFY_NONE
	};

	enum MaterialEditMode
	{
		MATERIAL_EDIT_MATERIAL,
		MATERIAL_EDIT_MATERIAL_MODIFIER,
		MATERIAL_EDIT_NONE
	};

	enum TransformWidgetMode
	{
		TRANSFORM_WIDGET_TRANSLATE,
		TRANSFORM_WIDGET_ROTATE,
		TRANSFORM_WIDGET_SCALE,
		NUM_TRANSFORM_WIDGET_MODES
	};

	struct Editor_Data
	{
		ImGuiContext* imguiContext;
		ImFontConfig fontConfig;

		ShaderID guiShader;
		ShaderBufferID guiPerFrameBuffer;
		RenderPassID renderPass;
		TextureID fontTexture;
		TextureID defaultTexture;
		TextureID gameWindowTexture;
		BufferID vertexBuffer;
		BufferID indexBuffer;

		TextureID directoryIcon;
		TextureID fileIcon;

		String openPopup;
		char tempInputText[64];
		char sceneNameInputText[32];
		char entityInputName[32];
		char materialNameInput[32];
		char fileNameInput[32];
		u32 selectedScene;
		EntityID selectedEntity;
		s32 selectedComponent;
		s32 selectedSystem;
		s32 selectedCollisionShape;
		b32 waitForKeySelection;
		Key* keyValueToSet;
		v3* colorPickerColor;

		TransformWidgetMode transformWidgetMode;
		MaterialEditMode materialEditMode;

		AssetModify dirModify;
		AssetModify fileModify;
		u32 assetModifyIndex;

		b32 drawMaterialEditorWindow;
		b32 drawMaterialModifierWindow;
		MaterialID materialToEdit;
		MaterialModifierID modifierToEdit;

		EUDirectory* currentDirectory;
		EUDirectory* prevDirectory;

		EngineCamera engineCamera;
	};

	static void DisplayEvent_UpdateImGuiInput(const Eunoia::DisplayEvent& eventInfo, void* userPtr)
	{
		if (eventInfo.type == Eunoia::DISPLAY_EVENT_KEY && eventInfo.inputType == Eunoia::DISPLAY_INPUT_EVENT_PRESS)
		{
			ImGuiIO& io = ImGui::GetIO();
			if (eventInfo.input == Eunoia::EU_KEY_BACKSPACE)
				io.KeysDown[io.KeyMap[ImGuiKey_Backspace]] = true;
			else if (eventInfo.input == Eunoia::EU_KEY_ENTER)
				io.KeysDown[io.KeyMap[ImGuiKey_Enter]] = true;
			else
				io.AddInputCharacter(Eunoia::EUInput::GetChar((Eunoia::Key)eventInfo.input));

			Editor_Data* data = (Editor_Data*)userPtr;
			if (data->waitForKeySelection)
			{
				data->waitForKeySelection = false;
				*data->keyValueToSet = (Key)eventInfo.input;
			}
		}
		if (eventInfo.type == Eunoia::DISPLAY_EVENT_KEY && eventInfo.inputType == Eunoia::DISPLAY_INPUT_EVENT_RELEASE)
		{
			ImGuiIO& io = ImGui::GetIO();
			if (eventInfo.input == Eunoia::EU_KEY_BACKSPACE)
				io.KeysDown[io.KeyMap[ImGuiKey_Backspace]] = false;
			else if (eventInfo.input == Eunoia::EU_KEY_ENTER)
				io.KeysDown[io.KeyMap[ImGuiKey_Enter]] = false;

		}
	}

	static Editor_Data s_Data;

	void EditorGUI::Init()
	{
		s_Data.openPopup = "";
		s_Data.selectedScene = EU_ECS_INVALID_SCENE_ID;
		s_Data.selectedEntity = EU_ECS_INVALID_ENTITY_ID;
		s_Data.selectedComponent = -1;
		s_Data.selectedSystem = -1;
		s_Data.gameWindowTexture = Engine::GetRenderer()->GetFinalOutput();
		s_Data.dirModify = ASSET_MODIFY_NONE;
		s_Data.fileModify = ASSET_MODIFY_NONE;
		s_Data.drawMaterialEditorWindow = false;
		s_Data.materialToEdit = EU_INVALID_MATERIAL_ID;
		s_Data.transformWidgetMode = TRANSFORM_WIDGET_TRANSLATE;
		s_Data.materialEditMode = MATERIAL_EDIT_NONE;
		s_Data.selectedCollisionShape = -1;
		s_Data.assetModifyIndex = 0;
		s_Data.engineCamera = ENGINE_CAMERA_3D;

		Engine::GetDisplay()->AddDisplayEventCallback(DisplayEvent_UpdateImGuiInput, &s_Data);

		InitImGui();
		InitRenderPass();
		InitResources();
	}

	void EditorGUI::InitImGui()
	{
		s_Data.imguiContext = ImGui::CreateContext();
		ImGui::SetCurrentContext(s_Data.imguiContext);

		ImGuizmo::SetImGuiContext(s_Data.imguiContext);
		ImGuizmo::Enable(true);

		ImGuiIO& io = ImGui::GetIO();
		InitDarkTheme();
		io.FontDefault = io.Fonts->AddFontFromFileTTF("Res/Fonts/OpenSans/static/OpenSans/OpenSans-Regular.ttf", 18.0f);
		io.KeyMap[ImGuiKey_Backspace] = EU_KEY_BACKSPACE;
		io.DisplaySize.x = Engine::GetDisplay()->GetWidth();
		io.DisplaySize.y = Engine::GetDisplay()->GetHeigth();
		io.Framerate = 1.0f / 60.0f;
		io.ConfigFlags |= ImGuiConfigFlags_DockingEnable;
		io.ConfigFlags |= ImGuiConfigFlags_ViewportsEnable;

#ifdef EU_PLATFORM_WINDOWS
		((DisplayWin32*)Engine::GetDisplay())->AddWindowProc(ImGui_ImplWin32_WndProcHandler);
#endif

		io.Fonts->AddFontDefault();

		u8* pixels;
		s32 width, height;
		io.Fonts->GetTexDataAsRGBA32(&pixels, &width, &height);
		s_Data.fontTexture = Engine::GetRenderContext()->CreateTexture2D(pixels, width, height, Eunoia::TEXTURE_FORMAT_RGBA8_UNORM);
		io.Fonts->SetTexID(s_Data.fontTexture);


#ifdef EU_PLATFORM_WINDOWS
		if (!ImGui_ImplWin32_Init(((Eunoia::DisplayWin32*)Eunoia::Engine::GetDisplay())->GetHandle()))
		{
			EU_LOG_FATAL("Could not initialize ImGuiWin32");
			return;
		}

		EU_LOG_TRACE("Initialized ImGui for Win32");
#endif
	}

	void EditorGUI::InitDarkTheme()
	{
		ImGuiStyle& style = ImGui::GetStyle();

		style.Colors[ImGuiCol_WindowBg] = ImVec4(0.1f, 0.105f, 0.11f, 1.0f);

		style.Colors[ImGuiCol_Header] = ImVec4(0.2f, 0.205f, 0.21f, 1.0f);
		style.Colors[ImGuiCol_HeaderHovered] = ImVec4(0.3f, 0.305f, 0.31f, 1.0f);
		style.Colors[ImGuiCol_HeaderActive] = ImVec4(0.15f, 0.1505f, 0.151f, 1.0f);
		
		style.Colors[ImGuiCol_Button] = ImVec4(0.2f, 0.205f, 0.21f, 1.0f);
		style.Colors[ImGuiCol_ButtonHovered] = ImVec4(0.3f, 0.305f, 0.31f, 1.0f);
		style.Colors[ImGuiCol_ButtonActive] = ImVec4(0.15f, 0.1505f, 0.151f, 1.0f);

		style.Colors[ImGuiCol_FrameBg] = ImVec4(0.2f, 0.205f, 0.21f, 1.0f);
		style.Colors[ImGuiCol_FrameBgHovered] = ImVec4(0.3f, 0.305f, 0.31f, 1.0f);
		style.Colors[ImGuiCol_FrameBgActive] = ImVec4(0.15f, 0.1505f, 0.151f, 1.0f);

		style.Colors[ImGuiCol_Tab] = ImVec4(0.15f, 0.1505f, 0.151f, 1.0f);
		style.Colors[ImGuiCol_TabHovered] = ImVec4(0.38f, 0.3805f, 0.381f, 1.0f);
		style.Colors[ImGuiCol_TabActive] = ImVec4(0.28f, 0.2805f, 0.281f, 1.0f);
		style.Colors[ImGuiCol_TabUnfocused] = ImVec4(0.15f, 0.1505f, 0.151f, 1.0f);
		style.Colors[ImGuiCol_TabUnfocusedActive] = ImVec4(0.2f, 0.205f, 0.21f, 1.0f);

		style.Colors[ImGuiCol_TitleBg] = ImVec4(0.15f, 0.1505f, 0.151f, 1.0f);
		style.Colors[ImGuiCol_TitleBgActive] = ImVec4(0.15f, 0.1505f, 0.151f, 1.0f);
		style.Colors[ImGuiCol_TitleBgCollapsed] = ImVec4(0.15f, 0.1505f, 0.151f, 1.0f);
	}

	void EditorGUI::InitRenderPass()
	{
		RenderContext* rc = Engine::GetRenderContext();

		RenderPass renderPass;
		Framebuffer* framebuffer = &renderPass.framebuffer;
		framebuffer->useSwapchainSize = true;
		framebuffer->numAttachments = 1;
		framebuffer->attachments[0].format = TEXTURE_FORMAT_SWAPCHAIN_FORMAT;
		framebuffer->attachments[0].isClearAttachment = true;
		framebuffer->attachments[0].isSamplerAttachment = false;
		framebuffer->attachments[0].isStoreAttachment = true;
		framebuffer->attachments[0].isSubpassInputAttachment = false;
		framebuffer->attachments[0].isSwapchainAttachment = true;
		framebuffer->attachments[0].nonClearAttachmentPreserve = false;
		framebuffer->attachments[0].memoryTransferSrc = false;

		Subpass subpass;
		subpass.useDepthStencilAttachment = false;
		subpass.depthStencilAttachment = 0;
		subpass.numReadAttachments = 0;
		subpass.numWriteAttachments = 1;
		subpass.writeAttachments[0] = 0;

		s_Data.guiShader = rc->LoadShader("EditorGUI");

		GraphicsPipeline pipeline{};
		pipeline.shader = s_Data.guiShader;
		pipeline.topology = PRIMITIVE_TOPOLOGY_TRIANGLE_LIST;
		pipeline.viewportState.useFramebufferSizeForScissor = pipeline.viewportState.useFramebufferSizeForViewport = true;
		pipeline.viewportState.viewport.x = pipeline.viewportState.viewport.y =
		pipeline.viewportState.scissor.x = pipeline.viewportState.scissor.y = 0;
		pipeline.vertexInputState.vertexSize = EU_VERTEX_SIZE_AUTO;
		pipeline.vertexInputState.numAttributes = 3;
		pipeline.vertexInputState.attributes[0].location = 0;
		pipeline.vertexInputState.attributes[0].type = VERTEX_ATTRIBUTE_FLOAT2;
		pipeline.vertexInputState.attributes[0].name = "POSITION";
		pipeline.vertexInputState.attributes[1].location = 1;
		pipeline.vertexInputState.attributes[1].type = VERTEX_ATTRIBUTE_FLOAT2;
		pipeline.vertexInputState.attributes[1].name = "TEXCOORD";
		pipeline.vertexInputState.attributes[2].location = 2;
		pipeline.vertexInputState.attributes[2].type = VERTEX_ATTRIBUTE_U32;
		pipeline.vertexInputState.attributes[2].name = "POSITION";
		pipeline.rasterizationState.cullMode = CULL_MODE_BACK;
		pipeline.rasterizationState.depthClampEnabled = false;
		pipeline.rasterizationState.discard = false;
		pipeline.rasterizationState.frontFace = FRONT_FACE_CW;
		pipeline.rasterizationState.polygonMode = POLYGON_MODE_FILL;
		pipeline.numBlendStates = 1;
		pipeline.blendStates[0].blendEnabled = true;
		pipeline.blendStates[0].color.dstFactor = BLEND_FACTOR_ONE_MINUS_SRC_ALPHA;
		pipeline.blendStates[0].color.srcFactor = BLEND_FACTOR_SRC_ALPHA;
		pipeline.blendStates[0].color.operation = BLEND_OPERATION_ADD;
		pipeline.blendStates[0].alpha = pipeline.blendStates[0].color;
		pipeline.blendStates[0].alpha.operation = BLEND_OPERATION_SUB;
		pipeline.depthStencilState.depthTestEnabled = false;
		pipeline.depthStencilState.depthWriteEnabled = false;
		pipeline.depthStencilState.stencilTestEnabled = false;
		pipeline.depthStencilState.depthCompare = COMPARE_OPERATION_LESS;
		pipeline.dynamicStates[DYNAMIC_STATE_SCISSOR] = true;

		MaxTextureGroupBinds maxBinds;
		maxBinds.set = 1;
		maxBinds.maxBinds = 128;

		pipeline.maxTextureGroupBinds.Push(maxBinds);

		subpass.pipelines.Push(pipeline);
		pipeline.blendStates[0].blendEnabled = false;
		subpass.pipelines.Push(pipeline);

		renderPass.subpasses.Push(subpass);

		s_Data.renderPass = rc->CreateRenderPass(renderPass);

		s_Data.guiPerFrameBuffer = rc->CreateShaderBuffer(SHADER_BUFFER_UNIFORM_BUFFER, sizeof(v2), 1);
		rc->AttachShaderBufferToRenderPass(s_Data.renderPass, s_Data.guiPerFrameBuffer, 0, 0, 0, 0);
		rc->AttachShaderBufferToRenderPass(s_Data.renderPass, s_Data.guiPerFrameBuffer, 0, 1, 0, 0);
	}

	void EditorGUI::InitResources()
	{
		RenderContext* rc = Engine::GetRenderContext();

		const u32 MAX_VERTICES = 5000;
		const u32 MAX_INDICES = MAX_VERTICES * 6;

		u8 white[4] = { 255, 255, 255, 255 };

		s_Data.vertexBuffer = rc->CreateBuffer(BUFFER_TYPE_VERTEX, BUFFER_USAGE_DYNAMIC, EU_NULL, sizeof(ImDrawVert) * MAX_VERTICES);
		s_Data.indexBuffer = rc->CreateBuffer(BUFFER_TYPE_INDEX, BUFFER_USAGE_DYNAMIC, EU_NULL, sizeof(ImDrawIdx) * MAX_INDICES);
		s_Data.defaultTexture = rc->CreateTexture2D(white, 1, 1, TEXTURE_FORMAT_RGBA8_UNORM);
		s_Data.directoryIcon = rc->CreateTexture2D("Res/Editor/DirectoryIcon.eutex");
		s_Data.fileIcon = rc->CreateTexture2D("Res/Editor/FileIcon.eutex");
	}

	void EditorGUI::Destroy()
	{
#ifdef EU_PLATFORM_WINDOWS
		ImGui_ImplWin32_Shutdown();
#endif
		ImGui::DestroyContext();
	}

	void EditorGUI::UpdateInput()
	{
#ifdef EU_PLATFORM_WINDOWS
		ImGui_ImplWin32_NewFrame();
#endif
	}

	void EditorGUI::BeginFrame()
	{
		ImGui::SetCurrentContext(s_Data.imguiContext);
		ImGui::NewFrame();
		ImGuizmo::BeginFrame();
	}

	void EditorGUI::EndFrame()
	{
		Eunoia::RenderContext* rc = Eunoia::Engine::GetRenderContext();
		ImGui::Render();
		ImDrawData* drawData = ImGui::GetDrawData();

		int fbWidth = (int)(drawData->DisplaySize.x * drawData->FramebufferScale.x);
		int fbHeight = (int)(drawData->DisplaySize.y * drawData->FramebufferScale.y);
		if (fbWidth <= 0 || fbHeight <= 0 || drawData->TotalVtxCount == 0)
			return;

		ImDrawVert* vertices = (ImDrawVert*)rc->MapBuffer(s_Data.vertexBuffer);
		ImDrawIdx* indices = (ImDrawIdx*)rc->MapBuffer(s_Data.indexBuffer);

		for (u32 i = 0; i < drawData->CmdListsCount; i++)
		{
			ImDrawList* drawList = drawData->CmdLists[i];
			memcpy(vertices, &drawList->VtxBuffer[0], drawList->VtxBuffer.Size * sizeof(ImDrawVert));
			memcpy(indices, &drawList->IdxBuffer[0], drawList->IdxBuffer.Size * sizeof(ImDrawIdx));
			vertices += drawList->VtxBuffer.Size;
			indices += drawList->IdxBuffer.Size;
		}

		Eunoia::RenderPassBeginInfo beginInfo;
		beginInfo.initialPipeline = 0;
		beginInfo.renderPass = s_Data.renderPass;
		beginInfo.numClearValues = 1;
		beginInfo.clearValues[0].color = { 0.0, 0.0, 0.0, 0.0 };

		rc->BeginRenderPass(beginInfo);

		u32 width = Eunoia::Engine::GetDisplay()->GetWidth();
		u32 height = Eunoia::Engine::GetDisplay()->GetHeigth();
		Eunoia::v2 screenSize(width, height);
		rc->UpdateShaderBuffer(s_Data.guiPerFrameBuffer, &screenSize, sizeof(v2));
		
		Eunoia::RenderCommand command;
		command.vertexBuffer = s_Data.vertexBuffer;
		command.indexBuffer = s_Data.indexBuffer;
		command.indexType = Eunoia::INDEX_TYPE_U16;

		ImVec2 clipOff = drawData->DisplayPos;
		ImVec2 clipScale = drawData->FramebufferScale;

		u32 vertexOffset = 0;
		u32 indexOffset = 0;
		for (u32 i = 0; i < drawData->CmdListsCount; i++)
		{
			ImDrawList* drawList = drawData->CmdLists[i];


			for (u32 j = 0; j < drawList->CmdBuffer.Size; j++)
			{
				const ImDrawCmd* drawCmd = &drawList->CmdBuffer[j];

				Rect clip_rect;
				clip_rect.x = (drawCmd->ClipRect.x - clipOff.x) * clipScale.x;
				clip_rect.y = (drawCmd->ClipRect.y - clipOff.y) * clipScale.y;
				clip_rect.width = (drawCmd->ClipRect.z - clipOff.x) * clipScale.x;
				clip_rect.height = (drawCmd->ClipRect.w - clipOff.y) * clipScale.y;

				if (clip_rect.x < fbWidth && clip_rect.y < fbHeight && clip_rect.width >= 0.0f && clip_rect.height >= 0.0f)
				{
					if (clip_rect.x < 0.0f)
						clip_rect.x = 0.0f;
					if (clip_rect.y < 0.0f)
						clip_rect.y = 0.0f;
					
					clip_rect.width = clip_rect.width - clip_rect.x;
					clip_rect.height = clip_rect.height - clip_rect.y;
					rc->SetScissor(clip_rect);

					TextureID id = s_Data.defaultTexture;
					if (drawCmd->TextureId != EU_INVALID_TEXTURE_ID)
						id = drawCmd->TextureId;

					if (id == s_Data.fontTexture || id == s_Data.directoryIcon || id == s_Data.fileIcon)
						rc->SwitchPipeline(0);
					else
						rc->SwitchPipeline(1);

					Eunoia::TextureGroupBind bind;
					bind.numTextureBinds = 1;
					bind.set = 1;
					bind.binds[0].binding = 0;
					bind.binds[0].textureArrayLength = 1;
					bind.binds[0].sampler = EU_SAMPLER_LINEAR_CLAMP_TO_EDGE;
					bind.binds[0].texture[0] = id;

					rc->BindTextureGroup(bind);

					command.indexOffset = drawCmd->IdxOffset + indexOffset;
					command.vertexOffset = drawCmd->VtxOffset + vertexOffset;
					command.count = drawCmd->ElemCount;

					rc->SubmitRenderCommand(command);
				}
			}

			vertexOffset += drawList->VtxBuffer.Size;
			indexOffset += drawList->IdxBuffer.Size;
		}

		rc->UnmapBuffer(s_Data.vertexBuffer);
		rc->UnmapBuffer(s_Data.indexBuffer);

		rc->EndRenderPass();
	}

	void EditorGUI::RenderGUI()
	{
		RenderContext* rc = Engine::GetRenderContext();
		u32 width, height;
		rc->GetFramebufferSize(s_Data.renderPass, &width, &height);

		//if (!ProjectManager::GetProject()->loaded)
		//{
		//	DrawSelectProjectWindow();
		//	return;
		//}

		ImGuiDockNodeFlags dockspace_flags = ImGuiDockNodeFlags_None;
		ImGuiWindowFlags window_flags = ImGuiWindowFlags_MenuBar | ImGuiWindowFlags_NoDocking;
		ImGuiViewport* viewport = ImGui::GetMainViewport();
		ImGui::SetNextWindowPos(viewport->Pos);
		ImGui::SetNextWindowSize(viewport->Size);
		ImGui::SetNextWindowViewport(viewport->ID);
		ImGui::PushStyleVar(ImGuiStyleVar_WindowRounding, 0.0f);
		ImGui::PushStyleVar(ImGuiStyleVar_WindowBorderSize, 0.0f);
		window_flags |= ImGuiWindowFlags_NoTitleBar | ImGuiWindowFlags_NoCollapse | ImGuiWindowFlags_NoResize | ImGuiWindowFlags_NoMove;
		window_flags |= ImGuiWindowFlags_NoBringToFrontOnFocus | ImGuiWindowFlags_NoNavFocus;

		bool open;
		ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(0.0f, 0.0f));
		ImGui::Begin("EunoiaEditor", &open, window_flags);
		ImGui::PopStyleVar(3);

		ImGuiID dockspace_id = ImGui::GetID("eunoia");

		if (ImGui::DockBuilderGetNode(dockspace_id) == 0)
		{
			ImGui::DockBuilderRemoveNode(dockspace_id); // Clear out existing layout
			ImGuiID test = ImGui::DockBuilderAddNode(dockspace_id, ImGuiDockNodeFlags_DockSpace); // Add empty node
			ImGui::DockBuilderSetNodeSize(dockspace_id, ImVec2(width, height));
			ImGuiID dock_main_id = dockspace_id;
			ImGuiID dock_up_id = ImGui::DockBuilderSplitNode(dock_main_id, ImGuiDir_Up, 0.12f, 0, &dock_main_id);
			ImGuiID dock_right_id = ImGui::DockBuilderSplitNode(dock_main_id, ImGuiDir_Right, 0.25f, 0, &dock_main_id);
			ImGuiID dock_right_down_id = ImGui::DockBuilderSplitNode(dock_right_id, ImGuiDir_Right, 0.25f, 0, &dock_right_id);
			ImGuiID dock_left_id = ImGui::DockBuilderSplitNode(dock_main_id, ImGuiDir_Left, 0.2f, 0, &dock_main_id);
			ImGuiID dock_center_id = ImGui::DockBuilderSplitNode(dock_main_id, ImGuiDir_Right, 1.0f, 0, &dock_left_id);
			ImGuiID dock_down_id = ImGui::DockBuilderSplitNode(dock_center_id, ImGuiDir_Down, 0.2f, 0, &dock_center_id);

			ImGui::DockBuilderDockWindow("Compile", dock_up_id);
			ImGui::DockBuilderDockWindow("World", dock_left_id);
			ImGui::DockBuilderDockWindow("Content", dock_right_id);
			ImGui::DockBuilderDockWindow("Asset Browser", dock_down_id);
			ImGui::DockBuilderDockWindow("Game", dock_center_id);
		}

		ImGui::DockSpace(dockspace_id, ImVec2(0.0f, 0.0f), dockspace_flags);

		DrawMainMenuBar();
		DrawCompileWindow();
		DrawSceneHierarchyWindow();
		DrawPropertiesWindow();
		DrawSystemsWindow();
		DrawAssetBrowserWindow();
		DrawRenderersWindow();
		DrawGameWindow();
		DrawMaterialEditorWindow();

		DrawPopupWindows();

		ImGui::End();
	}

	b32 EditorGUI::CheckForShortcut(const EditorShortcut& shortcut)
	{
		for (u32 i = 0; i < shortcut.numHoldKeys; i++)
			if (!EUInput::IsKeyDown(shortcut.holdKeys[i]))
				return false;

		if (EUInput::IsKeyPressed(shortcut.pressKey))
			return true;

		return false;
	}

	void EditorGUI::CheckForShortcuts()
	{
		EditorSettings* settings = EditorSettings::GetEditorSettings();
		if (CheckForShortcut(settings->saveProjectShortcut))
			ProjectManager::SaveProject();
		else if (CheckForShortcut(settings->switchModeShortcut))
		{
			if(s_Data.selectedEntity != EU_ECS_INVALID_ENTITY_ID &&
			   ProjectManager::GetProject()->application->GetECS()->DoesEntityHaveComponent<Transform3DComponent>(s_Data.selectedEntity))
					s_Data.transformWidgetMode = (TransformWidgetMode)((s_Data.transformWidgetMode + 1) % NUM_TRANSFORM_WIDGET_MODES);
		}
		else if (CheckForShortcut(settings->recompileShortcut))
		{
			b32 recompiled = ProjectManager::RecompileProject();
		}
		else if (CheckForShortcut(settings->toggleStepApplicationShortcut))
		{
			EunoiaProject* project = ProjectManager::GetProject();
			if (project->loaded)
			{
				project->stepApplication = !project->stepApplication;
				project->stepPaused = false;
			}
		}
		else if (CheckForShortcut(settings->pauseShortcut))
		{
			EunoiaProject* project = ProjectManager::GetProject();
			if (project->loaded)
				project->stepPaused = !project->stepPaused;
		}
	}

	void EditorGUI::DrawSelectProjectWindow()
	{
		if (ImGui::Begin("Select Project"))
		{
			const List<String>& projectNames = ProjectManager::GetProjectNamesInProjectFolder();
			EU_PERSISTENT u32 SelectedProject = 0;
			if (ImGui::BeginCombo("Project##SelectProjectToLoad", projectNames[SelectedProject].C_Str()))
			{
				for (u32 i = 0; i < projectNames.Size(); i++)
				{
					if (ImGui::Selectable((projectNames[i] + "##SelectProjectToLoad" + projectNames[i]).C_Str(), SelectedProject == i))
					{
						SelectedProject = i;
					}
				}
				ImGui::EndCombo();
			}
		}
		ImGui::End();

		if (ImGui::Button("Load Project##LoadSelectedProject"))
		{
			Engine::RecreateWorld(EUNOIA_WORLD_MAIN, EUNOIA_WORLD_FLAG_ALL, true);
		}
	}

	void EditorGUI::DrawMainMenuBar()
	{
		if (ImGui::BeginMainMenuBar())
		{
			if (ImGui::BeginMenu("File"))
			{
				if (ImGui::MenuItem("Create Project"))
				{
					s_Data.openPopup = "Create Project";
				} ImGui::Separator();
				if (ImGui::MenuItem("Load Project"))
				{
					s_Data.openPopup = "Load Project";
				} ImGui::Separator();
				if (ImGui::MenuItem("Save Project"))
				{
					ProjectManager::SaveProject();
				}
				ImGui::EndMenu();
			}
			if (ImGui::BeginMenu("Create"))
			{
				if (ImGui::MenuItem("Component Type"))
				{
					s_Data.openPopup = "Create Component Type";
				} ImGui::Separator();
				if (ImGui::MenuItem("System Type"))
				{
					s_Data.openPopup = "Create System Type";
				}ImGui::Separator();
				if (ImGui::MenuItem("Event Type"))
				{
					s_Data.openPopup = "Create Event Type";
				}
				ImGui::EndMenu();
			}
			if (ImGui::BeginMenu("Editor"))
			{
				if (ImGui::MenuItem("Settings"))
				{
					s_Data.openPopup = "Editor Settings";
				} ImGui::Separator();
				if (ImGui::BeginMenu("Camera"))
				{
					ImVec4 textColor = ImGui::GetStyle().Colors[ImGuiCol_Text];
					textColor.x -= 0.15;
					textColor.z -= 0.15;

					EngineCamera ec = s_Data.engineCamera;

					if(ec == ENGINE_CAMERA_3D)
						ImGui::PushStyleColor(ImGuiCol_Text, textColor);

					if (ImGui::MenuItem("3D"))
					{
						s_Data.engineCamera = ENGINE_CAMERA_3D;
						SetEngineCamera(s_Data.engineCamera);
					} ImGui::Separator();

					if (ec == ENGINE_CAMERA_3D)
						ImGui::PopStyleColor();
					else if(ec == ENGINE_CAMERA_2D)
						ImGui::PushStyleColor(ImGuiCol_Text, textColor);

					if (ImGui::MenuItem("2D"))
					{
						s_Data.engineCamera = ENGINE_CAMERA_2D;
						SetEngineCamera(s_Data.engineCamera);
					}

					if (ec == ENGINE_CAMERA_2D)
						ImGui::PopStyleColor();

					ImGui::EndMenu();
				} ImGui::Separator();
				if (ImGui::MenuItem("Theme"))
				{

				}
				ImGui::EndMenu();
			}
			ImGui::EndMainMenuBar();
		}
	}

	void EditorGUI::DrawCompileWindow()
	{
		if (ImGui::Begin("Compile"))
		{
			if (ImGui::Button("Recompile"))
			{
				ProjectManager::RecompileProject();
			}ImGui::SameLine();
			if (ImGui::Button("Create Final Build"))
			{
				ProjectManager::CreateFinalBuild();
			}ImGui::SameLine();
			if (ImGui::Button("Open VS2019"))
			{
				ProjectManager::OpenVS2019();
			}
			EunoiaProject* project = ProjectManager::GetProject();
			if (!project->loaded)
			{
				ImGui::Button("PLAY");
			}
			else
			{
				if (project->stepApplication)
				{
					if (ImGui::Button("PAUSE"))
					{
						project->stepApplication = false;
						project->stepPaused = true;
					} ImGui::SameLine();
					if (ImGui::Button("STOP"))
					{
						project->stepApplication = false;
						project->stepPaused = false;
						project->application->GetECS()->RestoreResetPoint(project->resetPoint);
						SetEngineCamera(s_Data.engineCamera);
					}
				}
				else
				{
					if (project->stepPaused)
					{
						if (ImGui::Button("PLAY"))
						{
							project->stepApplication = true;
							project->stepPaused = false;
						} ImGui::SameLine();
						if (ImGui::Button("STOP"))
						{
							project->stepApplication = false;
							project->stepPaused = false;
							project->application->GetECS()->RestoreResetPoint(project->resetPoint);
							SetEngineCamera(s_Data.engineCamera);
						}
					}
					else
					{
						if (ImGui::Button("PLAY"))
						{
							project->stepApplication = true;
							project->stepPaused = false;
							project->application->GetECS()->CreateResetPoint(&project->resetPoint);
							SetEngineCamera(ENGINE_CAMERA_NONE);
						}
					}
				}
			}
		}
		ImGui::End();
	}

	static void DrawEntityHierarchy(EntityID entityID, const List<ECSEntityContainer>& entities)
	{
		const ECSEntityContainer& entity = entities[entityID - 2];
		if (ImGui::Selectable((entity.name + "##SelectEntity").C_Str(), entityID == s_Data.selectedEntity))
		{
			s_Data.selectedEntity = entityID;
			s_Data.selectedComponent = -1;
			s_Data.selectedCollisionShape = -1;
			memcpy(s_Data.entityInputName, entity.name.C_Str(), entity.name.Length() + 1);
		}

		if (ImGui::IsItemHovered() && ImGui::IsMouseDown(EU_BUTTON_RIGHT))
		{
			s_Data.selectedEntity = entityID;
			s_Data.selectedComponent = -1;
			s_Data.selectedCollisionShape = -1;
			s_Data.openPopup = "Entity Right Clicked";
		}

		ImGui::Indent();
		for (u32 i = 0; i < entity.children.Size(); i++)
		{
			DrawEntityHierarchy(entity.children[i], entities);
		}
		ImGui::Unindent();
	}

	void EditorGUI::DrawSceneHierarchyWindow()
	{
		if (ImGui::Begin("Scene Hierarchy"))
		{
			EunoiaProject* project = ProjectManager::GetProject();
			if (!project->loaded)
			{
				ImGui::End();
				return;
			}

			ECS* ecs = project->application->GetECS();
			List<ECSScene>& scenes = ecs->GetAllScenes_();

			String activeSceneName = ecs->GetActiveSceneName();
			memcpy(s_Data.sceneNameInputText, activeSceneName.C_Str(), activeSceneName.Length() + 1);
			ImGui::PushItemWidth(175);
			if (ImGui::InputText("##SelectSceneInputText", s_Data.sceneNameInputText, 64, ecs->GetActiveScene() == EU_ECS_INVALID_SCENE_ID ? ImGuiInputTextFlags_ReadOnly : ImGuiInputTextFlags_None))
			{
				String newSceneName = s_Data.sceneNameInputText;
				ecs->SetSceneName(ecs->GetActiveScene(), s_Data.sceneNameInputText);
			}
			ImGui::PopItemWidth();

			ImVec2 pos = ImGui::GetItemRectMin();
			ImVec2 size = ImGui::GetItemRectSize();
			size.x = 175;

			ImGui::SameLine(0, 0);
			if (ImGui::ArrowButton("SceneSelectDropdownButton", ImGuiDir_Down))
			{
				ImGui::OpenPopup("SceneSelectDropdownPopup");
			}
			r32 arrowButtonWidth = ImGui::GetItemRectSize().x;

			ImGui::SameLine();
			if (ImGui::Button("Add Scene"))
			{
				String sceneName = "New Scene";
				if (!scenes.Empty())
					sceneName += " " + String::S32ToString(scenes.Size());

				ecs->CreateScene(sceneName);
			}
			pos.y += size.y;
			size.x += arrowButtonWidth;
			size.y += size.y * EU_MIN(scenes.Size(), 6) - 5;
			ImGui::SetNextWindowPos(pos);
			ImGui::SetNextWindowSize(size);
			if (ImGui::BeginPopup("SceneSelectDropdownPopup", ImGuiWindowFlags_NoMove))
			{
				for (u32 i = 0; i < scenes.Size(); i++)
				{
					if (!scenes[i].created)
						continue;

					if (ImGui::Selectable(scenes[i].name.C_Str()))
					{
						ecs->SetActiveScene(i + 1);
						ImGui::CloseCurrentPopup();
					}
				}
				ImGui::EndPopup();
			}

			if (ImGui::IsWindowHovered() && ImGui::IsMouseClicked(EU_BUTTON_RIGHT))
			{
				s_Data.openPopup = "SceneHierarchyAddContent";
			}

			if (ecs->GetActiveScene() != EU_ECS_INVALID_SCENE_ID)
			{
				const List<ECSEntityContainer>& entities = ecs->GetAllEntities_();
				EntityID rootEntityID = ecs->GetRootEntity();
				const ECSEntityContainer rootEntity = entities[rootEntityID - 2];

				for (u32 i = 0; i < rootEntity.children.Size(); i++)
					DrawSceneHierarchyEntity(rootEntity.children[i]);
			}
		}
		ImGui::End();
	}

	void EditorGUI::DrawSceneHierarchyEntity(EntityID entityID)
	{
		ECS* ecs = ProjectManager::GetProject()->application->GetECS();
		const List<ECSEntityContainer>& entities = ecs->GetAllEntities_();
		const ECSEntityContainer& entity = entities[entityID - 2];

		ImGuiTreeNodeFlags flags = ImGuiTreeNodeFlags_OpenOnArrow | ImGuiTreeNodeFlags_SpanAvailWidth;
		if (s_Data.selectedEntity == entityID)
			flags |= ImGuiTreeNodeFlags_Selected;

		if (!entity.enabled)
			ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(0.35f, 0.3f, 0.3f, 1.0f));

		b32 opened = ImGui::TreeNodeEx((entity.name + "##SceneHierarchy" + entity.name).C_Str(), flags);

		if(!entity.enabled)
			ImGui::PopStyleColor();

		if (ImGui::IsItemClicked(EU_BUTTON_LEFT))
		{
			s_Data.selectedEntity = entityID;

			String selectedEntityName = ecs->GetEntityName(s_Data.selectedEntity);
			memcpy(s_Data.entityInputName, selectedEntityName.C_Str(), selectedEntityName.Length() + 1);
		}
		else if (ImGui::IsItemClicked(EU_BUTTON_RIGHT))
		{
			s_Data.selectedEntity = entityID;
			s_Data.openPopup = "SceneHierarchyEntityAddContent";

			String selectedEntityName = ecs->GetEntityName(s_Data.selectedEntity);
			memcpy(s_Data.entityInputName, selectedEntityName.C_Str(), selectedEntityName.Length() + 1);
		}

		if (ImGui::IsWindowHovered() && ImGui::IsMouseClicked(EU_BUTTON_LEFT))
		{
			s_Data.selectedEntity = EU_ECS_INVALID_ENTITY_ID;
		}

		if (opened)
		{
			for (u32 i = 0; i < entity.children.Size(); i++)
				DrawSceneHierarchyEntity(entity.children[i]);

			ImGui::TreePop();
		}
	}

	void EditorGUI::DrawPropertiesWindow()
	{
		if (ImGui::Begin("Properties"))
		{
			if (s_Data.selectedEntity != EU_ECS_INVALID_ENTITY_ID)
			{
				ECS* ecs = ProjectManager::GetProject()->application->GetECS();

				if (ImGui::InputText("##EntityNameInputText", s_Data.entityInputName, 32))
				{
					ecs->SetEntityName(s_Data.selectedEntity, s_Data.entityInputName);
				} ImGui::SameLine();
				if (ImGui::Button("Add Component"))
				{
					s_Data.openPopup = "Add Component";
				}

				ImGui::NewLine();

				List<ECSEntityContainer>& entities = ecs->GetAllEntities_();
				ECSEntityContainer* entity = &entities[s_Data.selectedEntity - 2];

				for (u32 i = 0; i < entity->components.Size(); i++)
				{
					ECSComponentContainer* component = &entity->components[i];
					const MetadataInfo& componentMetadata = Metadata::GetMetadata(component->typeID);
					String treeID = "##Properties" + entity->name + componentMetadata.cls->name;

					if (!component->actualComponent->enabled)
						ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(0.35f, 0.3f, 0.3f, 1.0f));

					b32 opened = ImGui::TreeNodeEx((componentMetadata.cls->name + treeID).C_Str(), ImGuiTreeNodeFlags_Selected | ImGuiTreeNodeFlags_SpanAvailWidth | ImGuiTreeNodeFlags_OpenOnArrow);
					
					if (!component->actualComponent->enabled)
						ImGui::PopStyleColor();
					
					if (ImGui::IsItemHovered() && ImGui::IsMouseClicked(EU_BUTTON_RIGHT))
					{
						s_Data.openPopup = "PropertiesComponentOptions";
						s_Data.selectedComponent = i;
					}
					if (opened)
					{
						DrawMetadata(componentMetadata, (const u8*)component->actualComponent);
						if (component->typeID == Metadata::GetTypeID<Transform3DComponent>())
						{
							if(ImGui::Button("Set to Editor Camera Transform"))
							{
								ECS* editorECS = Engine::GetActiveApplication()->GetECS();
								EntityID editorCamera = editorECS->GetEntityID("Camera");
								const Transform3D& editorCameraTransform = editorECS->GetComponent<Transform3DComponent>(editorCamera)->worldTransform;

								Transform3D* transform = &((Transform3DComponent*)component->actualComponent)->localTransform;
								transform->pos = editorCameraTransform.pos;
								transform->rot = editorCameraTransform.rot;
							}
						}
						ImGui::TreePop();
					}
				}
			}
		}
		ImGui::End();
	}

	void EditorGUI::DrawSystemsWindow()
	{
		if (ImGui::Begin("Systems"))
		{
			if (!ProjectManager::GetProject()->loaded)
			{
				ImGui::End();
				return;
			}

			ECS* ecs = ProjectManager::GetProject()->application->GetECS();

			if (ecs->GetActiveScene() == EU_ECS_INVALID_SCENE_ID)
			{
				ImGui::End();
				return;
			}

			List<ECSSystemContainer>& systems = ecs->GetSystems_();
			for (u32 i = 0; i < systems.Size(); i++)
			{
				ECSSystemContainer* system = &systems[i];
				const MetadataInfo& systemMetadata = Metadata::GetMetadata(system->typeID);
				String idString = "##SystemsWindow" + systemMetadata.cls->name;

				if (!system->actualSystem->enabled)
					ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(0.35f, 0.3f, 0.3f, 1.0));

				b32 opened = ImGui::TreeNodeEx((systemMetadata.cls->name + idString).C_Str(), ImGuiTreeNodeFlags_Selected);

				if (!system->actualSystem->enabled)
					ImGui::PopStyleColor();

				if (opened)
				{
					DrawMetadata(systemMetadata, (const u8*)system->actualSystem);
					ImGui::TreePop();
				}
			}

			if (ImGui::Button("Add Systems##SystemsWindowAddSystemsButton"))
			{
				s_Data.openPopup = "Add System";
			}
		}
		ImGui::End();
	}

	void EditorGUI::DrawMaterialEditorWindow()
	{
		if (ImGui::Begin("Material"))
		{
			if (s_Data.materialEditMode == MATERIAL_EDIT_NONE)
			{
				ImGui::End();
				return;
			}

			
			r32 texSize_ = 64;
			ImVec2 texSize(texSize_, texSize_);

			if (ImGui::TreeNodeEx("Albedo##MaterialWindowAlbedo", ImGuiTreeNodeFlags_Selected))
			{
				if (s_Data.materialEditMode == MATERIAL_EDIT_MATERIAL)
				{
					Material& material = AssetManager::GetMaterial_(s_Data.materialToEdit);
					ImGui::Image(material.textures[MATERIAL_TEXTURE_ALBEDO], texSize);
					DoDragDropMaterialTexture(MATERIAL_TEXTURE_ALBEDO);
				}
				else if (s_Data.materialEditMode == MATERIAL_EDIT_MATERIAL_MODIFIER)
				{
					MaterialModifier& modifier = AssetManager::GetMaterialModifier_(s_Data.modifierToEdit);
					v3& albedo = modifier.albedo;
					if (ImGui::ColorButton("Albedo##MaterialModifierEditAlbedo", ImVec4(albedo.x, albedo.y, albedo.z, 1.0f)))
					{
						s_Data.openPopup = "ColorPicker";
						s_Data.colorPickerColor = &albedo;
					}
				}
				ImGui::TreePop();
			}
			if (ImGui::TreeNodeEx("Normal##MaterialWindowNormal", ImGuiTreeNodeFlags_Selected))
			{
				if (s_Data.materialEditMode == MATERIAL_EDIT_MATERIAL)
				{
					Material& material = AssetManager::GetMaterial_(s_Data.materialToEdit);
					ImGui::Image(material.textures[MATERIAL_TEXTURE_NORMAL], texSize);
					DoDragDropMaterialTexture(MATERIAL_TEXTURE_NORMAL);
				}
				else if (s_Data.materialEditMode == MATERIAL_EDIT_MATERIAL_MODIFIER)
				{

				}
				ImGui::TreePop();
			}
			if (ImGui::TreeNodeEx("Displacement##MaterialWindowDisplacement", ImGuiTreeNodeFlags_Selected))
			{
				if (s_Data.materialEditMode == MATERIAL_EDIT_MATERIAL)
				{
					Material& material = AssetManager::GetMaterial_(s_Data.materialToEdit);
					ImGui::Image(material.textures[MATERIAL_TEXTURE_DISPLACEMENT], texSize);
					DoDragDropMaterialTexture(MATERIAL_TEXTURE_DISPLACEMENT);
				}
				else if (s_Data.materialEditMode == MATERIAL_EDIT_MATERIAL_MODIFIER)
				{

				}
				ImGui::TreePop();
			}
			if (ImGui::TreeNodeEx("Metallic##MaterialWindowMetallic", ImGuiTreeNodeFlags_Selected))
			{
				if (s_Data.materialEditMode == MATERIAL_EDIT_MATERIAL)
				{
					Material& material = AssetManager::GetMaterial_(s_Data.materialToEdit);
					ImGui::Image(material.textures[MATERIAL_TEXTURE_METALLIC], texSize);
					DoDragDropMaterialTexture(MATERIAL_TEXTURE_METALLIC);
				}
				else if (s_Data.materialEditMode == MATERIAL_EDIT_MATERIAL_MODIFIER)
				{

				}
				ImGui::TreePop();
			}
			if (ImGui::TreeNodeEx("Roughness##MaterialWindowRoughness", ImGuiTreeNodeFlags_Selected))
			{
				if (s_Data.materialEditMode == MATERIAL_EDIT_MATERIAL)
				{
					Material& material = AssetManager::GetMaterial_(s_Data.materialToEdit);
					ImGui::Image(material.textures[MATERIAL_TEXTURE_ROUGHNESS], texSize);
					DoDragDropMaterialTexture(MATERIAL_TEXTURE_ROUGHNESS);
				}
				else if (s_Data.materialEditMode == MATERIAL_EDIT_MATERIAL_MODIFIER)
				{

				}
				ImGui::TreePop();
			}
			if (ImGui::TreeNodeEx("AO##MaterialWindowAO", ImGuiTreeNodeFlags_Selected))
			{
				if (s_Data.materialEditMode == MATERIAL_EDIT_MATERIAL)
				{
					Material& material = AssetManager::GetMaterial_(s_Data.materialToEdit);
					ImGui::Image(material.textures[MATERIAL_TEXTURE_AO], texSize);
					DoDragDropMaterialTexture(MATERIAL_TEXTURE_AO);
				}
				else if (s_Data.materialEditMode == MATERIAL_EDIT_MATERIAL_MODIFIER)
				{

				}
				ImGui::TreePop();
			}

		}
		ImGui::End();
	}

	void EditorGUI::DrawRenderersWindow()
	{
		RenderContext* rc = Engine::GetRenderContext();
		Renderer2D* r2D = Engine::GetRenderer()->GetRenderer2D();
		Renderer3D* r3D = Engine::GetRenderer()->GetRenderer3D();

		ImVec2 textureSize(128, 72);

		if (ImGui::Begin("Renderers"))
		{
			if (ImGui::TreeNode("Renderer2D"))
			{
				if (ImGui::TreeNode("Framebuffers##Renderer3DFramebuffers"))
				{
					if (ImGui::ImageButton(r2D->GetOutputTexture(), textureSize, ImVec2(0, 0), ImVec2(1, 1), 0))
					{
						s_Data.gameWindowTexture = r2D->GetOutputTexture();
					}
					ImGui::TreePop();
				}
				if (ImGui::TreeNode("Settings##Renderer2DSettings"))
				{
					ImGui::Text("SpritePosOrigin ");
					if (ImGui::BeginCombo("##Renderer2DSelectSpritePosOrigin", ProjectManager::GetSpritePosOriginString(r2D->GetSpritePosOrigin()).C_Str()))
					{
						for (u32 i = 0; i < NUM_SPRITE_POS_ORIGIN_TYPES; i++)
						{
							if (ImGui::Selectable(ProjectManager::GetSpritePosOriginString((SpritePosOrigin)i).C_Str()))
							{
								r2D->SetSpritePosOrigin((SpritePosOrigin)i);
							}
						}
						ImGui::EndCombo();
					}
					ImGui::TreePop();
				}
				ImGui::TreePop();
			}
			if (ImGui::TreeNode("Renderer3D"))
			{
				if (ImGui::TreeNode("Framebuffers##Renderer3DFramebuffers"))
				{
					const Renderer3DOutputTextures& textures = r3D->GetOutputTextures();

					r32 sizeDivide = 6;

					if (ImGui::TreeNode("Shadow Pass"))
					{
						if (ImGui::ImageButton(textures.shadowMap, textureSize, ImVec2(0, 0), ImVec2(1, 1), 0))
						{
							s_Data.gameWindowTexture = textures.shadowMap;
						}
						ImGui::TreePop();
					}

					if (ImGui::TreeNode("Deferred Pass"))
					{
						ImGui::Text("Albedo");
						if (ImGui::ImageButton(textures.gbufferAlbedo, textureSize, ImVec2(0, 0), ImVec2(1, 1), 0))
						{
							s_Data.gameWindowTexture = textures.gbufferAlbedo;
						}
						ImGui::Text("World Pos");
						if (ImGui::ImageButton(textures.gbufferPosition, textureSize, ImVec2(0, 0), ImVec2(1, 1), 0))
						{
							s_Data.gameWindowTexture = textures.gbufferPosition;
						}
						ImGui::Text("Normal");
						if (ImGui::ImageButton(textures.gbufferNormal, textureSize, ImVec2(0, 0), ImVec2(1, 1), 0))
						{
							s_Data.gameWindowTexture = textures.gbufferNormal;
						}
						ImGui::Text("Depth");
						if (ImGui::ImageButton(textures.gbufferDepth, textureSize, ImVec2(0, 0), ImVec2(1, 1), 0))
						{
							s_Data.gameWindowTexture = textures.gbufferDepth;
						}
						ImGui::Text("Lighting");
						if (ImGui::ImageButton(textures.gbufferOutput, textureSize, ImVec2(0, 0), ImVec2(1, 1), 0))
						{
							s_Data.gameWindowTexture = textures.gbufferOutput;
						}
						ImGui::Text("Bloom Threshold");
						if (ImGui::ImageButton(textures.gbufferBloomThreshold, textureSize, ImVec2(0, 0), ImVec2(1, 1), 0))
						{
							s_Data.gameWindowTexture = textures.gbufferBloomThreshold;
						}

						ImGui::TreePop();
					}

					if (ImGui::TreeNode("Bloom Pass"))
					{
						if (ImGui::ImageButton(textures.bloomTexture, textureSize, ImVec2(0, 0), ImVec2(1, 1), 0))
						{
							s_Data.gameWindowTexture = textures.bloomTexture;
						}
						ImGui::TreePop();
					}

					if (ImGui::TreeNode("Final Pass"))
					{
						if (ImGui::ImageButton(textures.outputTexture, textureSize, ImVec2(0, 0), ImVec2(1, 1), 0))
						{
							s_Data.gameWindowTexture = textures.outputTexture;
						}
						ImGui::TreePop();
					}

					ImGui::TreePop();
				}
				if (ImGui::TreeNode("Settings##Renderer3DSettings"))
				{
					v3& ambient = r3D->GetAmbient();
					ImGui::Text("Ambient"); ImGui::SameLine();
					ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(1.0f, 0.0f, 0.0f, 1.0f));
					ImGui::PushItemWidth(50);
					ImGui::DragFloat("##Renderer3DAmbientR", &ambient.x, 0.01f, 0.0f, 1.0f); ImGui::SameLine();
					ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(0.0f, 1.0f, 0.0f, 1.0f));
					ImGui::DragFloat("##Renderer3DAmbientG", &ambient.y, 0.01f, 0.0f, 1.0f); ImGui::SameLine();
					ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(0.0f, 0.0f, 1.0f, 1.0f));
					ImGui::DragFloat("##Renderer3DAmbientB", &ambient.z, 0.01f, 0.0f, 1.0f);

					ImGui::PopStyleColor(3);

					ImGui::Text("BloomThreshold"); ImGui::SameLine();
					r32& threshold = r3D->GetBloomThreshold();
					ImGui::DragFloat("##Renderer3DBloomThreshold", &threshold, 0.01f, 0.0f);

					ImGui::Text("BloomBlurIterCount"); ImGui::SameLine();
					u32& iterCount = r3D->GetBloomBlurIterCount();
					ImGui::DragInt("##Renderer3DBloomBlurIterCount", (s32*)&iterCount, 0.09f, 0, EU_RENDERER3D_MAX_BLOOM_BLUR_ITERATIONS);

					ImGui::PopItemWidth();
					ImGui::TreePop();
				}
				ImGui::TreePop();
			}
			if (ImGui::TreeNode("Master Renderer"))
			{
				if (ImGui::TreeNode("Framebuffers"))
				{
					if (ImGui::ImageButton(Engine::GetRenderer()->GetFinalOutput(), textureSize, ImVec2(0, 0), ImVec2(1, 1), 0))
					{
						s_Data.gameWindowTexture = Engine::GetRenderer()->GetFinalOutput();
					}
					ImGui::TreePop();
				}
				ImGui::TreePop();
			}
		}
		ImGui::End();
	}

	void EditorGUI::DrawAssetBrowserWindow()
	{
		if (ImGui::Begin("Asset Browser"))
		{
			r32 padding = 8.0f;
			r32 thumbnailSize = 128;
			r32 cellSize = thumbnailSize + padding;
			r32 contentWidth = ImGui::GetContentRegionAvail().x;
			u32 columnCount = EU_MAX(contentWidth / cellSize, 1);

			if (!ProjectManager::GetProject()->loaded)
			{
				ImGui::End();
				return;
			}

			if (ImGui::ArrowButton("AssetBrowsertBackDirButton", ImGuiDir_Left))
			{
				if (s_Data.currentDirectory->parentDirectory)
				{
					s_Data.prevDirectory = s_Data.currentDirectory;
					s_Data.currentDirectory = s_Data.currentDirectory->parentDirectory;
				}
			} ImGui::SameLine(); 
			if (ImGui::ArrowButton("AssetBrowserPrevButton", ImGuiDir_Right))
			{
				if (s_Data.prevDirectory)
					s_Data.currentDirectory = s_Data.prevDirectory;
			} ImGui::SameLine();
			if (ImGui::Button("Refresh##AssetBrowserRefresh"))
			{
				s_Data.currentDirectory->Refresh();
			}
			ImGui::Separator();

			EU_PERSISTENT b32 RenamingDirectory = false;
			EU_PERSISTENT b32 RenamingFile = false;
			if (s_Data.dirModify == ASSET_MODIFY_DELETE)
			{
				s_Data.currentDirectory->directories[s_Data.assetModifyIndex]->Delete();
				s_Data.dirModify = ASSET_MODIFY_NONE;
			}
			else if (s_Data.dirModify == ASSET_MODIFY_RENAME)
			{
				RenamingDirectory = true;
				s_Data.dirModify = ASSET_MODIFY_NONE;
				const String& fileName = s_Data.currentDirectory->directories[s_Data.assetModifyIndex]->name;
				memcpy(s_Data.fileNameInput, fileName.C_Str(), fileName.Length() + 1);
			}
			if (s_Data.fileModify == ASSET_MODIFY_DELETE)
			{
				s_Data.currentDirectory->files[s_Data.assetModifyIndex].Delete();
				s_Data.fileModify = ASSET_MODIFY_NONE;
			}
			else if (s_Data.fileModify == ASSET_MODIFY_RENAME)
			{
				RenamingFile = true;
				s_Data.fileModify = ASSET_MODIFY_NONE;
				const String& fileName = s_Data.currentDirectory->files[s_Data.assetModifyIndex].name;
				memcpy(s_Data.fileNameInput, fileName.C_Str(), fileName.Length() + 1);
			}

			ImGui::Columns(columnCount, 0, false);
			
			ImVec4 iconBgColor = ImGui::GetStyle().Colors[ImGuiCol_WindowBg];
			b32 iconRightClicked = false;
			EUDirectory* newDirectory = s_Data.currentDirectory;
			for (u32 i = 0; i < s_Data.currentDirectory->directories.Size(); i++)
			{
				EUDirectory* dir = s_Data.currentDirectory->directories[i];
				ImGui::PushID(("AssetBrowserDirectory" + String::S32ToString(i)).C_Str());
				ImGui::ImageButton(s_Data.directoryIcon, ImVec2(thumbnailSize, thumbnailSize), ImVec2(0, 0), ImVec2(1, 1), 0, iconBgColor);
				if (ImGui::BeginDragDropTarget())
				{
					const ImGuiPayload* assetFile = ImGui::AcceptDragDropPayload("AssetFile");
					const ImGuiPayload* assetDir = ImGui::AcceptDragDropPayload("AssetDir");

					if (assetFile)
					{
						u32 assetFileIndex = *(u32*)assetFile->Data;
						s_Data.currentDirectory->MoveFileIntoDir(i, assetFileIndex);
					}
					if (assetDir)
					{
						u32 assetDirIndex = *(u32*)assetDir->Data;
						s_Data.currentDirectory->MoveDirectory(i, assetDirIndex);
					}

					ImGui::EndDragDropTarget();
				}
				if (ImGui::BeginDragDropSource())
				{
					ImGui::ImageButton(s_Data.directoryIcon, ImVec2(thumbnailSize / 2, thumbnailSize / 2), ImVec2(0, 0), ImVec2(1, 1), 0, iconBgColor);
					ImGui::SetDragDropPayload("AssetDir", &i, sizeof(u32));
					ImGui::EndDragDropSource();
				}
				ImGui::PopID();
				if (ImGui::IsItemHovered())
				{
					if (ImGui::IsMouseDoubleClicked(EU_BUTTON_LEFT))
					{
						newDirectory = dir;
					}
					else if (ImGui::IsMouseClicked(EU_BUTTON_RIGHT))
					{
						iconRightClicked = true;
						s_Data.openPopup = "Modify Directory";
						s_Data.assetModifyIndex = i;
					}
				}

				if (RenamingDirectory && (s_Data.assetModifyIndex == i))
				{
					ImGui::SetKeyboardFocusHere(0);
					if (ImGui::InputText(("##AssetDirectoryRename" + String::S32ToString(i)).C_Str(), s_Data.fileNameInput, 32, ImGuiInputTextFlags_EnterReturnsTrue | ImGuiInputTextFlags_AutoSelectAll) ||
						((ImGui::IsMouseClicked(0) || ImGui::IsMouseClicked(1)) && !ImGui::IsItemHovered()))
					{
						RenamingDirectory = false;
						s_Data.assetModifyIndex = -1;
						if (s_Data.fileNameInput[0] != '\0')
							s_Data.currentDirectory->directories[i]->Rename(s_Data.fileNameInput);
					}
				}
				else
				{
					ImGui::Text(dir->name.C_Str());
				}

				ImGui::NextColumn();
			}

			for (u32 i = 0; i < s_Data.currentDirectory->files.Size(); i++)
			{
				EUFile* file = &s_Data.currentDirectory->files[i];
				ImGui::PushID(("AssetBrowserFile" + String::S32ToString(i)).C_Str());
				ImGui::ImageButton(s_Data.fileIcon, ImVec2(thumbnailSize, thumbnailSize), ImVec2(0, 0), ImVec2(1, 1), 0, iconBgColor);
				if (ImGui::BeginDragDropSource())
				{
					ImGui::ImageButton(s_Data.fileIcon, ImVec2(thumbnailSize / 2, thumbnailSize / 2), ImVec2(0, 0), ImVec2(1, 1), 0, iconBgColor, ImVec4(0.6f, 0.6f, 0.6f, 0.85f));
					ImGui::SetDragDropPayload("AssetFile", &i, sizeof(u32));
					ImGui::EndDragDropSource();
				}
				ImGui::PopID();
				if (ImGui::IsItemHovered())
				{
					if (ImGui::IsMouseDoubleClicked(EU_BUTTON_LEFT))
					{
						if (file->extension == "eumat")
						{
							s_Data.materialEditMode = MATERIAL_EDIT_MATERIAL;
							MaterialID mid = AssetManager::GetMaterialID(s_Data.currentDirectory->files[i].name);
							if (mid != EU_INVALID_MATERIAL_ID)
							{
								s_Data.materialToEdit = mid;
							}
							else
							{
								LoadedMaterialFile loadedMaterialFile;
								MaterialLoader::LoadEumtlMaterial(s_Data.currentDirectory->files[i].path, &loadedMaterialFile);

								if (loadedMaterialFile.materials.Empty())
								{
									Material defaultMaterial = AssetManager::GetMaterial(EU_DEFAULT_MATERIAL_ID);
									defaultMaterial.name = s_Data.currentDirectory->files[i].name;
									defaultMaterial.path = s_Data.currentDirectory->files[i].path;
									s_Data.materialToEdit = AssetManager::CreateMaterial(defaultMaterial);
								}
								else
								{
									AssetManager::CreateMaterials(loadedMaterialFile, EU_SAMPLER_LINEAR_REPEAT_AF, &mid);
									s_Data.materialToEdit = mid;
								}
							}
						}
						else if (file->extension == "eummod")
						{
							s_Data.materialEditMode = MATERIAL_EDIT_MATERIAL_MODIFIER;
							MaterialModifierID mmid = AssetManager::GetMaterialModifierID(s_Data.currentDirectory->files[i].name);
							if (mmid != EU_INVALID_MATERIAL_MODIFIER_ID)
							{
								s_Data.modifierToEdit = mmid;
							}
							else
							{
								LoadedMaterialFile loadedModifierFile;
								MaterialLoader::LoadEumtlMaterial(s_Data.currentDirectory->files[i].path, &loadedModifierFile);

								if (loadedModifierFile.modifiers.Empty())
								{
									LoadedMaterialModifier modifier;
									modifier.modifier = AssetManager::GetMaterialModifier(EU_DEFAULT_MATERIAL_MODIFIER_ID);
									modifier.name = s_Data.currentDirectory->files[i].name;
									s_Data.modifierToEdit = AssetManager::CreateMaterialModifier(modifier);
								}
								else
								{
									MaterialID mid;
									AssetManager::CreateMaterials(loadedModifierFile, EU_SAMPLER_LINEAR_REPEAT_AF, &mid, &mmid);
									s_Data.modifierToEdit = mmid;
								}
							}
						}
						else if (file->extension == "euscene")
						{
							ECSLoadedScene loadedScene;
							ECSLoader::LoadECSSceneFromFile(&loadedScene, file->path);
							SceneID scene = ProjectManager::GetProject()->application->GetECS()->LoadSceneFromLoadedDataFormat(loadedScene, true);
							file->userData = malloc(sizeof(SceneID)); //TODO: free data if file is deleted
							memcpy(file->userData, &scene, sizeof(SceneID));
						}
					}
					else if (ImGui::IsMouseClicked(EU_BUTTON_RIGHT))
					{
						iconRightClicked = true;
						s_Data.openPopup = "Modify File";
						s_Data.assetModifyIndex = i;
					}
				}

				if (RenamingFile && (s_Data.assetModifyIndex == i))
				{
					ImGui::SetKeyboardFocusHere(0);
					if (ImGui::InputText(("##AssetFileRename" + String::S32ToString(i)).C_Str(), s_Data.fileNameInput, 32, ImGuiInputTextFlags_EnterReturnsTrue | ImGuiInputTextFlags_AutoSelectAll) ||
						((ImGui::IsMouseClicked(0) || ImGui::IsMouseClicked(1)) && !ImGui::IsItemHovered()))
					{
						RenamingFile = false;
						s_Data.assetModifyIndex = -1;
						if (s_Data.fileNameInput[0] != '\0')
							s_Data.currentDirectory->files[i].Rename(s_Data.fileNameInput);
					}
				}
				else
				{
					ImGui::Text(file->name.C_Str());
				}

				ImGui::NextColumn();
			}

			s_Data.currentDirectory = newDirectory;

			ImGui::Columns(1);

			if (ImGui::IsWindowHovered() & ImGui::IsMouseClicked(EU_BUTTON_RIGHT) && !iconRightClicked)
			{
				s_Data.openPopup = "Create Asset";
			}
		}
		ImGui::End();
	}

	void EditorGUI::DrawGameWindow()
	{
		if (ImGui::Begin("Game"))
		{
			RenderContext* rc = Engine::GetRenderContext();
			TextureID outputTexture = s_Data.gameWindowTexture;
			u32 width, height;
			MasterRenderer* r = Engine::GetRenderer();
			Renderer3D* r3D = r->GetRenderer3D();
			rc->GetTextureSize(r->GetFinalOutput(), &width, &height);
			ImVec2 windowSize = ImGui::GetWindowSize();
			windowSize.y -= 40;
			if (windowSize.x != width || windowSize.y != height)
			{
				r->ResizeOutput(windowSize.x, windowSize.y);
			}
			ImVec2 GameTexturePos = ImGui::GetCursorPos();
			ImGui::Image(outputTexture, windowSize);

			if (s_Data.selectedEntity != EU_ECS_INVALID_ENTITY_ID)
			{
				EunoiaProject* project = ProjectManager::GetProject();

				ECS* ecs = project->application->GetECS();

				Transform3DComponent* transform3DComponent = ecs->GetComponent<Transform3DComponent>(s_Data.selectedEntity);
				if (transform3DComponent || s_Data.selectedCollisionShape != -1)
				{
					ImGuizmo::OPERATION transformOperation = ImGuizmo::TRANSLATE;
					switch (s_Data.transformWidgetMode)
					{
					case TRANSFORM_WIDGET_TRANSLATE: transformOperation = ImGuizmo::TRANSLATE; break;
					case TRANSFORM_WIDGET_ROTATE: transformOperation = ImGuizmo::ROTATE; break;
					case TRANSFORM_WIDGET_SCALE: transformOperation = ImGuizmo::SCALE; break;
					}

					v3 camPos = r3D->GetCameraPos();
					quat camRot = r3D->GetCameraRot();
					camPos.y *= -1.0f;
					camRot.x *= -1.0f;
					camRot.z *= -1.0f;
					m4 view = m4::CreateView(camPos, camRot).Transpose();
					m4 projection = r3D->GetProjection().Transpose();

					List<ECSEntityContainer>& entities = ecs->GetAllEntities_();
					ECSEntityContainer* selectedEntity = &entities[s_Data.selectedEntity - 2];

					Transform3D transformation;
					b32 usingCollisionShapeTransform;

					if (s_Data.selectedCollisionShape != -1)
					{
						usingCollisionShapeTransform = true;
						btCompoundShape* shapes = (btCompoundShape*)((RigidBodyComponent*)selectedEntity->components[s_Data.selectedComponent].actualComponent)->body.GetRigidBody()->getCollisionShape();
						btCollisionShape* shape = shapes->getChildShape(s_Data.selectedCollisionShape);
						const btTransform& localTransformBt = shapes->getChildTransform(s_Data.selectedCollisionShape);
						const Transform3D& parentTransform = transform3DComponent->worldTransform;
						Transform3D localTransform(PhysicsEngine3D::ToEngineVector(localTransformBt.getOrigin()), v3(1.0f, 1.0f, 1.0f), PhysicsEngine3D::ToEngineQuat(localTransformBt.getRotation()));
						transformation = parentTransform * localTransform;
					}
					else
					{
						transformation = transform3DComponent->worldTransform;
						usingCollisionShapeTransform = false;
					}

					Transform3D normalTransform = transformation;
					transformation.pos.y *= -1.0f;
					transformation.rot.x *= -1.0f;
					transformation.rot.z *= -1.0f;
					m4 translation = m4::CreateTranslation(transformation.pos).Transpose();
					m4 rotation = transformation.rot.CreateRotationMatrix().Transpose();
					m4 scale = m4::CreateScale(transformation.scale).Transpose();
					m4 transform = translation * (rotation * scale);

					ImGuizmo::SetDrawlist(ImGui::GetWindowDrawList());
					ImVec2 windowPos = ImGui::GetWindowPos();
					ImVec2 windowSize = ImGui::GetWindowSize();
					ImGuizmo::SetRect(windowPos.x, windowPos.y, windowSize.x, windowSize.y);
					m4 deltaMatrix;
					b32 manipulated = ImGuizmo::Manipulate(&view[0][0], &projection[0][0], transformOperation, ImGuizmo::WORLD, &transform[0][0], &deltaMatrix[0][0]);

					v3 deltaPos, deltaRot, deltaScale;
					ImGuizmo::DecomposeMatrixToComponents(&deltaMatrix[0][0], &deltaPos.x, &deltaRot.x, &deltaScale.x);

					if (manipulated)
					{
						deltaPos.y *= -1.0f;
						deltaRot.x *= -1.0f;
						deltaRot.z *= -1.0f;
						if (usingCollisionShapeTransform)
						{
							RigidBody* rigidBody = &((RigidBodyComponent*)selectedEntity->components[s_Data.selectedComponent].actualComponent)->body;
							btCompoundShape* shapes = (btCompoundShape*)rigidBody->GetRigidBody()->getCollisionShape();
							btTransform& localTransformBt = shapes->getChildTransform(s_Data.selectedCollisionShape);
							Transform3D newLocalTransform;
							newLocalTransform.pos = PhysicsEngine3D::ToEngineVector(localTransformBt.getOrigin());
							newLocalTransform.rot = PhysicsEngine3D::ToEngineQuat(localTransformBt.getRotation());

							newLocalTransform.Translate(deltaPos);

							if (deltaRot.x != 0.0f)
								newLocalTransform.Rotate(v3(1.0f, 0.0f, 0.0f), deltaRot.x);
							if (deltaRot.y != 0.0f)
								newLocalTransform.Rotate(v3(0.0f, 1.0f, 0.0f), deltaRot.y);
							if (deltaRot.z != 0.0f)
								newLocalTransform.Rotate(v3(0.0f, 0.0f, 1.0f), deltaRot.z);

							RigidBodyInfo rigidBodyInfo;
							rigidBody->GetSerializableInfo(&rigidBodyInfo);
							rigidBody->RemoveFromPhysicsWorld();
							rigidBody->DestroyRigidBody();

							RigidBodyShapeInfo* shapeInfo = &rigidBodyInfo.shapes[s_Data.selectedCollisionShape];
							shapeInfo->localTransform.setOrigin(PhysicsEngine3D::ToBulletVector(newLocalTransform.pos));
							shapeInfo->localTransform.setRotation(PhysicsEngine3D::ToBulletQuat(newLocalTransform.rot));
							rigidBody->CreateRigidBodyFromInfo(rigidBodyInfo);
						}
						else
						{
							transform3DComponent->localTransform.Translate(deltaPos);

							if (deltaRot.x != 0.0f)
								transform3DComponent->localTransform.Rotate(v3(1.0f, 0.0f, 0.0f), -deltaRot.x);
							if (deltaRot.y != 0.0f)
								transform3DComponent->localTransform.Rotate(v3(0.0f, 1.0f, 0.0f), deltaRot.y);
							if (deltaRot.z != 0.0f)
								transform3DComponent->localTransform.Rotate(v3(0.0f, 0.0f, 1.0f), -deltaRot.z);
						}
							
					}					
				}
			}

			EunoiaProject* project = ProjectManager::GetProject();
			if (project->loaded && ImGui::IsWindowHovered() && ImGui::IsMouseClicked(EU_BUTTON_LEFT) && !ImGuizmo::IsUsing())
			{
				ImVec2 windowPos = ImGui::GetWindowPos();
				ImVec2 mousePos = ImGui::GetMousePos();
				v2 mousePosOnWindow = v2(mousePos.x - (windowPos.x + GameTexturePos.x), mousePos.y - (windowPos.y + GameTexturePos.y));
				EntityID selectedEntity = r3D->GetEntityIDFromGBuffer(mousePosOnWindow.x, mousePosOnWindow.y);
				if (project->application->GetECS()->DoesEntityExist(selectedEntity))
					s_Data.selectedEntity = selectedEntity;
			}
		}
		ImGui::End();
	}

	void EditorGUI::DoDragDropMaterialTexture(Eunoia::MaterialTextureType texType)
	{
		if (ImGui::BeginDragDropTarget())
		{
			const ImGuiPayload* assetFile = ImGui::AcceptDragDropPayload("AssetFile");
			if (assetFile)
			{
				u32 fileIndex = *(u32*)assetFile->Data;
				if (s_Data.currentDirectory->files[fileIndex].extension == "eutex")
				{
					TextureID texture = AssetManager::CreateTexture(s_Data.currentDirectory->files[fileIndex].path);
					Material& material = AssetManager::GetMaterial_(s_Data.materialToEdit);
					material.textures[texType] = texture;
				}
			}

			ImGui::EndDragDropTarget();
		}
	}

	void EditorGUI::DrawPopupWindows()
	{
		if (!s_Data.openPopup.Empty())
			ImGui::OpenPopup(s_Data.openPopup.C_Str());

		s_Data.openPopup = "";

		if (ImGui::BeginPopupModal("Create Project"))
		{
			ImGui::InputText("Project Name", s_Data.tempInputText, 64);

			if (ImGui::Button("Cancel"))
			{
				s_Data.tempInputText[0] = 0;
				ImGui::CloseCurrentPopup();
			}

			ImGui::SameLine();
			if (ImGui::Button("Create##ConfirmCreateNewProject"))
			{
				ProjectManager::CreateNewProject(s_Data.tempInputText);
				s_Data.currentDirectory = ProjectManager::GetProject()->assetDirectory;
				s_Data.tempInputText[0] = 0;
				ImGui::CloseCurrentPopup();
			}

			ImGui::EndPopup();
		}
		if (ImGui::BeginPopupModal("Load Project"))
		{
			EU_PERSISTENT bool CompileProject = false;
			ImGui::Checkbox("Compile##CompileProjectOnLoad", &CompileProject);

			const List<String>& projectNames = ProjectManager::GetProjectNamesInProjectFolder();
			EU_PERSISTENT u32 SelectedProject = -1;
			for (u32 i = 0; i < projectNames.Size(); i++)
			{
				if (ImGui::RadioButton(projectNames[i].C_Str(), SelectedProject == i))
					SelectedProject = i;
			}

			if (ImGui::Button("Cancel##CancelLoadProject"))
			{
				SelectedProject = -1;
				ImGui::CloseCurrentPopup();
			}
			ImGui::SameLine();

			if (ImGui::Button("Load##ConfirmProjectLoad"))
			{
				if (SelectedProject != -1)
				{
					ProjectManager::LoadProject(projectNames[SelectedProject], CompileProject);
					s_Data.currentDirectory = ProjectManager::GetProject()->assetDirectory;
					SelectedProject = -1;
				}

				ImGui::CloseCurrentPopup();
			}

			ImGui::EndPopup();
		}
		if (ImGui::BeginPopup("SceneHierarchyAddContent"))
		{
			if (ImGui::Selectable("+ Add Entity"))
			{
				ProjectManager::GetProject()->application->GetECS()->CreateEntity();
			}
		
			ImGui::EndPopup();
		}
		if (ImGui::BeginPopup("SceneHierarchyEntityAddContent"))
		{
			ECS* ecs = ProjectManager::GetProject()->application->GetECS();
			String enableDisableText = ecs->IsEntityEnabled(s_Data.selectedEntity) ? "Disable" : "Enable";

			if (ImGui::Selectable("Add Child"))
			{
				ecs->CreateEntity(s_Data.selectedEntity);
			}
			if (ImGui::Selectable(enableDisableText.C_Str()))
			{
				ecs->SetEntityEnabledOpposite(s_Data.selectedEntity);
			}
			if (ImGui::Selectable("Destroy"))
			{
				ecs->DestroyEntity(s_Data.selectedEntity);
			}

			ImGui::EndPopup();
		}
		if (ImGui::BeginPopup("PropertiesComponentOptions"))
		{
			ECS* ecs = ProjectManager::GetProject()->application->GetECS();
			String enableDisableText = ecs->GetComponentByIndex(s_Data.selectedEntity, s_Data.selectedComponent)->enabled ? "Disable" : "Enable";

			if (ImGui::Selectable("Destroy"))
			{
				ProjectManager::GetProject()->application->GetECS()->DestroyComponentByIndex(s_Data.selectedComponent, s_Data.selectedComponent);
				s_Data.selectedComponent = -1;
			}
			if (ImGui::Selectable(enableDisableText.C_Str()))
			{
				ecs->GetComponentByIndex(s_Data.selectedEntity, s_Data.selectedComponent)->enabled = 
					!ecs->GetComponentByIndex(s_Data.selectedEntity, s_Data.selectedComponent)->enabled;
			}
			if (ImGui::Selectable("Copy"))
			{

			}
			ImGui::EndPopup();
		}
		if (ImGui::BeginPopup("ColorPicker"))
		{
			ImGui::ColorPicker3("Color Picker", &s_Data.colorPickerColor->x);
			if (ImGui::Button("Close##Close Color Picker"))
			{
				ImGui::CloseCurrentPopup();
			}
			ImGui::EndPopup();
		}
		if (ImGui::BeginPopupModal("Create Component Type"))
		{
			if (ImGui::InputText("Type name", s_Data.tempInputText, 32, ImGuiInputTextFlags_EnterReturnsTrue))
			{
				ProjectManager::CreateNewComponentType(s_Data.tempInputText);
				s_Data.tempInputText[0] = 0;
				ImGui::CloseCurrentPopup();
			}

			if (ImGui::Button("Cancel##CreateComponentFileCancel"))
			{
				s_Data.tempInputText[0] = 0;
				ImGui::CloseCurrentPopup();
			} ImGui::SameLine();
			if (ImGui::Button("Create##CreateComponentFileConfirm"))
			{
				ProjectManager::CreateNewComponentType(s_Data.tempInputText);
				s_Data.tempInputText[0] = 0;
				ImGui::CloseCurrentPopup();
			}
			ImGui::EndPopup();
		}
		if (ImGui::BeginPopupModal("Create System Type"))
		{
			ImGui::InputText("Type name", s_Data.tempInputText, 32);
			
			EU_PERSISTENT bool PreUpdate = false;
			EU_PERSISTENT bool Update = false;
			EU_PERSISTENT bool PostUpdate = false;
			EU_PERSISTENT bool PreRender = false;
			EU_PERSISTENT bool Render = false;
			EU_PERSISTENT bool PostRender = false;
			ImGui::Checkbox("PreUpdate", &PreUpdate); ImGui::SameLine();
			ImGui::Checkbox("Update", &Update); ImGui::SameLine();
			ImGui::Checkbox("PostUpdate", &PostUpdate);
			ImGui::Checkbox("PreRender", &PreRender); ImGui::SameLine();
			ImGui::Checkbox("Render", &Render); ImGui::SameLine();
			ImGui::Checkbox("PostRender", &PostRender);

			if (ImGui::Button("Cancel##CreatSystemFileCancel"))
			{
				s_Data.tempInputText[0] = 0;
				PreUpdate = Update = PostUpdate = PreRender = Render = PostRender = false;
				ImGui::CloseCurrentPopup();
			} ImGui::SameLine();
			if (ImGui::Button("Create##CreateSystemFileConfirm"))
			{
				u32 writeFunctionFlags = SYSTEM_FUNCTION_PreUpdate * PreUpdate | SYSTEM_FUNCTION_Update * Update | SYSTEM_FUNCTION_PostUpdate * PostUpdate |
							             SYSTEM_FUNCTION_PreRender * PreRender | SYSTEM_FUNCTION_Render * Render | SYSTEM_FUNCTION_PostRender * PostRender;

				PreUpdate = Update = PostUpdate = PreRender = Render = PostRender = false;

				ProjectManager::CreateNewSystemType(s_Data.tempInputText, writeFunctionFlags);
				s_Data.tempInputText[0] = 0;
				ImGui::CloseCurrentPopup();
			}

			ImGui::EndPopup();
		}
		if (ImGui::BeginPopupModal("Create Event Type"))
		{
			if (ImGui::InputText("Type name", s_Data.tempInputText, 32, ImGuiInputTextFlags_EnterReturnsTrue))
			{
				ProjectManager::CreateNewEventType(s_Data.tempInputText);
				s_Data.tempInputText[0] = 0;
				ImGui::CloseCurrentPopup();
			}

			if (ImGui::Button("Cancel##CreateEventFileCancel"))
			{
				s_Data.tempInputText[0] = 0;
				ImGui::CloseCurrentPopup();
			} ImGui::SameLine();
			if (ImGui::Button("Create##CreateEventFileConfirm"))
			{
				ProjectManager::CreateNewEventType(s_Data.tempInputText);
				s_Data.tempInputText[0] = 0;
				ImGui::CloseCurrentPopup();
			}
			ImGui::EndPopup();
		}
		if (ImGui::BeginPopupModal("Destroy Entity"))
		{
			ImGui::Text("This will destroy all child entities.");
			ImGui::Text("Are you sure?");
			if (ImGui::Button("YES"))
			{
				EntityID e = s_Data.selectedEntity;
				ProjectManager::GetProject()->application->GetECS()->DestroyEntity(s_Data.selectedEntity);
				s_Data.selectedEntity = EU_ECS_INVALID_ENTITY_ID;
				s_Data.selectedComponent = -1;
				s_Data.selectedCollisionShape = -1;
				ImGui::CloseCurrentPopup();
			}
			else if (ImGui::Button("NO"))
			{
				ImGui::CloseCurrentPopup();
			}
			ImGui::EndPopup();
		}
		if (ImGui::BeginPopupModal("Add Component"))
		{
			EU_PERSISTENT bool ShowEngineComponents = true;
			EU_PERSISTENT bool ShowProjectComponents = true;
			ImGui::Text("Show Components: "); ImGui::SameLine();
			ImGui::Checkbox("Engine##ShowEngineComponents", &ShowEngineComponents); ImGui::SameLine();
			ImGui::Checkbox("Project##ShowProjectComponents", &ShowProjectComponents);
			ImGui::Separator();

			ImGui::NewLine();
			ImGui::Text("Engine Components");
			ImGui::Separator();
			ImGui::NewLine();

			const List<MetadataInfo>& componentMetadatas = Metadata::GetComponentMetadataList();

			EU_PERSISTENT bool InitializeSelectedComponents = true;
			EU_PERSISTENT List<bool> SelectedComponents;

			if (InitializeSelectedComponents)
			{
				SelectedComponents.SetCapacityAndElementCount(componentMetadatas.Size());
				memset(&SelectedComponents[0], false, SelectedComponents.Size());
				InitializeSelectedComponents = false;
			}

			const metadata_typeid rigidBodyMetadataTypeID = Metadata::GetTypeID<RigidBodyComponent>();
			const u32 COMPONENTS_PER_LINE = 5;
			if (ShowEngineComponents)
			{
				for (u32 i = 0; i < componentMetadatas.Size(); i++)
				{
					if (componentMetadatas[i].id <= Metadata::LastEngineTypeID)
					{
						ImGui::Checkbox((componentMetadatas[i].cls->name + "##AddComponentType_" + componentMetadatas[i].cls->name).C_Str(), &SelectedComponents[i]);
					}

					if (((i + 1) % COMPONENTS_PER_LINE != 0) && i != (componentMetadatas.Size() - 1))
						ImGui::SameLine();
				}
			}

			ImGui::NewLine();
			ImGui::Text("Project Components");
			ImGui::Separator();
			ImGui::NewLine();

			
			if (ShowProjectComponents)
			{
				for (u32 i = 0; i < componentMetadatas.Size(); i++)
				{
					if (componentMetadatas[i].id > Metadata::LastEngineTypeID)
					{
						ImGui::Checkbox((componentMetadatas[i].cls->name + "##AddComponentType_" + componentMetadatas[i].cls->name).C_Str(), &SelectedComponents[i]);
					}

					if (((i + 1) % COMPONENTS_PER_LINE != 0) && i != (componentMetadatas.Size() - 1))
						ImGui::SameLine();
				}
			}

			ImGui::Separator();
			if (ImGui::Button("Confirm##ConfirmAddSelectedComponents"))
			{
				for (u32 i = 0; i < SelectedComponents.Size(); i++)
				{
					if (SelectedComponents[i])
						ProjectManager::GetProject()->application->GetECS()->CreateComponent(s_Data.selectedEntity, componentMetadatas[i].id);
				}

				InitializeSelectedComponents = true;
				ImGui::CloseCurrentPopup();
			}ImGui::SameLine();
			if (ImGui::Button("Cancel##CancelAddSelectedComponents"))
			{
				InitializeSelectedComponents = true;
				ImGui::CloseCurrentPopup();
			}

			ImGui::EndPopup();
		}
		if (ImGui::BeginPopupModal("Remove Component"))
		{
			ImGui::Text("Are you sure?");
			if (ImGui::Button("YES"))
			{
				ProjectManager::GetProject()->application->GetECS()->DestroyComponentByIndex(s_Data.selectedEntity, s_Data.selectedComponent);
				ImGui::CloseCurrentPopup();
			}ImGui::SameLine();
			if (ImGui::Button("NO"))
			{
				ImGui::CloseCurrentPopup();
			}
			ImGui::EndPopup();
		}
		if (ImGui::BeginPopupModal("Add System"))
		{
			EU_PERSISTENT bool ShowEngineSystems = true;
			EU_PERSISTENT bool ShowProjectSystems = true;
			ImGui::Text("Show Systems: "); ImGui::SameLine();
			ImGui::Checkbox("Engine##ShowEngineSystems", &ShowEngineSystems); ImGui::SameLine();
			ImGui::Checkbox("Project##ShowProjectSystems", &ShowProjectSystems);
			ImGui::Separator();

			const List<MetadataInfo>& systemMetadatas = Metadata::GetSystemMetadataList();

			EU_PERSISTENT bool InitializeSelectedSystems = true;
			EU_PERSISTENT List<bool> SelectedSystems;

			if (InitializeSelectedSystems)
			{
				SelectedSystems.SetCapacityAndElementCount(systemMetadatas.Size());
				memset(&SelectedSystems[0], false, SelectedSystems.Size());
				InitializeSelectedSystems = false;
			}

			const u32 COMPONENTS_PER_LINE = 5;

			ImGui::NewLine();
			ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(0.18f, 0.49f, 0.98f, 1.0f));
			ImGui::Text("Engine Systems");
			ImGui::Separator();
			ImGui::NewLine();
			ImGui::PopStyleColor();

			for (u32 i = 0; i < systemMetadatas.Size(); i++)
			{
				if (systemMetadatas[i].id <= Metadata::LastEngineTypeID)
				{
					ImGui::Checkbox((systemMetadatas[i].cls->name + "##AddSystemType" + systemMetadatas[i].cls->name).C_Str(), &SelectedSystems[i]);
				}

				if (((i + 1) % COMPONENTS_PER_LINE != 0) && i != (systemMetadatas.Size() - 1))
					ImGui::SameLine();
			}

			ImGui::NewLine();
			ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(0.18f, 0.49f, 0.98f, 1.0f));
			ImGui::Text("Project Systems");
			ImGui::Separator();
			ImGui::NewLine();
			ImGui::PopStyleColor();

			if (ShowProjectSystems)
			{
				for (u32 i = 0; i < systemMetadatas.Size(); i++)
				{
					if (systemMetadatas[i].id > Metadata::LastEngineTypeID)
					{
						ImGui::Checkbox((systemMetadatas[i].cls->name + "##AddSystemType" + systemMetadatas[i].cls->name).C_Str(), &SelectedSystems[i]);
					}

					if (((i + 1) % COMPONENTS_PER_LINE != 0) && i != (systemMetadatas.Size() - 1))
						ImGui::SameLine();
				}
			}

			ImGui::Separator();
			if (ImGui::Button("Confirm##ConfirmAddSelectedSystems"))
			{
				for (u32 i = 0; i < SelectedSystems.Size(); i++)
				{
					if (SelectedSystems[i])
						ProjectManager::GetProject()->application->GetECS()->CreateSystem(systemMetadatas[i].id);
				}

				InitializeSelectedSystems = true;
				ImGui::CloseCurrentPopup();
			}ImGui::SameLine();
			if (ImGui::Button("Cancel##CancelAddSelectedSystems"))
			{
				InitializeSelectedSystems = true;
				ImGui::CloseCurrentPopup();
			}

			ImGui::EndPopup();
		}
		if (ImGui::BeginPopupModal("Remove System"))
		{
			ImGui::Text("Are you sure?");
			if (ImGui::Button("YES##RemoveSystemYesButton"))
			{
				ProjectManager::GetProject()->application->GetECS()->DestroySystemByIndex(s_Data.selectedSystem);
				ImGui::CloseCurrentPopup();
			}
			if (ImGui::Button("NO##RemoveSystemNoButton"))
			{
				ImGui::CloseCurrentPopup();
			}
			ImGui::EndPopup();
		}
		if (ImGui::BeginPopup("Create Asset"))
		{
			if (ImGui::Selectable("+ New Folder"))
			{
				s_Data.currentDirectory->CreateNewDir("Folder" + String::S32ToString(s_Data.currentDirectory->directories.Size() + 1));
				s_Data.dirModify = ASSET_MODIFY_RENAME;
				s_Data.assetModifyIndex = s_Data.currentDirectory->directories.Size() - 1;
				ImGui::CloseCurrentPopup();
			} ImGui::Separator();
			if (ImGui::Selectable("+ New Material"))
			{
				s_Data.currentDirectory->CreateNewFile("File" + String::S32ToString(s_Data.currentDirectory->files.Size() + 1) + ".eumat");
				s_Data.fileModify = ASSET_MODIFY_RENAME;
				s_Data.assetModifyIndex = s_Data.currentDirectory->files.Size() - 1;
				ImGui::CloseCurrentPopup();
			} ImGui::Separator();
			if (ImGui::Selectable("+ New Material Modifier"))
			{
				s_Data.currentDirectory->CreateNewFile("File" + String::S32ToString(s_Data.currentDirectory->files.Size() + 1) + ".eummod");
				s_Data.fileModify = ASSET_MODIFY_RENAME;
				s_Data.assetModifyIndex = s_Data.currentDirectory->files.Size() - 1;
			} ImGui::Separator();
			if (ImGui::Selectable("+ New Scene"))
			{
				s_Data.currentDirectory->CreateNewFile("File" + String::S32ToString(s_Data.currentDirectory->files.Size() + 1) + ".euscene");
				s_Data.fileModify = ASSET_MODIFY_RENAME;
				s_Data.assetModifyIndex = s_Data.currentDirectory->files.Size() - 1;

				ECSLoadedScene loadedScene;
				loadedScene.name = "New Scene";
				
				ECSLoadedEntity root;
				root.name = "Root";
				root.enabled = true;
				root.numChildren = 0;
				
				loadedScene.entities.Push(root);
			}
			if (ImGui::Selectable("Load Resources"))
			{
				for (u32 i = 0; i < s_Data.currentDirectory->files.Size(); i++)
				{
					if (s_Data.currentDirectory->files[i].extension == "eutex")
						AssetManager::CreateTexture(s_Data.currentDirectory->files[i].path);
					else if (s_Data.currentDirectory->files[i].extension == "eumat")
					{
						LoadedMaterialFile loadedMaterialFile;
						MaterialLoader::LoadEumtlMaterial(s_Data.currentDirectory->files[i].path, &loadedMaterialFile);
						MaterialID mid;
						AssetManager::CreateMaterials(loadedMaterialFile, EU_SAMPLER_LINEAR_REPEAT_AF, &mid);
					}
					else if (s_Data.currentDirectory->files[i].extension == "eumdl")
					{					
						ModelID mid = AssetManager::CreateModel(s_Data.currentDirectory->files[i].path);
						
					}
				}
			}
			ImGui::EndPopup();
		}
		if (ImGui::BeginPopup("Modify Directory"))
		{
			if (ImGui::Selectable("Rename"))
			{
				s_Data.dirModify = ASSET_MODIFY_RENAME;
				ImGui::CloseCurrentPopup();
			} ImGui::Separator();
			if (ImGui::Selectable("Delete"))
			{
				s_Data.dirModify = ASSET_MODIFY_DELETE;
				ImGui::CloseCurrentPopup();
			}
			ImGui::EndPopup();
		}
		if (ImGui::BeginPopup("Modify File"))
		{
			if (ImGui::Selectable("Rename"))
			{
				s_Data.fileModify = ASSET_MODIFY_RENAME;
				ImGui::CloseCurrentPopup();
			} ImGui::Separator();
			if (ImGui::Selectable("Delete"))
			{
				s_Data.fileModify = ASSET_MODIFY_DELETE;
				ImGui::CloseCurrentPopup();
			}
			ImGui::EndPopup();
		}
		if (ImGui::BeginPopupModal("Select Key"))
		{
			ImGui::Text("Waiting for key selection...");

			if (!s_Data.waitForKeySelection)
				ImGui::CloseCurrentPopup();

			ImGui::EndPopup();
		}
		if (ImGui::BeginPopup("Entity Right Clicked"))
		{
			ECS* ecs = ProjectManager::GetProject()->application->GetECS();

			if (ImGui::Selectable("Add Child Entity"))
			{
				ecs->CreateEntity(s_Data.selectedEntity);
			} ImGui::Separator();
			if (ImGui::BeginMenu("Add Camera Entity"))
			{
				EntityID camera = ecs->CreateEntity(s_Data.selectedEntity);
				ecs->CreateComponent<CameraComponent>(camera);
				ecs->CreateComponent<Transform3DComponent>(camera);
				if (ImGui::Selectable("Keyboard/Mouse##CameraKeyboardMouseControl"))
				{
					ecs->CreateComponent<KeyboardMovement3DComponent>(camera);
					ecs->CreateComponent<MouseLookAround3DComponent>(camera);
				} ImGui::Separator();
				if (ImGui::Selectable("Gamepad##CameraGamepadControl"))
				{
					ecs->CreateComponent<Gamepad3DComponent>(camera);
				}
				
				ImGui::EndMenu();
			}
			if (ImGui::BeginMenu("Add Model Entity"))
			{
				EntityID ent = ecs->CreateEntity();
				ecs->CreateComponent<Transform3DComponent>(ent);
				if (ImGui::Selectable("Empty Material Model"))
				{
					ecs->CreateComponent<ModelComponent>(ent);
					ecs->CreateComponent<MaterialComponent>(ent);
				}

				ImGui::EndMenu();
			}
			ImGui::EndPopup();
		}
		if (ImGui::BeginPopupModal("Editor Settings"))
		{
			if (ImGui::Button("Save##SaveEditorSettingsButton"))
			{
				ImGui::CloseCurrentPopup();
			} ImGui::SameLine();
			if (ImGui::Button("Cancel##CancelSaveEditorSettingsButton"))
			{
				ImGui::CloseCurrentPopup();
			}

			ImGui::EndPopup();
		}
	}

	void EditorGUI::DrawMetadata(const MetadataInfo& metadata, const u8* data, const String& idString)
	{
		if (metadata.type == METADATA_CLASS)
		{
			MetadataClass* cls = metadata.cls;

			ImGui::BeginColumns((cls->name + "Colomns").C_Str(), 2);
			DrawMetadataHelper(metadata, data, idString);
			ImGui::EndColumns();
		}
	}

	void EditorGUI::DrawMetadataHelper(const MetadataInfo& metadata, const u8* data, const String& idString)
	{
		MetadataClass* cls = metadata.cls;

		for (u32 i = 0; i < cls->members.Size(); i++)
		{
			String newIDString = idString.Empty() ? "##" + cls->name + String::S32ToString(i) : idString + cls->name + String::S32ToString(i);
			DrawMetadataMember(cls, i, data, newIDString);
		}
	}

	void EditorGUI::DrawMetadataMember(MetadataClass* cls, u32 memberIndex, const u8* data, const String& idString, u32 indent)
	{
		//ImGui::Indent(indent);
		
		const MetadataMember& member = cls->members[memberIndex];
		String memberName = member.name;
		const MetadataInfo& metadata = Metadata::GetMetadata(member.typeID);
		const u8* offsetedData = data + member.offset;

		if (metadata.type == METADATA_PRIMITIVE)
		{
			String newIDString = idString + member.name;

			if (member.typeName == "TextureID")
			{
				ImGui::Text(memberName.C_Str()); ImGui::SameLine();
				ImGui::NextColumn();
				TextureID* texID = (TextureID*)offsetedData;
				if (texID != EU_INVALID_TEXTURE_ID)
				{
					ImGui::Image(*texID, ImVec2(32, 32));
				}
				else
				{
					ImGui::ColorButton((newIDString + "ColorButton").C_Str(), ImVec4(1.0f, 1.0f, 1.0f, 1.0f), 0, ImVec2(32, 32));
				}

				if (ImGui::BeginDragDropTarget())
				{
					const ImGuiPayload* assetFile = ImGui::AcceptDragDropPayload("AssetFile");
					if (assetFile)
					{
						u32 assetFileIndex = *(u32*)assetFile->Data;
						const EUFile& droppedFile = s_Data.currentDirectory->files[assetFileIndex];
						if (droppedFile.extension == "eutex")
						{
							u32 w, h;
							*texID = Engine::GetRenderContext()->CreateTexture2D(droppedFile.path);
						}
					}
					ImGui::EndDragDropTarget();
				}
			}
			else if (member.typeName == "MaterialID")
			{

				ImGui::PushItemWidth(75);
				const List<Material>& materials= AssetManager::GetMaterialList();
				MaterialID selectedMaterial = *(MaterialID*)offsetedData;
				ImGui::Text(memberName.C_Str()); ImGui::SameLine();
				ImGui::NextColumn();
				if (ImGui::BeginCombo(newIDString.C_Str(), materials[selectedMaterial - 1].name.C_Str()))
				{
					for (u32 i = 0; i < materials.Size(); i++)
					{
						if (ImGui::Selectable(materials[i].name.C_Str()))
							*(MaterialID*)offsetedData = (i + 1);
					}
					ImGui::EndCombo();
				}
				ImGui::NextColumn();
				ImGui::PopItemWidth();
			}
			else if (member.typeName == "ModelID")
			{
				ImGui::PushItemWidth(75);
				const List<Model>& models = AssetManager::GetModelList();
				ModelID selectedModel = *(ModelID*)offsetedData;
				String modelPath = AssetManager::GetModelPath(selectedModel);
				String modelName = modelPath.SubString(modelPath.FindLastOf("/") + 1);
				ImGui::Text(memberName.C_Str()); ImGui::SameLine();
				ImGui::NextColumn();
				if (ImGui::BeginCombo(newIDString.C_Str(), modelName.C_Str()))
				{
					for (u32 i = 0; i < models.Size(); i++)
					{
						modelPath = AssetManager::GetModelPath(i + 1);
						modelName = modelPath.SubString(modelPath.FindLastOf("/") + 1);
						if(ImGui::Selectable(modelName.C_Str()))
							*(ModelID*)offsetedData = (i + 1);
					}
					ImGui::EndCombo();
				}
				ImGui::NextColumn();
			}
			else if (member.typeName == "MaterialModifierID")
			{
				ImGui::PushItemWidth(75);
				const List<LoadedMaterialModifier>& modifiers = AssetManager::GetMaterialModifierList();
				MaterialModifierID selectedModifier = *(MaterialModifierID*)offsetedData;
				ImGui::Text(memberName.C_Str()); ImGui::SameLine();
				ImGui::NextColumn();
				if (ImGui::BeginCombo(newIDString.C_Str(), modifiers[selectedModifier - 1].name.C_Str()))
				{
					for (u32 i = 0; i < modifiers.Size(); i++)
					{
						if (ImGui::Selectable(modifiers[i].name.C_Str()))
							*(MaterialModifierID*)offsetedData = (i + 1);
					}
					ImGui::EndCombo();
				}
				ImGui::NextColumn();
				ImGui::PopItemWidth();
			}
			else
			{
				ImGui::PushItemWidth(75);
				const MetadataPrimitive* primitive = metadata.primitive;
				switch (primitive->type)
				{
				case METADATA_PRIMITIVE_BOOL: {
					bool* value = (bool*)offsetedData;
					ImGui::Text(memberName.C_Str()); ImGui::SameLine();
					ImGui::NextColumn();
					ImGui::Checkbox(newIDString.C_Str(), value);
					ImGui::NextColumn();
				} break;
				case METADATA_PRIMITIVE_R32: {
					r32* value = (r32*)offsetedData;
					ImGui::Text(memberName.C_Str()); ImGui::SameLine();
					ImGui::NextColumn();
					ImGui::DragFloat(newIDString.C_Str(), value, cls->members[memberIndex].uiSliderSpeed, cls->members[memberIndex].uiSliderMin.x, cls->members[memberIndex].uiSliderMax.x);
					ImGui::NextColumn();
				} break;
				case METADATA_PRIMITIVE_R64: {
					r64* value = (r64*)offsetedData;
					ImGui::Text(memberName.C_Str()); ImGui::SameLine();
					ImGui::NextColumn();
					ImGui::DragFloat(newIDString.C_Str(), (r32*)value, cls->members[memberIndex].uiSliderSpeed, cls->members[memberIndex].uiSliderMin.x, cls->members[memberIndex].uiSliderMax.x);
					ImGui::NextColumn();
				} break;
				case METADATA_PRIMITIVE_U8: {
					u8* value = (u8*)offsetedData;
					ImGui::Text(memberName.C_Str()); ImGui::SameLine();
					ImGui::NextColumn();
					ImGui::DragInt(newIDString.C_Str(), (s32*)value, cls->members[memberIndex].uiSliderSpeed, EU_MAX(cls->members[memberIndex].uiSliderMin.x, 0), EU_MIN(cls->members[memberIndex].uiSliderMax.x, EU_U8_MAX));
					ImGui::NextColumn();
				} break;
				case METADATA_PRIMITIVE_U16: {
					u16* value = (u16*)offsetedData;
					ImGui::Text(memberName.C_Str()); ImGui::SameLine();
					ImGui::NextColumn();
					ImGui::DragInt(newIDString.C_Str(), (s32*)value, cls->members[memberIndex].uiSliderSpeed, EU_MAX(cls->members[memberIndex].uiSliderMin.x, 0), EU_MIN(cls->members[memberIndex].uiSliderMax.x, EU_U16_MAX));
					ImGui::NextColumn();
				} break;
				case METADATA_PRIMITIVE_U32: {
					u32* value = (u32*)offsetedData;
					ImGui::Text(memberName.C_Str()); ImGui::SameLine();
					ImGui::NextColumn();
					if (member.is32BitBool)
						ImGui::Checkbox(newIDString.C_Str(), (bool*)value);
					else
						ImGui::DragInt(newIDString.C_Str(), (s32*)value, cls->members[memberIndex].uiSliderSpeed, EU_MAX(cls->members[memberIndex].uiSliderMin.x, 0), EU_MIN(cls->members[memberIndex].uiSliderMax.x, EU_U32_MAX));
				
					ImGui::NextColumn();
				} break;
				case METADATA_PRIMITIVE_S8: {
					s8* value = (s8*)offsetedData;
					ImGui::Text(memberName.C_Str()); ImGui::SameLine();
					ImGui::NextColumn();
					ImGui::DragInt(newIDString.C_Str(), (s32*)value, cls->members[memberIndex].uiSliderSpeed, EU_MAX(cls->members[memberIndex].uiSliderMin.x, 0), EU_MIN(cls->members[memberIndex].uiSliderMax.x, EU_S8_MAX));
					ImGui::NextColumn();
				} break;
				case METADATA_PRIMITIVE_S16: {
					s16* value = (s16*)offsetedData;
					ImGui::Text(memberName.C_Str()); ImGui::SameLine();
					ImGui::NextColumn();
					ImGui::DragInt(newIDString.C_Str(), (s32*)value, cls->members[memberIndex].uiSliderSpeed, EU_MAX(cls->members[memberIndex].uiSliderMin.x, 0), EU_MIN(cls->members[memberIndex].uiSliderMax.x, EU_S16_MAX));
					ImGui::NextColumn();
				} break;
				case METADATA_PRIMITIVE_S32: {
					s32* value = (s32*)offsetedData;
					ImGui::Text(memberName.C_Str()); ImGui::SameLine();
					ImGui::NextColumn();
					ImGui::DragInt(newIDString.C_Str(), (s32*)value, cls->members[memberIndex].uiSliderSpeed, EU_MAX(cls->members[memberIndex].uiSliderMin.x, 0), EU_MIN(cls->members[memberIndex].uiSliderMax.x, EU_S32_MAX));
					ImGui::NextColumn();
				} break;
				}
				ImGui::PopItemWidth();
			}
		}
		else if (metadata.type == METADATA_CLASS)
		{
			MetadataClass* memberClass = metadata.cls;

			String newIDString = idString + memberClass->name;

			if (cls->members[memberIndex].typeID == Metadata::GetTypeID<v2>())
			{
				ImGui::EndColumns();
				ImGui::BeginColumns((newIDString + "Columns").C_Str(), 3);
				ImGui::PushItemWidth(75);
				r32* xValue = (r32*)(offsetedData + memberClass->members[0].offset);
				r32* yValue = (r32*)(offsetedData + memberClass->members[1].offset);
				ImGui::Text((memberName + " ").C_Str()); ImGui::SameLine();
				ImGui::NextColumn();
				ImGui::Text("X"); ImGui::SameLine();
				ImGui::DragFloat((newIDString + "x").C_Str(), xValue, cls->members[memberIndex].uiSliderSpeed, cls->members[memberIndex].uiSliderMin.x, cls->members[memberIndex].uiSliderMax.x);
				ImGui::NextColumn();
				ImGui::Text("Y"); ImGui::SameLine();
				ImGui::DragFloat((newIDString + "y").C_Str(), yValue, cls->members[memberIndex].uiSliderSpeed, cls->members[memberIndex].uiSliderMin.y, cls->members[memberIndex].uiSliderMax.y);
				ImGui::PopItemWidth();
				ImGui::EndColumns();
				ImGui::BeginColumns((newIDString + "Columns").C_Str(), 2);
			}
			else if (cls->members[memberIndex].typeID == Metadata::GetTypeID<v3>())
			{
				ImGui::EndColumns();
				ImGui::BeginColumns((newIDString + "Columns").C_Str(), 4);
				ImGui::PushItemWidth(75);
				r32* xValue = (r32*)(offsetedData + memberClass->members[0].offset);
				r32* yValue = (r32*)(offsetedData + memberClass->members[1].offset);
				r32* zValue = (r32*)(offsetedData + memberClass->members[2].offset);
				ImGui::Text((memberName + " ").C_Str()); ImGui::SameLine();
				ImGui::NextColumn();
				ImGui::Text("X"); ImGui::SameLine();
				ImGui::DragFloat((newIDString + "x").C_Str(), xValue, cls->members[memberIndex].uiSliderSpeed, cls->members[memberIndex].uiSliderMin.x, cls->members[memberIndex].uiSliderMax.x);
				ImGui::NextColumn();
				ImGui::Text("Y"); ImGui::SameLine();
				ImGui::DragFloat((newIDString + "y").C_Str(), yValue, cls->members[memberIndex].uiSliderSpeed, cls->members[memberIndex].uiSliderMin.y, cls->members[memberIndex].uiSliderMax.y);
				ImGui::NextColumn();
				ImGui::Text("Z"); ImGui::SameLine();
				ImGui::DragFloat((newIDString + "z").C_Str(), zValue, cls->members[memberIndex].uiSliderSpeed, cls->members[memberIndex].uiSliderMin.z, cls->members[memberIndex].uiSliderMax.z);
				ImGui::PopItemWidth();
				ImGui::EndColumns();
				ImGui::BeginColumns((newIDString + "Columns").C_Str(), 2);
			}
			else if (cls->members[memberIndex].typeID == Metadata::GetTypeID<v4>())
			{
				ImGui::EndColumns();
				ImGui::BeginColumns((newIDString + "Columns").C_Str(), 5);
				ImGui::PushItemWidth(75);
				r32* xValue = (r32*)(offsetedData + memberClass->members[0].offset);
				r32* yValue = (r32*)(offsetedData + memberClass->members[1].offset);
				r32* zValue = (r32*)(offsetedData + memberClass->members[2].offset);
				r32* wValue = (r32*)(offsetedData + memberClass->members[3].offset);

				ImGui::Text((memberName + " ").C_Str()); ImGui::SameLine();
				ImGui::NextColumn();
				ImGui::Text("X"); ImGui::SameLine();
				ImGui::DragFloat((newIDString + "x").C_Str(), xValue, cls->members[memberIndex].uiSliderSpeed, cls->members[memberIndex].uiSliderMin.x, cls->members[memberIndex].uiSliderMax.x);
				ImGui::NextColumn();
				ImGui::Text("Y"); ImGui::SameLine();
				ImGui::DragFloat((newIDString + "y").C_Str(), yValue, cls->members[memberIndex].uiSliderSpeed, cls->members[memberIndex].uiSliderMin.y, cls->members[memberIndex].uiSliderMax.y);
				ImGui::NextColumn();
				ImGui::Text("Z"); ImGui::SameLine();
				ImGui::DragFloat((newIDString + "z").C_Str(), zValue, cls->members[memberIndex].uiSliderSpeed, cls->members[memberIndex].uiSliderMin.z, cls->members[memberIndex].uiSliderMax.z);
				ImGui::NextColumn();
				ImGui::Text("W"); ImGui::SameLine();
				ImGui::DragFloat((newIDString + "w").C_Str(), wValue, cls->members[memberIndex].uiSliderSpeed, cls->members[memberIndex].uiSliderMin.w, cls->members[memberIndex].uiSliderMax.w);
				ImGui::PopItemWidth();
				ImGui::EndColumns();
				ImGui::BeginColumns((newIDString + "Columns").C_Str(), 2);
			}
			else if (cls->members[memberIndex].typeID == Metadata::GetTypeID<quat>())
			{
				ImGui::EndColumns();
				ImGui::BeginColumns((newIDString + "Columns").C_Str(), 4);
				ImGui::PushItemWidth(75);
				ImGui::PushItemWidth(75);
				quat* q = (quat*)(offsetedData);
				
				v3 euler = q->ToEulerRotation();
				euler *= (360.0f / EU_2PI);

				ImGui::Text((memberName + " ").C_Str());
				ImGui::NextColumn();
				ImGui::Text("X"); ImGui::SameLine();
				ImGui::InputFloat((newIDString + "x").C_Str(), &euler.x, 0.0f, 0.0f, 2, ImGuiInputTextFlags_ReadOnly);
				ImGui::NextColumn();
				ImGui::Text("Y"); ImGui::SameLine();
				ImGui::InputFloat((newIDString + "y").C_Str(), &euler.y, 0.0f, 0.0f, 2, ImGuiInputTextFlags_ReadOnly); 
				ImGui::NextColumn();
				ImGui::Text("Z"); ImGui::SameLine();
				ImGui::InputFloat((newIDString + "z").C_Str(), &euler.z, 0.0f, 0.0f, 2, ImGuiInputTextFlags_ReadOnly);
				ImGui::EndColumns();
				ImGui::BeginColumns((newIDString + "Columns").C_Str(), 2);
			}
			else if (member.typeID == Metadata::GetTypeID<String>())
			{

			}
			else if (member.typeID == Metadata::GetTypeID<RigidBody>())
			{
				ImGui::PushItemWidth(75);
				RigidBody* rigidBody = (RigidBody*)(offsetedData);

				if (rigidBody->WasRigidBodyCreated())
				{
					r32 mass = rigidBody->GetMass();
					r32 initialMass = mass;

					r32 friction = rigidBody->GetFriction();
					r32 initialFriction = friction;

					v3 localInertia = rigidBody->GetLocalInertia();
					v3 initialLocalInertia = localInertia;

					ImGui::Text("Mass"); ImGui::SameLine();
					ImGui::DragFloat("##ViewRigidBodyCompnentMass", &mass, 0.1f, 0.0f, 100.0f);
					ImGui::Text("Friction"); ImGui::SameLine();
					ImGui::DragFloat("##ViewRigidBodyComponentFriction", &friction, 0.1f, 0.0f, 100.0f);
					ImGui::Text("Local Inertia"); ImGui::SameLine();
					ImGui::Text("X"); ImGui::SameLine(); ImGui::DragFloat("##ViewRigidBodyComponentLocalInertiaX", &localInertia.x, 0.01f, 0.0f, 1.0f); ImGui::SameLine();
					ImGui::Text("Y"); ImGui::SameLine(); ImGui::DragFloat("##ViewRigidBodyComponentLocalInertiaY", &localInertia.y, 0.01f, 0.0f, 1.0f); ImGui::SameLine();
					ImGui::Text("Z"); ImGui::SameLine(); ImGui::DragFloat("##ViewRigidBodyComponentLocalInertiaZ", &localInertia.z, 0.01f, 0.0f, 1.0f); ImGui::SameLine();
					if (ImGui::Button("Calculate##ViewRigidBodyComonentLocalInertiaCalculateButton"))
					{
						btVector3 btLocalInetia;
						rigidBody->GetRigidBody()->getCollisionShape()->calculateLocalInertia(rigidBody->GetMass(), btLocalInetia);
						rigidBody->SetLocalInertia(PhysicsEngine3D::ToEngineVector(btLocalInetia));
					}

					if (mass != initialMass)
						rigidBody->SetMass(mass);
					if (friction != initialFriction)
						rigidBody->SetFriction(friction);
					if (localInertia != initialLocalInertia)
						rigidBody->SetLocalInertia(localInertia);
				}

				MetadataEnum* shapeTypeEnumMetadata = Metadata::GetMetadata(Metadata::GetTypeID<RigidBodyShapeType>()).enm;

				EU_PERSISTENT RigidBodyShapeType SelectedShapeType = RIGID_BODY_SHAPE_STATIC_PLANE;

				String previewValue;
				for (u32 i = 0; i < shapeTypeEnumMetadata->values.Size(); i++)
				{
					if (shapeTypeEnumMetadata->values[i].value == SelectedShapeType)
					{
						previewValue = shapeTypeEnumMetadata->values[i].name;
						break;
					}
				}
				ImGui::PushItemWidth(100);
				if (ImGui::BeginCombo("##ViewRigidBodyComponentShapeTypeDropDown", previewValue.C_Str()))
				{
					for (u32 i = 0; i < shapeTypeEnumMetadata->values.Size(); i++)
					{
						if (ImGui::Selectable((shapeTypeEnumMetadata->values[i].name + "##ViewRigidBodyComponentShapeTypeOption" + shapeTypeEnumMetadata->values[i].name + String::S32ToString(i)).C_Str()))
						{
							SelectedShapeType = (RigidBodyShapeType)shapeTypeEnumMetadata->values[i].value;
						}
					}
					ImGui::EndCombo();
				}
				ImGui::PopItemWidth();

				ImGui::SameLine();
				if (ImGui::Button("Add Shape Type"))
				{
					switch (SelectedShapeType)
					{
					case RIGID_BODY_SHAPE_STATIC_PLANE: {
						rigidBody->AddStaticPlaneShape(v3(0.0f, 1.0f, 0.0f), 0.0f);
					} break;
					case RIGID_BODY_SHAPE_BOX: {
						rigidBody->AddBoxShape(v3(1.0f, 1.0f, 1.0f));
					} break;
					case RIGID_BODY_SHAPE_SPHERE: {
						rigidBody->AddSphereShape(1.0f);
					} break;
					}
				}



				if (rigidBody->WasRigidBodyCreated())
				{
					btCompoundShape* shapes = (btCompoundShape*)rigidBody->GetRigidBody()->getCollisionShape();

					ImGui::Text("Created Collision Shapes");

					if (ImGui::ListBoxHeader("##ViewRigidBodyComponentCreatedCollisionShapes", ImVec2(120, 25 * shapes->getNumChildShapes())))
					{
						u32 staticPlaneNumber = 1;
						u32 sphereNumber = 1;
						u32 boxNumber = 1;

						String shapeName;
						for (u32 i = 0; i < shapes->getNumChildShapes(); i++)
						{
							switch (shapes->getChildShape(i)->getShapeType())
							{
							case STATIC_PLANE_PROXYTYPE: shapeName = "Static Plane" + (staticPlaneNumber > 1 ? String::S32ToString(staticPlaneNumber++) : ""); break;
							case SPHERE_SHAPE_PROXYTYPE: shapeName = "Sphere" + (sphereNumber > 1 ? String::S32ToString(sphereNumber++) : ""); break;
							case BOX_SHAPE_PROXYTYPE: shapeName = "Box" + (boxNumber > 1 ? String::S32ToString(boxNumber++) : ""); break;
							}

							if (ImGui::Selectable((shapeName + "##ViewRigidBodyComponentCreatedCollisionShapeSelectable" + String::S32ToString(i)).C_Str(), s_Data.selectedCollisionShape == i))
							{
								s_Data.selectedCollisionShape = i;
							}
						}
						ImGui::ListBoxFooter();
					}

					if (s_Data.selectedCollisionShape != -1)
					{
						btCollisionShape* shape = shapes->getChildShape(s_Data.selectedCollisionShape);

						switch (shape->getShapeType())
						{
						case STATIC_PLANE_PROXYTYPE: {

							btStaticPlaneShape* plane = (btStaticPlaneShape*)shape;
							btVector3 planeNormalBt = plane->getPlaneNormal();
							v3 planeNormal = PhysicsEngine3D::ToEngineVector(planeNormalBt);
							r32 planeConstant = plane->getPlaneConstant();
							v3 initialPlaneNormal = planeNormal;
							r32 initialPlaneConstant = planeConstant;

							ImGui::Text("Plane Constant"); ImGui::SameLine();
							ImGui::DragFloat("##ViewRigidBodyComponentStaticPlaneConstant", &planeConstant, 0.1f, 0.0f, 100.0f);
							ImGui::Text("Plane Normal");
							ImGui::DragFloat("##ViewRigidBodyComponentStaticPlaneNormalX", &planeNormal.x, 0.01f, 0.0f, 1.0f); ImGui::SameLine();
							ImGui::DragFloat("##ViewRigidBodyComponentStaticPlaneNormalY", &planeNormal.y, 0.01f, 0.0f, 1.0f); ImGui::SameLine();
							ImGui::DragFloat("##ViewRigidBodyComponentStaticPlaneNormalZ", &planeNormal.z, 0.01f, 0.0f, 1.0f);

							if (planeNormal != initialPlaneNormal)
							{
								rigidBody->RemoveFromPhysicsWorld();
								RigidBodyInfo rigidBodyInfo;
								rigidBody->GetSerializableInfo(&rigidBodyInfo);
								rigidBody->DestroyRigidBody();
								RigidBodyShapeInfo* shapeInfo = &rigidBodyInfo.shapes[s_Data.selectedCollisionShape];

								shapeInfo->info.x = planeNormal.x;
								shapeInfo->info.y = planeNormal.y;
								shapeInfo->info.z = planeNormal.z;

								rigidBody->CreateRigidBodyFromInfo(rigidBodyInfo);
							}

							if (planeConstant != initialPlaneConstant)
							{
								rigidBody->RemoveFromPhysicsWorld();
								RigidBodyInfo rigidBodyInfo;
								rigidBody->GetSerializableInfo(&rigidBodyInfo);
								rigidBody->DestroyRigidBody();
								RigidBodyShapeInfo* shapeInfo = &rigidBodyInfo.shapes[s_Data.selectedCollisionShape];

								shapeInfo->info.w = planeConstant;

								rigidBody->CreateRigidBodyFromInfo(rigidBodyInfo);

							}
						} break;
						case SPHERE_SHAPE_PROXYTYPE: {

							btSphereShape* sphere = (btSphereShape*)shape;
							r32 radius = sphere->getRadius();
							r32 initialRadius = radius;
							ImGui::Text("Radius"); ImGui::SameLine();
							ImGui::DragFloat("##ViewRigidBodyComponentSphereRadius", &radius, 0.01f, 0.0f, 100.0f);

							const btTransform& localTransform = shapes->getChildTransform(s_Data.selectedCollisionShape);
							v3 localPos = PhysicsEngine3D::ToEngineVector(localTransform.getOrigin());
							quat localRot = PhysicsEngine3D::ToEngineQuat(localTransform.getRotation());
							v3 initialLocalPos = localPos;
							quat initialLocalRot = localRot;

							ImGui::Text("Local Position");
							ImGui::Text("X"); ImGui::SameLine(); ImGui::DragFloat("##ViewRigidBodyComponentStaticPlaneLocalPosX", &localPos.x, 0.01f, 0.0f, 100.0f); ImGui::SameLine();
							ImGui::Text("Y"); ImGui::SameLine(); ImGui::DragFloat("##ViewRigidBodyComponentStaticPlaneLocalPosY", &localPos.y, 0.01f, 0.0f, 100.0f); ImGui::SameLine();
							ImGui::Text("Z"); ImGui::SameLine(); ImGui::DragFloat("##ViewRigidBodyComponentStaticPlaneLocalPosZ", &localPos.z, 0.01f, 0.0f, 100.0f);

							ImGui::Text("Local Rotation");
							ImGui::Text("X"); ImGui::SameLine(); ImGui::DragFloat("##ViewRigidBodyComponentStaticPlaneLocalRotX", &localRot.x, 0.01f, 0.0f, 100.0f); ImGui::SameLine();
							ImGui::Text("Y"); ImGui::SameLine(); ImGui::DragFloat("##ViewRigidBodyComponentStaticPlaneLocalRotY", &localRot.y, 0.01f, 0.0f, 100.0f); ImGui::SameLine();
							ImGui::Text("Z"); ImGui::SameLine(); ImGui::DragFloat("##ViewRigidBodyComponentStaticPlaneLocalRotZ", &localRot.z, 0.01f, 0.0f, 100.0f); ImGui::SameLine();
							ImGui::Text("W"); ImGui::SameLine(); ImGui::DragFloat("##ViewRigidBodyComponentStaticPlaneLocalRotW", &localRot.w, 0.01f, 0.0f, 100.0f);

							if (localPos != initialLocalPos)
							{
								rigidBody->RemoveFromPhysicsWorld();
								RigidBodyInfo rigidBodyInfo;
								rigidBody->GetSerializableInfo(&rigidBodyInfo);
								rigidBody->DestroyRigidBody();

								RigidBodyShapeInfo* shapeInfo = &rigidBodyInfo.shapes[s_Data.selectedCollisionShape];
								shapeInfo->localTransform.setOrigin(PhysicsEngine3D::ToBulletVector(localPos));

								rigidBody->CreateRigidBodyFromInfo(rigidBodyInfo);
							}

							if (localRot != initialLocalRot)
							{

							}

							if (radius != initialRadius)
							{
								rigidBody->RemoveFromPhysicsWorld();
								RigidBodyInfo rigidBodyInfo;
								rigidBody->GetSerializableInfo(&rigidBodyInfo);
								rigidBody->DestroyRigidBody();
								RigidBodyShapeInfo* shapeInfo = &rigidBodyInfo.shapes[s_Data.selectedCollisionShape];

								shapeInfo->info.x = radius;
								rigidBody->CreateRigidBodyFromInfo(rigidBodyInfo);
							}
						} break;
						case BOX_SHAPE_PROXYTYPE: {

							btBoxShape* box = (btBoxShape*)shape;
							v3 halfExtents = PhysicsEngine3D::ToEngineVector(box->getHalfExtentsWithMargin());
							v3 initialHalfExtents = halfExtents;

							const btTransform& localTransform = shapes->getChildTransform(s_Data.selectedCollisionShape);
							v3 localPos = PhysicsEngine3D::ToEngineVector(localTransform.getOrigin());
							quat localRot = PhysicsEngine3D::ToEngineQuat(localTransform.getRotation());
							v3 initialLocalPos = localPos;
							quat initialLocalRot = localRot;

							ImGui::Text("Half Extents");
							ImGui::Text("X"); ImGui::SameLine(); ImGui::DragFloat("##ViewRigidBodyComponentBoxHalfExtentsX", &halfExtents.x, 0.01f, 0.0f, 100.0f); ImGui::SameLine();
							ImGui::Text("Y"); ImGui::SameLine(); ImGui::DragFloat("##ViewRigidBodyComponentBoxHalfExtentsY", &halfExtents.y, 0.01f, 0.0f, 100.0f); ImGui::SameLine();
							ImGui::Text("Z"); ImGui::SameLine(); ImGui::DragFloat("##ViewRigidBodyComponentBoxHalfExtentsZ", &halfExtents.z, 0.01f, 0.0f, 100.0f);

							ImGui::Text("Local Position");
							ImGui::Text("X"); ImGui::SameLine(); ImGui::DragFloat("##ViewRigidBodyComponentStaticPlaneLocalPosX", &localPos.x, 0.01f, 0.0f, 100.0f); ImGui::SameLine();
							ImGui::Text("Y"); ImGui::SameLine(); ImGui::DragFloat("##ViewRigidBodyComponentStaticPlaneLocalPosY", &localPos.y, 0.01f, 0.0f, 100.0f); ImGui::SameLine();
							ImGui::Text("Z"); ImGui::SameLine(); ImGui::DragFloat("##ViewRigidBodyComponentStaticPlaneLocalPosZ", &localPos.z, 0.01f, 0.0f, 100.0f);

							ImGui::Text("Local Rotation");
							ImGui::Text("X"); ImGui::SameLine(); ImGui::DragFloat("##ViewRigidBodyComponentStaticPlaneLocalRotX", &localRot.x, 0.01f, 0.0f, 100.0f); ImGui::SameLine();
							ImGui::Text("Y"); ImGui::SameLine(); ImGui::DragFloat("##ViewRigidBodyComponentStaticPlaneLocalRotY", &localRot.y, 0.01f, 0.0f, 100.0f); ImGui::SameLine();
							ImGui::Text("Z"); ImGui::SameLine(); ImGui::DragFloat("##ViewRigidBodyComponentStaticPlaneLocalRotZ", &localRot.z, 0.01f, 0.0f, 100.0f); ImGui::SameLine();
							ImGui::Text("W"); ImGui::SameLine(); ImGui::DragFloat("##ViewRigidBodyComponentStaticPlaneLocalRotW", &localRot.w, 0.01f, 0.0f, 100.0f);

							if (localPos != initialLocalPos)
							{
								rigidBody->RemoveFromPhysicsWorld();
								RigidBodyInfo rigidBodyInfo;
								rigidBody->GetSerializableInfo(&rigidBodyInfo);
								rigidBody->DestroyRigidBody();

								RigidBodyShapeInfo* shapeInfo = &rigidBodyInfo.shapes[s_Data.selectedCollisionShape];
								shapeInfo->localTransform.setOrigin(PhysicsEngine3D::ToBulletVector(localPos));

								rigidBody->CreateRigidBodyFromInfo(rigidBodyInfo);
							}

							if (localRot != initialLocalRot)
							{

							}

							if (halfExtents != initialHalfExtents)
							{
								rigidBody->RemoveFromPhysicsWorld();
								RigidBodyInfo rigidBodyInfo;
								rigidBody->GetSerializableInfo(&rigidBodyInfo);
								rigidBody->DestroyRigidBody();
								RigidBodyShapeInfo* shapeInfo = &rigidBodyInfo.shapes[s_Data.selectedCollisionShape];

								shapeInfo->info.x = halfExtents.x;
								shapeInfo->info.y = halfExtents.y;
								shapeInfo->info.z = halfExtents.z;

								rigidBody->CreateRigidBodyFromInfo(rigidBodyInfo);
							}
						} break;
						}
					}

					ImGui::PopItemWidth();
				}
			}
			else
			{
				if (ImGui::TreeNodeEx((memberName + newIDString + "DropDown").C_Str(), ImGuiTreeNodeFlags_DefaultOpen))
				{
					ImGui::EndColumns();
					DrawMetadata(metadata, offsetedData, idString);
					ImGui::TreePop();
				}
				ImGui::BeginColumns((newIDString + "Columns").C_Str(), 2);
			}
		}
		else if (metadata.type == METADATA_ENUM)
		{
			MetadataEnum* enm = Metadata::GetMetadata(member.typeID).enm;
			String newIDString = idString + enm->name;

			if (member.typeID == Metadata::GetTypeID<Key>())
			{
				Key currentValue = *(Key*)offsetedData;
				ImGui::Text(memberName.C_Str()); ImGui::SameLine();
				ImGui::NextColumn();

				for (u32 i = 0; i < enm->values.Size(); i++)
				{
					if (enm->values[i].value == currentValue)
					{
						if (ImGui::Button((enm->values[i].name + "##SelectKeyValue" + enm->values[i].name).C_Str()))
						{
							s_Data.waitForKeySelection = true;
							s_Data.keyValueToSet = (Key*)offsetedData;
							s_Data.openPopup = "Select Key";
						}

						ImGui::NextColumn();
						break;
					}
				}
			}
			else
			{
				MetadataEnum* enm = Metadata::GetMetadata(member.typeID).enm;

				u32 currentValue = *(u32*)offsetedData;
				u32 currentValueIndex = 0;
				for (u32 i = 0; i < enm->values.Size(); i++)
					if (enm->values[i].value == currentValue)
						currentValueIndex = i;

				ImGui::Text(memberName.C_Str());
				ImGui::NextColumn();

				if (ImGui::BeginCombo((newIDString).C_Str(), enm->values[currentValueIndex].name.C_Str()))
				{
					for (u32 i = 0; i < enm->values.Size(); i++)
					{
						const MetadataEnumValue& enumValue = enm->values[i];
						if (ImGui::Selectable((memberName + enumValue.name + newIDString).C_Str()))
						{
							*(u32*)offsetedData = enm->values[i].value;
						}
					}
					ImGui::EndCombo();
				}
				ImGui::NextColumn();
			}
		}

		//ImGui::Unindent(indent);
	}

	void EditorGUI::BlueText(const char* chars)
	{
		ImGui::NewLine();
		ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(0.18f, 0.49f, 0.98f, 1.0f));
		ImGui::Text(chars);
		ImGui::NewLine();
		ImGui::PopStyleColor();
	}

	void EditorGUI::SetEngineCamera(EngineCamera camera)
	{
		switch (camera)
		{
		case ENGINE_CAMERA_2D: {
			Engine::GetActiveApplication()->GetECS()->SetEntityEnabled("Camera2D", true);
			Engine::GetActiveApplication()->GetECS()->SetEntityEnabled("Camera3D", false);
		} break;
		case ENGINE_CAMERA_3D: {
			Engine::GetActiveApplication()->GetECS()->SetEntityEnabled("Camera2D", false);
			Engine::GetActiveApplication()->GetECS()->SetEntityEnabled("Camera3D", true);
		} break;
		case ENGINE_CAMERA_NONE: {
			Engine::GetActiveApplication()->GetECS()->SetEntityEnabled("Camera2D", false);
			Engine::GetActiveApplication()->GetECS()->SetEntityEnabled("Camera3D", false);
		} break;
		}
	}
}