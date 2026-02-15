#include <windows.h>   // 窗口
#include <d3d12.h>    // D3D12核心头文件
#include <dxgi1_4.h>  // DXGI核心头文件

#pragma comment(lib,"d3d12.lib")  //链接D3D12库文件
#pragma comment(lib,"dxgi.lib")   //链接DXGI库文件



LPCTSTR gWindowClassName = L"BattleFire";//ASCII   LPCTSTR是常量字符串指针宏  为啥不用const wchar_t* ？   主要是为了适配  一套代码，不用修改，只切换项目编码配置，就能支持两种字符编码


// 【渲染指挥官】ID3D12Device 接口指针
// 它是逻辑上的 GPU，负责创建所有渲染资源（纹理、缓存）和管线状态（PSO）
ID3D12Device* gD3D12Device = nullptr; 
ID3D12CommandQueue* gCommandQueue = nullptr; //命令队列com接口指针

IDXGISwapChain3* gSwapChain = nullptr; //交换链com接口指针
ID3D12Resource* gDSRT =	nullptr,*gColorRTs[2]; // 显存资源接口（buffer 或者 贴图）   这个的2个RT是给交互链用的 1个depthbuffer也是给交互链用的
int gCurrentRTIndex = 0; // LYY



ID3D12DescriptorHeap* gSwapChainRTVHeap = nullptr; //这个COM接口就是给RTV和DSV 申请内存 （是「CPU/GPU 均可访问的共享内存 / 显存」）
ID3D12DescriptorHeap* gSwapChainDSVHeap = nullptr; //

UINT gRTVDescriptorSize = 0; //描述符大小  每个显卡厂商的显卡  这个值都不一样  所以要动态获取
UINT gDSVDescriptorSize = 0; //描述符大小


ID3D12CommandAllocator* gCommandAllocator = nullptr; //命令分配器COM接口指针  负责显存的分配和管理（就是给命令列表分配内存用的）
ID3D12GraphicsCommandList* gCommandList = nullptr; //命令队列COM接口指针  负责记录渲染命令（就是画图用的）

ID3D12Fence* gFence = nullptr; //栅栏 计数器 每完成一个CammendList 即+1
HANDLE gFenceEvent = nullptr;//这是闹钟 cpu 等待GPU完成工作时用的  它会创建一个 Event，告诉系统：“等 Fence 到某个值的时候，提醒我

UINT64 gFenceValue = 0;//计数器 和Fence同步用的



