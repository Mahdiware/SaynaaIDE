package com.saynaa.view;

import android.content.Context;
import android.graphics.Canvas;
import android.graphics.Color;
import android.graphics.Paint;
import android.graphics.Rect;
import android.graphics.drawable.ColorDrawable;
import android.graphics.drawable.Drawable;
import android.os.Parcel;
import android.os.Parcelable;
import android.util.AttributeSet;
import android.util.DisplayMetrics;
import android.view.Gravity;
import android.view.MotionEvent;
import android.view.VelocityTracker;
import android.view.View;
import android.view.ViewConfiguration;
import android.view.ViewGroup;
import android.widget.Scroller;

import java.util.ArrayList;

/**
 * Lightweight AndroidX DrawerLayout replacement.
 *
 * Framework only:
 * - No AndroidX
 * - No AppCompat
 * - No Material
 *
 * Supported:
 * - One content view
 * - One START/LEFT drawer
 * - One END/RIGHT drawer
 * - Drawer child click handling
 * - Drawer child vertical scrolling
 * - Horizontal drawer dragging
 * - Edge swipe opening
 * - Swipe closing
 * - Velocity based settling
 * - Scrim tap closing
 * - Scrim drawing
 * - Drawer shadow
 * - Drawer elevation
 * - START / END / LEFT / RIGHT
 * - RTL
 * - Lock modes
 * - Drawer listeners
 * - Saved open state
 */
public class DrawerLayout extends ViewGroup {

  public static final int LOCK_MODE_UNLOCKED = 0;
  public static final int LOCK_MODE_LOCKED_CLOSED = 1;
  public static final int LOCK_MODE_LOCKED_OPEN = 2;
  public static final int LOCK_MODE_UNDEFINED = 3;

  public static final int STATE_IDLE = 0;
  public static final int STATE_DRAGGING = 1;
  public static final int STATE_SETTLING = 2;

  private static final int EDGE_NONE = 0;
  private static final int EDGE_LEFT = 1;
  private static final int EDGE_RIGHT = 2;

  private static final int INVALID_POINTER = -1;

  private static final int DEFAULT_MIN_DRAWER_MARGIN_DP = 64;
  private static final int DEFAULT_EDGE_SIZE_DP = 24;
  private static final int DEFAULT_DRAWER_WIDTH_DP = 320;

  private static final int DEFAULT_SCRIM_COLOR = 0x99000000;

  private static final int ANIMATION_BASE_DURATION = 180;
  private static final int ANIMATION_EXTRA_DURATION = 180;

  private final Paint mScrimPaint =
      new Paint(Paint.ANTI_ALIAS_FLAG);

  private final Rect mTempRect =
      new Rect();

  private final Rect mDrawerRect =
      new Rect();

  private final Scroller mLeftScroller;
  private final Scroller mRightScroller;

  private VelocityTracker mVelocityTracker;

  private View mLeftDrawer;
  private View mRightDrawer;

  private int mDrawerState = STATE_IDLE;

  private int mLeftLockMode =
      LOCK_MODE_UNDEFINED;

  private int mRightLockMode =
      LOCK_MODE_UNDEFINED;

  private int mMinDrawerMargin;
  private int mEdgeSize;

  private int mScrimColor =
      DEFAULT_SCRIM_COLOR;

  private float mScrimOpacity;

  private float mDrawerElevation;

  private Drawable mLeftShadow;
  private Drawable mRightShadow;

  private int mTouchSlop;
  private int mMinFlingVelocity;
  private int mMaxFlingVelocity;

  private int mActivePointerId =
      INVALID_POINTER;

  private float mInitialX;
  private float mInitialY;

  private float mLastX;
  private float mLastY;

  private boolean mDragging;

  /**
   * Drawer currently being manipulated.
   */
  private int mDragGravity =
      Gravity.NO_GRAVITY;

  /**
   * Initial area:
   *
   * - EDGE_LEFT / EDGE_RIGHT:
   *   gesture started at a screen edge,
   *   drawer was closed.
   *
   * - drawer gravity:
   *   gesture started inside an open drawer.
   *
   * - NO_GRAVITY:
   *   normal child/content touch.
   */
  private int mTouchTargetGravity =
      Gravity.NO_GRAVITY;

  /**
   * true when ACTION_DOWN happened
   * on the scrim/content while a drawer
   * is visible.
   */
  private boolean mTapOnScrim;

  private boolean mDisallowIntercept;

  private boolean mFirstLayout = true;

  private int mSavedOpenGravity =
      Gravity.NO_GRAVITY;

  private final ArrayList<DrawerListener>
      mListeners =
      new ArrayList<DrawerListener>();

  // ============================================================
  // LISTENER
  // ============================================================

  public interface DrawerListener {

    void onDrawerSlide(
        View drawerView,
        float slideOffset);

    void onDrawerOpened(
        View drawerView);

    void onDrawerClosed(
        View drawerView);

    void onDrawerStateChanged(
        int newState);
  }

  public static class SimpleDrawerListener
      implements DrawerListener {

    @Override
    public void onDrawerSlide(
        View drawerView,
        float slideOffset) {
    }

    @Override
    public void onDrawerOpened(
        View drawerView) {
    }

    @Override
    public void onDrawerClosed(
        View drawerView) {
    }

    @Override
    public void onDrawerStateChanged(
        int newState) {
    }
  }

  // ============================================================
  // LAYOUT PARAMS
  // ============================================================

  public static class LayoutParams
      extends MarginLayoutParams {

    public int gravity =
        Gravity.NO_GRAVITY;

    float onScreen;

    boolean knownOpen;

    public LayoutParams(
        Context context,
        AttributeSet attrs) {

      super(context, attrs);
    }

    public LayoutParams(
        int width,
        int height) {

      super(width, height);
    }

    public LayoutParams(
        int width,
        int height,
        int gravity) {

      super(width, height);

      this.gravity = gravity;
    }

    public LayoutParams(
        LayoutParams source) {

      super(source);

      gravity = source.gravity;
      onScreen = source.onScreen;
      knownOpen = source.knownOpen;
    }

    public LayoutParams(
        ViewGroup.LayoutParams source) {

      super(source);
    }

    public LayoutParams(
        MarginLayoutParams source) {

      super(source);
    }
  }

