#pragma execution_character_set("utf-8")
#include <Windows.h>
#include "Shlobj.h"
#include "ImGui\imgui.h"
#include "ImGui\imgui_impl_win32.h"
#include "ImGui\imgui_impl_dx9.h"
#include "ImGui\imgui_internal.h"
#include "plugin.h"

#include "font.h"

using namespace plugin;

IDirect3DDevice9* m_pDevice = nullptr;
WNDPROC m_pWindowProc; // Переменная оригинального обработчика окна

extern IMGUI_IMPL_API LRESULT ImGui_ImplWin32_WndProcHandler(HWND hWnd, UINT msg, WPARAM wParam, LPARAM lParam);

void show_cursor(bool show)
{
	if (show) {
		patch::Nop(0x541DF5, 5); // don't call CControllerConfigManager::AffectPadFromKeyBoard
		patch::Nop(0x53F417, 5); // don't call CPad__getMouseState
		patch::SetRaw(0x53F41F, "\x33\xC0\x0F\x84", 4); // test eax, eax -> xor eax, eax
														// jl loc_53F526 -> jz loc_53F526
		patch::PutRetn(0x6194A0); // disable RsMouseSetPos (ret)
		ImGui::GetIO().MouseDrawCursor = true;
		// (*(IDirect3DDevice9**)0xC97C28)->ShowCursor(true);

	}
	else {
		patch::SetRaw(0x541DF5, "\xE8\x46\xF3\xFE\xFF", 5); // call CControllerConfigManager::AffectPadFromKeyBoard
		patch::SetRaw(0x53F417, "\xE8\xB4\x7A\x20\x00", 5); // call CPad__getMouseState
		patch::SetRaw(0x53F41F, "\x85\xC0\x0F\x8C", 4); // xor eax, eax -> test eax, eax
														// jz loc_53F526 -> jl loc_53F526
		patch::SetUChar(0x6194A0, 0xE9); // jmp setup
		// (*(IDirect3DDevice9**)0xC97C28)->ShowCursor(false);
		ImGui::GetIO().MouseDrawCursor = false;
		//ShowCursor(false);
	}

	(*reinterpret_cast<CMouseControllerState*>(0xB73418)).X = 0.0f;
	(*reinterpret_cast<CMouseControllerState*>(0xB73418)).Y = 0.0f;
	((void(__cdecl*)())(0x541BD0))(); // CPad::ClearMouseHistory
	((void(__cdecl*)())(0x541DD0))(); // CPad::UpdatePads
}

bool bMenuState = false;

// Обработчик событий окна
auto __stdcall WndProcHandler(HWND hWnd, UINT msg, WPARAM wParam, LPARAM lParam)->LRESULT {

	static bool buttonNotChanged = true;
	switch (msg)
	{
		// При нажатия на кнопку
	case WM_SYSKEYDOWN:
	case WM_KEYDOWN:
		if (wParam == VK_END && *reinterpret_cast<unsigned char*>(0xB7CB49) == FALSE)
			bMenuState ^= TRUE;

		break;
	}

	static bool popen = false;
	// Если не меняем клавишу...
	if (buttonNotChanged) {
		// Проверяем состояние окна
		if (bMenuState)
		{
			// Если окно открыто, но не включена мышь
			if (!popen)
				show_cursor(true);
			popen = true;
			// Вызываем обработчик событий ImGui, чтобы виджеты могли работать
			ImGui_ImplWin32_WndProcHandler(hWnd, msg, wParam, lParam);
		}
		else
		{
			// Если окно закрыто, но мышь все еще включена
			if (popen)
			{
				popen = false;
				show_cursor(false);
			}
		}
		// Вызываем оригинальный обработчик событий окна
		return CallWindowProcA(m_pWindowProc, hWnd, msg, wParam, lParam);
	}
	// Чтобы не происходило лишних действий пока у нас активен выбор новой клавиши, мы просто говорим цепочке обработчиков событий, что событие успешно обработано
	buttonNotChanged = true;
	return TRUE;
}


