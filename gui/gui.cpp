#include "gui.h"
#include "../imgui/imgui.h"
#include "../imgui/imgui_impl_dx9.h"
#include "../imgui/imgui_impl_win32.h"
#include <string>
#include <iostream>
#include <fstream>
#include <vector>
#include "setting.h"
#include "files.h"
#include <filesystem>
namespace fs = std::filesystem;

extern IMGUI_IMPL_API LRESULT ImGui_ImplWin32_WndProcHandler(
	HWND window,
	UINT message,
	WPARAM wideParameter,
	LPARAM longParameter
);

long __stdcall WindowProcess(
	HWND window,
	UINT message,
	WPARAM wideParameter,
	LPARAM longParameter)
{
	if (ImGui_ImplWin32_WndProcHandler(window, message, wideParameter, longParameter))
		return true;

	switch (message)
	{
	case WM_SIZE: {
		if (gui::device && wideParameter != SIZE_MINIMIZED)
		{
			gui::presentParameters.BackBufferWidth = LOWORD(longParameter);
			gui::presentParameters.BackBufferHeight = HIWORD(longParameter);
			gui::ResetDevice();
		}
	} return 0;

	case WM_SYSCOMMAND: {
		if ((wideParameter & 0xfff0) == SC_KEYMENU) 
			return 0;
	} break;

	case WM_DESTROY: {
		PostQuitMessage(0);
	} return 0;
	}

	return DefWindowProc(window, message, wideParameter, longParameter);
}

void gui::CreateHWindow(const char* windowName) noexcept
{
	windowClass.cbSize = sizeof(WNDCLASSEX);
	windowClass.style = CS_CLASSDC;
	windowClass.lpfnWndProc = WindowProcess;
	windowClass.cbClsExtra = 0;
	windowClass.cbWndExtra = 0;
	windowClass.hInstance = GetModuleHandleA(0);
	windowClass.hIcon = 0;
	windowClass.hCursor = 0;
	windowClass.hbrBackground = 0;
	windowClass.lpszMenuName = 0;
	windowClass.lpszClassName = "class001";
	windowClass.hIconSm = 0;

	RegisterClassEx(&windowClass);

	window = CreateWindowEx(
		0,
		"class001",
		windowName,
		WS_OVERLAPPEDWINDOW,
		100,
		100,
		WIDTH,
		HEIGHT,
		0,
		0,
		windowClass.hInstance,
		0
	);

	ShowWindow(window, SW_SHOWDEFAULT);
	UpdateWindow(window);
}

void gui::DestroyHWindow() noexcept
{
	DestroyWindow(window);
	UnregisterClass(windowClass.lpszClassName, windowClass.hInstance);
}

bool gui::CreateDevice() noexcept
{
	d3d = Direct3DCreate9(D3D_SDK_VERSION);

	if (!d3d)
		return false;

	ZeroMemory(&presentParameters, sizeof(presentParameters));

	presentParameters.Windowed = TRUE;
	presentParameters.SwapEffect = D3DSWAPEFFECT_DISCARD;
	presentParameters.BackBufferFormat = D3DFMT_UNKNOWN;
	presentParameters.EnableAutoDepthStencil = TRUE;
	presentParameters.AutoDepthStencilFormat = D3DFMT_D16;
	presentParameters.PresentationInterval = D3DPRESENT_INTERVAL_ONE;

	if (d3d->CreateDevice(
		D3DADAPTER_DEFAULT,
		D3DDEVTYPE_HAL,
		window,
		D3DCREATE_HARDWARE_VERTEXPROCESSING,
		&presentParameters,
		&device) < 0)
		return false;

	return true;
}

void gui::ResetDevice() noexcept
{
	ImGui_ImplDX9_InvalidateDeviceObjects();

	const auto result = device->Reset(&presentParameters);

	if (result == D3DERR_INVALIDCALL)
		IM_ASSERT(0);

	ImGui_ImplDX9_CreateDeviceObjects();
}

void gui::DestroyDevice() noexcept
{
	if (device)
	{
		device->Release();
		device = nullptr;
	}

	if (d3d)
	{
		d3d->Release();
		d3d = nullptr;
	}
}

void gui::CreateImGui() noexcept
{
	IMGUI_CHECKVERSION();
	ImGui::CreateContext();
	ImGuiIO& io = ::ImGui::GetIO();

	io.Fonts->AddFontFromFileTTF("C:\\Windows\\Fonts\\CascadiaMono.ttf", settings::fontSize);

	io.IniFilename = NULL;


	ImGui::StyleColorsLight();

	ImGui_ImplWin32_Init(window);
	ImGui_ImplDX9_Init(device);
}

void gui::DestroyImGui() noexcept
{
	ImGui_ImplDX9_Shutdown();
	ImGui_ImplWin32_Shutdown();
	ImGui::DestroyContext();
}

