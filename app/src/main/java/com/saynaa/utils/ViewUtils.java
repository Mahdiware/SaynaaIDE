package com.saynaa.utils;

import android.content.Context;
import android.graphics.Rect;
import android.os.Handler;
import android.os.Looper;
import android.view.View;
import android.view.ViewGroup;

public final class ViewUtils {
  private ViewUtils() {
    // Utility class
  }

  // DP <-> PX
  public static int dpToPx(Context context, float dp) {
    return Math.round(dp * context.getResources().getDisplayMetrics().density);
  }

  public static float dpToPxF(Context context, float dp) {
    return dp * context.getResources().getDisplayMetrics().density;
  }

  public static int pxToDp(Context context, float px) {
    return Math.round(px / context.getResources().getDisplayMetrics().density);
  }

  public static float pxToDpF(Context context, float px) {
    return px / context.getResources().getDisplayMetrics().density;
  }

  // SP <-> PX
  public static int spToPx(Context context, float sp) {
    return Math.round(sp * context.getResources().getDisplayMetrics().scaledDensity);
  }

  public static float spToPxF(Context context, float sp) {
    return sp * context.getResources().getDisplayMetrics().scaledDensity;
  }

  public static int pxToSp(Context context, float px) {
    return Math.round(px / context.getResources().getDisplayMetrics().scaledDensity);
  }

  public static float pxToSpF(Context context, float px) {
    return px / context.getResources().getDisplayMetrics().scaledDensity;
  }

  // SIZE
  public static int getWidth(View view) {
    return view.getWidth();
  }

  public static int getHeight(View view) {
    return view.getHeight();
  }

  public static int getWidthDp(View view) {
    return pxToDp(view.getContext(), view.getWidth());
  }

  public static int getHeightDp(View view) {
    return pxToDp(view.getContext(), view.getHeight());
  }

  // MEASURED SIZE
  public static int getMeasuredWidth(View view) {
    return view.getMeasuredWidth();
  }

  public static int getMeasuredHeight(View view) {
    return view.getMeasuredHeight();
  }

  public static int getMeasuredWidthDp(View view) {
    return pxToDp(view.getContext(), view.getMeasuredWidth());
  }

  public static int getMeasuredHeightDp(View view) {
    return pxToDp(view.getContext(), view.getMeasuredHeight());
  }

  // MEASURE
  public static void measure(View view) {
    view.measure(View.MeasureSpec.makeMeasureSpec(0, View.MeasureSpec.UNSPECIFIED),
        View.MeasureSpec.makeMeasureSpec(0, View.MeasureSpec.UNSPECIFIED));
  }

  public static void measureWidth(View view) {
    view.measure(View.MeasureSpec.makeMeasureSpec(0, View.MeasureSpec.UNSPECIFIED),
        View.MeasureSpec.makeMeasureSpec(view.getHeight(), View.MeasureSpec.EXACTLY));
  }

  public static void measureHeight(View view) {
    view.measure(View.MeasureSpec.makeMeasureSpec(view.getWidth(), View.MeasureSpec.EXACTLY),
        View.MeasureSpec.makeMeasureSpec(0, View.MeasureSpec.UNSPECIFIED));
  }

  // POST LAYOUT
  public static void afterLayout(View view, Runnable action) {
    view.post(action);
  }

  public static boolean isLaidOut(View view) {
    return view.isLaidOut();
  }

  // POSITION
  public static float getX(View view) {
    return view.getX();
  }

  public static float getY(View view) {
    return view.getY();
  }

  public static int getLeft(View view) {
    return view.getLeft();
  }

  public static int getTop(View view) {
    return view.getTop();
  }

  public static int getRight(View view) {
    return view.getRight();
  }

  public static int getBottom(View view) {
    return view.getBottom();
  }

  // SCREEN POSITION
  public static int getScreenX(View view) {
    int[] location = new int[2];
    view.getLocationOnScreen(location);

    return location[0];
  }

  public static int getScreenY(View view) {
    int[] location = new int[2];
    view.getLocationOnScreen(location);

    return location[1];
  }

  public static int getWindowX(View view) {
    int[] location = new int[2];
    view.getLocationInWindow(location);

    return location[0];
  }

  public static int getWindowY(View view) {
    int[] location = new int[2];
    view.getLocationInWindow(location);

    return location[1];
  }

  // BOUNDS
  public static Rect getGlobalBounds(View view) {
    Rect rect = new Rect();
    view.getGlobalVisibleRect(rect);

    return rect;
  }

  public static Rect getLocalBounds(View view) {
    return new Rect(0, 0, view.getWidth(), view.getHeight());
  }

  // PADDING
  public static int getPaddingLeft(View view) {
    return view.getPaddingLeft();
  }

  public static int getPaddingTop(View view) {
    return view.getPaddingTop();
  }

  public static int getPaddingRight(View view) {
    return view.getPaddingRight();
  }

  public static int getPaddingBottom(View view) {
    return view.getPaddingBottom();
  }

  public static void setPadding(View view, int left, int top, int right, int bottom) {
    view.setPadding(left, top, right, bottom);
  }

