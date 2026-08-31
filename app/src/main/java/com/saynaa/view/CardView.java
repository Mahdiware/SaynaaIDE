package com.saynaa.view;

import android.content.Context;
import android.content.res.ColorStateList;
import android.graphics.Canvas;
import android.graphics.Color;
import android.graphics.ColorFilter;
import android.graphics.LinearGradient;
import android.graphics.Outline;
import android.graphics.Paint;
import android.graphics.Path;
import android.graphics.PixelFormat;
import android.graphics.PorterDuff;
import android.graphics.PorterDuffColorFilter;
import android.graphics.RadialGradient;
import android.graphics.Rect;
import android.graphics.RectF;
import android.graphics.Shader;
import android.graphics.drawable.Drawable;
import android.os.Build;
import android.os.Parcel;
import android.os.Parcelable;
import android.util.AttributeSet;
import android.util.TypedValue;
import android.view.View;
import android.widget.FrameLayout;

/**
 * Standalone CardView-style container for the Saynaa project.
 *
 * API compatible with the important public AndroidX CardView API while
 * deliberately avoiding any AndroidX dependency.
 *
 * Supported:
 * - cardBackgroundColor
 * - cardCornerRadius
 * - cardElevation
 * - cardMaxElevation
 * - cardUseCompatPadding
 * - cardPreventCornerOverlap
 * - contentPadding and all four individual content paddings
 * - ColorStateList backgrounds
 * - API 21+ native elevation and outline
 * - custom software shadow for all API levels
 * - drawable state changes
 * - minimum dimensions
 * - RTL-aware content padding storage
 * - save/restore of card configuration
 */
public class CardView extends FrameLayout {

    private static final double COS_45 = Math.cos(Math.toRadians(45.0));
    private static final float SHADOW_MULTIPLIER = 1.5f;

    private final Rect contentPadding = new Rect();
    private final Rect shadowPadding = new Rect();

    private ColorStateList backgroundColor;
    private float radius;
    private float elevation;
    private float maxElevation;

    private boolean useCompatPadding;
    private boolean preventCornerOverlap = true;

    private boolean compatShadowEnabled = true;
    private boolean softwareShadowForced;

    private int userMinWidth;
    private int userMinHeight;

    private CardDrawable cardDrawable;

    public CardView(Context context) {
        super(context);
        initialize(context, null, 0);
    }

    public CardView(Context context, AttributeSet attrs) {
        super(context, attrs);
        initialize(context, attrs, 0);
    }

    public CardView(
            Context context,
            AttributeSet attrs,
            int defStyleAttr) {
        super(context, attrs, defStyleAttr);
        initialize(context, attrs, defStyleAttr);
    }

    private void initialize(
            Context context,
            AttributeSet attrs,
            int defStyleAttr) {

        backgroundColor = ColorStateList.valueOf(Color.WHITE);

        radius = dp(2);
        elevation = dp(2);
        maxElevation = elevation;

        setClipToPadding(false);

        if (Build.VERSION.SDK_INT >= 21) {
            setClipToOutline(false);
        }

        if (attrs != null) {
            TypedValue value = new TypedValue();

            if (attrs.getAttributeResourceValue(
                    "http://schemas.android.com/apk/res/android",
                    "background",
                    0) != 0) {
                // Card background is deliberately controlled by the Card API.
            }

            int minW = attrs.getAttributeResourceValue(
                    "http://schemas.android.com/apk/res/android",
                    "minWidth",
                    0);

            int minH = attrs.getAttributeResourceValue(
                    "http://schemas.android.com/apk/res/android",
                    "minHeight",
                    0);

            if (minW != 0 && context.getTheme().resolveAttribute(
                    minW, value, true)) {
                userMinWidth = value.data;
            }

            if (minH != 0 && context.getTheme().resolveAttribute(
                    minH, value, true)) {
                userMinHeight = value.data;
            }
        }

        setMinimumWidth(userMinWidth);
        setMinimumHeight(userMinHeight);

        rebuildDrawable();
    }

