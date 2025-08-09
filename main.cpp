#include <windows.h>
#include <shellapi.h>
#include <thread>
#include <random>
#include <fstream>
#include <string>
#include <chrono>
#include <iostream>
#include <cstring>

// 资源ID定义
#define IDI_TRAY_ICON 1
#define ID_TRAY_EXIT 1001
#define ID_TRAY_TOGGLE 1002
#define ID_TRAY_STATUS 1003
#define ID_TRAY_CONFIG 1004
#define WM_TRAYICON (WM_USER + 1)

// 配置结构体
struct Config {
	int t1 = 130;  // 按下延迟最小值
	int t2 = 160;  // 按下延迟最大值
	int t3 = 25;   // 释放延迟最小值
	int t4 = 35;   // 释放延迟最大值
	int key = 1;   // 绑定键：1=右键, 4=侧键1, 5=侧键2
};

// 全局变量
HWND g_hWnd = nullptr;
NOTIFYICONDATA g_nid = {};
bool g_isRunning = false;
bool g_isClicking = false;
Config g_config;
std::thread g_clickThread;
std::default_random_engine g_generator;
std::normal_distribution<double> g_distribution(0.0, 1.0);

// 函数声明
LRESULT CALLBACK WindowProc(HWND hwnd, UINT uMsg, WPARAM wParam, LPARAM lParam);
void LoadConfig();
void SaveConfig();
void CreateTrayIcon();
void RemoveTrayIcon();
void ShowTrayMenu();
void StartClickThread();
void StopClickThread();
void PerformClick();
int GetRandomDelay(int min, int max);
BOOL IsKeyPressed(int vKey);

// 程序入口点
int WINAPI WinMain(HINSTANCE hInstance, HINSTANCE hPrevInstance, LPSTR lpCmdLine, int nCmdShow) {
	// 初始化随机数生成器
	g_generator.seed(std::chrono::system_clock::now().time_since_epoch().count());
	
	// 加载配置
	LoadConfig();
	
	// 注册窗口类
	const char* CLASS_NAME = "MouseClickerTray";
	WNDCLASS wc = {};
	wc.lpfnWndProc = WindowProc;
	wc.hInstance = hInstance;
	wc.lpszClassName = CLASS_NAME;
	wc.hCursor = LoadCursor(nullptr, IDC_ARROW);
	
	RegisterClass(&wc);
	
	// 创建隐藏窗口（用于接收消息）
	g_hWnd = CreateWindowEx(
							0,
							CLASS_NAME,
							"Mouse Clicker",
							0,
							CW_USEDEFAULT, CW_USEDEFAULT, CW_USEDEFAULT, CW_USEDEFAULT,
							nullptr, nullptr, hInstance, nullptr
							);
	
	if (g_hWnd == nullptr) {
		return -1;
	}
	
	// 创建托盘图标
	CreateTrayIcon();
	
	// 启动监听线程
	g_isRunning = true;
	StartClickThread();
	
	// 消息循环
	MSG msg;
	while (GetMessage(&msg, nullptr, 0, 0)) {
		TranslateMessage(&msg);
		DispatchMessage(&msg);
	}
	
	// 清理
	StopClickThread();
	RemoveTrayIcon();
	return 0;
}

// 窗口消息处理函数
LRESULT CALLBACK WindowProc(HWND hwnd, UINT uMsg, WPARAM wParam, LPARAM lParam) {
	switch (uMsg) {
	case WM_TRAYICON:
		if (lParam == WM_RBUTTONUP) {
			ShowTrayMenu();
		}
		break;
		
	case WM_COMMAND:
		switch (LOWORD(wParam)) {
		case ID_TRAY_EXIT:
			PostQuitMessage(0);
			break;
		case ID_TRAY_TOGGLE:
			g_isClicking = !g_isClicking;
			break;
		case ID_TRAY_CONFIG:
			ShellExecute(nullptr, "open", "notepad.exe", "config.ini", nullptr, SW_SHOW);
			break;
		}
		break;
		
	case WM_DESTROY:
		PostQuitMessage(0);
		break;
		
	default:
		return DefWindowProc(hwnd, uMsg, wParam, lParam);
	}
	return 0;
}

// 加载配置文件
void LoadConfig() {
	std::ifstream file("config.ini");
	if (!file.is_open()) {
		// 如果配置文件不存在，创建默认配置
		SaveConfig();
		return;
	}
	
	std::string line;
	while (std::getline(file, line)) {
		if (line.find("t1=") == 0) {
			g_config.t1 = std::stoi(line.substr(3));
		}
		else if (line.find("t2=") == 0) {
			g_config.t2 = std::stoi(line.substr(3));
		}
		else if (line.find("t3=") == 0) {
			g_config.t3 = std::stoi(line.substr(3));
		}
		else if (line.find("t4=") == 0) {
			g_config.t4 = std::stoi(line.substr(3));
		}
		else if (line.find("key=") == 0) {
			g_config.key = std::stoi(line.substr(4));
		}
	}
	file.close();
}

// 保存配置文件
void SaveConfig() {
	std::ofstream file("config.ini");
	if (file.is_open()) {
		file << "[interval]\n";
		file << "t1=" << g_config.t1 << "\n";
		file << "t2=" << g_config.t2 << "\n";
		file << "t3=" << g_config.t3 << "\n";
		file << "t4=" << g_config.t4 << "\n";
		file << "key=" << g_config.key << "\n";
		file << "\n# 配置说明：\n";
		file << "# t1<t2 控制按下到释放的间隔(ms)，建议130~170\n";
		file << "# t3<t4 控制释放到再次按下的间隔(ms)，建议25~35\n";
		file << "# key: 1=右键, 4=侧键1, 5=侧键2\n";
		file.close();
	}
}

