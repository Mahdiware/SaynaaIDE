package com.saynaa.view;

import android.animation.ValueAnimator;
import android.content.Context;
import android.graphics.Canvas;
import android.graphics.Color;
import android.graphics.Paint;
import android.graphics.Path;
import android.graphics.RectF;
import android.graphics.Typeface;
import android.graphics.drawable.Drawable;
import android.text.Editable;
import android.text.InputType;
import android.text.TextWatcher;
import android.util.AttributeSet;
import android.util.TypedValue;
import android.view.Gravity;
import android.view.View;
import android.view.ViewGroup;
import android.view.animation.DecelerateInterpolator;
import android.widget.EditText;
import android.widget.FrameLayout;
import android.widget.ImageView;
import android.widget.TextView;

public class FloatingEditText extends FrameLayout {
  private OutlineField fieldContainer;
  private EditText editText;
  private TextView label;
  private TextView errorText;
  private ImageView errorIcon;

  private String hint = "";

  private int normalColor;
  private int focusColor;
  private int errorColor;
  private int disabledColor;
  private int backgroundColor;

  private float cornerRadius = 8f;

  private int fieldHeight = 56;

  private boolean hasError;
  private boolean enabled = true;

  private float labelProgress;

  private ValueAnimator labelAnimator;
  private ValueAnimator errorAnimator;

  private int horizontalPadding = 14;

  // ==================================================
  // Constructors
  // ==================================================

  public FloatingEditText(Context context) {
    this(context, null);
  }

  public FloatingEditText(Context context, AttributeSet attrs) {
    this(context, attrs, 0);
  }

  public FloatingEditText(Context context, AttributeSet attrs, int defStyleAttr) {
    super(context, attrs, defStyleAttr);

    setClipChildren(false);
    setClipToPadding(false);

    initThemeColors();
    init(context);
  }

  // ==================================================
  // Theme colors
  // ==================================================

  private void initThemeColors() {
    backgroundColor = Color.TRANSPARENT;
    /* getThemeColor(
         android.R.attr.colorBackground
     );*/

    normalColor = getThemeColor(android.R.attr.textColorSecondary);

    focusColor = getThemeColor(android.R.attr.colorAccent);

    disabledColor = getThemeColor(android.R.attr.textColorTertiary);

    /*   if (backgroundColor == Color.TRANSPARENT) {
           backgroundColor = Color.WHITE;
       }*/

    if (normalColor == Color.TRANSPARENT) {
      normalColor = Color.GRAY;
    }

    if (focusColor == Color.TRANSPARENT) {
      focusColor = normalColor;
    }

    if (disabledColor == Color.TRANSPARENT) {
      disabledColor = normalColor;
    }

    errorColor = Color.rgb(211, 47, 47);
  }

  private int getThemeColor(int attribute) {
    TypedValue value = new TypedValue();

    boolean found = getContext().getTheme().resolveAttribute(attribute, value, true);

    if (!found) {
      return Color.TRANSPARENT;
    }

    if (value.type >= TypedValue.TYPE_FIRST_COLOR_INT && value.type <= TypedValue.TYPE_LAST_COLOR_INT) {
      return value.data;
    }

    if (value.resourceId != 0) {
      try {
        if (android.os.Build.VERSION.SDK_INT >= 23) {
          return getResources().getColor(value.resourceId, getContext().getTheme());

        } else {
          return getResources().getColor(value.resourceId);
        }

      } catch (Exception ignored) {
      }
    }

    return Color.TRANSPARENT;
  }

  // ==================================================
  // Initialize
  // ==================================================