    /* --------------------------------------------------------------------- */
    /* Public CardView API                                                   */
    /* --------------------------------------------------------------------- */

    public void setCardBackgroundColor(int color) {
        setCardBackgroundColor(ColorStateList.valueOf(color));
    }

    public void setCardBackgroundColor(ColorStateList color) {
        if (color == null) {
            color = ColorStateList.valueOf(Color.TRANSPARENT);
        }

        backgroundColor = color;

        if (cardDrawable != null) {
            cardDrawable.setColor(color);
        }

        refreshDrawableState();
        invalidate();
    }

    public ColorStateList getCardBackgroundColor() {
        return backgroundColor;
    }

    public void setRadius(float radius) {
        if (radius < 0) {
            radius = 0;
        }

        if (this.radius == radius) {
            return;
        }

        this.radius = radius;

        if (cardDrawable != null) {
            cardDrawable.setCornerRadius(radius);
        }

        updateCardPadding();
        invalidateOutlineCompat();
    }

    public float getRadius() {
        return radius;
    }

public void setCardElevation(float elevation) {
    elevation = Math.max(0f, elevation);

    this.elevation = elevation;

    if (maxElevation < elevation) {
        maxElevation = elevation;
    }

    if (Build.VERSION.SDK_INT >= 21 && !softwareShadowForced) {
        super.setElevation(elevation);
    }

    if (cardDrawable != null) {
        cardDrawable.setShadowSize(
            elevation,
            maxElevation
        );
    }

    updateCardPadding();
    invalidate();
}

    public float getCardElevation() {
        return elevation;
    }

    public void setMaxCardElevation(float maxElevation) {
        if (maxElevation < 0) {
            maxElevation = 0;
        }

        if (maxElevation < elevation) {
            maxElevation = elevation;
        }

        this.maxElevation = maxElevation;

        if (cardDrawable != null) {
            cardDrawable.setShadowSize(elevation, maxElevation);
        }

        updateCardPadding();
    }

    public float getMaxCardElevation() {
        return maxElevation;
    }

    public void setUseCompatPadding(boolean useCompatPadding) {
        if (this.useCompatPadding == useCompatPadding) {
            return;
        }

        this.useCompatPadding = useCompatPadding;
        updateCardPadding();
    }

    public boolean getUseCompatPadding() {
        return useCompatPadding;
    }

    public boolean isUseCompatPadding() {
        return useCompatPadding;
    }

    public void setPreventCornerOverlap(boolean preventCornerOverlap) {
        if (this.preventCornerOverlap == preventCornerOverlap) {
            return;
        }

        this.preventCornerOverlap = preventCornerOverlap;

        if (cardDrawable != null) {
            cardDrawable.setAddPaddingForCorners(preventCornerOverlap);
        }

        updateCardPadding();
    }

    public boolean getPreventCornerOverlap() {
        return preventCornerOverlap;
    }

    public boolean isPreventCornerOverlap() {
        return preventCornerOverlap;
    }

    public void setContentPadding(
            int left,
            int top,
            int right,
            int bottom) {

        contentPadding.set(left, top, right, bottom);
        updateCardPadding();
    }

    public int getContentPaddingLeft() {
        return contentPadding.left;
    }

    public int getContentPaddingTop() {
        return contentPadding.top;
    }

    public int getContentPaddingRight() {
        return contentPadding.right;
    }

    public int getContentPaddingBottom() {
        return contentPadding.bottom;
    }

    public void setContentPaddingLeft(int left) {
        contentPadding.left = left;
        updateCardPadding();
    }

    public void setContentPaddingTop(int top) {
        contentPadding.top = top;
        updateCardPadding();
    }

    public void setContentPaddingRight(int right) {
        contentPadding.right = right;
        updateCardPadding();
    }

    public void setContentPaddingBottom(int bottom) {
        contentPadding.bottom = bottom;
        updateCardPadding();
    }

    public Rect getContentPadding() {
        return new Rect(contentPadding);
    }

    public Rect getShadowPadding() {
        return new Rect(shadowPadding);
    }

