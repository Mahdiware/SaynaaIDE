package com.saynaa.view;

import android.animation.Animator;
import android.animation.AnimatorListenerAdapter;
import android.app.Activity;
import android.content.ClipData;
import android.content.ClipboardManager;
import android.content.Context;
import android.content.Intent;
import android.graphics.Color;
import android.graphics.PixelFormat;
import android.graphics.Typeface;
import android.graphics.drawable.GradientDrawable;
import android.graphics.drawable.StateListDrawable;
import android.os.Build;
import android.util.DisplayMetrics;
import android.view.Gravity;
import android.view.MotionEvent;
import android.view.View;
import android.view.WindowManager;
import android.widget.Button;
import android.widget.ImageButton;
import android.widget.ImageView;
import android.widget.LinearLayout;
import android.widget.ScrollView;
import android.widget.Space;
import android.widget.TextView;
import android.widget.Toast;
import java.text.SimpleDateFormat;
import java.util.Date;
import java.util.Locale;

public class ErrorWindow {
  // ============================================================
  // COLORS
  // ============================================================

  private static final int WINDOW_BG = Color.rgb(20, 22, 26);
  private static final int HEADER_BG = Color.rgb(29, 32, 37);
  private static final int CONTENT_BG = Color.rgb(24, 27, 32);
  private static final int FOOTER_BG = Color.rgb(29, 32, 37);

  private static final int TEXT = Color.rgb(242, 243, 245);
  private static final int TEXT_SECONDARY = Color.rgb(145, 152, 163);
  private static final int ERROR = Color.rgb(255, 91, 108);

  private static final int BUTTON_BG = Color.rgb(43, 47, 54);
  private static final int BUTTON_PRESSED = Color.rgb(75, 82, 94);

  private static final int RESIZE_COLOR = Color.rgb(100, 106, 116);

  // ============================================================
  // OBJECTS
  // ============================================================

  private final Context context;
  private final WindowManager manager;

  private View root;
  private WindowManager.LayoutParams params;

  private TextView titleView;
  private TextView subtitleView;
  private TextView messageView;
  private ScrollView scrollView; // Kept at class level for auto-scrolling

  private ImageButton closeButton;
  private Button copyButton;
  private Button shareButton;
  private Button closeTextButton;

  private LinearLayout resizeHandle;

  private final SimpleDateFormat timeFormat;

  // ============================================================
  // STATE
  // ============================================================

  private boolean visible = false;
  private boolean isAnimating = false;

  private float dragDownX;
  private float dragDownY;
  private int dragStartX;
  private int dragStartY;

  private float resizeDownX;
  private float resizeDownY;
  private int resizeStartWidth;
  private int resizeStartHeight;

  private final int screenWidth, screenHeight;
  private final int defaultWidth, defaultHeight;

  // ============================================================
  // CONSTRUCTOR
  // ============================================================

  public ErrorWindow(Context context) {
    this.context = context;
    this.manager = (WindowManager) context.getSystemService(Context.WINDOW_SERVICE);
    this.timeFormat = new SimpleDateFormat("HH:mm:ss", Locale.getDefault());

    DisplayMetrics metrics = context.getResources().getDisplayMetrics();

    screenWidth = metrics.widthPixels;
    screenHeight = metrics.heightPixels;

    defaultWidth = (int) (screenWidth * 0.80f);
    defaultHeight = (int) (screenHeight * 0.60f);

    createLayout();
  }

  private int dp(float value) {
    return (int) (value * context.getResources().getDisplayMetrics().density + 0.5f);
  }

  private GradientDrawable background(int color, float radius) {
    GradientDrawable drawable = new GradientDrawable();
    drawable.setColor(color);
    drawable.setCornerRadius(dp(radius));
    return drawable;
  }

  private StateListDrawable buttonBackground(int normalColor, int pressedColor, float radius) {
    StateListDrawable states = new StateListDrawable();
    states.addState(new int[] {android.R.attr.state_pressed}, background(pressedColor, radius));
    states.addState(new int[] {}, background(normalColor, radius));
    return states;
  }

  // ============================================================
  // CREATE LAYOUT
  // ============================================================