  private void init(Context context) {
    // ----------------------------------------------
    // Field container
    // ----------------------------------------------

    fieldContainer = new OutlineField(context);

    LayoutParams fieldParams = new LayoutParams(ViewGroup.LayoutParams.MATCH_PARENT, dp(fieldHeight + 8));

    addView(fieldContainer, fieldParams);

    // ----------------------------------------------
    // EditText
    // ----------------------------------------------

    editText = new EditText(context);

    editText.setSingleLine(true);

    editText.setTextSize(TypedValue.COMPLEX_UNIT_SP, 16);

    editText.setGravity(Gravity.CENTER_VERTICAL);

    /*
     * Remove Android's default background.
     * The parent draws the outline.
     */
    editText.setBackgroundColor(Color.TRANSPARENT);

    /*
     * Leave space for the error icon.
     */
    editText.setPadding(dp(horizontalPadding), 0, dp(horizontalPadding + 28), 0);

    LayoutParams editParams = new LayoutParams(ViewGroup.LayoutParams.MATCH_PARENT, dp(fieldHeight));

    editParams.topMargin = dp(8);

    fieldContainer.addView(editText, editParams);

    // ----------------------------------------------
    // Floating label
    // ----------------------------------------------

    label = new TextView(context);

    label.setTextSize(TypedValue.COMPLEX_UNIT_SP, 12);

    label.setSingleLine(true);

    label.setGravity(Gravity.CENTER_VERTICAL);

    label.setTypeface(Typeface.DEFAULT);

    label.setBackgroundColor(Color.TRANSPARENT);

    label.setVisibility(VISIBLE);

    LayoutParams labelParams = new LayoutParams(ViewGroup.LayoutParams.WRAP_CONTENT, dp(20));

    labelParams.leftMargin = dp(10);

    labelParams.topMargin = 0;

    fieldContainer.addView(label, labelParams);

    // ----------------------------------------------
    // Error icon
    // ----------------------------------------------

    errorIcon = new ImageView(context);

    errorIcon.setScaleType(ImageView.ScaleType.CENTER);

    errorIcon.setClickable(false);
    errorIcon.setFocusable(false);

    errorIcon.setVisibility(GONE);

    LayoutParams iconParams = new LayoutParams(dp(28), dp(fieldHeight));

    iconParams.gravity = Gravity.RIGHT;

    iconParams.topMargin = dp(8);

    fieldContainer.addView(errorIcon, iconParams);

    // ----------------------------------------------
    // Error text
    // ----------------------------------------------

    errorText = new TextView(context);

    errorText.setTextSize(TypedValue.COMPLEX_UNIT_SP, 12);

    errorText.setTextColor(errorColor);

    errorText.setGravity(Gravity.TOP);

    errorText.setPadding(dp(horizontalPadding), dp(4), dp(horizontalPadding), 0);

    errorText.setVisibility(GONE);

    LayoutParams errorParams = new LayoutParams(ViewGroup.LayoutParams.MATCH_PARENT, ViewGroup.LayoutParams.WRAP_CONTENT);

    addView(errorText, errorParams);

    // ----------------------------------------------
    // Listeners
    // ----------------------------------------------

    editText.setOnFocusChangeListener(new OnFocusChangeListener() {
      @Override
      public void onFocusChange(View view, boolean focused) {
        updateState(true);
      }
    });

    editText.addTextChangedListener(new TextWatcher() {
      @Override
      public void beforeTextChanged(CharSequence s, int start, int count, int after) {
      }

      @Override
      public void onTextChanged(CharSequence s, int start, int before, int count) {
        updateState(true);
      }

      @Override
      public void afterTextChanged(Editable editable) {
      }
    });

    fieldContainer.setOnClickListener(new OnClickListener() {
      @Override
      public void onClick(View v) {
        if (editText.isEnabled()) {
          editText.requestFocus();

          editText.setSelection(editText.length());
        }
      }
    });

    updateState(false);
  }

  // ==================================================
  // State
  // ==================================================

  private void updateState(boolean animate) {
    boolean floating = editText.hasFocus() || editText.length() > 0;

    float target = floating ? 1f : 0f;

    animateLabel(target, animate);

    fieldContainer.invalidate();

    updateColors();
  }

  private void updateColors() {
    int borderColor;

    if (!enabled) {
      borderColor = disabledColor;

    } else if (hasError) {
      borderColor = errorColor;

    } else if (editText.hasFocus()) {
      borderColor = focusColor;

    } else {
      borderColor = normalColor;
    }

    fieldContainer.setBorderColor(borderColor);

    fieldContainer.setBorderWidth(editText.hasFocus() ? dp(2) : dp(1));

    int labelColor;

    if (!enabled) {
      labelColor = disabledColor;

    } else if (hasError) {
      labelColor = errorColor;

    } else if (editText.hasFocus()) {
      labelColor = focusColor;

    } else {
      labelColor = normalColor;
    }

    label.setTextColor(labelColor);
  }

  // ==================================================
  // Floating label animation
  // ==================================================

