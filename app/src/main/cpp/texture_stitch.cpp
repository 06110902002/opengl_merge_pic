// 包含头文件
#include "texture_stitch.h"
#include <cmath>
#include <algorithm>
#include <cstring>

// 定义全局纹理拼接器实例指针，初始化为nullptr
static TextureStitcher* gStitcher = nullptr;

// TextureStitcher类的构造函数
TextureStitcher::TextureStitcher()
        : mProgram(0), mVAO(0), mVBO(0), mEBO(0),
          mViewportWidth(0), mViewportHeight(0),
          mInitialized(false), mAssetManager(nullptr) {
    // 输出构造函数调用日志
    LOGI("TextureStitcher constructor called");

    // 初始化变换参数
    mTransform.scale = 1.0f;        // 初始缩放为1（不缩放）
    mTransform.translateX = 0.0f;   // 初始X平移为0
    mTransform.translateY = 0.0f;   // 初始Y平移为0
    mTransform.minScale = 0.5f;     // 最小缩放0.5倍
    mTransform.maxScale = 3.0f;     // 最大缩放3倍
}

// TextureStitcher类的析构函数
TextureStitcher::~TextureStitcher() {
    // 调用cleanup函数清理资源
    cleanup();
}

// 检查OpenGL错误的辅助函数
void TextureStitcher::checkGLError(const char* operation) {
    // 定义OpenGL错误码变量
    GLenum error;
    // 循环检查直到没有更多错误
    while ((error = glGetError()) != GL_NO_ERROR) {
        // 输出错误信息，包括操作名称和错误码
        LOGE("OpenGL error during %s: 0x%04X", operation, error);
    }
}

// 从assets目录加载shader文件的函数
std::string TextureStitcher::loadShaderFromAssets(AAssetManager* assetManager, const char* shaderPath) {
    // 输出正在加载的shader文件路径
    LOGI("Loading shader from: %s", shaderPath);
    // 创建空字符串用于存储shader代码
    std::string shaderCode;
    // 打开assets中的shader文件
    AAsset* asset = AAssetManager_open(assetManager, shaderPath, AASSET_MODE_BUFFER);
    // 检查文件是否成功打开
    if (asset) {
        // 获取文件长度
        size_t length = AAsset_getLength(asset);
        // 输出文件大小信息
        LOGI("Shader file size: %zu", length);
        // 调整字符串大小以容纳文件内容
        shaderCode.resize(length);
        // 读取文件内容到字符串中
        AAsset_read(asset, &shaderCode[0], length);
        // 关闭文件资源
        AAsset_close(asset);
        // 输出成功加载日志
        LOGI("Successfully loaded shader: %s", shaderPath);
    } else {
        // 输出加载失败日志
        LOGE("Failed to load shader from assets: %s", shaderPath);
        // 如果加载失败，使用硬编码shader作为备用方案
        if (strcmp(shaderPath, "shaders/vertex_shader.glsl") == 0) {
            // 备用顶点着色器代码
            shaderCode = "#version 300 es\nlayout(location=0)in vec3 aPos;layout(location=1)in vec2 aTexCoord;out vec2 TexCoord;void main(){gl_Position=vec4(aPos,1.0);TexCoord=aTexCoord;}";
        } else {
            // 备用片段着色器代码
            shaderCode = "#version 300 es\nprecision mediump float;in vec2 TexCoord;out vec4 FragColor;uniform sampler2D texture0;void main(){FragColor=texture(texture0,TexCoord);}";
        }
        // 输出使用备用shader的日志
        LOGI("Using fallback shader for: %s", shaderPath);
    }
    // 返回shader代码字符串
    return shaderCode;
}

