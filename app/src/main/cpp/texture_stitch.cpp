#include "texture_stitch.h"
#include <cmath>
#include <algorithm>
#include <cstring>

static TextureStitcher* gStitcher = nullptr;

TextureStitcher::TextureStitcher()
        : mProgram(0), mVAO(0), mVBO(0), mEBO(0),
          mViewportWidth(0), mViewportHeight(0),
          mInitialized(false), mAssetManager(nullptr) {
    LOGI("TextureStitcher constructor called");
}

TextureStitcher::~TextureStitcher() {
    cleanup();
}

void TextureStitcher::checkGLError(const char* operation) {
    GLenum error;
    while ((error = glGetError()) != GL_NO_ERROR) {
        LOGE("OpenGL error during %s: 0x%04X", operation, error);
    }
}

std::string TextureStitcher::loadShaderFromAssets(AAssetManager* assetManager, const char* shaderPath) {
    LOGI("Loading shader from: %s", shaderPath);
    std::string shaderCode;
    AAsset* asset = AAssetManager_open(assetManager, shaderPath, AASSET_MODE_BUFFER);
    if (asset) {
        size_t length = AAsset_getLength(asset);
        LOGI("Shader file size: %zu", length);
        shaderCode.resize(length);
        AAsset_read(asset, &shaderCode[0], length);
        AAsset_close(asset);
        LOGI("Successfully loaded shader: %s", shaderPath);
    } else {
        LOGE("Failed to load shader from assets: %s", shaderPath);
        // 如果加载失败，使用硬编码shader作为备用
        if (strcmp(shaderPath, "shaders/vertex_shader.glsl") == 0) {
            shaderCode = "#version 300 es\nlayout(location=0)in vec3 aPos;layout(location=1)in vec2 aTexCoord;out vec2 TexCoord;void main(){gl_Position=vec4(aPos,1.0);TexCoord=aTexCoord;}";
        } else {
            shaderCode = "#version 300 es\nprecision mediump float;in vec2 TexCoord;out vec4 FragColor;uniform sampler2D texture0;void main(){FragColor=texture(texture0,TexCoord);}";
        }
        LOGI("Using fallback shader for: %s", shaderPath);
    }
    return shaderCode;
}

bool TextureStitcher::initialize(AAssetManager* assetManager) {
    LOGI("initialize called");
    if (mInitialized) {
        LOGI("Already initialized");
        return true;
    }

    mAssetManager = assetManager;
    LOGI("AssetManager set");

    std::string vertexShaderCode = loadShaderFromAssets(assetManager, "shaders/vertex_shader.glsl");
    std::string fragmentShaderCode = loadShaderFromAssets(assetManager, "shaders/fragment_shader.glsl");

    LOGI("Creating shader program");
    mProgram = createProgram(vertexShaderCode.c_str(), fragmentShaderCode.c_str());
    if (mProgram == 0) {
        LOGE("Failed to create shader program");
        return false;
    }
    LOGI("Shader program created: %d", mProgram);

    glGenVertexArrays(1, &mVAO);
    glGenBuffers(1, &mVBO);
    glGenBuffers(1, &mEBO);
    LOGI("OpenGL objects generated: VAO=%d, VBO=%d, EBO=%d", mVAO, mVBO, mEBO);

    checkGLError("initialize");

    mInitialized = true;
    LOGI("TextureStitcher initialized successfully");
    return true;
}

void TextureStitcher::setViewport(int width, int height) {
    LOGI("setViewport: %dx%d", width, height);
    mViewportWidth = width;
    mViewportHeight = height;
    glViewport(0, 0, width, height);
}

bool TextureStitcher::addImage(void* pixels, int width, int height) {
    LOGI("addImage called: %dx%d", width, height);

    if (!pixels) {
        LOGE("Null pixels provided");
        return false;
    }

    TextureInfo textureInfo;
    textureInfo.width = width;
    textureInfo.height = height;

    glGenTextures(1, &textureInfo.textureId);
    LOGI("Generated texture ID: %d", textureInfo.textureId);

    glBindTexture(GL_TEXTURE_2D, textureInfo.textureId);

    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);

    glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, width, height, 0,
                 GL_RGBA, GL_UNSIGNED_BYTE, pixels);

    checkGLError("addImage");

    mTextures.push_back(textureInfo);
    LOGI("Texture added successfully. Total textures: %zu", mTextures.size());
    return true;
}