    /**
     * Forces the software shadow renderer. Normally API 21+ uses native
     * elevation, matching AndroidX's platform strategy.
     */
    public void setSoftwareShadowEnabled(boolean enabled) {
        softwareShadowForced = enabled;

        if (Build.VERSION.SDK_INT >= 21) {
            super.setElevation(enabled ? 0 : elevation);
        }

        if (cardDrawable != null) {
            cardDrawable.setShadowEnabled(enabled || Build.VERSION.SDK_INT < 21);
        }

        updateCardPadding();
        invalidate();
    }

    public boolean isSoftwareShadowEnabled() {
        return softwareShadowForced || Build.VERSION.SDK_INT < 21;
    }

    /**
     * Allows applications to disable the custom shadow entirely.
     * This is useful for very low-end devices or special rendering cases.
     */
    public void setCompatShadowEnabled(boolean enabled) {
        compatShadowEnabled = enabled;

        if (cardDrawable != null) {
            cardDrawable.setShadowEnabled(
                    enabled && (softwareShadowForced || Build.VERSION.SDK_INT < 21));
        }

        updateCardPadding();
        invalidate();
    }

    public boolean isCompatShadowEnabled() {
        return compatShadowEnabled;
    }

    /* --------------------------------------------------------------------- */
    /* Padding                                                               */
    /* --------------------------------------------------------------------- */

    @Override
    public void setPadding(
            int left,
            int top,
            int right,
            int bottom) {

        // CardView owns actual padding. Use setContentPadding() for content.
        setContentPadding(left, top, right, bottom);
    }

    @Override
    public void setPaddingRelative(
            int start,
            int top,
            int end,
            int bottom) {

        if (getLayoutDirection() == View.LAYOUT_DIRECTION_RTL) {
            setContentPadding(end, top, start, bottom);
        } else {
            setContentPadding(start, top, end, bottom);
        }
    }

    @Override
    public int getPaddingLeft() {
        return contentPadding.left + shadowPadding.left;
    }

    @Override
    public int getPaddingTop() {
        return contentPadding.top + shadowPadding.top;
    }

    @Override
    public int getPaddingRight() {
        return contentPadding.right + shadowPadding.right;
    }

    @Override
    public int getPaddingBottom() {
        return contentPadding.bottom + shadowPadding.bottom;
    }

    @Override
    public int getPaddingStart() {
        return getPaddingLeft();
    }

    @Override
    public int getPaddingEnd() {
        return getPaddingRight();
    }

    /* --------------------------------------------------------------------- */
    /* Minimum dimensions                                                    */
    /* --------------------------------------------------------------------- */

    @Override
    public void setMinimumWidth(int minWidth) {
        userMinWidth = Math.max(0, minWidth);
        super.setMinimumWidth(userMinWidth);
    }

    @Override
    public void setMinimumHeight(int minHeight) {
        userMinHeight = Math.max(0, minHeight);
        super.setMinimumHeight(userMinHeight);
    }

    public int getCardMinimumWidth() {
        int drawableWidth = cardDrawable == null
                ? 0
                : (int) Math.ceil(cardDrawable.getMinWidth());

        return Math.max(userMinWidth, drawableWidth);
    }

    public int getCardMinimumHeight() {
        int drawableHeight = cardDrawable == null
                ? 0
                : (int) Math.ceil(cardDrawable.getMinHeight());

        return Math.max(userMinHeight, drawableHeight);
    }

    /* --------------------------------------------------------------------- */
    /* Drawable/state                                                        */
    /* --------------------------------------------------------------------- */

    private void rebuildDrawable() {
        boolean software =
                softwareShadowForced || Build.VERSION.SDK_INT < 21;

        cardDrawable = new CardDrawable(
                backgroundColor,
                radius,
                elevation,
                maxElevation,
                preventCornerOverlap,
                software && compatShadowEnabled);

        setBackground(cardDrawable);

        if (Build.VERSION.SDK_INT >= 21) {
            super.setElevation(softwareShadowForced ? 0 : elevation);

            setOutlineProvider(new android.view.ViewOutlineProvider() {
                @Override
                public void getOutline(View view, Outline outline) {
                    if (cardDrawable != null) {
                        cardDrawable.getOutline(outline);
                    }
                }
            });
        }

        updateCardPadding();
    }