// 初始化TextureStitcher的函数
bool TextureStitcher::initialize(AAssetManager* assetManager) {
    // 输出初始化开始日志
    LOGI("initialize called");
    // 检查是否已经初始化过
    if (mInitialized) {
        // 如果已初始化，直接返回true
        LOGI("Already initialized");
        return true;
    }

    // 保存AssetManager指针供后续使用
    mAssetManager = assetManager;
    // 输出AssetManager设置成功日志
    LOGI("AssetManager set");

    // 从assets加载顶点着色器代码
    std::string vertexShaderCode = loadShaderFromAssets(assetManager, "shaders/vertex_shader.glsl");
    // 从assets加载片段着色器代码
    std::string fragmentShaderCode = loadShaderFromAssets(assetManager, "shaders/fragment_shader.glsl");

    // 输出创建shader程序的日志
    LOGI("Creating shader program");
    // 创建着色器程序
    mProgram = createProgram(vertexShaderCode.c_str(), fragmentShaderCode.c_str());
    // 检查着色器程序是否创建成功
    if (mProgram == 0) {
        // 输出创建失败日志
        LOGE("Failed to create shader program");
        return false;
    }
    // 输出着色器程序创建成功日志，包含程序ID
    LOGI("Shader program created: %d", mProgram);

    // 生成顶点数组对象(VAO)
    glGenVertexArrays(1, &mVAO);
    // 生成顶点缓冲对象(VBO)
    glGenBuffers(1, &mVBO);
    // 生成元素缓冲对象(EBO)
    glGenBuffers(1, &mEBO);
    // 输出生成的OpenGL对象ID
    LOGI("OpenGL objects generated: VAO=%d, VBO=%d, EBO=%d", mVAO, mVBO, mEBO);

    // 检查初始化过程中的OpenGL错误
    checkGLError("initialize");

    // 设置初始化标志为true
    mInitialized = true;
    // 输出初始化成功日志
    LOGI("TextureStitcher initialized successfully");
    // 返回初始化成功
    return true;
}

// 设置OpenGL视口大小的函数
void TextureStitcher::setViewport(int width, int height) {
    // 输出视口设置日志，包含宽度和高度
    LOGI("setViewport: %dx%d", width, height);
    // 保存视口宽度
    mViewportWidth = width;
    // 保存视口高度
    mViewportHeight = height;
    // 设置OpenGL视口
    glViewport(0, 0, width, height);
}

// 添加图片到纹理拼接器的函数
bool TextureStitcher::addImage(void* pixels, int width, int height) {
    // 输出添加图片的日志，包含图片尺寸和像素指针
    LOGI("addImage called: %dx%d", width, height);

    // 检查像素数据是否为空
    if (!pixels) {
        // 输出空像素数据错误日志
        LOGE("Null pixels provided");
        return false;
    }

    // 创建纹理信息结构体
    TextureInfo textureInfo;
    // 设置纹理宽度
    textureInfo.width = width;
    // 设置纹理高度
    textureInfo.height = height;

    // 生成纹理对象
    glGenTextures(1, &textureInfo.textureId);
    // 输出生成的纹理ID
    LOGI("Generated texture ID: %d", textureInfo.textureId);

    // 绑定纹理到GL_TEXTURE_2D目标
    glBindTexture(GL_TEXTURE_2D, textureInfo.textureId);

    // 设置纹理S方向（水平方向）的包装方式为边缘钳制
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
    // 设置纹理T方向（垂直方向）的包装方式为边缘钳制
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
    // 设置纹理缩小过滤方式为线性过滤
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
    // 设置纹理放大过滤方式为线性过滤
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);

    // 上传纹理数据到GPU
    glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, width, height, 0,
                 GL_RGBA, GL_UNSIGNED_BYTE, pixels);

    // 检查纹理上传过程中的OpenGL错误
    checkGLError("addImage");

    // 将纹理信息添加到纹理数组中
    mTextures.push_back(textureInfo);
    // 输出纹理添加成功日志，包含当前纹理总数
    LOGI("Texture added successfully. Total textures: %zu", mTextures.size());
    // 返回添加成功
    return true;
}