  // ============================================================
  // CONSTRUCTORS
  // ============================================================

  public DrawerLayout(
      Context context) {

    this(context, null);
  }

  public DrawerLayout(
      Context context,
      AttributeSet attrs) {

    this(context, attrs, 0);
  }

  public DrawerLayout(
      Context context,
      AttributeSet attrs,
      int defStyleAttr) {

    super(
        context,
        attrs,
        defStyleAttr);

    ViewConfiguration vc =
        ViewConfiguration.get(context);

    mTouchSlop =
        vc.getScaledTouchSlop();

    mMinFlingVelocity =
        vc.getScaledMinimumFlingVelocity();

    mMaxFlingVelocity =
        vc.getScaledMaximumFlingVelocity();

    mMinDrawerMargin =
        dp(DEFAULT_MIN_DRAWER_MARGIN_DP);

    mEdgeSize =
        dp(DEFAULT_EDGE_SIZE_DP);

    mDrawerElevation =
        dp(16);

    mLeftScroller =
        new Scroller(context);

    mRightScroller =
        new Scroller(context);

    mScrimPaint.setColor(
        mScrimColor);

    setWillNotDraw(false);

    setFocusable(true);

    setFocusableInTouchMode(true);

    setDescendantFocusability(
        FOCUS_AFTER_DESCENDANTS);
  }

  // ============================================================
  // PUBLIC DRAWER API
  // ============================================================

  public void open() {
    openDrawer(Gravity.START);
  }

  public void close() {
    closeDrawer(Gravity.START);
  }

  public void openDrawer(
      int gravity) {

    openDrawer(
        gravity,
        true);
  }

  public void openDrawer(
      int gravity,
      boolean animate) {

    gravity =
        resolveGravity(gravity);

    View drawer =
        findDrawerWithGravity(gravity);

    if (drawer == null) {
      return;
    }

    cancelScroller(
        gravity == Gravity.LEFT
            ? Gravity.RIGHT
            : Gravity.LEFT);

    closeOtherDrawer(
        gravity);

    if (animate) {

      smoothSlideDrawer(
          gravity,
          1f);

    } else {

      moveDrawerToOffset(
          drawer,
          1f);

      setDrawerState(
          STATE_IDLE);
    }
  }

  public void openDrawer(
      View drawer) {

    openDrawer(
        drawer,
        true);
  }

  public void openDrawer(
      View drawer,
      boolean animate) {

    if (drawer == null) {
      return;
    }

    LayoutParams lp =
        getDrawerLayoutParams(
            drawer);

    openDrawer(
        lp.gravity,
        animate);
  }

  public void closeDrawer(
      int gravity) {

    closeDrawer(
        gravity,
        true);
  }

  public void closeDrawer(
      int gravity,
      boolean animate) {

    gravity =
        resolveGravity(gravity);

    View drawer =
        findDrawerWithGravity(gravity);

    if (drawer == null) {
      return;
    }

    if (animate) {

      smoothSlideDrawer(
          gravity,
          0f);

    } else {

      moveDrawerToOffset(
          drawer,
          0f);

      setDrawerState(
          STATE_IDLE);
    }
  }

  public void closeDrawer(
      View drawer) {

    closeDrawer(
        drawer,
        true);
  }

  public void closeDrawer(
      View drawer,
      boolean animate) {

    if (drawer == null) {
      return;
    }

    LayoutParams lp =
        getDrawerLayoutParams(
            drawer);

    closeDrawer(
        lp.gravity,
        animate);
  }

  public void closeDrawers() {

    if (mLeftDrawer != null) {
      closeDrawer(
          Gravity.LEFT);
    }

    if (mRightDrawer != null) {
      closeDrawer(
          Gravity.RIGHT);
    }
  }

  public boolean isDrawerOpen(
      int gravity) {

    View drawer =
        findDrawerWithGravity(gravity);

    return drawer != null
        && isDrawerOpen(drawer);
  }

  public boolean isDrawerOpen(
      View drawer) {

    if (drawer == null) {
      return false;
    }

    return getDrawerLayoutParams(
        drawer)
        .onScreen >= 0.999f;
  }

  public boolean isDrawerVisible(
      int gravity) {

    View drawer =
        findDrawerWithGravity(gravity);

    return drawer != null
        && isDrawerVisible(drawer);
  }

  public boolean isDrawerVisible(
      View drawer) {

    if (drawer == null) {
      return false;
    }

    return getDrawerLayoutParams(
        drawer)
        .onScreen > 0f;
  }

  public float getDrawerSlideOffset(
      int gravity) {

    View drawer =
        findDrawerWithGravity(gravity);

    if (drawer == null) {
      return 0f;
    }

    return getDrawerLayoutParams(
        drawer)
        .onScreen;
  }

  public View getDrawerView(
      int gravity) {

    return findDrawerWithGravity(
        gravity);
  }

  public View getOpenDrawer() {

    if (mLeftDrawer != null
        && isDrawerOpen(mLeftDrawer)) {

      return mLeftDrawer;
    }

    if (mRightDrawer != null
        && isDrawerOpen(mRightDrawer)) {

      return mRightDrawer;
    }

    return null;
  }

  // ============================================================
  // LOCKING
  // ============================================================

  public void setDrawerLockMode(
      int lockMode) {

    setDrawerLockMode(
        lockMode,
        Gravity.LEFT);

    setDrawerLockMode(
        lockMode,
        Gravity.RIGHT);
  }

  public void setDrawerLockMode(
      int lockMode,
      int gravity) {

    gravity =
        resolveGravity(gravity);

    if (gravity == Gravity.LEFT) {

      mLeftLockMode =
          lockMode;

    } else if (gravity
        == Gravity.RIGHT) {

      mRightLockMode =
          lockMode;
    }

    /*
     * Programmatic calls are still allowed.
     * Locking only restricts user interaction,
     * just like DrawerLayout.
     */
  }

  public void setDrawerLockMode(
      int lockMode,
      View drawer) {

    if (drawer == null) {
      return;
    }

    LayoutParams lp =
        getDrawerLayoutParams(drawer);

    setDrawerLockMode(
        lockMode,
        lp.gravity);
  }