void gui::BeginRender() noexcept
{
	MSG message;
	while (PeekMessage(&message, 0, 0, 0, PM_REMOVE))
	{
		TranslateMessage(&message);
		DispatchMessage(&message);

		if (message.message == WM_QUIT)
		{
			isRunning = !isRunning;
			return;
		}
	}

	// Start the Dear ImGui frame
	ImGui_ImplDX9_NewFrame();
	ImGui_ImplWin32_NewFrame();
	ImGui::NewFrame();
}

void gui::EndRender() noexcept
{
	ImGui::EndFrame();

	device->SetRenderState(D3DRS_ZENABLE, FALSE);
	device->SetRenderState(D3DRS_ALPHABLENDENABLE, FALSE);
	device->SetRenderState(D3DRS_SCISSORTESTENABLE, FALSE);

	device->Clear(0, 0, D3DCLEAR_TARGET | D3DCLEAR_ZBUFFER, D3DCOLOR_RGBA(0, 0, 0, 255), 1.0f, 0);

	if (device->BeginScene() >= 0)
	{
		ImGui::Render();
		ImGui_ImplDX9_RenderDrawData(ImGui::GetDrawData());
		device->EndScene();
	}

	const auto result = device->Present(0, 0, 0, 0);

	// Handle loss of D3D9 device
	if (result == D3DERR_DEVICELOST && device->TestCooperativeLevel() == D3DERR_DEVICENOTRESET)
		ResetDevice();
}



std::string textBuffer;

static size_t activeFileIndex = 0;

struct OpenedFile {
	std::string name;
	std::vector<char> content;
	bool isOpen = false;
	bool showEdit = true;
};

std::vector<OpenedFile> openedFiles;

void LoadFile(const std::string& filename) {
	//is it ooen ???
	for (const OpenedFile& openedFile : openedFiles) {
		if (openedFile.name == filename && openedFile.isOpen) {
			std::cout << "File is already open: " << filename << std::endl;
			return; 
		}
	}

	std::ifstream inputFile(filename, std::ios::binary);
	if (inputFile.is_open()) {
		OpenedFile newFile;
		newFile.name = filename;

		// Check if the file is empty
		inputFile.seekg(0, std::ios::end);
		std::streamsize size = inputFile.tellg();
		inputFile.seekg(0, std::ios::beg);

		if (size > 0) {
			newFile.content.resize(size);
			inputFile.read(newFile.content.data(), size);
			inputFile.close();
			std::cout << "File loaded successfully: " << filename << std::endl;
		}
		else {
			inputFile.close();
			std::cout << "File is empty: " << filename << std::endl;
		}

		//make open
		newFile.isOpen = true;
		openedFiles.push_back(newFile);
	}
	else {
		std::cout << "Failed to open the file: " << filename << std::endl;
	}
}

void SaveFile(size_t index) {
	if (index >= openedFiles.size()) {
		std::cout << "Invalid file index." << std::endl;
		return;
	}

	OpenedFile& file = openedFiles[index];
	std::ofstream outputFile(file.name, std::ios::binary);
	if (outputFile.is_open()) {
		outputFile.write(file.content.data(), file.content.size());
		outputFile.close();
		std::cout << "File saved successfully: " << file.name << std::endl;
	}
	else {
		std::cout << "Failed to save the file: " << file.name << std::endl;
	}
}

void LoadFolder(const std::string& folderPath) {
	for (const auto& entry : fs::directory_iterator(folderPath)) {
		if (entry.is_regular_file()) {
			LoadFile(entry.path().string());
		}
		else if (entry.is_directory()) {
			LoadFolder(entry.path().string());
		}
	}
}