// 对单个顶点应用变换的函数
void TextureStitcher::applyTransformToVertex(Vertex& vertex) {
    // 先缩放，后平移的变换顺序
    vertex.position[0] = vertex.position[0] * mTransform.scale + mTransform.translateX;
    vertex.position[1] = vertex.position[1] * mTransform.scale + mTransform.translateY;
    // Z坐标保持不变
    vertex.position[2] = vertex.position[2];
}

// 更新顶点数据应用当前变换的函数
void TextureStitcher::updateVerticesWithTransform() {
    // 清空变换后的顶点数据
    mTransformedVertices.clear();

    // 对每个原始顶点应用变换
    for (const auto& originalVertex : mVertices) {
        Vertex transformedVertex = originalVertex;
        applyTransformToVertex(transformedVertex);
        mTransformedVertices.push_back(transformedVertex);
    }
}

// 处理缩放手势的函数
void TextureStitcher::handleScale(float scaleFactor, float focusX, float focusY) {
    // 输出缩放信息日志
    LOGI("handleScale: factor=%.2f, focus=(%.1f, %.1f)", scaleFactor, focusX, focusY);

    // 计算新的缩放值
    float newScale = mTransform.scale * scaleFactor;

    // 限制缩放范围
    if (newScale < mTransform.minScale) {
        newScale = mTransform.minScale;
    } else if (newScale > mTransform.maxScale) {
        newScale = mTransform.maxScale;
    }

    // 如果缩放值没有变化，直接返回
    if (newScale == mTransform.scale) {
        return;
    }

    // 计算缩放中心在OpenGL坐标系中的位置
    // 将屏幕坐标转换为OpenGL标准化设备坐标
    float glFocusX = (focusX / mViewportWidth) * 2.0f - 1.0f;
    float glFocusY = 1.0f - (focusY / mViewportHeight) * 2.0f;

    // 调整平移以使缩放围绕焦点进行
    // 缩放前后的坐标关系：newPos = focus + (oldPos - focus) * (newScale/oldScale)
    float scaleRatio = newScale / mTransform.scale;
    mTransform.translateX = glFocusX + (mTransform.translateX - glFocusX) * scaleRatio;
    mTransform.translateY = glFocusY + (mTransform.translateY - glFocusY) * scaleRatio;

    // 更新缩放值
    mTransform.scale = newScale;

    // 输出变换状态日志
    LOGI("Transform updated: scale=%.2f, translate=(%.2f, %.2f)",
         mTransform.scale, mTransform.translateX, mTransform.translateY);

    // 更新顶点数据
    updateVerticesWithTransform();
}

// 处理拖动手势的函数
void TextureStitcher::handleDrag(float dx, float dy) {
    // 将像素移动量转换为OpenGL坐标系中的移动量
    // OpenGL坐标系范围是[-1,1]，所以需要根据视口大小进行转换
    float glDx = (dx / mViewportWidth) * 2.0f;
    float glDy = -(dy / mViewportHeight) * 2.0f; // Y轴方向相反

    // 应用平移
    mTransform.translateX += glDx;
    mTransform.translateY += glDy;

    // 输出拖动信息日志
    LOGI("handleDrag: dx=%.1f, dy=%.1f, glDx=%.3f, glDy=%.3f, newTranslate=(%.2f, %.2f)",
         dx, dy, glDx, glDy, mTransform.translateX, mTransform.translateY);

    // 更新顶点数据
    updateVerticesWithTransform();
}

// 重置变换到初始状态的函数
void TextureStitcher::resetTransform() {
    // 输出重置日志
    LOGI("resetTransform called");

    // 重置所有变换参数
    mTransform.scale = 1.0f;
    mTransform.translateX = 0.0f;
    mTransform.translateY = 0.0f;

    // 更新顶点数据
    updateVerticesWithTransform();

    // 输出重置完成日志
    LOGI("Transform reset to identity");
}

