
//这个是最原生的状态 
//unity的Properties { _MainTex ("Texture", 2D) = "white" {} } 这种写法 实际上是为了兼容api又套了一层壳
cbuffer globalConstants:register(b0){

    float4 color; //美术可调参数
    
};


cbuffer DefaultVertexCB:register(b1){

    float4x4 ProjectionMatrix; 
    float4x4 ViewMatrix;
    float4x4 ModelMatrix;
    float4x4 IT_ModelMatrix; //模型矩阵的逆转置矩阵 用于法线变换
    float4x4 ReservedMemory[1024]; //预留内存 以防万一  1024个矩阵 4*4 float 每个float4占16字节 16*4=64字节 每个矩阵占64*4=256字节 1024个矩阵占256*1024=262144字节 256KB的内存空间
};



struct VertexDate{

    float4 position : POSITION;
    float4 texcoord : TEXCOORD0;

    float4 normal : NORMAL;
    float4 tangent : TANGENT;
};



struct VSout{

    float4 position : SV_POSITION;
    float4 color : COLOR0;
};





VSout MainVS(VertexDate inVertexData){

    VSout vo;

    //变换
    float4 posWS  = mul(ModelMatrix,inVertexData.position); // 这样是列矩阵  如果调换位置是行矩阵
    float4 posVS = mul(ViewMatrix,posWS);
    vo.position = mul(ProjectionMatrix,posVS);


    vo.color = float4(inVertexData.normal.xyz,1.0); 
    return vo;
}




float4 MainPS(VSout inPSInput) : SV_TARGET{

    float3 ambientColor = float3(0.1,0.1,0.1); //环境光颜色

    float3 diffuseColor = float3(0.0,0.0,0.0); 

    float3 specularColor = float3(0.0,0.0,0.0); 

    float3 surfaceColor = ambientColor + diffuseColor + specularColor;  

    return float4(surfaceColor,1.0); 

}