    private void updateCardPadding() {
        if (cardDrawable == null) {
            return;
        }

        Rect p = new Rect();

        boolean nativeElevation =
                Build.VERSION.SDK_INT >= 21 &&
                !softwareShadowForced;

        if (nativeElevation && !useCompatPadding) {
            p.set(0, 0, 0, 0);
        } else {
            cardDrawable.getMaxShadowAndCornerPadding(p);
        }

        shadowPadding.set(p);

        super.setPadding(
                contentPadding.left + shadowPadding.left,
                contentPadding.top + shadowPadding.top,
                contentPadding.right + shadowPadding.right,
                contentPadding.bottom + shadowPadding.bottom);

        requestLayout();
        invalidateOutlineCompat();
    }

    private void invalidateOutlineCompat() {
        if (Build.VERSION.SDK_INT >= 21) {
            invalidateOutline();
        }
    }

    @Override
    protected void drawableStateChanged() {
        super.drawableStateChanged();

        if (cardDrawable != null && cardDrawable.isStateful()) {
            if (cardDrawable.setState(getDrawableState())) {
                invalidate();
            }
        }
    }

    @Override
    protected void onSizeChanged(
            int w,
            int h,
            int oldw,
            int oldh) {

        super.onSizeChanged(w, h, oldw, oldh);

        if (cardDrawable != null) {
            cardDrawable.setBounds(0, 0, w, h);
        }
    }

    @Override
    protected void onMeasure(
            int widthMeasureSpec,
            int heightMeasureSpec) {

        int minWidth = getCardMinimumWidth();
        int minHeight = getCardMinimumHeight();

        super.onMeasure(widthMeasureSpec, heightMeasureSpec);

        int measuredWidth = getMeasuredWidth();
        int measuredHeight = getMeasuredHeight();

        int requiredWidth =
                Math.max(measuredWidth, minWidth);

        int requiredHeight =
                Math.max(measuredHeight, minHeight);

        if (requiredWidth != measuredWidth ||
                requiredHeight != measuredHeight) {

            setMeasuredDimension(
                    resolveSize(requiredWidth, widthMeasureSpec),
                    resolveSize(requiredHeight, heightMeasureSpec));
        }
    }

    /* --------------------------------------------------------------------- */
    /* State saving                                                          */
    /* --------------------------------------------------------------------- */

    @Override
    protected Parcelable onSaveInstanceState() {
        Parcelable superState = super.onSaveInstanceState();

        SavedState state = new SavedState(superState);

        state.backgroundColor =
                backgroundColor == null
                        ? Color.TRANSPARENT
                        : backgroundColor.getDefaultColor();

        state.radius = radius;
        state.elevation = elevation;
        state.maxElevation = maxElevation;
        state.useCompatPadding = useCompatPadding;
        state.preventCornerOverlap = preventCornerOverlap;

        state.contentLeft = contentPadding.left;
        state.contentTop = contentPadding.top;
        state.contentRight = contentPadding.right;
        state.contentBottom = contentPadding.bottom;

        return state;
    }

    @Override
    protected void onRestoreInstanceState(Parcelable state) {
        if (!(state instanceof SavedState)) {
            super.onRestoreInstanceState(state);
            return;
        }

        SavedState saved = (SavedState) state;

        super.onRestoreInstanceState(saved.getSuperState());

        setCardBackgroundColor(saved.backgroundColor);
        setRadius(saved.radius);
        setCardElevation(saved.elevation);
        setMaxCardElevation(saved.maxElevation);
        setUseCompatPadding(saved.useCompatPadding);
        setPreventCornerOverlap(saved.preventCornerOverlap);

        setContentPadding(
                saved.contentLeft,
                saved.contentTop,
                saved.contentRight,
                saved.contentBottom);
    }

    public static class SavedState extends BaseSavedState {

        int backgroundColor;
        float radius;
        float elevation;
        float maxElevation;

