#include <windows.h>   // 窗口
#include <d3d12.h>    // D3D12核心头文件
#include <dxgi1_4.h>  // DXGI核心头文件

#pragma comment(lib,"d3d12.lib")  //链接D3D12库文件
#pragma comment(lib,"dxgi.lib")   //链接DXGI库文件



LPCTSTR gWindowClassName = L"BattleFire";//ASCII   LPCTSTR是常量字符串指针宏  为啥不用const wchar_t* ？   主要是为了适配  一套代码，不用修改，只切换项目编码配置，就能支持两种字符编码


// 【渲染指挥官】ID3D12Device 接口指针
// 它是逻辑上的 GPU，负责创建所有渲染资源（纹理、缓存）和管线状态（PSO）
ID3D12Device* gD3D12Device = nullptr; 


//D3D12初始化函数
bool InitD3D12(HWND inhwnd,int inWidth,int inHeight) 
{

	HRESULT hResult;	//COM 接口操作的返回值类型（DX12 的所有接口方法 / 创建函数，返回值都是HRESULT），用于判断「操作是否成功」

	UINT dxgiFactoryFlags = 0;

#ifdef DEBUG
	{
		ID3D12Debug* debugController = nullptr;  //初始化接口指针（空值） 是的这是个COM接口

		if (SUCCEEDED(D3D12GetDebugInterface(IID_PPV_ARGS(&debugController)))) //如果成功获取调试接口 D3D12GetDebugInterface就是用来获取调试接口的函数
		{

			debugController->EnableDebugLayer(); //启用调试层 这知道这个功能就好了
			dxgiFactoryFlags |= DXGI_CREATE_FACTORY_DEBUG;  //给dxgiFactoryFlags变量「开启」DXGI_CREATE_FACTORY_DEBUG这个标志位，启用 DXGI 工厂的调试功能
		}

	}
#endif

	/////////////////////////////////////////////////////////////////////总结：DXGI 工厂是「连接 DX12 和显卡硬件的中转站」，必须先创建成功。 

	// 1. 声明DXGI工厂接口指针（空架子，无有效地址）
	IDXGIFactory4* dxgiFactory;  

	// 2. 调用专门的创建函数，创建DXGI工厂实例，给指针赋值有效地址
	hResult = CreateDXGIFactory2(dxgiFactoryFlags, IID_PPV_ARGS(&dxgiFactory));   //是这样： #define IID_PPV_ARGS(ppType) __uuidof(**(ppType)), IID_PPV_ARGS_Helper(ppType)
	
	//3. 判断创建是否失败，失败则直接返回false，终止DX12初始化（没有工厂后续步骤无法进行）
	if (FAILED(hResult))    //#define FAILED(hr) (((HRESULT)(hr)) < 0)
	{
		return false;
	}


	IDXGIAdapter1* adapter;  //显卡适配器指针 包括独立显卡、集成显卡、软件模拟显卡 这也是个COM接口指针
	int adapterIndex = 0;  //适配器索引
	bool adapterFound = false;//是否创建成功的标志
	
	while (dxgiFactory->EnumAdapters1(adapterIndex, &adapter) != DXGI_ERROR_NOT_FOUND)  //#define DXGI_ERROR_NOT_FOUND  这个宏表示没有找到显卡    EnumAdapters1是枚举显卡适配器的函数是DXGI工厂的一个方法    dxgiFactory会把适配器的地址写到你的 adapter 变量里 那么adapter就有值了（指向比如RTX4090的地址）
	{
		DXGI_ADAPTER_DESC1 desc;  //显卡描述结构体  存储显卡的信息（显卡名称、厂商 ID、显存大小、硬件级别等）

		adapter->GetDesc1(&desc);  // GetDesc1又是显卡适配器的一个方法  把显卡的信息写到desc结构体里

		if(desc.Flags&DXGI_ADAPTER_FLAG_SOFTWARE)  // DXGI_ADAPTER_FLAG_SOFTWARE是软件模拟显卡  这是“位掩码”的写法 记住“Flags & 枚举值”的写法
		{
			continue; // 这是软件渲染？？？  就跳过
		}

		//硬件适配器  试着创建D3D12设备 要么是独显 要么是集成显卡？  11只后是有computer shader的  然后返回结果给hResult
		hResult = D3D12CreateDevice(adapter, D3D_FEATURE_LEVEL_11_0, __uuidof(ID3D12Device), nullptr);  //创建的是一个“逻辑显卡”  物理显卡是真的显卡 逻辑显卡是你在代码里对这块硬件的操控手柄 用他分配内存创建管线管理同步 这里是NULL表示不需要这个设备接口指针地址  只想测试能不能创建成功
		                                                                                               //adapter 在循环的每一次都会指向一个完全不同的内存地址（代表不同的显卡硬件）。 如果测试创建成功，说明找到了合适的显卡适配器，可以用它来创建真正的 D3D12 设备。

		//如果创建成功 说明找到了合适的适配器
		if (SUCCEEDED(hResult))
		{
			adapterFound = true;
			break;

		}

		adapterIndex++; //索引++

	}

	if(false == adapterFound) //防御性编程，避免把相等判断（==）误写成赋值操作（=）
	{
		return false;
	}
	hResult = D3D12CreateDevice(adapter, D3D_FEATURE_LEVEL_11_0, IID_PPV_ARGS(&gD3D12Device));//真正创建逻辑显卡  （DX12设备指针）
	if (FAILED(hResult))
	{
		return false ;

	}


	//DX12初始化代码
}