// 计算图片布局的函数，确保图片间无重叠
void TextureStitcher::calculateLayout() {
    // 输出布局计算开始日志，包含当前纹理数量
    LOGI("calculateLayout called, texture count: %zu", mTextures.size());

    // 检查是否有纹理需要布局
    if (mTextures.empty()) {
        // 输出无纹理错误日志
        LOGE("No textures to layout");
        return;
    }

    // 设置网格列数为2
    int cols = 2;
    // 计算需要的行数（向上取整）
    int rows = (mTextures.size() + cols - 1) / cols;
    // 输出网格布局信息
    LOGI("Grid layout: %dx%d", cols, rows);

    // 计算每列的宽度（OpenGL坐标范围[-1,1]）
    float colWidth = 2.0f / cols;
    // 计算每行的高度
    float rowHeight = 2.0f / rows;

    // 清空原始顶点数据
    mVertices.clear();
    mIndices.clear();

    // 遍历所有纹理，计算每个纹理的位置
    for (int i = 0; i < mTextures.size(); ++i) {
        // 计算当前纹理所在的行
        int row = i / cols;
        // 计算当前纹理所在的列
        int col = i % cols;

        // 设置边距为0，确保图片紧密排列无缝隙
        float margin = 0.0f;
        // 计算左下角x坐标
        float x = -1.0f + col * colWidth + margin;
        // 计算左下角y坐标
        float y = 1.0f - row * rowHeight - margin;
        // 计算矩形宽度
        float width = colWidth - 2 * margin;
        // 计算矩形高度
        float height = rowHeight - 2 * margin;

        // 创建4个顶点，定义矩形的位置和纹理坐标
        Vertex vertices[4] = {
                // 左下角顶点：位置坐标和纹理坐标（修复了纹理颠倒问题）
                { {x, y - height, 0.0f}, {0.0f, 1.0f} },
                // 右下角顶点：位置坐标和纹理坐标
                { {x + width, y - height, 0.0f}, {1.0f, 1.0f} },
                // 右上角顶点：位置坐标和纹理坐标
                { {x + width, y, 0.0f}, {1.0f, 0.0f} },
                // 左上角顶点：位置坐标和纹理坐标
                { {x, y, 0.0f}, {0.0f, 0.0f} }
        };

        // 将4个顶点添加到顶点数组中
        for (int j = 0; j < 4; ++j) {
            mVertices.push_back(vertices[j]);
        }

        // 计算当前矩形的起始顶点索引
        GLuint baseIndex = i * 4;
        // 添加三角形索引，用两个三角形组成一个矩形
        mIndices.insert(mIndices.end(), {
                // 第一个三角形：左下->右下->右上
                baseIndex, baseIndex + 1, baseIndex + 2,
                // 第二个三角形：左下->右上->左上
                baseIndex, baseIndex + 2, baseIndex + 3
        });
    }

    // 计算布局后立即应用当前变换
    updateVerticesWithTransform();

    // 输出布局计算完成日志，包含顶点和索引数量
    LOGI("Layout calculated: %zu vertices, %zu indices", mVertices.size(), mIndices.size());
}

// 创建顶点数据的函数，将数据上传到GPU
void TextureStitcher::createVertexData() {
    // 输出创建顶点数据开始日志
    LOGI("createVertexData called");

    // 检查顶点数据是否为空
    if (mTransformedVertices.empty() || mIndices.empty()) {
        // 输出无顶点数据错误日志
        LOGE("No vertex data to create");
        return;
    }

    // 绑定顶点数组对象
    glBindVertexArray(mVAO);

    // 绑定顶点缓冲对象
    glBindBuffer(GL_ARRAY_BUFFER, mVBO);
    // 上传顶点数据到GPU（使用变换后的顶点数据）
    glBufferData(GL_ARRAY_BUFFER,
                 mTransformedVertices.size() * sizeof(Vertex),
                 mTransformedVertices.data(), GL_DYNAMIC_DRAW); // 改为DYNAMIC_DRAW因为数据会频繁更新

    // 绑定元素缓冲对象
    glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, mEBO);
    // 上传索引数据到GPU
    glBufferData(GL_ELEMENT_ARRAY_BUFFER,
                 mIndices.size() * sizeof(GLuint),
                 mIndices.data(), GL_STATIC_DRAW);

    // 设置顶点位置属性指针
    glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, sizeof(Vertex), (void*)0);
    // 启用顶点位置属性
    glEnableVertexAttribArray(0);

    // 设置纹理坐标属性指针
    glVertexAttribPointer(1, 2, GL_FLOAT, GL_FALSE, sizeof(Vertex),
                          (void*)offsetof(Vertex, texCoord));
    // 启用纹理坐标属性
    glEnableVertexAttribArray(1);

    // 解绑顶点数组对象
    glBindVertexArray(0);
    // 检查顶点数据创建过程中的OpenGL错误
    checkGLError("createVertexData");

    // 输出顶点数据创建成功日志
    LOGI("Vertex data created successfully with transform");
}

