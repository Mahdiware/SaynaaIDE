package com.android.saynaa.adapter;

import android.annotation.SuppressLint;
import android.content.Context;
import android.content.res.Resources;
import android.graphics.Bitmap;
import android.graphics.BitmapFactory;
import android.graphics.drawable.BitmapDrawable;
import android.graphics.drawable.Drawable;
import android.os.Handler;
import android.os.Message;
import android.view.LayoutInflater;
import android.view.View;
import android.view.ViewGroup;
import android.view.animation.Animation;
import android.widget.BaseAdapter;
import android.widget.Filter;
import android.widget.Filterable;
import android.widget.ImageView;
import android.widget.TextView;
import java.io.File;
import java.lang.reflect.Method;
import java.util.ArrayList;
import java.util.HashMap;
import java.util.LinkedList;
import java.util.List;
import java.util.Map;
import java.util.Set;

public class SaynaaAdapter extends BaseAdapter implements Filterable {
  public interface SaynaaViewFactory {
    View createView(LayoutInflater inflater, ViewGroup parent);
  }

  public interface SaynaaRowFilter {
    void filter(List<Map<String, Object>> source, List<Map<String, Object>> out, CharSequence prefix)
        throws Exception;
  }

  public interface SaynaaAnimationFactory {
    Animation createAnimation();
  }

  private final Context mContext;
  private final Resources mResources;
  private final LayoutInflater mInflater;
  private final int mLayoutResId;
  private final SaynaaViewFactory mViewFactory;

  private final List<Map<String, Object>> mBaseData;
  private List<Map<String, Object>> mData;
  private Map<String, Object> mTheme;

  private final Object mLock = new Object();
  private CharSequence mPrefix;
  private SaynaaAnimationFactory mAnimationFactory;
  private final Map<View, Animation> mAnimationCache = new HashMap<>();
  private final Map<View, Boolean> mStyleCache = new HashMap<>();
  private final Map<String, Boolean> mLoadedImages = new HashMap<>();

  private boolean mNotifyOnChange = true;
  private boolean mUpdating;

  private SaynaaRowFilter mRowFilter;
  private SaynaaArrayFilter mFilter;

  @SuppressLint("HandlerLeak")
  private final Handler mHandler = new Handler() {
    @Override
    public void handleMessage(Message msg) {
      if (msg.what == 0) {
        notifyDataSetChanged();
        return;
      }

      try {
        List<Map<String, Object>> filtered = new ArrayList<>();
        if (mRowFilter != null) {
          mRowFilter.filter(mBaseData, filtered, mPrefix);
        } else {
          filtered.addAll(mBaseData);
        }
        mData = filtered;
        notifyDataSetChanged();
      } catch (Exception e) {
        throw new RuntimeException("SaynaaAdapter filter failed", e);
      }
    }
  };

  public SaynaaAdapter(Context context, int layoutResId) {
    this(context, layoutResId, null, null);
  }

  public SaynaaAdapter(Context context, int layoutResId, List<Map<String, Object>> data) {
    this(context, layoutResId, data, null);
  }

  public SaynaaAdapter(Context context, SaynaaViewFactory viewFactory) {
    this(context, 0, null, viewFactory);
  }

  public SaynaaAdapter(Context context, SaynaaViewFactory viewFactory, List<Map<String, Object>> data) {
    this(context, 0, data, viewFactory);
  }

  private SaynaaAdapter(Context context, int layoutResId, List<Map<String, Object>> data,
      SaynaaViewFactory viewFactory) {
    if (context == null) {
      throw new IllegalArgumentException("context == null");
    }
    if (layoutResId == 0 && viewFactory == null) {
      throw new IllegalArgumentException("Either layoutResId or viewFactory must be provided.");
    }

    mContext = context;
    mResources = context.getResources();
    mInflater = LayoutInflater.from(context);
    mLayoutResId = layoutResId;
    mViewFactory = viewFactory;

    mBaseData = data != null ? data : new ArrayList<Map<String, Object>>();
    mData = mBaseData;
  }

  public void setAnimation(Animation animation) {
    mAnimationCache.clear();
    mAnimationFactory = animation == null ? null : new StaticAnimationFactory(animation);
  }

  public void setAnimationFactory(SaynaaAnimationFactory animationFactory) {
    mAnimationCache.clear();
    mAnimationFactory = animationFactory;
  }

  public void setTheme(Map<String, Object> theme) {
    mStyleCache.clear();
    mTheme = theme;
  }

  public void setFilter(SaynaaRowFilter filter) {
    mRowFilter = filter;
  }

