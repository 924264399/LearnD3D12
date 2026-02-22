
//这个是最原生的状态 
//unity的Properties { _MainTex ("Texture", 2D) = "white" {} } 这种写法 实际上是为了兼容api又套了一层壳
cbuffer globalConstants:register(b0){

    float4 color; //美术可调参数
    
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

    vo.position = inVertexData.position;  //ou 这个为了掩饰 直接就是ndc空间的坐标了  所以不需要进行mvp矩阵变换
    vo.color = inVertexData.texcoord + color;  //???  怎么啥都没 没矩阵变化 颜色也没有

    return vo;
}




float4 MainPS(VSout inPSInput) : SV_TARGET{

    return inPSInput.color;
}   