  private void animateLabel(float target, boolean animate) {
    if (labelAnimator != null) {
      labelAnimator.cancel();
    }

    if (!animate) {
      labelProgress = target;

      applyLabelProgress();

      return;
    }

    labelAnimator = ValueAnimator.ofFloat(labelProgress, target);

    labelAnimator.setDuration(180);

    labelAnimator.setInterpolator(new DecelerateInterpolator());

    labelAnimator.addUpdateListener(new ValueAnimator.AnimatorUpdateListener() {
      @Override
      public void onAnimationUpdate(ValueAnimator animation) {
        labelProgress = (Float) animation.getAnimatedValue();

        applyLabelProgress();
      }
    });

    labelAnimator.start();
  }

  private void applyLabelProgress() {
    /*
     * Center position:
     *
     * approximately inside the field.
     */
    float centerY = dp(28);

    /*
     * Floating position:
     *
     * centered over the top border.
     */
    float floatingY = dp(-1);

    float y = centerY + (floatingY - centerY) * labelProgress;

    label.setTranslationY(y);

    /*
     * Slight scale transition.
     */
    float scale = 1f - 0.02f * labelProgress;

    label.setScaleX(scale);

    label.setScaleY(scale);

    fieldContainer.invalidate();
  }

  // ==================================================
  // Hint
  // ==================================================

  public void setHint(String text) {
    if (text == null) {
      text = "";
    }

    hint = text;

    label.setText(text);

    /*
     * Hint remains visible while the label is
     * not floating.
     */
    /* editText.setHint(
         text
     );*/

    updateState(false);
  }

  public String getHint() {
    return hint;
  }

  // ==================================================
  // Text
  // ==================================================

  public void setText(String text) {
    if (text == null) {
      text = "";
    }

    editText.setText(text);

    editText.setSelection(editText.length());

    updateState(false);
  }

  public String getText() {
    return editText.getText().toString();
  }

  public EditText getEditText() {
    return editText;
  }

  // ==================================================
  // Error
  // ==================================================

  public void setError(String message) {
    if (message == null || message.length() == 0) {
      clearError();

      return;
    }

    hasError = true;

    errorText.setText(message);

    errorIcon.setImageDrawable(new ErrorIconDrawable(errorColor));

    updateColors();

    animateError(true);
  }

  public void clearError() {
    hasError = false;

    animateError(false);

    updateColors();
  }

  public boolean hasError() {
    return hasError;
  }

  private void animateError(boolean show) {
    if (errorAnimator != null) {
      errorAnimator.cancel();
    }

    if (show) {
      errorText.setVisibility(VISIBLE);

      errorIcon.setVisibility(VISIBLE);

      errorText.setAlpha(0f);

      errorIcon.setAlpha(0f);

      errorAnimator = ValueAnimator.ofFloat(0f, 1f);

    } else {
      errorAnimator = ValueAnimator.ofFloat(1f, 0f);
    }

    errorAnimator.setDuration(150);

    errorAnimator.setInterpolator(new DecelerateInterpolator());

    errorAnimator.addUpdateListener(new ValueAnimator.AnimatorUpdateListener() {
      @Override
      public void onAnimationUpdate(ValueAnimator animation) {
        float value = (Float) animation.getAnimatedValue();

        errorText.setAlpha(value);

        errorIcon.setAlpha(value);
      }
    });

    errorAnimator.addListener(new android.animation.AnimatorListenerAdapter() {
      @Override
      public void onAnimationEnd(android.animation.Animator animation) {
        if (!hasError) {
          errorText.setVisibility(GONE);

          errorIcon.setVisibility(GONE);
        }

        requestLayout();
      }
    });

    errorAnimator.start();

    requestLayout();
  }

  public void setErrorColor(int color) {
    errorColor = color;

    errorText.setTextColor(color);

    errorIcon.setImageDrawable(new ErrorIconDrawable(color));

    updateColors();
  }

  // ==================================================
  // Colors
  // ==================================================

  public void setFocusColor(int color) {
    focusColor = color;

    updateColors();
  }

  public void setNormalColor(int color) {
    normalColor = color;

    updateColors();
  }

  public void setDisabledColor(int color) {
    disabledColor = color;

    updateColors();
  }

  public void setFieldBackground(int color) {
    backgroundColor = color;

    fieldContainer.setBackgroundColor(color);

    updateColors();
  }

  // ==================================================
  // Radius
  // ==================================================