  public void setData(List<Map<String, Object>> data) {
    mBaseData.clear();
    if (data != null) {
      mBaseData.addAll(data);
    }
    mData = mBaseData;
    notifyDataSetChanged();
  }

  public List<Map<String, Object>> getData() {
    return mData;
  }

  public void add(Map<String, Object> item) {
    mBaseData.add(item);
    if (mNotifyOnChange) {
      notifyDataSetChanged();
    }
  }

  public void addAll(List<Map<String, Object>> items) {
    if (items != null && !items.isEmpty()) {
      mBaseData.addAll(items);
      if (mNotifyOnChange) {
        notifyDataSetChanged();
      }
    }
  }

  public void insert(int position, Map<String, Object> item) {
    mBaseData.add(position, item);
    if (mNotifyOnChange) {
      notifyDataSetChanged();
    }
  }

  public void remove(int position) {
    mBaseData.remove(position);
    if (mNotifyOnChange) {
      notifyDataSetChanged();
    }
  }

  public void clear() {
    mBaseData.clear();
    if (mNotifyOnChange) {
      notifyDataSetChanged();
    }
  }

  public void setNotifyOnChange(boolean notifyOnChange) {
    mNotifyOnChange = notifyOnChange;
    if (mNotifyOnChange) {
      notifyDataSetChanged();
    }
  }

  public void filter(CharSequence prefix) {
    getFilter().filter(prefix);
  }

  @Override
  public int getCount() {
    return mData == null ? 0 : mData.size();
  }

  @Override
  public Object getItem(int position) {
    return mData == null || position < 0 || position >= mData.size() ? null : mData.get(position);
  }

  @Override
  public long getItemId(int position) {
    return position;
  }

  @Override
  public void notifyDataSetChanged() {
    super.notifyDataSetChanged();
    if (!mUpdating) {
      mUpdating = true;
      new Handler().postDelayed(new Runnable() {
        @Override
        public void run() {
          mUpdating = false;
        }
      }, 500);
    }
  }

  @Override
  public View getDropDownView(int position, View convertView, ViewGroup parent) {
    return getView(position, convertView, parent);
  }

  @Override
  public View getView(int position, View convertView, ViewGroup parent) {
    View view;
    ViewHolder holder;

    if (convertView == null) {
      try {
        view = createRowView(parent);
        holder = new ViewHolder(view);
        view.setTag(holder);
      } catch (RuntimeException e) {
        return new View(mContext);
      }
    } else {
      view = convertView;
      Object tag = view.getTag();
      holder = tag instanceof ViewHolder ? (ViewHolder) tag : new ViewHolder(view);
      if (tag == null) {
        view.setTag(holder);
      }
    }

    if (mData == null || position < 0 || position >= mData.size()) {
      return view;
    }

    Map<String, Object> row = mData.get(position);
    if (row == null) {
      return view;
    }

    boolean firstBind = !mStyleCache.containsKey(view);
    if (firstBind) {
      mStyleCache.put(view, Boolean.TRUE);
    }

    applyEntries(holder, row, firstBind);

    if (mUpdating) {
      return view;
    }

    if (mAnimationFactory != null && convertView != null) {
      Animation animation = mAnimationCache.get(convertView);
      if (animation == null) {
        animation = mAnimationFactory.createAnimation();
        if (animation != null) {
          mAnimationCache.put(convertView, animation);
        }
      }
      if (animation != null) {
        view.clearAnimation();
        view.startAnimation(animation);
      }
    }

    return view;
  }

  @Override
  public Filter getFilter() {
    if (mFilter == null) {
      mFilter = new SaynaaArrayFilter();
    }
    return mFilter;
  }

  private View createRowView(ViewGroup parent) {
    if (mViewFactory != null) {
      View view = mViewFactory.createView(mInflater, parent);
      if (view == null) {
        throw new IllegalStateException("SaynaaViewFactory returned null.");
      }
      return view;
    }
    return mInflater.inflate(mLayoutResId, parent, false);
  }

  private void applyEntries(ViewHolder holder, Map<String, Object> row, boolean firstBind) {
    Set<Map.Entry<String, Object>> entries = row.entrySet();
    List<TextView> textViews = new ArrayList<>();
    holder.collectTextViews(textViews);
    int textViewIndex = 0;

    for (Map.Entry<String, Object> entry : entries) {
      String key = entry.getKey();
      Object value = entry.getValue();
      if (key == null || value == null) {
        continue;
      }

      // Try to find view by exact ID / tag match first
      View target = holder.find(key);
      if (target != null) {
        // Found by ID / tag
        if (mTheme != null && firstBind) {
          applyValue(target, mTheme.get(key));
        }
        applyValue(target, value);
        continue;
      }

      // Fallback : for text fields without matching views, apply to TextViews in order
      if (isTextProperty(key) && !textViews.isEmpty() && textViewIndex < textViews.size()) {
        if (mTheme != null && firstBind) {
          applyValue(textViews.get(textViewIndex), mTheme.get(key));
        }
        applyValue(textViews.get(textViewIndex), value);
        textViewIndex++;
      }
    }
  }