// 窗口信息处理函数（客服)  LRESULT是返回值 代表信息是否处理 处理不了就执行什么逻辑
// CALLBACK是额外修饰关键字  它的本质是__stdcall，是 Windows 系统和窗口过程函数之间的 “调用规矩”，保证调用不出错（普通函数调用是很明确的 你自己调用你自己写的函数 但是WindowProc不一样，调用者是 Windows 系统（不是你） 相当于你留的客服电话  所以需要这个 这个记住就行 窗口函数就是要）
//inHWND是消息所属的窗口句柄 标识这条消息是给哪个窗口的  比如打开多个窗口
//inMSG是消息类型  WM_CLOSE：用户点击了窗口右上角的 “关闭” 按钮  WM_DESTROY：窗口被销毁时触发  WM_SIZE窗口拉升
//WPARAM消息附加参数 1  具体含义随inMSG（消息类型）变化。  比如：WM_KEYDOWN（键盘按下）消息中，这个参数存储的是 “按下的按键编码”（比如VK_ESCAPE表示 ESC 键）。
// LPARAM是消息附加参数 2  具体含义随inMSG（消息类型）变化  比如：WM_LBUTTONDOWN（鼠标左键按下）消息中，这个参数存储的是 “鼠标点击的坐标”（高 16 位是 Y 坐标，低 16 位是 X 坐标）。
LRESULT CALLBACK WindowProc(HWND inHWND, UINT inMSG, WPARAM inWParam, LPARAM inLParam) {

	// 匹配消息类型，处理对应逻辑
	switch (inMSG)
	{
		// 消息1：用户点击了窗口关闭按钮
	case WM_CLOSE:
		PostQuitMessage(0); //enqueue WM_QUIT
		break;

	}
	return DefWindowProc(inHWND, inMSG, inWParam, inLParam);

}






