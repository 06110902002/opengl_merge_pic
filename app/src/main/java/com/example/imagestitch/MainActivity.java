package com.example.imagestitch;

import android.app.Activity;
import android.content.res.AssetManager;
import android.graphics.Bitmap;
import android.graphics.BitmapFactory;
import android.opengl.GLSurfaceView;
import android.os.Bundle;
import android.widget.Toast;
import java.io.InputStream;

public class MainActivity extends Activity {

    private GLSurfaceView glSurfaceView;
    private MyGLRenderer renderer;

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

            Bitmap[] bitmaps = new Bitmap[imageResources.length];

            for (int i = 0; i < imageResources.length; i++) {
                Bitmap originalBitmap = BitmapFactory.decodeResource(getResources(), imageResources[i]);
                if (originalBitmap != null) {
                    // 确保使用ARGB_8888格式
                    if (originalBitmap.getConfig() != Bitmap.Config.ARGB_8888) {
                        bitmaps[i] = originalBitmap.copy(Bitmap.Config.ARGB_8888, false);
                        originalBitmap.recycle();
                    } else {
                        bitmaps[i] = originalBitmap;
                    }
                }
            }

            renderer.setImages(bitmaps);
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

    @Override
    protected void onResume() {
        super.onResume();
        if (glSurfaceView != null) {
            glSurfaceView.onResume();
        }
    }

    @Override
    protected void onPause() {
        super.onPause();
        if (glSurfaceView != null) {
            glSurfaceView.onPause();
        }
    }
}

class MyGLRenderer implements GLSurfaceView.Renderer {
    private Bitmap[] pendingBitmaps;
    private MainActivity activity;

    public MyGLRenderer(MainActivity activity) {
        this.activity = activity;
    }

    public native void nativeSurfaceCreated(AssetManager assetManager);
    public native void nativeSurfaceChanged(int width, int height);
    public native void nativeDrawFrame();
    public native void nativeSetImages(Bitmap[] bitmaps, int count);

    @Override
    public void onSurfaceCreated(javax.microedition.khronos.opengles.GL10 gl,
                                 javax.microedition.khronos.egl.EGLConfig config) {
        // 通过Activity引用安全地获取AssetManager
        if (activity != null) {
            nativeSurfaceCreated(activity.getAppAssetManager());
        }

        if (pendingBitmaps != null) {
            nativeSetImages(pendingBitmaps, pendingBitmaps.length);
            pendingBitmaps = null;
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
    }
}