// 渲染函数，绘制所有纹理
void TextureStitcher::render() {
    // 设置清除颜色为深蓝色
    glClearColor(0.2f, 0.3f, 0.3f, 1.0f);
    // 清除颜色缓冲区
    glClear(GL_COLOR_BUFFER_BIT);

    // 检查是否已初始化
    if (!mInitialized) {
        // 输出未初始化错误日志
        LOGE("Not initialized, cannot render");
        return;
    }

    // 检查是否有纹理需要渲染
    if (mTextures.empty()) {
        // 输出无纹理日志
        LOGI("No textures to render");
        return;
    }

    // 输出开始渲染日志，包含纹理数量
    LOGI("Rendering %zu textures", mTextures.size());

    // 计算图片布局
    calculateLayout();
    // 创建顶点数据
    createVertexData();

    // 使用着色器程序
    glUseProgram(mProgram);

    // 获取纹理采样器uniform的位置
    GLint textureLoc = glGetUniformLocation(mProgram, "texture0");
    // 检查uniform位置是否有效
    if (textureLoc != -1) {
        // 设置纹理单元为0
        glUniform1i(textureLoc, 0);
    }

    // 遍历所有纹理进行渲染
    for (int i = 0; i < mTextures.size(); ++i) {
        // 输出正在渲染的纹理信息
        LOGI("Rendering texture %d: ID=%d", i, mTextures[i].textureId);

        // 激活纹理单元0
        glActiveTexture(GL_TEXTURE0);
        // 绑定当前纹理
        glBindTexture(GL_TEXTURE_2D, mTextures[i].textureId);

        // 绑定顶点数组对象
        glBindVertexArray(mVAO);
        // 绘制元素（两个三角形组成的矩形）
        glDrawElements(GL_TRIANGLES, 6, GL_UNSIGNED_INT,
                       (void*)(i * 6 * sizeof(GLuint)));

        // 检查渲染过程中的OpenGL错误
        checkGLError("render texture");
    }

    // 解绑顶点数组对象
    glBindVertexArray(0);
    // 输出渲染完成日志
    LOGI("Render completed");
}

// 清空纹理的方法
void TextureStitcher::clearTextures() {
    // 输出清空纹理开始日志
    LOGI("clearTextures called, texture count: %zu", mTextures.size());

    // 删除所有纹理对象
    for (auto& tex : mTextures) {
        if (tex.textureId) {
            // 删除纹理对象
            glDeleteTextures(1, &tex.textureId);
            // 输出删除纹理日志
            LOGI("Deleted texture: %d", tex.textureId);
        }
    }
    // 清空纹理数组
    mTextures.clear();
    // 清空顶点数据
    mVertices.clear();
    // 清空变换后的顶点数据
    mTransformedVertices.clear();
    // 清空索引数据
    mIndices.clear();
    // 输出清空完成日志
    LOGI("All textures cleared");
}

