
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
    float4 texcoord : TEXCOORD0; 
    float4 positionWS : TEXCOORD1; 
};





VSout MainVS(VertexDate inVertexData){

    VSout vo;

    //变换
    float4 posWS  = mul(ModelMatrix,inVertexData.position); // 这样是列矩阵  如果调换位置是行矩阵
    float4 posVS = mul(ViewMatrix,posWS);
    vo.position = mul(ProjectionMatrix,posVS);


    vo.normal = mul(IT_ModelMatrix,inVertexData.normal); 
    vo.positionWS = posWS;
    vo.texcoord = inVertexData.texcoord;

    //vo.normal = inVertexData.normal;
    return vo;
}




half4 MainPS(VSout inPSInput) : SV_TARGET{

    float3 N=normalize(inPSInput.normal.xyz); 
    float3 L = normalize(float3(1.0f,1.0f,-1.0f)); //光源方向 斜上方 
    half3 LightColor = half3(0.1h,0.4h,0.6h); 

    float3 CameraPosWS = float3(0.0f,0.0f,-5.0f); //相机位置 世界空间
    float3 V = normalize(CameraPosWS - inPSInput.positionWS.xyz); 
    float3 R = normalize(reflect(-L, N)); //反射向量
    float3 H = normalize(L + V); //半程向量

    half3 bottomColor = half3(0.1h,0.4h,0.6h);
    half3 topColor = half3(0.7h,0.7h,0.7h);

    
    float theta = asin(N.y); //法线与y轴的夹角 -PI/2 ~ PI/2
    theta/= PI; //归一化到0~1之间
    theta+=0.5f; //调整到0~1之间

    half ambientIns = 0.2h; 

    half3 ambientColor = lerp(bottomColor,topColor,theta) * ambientIns; 

   
    half NoL = saturate(dot(N,L)); 
    half NoH = saturate(dot(N,H));
     


    half3 diffuseColor = LightColor * NoL; 

    half3 specularColor = pow(NoH,16.0h) * LightColor ; 

    half3 surfaceColor = ambientColor + diffuseColor + specularColor;  

    return half4(surfaceColor,1.0); 

}