  public int getDrawerLockMode(
      int gravity) {

    gravity =
        resolveGravity(gravity);

    if (gravity == Gravity.LEFT) {
      return mLeftLockMode;
    }

    if (gravity == Gravity.RIGHT) {
      return mRightLockMode;
    }

    return LOCK_MODE_UNDEFINED;
  }

  public int getDrawerLockMode(
      View drawer) {

    if (drawer == null) {
      return LOCK_MODE_UNDEFINED;
    }

    LayoutParams lp =
        getDrawerLayoutParams(drawer);

    return getDrawerLockMode(
        lp.gravity);
  }

  // ============================================================
  // LISTENERS
  // ============================================================

  public void addDrawerListener(
      DrawerListener listener) {

    if (listener == null) {
      return;
    }

    if (!mListeners.contains(listener)) {
      mListeners.add(listener);
    }
  }

  public void removeDrawerListener(
      DrawerListener listener) {

    mListeners.remove(listener);
  }

  public void setDrawerListener(
      DrawerListener listener) {

    mListeners.clear();

    if (listener != null) {
      mListeners.add(listener);
    }
  }

  // ============================================================
  // SCRIM
  // ============================================================

  public void setScrimColor(
      int color) {

    mScrimColor = color;

    mScrimPaint.setColor(
        color);

    invalidate();
  }

  public int getScrimColor() {
    return mScrimColor;
  }

  public float getScrimOpacity() {
    return mScrimOpacity;
  }

  // ============================================================
  // EDGE
  // ============================================================

  public void setEdgeSize(
      int sizePx) {

    mEdgeSize =
        Math.max(0, sizePx);
  }

  public int getEdgeSize() {
    return mEdgeSize;
  }

  // ============================================================
  // ELEVATION
  // ============================================================

  public void setDrawerElevation(
      float elevation) {

    mDrawerElevation =
        Math.max(0f, elevation);

    applyDrawerElevation(
        mLeftDrawer);

    applyDrawerElevation(
        mRightDrawer);
  }

  public float getDrawerElevation() {
    return mDrawerElevation;
  }

  private void applyDrawerElevation(
      View drawer) {

    if (drawer == null) {
      return;
    }

    if (android.os.Build.VERSION.SDK_INT >= 21) {
      drawer.setElevation(
          mDrawerElevation);
    }
  }

  // ============================================================
  // SHADOW
  // ============================================================

  public void setDrawerShadow(
      Drawable shadow,
      int gravity) {

    gravity =
        resolveGravity(gravity);

    if (gravity == Gravity.LEFT) {

      mLeftShadow =
          shadow;

    } else if (gravity
        == Gravity.RIGHT) {

      mRightShadow =
          shadow;
    }

    invalidate();
  }

  // ============================================================
  // TITLES
  // ============================================================

  private CharSequence mLeftTitle;
  private CharSequence mRightTitle;

  public void setDrawerTitle(
      int gravity,
      CharSequence title) {

    gravity =
        resolveGravity(gravity);

    if (gravity == Gravity.LEFT) {
      mLeftTitle = title;
    } else if (gravity
        == Gravity.RIGHT) {
      mRightTitle = title;
    }
  }

  public CharSequence getDrawerTitle(
      int gravity) {

    gravity =
        resolveGravity(gravity);

    if (gravity == Gravity.LEFT) {
      return mLeftTitle;
    }

    if (gravity == Gravity.RIGHT) {
      return mRightTitle;
    }

    return null;
  }

  // ============================================================
  // CHILD MANAGEMENT
  // ============================================================

  @Override
  public void addView(
      View child,
      int index,
      ViewGroup.LayoutParams params) {

    LayoutParams lp =
        generateLayoutParams(params);

    boolean drawer =
        isDrawerLayoutParams(lp);

    if (drawer) {

      int gravity =
          resolveGravity(lp.gravity);

      if (gravity != Gravity.LEFT
          && gravity != Gravity.RIGHT) {

        throw new IllegalStateException(
            "Drawer must use LEFT, RIGHT, START or END gravity");
      }

      if (gravity == Gravity.LEFT) {

        if (mLeftDrawer != null) {

          throw new IllegalStateException(
              "Only one left drawer is allowed");
        }

        mLeftDrawer =
            child;

      } else {

        if (mRightDrawer != null) {

          throw new IllegalStateException(
              "Only one right drawer is allowed");
        }

        mRightDrawer =
            child;
      }

      child.setVisibility(
          INVISIBLE);

      applyDrawerElevation(child);

    } else {

      if (getContentView() != null) {

        throw new IllegalStateException(
            "Only one content view is allowed");
      }
    }

    super.addView(
        child,
        index,
        lp);
  }

  @Override
  public void removeView(
      View view) {

    if (view == mLeftDrawer) {
      mLeftDrawer = null;
    }

    if (view == mRightDrawer) {
      mRightDrawer = null;
    }

    super.removeView(view);
  }

  public View getContentView() {

    for (int i = 0;
         i < getChildCount();
         i++) {

      View child =
          getChildAt(i);

      if (!isDrawerView(child)) {
        return child;
      }
    }

    return null;
  }

  // ============================================================
  // LAYOUT PARAMS
  // ============================================================

  @Override
  protected LayoutParams
  generateDefaultLayoutParams() {

    return new LayoutParams(
        ViewGroup.LayoutParams.MATCH_PARENT,
        ViewGroup.LayoutParams.MATCH_PARENT);
  }

  @Override
  public LayoutParams
  generateLayoutParams(
      AttributeSet attrs) {

    return new LayoutParams(
        getContext(),
        attrs);
  }

  @Override
  protected LayoutParams
  generateLayoutParams(
      ViewGroup.LayoutParams params) {

    if (params instanceof LayoutParams) {

      return new LayoutParams(
          (LayoutParams) params);
    }

    if (params instanceof MarginLayoutParams) {

      return new LayoutParams(
          (MarginLayoutParams) params);
    }

    return new LayoutParams(params);
  }

  @Override
  protected boolean checkLayoutParams(
      ViewGroup.LayoutParams params) {

    return params instanceof LayoutParams;
  }

  // ============================================================
  // MEASURE
  // ============================================================