void gui::Render() noexcept
{

	ImGui::SetNextWindowPos({ 0, 23 }, ImGuiCond_FirstUseEver);
	ImGui::SetNextWindowSize({ 900, 740 }, ImGuiCond_FirstUseEver);
	ImGui::Begin(
		"banno",
		&isRunning,
		ImGuiWindowFlags_NoCollapse | ImGuiWindowFlags_NoTitleBar
	);

	//bit broken can fix by only using font for txt editor
	static float previousFontSize = settings::fontSize;
	if (settings::fontSize != previousFontSize) {
		ImGui::GetIO().FontGlobalScale = settings::fontSize / 18.0f;
		previousFontSize = settings::fontSize;
	}

	//menu bar
	if (ImGui::BeginMainMenuBar()) {
		if (ImGui::BeginMenu("File")) {
			if (ImGui::MenuItem("Open")) {
				OPENFILENAMEA ofn;
				char fileName[MAX_PATH] = "";
				ZeroMemory(&ofn, sizeof(ofn));
				ofn.lStructSize = sizeof(ofn);
				ofn.hwndOwner = gui::window;
				ofn.lpstrFilter = "All Files (*.*)\0*.*\0C++ Files (*.cpp;*.h)\0*.cpp;*.h\0Text Files (*.txt)\0*.txt\0";
				ofn.lpstrFile = fileName;
				ofn.nMaxFile = MAX_PATH;
				ofn.lpstrTitle = "Open File";
				ofn.Flags = OFN_FILEMUSTEXIST | OFN_PATHMUSTEXIST;

				if (GetOpenFileNameA(&ofn)) {
					LoadFile(fileName);
				}
			}
			if (ImGui::MenuItem("Open Folder")) {
				OPENFILENAMEA ofn;
				char folderName[MAX_PATH] = "";
				ZeroMemory(&ofn, sizeof(ofn));
				ofn.lStructSize = sizeof(ofn);
				ofn.hwndOwner = gui::window;
				ofn.lpstrFile = folderName;
				ofn.nMaxFile = MAX_PATH;
				ofn.lpstrTitle = "Open Folder";
				ofn.Flags = OFN_PATHMUSTEXIST | OFN_FILEMUSTEXIST | OFN_NOCHANGEDIR;

				if (GetOpenFileNameA(&ofn)) {
					LoadFolder(folderName);
				}
			}
			if (ImGui::MenuItem("Save")) {
				OPENFILENAMEA sfn;
				char fileName[MAX_PATH] = "";
				ZeroMemory(&sfn, sizeof(sfn));
				sfn.lStructSize = sizeof(sfn);
				sfn.hwndOwner = gui::window;
				sfn.lpstrFilter = "All Files (*.*)\0*.*\0C++ Files (*.cpp;*.h)\0*.cpp;*.h\0Text Files (*.txt)\0*.txt\0";
				sfn.lpstrFile = fileName;
				sfn.nMaxFile = MAX_PATH;
				sfn.lpstrTitle = "Save File";

				if (GetSaveFileNameA(&sfn)) {
					SaveFile(activeFileIndex);
				}
			}
			ImGui::EndMenu();
		}

		if (ImGui::BeginMenu("Settings")) {
			if (ImGui::MenuItem("Customisability")) {
				settings::showSettings = true;
				settings::showTxtEdit = false;
			}

			ImGui::EndMenu();
		}
		ImGui::EndMainMenuBar();
	}

	auto textInputCallback = [](ImGuiInputTextCallbackData* data) -> int {
		if (data->EventFlag == ImGuiInputTextFlags_CallbackResize) {
			textBuffer.resize(data->BufSize);
			data->Buf = textBuffer.data();
		}
		return 0;
	};

	//files tab
	if (ImGui::BeginTabBar("filesTab", ImGuiTabBarFlags_Reorderable ) && !settings::showSettings && settings::showTxtEdit) {
		for (size_t i = 0; i < openedFiles.size(); i++) {
			auto& file = openedFiles[i];
			fs::path filePath(file.name);
			std::string fileName = filePath.filename().string();
			std::string tabLabel = fileName + "##" + std::to_string(i);
			if (file.showEdit) {
				if (ImGui::BeginTabItem(tabLabel.c_str(), &file.showEdit)) {
					activeFileIndex = i;
					//txt editor
					ImGui::InputTextMultiline(
						"##inputText",
						&file.content[0],
						file.content.size() + 1,
						ImVec2(-1, -1),
						ImGuiInputTextFlags_CallbackResize,
						[](ImGuiInputTextCallbackData* data) -> int {
							if (data->EventFlag == ImGuiInputTextFlags_CallbackResize) {
								if (data->UserData) {
									auto& fileData = openedFiles[static_cast<size_t>(reinterpret_cast<uintptr_t>(data->UserData))];
									fileData.content.resize(data->BufSize);
									data->Buf = fileData.content.data();
								}
							}
							return 0;
						},
						reinterpret_cast<void*>(static_cast<uintptr_t>(i))
							);

					ImGui::EndTabItem();
				}
			}
		}
		ImGui::EndTabBar();
	}else if (settings::showSettings && !settings::showTxtEdit) {
		ImGui::SliderFloat("Font size", &settings::fontSize, 4.0f, 100.0f);
		if (ImGui::Button("Back")) {
			settings::showSettings = false;
			settings::showTxtEdit = true;
		}
	}

	ImGui::End();

	ImGui::SetNextWindowPos({ 900, 24 }, ImGuiCond_FirstUseEver);
	ImGui::SetNextWindowSize({ 290, 740 }, ImGuiCond_FirstUseEver);
	ImGui::Begin(
		"files",
		&isRunning,
		ImGuiWindowFlags_NoCollapse
	);

	if (ImGui::TreeNode("Folders")) {
		//display them as nodes
		for (const auto& entry : fs::directory_iterator(".")) {
			if (entry.is_directory()) {
				std::string folderName = entry.path().filename().string();
				if (ImGui::TreeNode(folderName.c_str())) {
					for (const auto& fileEntry : fs::directory_iterator(entry.path())) {
						if (fileEntry.is_regular_file()) {
							std::string fileName = fileEntry.path().filename().string();
							std::string buttonLabel = fileName + "##" + folderName;
							if (ImGui::Button(buttonLabel.c_str())) {
								std::string filePath = (entry.path() / fileName).string();
								LoadFile(filePath);
							}
						}
					}
					ImGui::TreePop();
				}
			}
		}
		ImGui::TreePop();
	}

	ImGui::End();
	ImGui::EndFrame();
}

