#include "StaticMeshComponent.h"
#include "BattleFireDirect.h"
#include <stdio.h>


void StaticMeshComponent::SetVertexCount(int inVertexCount)
{
	mVertexCount = inVertexCount;
	//申请内存
	mVertexData = new StaticMeshComponentVertexData[inVertexCount];//实例化StaticMeshComponemtVertexData结构体  并且是连续的inVertexCount 数量的结构体(即结构体数组)

	memset(mVertexData, 0, sizeof(StaticMeshComponentVertexData) * inVertexCount); //把申请的内存清零  避免垃圾值导致未知bug

}



void StaticMeshComponent::SetVertexPosition(int inIndex, float inX, float inY, float inZ, float inW)
{
	mVertexData[inIndex].mPosition[0] = inX;
	mVertexData[inIndex].mPosition[1] = inY;
	mVertexData[inIndex].mPosition[2] = inZ;
	mVertexData[inIndex].mPosition[3] = inW;
}


void StaticMeshComponent::SetVertexTexcoord(int inIndex, float inX, float inY, float inZ, float inW)
{
	mVertexData[inIndex].mTexcoord[0] = inX;
	mVertexData[inIndex].mTexcoord[1] = inY;
	mVertexData[inIndex].mTexcoord[2] = inZ;
	mVertexData[inIndex].mTexcoord[3] = inW;

}



void StaticMeshComponent::SetVertexNormal(int inIndex, float inX, float inY, float inZ, float inW)
{
	mVertexData[inIndex].mNormal[0] = inX;
	mVertexData[inIndex].mNormal[1] = inY;
	mVertexData[inIndex].mNormal[2] = inZ;
	mVertexData[inIndex].mNormal[3] = inW;



}


void StaticMeshComponent::SetVertexTangent(int inIndex, float inX, float inY, float inZ, float inW)
{

	mVertexData[inIndex].mTangent[0] = inX;
	mVertexData[inIndex].mTangent[1] = inY;
	mVertexData[inIndex].mTangent[2] = inZ;
	mVertexData[inIndex].mTangent[3] = inW;
}


void StaticMeshComponent::InitFromFile(ID3D12GraphicsCommandList* inCommandList,const char* inFilePath)
{

	FILE* pFile = nullptr;
	errno_t err = fopen_s(&pFile, inFilePath, "rb");
	if (err == 0)
	{
		int temp = 0;
		fread(&temp, 4, 1, pFile); //读取4个字节到temp中 也就是读取一个int类型的数据(int 占四字节)   这是读取顶点数量
		mVertexCount = temp; //将读取到的顶点数量赋值给成员变量
		mVertexData = new StaticMeshComponentVertexData[mVertexCount]; //根据顶点数量分配内存
		fread(mVertexData, 1, sizeof(StaticMeshComponentVertexData) * mVertexCount, pFile); //?读取顶点数据到mVertexData中 这里的1是每次读取的字节数 sizeof(StaticMeshComponentVertexData) * mVertexCount是总共要读取的字节数

		mVBO = CreateBufferObject(inCommandList, mVertexData,
			sizeof(StaticMeshComponentVertexData) * mVertexCount,
			D3D12_RESOURCE_STATE_VERTEX_AND_CONSTANT_BUFFER);



		mVBOView.BufferLocation = mVBO->GetGPUVirtualAddress();
		mVBOView.SizeInBytes = sizeof(StaticMeshComponentVertexData) * mVertexCount;
		mVBOView.StrideInBytes = sizeof(StaticMeshComponentVertexData);


		///submesh 是名字 和索引 这样的结构体  这里是读取submesh数据的循环
		while (!feof(pFile)) //循环读取文件直到文件末尾
		{

			fread(&temp, 4, 1, pFile); //先读取一个int类型的数据 也就是名字的长度？

			if (feof(pFile))
			{
				break;
			}

			char name[265] = { 0 }; //定义一个字符数组来存储名字 这里的265是为了存储最长的名字(最多了) 加上一个结束符
			fread(name, 1, temp, pFile); //读取名字到name中 这里的temp是名字的长度
			fread(&temp, 4, 1, pFile); //当前submesh有多少个索引 也就是索引数量

			SubMesh* subMesh = new SubMesh(); //来自staticMeshComponent.h的结构体 实例化一下
			subMesh->mIndexCount = temp; //将读取到的索引数量赋值给subMesh的成员变量

			unsigned int* indexes = new unsigned int[temp]; //根据索引数量分配内存来存储索引数据

			fread(indexes, 1, sizeof(unsigned int) * temp, pFile); //读取索引数据到indexes中 这里的temp是索引数量


			//初始化IBO 和 IBView 
			subMesh->mIBO = CreateBufferObject(inCommandList, indexes,  //为什么subMesh可以用mIBO？ SubMesh结构体中有一个成员变量mIBO，所以可以通过subMesh来访问mIBO
				sizeof(unsigned int) * temp,
				D3D12_RESOURCE_STATE_INDEX_BUFFER);

			//IBO VIEW的常规设置
			subMesh->mIBView.BufferLocation = subMesh->mIBO->GetGPUVirtualAddress(); //设置索引缓冲区视图的缓冲区位置为IBO的GPU虚拟地址
			subMesh->mIBView.Format = DXGI_FORMAT_R32_UINT; //设置索引缓冲区视图的格式为32位无符号整数
			subMesh->mIBView.SizeInBytes = sizeof(unsigned int) * temp; //  设置索引缓冲区视图的大小为索引数量乘以每个索引的字节数（4字节）


			mSubMeshes.insert(std::pair<std::string, SubMesh*>(name, subMesh));//将名字和subMesh指针插入到unordered_map中 以名字为键 subMesh结构体指针为值

			delete[] indexes; //释放索引数据的内存

		}
		fclose(pFile); //关闭文件

	}



}



void StaticMeshComponent::Render(ID3D12GraphicsCommandList* inCommandList)
{

	D3D12_VERTEX_BUFFER_VIEW vbos[] = { mVBOView };

	inCommandList->IASetVertexBuffers(0, 1, vbos); //绑定vbo

	if (mSubMeshes.empty())
	{

		inCommandList->DrawInstanced(mVertexCount, 1, 0, 0);

	}

	else
	{
		for (auto iter = mSubMeshes.begin();iter != mSubMeshes.end(); iter++)
		{
			inCommandList->IASetIndexBuffer(&iter->second->mIBView);
			inCommandList->DrawIndexedInstanced(iter->second->mIndexCount, 1, 0, 0, 0);
		}

	}

}
