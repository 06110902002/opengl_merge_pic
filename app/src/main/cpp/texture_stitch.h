#ifndef TEXTURE_STITCH_H
#define TEXTURE_STITCH_H

#include <GLES3/gl3.h>
#include <android/bitmap.h>
#include <android/asset_manager.h>
#include <android/asset_manager_jni.h>
#include <android/log.h>
#include <vector>
#include <string>

#define LOG_TAG "TextureStitch"
#define LOGI(...) __android_log_print(ANDROID_LOG_INFO, LOG_TAG, __VA_ARGS__)
#define LOGE(...) __android_log_print(ANDROID_LOG_ERROR, LOG_TAG, __VA_ARGS__)

struct TextureInfo {
    GLuint textureId;
    int width;
    int height;
};

struct Vertex {
    float position[3];
    float texCoord[2];
};

// 变换控制结构体
struct Transform {
    float scale;        // 缩放因子
    float translateX;   // X轴平移
    float translateY;   // Y轴平移
    float minScale;     // 最小缩放限制
    float maxScale;     // 最大缩放限制
};

class TextureStitcher {
public:
    TextureStitcher();
    ~TextureStitcher();

    bool initialize(AAssetManager* assetManager);
    void setViewport(int width, int height);
    bool addImage(void* pixels, int width, int height);
    void render();
    void cleanup();
    void clearTextures();

    // 新增手势控制方法
    void handleScale(float scaleFactor, float focusX, float focusY);
    void handleDrag(float dx, float dy);
    void resetTransform();

private:
    std::string loadShaderFromAssets(AAssetManager* assetManager, const char* shaderPath);
    GLuint compileShader(GLenum type, const char* source);
    GLuint createProgram(const char* vertexSource, const char* fragmentSource);
    void calculateLayout();
    void createVertexData();
    void updateVerticesWithTransform(); // 更新顶点数据应用变换
    void checkGLError(const char* operation);
    void applyTransformToVertex(Vertex& vertex); // 对单个顶点应用变换

    GLuint mProgram;
    GLuint mVAO;
    GLuint mVBO;
    GLuint mEBO;

    int mViewportWidth;
    int mViewportHeight;

    std::vector<TextureInfo> mTextures;
    std::vector<Vertex> mVertices;      // 原始顶点数据
    std::vector<Vertex> mTransformedVertices; // 变换后的顶点数据
    std::vector<GLuint> mIndices;

    bool mInitialized;
    AAssetManager* mAssetManager;

    // 变换控制
    Transform mTransform;
};

#ifdef __cplusplus
extern "C" {
#endif

JNIEXPORT void JNICALL
Java_com_example_imagestitch_MyGLRenderer_nativeSurfaceCreated(JNIEnv *env, jobject thiz,
                                                               jobject asset_manager);

JNIEXPORT void JNICALL
Java_com_example_imagestitch_MyGLRenderer_nativeSurfaceChanged(JNIEnv *env, jobject thiz,
                                                               jint width, jint height);

JNIEXPORT void JNICALL
Java_com_example_imagestitch_MyGLRenderer_nativeDrawFrame(JNIEnv *env, jobject thiz);

JNIEXPORT void JNICALL
Java_com_example_imagestitch_MyGLRenderer_nativeSetImages(JNIEnv *env, jobject thiz,
                                                          jobjectArray bitmaps, jint count);

JNIEXPORT void JNICALL
Java_com_example_imagestitch_MyGLRenderer_nativeCleanup(JNIEnv *env, jobject thiz);

// 新增手势控制JNI方法
JNIEXPORT void JNICALL
Java_com_example_imagestitch_MyGLRenderer_nativeHandleScale(JNIEnv *env, jobject thiz,
                                                            jfloat scaleFactor, jfloat focusX, jfloat focusY);

JNIEXPORT void JNICALL
Java_com_example_imagestitch_MyGLRenderer_nativeHandleDrag(JNIEnv *env, jobject thiz,
                                                           jfloat dx, jfloat dy);

JNIEXPORT void JNICALL
Java_com_example_imagestitch_MyGLRenderer_nativeResetTransform(JNIEnv *env, jobject thiz);

#ifdef __cplusplus
}
#endif

#endif