// 清理资源的函数
void TextureStitcher::cleanup() {
    // 输出清理开始日志
    LOGI("cleanup called");
    // 删除着色器程序
    if (mProgram) {
        glDeleteProgram(mProgram);
        mProgram = 0;
        // 输出删除程序日志
        LOGI("Shader program deleted");
    }
    // 删除顶点数组对象
    if (mVAO) {
        glDeleteVertexArrays(1, &mVAO);
        mVAO = 0;
        // 输出删除VAO日志
        LOGI("VAO deleted");
    }
    // 删除顶点缓冲对象
    if (mVBO) {
        glDeleteBuffers(1, &mVBO);
        mVBO = 0;
        // 输出删除VBO日志
        LOGI("VBO deleted");
    }
    // 删除元素缓冲对象
    if (mEBO) {
        glDeleteBuffers(1, &mEBO);
        mEBO = 0;
        // 输出删除EBO日志
        LOGI("EBO deleted");
    }

    // 清空所有纹理
    clearTextures();

    // 重置初始化标志
    mInitialized = false;
    // 重置AssetManager指针
    mAssetManager = nullptr;
    // 输出清理完成日志
    LOGI("TextureStitcher cleanup completed");
}

// 编译着色器的函数
GLuint TextureStitcher::compileShader(GLenum type, const char* source) {
    // 输出编译着色器类型日志
    LOGI("Compiling shader type: %d", type);
    // 创建着色器对象
    GLuint shader = glCreateShader(type);
    // 检查着色器对象是否创建成功
    if (shader == 0) {
        // 输出创建失败日志
        LOGE("Failed to create shader object");
        return 0;
    }

    // 设置着色器源代码
    glShaderSource(shader, 1, &source, nullptr);
    // 编译着色器
    glCompileShader(shader);

    // 检查编译状态
    GLint success;
    glGetShaderiv(shader, GL_COMPILE_STATUS, &success);
    // 检查编译是否成功
    if (!success) {
        // 获取编译错误信息
        char infoLog[512];
        glGetShaderInfoLog(shader, 512, nullptr, infoLog);
        // 输出编译失败日志
        LOGE("Shader compilation failed: %s", infoLog);
        // 删除着色器对象
        glDeleteShader(shader);
        return 0;
    }

    // 输出编译成功日志
    LOGI("Shader compiled successfully");
    // 返回编译成功的着色器对象
    return shader;
}

// 创建着色器程序的函数
GLuint TextureStitcher::createProgram(const char* vertexSource, const char* fragmentSource) {
    // 编译顶点着色器
    GLuint vertexShader = compileShader(GL_VERTEX_SHADER, vertexSource);
    // 检查顶点着色器是否编译成功
    if (vertexShader == 0) {
        return 0;
    }

    // 编译片段着色器
    GLuint fragmentShader = compileShader(GL_FRAGMENT_SHADER, fragmentSource);
    // 检查片段着色器是否编译成功
    if (fragmentShader == 0) {
        // 删除顶点着色器
        glDeleteShader(vertexShader);
        return 0;
    }

    // 创建着色器程序
    GLuint program = glCreateProgram();
    // 检查程序是否创建成功
    if (program == 0) {
        // 输出创建失败日志
        LOGE("Failed to create program object");
        // 删除着色器对象
        glDeleteShader(vertexShader);
        glDeleteShader(fragmentShader);
        return 0;
    }

    // 附加顶点着色器到程序
    glAttachShader(program, vertexShader);
    // 附加片段着色器到程序
    glAttachShader(program, fragmentShader);
    // 链接程序
    glLinkProgram(program);

    // 检查链接状态
    GLint success;
    glGetProgramiv(program, GL_LINK_STATUS, &success);
    // 检查链接是否成功
    if (!success) {
        // 获取链接错误信息
        char infoLog[512];
        glGetProgramInfoLog(program, 512, nullptr, infoLog);
        // 输出链接失败日志
        LOGE("Program linking failed: %s", infoLog);
        // 删除程序
        glDeleteProgram(program);
        program = 0;
    }

    // 删除着色器对象（已链接到程序，不再需要）
    glDeleteShader(vertexShader);
    glDeleteShader(fragmentShader);

    // 检查程序是否创建成功
    if (program != 0) {
        // 输出程序创建成功日志，包含程序ID
        LOGI("Program created successfully: %d", program);
    }

    // 返回着色器程序
    return program;
}