void TextureStitcher::calculateLayout() {
    LOGI("calculateLayout called, texture count: %zu", mTextures.size());

    if (mTextures.empty()) {
        LOGE("No textures to layout");
        return;
    }

    int cols = 2;
    int rows = (mTextures.size() + cols - 1) / cols;
    LOGI("Grid layout: %dx%d", cols, rows);

    float colWidth = 2.0f / cols;
    float rowHeight = 2.0f / rows;

    mVertices.clear();
    mIndices.clear();

    for (int i = 0; i < mTextures.size(); ++i) {
        int row = i / cols;
        int col = i % cols;

        float margin = 0.0f;
        float x = -1.0f + col * colWidth + margin;
        float y = 1.0f - row * rowHeight - margin;
        float width = colWidth - 2 * margin;
        float height = rowHeight - 2 * margin;

        Vertex vertices[4] = {
                { {x, y - height, 0.0f}, {0.0f, 1.0f} },
                { {x + width, y - height, 0.0f}, {1.0f, 1.0f} },
                { {x + width, y, 0.0f}, {1.0f, 0.0f} },
                { {x, y, 0.0f}, {0.0f, 0.0f} }
        };

        for (int j = 0; j < 4; ++j) {
            mVertices.push_back(vertices[j]);
        }

        GLuint baseIndex = i * 4;
        mIndices.insert(mIndices.end(), {
                baseIndex, baseIndex + 1, baseIndex + 2,
                baseIndex, baseIndex + 2, baseIndex + 3
        });
    }

    LOGI("Layout calculated: %zu vertices, %zu indices", mVertices.size(), mIndices.size());
}

void TextureStitcher::createVertexData() {
    LOGI("createVertexData called");

    if (mVertices.empty() || mIndices.empty()) {
        LOGE("No vertex data to create");
        return;
    }

    glBindVertexArray(mVAO);

    glBindBuffer(GL_ARRAY_BUFFER, mVBO);
    glBufferData(GL_ARRAY_BUFFER,
                 mVertices.size() * sizeof(Vertex),
                 mVertices.data(), GL_STATIC_DRAW);

    glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, mEBO);
    glBufferData(GL_ELEMENT_ARRAY_BUFFER,
                 mIndices.size() * sizeof(GLuint),
                 mIndices.data(), GL_STATIC_DRAW);

    glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, sizeof(Vertex), (void*)0);
    glEnableVertexAttribArray(0);

    glVertexAttribPointer(1, 2, GL_FLOAT, GL_FALSE, sizeof(Vertex),
                          (void*)offsetof(Vertex, texCoord));
    glEnableVertexAttribArray(1);

    glBindVertexArray(0);
    checkGLError("createVertexData");

    LOGI("Vertex data created successfully");
}

void TextureStitcher::render() {
    glClearColor(0.2f, 0.3f, 0.3f, 1.0f);
    glClear(GL_COLOR_BUFFER_BIT);

    if (!mInitialized) {
        LOGE("Not initialized, cannot render");
        return;
    }

    if (mTextures.empty()) {
        LOGI("No textures to render");
        return;
    }

    LOGI("Rendering %zu textures", mTextures.size());

    calculateLayout();
    createVertexData();

    glUseProgram(mProgram);

    GLint textureLoc = glGetUniformLocation(mProgram, "texture0");
    if (textureLoc != -1) {
        glUniform1i(textureLoc, 0);
    }

    for (int i = 0; i < mTextures.size(); ++i) {
        LOGI("Rendering texture %d: ID=%d", i, mTextures[i].textureId);

        glActiveTexture(GL_TEXTURE0);
        glBindTexture(GL_TEXTURE_2D, mTextures[i].textureId);

        glBindVertexArray(mVAO);
        glDrawElements(GL_TRIANGLES, 6, GL_UNSIGNED_INT,
                       (void*)(i * 6 * sizeof(GLuint)));

        checkGLError("render texture");
    }

    glBindVertexArray(0);
    LOGI("Render completed");
}

void TextureStitcher::cleanup() {
    LOGI("cleanup called");
    if (mProgram) {
        glDeleteProgram(mProgram);
        mProgram = 0;
    }
    if (mVAO) {
        glDeleteVertexArrays(1, &mVAO);
        mVAO = 0;
    }
    if (mVBO) {
        glDeleteBuffers(1, &mVBO);
        mVBO = 0;
    }
    if (mEBO) {
        glDeleteBuffers(1, &mEBO);
        mEBO = 0;
    }

    for (auto& tex : mTextures) {
        if (tex.textureId) {
            glDeleteTextures(1, &tex.textureId);
        }
    }
    mTextures.clear();

    mInitialized = false;
}

GLuint TextureStitcher::compileShader(GLenum type, const char* source) {
    LOGI("Compiling shader type: %d", type);
    GLuint shader = glCreateShader(type);
    if (shader == 0) {
        LOGE("Failed to create shader object");
        return 0;
    }

    glShaderSource(shader, 1, &source, nullptr);
    glCompileShader(shader);

    GLint success;
    glGetShaderiv(shader, GL_COMPILE_STATUS, &success);
    if (!success) {
        char infoLog[512];
        glGetShaderInfoLog(shader, 512, nullptr, infoLog);
        LOGE("Shader compilation failed: %s", infoLog);
        glDeleteShader(shader);
        return 0;
    }

    LOGI("Shader compiled successfully");
    return shader;
}