  @Override
  protected void onMeasure(
      int widthMeasureSpec,
      int heightMeasureSpec) {

    int widthMode =
        MeasureSpec.getMode(
            widthMeasureSpec);

    int widthSize =
        MeasureSpec.getSize(
            widthMeasureSpec);

    int heightMode =
        MeasureSpec.getMode(
            heightMeasureSpec);

    int heightSize =
        MeasureSpec.getSize(
            heightMeasureSpec);

    if (widthMode
        == MeasureSpec.UNSPECIFIED
        || heightMode
        == MeasureSpec.UNSPECIFIED) {

      throw new IllegalStateException(
          "DrawerLayout requires exact width and height");
    }

    setMeasuredDimension(
        widthSize,
        heightSize);

    for (int i = 0;
         i < getChildCount();
         i++) {

      View child =
          getChildAt(i);

      LayoutParams lp =
          getDrawerLayoutParams(child);

      if (!isDrawerView(child)) {

        int childWidthSpec =
            getChildMeasureSpec(
                widthMeasureSpec,
                lp.leftMargin
                    + lp.rightMargin,
                lp.width);

        int childHeightSpec =
            getChildMeasureSpec(
                heightMeasureSpec,
                lp.topMargin
                    + lp.bottomMargin,
                lp.height);

        child.measure(
            childWidthSpec,
            childHeightSpec);

        continue;
      }

      int maxDrawerWidth =
          widthSize
              - mMinDrawerMargin;

      int requestedWidth;

      if (lp.width
          == ViewGroup.LayoutParams.MATCH_PARENT) {

        requestedWidth =
            maxDrawerWidth;

      } else if (lp.width
          == ViewGroup.LayoutParams.WRAP_CONTENT) {

        requestedWidth =
            Math.min(
                maxDrawerWidth,
                dp(
                    DEFAULT_DRAWER_WIDTH_DP));

      } else {

        requestedWidth =
            lp.width;
      }

      requestedWidth =
          Math.max(
              0,
              Math.min(
                  requestedWidth,
                  maxDrawerWidth));

      int childWidthSpec =
          MeasureSpec.makeMeasureSpec(
              requestedWidth,
              MeasureSpec.EXACTLY);

      int childHeightSpec =
          getChildMeasureSpec(
              heightMeasureSpec,
              lp.topMargin
                  + lp.bottomMargin,
              lp.height);

      child.measure(
          childWidthSpec,
          childHeightSpec);
    }
  }

  // ============================================================
  // LAYOUT
  // ============================================================

  @Override
  protected void onLayout(
      boolean changed,
      int left,
      int top,
      int right,
      int bottom) {

    int width =
        right - left;

    int height =
        bottom - top;

    for (int i = 0;
         i < getChildCount();
         i++) {

      View child =
          getChildAt(i);

      LayoutParams lp =
          getDrawerLayoutParams(child);

      if (!isDrawerView(child)) {

        child.layout(
            lp.leftMargin,
            lp.topMargin,
            width - lp.rightMargin,
            height - lp.bottomMargin);

        continue;
      }

      int drawerWidth =
          child.getMeasuredWidth();

      int drawerHeight =
          child.getMeasuredHeight();

      int gravity =
          resolveGravity(
              lp.gravity);

      int childTop =
          lp.topMargin;

      if (gravity == Gravity.LEFT) {

        int childLeft =
            -drawerWidth
                + (int)
                (drawerWidth
                    * lp.onScreen);

        child.layout(
            childLeft,
            childTop,
            childLeft + drawerWidth,
            childTop + drawerHeight);

      } else {

        int childLeft =
            width
                - (int)
                (drawerWidth
                    * lp.onScreen);

        child.layout(
            childLeft,
            childTop,
            childLeft + drawerWidth,
            childTop + drawerHeight);
      }

      child.setVisibility(
          lp.onScreen > 0f
              ? VISIBLE
              : INVISIBLE);
    }

    mFirstLayout =
        false;

    updateScrimOpacity();

    if (mSavedOpenGravity
        != Gravity.NO_GRAVITY) {

      int gravity =
          mSavedOpenGravity;

      mSavedOpenGravity =
          Gravity.NO_GRAVITY;

      openDrawer(
          gravity,
          false);
    }
  }

  // ============================================================
  // DRAW ORDER
  // ============================================================

  @Override
  protected void dispatchDraw(
      Canvas canvas) {

    View content =
        getContentView();

    /*
     * Draw content first.
     */
    if (content != null
        && content.getVisibility()
            != GONE) {

      drawChild(
          canvas,
          content,
          getDrawingTime());
    }

    /*
     * Draw scrim above content but below
     * the drawers.
     */
    if (mScrimOpacity > 0f) {

      int alpha =
          (int)
          (Color.alpha(mScrimColor)
              * mScrimOpacity);

      if (alpha > 0) {

        mScrimPaint.setAlpha(
            alpha);

        canvas.drawRect(
            0,
            0,
            getWidth(),
            getHeight(),
            mScrimPaint);

        mScrimPaint.setAlpha(255);
      }
    }

    /*
     * Left drawer.
     */
    if (mLeftDrawer != null
        && mLeftDrawer.getVisibility()
            != GONE) {

      drawChild(
          canvas,
          mLeftDrawer,
          getDrawingTime());

      drawDrawerShadow(
          canvas,
          mLeftDrawer,
          Gravity.LEFT);
    }

    /*
     * Right drawer.
     */
    if (mRightDrawer != null
        && mRightDrawer.getVisibility()
            != GONE) {

      drawChild(
          canvas,
          mRightDrawer,
          getDrawingTime());

      drawDrawerShadow(
          canvas,
          mRightDrawer,
          Gravity.RIGHT);
    }
  }

  private void drawDrawerShadow(
      Canvas canvas,
      View drawer,
      int gravity) {

    if (drawer == null) {
      return;
    }

    LayoutParams lp =
        getDrawerLayoutParams(drawer);

    if (lp.onScreen <= 0f) {
      return;
    }

    Drawable shadow =
        gravity == Gravity.LEFT
            ? mLeftShadow
            : mRightShadow;

    if (shadow == null) {
      return;
    }

    int width =
        shadow.getIntrinsicWidth();

    if (width <= 0) {
      return;
    }

    if (gravity == Gravity.LEFT) {

      shadow.setBounds(
          drawer.getRight(),
          drawer.getTop(),
          drawer.getRight() + width,
          drawer.getBottom());

    } else {

      shadow.setBounds(
          drawer.getLeft() - width,
          drawer.getTop(),
          drawer.getLeft(),
          drawer.getBottom());
    }

    shadow.draw(canvas);
  }

