#pragma once

#include <d3d12.h>
#include <dxgi1_4.h>
#include <d3dcompiler.h>
#include <DirectXMath.h>
#include <stdio.h>


//创建VBO 还有 IBO的函数
ID3D12Resource* CreateBufferObject(ID3D12GraphicsCommandList* inCommandList,
	void* inData, int inDataLen, D3D12_RESOURCE_STATES inFinalResourceState);

//Resource Barrier（资源屏障）
//封装「创建状态屏障切换」的函数
//在显卡内部，同一块显存（resource）在不同的阶段有不同的用途（state）。
//一个是用来展示（显示器），一个是用来画画 所以要切换
D3D12_RESOURCE_BARRIER InitResourceBarrier(ID3D12Resource* inResource, D3D12_RESOURCE_STATES inPrevState, D3D12_RESOURCE_STATES inNextState);

///初始化rootsignature的函数
ID3D12RootSignature* InitRootSignature();

///编译shader的函数
void CreateShaderFromFile(LPCWSTR inShaderFilePath,
	const char* inMainFunctionName, //主函数名称
	const char* inTarget,  //这个是版本 比如vs_5_0   ps_5_0  ps_4_0  比如 "vs_5_0" "ps_5_0" 比如5.0是基础班  5.1有DX12 专属增强版（支持更多寄存器、动态资源索引等）
	D3D12_SHADER_BYTECODE* inShader);





///用来创建CBV 即 constant buffer view的函数  跟签名那里的 用来传递shader参数到跟签名的常量缓冲区的
ID3D12Resource* CreateConstantBufferObject(int inDataLen);


//CBV 写数据的函数  这个函数里会把数据从CPU内存复制到GPU内存里去
void UpdateConstantBuffer(ID3D12Resource* inCB, void* inData, int inDataLen);


///PSO
ID3D12PipelineState* createPSO(ID3D12RootSignature* inD3D12RootSignature, D3D12_SHADER_BYTECODE inVertexShader, D3D12_SHADER_BYTECODE inPixelShader);



//D3D12初始化函数
bool InitD3D12(HWND inhwnd, int inWidth, int inHeight);



ID3D12GraphicsCommandList* GetCommandList();

ID3D12CommandAllocator* GetCommandAllocator();



//检查：看一眼 GPU 进度（GetCompletedValue）。
//登记：如果没干完，告诉系统“干完后叫醒我”（SetEventOnCompletion）。
//休眠：CPU 闭眼睡觉（WaitForSingleObject）。
void WaitForCompletionOfCommandList();


void EndCommandList();


//在显卡内部，同一块内存（Resource）在不同的阶段有不同的用途（State）。
//PRESENT（呈现状态）：显存块正在被“显示器”读取，用来把画面显示到屏幕上。
//RENDER_TARGET（渲染目标状态）：显存块正在被“渲染管线”写入，GPU 正在上面画画。

void BeginRenderToSwapChain(ID3D12GraphicsCommandList* inCommandList);  //ID3D12GraphicsCommandList 是ID3D12CommandList的字接口  图形渲染专用子接口（ 如画画 画三角形、设置流水线、清空画布、以及你刚才看到的资源屏障 ResourceBarrier）


void EndRenderToSwapChain(ID3D12GraphicsCommandList* inCommandList);


void SwapD3D12Buffers();