  public void setCornerRadius(float radius) {
    cornerRadius = radius;

    fieldContainer.invalidate();
  }

  // ==================================================
  // Enabled
  // ==================================================

  @Override
  public void setEnabled(boolean enabled) {
    super.setEnabled(enabled);

    this.enabled = enabled;

    if (editText != null) {
      editText.setEnabled(enabled);
    }

    if (!enabled) {
      editText.clearFocus();
    }

    updateColors();
  }

  @Override
  public boolean isEnabled() {
    return enabled;
  }

  // ==================================================
  // Input
  // ==================================================

  public void setInputType(int type) {
    editText.setInputType(type);
  }

  public int getInputType() {
    return editText.getInputType();
  }

  public void setSingleLine(boolean value) {
    editText.setSingleLine(value);
  }

  public void setTextSize(float size) {
    editText.setTextSize(TypedValue.COMPLEX_UNIT_SP, size);
  }

  // ==================================================
  // Measurement
  // ==================================================

  @Override
  protected void onMeasure(int widthMeasureSpec, int heightMeasureSpec) {
    int width = MeasureSpec.getSize(widthMeasureSpec);

    int fieldHeightTotal = dp(fieldHeight + 8);

    fieldContainer.measure(MeasureSpec.makeMeasureSpec(width, MeasureSpec.EXACTLY),
        MeasureSpec.makeMeasureSpec(fieldHeightTotal, MeasureSpec.EXACTLY));

    int desiredHeight = fieldHeightTotal;

    if (hasError && errorText.getVisibility() != GONE) {
      errorText.measure(MeasureSpec.makeMeasureSpec(width, MeasureSpec.EXACTLY),
          MeasureSpec.makeMeasureSpec(0, MeasureSpec.UNSPECIFIED));

      desiredHeight += errorText.getMeasuredHeight() + dp(4);
    }

    int mode = MeasureSpec.getMode(heightMeasureSpec);

    if (mode == MeasureSpec.EXACTLY) {
      desiredHeight = MeasureSpec.getSize(heightMeasureSpec);

    } else if (mode == MeasureSpec.AT_MOST) {
      desiredHeight = Math.min(desiredHeight, MeasureSpec.getSize(heightMeasureSpec));
    }

    setMeasuredDimension(width, desiredHeight);
  }

  // ==================================================
  // Layout
  // ==================================================

  @Override
  protected void onLayout(boolean changed, int left, int top, int right, int bottom) {
    fieldContainer.layout(0, 0, getMeasuredWidth(), dp(fieldHeight + 8));

    if (errorText.getVisibility() != GONE) {
      int topPosition = dp(fieldHeight + 12);

      errorText.layout(0, topPosition, getMeasuredWidth(), topPosition + errorText.getMeasuredHeight());
    }
  }

  // ==================================================
  // dp
  // ==================================================

  private int dp(float value) {
    return (int) (value * getResources().getDisplayMetrics().density + 0.5f);
  }

  // ==================================================
  // Custom outline field
  // ==================================================

  private class OutlineField extends FrameLayout {
    private Paint borderPaint = new Paint(Paint.ANTI_ALIAS_FLAG);

    private int borderColor = normalColor;

    private float borderWidth = dp(1);

    public OutlineField(Context context) {
      super(context);

      setWillNotDraw(false);

      setBackgroundColor(backgroundColor);

      borderPaint.setStyle(Paint.Style.STROKE);

      borderPaint.setStrokeCap(Paint.Cap.ROUND);
    }

    public void setBorderColor(int color) {
      borderColor = color;

      invalidate();
    }

    public void setBorderWidth(float width) {
      borderWidth = width;

      invalidate();
    }