bool bInit = false;
ImFont* pIconFont = nullptr;

class ImGuiTest {
public:
	static void Init()
	{
		// Устанавливаем хук событий окна
		// **(HWND**)0xC17054 -  HWND на главное окно игры, на него мы и вешаем обработчики
		m_pWindowProc = (WNDPROC)SetWindowLongW(**(HWND**)0xC17054, GWL_WNDPROC, (LONG)WndProcHandler);
		// Инициализируем ImGui
		ImGui::CreateContext();
		ImGui_ImplWin32_Init(**(HWND**)0xC17054);
		ImGui::GetIO().MouseDoubleClickTime = 0.8f;

		// Получаем путь до папки Fonts
		char pathFonstwindows[MAX_PATH]{};
		SHGetSpecialFolderPath(0, pathFonstwindows, CSIDL_FONTS, true);
		_snprintf_s(pathFonstwindows, sizeof(pathFonstwindows) - 1, "%s\\Arial.ttf", pathFonstwindows);
		// Грузим шрифт с кириллическими начертаниями, что вместо русского текста не было знаков вопроса
		ImGui::GetIO().Fonts->AddFontFromFileTTF(pathFonstwindows, 16.5f, NULL, ImGui::GetIO().Fonts->GetGlyphRangesCyrillic());

		// Грузим иконочный шрифт Font Awesome 5
		static const ImWchar icons_ranges[] = { 0xE005, 0xF8FF, 0 };
		ImFontConfig icons_config; icons_config.MergeMode = true; icons_config.PixelSnapH = true;
		pIconFont = ImGui::GetIO().Fonts->AddFontFromMemoryTTF(fasolid900, sizeof(fasolid900), 16.f, &icons_config, icons_ranges);

		Style();
		// m_pDevice = reinterpret_cast<IDirect3DDevice9 *>(RwD3D9GetCurrentD3DDevice());
		ImGui_ImplDX9_Init(reinterpret_cast<IDirect3DDevice9 *>(RwD3D9GetCurrentD3DDevice())/*m_pDevice*/);
	}

	static void Draw()
	{
		// Создаем новый фрейм ImGui
		ImGui_ImplDX9_NewFrame();
		ImGui_ImplWin32_NewFrame();
		ImGui::NewFrame();
		// Используем наш иконочный шрифт
		ImGui::PushFont(pIconFont);

		if (bMenuState)
		{	
			POINT sResolution = { *reinterpret_cast<int*>(0xC9C040), *reinterpret_cast<int*>(0xC9C044) };
			ImGui::SetNextWindowPos(ImVec2((float)(sResolution.x / 2 - 200), (float)(sResolution.y / 2 - 125)));
			ImGui::SetNextWindowSize(ImVec2(580, 355));

			ImGui::Begin(u8"\uf188 Тестовое окно \uf188", &bMenuState, ImGuiWindowFlags_NoSavedSettings);
			{
				ImGui::Text("Привет, мир!");
			}
			ImGui::End();
		}

		ImGui::PopFont();
		// Заканчиваем кадр
		ImGui::EndFrame();
		ImGui::Render();
		ImGui_ImplDX9_RenderDrawData(ImGui::GetDrawData());
	}

	static void Reset()
	{
		//if (bInit)
		ImGui_ImplDX9_InvalidateDeviceObjects();
	}
	
	static void Destory()
	{
		ImGui_ImplDX9_Shutdown();
		ImGui_ImplWin32_Shutdown();
		ImGui::DestroyContext();
	}