///用来创建VBO 即 vertex buffer object的函数 
ID3D12Resource* CreateBufferObject(ID3D12GraphicsCommandList* inCommandList, void* inData, int inDataLen, D3D12_RESOURCE_STATES inFinalResourceState)
{

	D3D12_HEAP_PROPERTIES d3dHeapProperties = {}; //堆属性结构体  你要申请显存 得告诉显卡这个读写的具体位置
												//包括了默认堆  上传堆 回读堆 三种 D3D12_HEAP_TYPE_DEFAULT  和 D3D12_HEAP_TYPE_UPLOAD 和D3D12_HEAP_TYPE_READBACK
												//权限分别是GPU专属读写   CPU读GPU写   GPU写CPU读

	d3dHeapProperties.Type = D3D12_HEAP_TYPE_DEFAULT; //默认堆 GPU



	////为什么要设置这个结构体？ 似乎在创建交换链的RT的时候 也用了这个结构体？  因为不管是创建什么资源 都要告诉显卡这个资源的读写权限和位置  这是显卡管理内存的基本要求  就好比你要在电脑里创建一个文件  你得告诉系统这个文件是放在C盘还是D盘  是文本文件还是二进制文件  是只读的还是可读写的  否则系统就不知道怎么帮你管理这个文件了
	D3D12_RESOURCE_DESC d3d12ResourceDesc = {}; //资源描述符结构体  这个结构体的成员非常多  你要创建什么样的资源 就要把对应的成员设置好  比如你要创建一个纹理  那么就要设置成Texture2D  如果是Buffer 就设置成Buffer  还有一些公共成员也要设置好 比如宽高深度这些
	d3d12ResourceDesc.Dimension = D3D12_RESOURCE_DIMENSION_BUFFER; //是Buffer
	d3d12ResourceDesc.Alignment = 0;
	d3d12ResourceDesc.Width = inDataLen;
	d3d12ResourceDesc.Height = 1;
	d3d12ResourceDesc.DepthOrArraySize = 1; //1张贴图 如果是cubemap 给6（因为cubemap 本质是一个array）
	d3d12ResourceDesc.MipLevels = 1; //需要多少层级的mip？ 
	d3d12ResourceDesc.Format = DXGI_FORMAT_UNKNOWN;  //Buffer资源的格式是未知的  因为Buffer资源不是用来当纹理采样的  也不是用来当RT的  所以不需要格式  但是Texture资源就需要格式了 因为它要当纹理采样或者当RT用
	d3d12ResourceDesc.SampleDesc.Count = 1; //没有MSAA这些
	d3d12ResourceDesc.SampleDesc.Quality = 0;  //关闭MSAA的化 这里必须是0  只有开启了MSAA才有意义 
	d3d12ResourceDesc.Layout = D3D12_TEXTURE_LAYOUT_ROW_MAJOR;  //就是创建出来用来干啥的  对于 Buffer（缓冲区）来说，数据就是一维的线，所以必须是这个格式
	d3d12ResourceDesc.Flags = D3D12_RESOURCE_FLAG_NONE; //表示这块内存没有特殊用途 vbo就没有特殊用途  

	ID3D12Resource* bufferObject = nullptr;

	///emmm 就是用这个函数创建vbo 然后你会发现这在创建交换链的RT的时候 也用了这个函数？  因为本质上VBO和RT和Buffer都是一样？ 都是内存内划一块地？
	//这里申请的内存可是不能写的默认堆  所以后面的流程是需要中转的
	gD3D12Device->CreateCommittedResource(
		&d3dHeapProperties,
		D3D12_HEAP_FLAG_NONE,
		&d3d12ResourceDesc,
		D3D12_RESOURCE_STATE_COPY_DEST, //表示初始化出来就可以被当做拷贝的目标(接受数据的状态 )
		nullptr, //优化相关的结构体  目前不需要
		IID_PPV_ARGS(&bufferObject) //资源接口指针二级地址  创建成功后资源的地址会写到这个指针里


	);


	d3d12ResourceDesc = bufferObject->GetDesc(); //获取资源描述符  前面不是都写了？为什么又获取？
												 //这里调用 GetDesc()，是为了拿到系统最终确认的、最准确的资源描述，确保后面的计算不会出错。


	//声明一堆 “接收器” 变量
	UINT64 memorySizeUsed = 0; // 用来存：这个资源总共占多少内存
	UINT64 rowSizeInBytes = 0; //用来存：每一行实际的数据大小（不含对齐
	UINT rowUsed = 0;  //用来存：这个资源有多少行

	//最重要的一个结构体：用来存“子资源在内存里的具体布局” 这个结构体的成员包括了这个资源在内存中的占用情况  包括了内存大小 行大小 行数等信息
	D3D12_PLACED_SUBRESOURCE_FOOTPRINT subresourceFootprint; 



	//gD3D12Device->GetCopyableFootprints(
	//	&d3d12ResourceDesc,  // [输入] 要算哪个资源的布局？
	//	0,                    // [输入] 从第几个子资源开始算？
	//	1,                    // [输入] 算几个子资源？
	//	0,                    // [输入] 如果放在大堆里，偏移量是多少？
	//	&subresourceFootprint,// [输出] 算出的具体布局信息放这
	//	&rowUsed,             // [输出] 算出有多少行放这
	//	&rowSizeInBytes,      // [输出] 算出每行数据大小放这
	//	&memorySizeUsed       // [输出] 算出总内存大小放这
	//);



	//调用 GetCopyableFootprints 计算布局
	gD3D12Device->GetCopyableFootprints(
		&d3d12ResourceDesc, //算这个资源
		0, //第一个子资源索引  这里是0  因为我们只有1个子资源
		1, //子资源数量  这里是1  因为我们只有1个子资源
		0, //内存偏移量  这里是0  表示从内存的起始位置开始计算
		&subresourceFootprint, 
		&rowUsed,				//行数  对于 Buffer：通常是 1（因为 Buffer 是线性的，一行就存完了）。   Texture：通常是height。
		&rowSizeInBytes,         //对于 Buffer：等于 Width（就是你实际的数据大小，不含 padding）
		&memorySizeUsed			//对于 Buffer：等于 rowSizeInBytes（因为只有一行）。 Texture：等于 rowSizeInBytes * height（因为有多行）。
	);



	//开始申请临时缓冲区  这个是中转站 所以要cpu写  Gpu读

	ID3D12Resource* tempBufferObject = nullptr;

	 d3dHeapProperties = {}; //堆属性结构体  
	d3dHeapProperties.Type = D3D12_HEAP_TYPE_UPLOAD; //这个是cpu写  Gpu读

	gD3D12Device->CreateCommittedResource(
		&d3dHeapProperties,		//上传堆
		D3D12_HEAP_FLAG_NONE,
		&d3d12ResourceDesc,
		D3D12_RESOURCE_STATE_GENERIC_READ,  //[4] 初始状态：通用读
		nullptr, 
		IID_PPV_ARGS(&tempBufferObject) 


	);



	//好了  接下来就是  CPU 把数据搬进中转站（也就是 Map -> memcpy -> Unmap 的过程）

	BYTE* pData;
	tempBufferObject->Map(0, nullptr, reinterpret_cast<void**>(&pData)); //cpu 通过这个函数获取了 GPU某个资源内存块的地址
																		//这里是0表示第一个子资源  nullptr表示整个资源都映射了  reinterpret_cast<void**>(&pData)是把pData的地址转换成void**类型的地址  因为Map函数需要一个void**类型的参数来接收映射后的地址
																		//reinterpret_cast<void**>是C++的一个类型转换操作符，用于在不同类型之间进行强制转换。
																		// 在这里，它将pData的地址（BYTE*类型）转换为void**类型，以满足Map函数的参数要求（要求第三个参数是指向无类型指针的指针）


	//对齐   “找好起点，然后一层一层把数据‘码’进 GPU 的内存里
	BYTE* pDstTempBuffer = reinterpret_cast<BYTE*>(pData + subresourceFootprint.Offset); //pData是GPU内存的起始地址  subresourceFootprint.Offset是这个资源在GPU内存中的偏移量 
	                                                                                  //pDstTempBuffer临时仓库的实际地址了（也就是我们要把数据搬到的临时buffer的地址）

	const BYTE* pSrcData = reinterpret_cast<BYTE*>(inData); //准备好 “货源起点”  这个是CPU内存里数据的起始地址  也就是我们要搬运的数据的来源地址（输入的顶点数据）


	//搬运  memcpy(目标地址, 源地址, 拷贝多少字节)
	for (UINT i = 0; i < rowUsed; i++) {
		memcpy(pDstTempBuffer + subresourceFootprint.Footprint.RowPitch * i, pSrcData + rowSizeInBytes * i, rowSizeInBytes);
	}

	//Unmap函数关闭cpu的权限    CPU 就不能再通过 pData 乱改这块内存了（避免和 GPU 抢着用导致冲突）
	tempBufferObject->Unmap(0, nullptr);

	//GPU内部拷贝  通过命令列表把中转站的内容搬到真正的vbo里  这个过程是GPU内部完成的  不会占用CPU资源
	//inCommandList->CopyBufferRegion(
	//	bufferObject,           // [1] 目标：最终仓库（默认堆的 VBO）
	//	0,                      // [2] 目标偏移：从最终仓库的第 0 个字节开始放
	//	tempBufferObject,       // [3] 源：临时中转站（上传堆）
	//	0,                      // [4] 源偏移：从中转站的第 0 个字节开始取
	//	subresourceFootprint.Footprint.Width // [5] 拷贝大小：整个资源的宽度
	//);

	inCommandList->CopyBufferRegion(bufferObject, 0, tempBufferObject, 0, subresourceFootprint.Footprint.Width);


	//准备一个“状态转换”的指令  把bufferObject 从D3D12_RESOURCE_STATE_COPY_DEST 转化到  最终的状态（需要输入）
	//真相：GPU 搬运的时候，bufferObject 的身份是“搬运目的地（COPY_DEST）”。
	//切换：搬完后，你得告诉 GPU：“现在把它变成‘顶点缓冲区（VERTEX_BUFFER）’”。

	D3D12_RESOURCE_BARRIER barrier = InitResourceBarrier(bufferObject, D3D12_RESOURCE_STATE_COPY_DEST, inFinalResourceState);

	inCommandList->ResourceBarrier(1, &barrier); //：强制 GPU 在切换身份前完成所有未完成的工作，并清空相关的缓存（Cache）。
	return bufferObject;
	


}