        boolean useCompatPadding;
        boolean preventCornerOverlap;

        int contentLeft;
        int contentTop;
        int contentRight;
        int contentBottom;

        SavedState(Parcelable superState) {
            super(superState);
        }

        SavedState(Parcel in) {
            super(in);

            backgroundColor = in.readInt();
            radius = in.readFloat();
            elevation = in.readFloat();
            maxElevation = in.readFloat();

            useCompatPadding = in.readInt() != 0;
            preventCornerOverlap = in.readInt() != 0;

            contentLeft = in.readInt();
            contentTop = in.readInt();
            contentRight = in.readInt();
            contentBottom = in.readInt();
        }

        @Override
        public void writeToParcel(
                Parcel out,
                int flags) {

            super.writeToParcel(out, flags);

            out.writeInt(backgroundColor);
            out.writeFloat(radius);
            out.writeFloat(elevation);
            out.writeFloat(maxElevation);

            out.writeInt(useCompatPadding ? 1 : 0);
            out.writeInt(preventCornerOverlap ? 1 : 0);

            out.writeInt(contentLeft);
            out.writeInt(contentTop);
            out.writeInt(contentRight);
            out.writeInt(contentBottom);
        }

        public static final Parcelable.Creator<SavedState> CREATOR =
                new Parcelable.Creator<SavedState>() {

                    @Override
                    public SavedState createFromParcel(Parcel in) {
                        return new SavedState(in);
                    }

                    @Override
                    public SavedState[] newArray(int size) {
                        return new SavedState[size];
                    }
                };
    }

    /* --------------------------------------------------------------------- */
    /* Helpers                                                               */
    /* --------------------------------------------------------------------- */

    public int dpInt(float value) {
        return Math.round(
                value * getResources()
                        .getDisplayMetrics()
                        .density);
    }

    /* --------------------------------------------------------------------- */
    /* Drawable implementation                                               */
    /* --------------------------------------------------------------------- */

    private static final class CardDrawable extends Drawable {

        private static final double COS_45 =
                Math.cos(Math.toRadians(45.0));

        private static final float SHADOW_MULTIPLIER = 1.5f;

        private final Paint paint =
                new Paint(Paint.ANTI_ALIAS_FLAG |
                        Paint.DITHER_FLAG);

        private final Paint shadowPaint =
                new Paint(Paint.ANTI_ALIAS_FLAG |
                        Paint.DITHER_FLAG);

        private final Paint cornerShadowPaint =
                new Paint(Paint.ANTI_ALIAS_FLAG |
                        Paint.DITHER_FLAG);

        private final RectF boundsF =
                new RectF();

        private final RectF contentBounds =
                new RectF();

        private final Path cornerPath =
                new Path();

        private ColorStateList color;

        private ColorFilter colorFilter;
        private PorterDuffColorFilter tintFilter;

        private ColorStateList tint;
        private PorterDuff.Mode tintMode =
                PorterDuff.Mode.SRC_IN;

        private boolean clearColorFilter;

        private float cornerRadius;
        private float shadowSize;
        private float maxShadowSize;

        private boolean addPaddingForCorners = true;
        private boolean insetForPadding = true;
        private boolean shadowEnabled;

        private float rawShadowSize;
        private float rawMaxShadowSize;

        private boolean dirty = true;

        CardDrawable(
                ColorStateList color,
                float radius,
                float elevation,
                float maxElevation,
                boolean preventCorners,
                boolean shadowEnabled) {

            this.color = color == null
                    ? ColorStateList.valueOf(Color.TRANSPARENT)
                    : color;

            this.cornerRadius = Math.max(0, radius);
            this.addPaddingForCorners = preventCorners;

            this.shadowEnabled = shadowEnabled;

            setShadowSize(elevation, maxElevation);
        }