  // ============================================================
  // TOUCH INTERCEPTION
  // ============================================================

  @Override
  public boolean onInterceptTouchEvent(
      MotionEvent event) {

    final int action =
        event.getActionMasked();

    switch (action) {

      case MotionEvent.ACTION_DOWN:

        mDisallowIntercept =
            false;

        mDragging =
            false;

        mDragGravity =
            Gravity.NO_GRAVITY;

        mTouchTargetGravity =
            Gravity.NO_GRAVITY;

        mTapOnScrim =
            false;

        mActivePointerId =
            event.getPointerId(0);

        mInitialX =
            event.getX();

        mInitialY =
            event.getY();

        mLastX =
            mInitialX;

        mLastY =
            mInitialY;

        ensureVelocityTracker();

        mVelocityTracker.clear();

        mVelocityTracker.addMovement(
            event);

        /*
         * Important:
         *
         * If the drawer itself is touched,
         * do NOT intercept ACTION_DOWN.
         *
         * This lets buttons, EditTexts,
         * ListViews, ScrollViews etc. receive
         * the original DOWN event.
         */
        int drawerGravity =
            findOpenDrawerUnder(
                mInitialX,
                mInitialY);

        if (drawerGravity
            != Gravity.NO_GRAVITY) {

          mTouchTargetGravity =
              drawerGravity;

          return false;
        }

        /*
         * When a drawer is open, a touch on the
         * content/scrim is a scrim interaction.
         *
         * AndroidX has the same basic distinction:
         * it separately detects a tap on content
         * while a drawer is visible.
         */
        if (hasVisibleDrawer()
            && isPointInContent(
                mInitialX,
                mInitialY)) {

          mTapOnScrim =
              true;

          return true;
        }

        /*
         * Otherwise this may become an edge
         * opening gesture.
         */
        return false;

      case MotionEvent.ACTION_MOVE:

        if (mDisallowIntercept) {
          return false;
        }

        ensureVelocityTracker();

        mVelocityTracker.addMovement(
            event);

        int index =
            event.findPointerIndex(
                mActivePointerId);

        if (index < 0) {
          return false;
        }

        float x =
            event.getX(index);

        float y =
            event.getY(index);

        float dx =
            x - mInitialX;

        float dy =
            y - mInitialY;

        /*
         * Not enough movement yet.
         */
        if (Math.abs(dx)
            <= mTouchSlop
            && Math.abs(dy)
                <= mTouchSlop) {

          return false;
        }

        /*
         * Vertical movement belongs to
         * ScrollView/ListView/etc.
         */
        if (Math.abs(dy)
            >= Math.abs(dx)) {

          return false;
        }

        /*
         * If the touch began inside an open
         * drawer, only the direction that moves
         * that drawer toward closed is accepted.
         */
        if (mTouchTargetGravity
            == Gravity.LEFT) {

          if (dx >= 0f) {
            return false;
          }

          if (getDrawerLockMode(
                  Gravity.LEFT)
              == LOCK_MODE_LOCKED_OPEN) {

            return false;
          }

          beginDrawerDrag(
              Gravity.LEFT);

          return true;
        }

        if (mTouchTargetGravity
            == Gravity.RIGHT) {

          if (dx <= 0f) {
            return false;
          }

          if (getDrawerLockMode(
                  Gravity.RIGHT)
              == LOCK_MODE_LOCKED_OPEN) {

            return false;
          }

          beginDrawerDrag(
              Gravity.RIGHT);

          return true;
        }

        /*
         * Closed drawer edge opening.
         */
        if (dx > 0f
            && mInitialX <= mEdgeSize
            && mLeftDrawer != null
            && getDrawerLockMode(
                    Gravity.LEFT)
                != LOCK_MODE_LOCKED_CLOSED) {

          beginDrawerDrag(
              Gravity.LEFT);

          return true;
        }

        if (dx < 0f
            && mInitialX >= getWidth()
                - mEdgeSize
            && mRightDrawer != null
            && getDrawerLockMode(
                    Gravity.RIGHT)
                != LOCK_MODE_LOCKED_CLOSED) {

          beginDrawerDrag(
              Gravity.RIGHT);

          return true;
        }

        return false;

      case MotionEvent.ACTION_POINTER_UP:

        handlePointerUp(event);

        return false;

      case MotionEvent.ACTION_UP:
      case MotionEvent.ACTION_CANCEL:

        releaseTouchTracker();

        return false;
    }

    return false;
  }

  // ============================================================
  // TOUCH HANDLING
  // ============================================================

  @Override
  public boolean onTouchEvent(
      MotionEvent event) {

    ensureVelocityTracker();

    mVelocityTracker.addMovement(
        event);

    switch (event.getActionMasked()) {

      case MotionEvent.ACTION_DOWN:

        mActivePointerId =
            event.getPointerId(0);

        mInitialX =
            event.getX();

        mInitialY =
            event.getY();

        mLastX =
            mInitialX;

        mLastY =
            mInitialY;

        return true;

      case MotionEvent.ACTION_MOVE:

        if (mTapOnScrim) {
          return true;
        }

        handleDragMove(event);

        return true;

      case MotionEvent.ACTION_POINTER_DOWN:

        handlePointerDown(event);

        return true;

      case MotionEvent.ACTION_POINTER_UP:

        handlePointerUp(event);

        return true;

      case MotionEvent.ACTION_UP:

        if (mTapOnScrim) {

          /*
           * Scrim/content tap.
           *
           * Don't touch inside-drawer views here;
           * this path only occurs when ACTION_DOWN
           * started on content.
           */
          closeVisibleDrawer();

          mTapOnScrim =
              false;

          releaseTouchTracker();

          return true;
        }

        finishDrag(event);

        return true;

      case MotionEvent.ACTION_CANCEL:

        cancelDrag();

        return true;
    }

    return true;
  }

  private void handleDragMove(
      MotionEvent event) {

    if (!mDragging) {
      return;
    }

    int index =
        event.findPointerIndex(
            mActivePointerId);

    if (index < 0) {
      return;
    }

    float x =
        event.getX(index);

    float dx =
        x - mLastX;

    dragDrawer(dx);

    mLastX = x;
    mLastY = event.getY(index);
  }

