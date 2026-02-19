#include "StaticMeshComponent.h"


void StaticMeshComponent::SetVertexCount(int inVertexCount)
{
	mVertexCount = inVertexCount;
	//申请内存
	mVertexData = new StaticMeshComponemtVertexData[inVertexCount];//实例化StaticMeshComponemtVertexData结构体

	memset(mVertexData, 0, sizeof(StaticMeshComponemtVertexData) * inVertexCount); //把申请的内存清零  避免垃圾值导致未知bug

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
	mVBOView.BufferLocation = mVBO->GetGPUVirtualAddress();
	mVBOView.SizeInBytes = sizeof(StaticMeshComponemtVertexData) * mVertexCount;
	mVBOView.StrideInBytes = sizeof(StaticMeshComponemtVertexData);



}