// JNI函数实现区域开始
#ifdef __cplusplus
extern "C" {
#endif

// Surface创建时的JNI函数实现
JNIEXPORT void JNICALL
Java_com_example_imagestitch_MyGLRenderer_nativeSurfaceCreated(JNIEnv *env, jobject thiz,
                                                               jobject asset_manager) {
    // 输出函数调用日志
    LOGI("nativeSurfaceCreated called");
    // 创建纹理拼接器实例（如果不存在）
    if (gStitcher == nullptr) {
        gStitcher = new TextureStitcher();
        // 输出创建成功日志
        LOGI("Created new TextureStitcher instance");
    }

    // 从Java对象获取AAssetManager
    AAssetManager* assetManager = AAssetManager_fromJava(env, asset_manager);
    // 检查AssetManager是否有效
    if (assetManager) {
        // 输出AssetManager获取成功日志
        LOGI("AAssetManager obtained successfully");
        // 初始化纹理拼接器
        if (!gStitcher->initialize(assetManager)) {
            // 输出初始化失败日志
            LOGE("Failed to initialize TextureStitcher");
        } else {
            // 输出初始化成功日志
            LOGI("TextureStitcher initialized successfully");
        }
    } else {
        // 输出AssetManager获取失败日志
        LOGE("Failed to get AAssetManager from Java");
    }
}

// Surface大小改变时的JNI函数实现
JNIEXPORT void JNICALL
Java_com_example_imagestitch_MyGLRenderer_nativeSurfaceChanged(JNIEnv *env, jobject thiz,
                                                               jint width, jint height) {
    // 输出函数调用日志，包含新的视口尺寸
    LOGI("nativeSurfaceChanged: %dx%d", width, height);
    // 设置视口大小
    if (gStitcher) {
        gStitcher->setViewport(width, height);
    }
}

// 绘制帧的JNI函数实现
JNIEXPORT void JNICALL
Java_com_example_imagestitch_MyGLRenderer_nativeDrawFrame(JNIEnv *env, jobject thiz) {
    // 调用渲染函数
    if (gStitcher) {
        gStitcher->render();
    }
}

// 设置图片的JNI函数实现
JNIEXPORT void JNICALL
Java_com_example_imagestitch_MyGLRenderer_nativeSetImages(JNIEnv *env, jobject thiz,
                                                          jobjectArray bitmaps, jint count) {
    // 输出函数调用日志，包含图片数量
    LOGI("nativeSetImages called with %d images", count);

    // 检查gStitcher是否有效
    if (!gStitcher) {
        // 输出gStitcher为空错误日志
        LOGE("gStitcher is null");
        return;
    }

    // 检查图片数量是否有效
    if (count <= 0) {
        // 输出无效图片数量错误日志
        LOGE("Invalid image count: %d", count);
        return;
    }

    // 成功处理图片计数
    int successCount = 0;
    // 遍历所有bitmap
    for (int i = 0; i < count; ++i) {
        // 获取bitmap对象
        jobject bitmap = env->GetObjectArrayElement(bitmaps, i);
        // 检查bitmap是否为空
        if (bitmap == nullptr) {
            // 输出bitmap为空错误日志
            LOGE("Bitmap %d is null", i);
            continue;
        }

        // 定义bitmap信息结构体
        AndroidBitmapInfo info;
        // 获取bitmap信息
        if (AndroidBitmap_getInfo(env, bitmap, &info) != ANDROID_BITMAP_RESULT_SUCCESS) {
            // 输出获取bitmap信息失败日志
            LOGE("Failed to get bitmap info for bitmap %d", i);
            // 删除本地引用
            env->DeleteLocalRef(bitmap);
            continue;
        }

        // 输出bitmap信息日志
        LOGI("Bitmap %d: %dx%d, format: %d", i, info.width, info.height, info.format);

        // 检查bitmap格式是否为RGBA_8888
        if (info.format != ANDROID_BITMAP_FORMAT_RGBA_8888) {
            // 输出不支持的格式错误日志
            LOGE("Unsupported bitmap format: %d", info.format);
            // 删除本地引用
            env->DeleteLocalRef(bitmap);
            continue;
        }

        // 定义像素数据指针
        void* pixels;
        // 锁定bitmap像素
        if (AndroidBitmap_lockPixels(env, bitmap, &pixels) != ANDROID_BITMAP_RESULT_SUCCESS) {
            // 输出锁定像素失败日志
            LOGE("Failed to lock pixels for bitmap %d", i);
            // 删除本地引用
            env->DeleteLocalRef(bitmap);
            continue;
        }

        // 添加图片到拼接器
        if (gStitcher->addImage(pixels, info.width, info.height)) {
            // 增加成功计数
            successCount++;
            // 输出添加成功日志
            LOGI("Successfully added bitmap %d", i);
        } else {
            // 输出添加失败日志
            LOGE("Failed to add bitmap %d", i);
        }

        // 解锁bitmap像素
        AndroidBitmap_unlockPixels(env, bitmap);
        // 删除本地引用
        env->DeleteLocalRef(bitmap);
    }

    // 输出处理结果日志
    LOGI("Image processing completed: %d/%d successful", successCount, count);
}

// 清理资源的JNI函数实现
JNIEXPORT void JNICALL
Java_com_example_imagestitch_MyGLRenderer_nativeCleanup(JNIEnv *env, jobject thiz) {
    // 输出清理函数调用日志
    LOGI("nativeCleanup called");
    // 检查gStitcher是否存在
    if (gStitcher) {
        // 清空所有纹理
        gStitcher->clearTextures();
        // 输出清理完成日志
        LOGI("Native cleanup completed");
    } else {
        // 输出gStitcher不存在日志
        LOGE("gStitcher is null in nativeCleanup");
    }
}

// 处理缩放手势的JNI函数实现
JNIEXPORT void JNICALL
Java_com_example_imagestitch_MyGLRenderer_nativeHandleScale(JNIEnv *env, jobject thiz,
                                                            jfloat scaleFactor, jfloat focusX, jfloat focusY) {
    // 输出JNI缩放调用日志
    LOGI("nativeHandleScale called: factor=%.2f, focus=(%.1f, %.1f)", scaleFactor, focusX, focusY);
    // 检查gStitcher是否存在
    if (gStitcher) {
        // 调用缩放处理函数
        gStitcher->handleScale(scaleFactor, focusX, focusY);
    } else {
        // 输出gStitcher不存在错误日志
        LOGE("gStitcher is null in nativeHandleScale");
    }
}

// 处理拖动手势的JNI函数实现
JNIEXPORT void JNICALL
Java_com_example_imagestitch_MyGLRenderer_nativeHandleDrag(JNIEnv *env, jobject thiz,
                                                           jfloat dx, jfloat dy) {
    // 输出JNI拖动调用日志
    LOGI("nativeHandleDrag called: dx=%.1f, dy=%.1f", dx, dy);
    // 检查gStitcher是否存在
    if (gStitcher) {
        // 调用拖动处理函数
        gStitcher->handleDrag(dx, dy);
    } else {
        // 输出gStitcher不存在错误日志
        LOGE("gStitcher is null in nativeHandleDrag");
    }
}

// 重置变换的JNI函数实现
JNIEXPORT void JNICALL
Java_com_example_imagestitch_MyGLRenderer_nativeResetTransform(JNIEnv *env, jobject thiz) {
    // 输出JNI重置变换调用日志
    LOGI("nativeResetTransform called");
    // 检查gStitcher是否存在
    if (gStitcher) {
        // 调用重置变换函数
        gStitcher->resetTransform();
    } else {
        // 输出gStitcher不存在错误日志
        LOGE("gStitcher is null in nativeResetTransform");
    }
}

}
// JNI函数实现