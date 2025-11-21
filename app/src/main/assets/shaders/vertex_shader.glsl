#version 300 es
// 定义顶点位置输入属性，位置索引为0
layout(location = 0) in vec3 aPos;
// 定义纹理坐标输入属性，位置索引为1
layout(location = 1) in vec2 aTexCoord;
// 定义纹理坐标输出变量，传递给片段着色器
out vec2 TexCoord;
// 主函数开始
void main() {
    // 将顶点位置转换为齐次坐标并赋值给内置输出变量gl_Position
    gl_Position = vec4(aPos, 1.0);
    // 将输入的纹理坐标传递给片段着色器
    TexCoord = aTexCoord;
}
// 主函数结束