///PSO
ID3D12PipelineState* createPSO(ID3D12RootSignature* inD3D12RootSignature,D3D12_SHADER_BYTECODE inVertexShader, D3D12_SHADER_BYTECODE inPixelShader)
{

	//D3D12_INPUT_ELEMENT_DESC 这个结构体的成员分别是：语义名称（POSITION）、语义索引（0）、数据格式（DXGI_FORMAT_R32G32B32_FLOAT）、输入槽（0）、对齐偏移量（因为是连续内存 cpu需要知道从哪里开始）、输入数据分类（D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA，表示每个顶点都有独立的数据）、实例数据步长率（0，表示不使用实例数据）。  
	// 这个结构体数组定义了顶点数据的布局，告诉 GPU 每个顶点的数据由哪些部分组成，以及它们在内存中的排列方式。
	// 要打包为数组哈 每个成员都是一个D3D12_INPUT_ELEMENT_DESC结构体
	D3D12_INPUT_ELEMENT_DESC vertexDataElementDesc[] =
	{
		{ "POSITION", 0, DXGI_FORMAT_R32G32B32A32_FLOAT, 0, 0, D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA, 0 }, //DXGI_FORMAT_R32G32B32A32_FLOAT就是float4  rgba嘛
		{ "TEXCOORD", 0, DXGI_FORMAT_R32G32B32A32_FLOAT, 0, sizeof(float) * 4, D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA, 0},  //texcoor 和 0的参数合在一起就是TEXCOORD0
		{ "NORMAL", 0, DXGI_FORMAT_R32G32B32A32_FLOAT, 0, sizeof(float) * 8, D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA, 0 }
	};


	D3D12_INPUT_LAYOUT_DESC vertexDataLayoutDesc = {};
	vertexDataLayoutDesc.NumElements = 3; // 为什么是3？ 因为我们定义了3个元素：POSITION、TEXCOORD、NORMAL
	vertexDataLayoutDesc.pInputElementDescs = vertexDataElementDesc; //输入布局描述结构体  


	D3D12_GRAPHICS_PIPELINE_STATE_DESC posDesc = {};
	posDesc.pRootSignature = inD3D12RootSignature; //根签名
	posDesc.VS = inVertexShader; //顶点着色器
	posDesc.PS = inPixelShader; //像素着色器
	posDesc.RTVFormats[0] = DXGI_FORMAT_R8G8B8A8_UNORM; //RT格式  这个要和交换链的格式一致
	posDesc.DSVFormat = DXGI_FORMAT_D24_UNORM_S8_UINT; //DS格式  这个要和你创建depth buffer的格式一致
	posDesc.SampleDesc.Count = 1; //MSAA数量  这个要和交换链的设置一致
	posDesc.SampleDesc.Quality = 0; //MSAA质量  这个要和交换链的设置一致
	posDesc.SampleMask = 0xffffffff; //采样掩码  32位？

	posDesc.InputLayout = vertexDataLayoutDesc; //输入布局描述结构体
	posDesc.PrimitiveTopologyType = D3D12_PRIMITIVE_TOPOLOGY_TYPE_TRIANGLE; //图元类型  这个是三角形列表  还有线段列表 线段带 三角形扇等等

	posDesc.RasterizerState.FillMode = D3D12_FILL_MODE_SOLID; //光栅化状态  填充模式  实心填充  还有线框模式 这个里是实心
	posDesc.RasterizerState.CullMode = D3D12_CULL_MODE_BACK; //剔除模式  剔除背面  还有正面剔除 和 不剔除

	//下面几个默认都是FALSE 其实都可以不写的 这里为了展示写出来
	posDesc.RasterizerState.FrontCounterClockwise = FALSE; //顺时针为正面  这个要和你的顶点数据的定义保持一致  因为你定义的顶点数据是按照顺时针还是逆时针排列的  如果不一致 可能会导致你模型的某些面被错误地剔除掉
	posDesc.RasterizerState.DepthBias = D3D12_DEFAULT_DEPTH_BIAS; //深度偏移  这里D3D12_DEFAULT_DEPTH_BIAS就是FALSE
	posDesc.RasterizerState.DepthBiasClamp = D3D12_DEFAULT_DEPTH_BIAS_CLAMP; //深度偏移夹紧  这里D3D12_DEFAULT_DEPTH_BIAS_CLAMP就是FALSE
	posDesc.RasterizerState.SlopeScaledDepthBias = D3D12_DEFAULT_SLOPE_SCALED_DEPTH_BIAS; //斜率缩放深度偏移  这里D3D12_DEFAULT_SLOPE_SCALED_DEPTH_BIAS就是FALSE
	posDesc.RasterizerState.MultisampleEnable = FALSE; //多重采样启用  这里是FALSE  因为我们没有开启MSAA
	posDesc.RasterizerState.AntialiasedLineEnable = FALSE; //抗锯齿线启用  这里是FALSE  因为我们没有画线段
	posDesc.RasterizerState.ForcedSampleCount = 0; //强制采样数量  这里是0  表示不强制
	posDesc.RasterizerState.ConservativeRaster = D3D12_CONSERVATIVE_RASTERIZATION_MODE_OFF; //保守光栅化  这里是关闭的  因为我们不需要这个功能


	posDesc.RasterizerState.DepthClipEnable = TRUE; //启用深度裁剪  这个一般都要启用  否则可能会导致一些深度问题


	posDesc.DepthStencilState.DepthEnable = TRUE; //启用深度测试  这个一般都要启用  否则你的3D场景会乱套
	posDesc.DepthStencilState.DepthWriteMask = D3D12_DEPTH_WRITE_MASK_ALL; //深度写入掩码  写入所有深度值	
	posDesc.DepthStencilState.DepthFunc = D3D12_COMPARISON_FUNC_LESS_EQUAL; //深度比较函数  小于等于 即近的覆盖远的

	D3D12_RENDER_TARGET_BLEND_DESC rtBlendDesc = {}; //默认渲染目标混合描述结构体
	rtBlendDesc.BlendEnable = FALSE; //混合启用  这里是FALSE  表示不启用混合
	rtBlendDesc.LogicOpEnable = FALSE; //逻辑操作启用  这里是FALSE  表示不启用逻辑操作
	rtBlendDesc.SrcBlend = D3D12_BLEND_ONE; //源混合因子  这里是一个默认值  因为我们没有启用混合
	rtBlendDesc.DestBlend = D3D12_BLEND_ZERO; //目标混合因子  这里是一个默认值  因为我们没有启用混合
	rtBlendDesc.BlendOp = D3D12_BLEND_OP_ADD; //混合操作  这里是一个默认值  因为我们没有启用混合

	rtBlendDesc.SrcBlendAlpha = D3D12_BLEND_ONE; //源混合因子（Alpha）  这里是一个默认值  因为我们没有启用混合
	rtBlendDesc.DestBlendAlpha = D3D12_BLEND_ZERO; //目标混合因子（Alpha）  这里是一个默认值  因为我们没有启用混合
	rtBlendDesc.BlendOpAlpha = D3D12_BLEND_OP_ADD; //混合操作（Alpha）  这里是一个默认值  因为我们没有启用混合

	rtBlendDesc.LogicOp = D3D12_LOGIC_OP_NOOP; //逻辑操作  这里是一个默认值  因为我们没有启用逻辑操作
	rtBlendDesc.RenderTargetWriteMask = D3D12_COLOR_WRITE_ENABLE_ALL; //渲染目标写入掩码  写入所有颜色分量

	for(int i = 0;i < D3D12_SIMULTANEOUS_RENDER_TARGET_COUNT;++i)
	{
		posDesc.BlendState.RenderTarget[i] = rtBlendDesc; //把上面定义的混合描述结构体，赋值给管线状态描述结构体里每个渲染目标的混合状态
														  //当然实际上是可以每个都不一样的哈
	}

	posDesc.NumRenderTargets = 1;//渲染目标数量  这里是1  因为我们交换链的格式里只有一个RT  如果你交换链里有多个RT 这里就要改成对应的数量

	ID3D12PipelineState* D3D12PSO = nullptr; //管线状态对象指针

	HRESULT hResult = gD3D12Device->CreateGraphicsPipelineState(&posDesc, IID_PPV_ARGS(&D3D12PSO)); //创建管线状态对象  通过设备（逻辑显卡）调用 CreateGraphicsPipelineState 方法，传入管线状态描述结构体的地址，以及一个二级指针来接收创建成功后的管线状态对象地址。

	if (FAILED(hResult)) //如果创建失败
	{
		return nullptr; //返回空指针
	}
	
	return D3D12PSO; //返回管线状态对象指针
	
}