    @Override
    protected void onDraw(Canvas canvas) {
      super.onDraw(canvas);

      borderPaint.setColor(borderColor);

      borderPaint.setStrokeWidth(borderWidth);

      borderPaint.setStyle(Paint.Style.STROKE);

      float half = borderWidth / 2f;

      RectF rect = new RectF(half, dp(8) + half, getWidth() - half, dp(fieldHeight) + dp(8) - half);

      /*
       * --------------------------------------------------
       * Normal outline
       * --------------------------------------------------
       */

      if (labelProgress < 0.01f) {
        canvas.drawRoundRect(rect, dp(cornerRadius), dp(cornerRadius), borderPaint);

        return;
      }

      /*
       * --------------------------------------------------
       * Floating label notch
       * --------------------------------------------------
       *
       * Draw the outline as four sections,
       * leaving a gap where the label sits.
       */

      float labelLeft = dp(10);

      float labelWidth = label.getMeasuredWidth();

      float gapLeft = labelLeft - dp(3);

      float gapRight = labelLeft + labelWidth + dp(3);

      /*
       * Top-left section.
       */

      Path path = new Path();

      path.moveTo(rect.left + dp(cornerRadius), rect.top);

      path.lineTo(gapLeft, rect.top);

      canvas.drawPath(path, borderPaint);

      /*
       * Top-right section.
       */

      path.reset();

      path.moveTo(gapRight, rect.top);

      path.lineTo(rect.right - dp(cornerRadius), rect.top);

      canvas.drawPath(path, borderPaint);

      /*
       * Remaining rounded rectangle.
       */

      Path remaining = new Path();

      RectF remainingRect = new RectF(rect.left, rect.top, rect.right, rect.bottom);

      /*
       * Draw left side.
       */
      canvas.drawLine(rect.left, rect.top + dp(cornerRadius), rect.left,
          rect.bottom - dp(cornerRadius), borderPaint);

      /*
       * Draw bottom.
       */
      canvas.drawLine(rect.left + dp(cornerRadius), rect.bottom, rect.right - dp(cornerRadius),
          rect.bottom, borderPaint);

      /*
       * Draw right.
       */
      canvas.drawLine(rect.right, rect.top + dp(cornerRadius), rect.right,
          rect.bottom - dp(cornerRadius), borderPaint);

      /*
       * Rounded corners.
       */

      canvas.drawArc(new RectF(rect.left, rect.top, rect.left + dp(cornerRadius * 2),
                         rect.top + dp(cornerRadius * 2)),
          180, 90, false, borderPaint);

      canvas.drawArc(new RectF(rect.right - dp(cornerRadius * 2), rect.top, rect.right,
                         rect.top + dp(cornerRadius * 2)),
          270, 90, false, borderPaint);

      canvas.drawArc(new RectF(rect.left, rect.bottom - dp(cornerRadius * 2),
                         rect.left + dp(cornerRadius * 2), rect.bottom),
          90, 90, false, borderPaint);

      canvas.drawArc(new RectF(rect.right - dp(cornerRadius * 2),
                         rect.bottom - dp(cornerRadius * 2), rect.right, rect.bottom),
          0, 90, false, borderPaint);
    }
  }

  // ==================================================
  // Error icon
  // ==================================================

  private static class ErrorIconDrawable extends Drawable {
    private final Paint paint = new Paint(Paint.ANTI_ALIAS_FLAG);

    private final Path path = new Path();

    private final int color;

    ErrorIconDrawable(int color) {
      this.color = color;

      paint.setStrokeCap(Paint.Cap.ROUND);
    }

    @Override
    public void draw(Canvas canvas) {
      RectF bounds = new RectF(getBounds());

      float cx = bounds.centerX();

      float cy = bounds.centerY();

      float size = Math.min(bounds.width(), bounds.height()) * 0.55f;

      float radius = size * 0.5f;

      /*
       * Triangle.
       */

      path.reset();

      path.moveTo(cx, cy - radius);

      path.lineTo(cx - radius, cy + radius);

      path.lineTo(cx + radius, cy + radius);

      path.close();

      paint.setStyle(Paint.Style.FILL);

      paint.setColor(color);

      canvas.drawPath(path, paint);

      /*
       * Exclamation mark.
       */

      paint.setColor(Color.WHITE);

      paint.setStrokeWidth(size * 0.10f);

      paint.setStyle(Paint.Style.STROKE);

      canvas.drawLine(cx, cy - size * 0.20f, cx, cy + size * 0.12f, paint);

      paint.setStyle(Paint.Style.FILL);

      canvas.drawCircle(cx, cy + size * 0.28f, size * 0.055f, paint);
    }

    @Override
    public void setAlpha(int alpha) {
      paint.setAlpha(alpha);
    }

    @Override
    public void setColorFilter(android.graphics.ColorFilter filter) {
      paint.setColorFilter(filter);
    }

    @Override
    public int getOpacity() {
      return android.graphics.PixelFormat.TRANSLUCENT;
    }
  }
}
