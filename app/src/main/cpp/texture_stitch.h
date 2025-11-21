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

class TextureStitcher {
public:
    TextureStitcher();
    ~TextureStitcher();

    bool initialize(AAssetManager* assetManager);
    void setViewport(int width, int height);
    bool addImage(void* pixels, int width, int height);
    void render();
    void cleanup();
    void clearTextures(); // 新增：清空纹理方法

private:
    std::string loadShaderFromAssets(AAssetManager* assetManager, const char* shaderPath);
    GLuint compileShader(GLenum type, const char* source);
    GLuint createProgram(const char* vertexSource, const char* fragmentSource);
    void calculateLayout();
    void createVertexData();
    void checkGLError(const char* operation);

    GLuint mProgram;
    GLuint mVAO;
    GLuint mVBO;
    GLuint mEBO;

    int mViewportWidth;
    int mViewportHeight;

    std::vector<TextureInfo> mTextures;
    std::vector<Vertex> mVertices;
    std::vector<GLuint> mIndices;

    bool mInitialized;
    AAssetManager* mAssetManager;
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

#ifdef __cplusplus
}
#endif

#endif