//Resource Barrier（资源屏障）
//封装「创建状态屏障切换」的函数
//在显卡内部，同一块显存（resource）在不同的阶段有不同的用途（state）。
//一个是用来展示（显示器），一个是用来画画 所以要切换
D3D12_RESOURCE_BARRIER InitResourceBarrier(ID3D12Resource* inResource, D3D12_RESOURCE_STATES inPrevState, D3D12_RESOURCE_STATES inNextState)   //LYY
{


	D3D12_RESOURCE_BARRIER d3d12ResourceBarrier;

	//把结构体内存清零，避免垃圾值导致配置错误（新手必备，防止未知bug）
	memset(&d3d12ResourceBarrier, 0, sizeof(d3d12ResourceBarrier));

	//最常用的屏障类型，表示“状态切换
	d3d12ResourceBarrier.Type = D3D12_RESOURCE_BARRIER_TYPE_TRANSITION;

	// 屏障标志：无特殊设置（默认即可，无需额外配置）
	d3d12ResourceBarrier.Flags = D3D12_RESOURCE_BARRIER_FLAG_NONE;


	//指定「要切换状态的资源」（比如 gColorRTs 里的交换链纹理）
	d3d12ResourceBarrier.Transition.pResource = inResource;


	//换前的旧状态
	d3d12ResourceBarrier.Transition.StateBefore = inPrevState;

	//切换后的新状态
	d3d12ResourceBarrier.Transition.StateAfter = inNextState;

	//指定「要切换的子资源」：所有子资源（新手阶段纹理只有1个子资源，默认即可）
	d3d12ResourceBarrier.Transition.Subresource = D3D12_RESOURCE_BARRIER_ALL_SUBRESOURCES;

	return d3d12ResourceBarrier;

}




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

		if(desc.Flags&DXGI_ADAPTER_FLAG_SOFTWARE)  // DXGI_ADAPTER_FLAG_SOFTWARE是软件模拟显卡  这是“位掩码”的写法 记住“Flags & 枚举值”的写法   并且内存要大于你设定的值 这里我设置的是512
		{
			continue; // 如果是软件模拟显卡 或者是  显存
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

		adapterIndex++; //索引++  注意这个index是和具体显卡绑定的  这里其实是可以拿到这个index的 有了这个index 你可以用这个index去创建CommittedResource（申请显存你得知道你要在哪个显卡上申请啊）

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


	D3D12_COMMAND_QUEUE_DESC d3d12CommandQueueDesc = {}; //命令队列描述结构体 12开始就要依赖command queue了  以前是直接设备画图
	hResult = gD3D12Device->CreateCommandQueue(&d3d12CommandQueueDesc, IID_PPV_ARGS(&gCommandQueue)); //创建命令队列  是逻辑显卡（Device） 和 显卡的 中间桥梁  通过命令队列把渲染命令传给显卡  这里看到是用 Device 去创建命令队列的 因为命令队列是逻辑显卡的一个子系统
																										//如果你电脑上有两块显卡（独显和核显），你会创建两个 Device。每个 Device 创建出的 Command Queue 只会把活儿发给它自己的那个 GPU。
																										//这里传入空的二级指针 创建成功的命令队列地址会写到 gCommandQueue 指针里

	if (FAILED(hResult))
	{
		return false;
	}


	DXGI_SWAP_CHAIN_DESC swapChainDesc = {}; //交换链描述结构体  是屏幕和显卡之间的桥梁（相当于中间的几个画布 先画好再传给屏幕）  既然是屏幕和显卡的桥梁 那么你看结构体的成员就全是关于画布的描述 

	swapChainDesc.BufferCount = 2; //双缓冲  或者 3缓冲  单缓冲一般会卡  双缓冲性能可以开到6成	3缓冲是一定能跑满显示器帧率和显卡负载的（利用率极致啊bro
	swapChainDesc.BufferDesc = {}; //缓冲区描述结构体
	swapChainDesc.BufferUsage = DXGI_USAGE_RENDER_TARGET_OUTPUT; //缓冲区用作渲染目标输出
	swapChainDesc.BufferDesc.Width = inWidth;  //画布宽
	swapChainDesc.BufferDesc.Height = inHeight; //画布高
	swapChainDesc.BufferDesc.Format = DXGI_FORMAT_R8G8B8A8_UNORM; //画布格式  32位颜色格式 8位红绿蓝alpha
	swapChainDesc.OutputWindow = inhwnd; //输出窗口句柄
	swapChainDesc.SampleDesc.Count = 1; //多重采样数量MSAA  1表示不使用多重采样
	swapChainDesc.Windowed = TRUE; //窗口模式 没有全屏
	swapChainDesc.SwapEffect = DXGI_SWAP_EFFECT_FLIP_DISCARD; //交换效果 丢弃模式  这个模式只支持双缓冲和三缓冲


	IDXGISwapChain* swapChain = nullptr; //交换链临时指针  这里不是3接口 因为CreateSwapChain只能创建到2接口（这是低版本的 所以下面要转到高版本）
	dxgiFactory->CreateSwapChain(
		gCommandQueue, //命令队列指针  通过命令队列把渲染好的画布传给交换链
		&swapChainDesc, //交换链描述结构体地址
		&swapChain //交换链指针二级地址
	);

	gSwapChain = static_cast<IDXGISwapChain3*>(swapChain);//交换链指针 转成3接口指针 赋值给全局变量 
														  //static_cast关键字 静态类型转换 强制转化 语法是static_cast<目标类型>(待转换变量/指针)
	
														  
														  
														  //IDXGISwapChain3 和 IDXGISwapChain 是COM 接口的版本迭代关系，高版本接口 IDXGISwapChain3 继承自低版本接口 IDXGISwapChain（就像 C++ 中子类继承父类），两者完全兼容，所以可以用 static_cast 进行转换。  
														  // 这样做的原因是版本迭代后，低版本接口的方法可能不够用，需要使用高版本接口新增的方法和功能。



	

	D3D12_HEAP_PROPERTIES d3dHeapProperties = {}; //堆属性结构体  你要申请显存 得告诉显卡这个读写的具体位置
												//包括了默认堆  上传堆 回读堆 三种 D3D12_HEAP_TYPE_DEFAULT  和 D3D12_HEAP_TYPE_UPLOAD 和D3D12_HEAP_TYPE_READBACK
												//权限分别是GPU专属读写   CPU读GPU写   GPU写CPU读
												
	d3dHeapProperties.Type = D3D12_HEAP_TYPE_DEFAULT; //默认堆 
	//d3dHeapProperties.CreationNodeMask = 0;   //你在哪个显卡上创建这个内存 这里比较麻烦 就是对于多显卡的电脑
	//d3dHeapProperties.VisibleNodeMask = 0;   //这里都是写0  是指向我自己电脑的第0号GPU  但是实际项目不是这么写的 要指向实际GPU的index的

	D3D12_RESOURCE_DESC d3d12ResourceDesc = {}; //资源描述符结构体
	d3d12ResourceDesc.Dimension = D3D12_RESOURCE_DIMENSION_TEXTURE2D; //是Texture 
	d3d12ResourceDesc.Alignment = 0;
	d3d12ResourceDesc.Width = inWidth;
	d3d12ResourceDesc.Height = inHeight;
	d3d12ResourceDesc.DepthOrArraySize = 1; //1张贴图 如果是cubemap 给6（因为cubemap 本质是一个array）
	d3d12ResourceDesc.MipLevels = 1; //需要多少层级的mip？ 
	d3d12ResourceDesc.Format = DXGI_FORMAT_D24_UNORM_S8_UINT;  //这就是D24格式 记得吗  24位存深度（Depth），8位存模板（Stencil）
	d3d12ResourceDesc.SampleDesc.Count = 1; //没有MSAA这些
	d3d12ResourceDesc.SampleDesc.Quality = 0;  //关闭MSAA的化 这里必须是0  只有开启了MSAA才有意义 
	d3d12ResourceDesc.Layout = D3D12_TEXTURE_LAYOUT_UNKNOWN;  //就是创建出来用来干啥的  有纹理采样  有用来当RT的  目前先设置为UKNOWN 
	d3d12ResourceDesc.Flags = D3D12_RESOURCE_FLAG_ALLOW_DEPTH_STENCIL; //这是啥子？


	D3D12_CLEAR_VALUE dsClearValue = {}; //预设的扫除值结构体  显卡渲染每一帧前，都要清空深度缓冲  如果你告诉显卡：“我以后每次清空深度图都会用 1.0 这个值”，显卡硬件会针对这个特定值进行电路级优化（叫做 Fast Clear）。如果你后续清空时的值和这里设的不一样，渲染就会变慢。
	dsClearValue.Format = DXGI_FORMAT_D24_UNORM_S8_UINT;
	dsClearValue.DepthStencil.Depth = 1.0f;
	dsClearValue.DepthStencil.Stencil = 0;

	gD3D12Device->CreateCommittedResource(
		&d3dHeapProperties,
		D3D12_HEAP_FLAG_NONE,
		&d3d12ResourceDesc,
		D3D12_RESOURCE_STATE_DEPTH_WRITE, //表示初始化出来就可以被写入深度
		&dsClearValue,
		IID_PPV_ARGS(&gDSRT)
	
	);


	/////////////////////////////现在开始初始化RTV 和 DSV  就是RT和Depth buffer的使用说明书  


	D3D12_DESCRIPTOR_HEAP_DESC d3dDescriptorHeapDescRTV = {}; //就是为了CreateDescriptorHeap 函数提供「创建描述符堆的「配置清单 / 需求说明书」」
	d3dDescriptorHeapDescRTV.NumDescriptors = 2; //需要两  因为Swap Chain我们设置是双缓冲  有两RT 需要两
	d3dDescriptorHeapDescRTV.Type = D3D12_DESCRIPTOR_HEAP_TYPE_RTV; //类型是RTV

	gD3D12Device->CreateDescriptorHeap(
		&d3dDescriptorHeapDescRTV,
		IID_PPV_ARGS(&gSwapChainRTVHeap)
	); //这句只是在给 RTV 申请“宿舍（内存）
	gRTVDescriptorSize = gD3D12Device->GetDescriptorHandleIncrementSize(D3D12_DESCRIPTOR_HEAP_TYPE_RTV); //获取RTV描述符大小  每个显卡厂商的显卡  这个值都不一样  所以要动态获取 态获取每个 RTV 描述符占用的内存大小。
	//因为不同显卡厂商的驱动，对描述符的内存布局定义不同，这个值不是固定的（可能是 32 字节、64 字节等）。后续你要访问「第二个 RTV」时，需要通过「第一个 RTV 句柄 + gRTVDescriptorSize」来计算它的偏移量，找到对应的描述符（相当于「钥匙柜的第二个抽屉」）。



	D3D12_DESCRIPTOR_HEAP_DESC d3dDescriptorHeapDescDSV = {}; //
	d3dDescriptorHeapDescDSV.NumDescriptors = 1; //DSV我们只需要一个 因为交互链只需要一个depth buffer
	d3dDescriptorHeapDescDSV.Type = D3D12_DESCRIPTOR_HEAP_TYPE_DSV; //类型是DSV

	gD3D12Device->CreateDescriptorHeap(
		&d3dDescriptorHeapDescDSV,
		IID_PPV_ARGS(&gSwapChainDSVHeap)
	); //给DSV申请内存
	gDSVDescriptorSize = gD3D12Device->GetDescriptorHandleIncrementSize(D3D12_DESCRIPTOR_HEAP_TYPE_DSV); //获取DSV描述符大小  每个显卡厂商的显卡  这个值都不一样  所以要动态获取


	D3D12_CPU_DESCRIPTOR_HANDLE rtvHeapStart = gSwapChainRTVHeap->GetCPUDescriptorHandleForHeapStart(); // 你刚申请的那块内存的第一个内存地址（这里是RTV的申请内存）


	// 此时创建真正的RTV 用的CreateRenderTargetView函数
	for (int i = 0;i < 2;i++) //循环两次 对应双缓冲
	{
		gSwapChain->GetBuffer(i, IID_PPV_ARGS(&gColorRTs[i]));  //// 绑定的 RT 资源载体（交换链自动创建的缓冲区）  从交换链（gSwapChain）中，取出第 i 个后台缓冲区资源，存入 gColorRTs[i] 数组中（这就是你之前疑惑的「交换链自动创建的 RT 资源」）。
		D3D12_CPU_DESCRIPTOR_HANDLE rtvPointer;  //定义临时的 RTV 具体位置句柄
		rtvPointer.ptr = rtvHeapStart.ptr + i * gRTVDescriptorSize;  //计算 RTV 的具体存储位置   初始地址 +   RTV 描述符占用的内存大小 * i
		gD3D12Device->CreateRenderTargetView(
			gColorRTs[i],
			nullptr,
			rtvPointer //上一句计算好的地址
		); //创建真正的 RTV 并存入堆中

	}

	// 这又是啥？ 定义 DSV 的「自定义配置说明书」结构体
	D3D12_DEPTH_STENCIL_VIEW_DESC d3dDSViewDesc = {};
	d3dDSViewDesc.Format = DXGI_FORMAT_D24_UNORM_S8_UINT;
	d3dDSViewDesc.ViewDimension = D3D12_DSV_DIMENSION_TEXTURE2D;

	//此时创建真正的DSV
	gD3D12Device->CreateDepthStencilView(gDSRT, &d3dDSViewDesc, gSwapChainDSVHeap->GetCPUDescriptorHandleForHeapStart()); //这里链接了gDSRT（即为depthrt开辟的内存块）



	////////////////////////////////////////////////////-----------------------命令分配器和命令列表的创建

	gD3D12Device->CreateCommandAllocator(
		D3D12_COMMAND_LIST_TYPE_DIRECT, //直接命令列表类型 这个能用来画图 能用computer shader  还有诸如BUNDLE（多线程）  还有COPY（拷贝命令列表）  还有COMPUTE（专门computer shader）
		IID_PPV_ARGS(&gCommandAllocator)
	); //创建命令分配器   这是“纸” 负责内存


	gD3D12Device->CreateCommandList(
		0,  //0号GPU
		D3D12_COMMAND_LIST_TYPE_DIRECT,
		gCommandAllocator,//传入命令分配器：获取内存（纸张）
		nullptr,
		IID_PPV_ARGS(&gCommandList)
	); //创建命令列表   这是“笔” 负责记录


	///CommandAllocator 和 CommandList 和 CommandQueue 之间的关系总结： 纸（内存）  笔（记录） 和  传送带


	///////////////////////////////////////////////////-------------------------cpu和GPU的同步机制 使用Fence 和 Event


	 //这是栅栏 是计数器 cpu  
	//1. CPU 命令 GPU：“干完这些活后，把 Fence 的值加到 1。”
	//2. CPU 就在旁边等着，看 Fence 是不是到 1 了。
	//3. 到了 1，说明 GPU 之前的活干完了，CPU 才能放心地重用内存。
	gD3D12Device->CreateFence(0, D3D12_FENCE_FLAG_NONE, IID_PPV_ARGS(&gFence));



	//这是事件 是一个闹钟 CPU 如果想死等 GPU，它不能一直死循环盯着 Fence（那太费电了）。它会创建一个 Event，告诉系统：“等 Fence 到某个值的时候，提醒我
	gFenceEvent = CreateEvent(nullptr, FALSE, FALSE, nullptr); //这是啥


	gCurrentRTIndex = gSwapChain->GetCurrentBackBufferIndex(); //这个函数是交换链的一个方法  用来获取当前要渲染的后台缓冲区索引  也就是你要画到哪个RT上面去画  双缓冲就是0和1交替变化
															   //即每帧都在变换哦 


	return true;
}


