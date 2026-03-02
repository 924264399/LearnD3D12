#include <windows.h>   // 窗口

#include "BattleFireDirect.h"

#include "StaticMeshComponent.h"


#pragma comment(lib,"d3d12.lib")  //链接D3D12库文件
#pragma comment(lib,"dxgi.lib")   //链接DXGI库文件
#pragma comment(lib,"d3dcompiler.lib")  //链接D3D编译器库文件





LPCTSTR gWindowClassName = L"BattleFire";//ASCII   LPCTSTR是常量字符串指针宏  为啥不用const wchar_t* ？   主要是为了适配  一套代码，不用修改，只切换项目编码配置，就能支持两种字符编码




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


	///下面这部分是  游戏 / 程序启动时的一次性资源准备（进入主循环之前）


	InitD3D12(hwnd, viewportWidth, viewportHeight); //初始化D3D12


	ID3D12GraphicsCommandList* gCommandList = GetCommandList();
	ID3D12CommandAllocator* gCommandAllocator = GetCommandAllocator();


	StaticMeshComponent staticMeshComponent;

	staticMeshComponent.InitFromFile(gCommandList, "Res/Model/Sphere.lhsm");


	////CBO 和 IBO的两个的view 已经被封装
	

	ID3D12RootSignature* rootSignature = InitRootSignature(); //初始化根签名
	D3D12_SHADER_BYTECODE vs, ps;
	CreateShaderFromFile(L"Res/Shader/ndctriangle.hlsl", "MainVS", "vs_5_0", &vs);   //开始编译
	CreateShaderFromFile(L"Res/Shader/ndctriangle.hlsl", "MainPS", "ps_5_0", &ps);

	ID3D12PipelineState* pso = createPSO(rootSignature, vs, ps);




	ID3D12Resource* cb = CreateConstantBufferObject(65536);//65536 是1024*64(4*4矩阵)  代表这个常量缓冲区能存放65536/4=16384个float数据  也就是4096个float4数据  也就是4096行根参数表数据（每行一个float4）  这个数量根据项目的需求来定的  

	///////下面是mvp矩阵的计算  用的都是
	
	// 这里用 DirectXMath 封装好的函数
	// 1.计算 Projection（投影）矩阵：把 3D 空间投影到 2D 屏幕  参数：FOV 45度，宽高比 1280/720，近裁剪面 0.1，远裁剪面 1000
	DirectX::XMMATRIX projectionMatrix = DirectX::XMMatrixPerspectiveFovLH((45.0f * 3.151592f) / 180.0f, 1280.0f / 720.0f, 0.1f, 1000.0f);

	// 2. 计算 View（视图）矩阵：代表相机的位置和朝向  这里暂时设为单位矩阵，意思是相机在原点 (0,0,0)，看向 +Z 方向
	DirectX::XMMATRIX viewMatrix = DirectX::XMMatrixIdentity();

	//3.计算 Model（模型）矩阵：代表物体在世界空间的位置  这里把所有物体统一往 +Z 方向移动 5 个单位（左手坐标系，+Z 是屏幕往里
	DirectX::XMMATRIX modelMatrix = DirectX::XMMatrixTranslation(0.0f,0.0f,5.0f); 

	DirectX::XMFLOAT4X4 tempMatrix; //【中间变量】用于存储单个矩阵
	float matrices[48]; //最终数组

	///中间这里转一遍  是因为XMMATRIX这个类型有奇葩的需求（对齐啥的  所以相对麻烦点 要XMMATRIX → XMFLOAT4X4 → float[] ）
	DirectX::XMStoreFloat4x4(&tempMatrix, projectionMatrix); //把 Proj 矩阵存到 tempMatrix
	memcpy(matrices, &tempMatrix, sizeof(float) * 16);  //再 memcpy 到大数组的前 16 个 float  memcpy(目标地址, 源地址, 拷贝多少字节)  数组名本身就是指向第一个元素的指针 所以可以直接用

	DirectX::XMStoreFloat4x4(&tempMatrix, viewMatrix);
	memcpy(matrices+16, &tempMatrix, sizeof(float) * 16); //下面的就要偏移一下

	DirectX::XMStoreFloat4x4(&tempMatrix, modelMatrix);
	memcpy(matrices+32, &tempMatrix, sizeof(float) * 16);

	UpdateConstantBuffer(cb, matrices, sizeof(float)*48); //把数据更新到常量缓冲区对象里  这个函数里会把数据从CPU内存复制到GPU内存里去  因为是3个矩阵 所以是16*3=48


	EndCommandList();
	WaitForCompletionOfCommandList();







	ShowWindow(hwnd, inShowCmd);
	UpdateWindow(hwnd);


	//临时颜色（美术可调 来自shader）这里先临时

	float color[] = { 0.5f,0.5f,0.5f,1.0f };


	////////////////////主循环  
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

			WaitForCompletionOfCommandList();//CPU 和 GPU同步   CPU 先等 GPU 干完上一帧的所有活，再开始新一帧的渲染  所以放在最开始

			gCommandAllocator->Reset();  //这个reset的作用是？ 清空内存   以及为什么使用命令分配器的方法？因为命令分配器是负责内存的 

			gCommandList->Reset(gCommandAllocator, nullptr);


		
			BeginRenderToSwapChain(gCommandList); //画布状态切换 + 擦除画布
												//把当前要渲染的后台缓冲区（RT）从「显示状态（PRESENT）」切换为「渲染状态（RENDER_TARGET）」；
												//告诉 GPU：“接下来要往这个 RT 和深度缓冲里画东西”（绑定 RTV / DSV）；
												//设定「画面显示的区域」（视口 / 裁剪矩形）；
												//清空上一帧的画面（RT）和深度数据（DSV），准备新的绘制；


			//draw


			gCommandList->SetPipelineState(pso); //设置PSO

			gCommandList->SetGraphicsRootSignature(rootSignature);  //设置根签名  

			gCommandList->SetGraphicsRoot32BitConstants(0, 4, color, 0);//第一个参数对应D3D12_ROOT_PARAMETER 数组的index

			gCommandList->SetGraphicsRootConstantBufferView(1, cb->GetGPUVirtualAddress()); //CBV 绑定到根参数表  第一个参数对应D3D12_ROOT_PARAMETER 数组的index这里是第二个所以是1    第二个参数是常量缓冲区对象的GPU地址

			gCommandList->IASetPrimitiveTopology(D3D_PRIMITIVE_TOPOLOGY_TRIANGLELIST); //设置图元类型  这里是三角形列表  每三个点组成一个三角形


			staticMeshComponent.Render(gCommandList);




			//gCommandList->DrawInstanced(3, 1, 0, 0); //正式下达 画图命令 

			//End

			EndRenderToSwapChain(gCommandList); //画布状态切换

			EndCommandList(); // commandlist设置停止记录命令  然后正式提交commandlist到gpu（要把commandlist打包到数组 然后用批提交函数ExecuteCommandLists）  然后gFenceValue加1

			
			//以下函数实现  gSwapChain->Present(0, 0);   //切换！后变前！（把渲染好的后台缓冲区切换为前台缓冲区，显示到屏幕上）  第一个参数 0 (SyncInterval)：垂直同步开关。
			SwapD3D12Buffers();

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