  private void beginDrawerDrag(
      int gravity) {

    mDragging =
        true;

    mDragGravity =
        gravity;

    mTouchTargetGravity =
        gravity;

    cancelScroller(gravity);

    setDrawerState(
        STATE_DRAGGING);
  }

  private void dragDrawer(
      float dx) {

    View drawer =
        findDrawerWithGravity(
            mDragGravity);

    if (drawer == null) {
      return;
    }

    LayoutParams lp =
        getDrawerLayoutParams(drawer);

    int width =
        drawer.getWidth();

    if (width <= 0) {
      return;
    }

    float newOffset;

    if (mDragGravity
        == Gravity.LEFT) {

      newOffset =
          lp.onScreen
              + dx / width;

    } else {

      newOffset =
          lp.onScreen
              - dx / width;
    }

    moveDrawerToOffset(
        drawer,
        clamp(
            newOffset,
            0f,
            1f));
  }

  private void finishDrag(
      MotionEvent event) {

    if (!mDragging) {

      releaseTouchTracker();

      return;
    }

    if (mVelocityTracker != null) {

      mVelocityTracker.computeCurrentVelocity(
          1000,
          mMaxFlingVelocity);
    }

    float velocityX =
        mVelocityTracker == null
            ? 0f
            : mVelocityTracker
                .getXVelocity(
                    mActivePointerId);

    View drawer =
        findDrawerWithGravity(
            mDragGravity);

    if (drawer != null) {

      LayoutParams lp =
          getDrawerLayoutParams(
              drawer);

      float target =
          calculateTarget(
              lp.onScreen,
              velocityX,
              mDragGravity);

      smoothSlideDrawer(
          mDragGravity,
          target);
    }

    mDragging =
        false;

    mDragGravity =
        Gravity.NO_GRAVITY;

    releaseTouchTracker();
  }

  private void cancelDrag() {

    if (mDragging) {

      View drawer =
          findDrawerWithGravity(
              mDragGravity);

      if (drawer != null) {

        LayoutParams lp =
            getDrawerLayoutParams(
                drawer);

        float target =
            lp.onScreen >= 0.5f
                ? 1f
                : 0f;

        smoothSlideDrawer(
            mDragGravity,
            target);
      }
    }

    mDragging =
        false;

    mDragGravity =
        Gravity.NO_GRAVITY;

    releaseTouchTracker();
  }

  // ============================================================
  // TARGET CALCULATION
  // ============================================================

  private float calculateTarget(
      float offset,
      float velocityX,
      int gravity) {

    if (Math.abs(velocityX)
        >= mMinFlingVelocity) {

      if (gravity == Gravity.LEFT) {

        return velocityX > 0f
            ? 1f
            : 0f;

      } else {

        return velocityX < 0f
            ? 1f
            : 0f;
      }
    }

    return offset >= 0.5f
        ? 1f
        : 0f;
  }

  // ============================================================
  // ANIMATION
  // ============================================================

  private void smoothSlideDrawer(
      int gravity,
      float target) {

    gravity =
        resolveGravity(gravity);

    View drawer =
        findDrawerWithGravity(
            gravity);

    if (drawer == null) {
      return;
    }

    LayoutParams lp =
        getDrawerLayoutParams(
            drawer);

    float current =
        lp.onScreen;

    target =
        clamp(
            target,
            0f,
            1f);

    if (Math.abs(
        current - target)
        < 0.0001f) {

      moveDrawerToOffset(
          drawer,
          target);

      setDrawerState(
          STATE_IDLE);

      return;
    }

    if (target > 0f) {
      closeOtherDrawer(gravity);
    }

    Scroller scroller =
        gravity == Gravity.LEFT
            ? mLeftScroller
            : mRightScroller;

    scroller.abortAnimation();

    int width =
        drawer.getWidth();

    if (width <= 0) {

      moveDrawerToOffset(
          drawer,
          target);

      return;
    }

    int start =
        Math.round(
            current * width);

    int end =
        Math.round(
            target * width);

    int distance =
        Math.abs(
            end - start);

    int duration =
        ANIMATION_BASE_DURATION
            + (int)
              (ANIMATION_EXTRA_DURATION
                  * Math.min(
                      1f,
                      distance
                          / (float) width));

    scroller.startScroll(
        start,
        0,
        end - start,
        0,
        duration);

    setDrawerState(
        STATE_SETTLING);

    postInvalidateOnAnimation();
  }

  @Override
  public void computeScroll() {

    boolean left =
        mLeftScroller
            .computeScrollOffset();

    boolean right =
        mRightScroller
            .computeScrollOffset();

    if (left
        && mLeftDrawer != null) {

      updateFromScroller(
          mLeftDrawer,
          mLeftScroller);
    }

    if (right
        && mRightDrawer != null) {

      updateFromScroller(
          mRightDrawer,
          mRightScroller);
    }

    if (left || right) {

      postInvalidateOnAnimation();

      return;
    }

    if (mDrawerState
        == STATE_SETTLING) {

      setDrawerState(
          STATE_IDLE);
    }
  }

  private void updateFromScroller(
      View drawer,
      Scroller scroller) {

    int width =
        drawer.getWidth();

    if (width <= 0) {
      return;
    }

    float offset =
        scroller.getCurrX()
            / (float) width;

    moveDrawerToOffset(
        drawer,
        clamp(
            offset,
            0f,
            1f));
  }

  private void cancelScroller(
      int gravity) {

    gravity =
        resolveGravity(gravity);

    if (gravity == Gravity.LEFT) {

      mLeftScroller.abortAnimation();

    } else if (gravity
        == Gravity.RIGHT) {

      mRightScroller.abortAnimation();
    }
  }

  // ============================================================
  // PHYSICAL DRAWER MOVEMENT
  // ============================================================