  private void createLayout() {
    // [Root Layout Setup]
    LinearLayout rootLayout = new LinearLayout(context);
    rootLayout.setOrientation(LinearLayout.VERTICAL);
    rootLayout.setBackground(background(WINDOW_BG, 12));
    if (Build.VERSION.SDK_INT >= 21)
      rootLayout.setElevation(dp(12));
    this.root = rootLayout;

    // [Header Setup]
    LinearLayout header = new LinearLayout(context);
    header.setOrientation(LinearLayout.HORIZONTAL);
    header.setGravity(Gravity.CENTER_VERTICAL);
    header.setPadding(dp(12), 0, dp(6), 0);
    header.setBackground(background(HEADER_BG, 12));
    rootLayout.addView(header, new LinearLayout.LayoutParams(-1, dp(56)));

    ImageView errorIcon = new ImageView(context);
    errorIcon.setImageResource(android.R.drawable.ic_dialog_alert);
    errorIcon.setColorFilter(ERROR);
    errorIcon.setPadding(dp(4), dp(4), dp(4), dp(4));
    header.addView(errorIcon, new LinearLayout.LayoutParams(dp(34), dp(34)));

    LinearLayout titleArea = new LinearLayout(context);
    titleArea.setOrientation(LinearLayout.VERTICAL);
    titleArea.setGravity(Gravity.CENTER_VERTICAL);
    titleArea.setPadding(dp(9), 0, 0, 0);
    header.addView(titleArea, new LinearLayout.LayoutParams(0, -1, 1));

    titleView = new TextView(context);
    titleView.setText("Saynaa Error");
    titleView.setTextColor(TEXT);
    titleView.setTextSize(15);
    titleView.setTypeface(null, Typeface.BOLD);
    titleView.setSingleLine(true);
    titleArea.addView(titleView, new LinearLayout.LayoutParams(-1, dp(24)));

    subtitleView = new TextView(context);
    subtitleView.setText("Runtime log");
    subtitleView.setTextColor(TEXT_SECONDARY);
    subtitleView.setTextSize(10);
    subtitleView.setSingleLine(true);
    titleArea.addView(subtitleView, new LinearLayout.LayoutParams(-1, dp(18)));

    closeButton = new ImageButton(context);
    closeButton.setImageResource(android.R.drawable.ic_menu_close_clear_cancel);
    closeButton.setColorFilter(TEXT_SECONDARY);
    closeButton.setPadding(dp(9), dp(9), dp(9), dp(9));
    closeButton.setBackground(buttonBackground(Color.TRANSPARENT, BUTTON_PRESSED, 22));
    header.addView(closeButton, new LinearLayout.LayoutParams(dp(42), dp(42)));

    View errorLine = new View(context);
    errorLine.setBackgroundColor(ERROR);
    rootLayout.addView(errorLine, new LinearLayout.LayoutParams(-1, dp(2)));

    // [Scroll View Setup]
    scrollView = new ScrollView(context);
    scrollView.setFillViewport(true);
    scrollView.setBackgroundColor(CONTENT_BG);
    rootLayout.addView(scrollView, new LinearLayout.LayoutParams(-1, 0, 1));

    LinearLayout messageContainer = new LinearLayout(context);
    messageContainer.setOrientation(LinearLayout.VERTICAL);
    messageContainer.setPadding(dp(15), dp(10), dp(15), dp(14));
    scrollView.addView(messageContainer, new ScrollView.LayoutParams(-1, -2));

    messageView = new TextView(context);
    messageView.setTextColor(TEXT);
    messageView.setTextSize(12);
    messageView.setTypeface(Typeface.MONOSPACE);
    messageView.setLineSpacing(0, 1.15f);
    messageView.setTextIsSelectable(true);
    messageContainer.addView(messageView, new LinearLayout.LayoutParams(-1, -2));

    // [Footer Setup]
    LinearLayout footer = new LinearLayout(context);
    footer.setOrientation(LinearLayout.HORIZONTAL);
    footer.setGravity(Gravity.CENTER_VERTICAL);
    footer.setPadding(dp(10), dp(7), dp(10), dp(7));
    footer.setBackground(background(FOOTER_BG, 12));
    rootLayout.addView(footer, new LinearLayout.LayoutParams(-1, dp(54)));

    shareButton = new Button(context);
    shareButton.setText("Share");
    shareButton.setTextSize(11);
    shareButton.setTextColor(TEXT);
    shareButton.setAllCaps(false);
    shareButton.setBackground(buttonBackground(BUTTON_BG, BUTTON_PRESSED, 7));
    footer.addView(shareButton, new LinearLayout.LayoutParams(dp(70), dp(38)));

    footer.addView(new Space(context), new LinearLayout.LayoutParams(dp(8), 1));

    copyButton = new Button(context);
    copyButton.setText("Copy");
    copyButton.setTextSize(11);
    copyButton.setTextColor(TEXT);
    copyButton.setAllCaps(false);
    copyButton.setBackground(buttonBackground(BUTTON_BG, BUTTON_PRESSED, 7));
    footer.addView(copyButton, new LinearLayout.LayoutParams(dp(70), dp(38)));

    footer.addView(new Space(context), new LinearLayout.LayoutParams(0, 1, 1));

    closeTextButton = new Button(context);
    closeTextButton.setText("Close");
    closeTextButton.setTextSize(11);
    closeTextButton.setTextColor(TEXT);
    closeTextButton.setAllCaps(false);
    closeTextButton.setBackground(buttonBackground(ERROR, Color.rgb(220, 60, 80), 7));
    footer.addView(closeTextButton, new LinearLayout.LayoutParams(dp(70), dp(38)));

    // [Resize Handle]
    resizeHandle = new LinearLayout(context);
    resizeHandle.setGravity(Gravity.CENTER);
    resizeHandle.setBackgroundColor(Color.TRANSPARENT);

    TextView resizeText = new TextView(context);
    resizeText.setText("⇲");
    resizeText.setTextSize(18);
    resizeText.setTextColor(RESIZE_COLOR);
    resizeText.setGravity(Gravity.CENTER);
    resizeHandle.addView(resizeText, new LinearLayout.LayoutParams(dp(24), dp(24)));

    android.widget.FrameLayout frame = new android.widget.FrameLayout(context);
    frame.setBackgroundColor(Color.TRANSPARENT);
    frame.addView(rootLayout, new android.widget.FrameLayout.LayoutParams(-1, -1));

    android.widget.FrameLayout.LayoutParams resizeParams = new android.widget.FrameLayout.LayoutParams(
        dp(30), dp(30), Gravity.RIGHT | Gravity.BOTTOM);
    frame.addView(resizeHandle, resizeParams);
    this.root = frame;

    // [Listeners]
    closeButton.setOnClickListener(v -> hide());
    closeTextButton.setOnClickListener(v -> hide());
    copyButton.setOnClickListener(v -> copyError());
    shareButton.setOnClickListener(v -> shareError());

    // DRAG
    header.setOnTouchListener((v, event) -> {
      if (!visible || isAnimating)
        return true;
      switch (event.getActionMasked()) {
      case MotionEvent.ACTION_DOWN:
        dragDownX = event.getRawX();
        dragDownY = event.getRawY();
        dragStartX = params.x;
        dragStartY = params.y;
        return true;
      case MotionEvent.ACTION_MOVE:
        params.x = dragStartX + (int) (event.getRawX() - dragDownX);
        params.y = dragStartY + (int) (event.getRawY() - dragDownY);
        manager.updateViewLayout(root, params);
        return true;
      }
      return true;
    });

    // RESIZE
    resizeHandle.setOnTouchListener((v, event) -> {
      if (!visible || isAnimating)
        return true;
      switch (event.getActionMasked()) {
      case MotionEvent.ACTION_DOWN:
        resizeDownX = event.getRawX();
        resizeDownY = event.getRawY();
        resizeStartWidth = params.width;
        resizeStartHeight = params.height;
        return true;
      case MotionEvent.ACTION_MOVE:
        int newWidth = resizeStartWidth + (int) (event.getRawX() - resizeDownX);
        int newHeight = resizeStartHeight + (int) (event.getRawY() - resizeDownY);
        params.width = newWidth;
        params.height = newHeight;
        manager.updateViewLayout(root, params);
        return true;
      }
      return true;
    });
  }