//检查：看一眼 GPU 进度（GetCompletedValue）。
//登记：如果没干完，告诉系统“干完后叫醒我”（SetEventOnCompletion）。
//休眠：CPU 闭眼睡觉（WaitForSingleObject）。

void WaitForCompletionOfCommandList()
{
	if (gFence->GetCompletedValue() < gFenceValue) //查询fence的值 如果返回值 < 目标值：说明 GPU 还在干活（进度牌数字 < 目标编号），需要进入等待流程；    如果返回值 ≥ 目标值：说明 GPU 已经干完了（进度牌数字 ≥ 目标编号），直接跳过等待，CPU 继续干活。
	{
		gFence->SetEventOnCompletion(gFenceValue, gFenceEvent); //设置闹钟 首先这句能执行 那肯定是GPU还在干活    翻译成人话就是：预约一个“完工闹钟“
		                                                        //告诉系统：“等 Fence（计数器）的值达到 gFenceValue（目标编号）时，触发 gFenceEvent（闹钟事件）”，提醒 CPU 可以继续工作了。 
																//这个其实是DX12帮你封装的  不然你需要一直循环去查询fence的值  这样很浪费CPU资源

		WaitForSingleObject(gFenceEvent, INFINITE);   //这是 Windows 系统的 API（不是 D3D12 专属）。它会让当前的 CPU 线程进入阻塞状态（Sleep），不再消耗 CPU 循环
													//就是没完工的化 ，cpu就是休眠状态
													//当然你这样让cpu休眠 那肯定在项目里不是这样的 这是演示用的 每帧硬同步
													// 实际项目是 ：CPU 会去干别的活儿（比如准备下一帧的数据），等到下一帧需要用到 GPU 结果时，再来检查 Fence 的值，看 GPU 有没有干完。如果没干完，CPU 再等；干完了，CPU 再继续。 “多帧入队”（Multi-frame In-flight" cpu永远比gpu领先 GPU 1-2 帧 
													

	}

}