  private void moveDrawerToOffset(
      View drawer,
      float offset) {

    if (drawer == null) {
      return;
    }

    LayoutParams lp =
        getDrawerLayoutParams(
            drawer);

    float old =
        lp.onScreen;

    offset =
        clamp(
            offset,
            0f,
            1f);

    lp.onScreen =
        offset;

    int width =
        drawer.getWidth();

    if (width > 0) {

      int gravity =
          resolveGravity(
              lp.gravity);

      int newLeft;

      if (gravity
          == Gravity.LEFT) {

        newLeft =
            -width
                + (int)
                (width * offset);

      } else {

        newLeft =
            getWidth()
                - (int)
                (width * offset);
      }

      int delta =
          newLeft
              - drawer.getLeft();

      if (delta != 0) {

        drawer.offsetLeftAndRight(
            delta);
      }
    }

    drawer.setVisibility(
        offset > 0f
            ? VISIBLE
            : INVISIBLE);

    updateScrimOpacity();

    invalidate();

    if (Math.abs(
        old - offset)
        > 0.00001f) {

      notifySlide(
          drawer,
          offset);
    }

    if (offset >= 1f
        && old < 1f) {

      if (!lp.knownOpen) {

        lp.knownOpen =
            true;

        notifyOpened(drawer);
      }

    } else if (offset <= 0f
        && old > 0f) {

      if (lp.knownOpen) {

        lp.knownOpen =
            false;

        notifyClosed(drawer);
      }
    }
  }

  private void updateScrimOpacity() {

    float left =
        mLeftDrawer == null
            ? 0f
            : getDrawerLayoutParams(
                    mLeftDrawer)
                .onScreen;

    float right =
        mRightDrawer == null
            ? 0f
            : getDrawerLayoutParams(
                    mRightDrawer)
                .onScreen;

    mScrimOpacity =
        Math.max(left, right);
  }

  // ============================================================
  // LISTENER NOTIFICATIONS
  // ============================================================

  private void notifySlide(
      View drawer,
      float offset) {

    for (DrawerListener listener :
        mListeners) {

      listener.onDrawerSlide(
          drawer,
          offset);
    }
  }

  private void notifyOpened(
      View drawer) {

    for (DrawerListener listener :
        mListeners) {

      listener.onDrawerOpened(
          drawer);
    }
  }

  private void notifyClosed(
      View drawer) {

    for (DrawerListener listener :
        mListeners) {

      listener.onDrawerClosed(
          drawer);
    }
  }

  private void setDrawerState(
      int state) {

    if (mDrawerState == state) {
      return;
    }

    mDrawerState =
        state;

    for (DrawerListener listener :
        mListeners) {

      listener.onDrawerStateChanged(
          state);
    }
  }

  public int getDrawerState() {
    return mDrawerState;
  }

  // ============================================================
  // SCRIM / HIT TEST
  // ============================================================

  private boolean hasVisibleDrawer() {

    return (mLeftDrawer != null
        && isDrawerVisible(
            mLeftDrawer))
        || (mRightDrawer != null
        && isDrawerVisible(
            mRightDrawer));
  }

  private void closeVisibleDrawer() {

    View drawer =
        getOpenDrawer();

    if (drawer != null) {

      LayoutParams lp =
          getDrawerLayoutParams(
              drawer);

      if (getDrawerLockMode(
              lp.gravity)
          != LOCK_MODE_LOCKED_OPEN) {

        closeDrawer(drawer);
      }

      return;
    }

    /*
     * Also handle a drawer that is only
     * partially open during a gesture.
     */
    if (mLeftDrawer != null
        && isDrawerVisible(
            mLeftDrawer)) {

      closeDrawer(
          mLeftDrawer);

    } else if (mRightDrawer != null
        && isDrawerVisible(
            mRightDrawer)) {

      closeDrawer(
          mRightDrawer);
    }
  }

  private int findOpenDrawerUnder(
      float x,
      float y) {

    if (mLeftDrawer != null
        && isDrawerVisible(
            mLeftDrawer)
        && isPointInside(
            mLeftDrawer,
            x,
            y)) {

      return Gravity.LEFT;
    }

    if (mRightDrawer != null
        && isDrawerVisible(
            mRightDrawer)
        && isPointInside(
            mRightDrawer,
            x,
            y)) {

      return Gravity.RIGHT;
    }

    return Gravity.NO_GRAVITY;
  }

  private boolean isPointInside(
      View view,
      float x,
      float y) {

    mDrawerRect.set(
        view.getLeft(),
        view.getTop(),
        view.getRight(),
        view.getBottom());

    return mDrawerRect.contains(
        (int) x,
        (int) y);
  }

  private boolean isPointInContent(
      float x,
      float y) {

    View content =
        getContentView();

    if (content == null) {
      return false;
    }

    return isPointInside(
        content,
        x,
        y);
  }

  // ============================================================
  // POINTERS
  // ============================================================

  private void ensureVelocityTracker() {

    if (mVelocityTracker == null) {

      mVelocityTracker =
          VelocityTracker.obtain();
    }
  }

  private void handlePointerDown(
      MotionEvent event) {

    int index =
        event.getActionIndex();

    mActivePointerId =
        event.getPointerId(index);

    mLastX =
        event.getX(index);

    mLastY =
        event.getY(index);
  }

  private void handlePointerUp(
      MotionEvent event) {

    int actionIndex =
        event.getActionIndex();

    int pointerId =
        event.getPointerId(
            actionIndex);

    if (pointerId
        != mActivePointerId) {

      return;
    }

    int newIndex =
        actionIndex == 0
            ? 1
            : 0;

    if (newIndex
        < event.getPointerCount()) {

      mActivePointerId =
          event.getPointerId(
              newIndex);

      mLastX =
          event.getX(newIndex);

      mLastY =
          event.getY(newIndex);

    } else {

      mActivePointerId =
          INVALID_POINTER;
    }
  }

  private void releaseTouchTracker() {

    mActivePointerId =
        INVALID_POINTER;

    if (mVelocityTracker != null) {

      mVelocityTracker.recycle();

      mVelocityTracker = null;
    }

    mTapOnScrim =
        false;
  }

  // ============================================================
  // REQUEST DISALLOW
  // ============================================================

  @Override
  public void requestDisallowInterceptTouchEvent(
      boolean disallowIntercept) {

    mDisallowIntercept =
        disallowIntercept;

    /*
     * A drawer's ScrollView/ListView is
     * allowed to request disallow-intercept.
     *
     * We must not steal that interaction.
     */
    super.requestDisallowInterceptTouchEvent(
        disallowIntercept);
  }

  // ============================================================
  // GRAVITY
  // ============================================================