  // ============================================================
  // VISIBILITY CONTROL (Now return ErrorWindow for chaining)
  // ============================================================

  /** Show the window with current contents */
  public ErrorWindow show() {
    if (visible) {
      manager.updateViewLayout(root, params);
      return this;
    }

    params = new WindowManager.LayoutParams();
    params.width = defaultWidth;
    params.height = defaultHeight;
    params.gravity = Gravity.LEFT | Gravity.TOP;
    params.x = dp(30);
    params.y = dp(120);
    params.format = PixelFormat.TRANSLUCENT;
    params.type = WindowManager.LayoutParams.TYPE_APPLICATION;
    params.flags = WindowManager.LayoutParams.FLAG_NOT_FOCUSABLE | WindowManager.LayoutParams.FLAG_LAYOUT_IN_SCREEN;

    try {
      root.setAlpha(0f);
      root.setScaleX(0.8f);
      root.setScaleY(0.8f);
      manager.addView(root, params);
      visible = true;

      isAnimating = true;
      root.animate()
          .alpha(1f)
          .scaleX(1f)
          .scaleY(1f)
          .setDuration(200)
          .setListener(new AnimatorListenerAdapter() {
            @Override
            public void onAnimationEnd(Animator animation) {
              isAnimating = false;
            }
          })
          .start();
    } catch (Exception e) {
      visible = false;
    }
    return this;
  }

