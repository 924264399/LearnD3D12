
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


#define PI 3.1415


struct VertexDate{

    float4 position : POSITION;
    float4 texcoord : TEXCOORD0;

    float4 normal : NORMAL;
    float4 tangent : TANGENT;
};



struct VSout{

    float4 position : SV_POSITION;
    float4 normal : NORMAL;
};





VSout MainVS(VertexDate inVertexData){

    VSout vo;

    //变换
    float4 posWS  = mul(ModelMatrix,inVertexData.position); // 这样是列矩阵  如果调换位置是行矩阵
    float4 posVS = mul(ViewMatrix,posWS);
    vo.position = mul(ProjectionMatrix,posVS);


    vo.normal = inVertexData.normal; 
    return vo;
}




half4 MainPS(VSout inPSInput) : SV_TARGET{

    float3 N=normalize(inPSInput.normal.xyz); 


    half3 bottomColor = half3(0.1h,0.4h,0.6h);
    half3 topColor = half3(0.7h,0.7h,0.7h);

    
    float theta = acos(N.y); //法线与y轴的夹角 -PI/2 ~ PI/2
    theta/= PI; //归一化到0~1之间
    theta+=0.5f; //调整到0~1之间

    half ambientIns = 0.2h; 

    half3 ambientColor = lerp(bottomColor,topColor,theta) * ambientIns; 

    half3 diffuseColor = half3(0.0h,0.0h,0.0h); 

    half3 specularColor = half3(0.0h,0.0h,0.0h); 

    half3 surfaceColor = ambientColor + diffuseColor + specularColor;  

    return half4(surfaceColor,1.0); 

}