  private boolean isTextProperty(String key) {
    if (key == null) {
      return false;
    }
    String lower = key.toLowerCase();
    return lower.equals("text") || lower.equals("title") || lower.equals("subtitle")
        || lower.equals("content") || lower.equals("label") || lower.equals("name")
        || lower.equals("description") || lower.equals("message");
  }

  private void applyValue(View view, Object value) {
    try {
      if (value instanceof Map) {
        @SuppressWarnings("unchecked") Map<String, Object> fields = (Map<String, Object>) value;
        applyFields(view, fields);
        return;
      }

      if (view instanceof TextView) {
        applyTextValue((TextView) view, value);
        return;
      }

      if (view instanceof ImageView) {
        applyImageValue((ImageView) view, value);
        return;
      }

      if (applyGenericSetter(view, "value", value)) {
        return;
      }

      if (value instanceof View) {
        return;
      }
    } catch (Exception e) {
      // Keep adapter resilient; ignore one bad field instead of killing the row.
      e.printStackTrace();
    }
  }

  private void applyFields(View view, Map<String, Object> fields) throws Exception {
    Set<Map.Entry<String, Object>> entries = fields.entrySet();
    for (Map.Entry<String, Object> entry : entries) {
      String key = entry.getKey();
      Object value = entry.getValue();
      if (key == null) {
        continue;
      }
      if (key.toLowerCase().startsWith("on") && value instanceof View.OnClickListener) {
        applyGenericSetter(view, key, value);
      } else if ("src".equalsIgnoreCase(key)) {
        applyValue(view, value);
      } else {
        applyGenericSetter(view, key, value);
      }
    }
  }

  private void applyTextValue(TextView textView, Object value) {
    if (value == null) {
      textView.setText("");
    } else if (value instanceof CharSequence) {
      textView.setText((CharSequence) value);
    } else {
      textView.setText(String.valueOf(value));
    }
  }

  private void applyImageValue(ImageView imageView, Object value) {
    if (value == null) {
      return;
    }

    if (value instanceof Bitmap) {
      imageView.setImageBitmap((Bitmap) value);
      return;
    }

    if (value instanceof Drawable) {
      imageView.setImageDrawable((Drawable) value);
      return;
    }

    if (value instanceof Number) {
      imageView.setImageResource(((Number) value).intValue());
      return;
    }

    if (value instanceof String) {
      String path = (String) value;
      if (path.length() == 0) {
        return;
      }
      if (path.startsWith("http://") || path.startsWith("https://")) {
        return;
      }
      File file = new File(path);
      if (file.exists()) {
        Bitmap bitmap = BitmapFactory.decodeFile(file.getAbsolutePath());
        if (bitmap != null) {
          imageView.setImageDrawable(new BitmapDrawable(mResources, bitmap));
        }
        return;
      }
      int resId = mResources.getIdentifier(path, "drawable", mContext.getPackageName());
      if (resId != 0) {
        imageView.setImageResource(resId);
      }
    }
  }

  private boolean applyGenericSetter(Object target, String propertyName, Object value) throws Exception {
    if (target == null || propertyName == null || propertyName.length() == 0) {
      return false;
    }

    String methodName = propertyName;
    if (Character.isLowerCase(methodName.charAt(0))) {
      methodName = Character.toUpperCase(methodName.charAt(0)) + methodName.substring(1);
    }
    methodName = "set" + methodName;

    Method[] methods = target.getClass().getMethods();
    for (Method method : methods) {
      if (!method.getName().equals(methodName) || method.getParameterTypes().length != 1) {
        continue;
      }

      Class<?> parameterType = method.getParameterTypes()[0];
      Object argument = coerceValue(parameterType, value);
      if (argument == COERCION_FAILED) {
        continue;
      }
      method.invoke(target, argument);
      return true;
    }
    return false;
  }

  private static final Object COERCION_FAILED = new Object();