  public ErrorWindow hide() {
    if (!visible || isAnimating)
      return this;
    isAnimating = true;

    root.animate()
        .alpha(0f)
        .scaleX(0.8f)
        .scaleY(0.8f)
        .setDuration(200)
        .setListener(new AnimatorListenerAdapter() {
          @Override
          public void onAnimationEnd(Animator animation) {
            try {
              if (root != null && root.getWindowToken() != null)
                manager.removeView(root);
            } catch (Exception ignored) {
            }
            visible = false;
            isAnimating = false;
          }
        })
        .start();
    return this;
  }

  // ============================================================
  // CONTENT UPDATERS (Fluent API)
  // ============================================================

  public ErrorWindow setTitle(String title) {
    titleView.setText(safe(title));
    return this;
  }

  public ErrorWindow setSubtitle(String subtitle) {
    subtitleView.setText(safe(subtitle));
    return this;
  }

  /** Overwrites the entire message */
  public ErrorWindow setMessage(String message) {
    messageView.setText(safe(message));
    scrollToBottom();
    return this;
  }

  /** Appends a new line to the existing message */
  public ErrorWindow appendMessage(String message) {
    String currentText = messageView.getText().toString();
    if (currentText.isEmpty()) {
      messageView.setText(safe(message));
    } else {
      messageView.setText(currentText + "\n" + safe(message));
    }
    scrollToBottom();
    return this;
  }

  /** Appends a message with a timestamp at the front [HH:mm:ss] */
  public ErrorWindow appendLog(String message) {
    String timestamp = "[" + timeFormat.format(new Date()) + "] ";
    return appendMessage(timestamp + safe(message));
  }

  /** Clears all text in the message view */
  public ErrorWindow clear() {
    messageView.setText("");
    return this;
  }

  // ============================================================
  // HELPERS
  // ============================================================

  private void scrollToBottom() {
    // Use post to ensure the scroll happens after layout is measured
    scrollView.post(() -> scrollView.fullScroll(ScrollView.FOCUS_DOWN));
  }

  private void copyError() {
    String text = messageView.getText().toString();
    ClipboardManager clipboard = (ClipboardManager) context.getSystemService(Context.CLIPBOARD_SERVICE);
    if (clipboard != null) {
      clipboard.setPrimaryClip(ClipData.newPlainText("Saynaa Error", text));
      Toast.makeText(context, "Copied to clipboard", Toast.LENGTH_SHORT).show();
    }
  }

  private void shareError() {
    String text = "Log Output:\n\n" + messageView.getText().toString();
    Intent intent = new Intent(Intent.ACTION_SEND);
    intent.setType("text/plain");
    intent.putExtra(Intent.EXTRA_TEXT, text);
    context.startActivity(Intent.createChooser(intent, "Share via"));
  }

  public boolean isVisible() {
    return visible;
  }

  private String safe(String value) {
    return value == null ? "" : value;
  }
}