  private int resolveGravity(
      int gravity) {

    if ((gravity & Gravity.START)
        == Gravity.START) {

      return isRtl()
          ? Gravity.RIGHT
          : Gravity.LEFT;
    }

    if ((gravity & Gravity.END)
        == Gravity.END) {

      return isRtl()
          ? Gravity.LEFT
          : Gravity.RIGHT;
    }

    if ((gravity & Gravity.LEFT)
        == Gravity.LEFT) {

      return Gravity.LEFT;
    }

    if ((gravity & Gravity.RIGHT)
        == Gravity.RIGHT) {

      return Gravity.RIGHT;
    }

    return Gravity.NO_GRAVITY;
  }

  private boolean isRtl() {

    if (android.os.Build.VERSION.SDK_INT
        >= 17) {

      return getLayoutDirection()
          == LAYOUT_DIRECTION_RTL;
    }

    return false;
  }

  @Override
  public void onRtlPropertiesChanged(
      int layoutDirection) {

    super.onRtlPropertiesChanged(
        layoutDirection);

    requestLayout();
  }

  // ============================================================
  // DRAWER DISCOVERY
  // ============================================================

  private View findDrawerWithGravity(
      int gravity) {

    gravity =
        resolveGravity(gravity);

    if (gravity == Gravity.LEFT) {
      return mLeftDrawer;
    }

    if (gravity == Gravity.RIGHT) {
      return mRightDrawer;
    }

    return null;
  }

  private boolean isDrawerView(
      View child) {

    if (child == null) {
      return false;
    }

    ViewGroup.LayoutParams raw =
        child.getLayoutParams();

    if (!(raw instanceof LayoutParams)) {
      return false;
    }

    return isDrawerLayoutParams(
        (LayoutParams) raw);
  }

  private boolean isDrawerLayoutParams(
      LayoutParams lp) {

    int gravity =
        resolveGravity(lp.gravity);

    return gravity == Gravity.LEFT
        || gravity == Gravity.RIGHT;
  }

  private LayoutParams
  getDrawerLayoutParams(
      View child) {

    ViewGroup.LayoutParams params =
        child.getLayoutParams();

    if (!(params instanceof LayoutParams)) {

      LayoutParams lp =
          new LayoutParams(params);

      child.setLayoutParams(lp);

      return lp;
    }

    return (LayoutParams) params;
  }

  // ============================================================
  // OTHER DRAWER
  // ============================================================

  private void closeOtherDrawer(
      int gravity) {

    gravity =
        resolveGravity(gravity);

    if (gravity == Gravity.LEFT) {

      if (mRightDrawer != null
          && isDrawerVisible(
              mRightDrawer)) {

        closeDrawer(
            mRightDrawer);
      }

    } else {

      if (mLeftDrawer != null
          && isDrawerVisible(
              mLeftDrawer)) {

        closeDrawer(
            mLeftDrawer);
      }
    }
  }

  // ============================================================
  // SAVED STATE
  // ============================================================

  @Override
  protected Parcelable
  onSaveInstanceState() {

    Parcelable superState =
        super.onSaveInstanceState();

    SavedState state =
        new SavedState(
            superState);

    View drawer =
        getOpenDrawer();

    if (drawer != null) {

      LayoutParams lp =
          getDrawerLayoutParams(
              drawer);

      state.openGravity =
          lp.gravity;

    } else {

      state.openGravity =
          Gravity.NO_GRAVITY;
    }

    return state;
  }

  @Override
  protected void onRestoreInstanceState(
      Parcelable state) {

    if (!(state instanceof SavedState)) {

      super.onRestoreInstanceState(
          state);

      return;
    }

    SavedState ss =
        (SavedState) state;

    super.onRestoreInstanceState(
        ss.getSuperState());

    mSavedOpenGravity =
        ss.openGravity;
  }

  private static class SavedState
      extends BaseSavedState {

    int openGravity =
        Gravity.NO_GRAVITY;

    SavedState(
        Parcelable superState) {

      super(superState);
    }

    SavedState(
        Parcel in) {

      super(in);

      openGravity =
          in.readInt();
    }

    @Override
    public void writeToParcel(
        Parcel out,
        int flags) {

      super.writeToParcel(
          out,
          flags);

      out.writeInt(
          openGravity);
    }

    public static final Creator<SavedState>
        CREATOR =
        new Creator<SavedState>() {

          @Override
          public SavedState
          createFromParcel(
              Parcel in) {

            return new SavedState(
                in);
          }

          @Override
          public SavedState[]
          newArray(int size) {

            return new SavedState[size];
          }
        };
  }

  // ============================================================
  // FOCUS
  // ============================================================

  @Override
  public void addFocusables(
      ArrayList<View> views,
      int direction,
      int focusableMode) {

    View drawer =
        getOpenDrawer();

    if (drawer != null) {

      drawer.addFocusables(
          views,
          direction,
          focusableMode);

      return;
    }

    View content =
        getContentView();

    if (content != null) {

      content.addFocusables(
          views,
          direction,
          focusableMode);
    }
  }

  // ============================================================
  // WINDOW
  // ============================================================

  @Override
  protected void onAttachedToWindow() {

    super.onAttachedToWindow();

    mFirstLayout =
        true;
  }

  @Override
  protected void onDetachedFromWindow() {

    mLeftScroller.abortAnimation();

    mRightScroller.abortAnimation();

    if (mVelocityTracker != null) {

      mVelocityTracker.recycle();

      mVelocityTracker = null;
    }

    super.onDetachedFromWindow();
  }

  // ============================================================
  // BACK
  // ============================================================

  public boolean onBackPressed() {

    View drawer =
        getOpenDrawer();

    if (drawer == null) {
      return false;
    }

    LayoutParams lp =
        getDrawerLayoutParams(
            drawer);

    if (getDrawerLockMode(
            lp.gravity)
        == LOCK_MODE_LOCKED_OPEN) {

      return false;
    }

    closeDrawer(drawer);

    return true;
  }

  // ============================================================
  // UTILITIES
  // ============================================================

  private int dp(float value) {

    DisplayMetrics dm =
        getResources()
            .getDisplayMetrics();

    return (int)
        (value
            * dm.density
            + 0.5f);
  }

  private float clamp(
      float value,
      float min,
      float max) {

    if (value < min) {
      return min;
    }

    if (value > max) {
      return max;
    }

    return value;
  }
}