GLuint TextureStitcher::createProgram(const char* vertexSource, const char* fragmentSource) {
    GLuint vertexShader = compileShader(GL_VERTEX_SHADER, vertexSource);
    if (vertexShader == 0) {
        return 0;
    }

    GLuint fragmentShader = compileShader(GL_FRAGMENT_SHADER, fragmentSource);
    if (fragmentShader == 0) {
        glDeleteShader(vertexShader);
        return 0;
    }

    GLuint program = glCreateProgram();
    if (program == 0) {
        glDeleteShader(vertexShader);
        glDeleteShader(fragmentShader);
        return 0;
    }

    glAttachShader(program, vertexShader);
    glAttachShader(program, fragmentShader);
    glLinkProgram(program);

    GLint success;
    glGetProgramiv(program, GL_LINK_STATUS, &success);
    if (!success) {
        char infoLog[512];
        glGetProgramInfoLog(program, 512, nullptr, infoLog);
        LOGE("Program linking failed: %s", infoLog);
        glDeleteProgram(program);
        program = 0;
    }

    glDeleteShader(vertexShader);
    glDeleteShader(fragmentShader);

    if (program != 0) {
        LOGI("Program created successfully: %d", program);
    }

    return program;
}

#ifdef __cplusplus
extern "C" {
#endif

JNIEXPORT void JNICALL
Java_com_example_imagestitch_MyGLRenderer_nativeSurfaceCreated(JNIEnv *env, jobject thiz,
                                                               jobject asset_manager) {
    LOGI("nativeSurfaceCreated called");
    if (gStitcher == nullptr) {
        gStitcher = new TextureStitcher();
        LOGI("Created new TextureStitcher instance");
    }

    AAssetManager* assetManager = AAssetManager_fromJava(env, asset_manager);
    if (assetManager) {
        LOGI("AAssetManager obtained successfully");
        if (!gStitcher->initialize(assetManager)) {
            LOGE("Failed to initialize TextureStitcher");
        } else {
            LOGI("TextureStitcher initialized successfully");
        }
    } else {
        LOGE("Failed to get AAssetManager from Java");
    }
}

JNIEXPORT void JNICALL
Java_com_example_imagestitch_MyGLRenderer_nativeSurfaceChanged(JNIEnv *env, jobject thiz,
                                                               jint width, jint height) {
    LOGI("nativeSurfaceChanged: %dx%d", width, height);
    if (gStitcher) {
        gStitcher->setViewport(width, height);
    }
}

JNIEXPORT void JNICALL
Java_com_example_imagestitch_MyGLRenderer_nativeDrawFrame(JNIEnv *env, jobject thiz) {
    if (gStitcher) {
        gStitcher->render();
    }
}

JNIEXPORT void JNICALL
Java_com_example_imagestitch_MyGLRenderer_nativeSetImages(JNIEnv *env, jobject thiz,
                                                          jobjectArray bitmaps, jint count) {
    LOGI("nativeSetImages called with %d images", count);

    if (!gStitcher) {
        LOGE("gStitcher is null");
        return;
    }

    if (count <= 0) {
        LOGE("Invalid image count: %d", count);
        return;
    }

    int successCount = 0;
    for (int i = 0; i < count; ++i) {
        jobject bitmap = env->GetObjectArrayElement(bitmaps, i);
        if (bitmap == nullptr) {
            LOGE("Bitmap %d is null", i);
            continue;
        }

        AndroidBitmapInfo info;
        if (AndroidBitmap_getInfo(env, bitmap, &info) != ANDROID_BITMAP_RESULT_SUCCESS) {
            LOGE("Failed to get bitmap info for bitmap %d", i);
            env->DeleteLocalRef(bitmap);
            continue;
        }

        LOGI("Bitmap %d: %dx%d, format: %d", i, info.width, info.height, info.format);

        if (info.format != ANDROID_BITMAP_FORMAT_RGBA_8888) {
            LOGE("Unsupported bitmap format: %d", info.format);
            env->DeleteLocalRef(bitmap);
            continue;
        }

        void* pixels;
        if (AndroidBitmap_lockPixels(env, bitmap, &pixels) != ANDROID_BITMAP_RESULT_SUCCESS) {
            LOGE("Failed to lock pixels for bitmap %d", i);
            env->DeleteLocalRef(bitmap);
            continue;
        }

        if (gStitcher->addImage(pixels, info.width, info.height)) {
            successCount++;
            LOGI("Successfully added bitmap %d", i);
        } else {
            LOGE("Failed to add bitmap %d", i);
        }

        AndroidBitmap_unlockPixels(env, bitmap);
        env->DeleteLocalRef(bitmap);
    }

    LOGI("Image processing completed: %d/%d successful", successCount, count);
}

#ifdef __cplusplus
}
#endif