  public static void setPaddingDp(View view, float left, float top, float right, float bottom) {
    Context context = view.getContext();

    view.setPadding(dpToPx(context, left), dpToPx(context, top), dpToPx(context, right),
        dpToPx(context, bottom));
  }

  // MARGINS
  private static ViewGroup.MarginLayoutParams getMarginParams(View view) {
    ViewGroup.LayoutParams params = view.getLayoutParams();

    if (params instanceof ViewGroup.MarginLayoutParams) {
      return (ViewGroup.MarginLayoutParams) params;
    }

    return null;
  }

  public static int getMarginLeft(View view) {
    ViewGroup.MarginLayoutParams params = getMarginParams(view);

    return params != null ? params.leftMargin : 0;
  }

  public static int getMarginTop(View view) {
    ViewGroup.MarginLayoutParams params = getMarginParams(view);

    return params != null ? params.topMargin : 0;
  }

  public static int getMarginRight(View view) {
    ViewGroup.MarginLayoutParams params = getMarginParams(view);

    return params != null ? params.rightMargin : 0;
  }

  public static int getMarginBottom(View view) {
    ViewGroup.MarginLayoutParams params = getMarginParams(view);

    return params != null ? params.bottomMargin : 0;
  }

  // TOTAL SIZE INCLUDING MARGINS
  public static int getTotalWidth(View view) {
    return view.getWidth() + getMarginLeft(view) + getMarginRight(view);
  }

  public static int getTotalHeight(View view) {
    return view.getHeight() + getMarginTop(view) + getMarginBottom(view);
  }

  public static int getTotalWidthDp(View view) {
    return pxToDp(view.getContext(), getTotalWidth(view));
  }

  public static int getTotalHeightDp(View view) {
    return pxToDp(view.getContext(), getTotalHeight(view));
  }

  // VISIBILITY
  public static void visible(View view) {
    view.setVisibility(View.VISIBLE);
  }

  public static void invisible(View view) {
    view.setVisibility(View.INVISIBLE);
  }

  public static void gone(View view) {
    view.setVisibility(View.GONE);
  }

  public static boolean isVisible(View view) {
    return view.getVisibility() == View.VISIBLE;
  }

  public static boolean isInvisible(View view) {
    return view.getVisibility() == View.INVISIBLE;
  }

  public static boolean isGone(View view) {
    return view.getVisibility() == View.GONE;
  }

  // ENABLED
  public static void enable(View view) {
    view.setEnabled(true);
  }

  public static void disable(View view) {
    view.setEnabled(false);
  }

  public static boolean isEnabled(View view) {
    return view.isEnabled();
  }

  // ALPHA
  public static void setAlpha(View view, float alpha) {
    view.setAlpha(alpha);
  }

  public static float getAlpha(View view) {
    return view.getAlpha();
  }

  // SCALE
  public static void setScale(View view, float scale) {
    view.setScaleX(scale);
    view.setScaleY(scale);
  }

  public static void setScaleX(View view, float scale) {
    view.setScaleX(scale);
  }

  public static void setScaleY(View view, float scale) {
    view.setScaleY(scale);
  }

  // ROTATION
  public static void setRotation(View view, float rotation) {
    view.setRotation(rotation);
  }

  public static float getRotation(View view) {
    return view.getRotation();
  }

  // CLICK
  public static void click(View view) {
    view.performClick();
  }

  // REQUEST
  public static void requestLayout(View view) {
    view.requestLayout();
  }

  public static void invalidate(View view) {
    view.invalidate();
  }

  // UI THREAD
  private static final Handler MAIN_HANDLER = new Handler(Looper.getMainLooper());

  public static void runOnUiThread(Runnable action) {
    if (Looper.myLooper() == Looper.getMainLooper()) {
      action.run();

    } else {
      MAIN_HANDLER.post(action);
    }
  }

  public static void post(View view, Runnable action) {
    view.post(action);
  }

  public static void postDelayed(View view, Runnable action, long delayMillis) {
    view.postDelayed(action, delayMillis);
  }

  public static void removeCallbacks(View view, Runnable action) {
    view.removeCallbacks(action);
  }

  // SCREEN DIMENSIONS
  public static int getScreenWidthPx(Context context) {
    return context.getResources().getDisplayMetrics().widthPixels;
  }

  public static int getScreenHeightPx(Context context) {
    return context.getResources().getDisplayMetrics().heightPixels;
  }

  public static int getScreenWidthDp(Context context) {
    return pxToDp(context, getScreenWidthPx(context));
  }

  public static int getScreenHeightDp(Context context) {
    return pxToDp(context, getScreenHeightPx(context));
  }

  // DENSITY
  public static float getDensity(Context context) {
    return context.getResources().getDisplayMetrics().density;
  }

  public static float getScaledDensity(Context context) {
    return context.getResources().getDisplayMetrics().scaledDensity;
  }

  public static int getDensityDpi(Context context) {
    return context.getResources().getDisplayMetrics().densityDpi;
  }
}