	static void Style()
	{
		ImGui::GetIO().ConfigFlags |= ImGuiConfigFlags_NoMouseCursorChange;
		ImGui::GetStyle().FrameRounding = 4.0f;
		ImGui::GetStyle().GrabRounding = 4.0f;

		ImVec4* colors = ImGui::GetStyle().Colors;
		colors[ImGuiCol_Text] = ImVec4(0.95f, 0.96f, 0.98f, 1.00f);
		colors[ImGuiCol_TextDisabled] = ImVec4(0.36f, 0.42f, 0.47f, 1.00f);
		colors[ImGuiCol_WindowBg] = ImVec4(0.11f, 0.15f, 0.17f, 1.00f);
		colors[ImGuiCol_ChildBg] = ImVec4(0.15f, 0.18f, 0.22f, 1.00f);
		colors[ImGuiCol_PopupBg] = ImVec4(0.08f, 0.08f, 0.08f, 0.94f);
		colors[ImGuiCol_Border] = ImVec4(0.08f, 0.10f, 0.12f, 1.00f);
		colors[ImGuiCol_BorderShadow] = ImVec4(0.00f, 0.00f, 0.00f, 0.00f);
		colors[ImGuiCol_FrameBg] = ImVec4(0.20f, 0.25f, 0.29f, 1.00f);
		colors[ImGuiCol_FrameBgHovered] = ImVec4(0.12f, 0.20f, 0.28f, 1.00f);
		colors[ImGuiCol_FrameBgActive] = ImVec4(0.09f, 0.12f, 0.14f, 1.00f);
		colors[ImGuiCol_TitleBg] = ImVec4(0.09f, 0.12f, 0.14f, 0.65f);
		colors[ImGuiCol_TitleBgActive] = ImVec4(0.08f, 0.10f, 0.12f, 1.00f);
		colors[ImGuiCol_TitleBgCollapsed] = ImVec4(0.00f, 0.00f, 0.00f, 0.51f);
		colors[ImGuiCol_MenuBarBg] = ImVec4(0.15f, 0.18f, 0.22f, 1.00f);
		colors[ImGuiCol_ScrollbarBg] = ImVec4(0.02f, 0.02f, 0.02f, 0.39f);
		colors[ImGuiCol_ScrollbarGrab] = ImVec4(0.20f, 0.25f, 0.29f, 1.00f);
		colors[ImGuiCol_ScrollbarGrabHovered] = ImVec4(0.18f, 0.22f, 0.25f, 1.00f);
		colors[ImGuiCol_ScrollbarGrabActive] = ImVec4(0.09f, 0.21f, 0.31f, 1.00f);
		colors[ImGuiCol_CheckMark] = ImVec4(0.28f, 0.56f, 1.00f, 1.00f);
		colors[ImGuiCol_SliderGrab] = ImVec4(0.28f, 0.56f, 1.00f, 1.00f);
		colors[ImGuiCol_SliderGrabActive] = ImVec4(0.37f, 0.61f, 1.00f, 1.00f);
		colors[ImGuiCol_Button] = ImVec4(0.20f, 0.25f, 0.29f, 1.00f);
		colors[ImGuiCol_ButtonHovered] = ImVec4(0.28f, 0.56f, 1.00f, 1.00f);
		colors[ImGuiCol_ButtonActive] = ImVec4(0.06f, 0.53f, 0.98f, 1.00f);
		colors[ImGuiCol_Header] = ImVec4(0.20f, 0.25f, 0.29f, 0.55f);
		colors[ImGuiCol_HeaderHovered] = ImVec4(0.26f, 0.59f, 0.98f, 0.80f);
		colors[ImGuiCol_HeaderActive] = ImVec4(0.26f, 0.59f, 0.98f, 1.00f);
		colors[ImGuiCol_Separator] = ImVec4(0.20f, 0.25f, 0.29f, 1.00f);
		colors[ImGuiCol_SeparatorHovered] = ImVec4(0.10f, 0.40f, 0.75f, 0.78f);
		colors[ImGuiCol_SeparatorActive] = ImVec4(0.10f, 0.40f, 0.75f, 1.00f);
		colors[ImGuiCol_ResizeGrip] = ImVec4(0.26f, 0.59f, 0.98f, 0.25f);
		colors[ImGuiCol_ResizeGripHovered] = ImVec4(0.26f, 0.59f, 0.98f, 0.67f);
		colors[ImGuiCol_ResizeGripActive] = ImVec4(0.26f, 0.59f, 0.98f, 0.95f);
		colors[ImGuiCol_Tab] = ImVec4(0.11f, 0.15f, 0.17f, 1.00f);
		colors[ImGuiCol_TabHovered] = ImVec4(0.26f, 0.59f, 0.98f, 0.80f);
		colors[ImGuiCol_TabActive] = ImVec4(0.20f, 0.25f, 0.29f, 1.00f);
		colors[ImGuiCol_TabUnfocused] = ImVec4(0.11f, 0.15f, 0.17f, 1.00f);
		colors[ImGuiCol_TabUnfocusedActive] = ImVec4(0.11f, 0.15f, 0.17f, 1.00f);
		colors[ImGuiCol_PlotLines] = ImVec4(0.61f, 0.61f, 0.61f, 1.00f);
		colors[ImGuiCol_PlotLinesHovered] = ImVec4(1.00f, 0.43f, 0.35f, 1.00f);
		colors[ImGuiCol_PlotHistogram] = ImVec4(0.90f, 0.70f, 0.00f, 1.00f);
		colors[ImGuiCol_PlotHistogramHovered] = ImVec4(1.00f, 0.60f, 0.00f, 1.00f);
		colors[ImGuiCol_TextSelectedBg] = ImVec4(0.26f, 0.59f, 0.98f, 0.35f);
		colors[ImGuiCol_DragDropTarget] = ImVec4(1.00f, 1.00f, 0.00f, 0.90f);
		colors[ImGuiCol_NavHighlight] = ImVec4(0.26f, 0.59f, 0.98f, 1.00f);
		colors[ImGuiCol_NavWindowingHighlight] = ImVec4(1.00f, 1.00f, 1.00f, 0.70f);
		colors[ImGuiCol_NavWindowingDimBg] = ImVec4(0.80f, 0.80f, 0.80f, 0.20f);
		colors[ImGuiCol_ModalWindowDimBg] = ImVec4(0.80f, 0.80f, 0.80f, 0.35f);
	}