void EndCommandList()
{

	gCommandList->Close(); // 停止记录命令（胶带封箱）  这句必须要的  不然你提交不了命令列表  就像你写完一篇文章不按保存键一样  这时候命令列表就像是「草稿」，还不能让显卡看到。调用 Close() 就是「保存草稿」，让它变成「正式文档」，显卡才能读取和执行里面的指令了。

	ID3D12CommandList* ppCommandListes[] = { gCommandList }; //CommandList 写满指令的「快递单」 
															 //ppCommandListes 数组 = 装快递单的「快递箱」（哪怕只有 1 张快递单，也要放进箱子里）


	gCommandQueue->ExecuteCommandLists(1, ppCommandListes); //PP 的意思是 Pointer to Pointer
															//这一步就是cpu 往GPU 发送CommandLists   第一个参数代表几个（如果数组里有3个commandlist 就填3）  第二个是CommandList的地址
															//CommandLists本身就是一揽子指令 为什么还要多个commandlist在打包进list数组呢？  因为这就是可以 它支持一次性提交多个命令列表啊
															



	//标记里程碑
	gFenceValue += 1;

	// 2. 让 CommandQueue（快递员）给 Fence（计数器）发信号： 完成之后 设置这批指令的目标编号
	//意思是 这“篮子”活儿干完了，你再把数字改了(把fence值设置为fence value)
	gCommandQueue->Signal(gFence, gFenceValue);

}



