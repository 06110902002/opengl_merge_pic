#version 300 es
// 设置浮点数精度为中等精度
precision mediump float;
// 定义从顶点着色器输入的纹理坐标
in vec2 TexCoord;
// 定义最终输出的颜色值
out vec4 FragColor;
// 定义2D纹理采样器uniform变量
uniform sampler2D texture0;
// 主函数开始
void main() {
    // 从纹理采样器texture0中根据纹理坐标TexCoord采样颜色值
    FragColor = texture(texture0, TexCoord);
}
// 主函数结束