    ImGuiTest() {
		/*
			Событие						| Функция-прототип						| Описание
			====================================================================================
			drawingEvent				| void()								| Отрисовка в 2D
			drawHudEvent				| void()								| Отрисовка HUD-элементов
			drawRadarEvent				| void()								| Отрисовка радара
			drawBlipsEvent				| void()								| Отрисовка иконок на радаре
			drawRadarOverlayEvent		| void(bool bInMenu)					| Отрисовка зон влияния на радаре
			drawMenuBackgroundEvent		| void(void *menuManager)				| Отрисовка фона в меню
			initRwEvent					| void()								| Инициализация графического движка
			shutdownRwEvent				| void()								| Деинициализация графического движка
			vehicleRenderEvent			| void(CVehicle* vehicle)				| Рендер транспорта
			pedRenderEvent				| void(CPed* ped)						| Рендер педа
			objectRenderEvent			| void(CObject* object)					| Рендер обьекта
			vehicleSetModelEvent		| void(CVehicle* vehicle, int modelId)	| Установка модели транспорта
			pedSetModelEvent			| void(CPed* ped, int modelId)			| Установка модели педа
			d3dResetEvent				| void()								| Пересоздание D3D-девайса
			d3dLostEvent				| void()								| Утерян доступ к D3D-девайсу
			gameProcessEvent			| void()								| Обработка разных компонентов игры
			initGameEvent				| void()								| Инициализация игры
			reInitGameEvent				| void()								| Повторная инициализация игры
			onPauseAllSounds			| void()								| Остановка всех звуков в игре
			onResumeAllSounds			| void()								| Включение всех звуков в игре
			initScriptsEvent			| void()								| Инициализация скриптов
			processScriptsEvent			| void()								| Обработка скриптов
		*/
		Events::initRwEvent.Add(Init);
		Events::drawingEvent.Add(Draw);
		Events::shutdownRwEvent.Add(Destory);
		Events::d3dResetEvent.Add(Reset);
    }
} imGuiTest;