//在显卡内部，同一块内存（Resource）在不同的阶段有不同的用途（State）。
//PRESENT（呈现状态）：显存块正在被“显示器”读取，用来把画面显示到屏幕上。
//RENDER_TARGET（渲染目标状态）：显存块正在被“渲染管线”写入，GPU 正在上面画画。

void BeginRenderToSwapChain(ID3D12GraphicsCommandList* inCommandList)  //ID3D12GraphicsCommandList 是ID3D12CommandList的字接口  图形渲染专用子接口（ 如画画 画三角形、设置流水线、清空画布、以及你刚才看到的资源屏障 ResourceBarrier）
{
	D3D12_RESOURCE_BARRIER barrier = InitResourceBarrier(gColorRTs[gCurrentRTIndex],D3D12_RESOURCE_STATE_PRESENT,D3D12_RESOURCE_STATE_RENDER_TARGET);//从显示切换到准备画画	

	//把资源屏障提交到命令列表，告诉 GPU：「立即执行这个资源状态切换」
//    第一个参数：屏障数量（这里只有1个，新手阶段都是1个）
//    第二个参数：屏障的地址（传入刚才创建的 barrier）
	inCommandList->ResourceBarrier(1, &barrier);


	//对RT 和 DSV 获取他们的实际的内存地址
	D3D12_CPU_DESCRIPTOR_HANDLE colorRT,dsv;  //定义两个CPU描述符句柄（用来定位RT和DSV））

	dsv.ptr = gSwapChainDSVHeap->GetCPUDescriptorHandleForHeapStart().ptr; //GetCPUDescriptorHandleForHeapStart获取的是内存 的句柄（内存的表示） .ptr之后获取到具体的内存地址（指针）  因为我们DSV堆里只有一个DSV 所以直接用起始地址就行了
	colorRT.ptr = gSwapChainRTVHeap->GetCPUDescriptorHandleForHeapStart().ptr + gCurrentRTIndex * gRTVDescriptorSize; //计算当前RTV的具体位置  因为我们之前把两个RTV放在一起了  所以要根据当前索引（0或1）来计算出当前帧要用哪个RT的地址


	//把画板挂到流水线上（输出合并阶段）
	inCommandList->OMSetRenderTargets(1, &colorRT, FALSE, &dsv); //设置渲染目标（Output Merger 阶段）  
																//这句的意思是：告诉 GPU，接下来我要开始画画了，画板（Render Target）和深度缓冲（Depth Stencil View）都准备好了，你们把它们挂到「输出合并阶段」的接口上吧！  
																// 这样 GPU 就知道了，后续的绘制命令要往哪个画板上画，以及深度测试要用哪个深度缓冲了。
																//1代表直往一个画布上画  DX12支持画8个  即MRT
																//FALSE 表示这些 RenderTarget 句柄在内存中不是连续存放的（通常单张画图都填 FALSE）。


	//设定画笔的作用范围（视口与裁剪）
	//下面两都是结构体  定义视口和裁剪矩形
	D3D12_VIEWPORT viewport = {0.0f,0.0f,1280.0f,720.0f};

	D3D12_RECT scissorRect = { 0,0,1280,720 };

	inCommandList->RSSetViewports(1, &viewport); //设置视口  视口是个矩形区域 定义了最终画面在屏幕上的位置和大小  这里设置成全屏
	inCommandList->RSSetScissorRects(1, &scissorRect); //设置裁剪矩形  定义了哪些像素可以被渲染到屏幕上  这里设置成和视口一样大  也就是不裁剪



	const float clearColor[] = { 0.1f,0.4f,0.6f,1.0f }; //清屏颜色 RGBA

	inCommandList->ClearRenderTargetView(colorRT, clearColor, 0, nullptr); //清空当前RTV对应的资源（画布）  用上面定义的颜色来清空  这里是全屏清空 因为裁剪矩形和视口一样大
	inCommandList->ClearDepthStencilView(dsv, D3D12_CLEAR_FLAG_DEPTH | D3D12_CLEAR_FLAG_STENCIL, 1.0f, 0, 0, nullptr); //清空DSV对应的资源（深度缓冲）  这里是全屏清空 因为裁剪矩形和视口一样大


}


	
void EndRenderToSwapChain(ID3D12GraphicsCommandList* inCommandList)  //
{
	D3D12_RESOURCE_BARRIER barrier = InitResourceBarrier(gColorRTs[gCurrentRTIndex], D3D12_RESOURCE_STATE_RENDER_TARGET, D3D12_RESOURCE_STATE_PRESENT); //画完了切换显示

	inCommandList->ResourceBarrier(1, &barrier);

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




	//他这个顶点的数据直接写在了dx12那边了啊 但是现实中是导入模型的啊。。。
	float vertexData[] = {

		//第一个点的数据        
		-0.5f, -0.5f, 0.0f, 1.0f, //positiion
		 1.0f, 0.0f, 0.0f, 1.0f, // 红色
		 0.0f,  0.0f, 0.0f, 0.0f,  //normal

		 //第二个点的数据
		  0.0f,  0.5f, 0.5f, 1.0f, //positiion
		  0.0f, 1.0f, 0.0f, 1.0f, // 绿色
		  0.0f,  0.0f, 0.0f, 0.0f,  //normal

		  //第三个点的数据
		  0.5f, -0.5f, 0.5f, 1.0f, //positiion
		  0.0f, 0.0f, 1.0f, 1.0f, // 蓝色
		  0.0f,  0.0f, 0.0f, 0.0f,  //normal                 
	};






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

			WaitForCompletionOfCommandList();//CPU 和 GPU同步   CPU 先等 GPU 干完上一帧的所有活，再开始新一帧的渲染  所以放在最开始

			gCommandAllocator->Reset();  //这个reset的作用是？ 清空内存   以及为什么使用命令分配器的方法？因为命令分配器是负责内存的 

			gCommandList->Reset(gCommandAllocator, nullptr);


		
			BeginRenderToSwapChain(gCommandList); //画布状态切换 + 擦除画布
												//把当前要渲染的后台缓冲区（RT）从「显示状态（PRESENT）」切换为「渲染状态（RENDER_TARGET）」；
												//告诉 GPU：“接下来要往这个 RT 和深度缓冲里画东西”（绑定 RTV / DSV）；
												//设定「画面显示的区域」（视口 / 裁剪矩形）；
												//清空上一帧的画面（RT）和深度数据（DSV），准备新的绘制；

			//draw


			//End

			EndRenderToSwapChain(gCommandList); //画布状态切换

			EndCommandList(); // commandlist设置停止记录命令  然后正式提交commandlist到gpu（要把commandlist打包到数组 然后用批提交函数ExecuteCommandLists）  然后gFenceValue加1

			
			gSwapChain->Present(0, 0);   //切换！后变前！（把渲染好的后台缓冲区切换为前台缓冲区，显示到屏幕上）  第一个参数 0 (SyncInterval)：垂直同步开关。

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