// 创建托盘图标
void CreateTrayIcon() {
	g_nid.cbSize = sizeof(NOTIFYICONDATA);
	g_nid.hWnd = g_hWnd;
	g_nid.uID = IDI_TRAY_ICON;
	g_nid.uFlags = NIF_ICON | NIF_MESSAGE | NIF_TIP;
	g_nid.uCallbackMessage = WM_TRAYICON;
	g_nid.hIcon = LoadIcon(nullptr, IDI_APPLICATION);
	strcpy(g_nid.szTip, "鼠标连点工具 - 未激活");
	
	Shell_NotifyIcon(NIM_ADD, &g_nid);
}

// 移除托盘图标
void RemoveTrayIcon() {
	Shell_NotifyIcon(NIM_DELETE, &g_nid);
}

// 显示托盘菜单
void ShowTrayMenu() {
	POINT pt;
	GetCursorPos(&pt);
	
	HMENU hMenu = CreatePopupMenu();
	
	// 添加菜单项
	const char* statusText = g_isClicking ? "停止连点" : "开始连点";
	AppendMenu(hMenu, MF_STRING, ID_TRAY_TOGGLE, statusText);
	AppendMenu(hMenu, MF_SEPARATOR, 0, nullptr);
	AppendMenu(hMenu, MF_STRING, ID_TRAY_CONFIG, "编辑配置");
	AppendMenu(hMenu, MF_SEPARATOR, 0, nullptr);
	AppendMenu(hMenu, MF_STRING, ID_TRAY_EXIT, "退出");
	
	// 设置焦点到窗口，确保菜单正确显示和隐藏
	SetForegroundWindow(g_hWnd);
	
	// 显示菜单
	TrackPopupMenu(hMenu, TPM_RIGHTBUTTON, pt.x, pt.y, 0, g_hWnd, nullptr);
	
	// 清理菜单
	DestroyMenu(hMenu);
}

// 启动点击监听线程
void StartClickThread() {
	g_clickThread = std::thread([]() {
		bool wasKeyPressed = false;
		
		while (g_isRunning) {
			// 重新加载配置（支持热更新）
			static auto lastConfigCheck = std::chrono::steady_clock::now();
			auto now = std::chrono::steady_clock::now();
			if (now - lastConfigCheck > std::chrono::seconds(1)) {
				LoadConfig();
				lastConfigCheck = now;
			}
			
			// 检查按键状态
			int targetKey;
			switch (g_config.key) {
				case 4: targetKey = VK_XBUTTON1; break;  // 侧键1
				case 5: targetKey = VK_XBUTTON2; break;  // 侧键2
				default: targetKey = VK_RBUTTON; break;  // 右键
			}
			
			bool isKeyPressed = IsKeyPressed(targetKey);
			
			if (isKeyPressed && !wasKeyPressed) {
				g_isClicking = true;
				strcpy(g_nid.szTip, "鼠标连点工具 - 连点中");
				Shell_NotifyIcon(NIM_MODIFY, &g_nid);
			}
			else if (!isKeyPressed && wasKeyPressed) {
				g_isClicking = false;
				strcpy(g_nid.szTip, "鼠标连点工具 - 未激活");
				Shell_NotifyIcon(NIM_MODIFY, &g_nid);
			}
			
			wasKeyPressed = isKeyPressed;
			
			// 如果正在连点，执行点击
			if (g_isClicking && isKeyPressed) {
				PerformClick();
			}
			
			std::this_thread::sleep_for(std::chrono::milliseconds(1));
		}
	});
}

// 停止点击线程
void StopClickThread() {
	g_isRunning = false;
	if (g_clickThread.joinable()) {
		g_clickThread.join();
	}
}

// 执行点击操作
void PerformClick() {
	// 获取当前鼠标位置
	POINT pt;
	GetCursorPos(&pt);
	
	// 按下鼠标
	mouse_event(MOUSEEVENTF_LEFTDOWN, pt.x, pt.y, 0, 0);
	
	// 随机延迟（按下到释放）
	int pressDelay = GetRandomDelay(g_config.t1, g_config.t2);
	std::this_thread::sleep_for(std::chrono::milliseconds(pressDelay));
	
	// 释放鼠标
	mouse_event(MOUSEEVENTF_LEFTUP, pt.x, pt.y, 0, 0);
	
	// 随机延迟（释放到下次点击）
	int releaseDelay = GetRandomDelay(g_config.t3, g_config.t4);
	std::this_thread::sleep_for(std::chrono::milliseconds(releaseDelay));
}

// 生成随机延迟（符合正态分布）
int GetRandomDelay(int min, int max) {
	double mean = (min + max) / 2.0;
	double stddev = (max - min) / 6.0;  // 3σ原则
	
	double value;
	do {
		value = g_distribution(g_generator) * stddev + mean;
	} while (value < min || value > max);
	
	return static_cast<int>(value);
}

// 检查按键是否被按下
BOOL IsKeyPressed(int vKey) {
	return GetAsyncKeyState(vKey) & 0x8000;
}
