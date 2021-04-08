Shader "myShader/shader01"
{
    Properties
    {
        _Color("main color", color) = (1, 1, 1, 1)
        _Ambient("main ambient", color) = (0.3, 0.3, 0.3, 0.3)
        _Specular("main specular", color) = (1, 1, 1, 1)
        _Shininess("Shininess", range(0, 4)) = 1.0
        _Emission("Emission", color) = (1, 1, 1, 1)
        _MainTex("mainTex", 2d) = ""{}
        _SecondTex("secondTex", 2d) = ""{}
        _Constant("constant", color) = (1, 1, 1, 0.3)
    }

    SubShader
    {
        Tags { "Queue" = "Transparent" }
        Pass
        {
            Blend SrcAlpha OneMinusSrcAlpha // Traditional transparency
           // color[_Color]
           material
           {
               diffuse[_Color]
               ambient[_Ambient]
               specular[_Specular]
               shininess[_Shininess]
               Emission[_Emission]
               
           }
           lighting on
           separatespecular on
           settexture[_MainTex]
           {
               combine texture * primary double
           }
           settexture[_SecondTex]
           {
               constantColor[_Constant]
               combine texture * previous double, texture * constant
           }
        }
    }
}