        @Override
public void draw(Canvas canvas) {
    Rect bounds = getBounds();

    if (bounds.width() <= 0 || bounds.height() <= 0) {
        return;
    }

    int color = this.color.getColorForState(
        getState(),
        this.color.getDefaultColor()
    );

    // Completely transparent card with no shadow.
    if (color == Color.TRANSPARENT &&
        (!shadowEnabled || shadowSize <= 0f)) {
        return;
    }

    // Never render a shadow when elevation is zero.
    if (shadowEnabled && shadowSize > 0f) {
        buildShadow();
        drawShadow(canvas);
    }

    paint.setStyle(Paint.Style.FILL);
    paint.setColor(color);

    ColorFilter filter =
        colorFilter != null
            ? colorFilter
            : tintFilter;

    paint.setColorFilter(filter);

    float left = bounds.left;
    float top = bounds.top;
    float right = bounds.right;
    float bottom = bounds.bottom;

    if (shadowEnabled && shadowSize > 0f) {
        float horizontalInset = shadowSize;
        float verticalInset = shadowSize * SHADOW_MULTIPLIER;

        left += horizontalInset;
        top += verticalInset;
        right -= horizontalInset;
        bottom -= verticalInset;
    }

    if (right > left && bottom > top) {
        canvas.drawRoundRect(
            left,
            top,
            right,
            bottom,
            cornerRadius,
            cornerRadius,
            paint
        );
    }

    paint.setColorFilter(null);
}

        private void drawShadow(Canvas canvas) {
            Rect bounds = getBounds();

            float shadowX = shadowSize;
            float shadowY =
                    shadowSize * SHADOW_MULTIPLIER;

            float left = bounds.left + shadowX;
            float top = bounds.top + shadowY;
            float right = bounds.right - shadowX;
            float bottom =
                    bounds.bottom - shadowY;

            float radius = cornerRadius;

            if (right <= left || bottom <= top) {
                return;
            }

            buildCornerPath(radius);

            int save = canvas.save();

            canvas.translate(
                    left + radius,
                    top + radius);

            canvas.drawPath(
                    cornerPath,
                    cornerShadowPaint);

            canvas.translate(
                    right - left - radius * 2,
                    0);

            canvas.rotate(90);
            canvas.drawPath(
                    cornerPath,
                    cornerShadowPaint);

            canvas.translate(
                    bottom - top - radius * 2,
                    0);

            canvas.rotate(90);
            canvas.drawPath(
                    cornerPath,
                    cornerShadowPaint);

            canvas.translate(
                    right - left - radius * 2,
                    0);

            canvas.rotate(90);
            canvas.drawPath(
                    cornerPath,
                    cornerShadowPaint);

            canvas.restoreToCount(save);

            float innerLeft = left + radius;
            float innerRight = right - radius;

            if (innerRight > innerLeft) {
                LinearGradient horizontal =
                        new LinearGradient(
                                innerLeft,
                                top,
                                innerLeft,
                                top + shadowY,
                                new int[] {
                                    0x00000000,
                                    0x55000000,
                                    0x00000000
                                },
                                new float[] {
                                    0f,
                                    0.5f,
                                    1f
                                },
                                Shader.TileMode.CLAMP);

                shadowPaint.setShader(horizontal);

                canvas.drawRect(
                        innerLeft,
                        top - shadowY,
                        innerRight,
                        top,
                        shadowPaint);

                canvas.drawRect(
                        innerLeft,
                        bottom,
                        innerRight,
                        bottom + shadowY,
                        shadowPaint);

                shadowPaint.setShader(null);
            }

            float innerTop = top + radius;
            float innerBottom = bottom - radius;

            if (innerBottom > innerTop) {
                LinearGradient vertical =
                        new LinearGradient(
                                left,
                                innerTop,
                                left + shadowX,
                                innerTop,
                                new int[] {
                                    0x00000000,
                                    0x44000000,
                                    0x00000000
                                },
                                new float[] {
                                    0f,
                                    0.5f,
                                    1f
                                },
                                Shader.TileMode.CLAMP);

                shadowPaint.setShader(vertical);

                canvas.drawRect(
                        left - shadowX,
                        innerTop,
                        left,
                        innerBottom,
                        shadowPaint);

                canvas.drawRect(
                        right,
                        innerTop,
                        right + shadowX,
                        innerBottom,
                        shadowPaint);

                shadowPaint.setShader(null);
            }
        }

