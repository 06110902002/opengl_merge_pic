package com.example.imagestitch;

import android.app.Activity;
import android.content.res.AssetManager;
import android.graphics.Bitmap;
import android.graphics.BitmapFactory;
import android.opengl.GLSurfaceView;
import android.os.Bundle;
import android.view.MotionEvent;
import android.view.ScaleGestureDetector;
import android.widget.Toast;

public class MainActivity extends Activity {

    private GLSurfaceView glSurfaceView;
    private MyGLRenderer renderer;
    private Bitmap[] loadedBitmaps;

    // 手势检测器
    private ScaleGestureDetector scaleGestureDetector;
    private float lastTouchX, lastTouchY;

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

        // 初始化手势检测器
        initGestureDetector();

        // 加载图片
        loadImages();
    }

    private void initGestureDetector() {
        // 创建缩放手势检测器
        scaleGestureDetector = new ScaleGestureDetector(this, new ScaleGestureDetector.SimpleOnScaleGestureListener() {
            @Override
            public boolean onScale(ScaleGestureDetector detector) {
                // 获取缩放因子
                float scaleFactor = detector.getScaleFactor();
                // 传递给native层
                renderer.handleScale(scaleFactor, detector.getFocusX(), detector.getFocusY());
                return true;
            }
        });
    }

    @Override
    public boolean onTouchEvent(MotionEvent event) {
        // 将触摸事件传递给缩放手势检测器
        scaleGestureDetector.onTouchEvent(event);

        final int action = event.getAction();

        switch (action & MotionEvent.ACTION_MASK) {
            case MotionEvent.ACTION_DOWN:
                // 记录触摸起始位置
                lastTouchX = event.getX();
                lastTouchY = event.getY();
                break;

            case MotionEvent.ACTION_MOVE:
                if (!scaleGestureDetector.isInProgress()) {
                    // 计算移动距离
                    final float x = event.getX();
                    final float y = event.getY();
                    final float dx = x - lastTouchX;
                    final float dy = y - lastTouchY;

                    // 传递给native层处理拖动
                    renderer.handleDrag(dx, dy);

                    // 更新最后触摸位置
                    lastTouchX = x;
                    lastTouchY = y;
                }
                break;

            case MotionEvent.ACTION_UP:
            case MotionEvent.ACTION_CANCEL:
                // 触摸结束，可以在这里添加惯性滚动等效果
                break;
        }

        return true;
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
            Toast.makeText(this, "加载了4张图片，支持双指缩放和拖动", Toast.LENGTH_SHORT).show();

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
    private boolean needResetImages = false;

    public MyGLRenderer(MainActivity activity) {
        this.activity = activity;
    }

    // 原有的Native方法
    public native void nativeSurfaceCreated(AssetManager assetManager);
    public native void nativeSurfaceChanged(int width, int height);
    public native void nativeDrawFrame();
    public native void nativeSetImages(Bitmap[] bitmaps, int count);
    public native void nativeCleanup();

    // 新增的手势控制Native方法
    public native void nativeHandleScale(float scaleFactor, float focusX, float focusY);
    public native void nativeHandleDrag(float dx, float dy);
    public native void nativeResetTransform(); // 重置变换

    @Override
    public void onSurfaceCreated(javax.microedition.khronos.opengles.GL10 gl,
                                 javax.microedition.khronos.egl.EGLConfig config) {
        if (activity != null) {
            nativeSurfaceCreated(activity.getAppAssetManager());
        }

        if (pendingBitmaps != null) {
            nativeSetImages(pendingBitmaps, pendingBitmaps.length);
            pendingBitmaps = null;
        } else if (needResetImages) {
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
        this.needResetImages = false;
    }

    public void markNeedResetImages() {
        this.needResetImages = true;
    }

    // 处理缩放手势
    public void handleScale(float scaleFactor, float focusX, float focusY) {
        nativeHandleScale(scaleFactor, focusX, focusY);
    }

    // 处理拖动手势
    public void handleDrag(float dx, float dy) {
        nativeHandleDrag(dx, dy);
    }

    // 重置变换（可选功能）
    public void resetTransform() {
        nativeResetTransform();
    }
}