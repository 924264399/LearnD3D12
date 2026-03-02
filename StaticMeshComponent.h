#pragma once
#include <d3d12.h>  

#include <unordered_map>
#include <string>



struct StaticMeshComponentVertexData
{
	float mPosition[4];
	float mTexcoord[4];
	float mNormal[4];
	float mTangent[4];
};



struct SubMesh
{
	ID3D12Resource* mIBO;
	D3D12_INDEX_BUFFER_VIEW mIBView;


	int mIndexCount;


};





class StaticMeshComponent
{
public:
	ID3D12Resource* mVBO;
	D3D12_VERTEX_BUFFER_VIEW mVBOView;

	StaticMeshComponentVertexData* mVertexData; //声明StaticMeshComponemtVertexData的指针 实例化在SetVertexCount函数内

	int mVertexCount;

	std::unordered_map<std::string, SubMesh*> mSubMeshes;; //使用unordered_map来存储submesh数据 以名字为键 submesh结构体指针为值


	// 设置有多少个点
	void SetVertexCount(int inVertexCount);


	//设置某个点的位置  以及为什么要设置w分量？ 因为在shader里是用float4来接收的啊  还有就是为了对齐  因为GPU内存访问是按照一定的规则对齐的  如果不对齐就会有性能问题甚至错误
	void SetVertexPosition(int inIndex, float inX, float inY, float inZ, float inW = 1.0f);

	void SetVertexTexcoord(int inIndex, float inX, float inY, float inZ, float inW = 1.0f);

	void SetVertexNormal(int inIndex, float inX, float inY, float inZ, float inW = 1.0f);


	void SetVertexTangent(int inIndex, float inX, float inY, float inZ, float inW = 1.0f);

	void InitFromFile(ID3D12GraphicsCommandList* inCommandList, const char* inFilePath);


	void Render(ID3D12GraphicsCommandList* inCommandList);

};