  private Object coerceValue(Class<?> parameterType, Object value) {
    if (value == null) {
      return parameterType.isPrimitive() ? COERCION_FAILED : null;
    }

    if (parameterType.isInstance(value)) {
      return value;
    }

    if (parameterType == CharSequence.class || parameterType == String.class) {
      return String.valueOf(value);
    }

    if (parameterType == int.class || parameterType == Integer.class) {
      if (value instanceof Number) {
        return ((Number) value).intValue();
      }
      if (value instanceof String) {
        try {
          return Integer.parseInt((String) value);
        } catch (NumberFormatException ignored) {
          return COERCION_FAILED;
        }
      }
    }

    if (parameterType == long.class || parameterType == Long.class) {
      if (value instanceof Number) {
        return ((Number) value).longValue();
      }
      if (value instanceof String) {
        try {
          return Long.parseLong((String) value);
        } catch (NumberFormatException ignored) {
          return COERCION_FAILED;
        }
      }
    }

    if (parameterType == float.class || parameterType == Float.class) {
      if (value instanceof Number) {
        return ((Number) value).floatValue();
      }
    }

    if (parameterType == double.class || parameterType == Double.class) {
      if (value instanceof Number) {
        return ((Number) value).doubleValue();
      }
    }

    if (parameterType == boolean.class || parameterType == Boolean.class) {
      if (value instanceof Boolean) {
        return value;
      }
      if (value instanceof String) {
        return Boolean.parseBoolean((String) value);
      }
    }

    if (parameterType == Drawable.class && value instanceof Bitmap) {
      return new BitmapDrawable(mResources, (Bitmap) value);
    }

    if (parameterType.isEnum() && value instanceof String) {
      @SuppressWarnings({"unchecked", "rawtypes"})
      Class<? extends Enum> enumType = (Class<? extends Enum>) parameterType;
      try {
        return Enum.valueOf(enumType, (String) value);
      } catch (IllegalArgumentException ignored) {
        return COERCION_FAILED;
      }
    }

    return COERCION_FAILED;
  }

  private void collectTextViews(View view, List<TextView> out) {
    if (view == null || out == null) {
      return;
    }

    if (view instanceof TextView) {
      out.add((TextView) view);
    }

    if (view instanceof ViewGroup) {
      ViewGroup group = (ViewGroup) view;
      for (int i = 0; i < group.getChildCount(); i++) {
        collectTextViews(group.getChildAt(i), out);
      }
    }
  }

  private final class ViewHolder {
    private final Map<String, View> views = new HashMap<>();
    private final List<TextView> textViews = new ArrayList<>();

    ViewHolder(View root) {
      indexView(root);
    }

    View find(String name) {
      return views.get(name);
    }

    private void indexView(View view) {
      if (view == null) {
        return;
      }

      if (view instanceof TextView) {
        textViews.add((TextView) view);
      }

      int id = view.getId();
      if (id != View.NO_ID) {
        try {
          String name = mResources.getResourceEntryName(id);
          if (name != null) {
            views.put(name, view);
          }
        } catch (Resources.NotFoundException ignored) {
          // Ignore anonymous IDs.
        }
      }

      // Fallback: allow views to be indexed by a String tag so programmatically
      // created rows (without resource ids) can be targeted by map keys.
      try {
        Object tag = view.getTag();
        if (tag != null) {
          String name = String.valueOf(tag);
          if (name.length() > 0 && !name.startsWith("android.")) {
            views.put(name, view);
          }
        }
      } catch (Exception ignored) {
        // Ignore tag lookup failures.
      }

      if (view instanceof ViewGroup) {
        ViewGroup group = (ViewGroup) view;
        for (int i = 0; i < group.getChildCount(); i++) {
          indexView(group.getChildAt(i));
        }
      }
    }

    void collectTextViews(List<TextView> out) {
      if (out == null) {
        return;
      }
      out.addAll(textViews);
    }
  }

  private final class StaticAnimationFactory implements SaynaaAnimationFactory {
    private final Animation animation;

    StaticAnimationFactory(Animation animation) {
      this.animation = animation;
    }

    @Override
    public Animation createAnimation() {
      return animation;
    }
  }

  private final class SaynaaArrayFilter extends Filter {
    @Override
    protected FilterResults performFiltering(CharSequence prefix) {
      FilterResults results = new FilterResults();
      mPrefix = prefix;
      if (mData == null) {
        return results;
      }
      if (mRowFilter != null) {
        mHandler.sendEmptyMessage(1);
        return null;
      }
      results.values = mData;
      results.count = mData.size();
      return results;
    }

    @Override
    @SuppressWarnings("unchecked")
    protected void publishResults(CharSequence constraint, FilterResults results) {
      if (results != null && results.values instanceof List) {
        mData = (List<Map<String, Object>>) results.values;
        if (results.count > 0) {
          notifyDataSetChanged();
        } else {
          notifyDataSetInvalidated();
        }
      }
    }
  }
}