//第一个参数是应用程序实例句柄 程序的身份证  第二个是无用兼容遗留 第三是 程序启动时外部传入的参数（比如控制台打参数）  第四个是窗口初始显示命令 还有最大化最小化隐藏窗口
int WINAPI WinMain(HINSTANCE hInstance, HINSTANCE hPrevInstance, LPSTR lpCmdLine, int inShowCmd)
{

	//窗口部分属于一次完成 之后很少改动的

	//////////////////////////////////////////////////////////////////////----------regiter 注册------------////////////////////////////////////////////////////////////////////////////////
	WNDCLASSEX wndClassEx;
	wndClassEx.cbSize = sizeof(WNDCLASSEX);
	wndClassEx.style = CS_HREDRAW | CS_VREDRAW;  //水平和垂直方向 如果不设置这个样式，当你用鼠标拉伸窗口（变大 / 变小）时，窗口的客户区（里面的空白区域）会出现花屏、残留旧画面的情况；设置后，窗口大小变化时，系统会自动触发重绘，保持窗口画面整洁。
	wndClassEx.cbClsExtra = NULL;  //class 申请额外备用内存   现代32/64系统几乎无用
	wndClassEx.cbWndExtra = NULL;  //instance  也没额外空间 这是是窗口实例 

	wndClassEx.hInstance = hInstance;  //hInstance是当前运行的程序（进程）的「唯一标识」 是身份证  
	//每个窗口实例（HWND）会继承所属窗口类的hInstance，确保资源归属正确 这是重点


	wndClassEx.hIcon = LoadIcon(NULL, IDI_APPLICATION);   //设置窗口的「大图标」（任务栏、窗口标题栏、Alt+Tab 切换时显示的图标）
	wndClassEx.hIconSm = LoadIcon(NULL, IDI_APPLICATION); //设置窗口的「小图标」（标题栏左上角、任务栏缩略图、任务栏小图标）
	wndClassEx.hCursor = LoadCursor(NULL, IDC_ARROW);     //设置鼠标在窗口客户区（渲染区域）内的「光标样式」，这里是系统默认的箭头光标
	wndClassEx.hbrBackground = NULL;                      //设置窗口的「背景画刷」（客户区的默认背景颜色）
	wndClassEx.lpszMenuName = NULL;						  //设置窗口的「菜单资源名称」，设为NULL表示窗口没有菜单
	wndClassEx.lpszClassName = gWindowClassName;             //给你的窗口类设置「唯一名称」（比如这里叫 “BattleFire”），相当于给 “窗口模板” 起个名字

	wndClassEx.lpfnWndProc = WindowProc;   //将 WindowProc函数的内存地址  给到lpfnWndProc（函数指针） 这里直接写函数名是隐式转化 函数指针 可以显示写成  &WindowProc

	if (!RegisterClassEx(&wndClassEx)) {
		MessageBox(NULL, L"Register Class Failed!", L"Error", MB_OK | MB_ICONERROR);
		return -1;

	}//RegisterClassEx(&wndClassEx)：调用 Windows API 注册窗口类，执行后有两种返回结果  这里&是去地址 注册失败返回会0



	//////////////////////////////////////////////////////////////////////----------creat 创建------------////////////////////////////////////////////////////////////////////////////////

	int viewportWidth = 1280;
	int viewportHeight = 720;  //画布的长宽 DX12渲染画布的客户区大小（核心，不包含窗口边框/标题栏）

	RECT rect;  //这也是个结构体
	rect.left = 0;
	rect.top = 0;
	rect.right = viewportWidth;
	rect.bottom = viewportHeight;

	AdjustWindowRect(&rect, WS_OVERLAPPEDWINDOW, FALSE); //把rect传进去  这个函数是用来计算实际大小的 避免渲染画布被窗口边框 / 标题栏占用） 传进去之后相当于重新算了rect的数值

	int windowWidth = rect.right - rect.left;   // 整个窗口的实际宽度（边框+客户区
	int windowHeight = rect.bottom - rect.top;  // 整个窗口的实际高度（标题栏+边框+客户区）

	HWND hwnd = CreateWindowEx(NULL,
		gWindowClassName,
		L"My RenderWindow",
		WS_OVERLAPPEDWINDOW,    //样式
		CW_USEDEFAULT, CW_USEDEFAULT,
		windowWidth, windowHeight,
		NULL,
		NULL,
		hInstance,
		NULL);

	//hwnd是窗口实例的唯一句柄（索引），通过它间接操作系统内存中的窗口实例(当然 初学阶段直接理解为窗口实例也ok)

	//这个函数的参数
	//HWND CreateWindowEx(
	//	DWORD     dwExStyle,      // 1. 窗口扩展样式
	//	LPCWSTR   lpClassName,    // 2. 窗口类名（核心）
	//	LPCWSTR   lpWindowName,   // 3. 窗口标题栏文字
	//	DWORD     dwStyle,        // 4. 窗口基础样式（核心）
	//	int       X,              // 5. 窗口初始横坐标
	//	int       Y,              // 6. 窗口初始纵坐标
	//	int       nWidth,         // 7. 窗口初始宽度（核心）
	//	int       nHeight,        // 8. 窗口初始高度（核心）
	//	HWND      hWndParent,     // 9. 父窗口句柄
	//	HMENU     hMenu,          // 10. 菜单句柄
	//	HINSTANCE hInstance,      // 11. 程序实例句柄（核心）
	//	LPVOID    lpParam         // 12. 附加参数
	//);




	if (!hwnd)
	{
		MessageBox(NULL, L"Register Window Failed!", L"Error", MB_OK | MB_ICONERROR);
		return -1;


	}



	//////////////////////////////////////////////////////////////////////----------show 显示------------////////////////////////////////////////////////////////////////////////////////


	InitD3D12(hwnd, viewportWidth, viewportHeight); //初始化D3D12
	ShowWindow(hwnd, inShowCmd);
	UpdateWindow(hwnd);


	MSG msg;
	while (true) {

		ZeroMemory(&msg, sizeof(MSG));
		if (PeekMessage(&msg, NULL, 0, 0, PM_REMOVE))
		{
			if (msg.message == WM_QUIT) //如果是用户点击了关闭的消息
			{
				break;

			}
			TranslateMessage(&msg);
			DispatchMessageW(&msg);

		}
		else
		{
			//rendering

		}

	}





	return 0;



}








//int WinMain(HINSTANCE hInstance, HINSTANCE hPrevInstance, LPSTR lpCmdLine, int nShowCmd)
//{
//
//	return 0;
//
//
//
//}














