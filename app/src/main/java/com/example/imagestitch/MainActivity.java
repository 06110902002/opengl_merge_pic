package com.example.imagestitch;

import android.app.Activity;
import android.content.res.AssetManager;
import android.graphics.Bitmap;
import android.graphics.BitmapFactory;
import android.opengl.GLSurfaceView;
import android.os.Bundle;
import android.widget.Toast;

public class MainActivity extends Activity {

    private GLSurfaceView glSurfaceView;
    private MyGLRenderer renderer;
    private Bitmap[] loadedBitmaps; // 保存加载的图片

    static {
        System.loadLibrary("texture-stitch");
    }

    @Override
    protected void onCreate(Bundle savedInstanceState) {
        super.onCreate(savedInstanceState);

        glSurfaceView = new GLSurfaceView(this);
        glSurfaceView.setEGLContextClientVersion(3);

        // 创建渲染器并传递Activity引用
        renderer = new MyGLRenderer(this);
        glSurfaceView.setRenderer(renderer);

        setContentView(glSurfaceView);

        // 加载图片
        loadImages();
    }

    private void loadImages() {
        try {
            // 从drawable加载图片
            int[] imageResources = {
                    R.drawable.image1,
                    R.drawable.image2,
                    R.drawable.image3,
                    R.drawable.image4
            };

            loadedBitmaps = new Bitmap[imageResources.length];

            for (int i = 0; i < imageResources.length; i++) {
                Bitmap originalBitmap = BitmapFactory.decodeResource(getResources(), imageResources[i]);
                if (originalBitmap != null) {
                    // 确保使用ARGB_8888格式
                    if (originalBitmap.getConfig() != Bitmap.Config.ARGB_8888) {
                        loadedBitmaps[i] = originalBitmap.copy(Bitmap.Config.ARGB_8888, false);
                        originalBitmap.recycle();
                    } else {
                        loadedBitmaps[i] = originalBitmap;
                    }
                }
            }

            // 设置图片到渲染器
            renderer.setImages(loadedBitmaps);
            Toast.makeText(this, "加载了4张图片", Toast.LENGTH_SHORT).show();

        } catch (Exception e) {
            e.printStackTrace();
            Toast.makeText(this, "图片加载失败", Toast.LENGTH_SHORT).show();
        }
    }

    // 提供获取AssetManager的方法
    public AssetManager getAppAssetManager() {
        return getAssets();
    }

    // 提供重新设置图片的方法
    public void reloadImages() {
        if (loadedBitmaps != null) {
            renderer.setImages(loadedBitmaps);
        }
    }

    @Override
    protected void onResume() {
        super.onResume();
        if (glSurfaceView != null) {
            glSurfaceView.onResume();
            // 回到前台时重新设置图片
            reloadImages();
        }
    }

    @Override
    protected void onPause() {
        super.onPause();
        if (glSurfaceView != null) {
            glSurfaceView.onPause();
        }
    }

    @Override
    protected void onDestroy() {
        super.onDestroy();
        // 释放Bitmap资源
        if (loadedBitmaps != null) {
            for (Bitmap bitmap : loadedBitmaps) {
                if (bitmap != null && !bitmap.isRecycled()) {
                    bitmap.recycle();
                }
            }
        }
    }
}

class MyGLRenderer implements GLSurfaceView.Renderer {
    private Bitmap[] pendingBitmaps;
    private MainActivity activity;
    private boolean needResetImages = false; // 标记是否需要重新设置图片

    public MyGLRenderer(MainActivity activity) {
        this.activity = activity;
    }

    public native void nativeSurfaceCreated(AssetManager assetManager);
    public native void nativeSurfaceChanged(int width, int height);
    public native void nativeDrawFrame();
    public native void nativeSetImages(Bitmap[] bitmaps, int count);
    public native void nativeCleanup(); // 新增清理方法

    @Override
    public void onSurfaceCreated(javax.microedition.khronos.opengles.GL10 gl,
                                 javax.microedition.khronos.egl.EGLConfig config) {
        // 通过Activity引用安全地获取AssetManager
        if (activity != null) {
            nativeSurfaceCreated(activity.getAppAssetManager());
        }

        // 如果有待处理的图片，设置它们
        if (pendingBitmaps != null) {
            nativeSetImages(pendingBitmaps, pendingBitmaps.length);
            pendingBitmaps = null;
        } else if (needResetImages) {
            // 如果需要重新设置图片，通知activity重新加载
            activity.reloadImages();
            needResetImages = false;
        }
    }

    @Override
    public void onSurfaceChanged(javax.microedition.khronos.opengles.GL10 gl,
                                 int width, int height) {
        nativeSurfaceChanged(width, height);
    }

    @Override
    public void onDrawFrame(javax.microedition.khronos.opengles.GL10 gl) {
        nativeDrawFrame();
    }

    public void setImages(Bitmap[] bitmaps) {
        this.pendingBitmaps = bitmaps;
        this.needResetImages = false; // 重置标记
    }

    // 标记需要重新设置图片
    public void markNeedResetImages() {
        this.needResetImages = true;
    }
}