        private void buildCornerPath(float radius) {
            cornerPath.reset();

            float shadow =
                    Math.max(1f, shadowSize);

            RectF outer =
                    new RectF(
                            -radius,
                            -radius,
                            radius,
                            radius);

            RectF inner =
                    new RectF(
                            -radius + shadow,
                            -radius + shadow,
                            radius - shadow,
                            radius - shadow);

            cornerPath.setFillType(
                    Path.FillType.EVEN_ODD);

            cornerPath.addRoundRect(
                    outer,
                    radius,
                    radius,
                    Path.Direction.CW);

            if (inner.width() > 0 &&
                    inner.height() > 0) {
                cornerPath.addRoundRect(
                        inner,
                        Math.max(
                                0,
                                radius - shadow),
                        Math.max(
                                0,
                                radius - shadow),
                        Path.Direction.CCW);
            }

            float gradientRadius =
                    radius + shadow;

            cornerShadowPaint.setShader(
                    new RadialGradient(
                            0,
                            0,
                            gradientRadius,
                            new int[] {
                                0x55000000,
                                0x33000000,
                                0x00000000
                            },
                            new float[] {
                                0f,
                                radius /
                                        Math.max(
                                                gradientRadius,
                                                1f),
                                1f
                            },
                            Shader.TileMode.CLAMP));
        }

        private void buildShadow() {
            if (!dirty) {
                return;
            }

            dirty = false;

            shadowPaint.setStyle(Paint.Style.FILL);
            cornerShadowPaint.setStyle(Paint.Style.FILL);

            shadowPaint.setColor(0x55000000);
        }

        void setColor(ColorStateList color) {
            if (color == null) {
                color = ColorStateList.valueOf(Color.TRANSPARENT);
            }

            this.color = color;
            invalidateSelf();
        }

        ColorStateList getColor() {
            return color;
        }

        void setCornerRadius(float radius) {
            radius = Math.max(0, radius);

            if (cornerRadius == radius) {
                return;
            }

            cornerRadius = radius;
            dirty = true;

            invalidateSelf();
        }

        float getCornerRadius() {
            return cornerRadius;
        }

void setShadowSize(float shadow, float maxShadow) {
    shadow = Math.max(0f, shadow);
    maxShadow = Math.max(0f, maxShadow);

    if (maxShadow < shadow) {
        maxShadow = shadow;
    }

    rawShadowSize = shadow;
    rawMaxShadowSize = maxShadow;

    shadowSize = roundEven(shadow);
    maxShadowSize = roundEven(maxShadow);

    shadowEnabled = shadowSize > 0f;

    dirty = true;
    invalidateSelf();
}

        float getShadowSize() {
            return rawShadowSize;
        }

        float getMaxShadowSize() {
            return rawMaxShadowSize;
        }

        void setAddPaddingForCorners(boolean enabled) {
            if (addPaddingForCorners == enabled) {
                return;
            }

            addPaddingForCorners = enabled;
            dirty = true;

            invalidateSelf();
        }

        boolean getAddPaddingForCorners() {
            return addPaddingForCorners;
        }

        void setShadowEnabled(boolean enabled) {
            if (shadowEnabled == enabled) {
                return;
            }

            shadowEnabled = enabled;
            dirty = true;

            invalidateSelf();
        }

        boolean isShadowEnabled() {
            return shadowEnabled;
        }

        float getMinWidth() {
            float horizontal =
                    maxShadowSize * 2f;

            if (addPaddingForCorners) {
                horizontal +=
                        (1f - (float) COS_45) *
                                cornerRadius * 2f;
            }

            return horizontal;
        }

        float getMinHeight() {
            float vertical =
                    maxShadowSize *
                            SHADOW_MULTIPLIER * 2f;

            if (addPaddingForCorners) {
                vertical +=
                        (1f - (float) COS_45) *
                                cornerRadius * 2f;
            }

            return vertical;
        }

        void getMaxShadowAndCornerPadding(Rect out) {
            float h =
                    calculateHorizontalPadding(
                            maxShadowSize,
                            cornerRadius,
                            addPaddingForCorners);

            float v =
                    calculateVerticalPadding(
                            maxShadowSize,
                            cornerRadius,
                            addPaddingForCorners);

            out.set(
                    (int) Math.ceil(h),
                    (int) Math.ceil(v),
                    (int) Math.ceil(h),
                    (int) Math.ceil(v));
        }

        static float calculateHorizontalPadding(
                float shadowSize,
                float radius,
                boolean addPaddingForCorners) {

            if (addPaddingForCorners) {
                return shadowSize +
                        (1f - (float) COS_45) *
                                radius;
            }

            return shadowSize;
        }

        static float calculateVerticalPadding(
                float shadowSize,
                float radius,
                boolean addPaddingForCorners) {

            if (addPaddingForCorners) {
                return shadowSize *
                        SHADOW_MULTIPLIER +
                        (1f - (float) COS_45) *
                                radius;
            }

            return shadowSize *
                    SHADOW_MULTIPLIER;
        }

        public void setTintList(ColorStateList tint) {
            this.tint = tint;
            updateTintFilter();
        }

        ColorStateList getTintList() {
            return tint;
        }

        public void setTintMode(PorterDuff.Mode mode) {
            tintMode = mode;
            updateTintFilter();
        }

        PorterDuff.Mode getTintMode() {
            return tintMode;
        }

        private void updateTintFilter() {
            if (tint == null || tintMode == null) {
                tintFilter = null;
                return;
            }

            int c = tint.getColorForState(
                    getState(),
                    Color.TRANSPARENT);

            tintFilter =
                    new PorterDuffColorFilter(
                            c,
                            tintMode);

            invalidateSelf();
        }

        @Override
        protected void onBoundsChange(Rect bounds) {
            super.onBoundsChange(bounds);

            boundsF.set(bounds);
            contentBounds.set(bounds);

            dirty = true;
        }

        @Override
        public boolean isStateful() {
            return color != null &&
                    color.isStateful() ||
                    tint != null &&
                    tint.isStateful();
        }

        @Override
        protected boolean onStateChange(int[] state) {
            boolean changed = false;

            if (color != null) {
                int newColor =
                        color.getColorForState(
                                state,
                                color.getDefaultColor());

                int oldColor =
                        paint.getColor();

                if (newColor != oldColor) {
                    paint.setColor(newColor);
                    changed = true;
                }
            }

            if (tint != null) {
                updateTintFilter();
                changed = true;
            }

            return changed;
        }

        @Override
        public void setAlpha(int alpha) {
            paint.setAlpha(alpha);
            invalidateSelf();
        }

        @Override
        public void setColorFilter(ColorFilter filter) {
            colorFilter = filter;
            invalidateSelf();
        }

        @Override
        public int getOpacity() {
            return PixelFormat.TRANSLUCENT;
        }

        @Override
        public int getIntrinsicWidth() {
            return (int) Math.ceil(getMinWidth());
        }

        @Override
        public int getIntrinsicHeight() {
            return (int) Math.ceil(getMinHeight());
        }

        @Override
        public boolean getPadding(Rect padding) {
            getMaxShadowAndCornerPadding(padding);
            return true;
        }

        @Override
        public void getOutline(Outline outline) {
            if (Build.VERSION.SDK_INT < 21) {
                return;
            }

            Rect bounds = getBounds();

            if (bounds.width() <= 0 ||
                    bounds.height() <= 0) {
                return;
            }

            outline.setRoundRect(
                    bounds,
                    cornerRadius);
        }

        private static float roundEven(float value) {
            int i = Math.round(value);

            if ((i & 1) == 1) {
                i--;
            }

            return Math.max(0, i);
        }
    }

    /* --------------------------------------------------------------------- */
    /* Utility methods                                                       */
    /* --------------------------------------------------------------------- */

    private float dp(float value) {
        return TypedValue.applyDimension(
                TypedValue.COMPLEX_UNIT_DIP,
                value,
                getResources().getDisplayMetrics());
    }
}
