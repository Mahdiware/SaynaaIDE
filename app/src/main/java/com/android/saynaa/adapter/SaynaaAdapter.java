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
import android.util.Log;
import android.view.LayoutInflater;
import android.view.View;
import android.view.ViewGroup;
import android.view.animation.Animation;
import android.widget.BaseAdapter;
import android.widget.Filter;
import android.widget.Filterable;
import android.widget.ImageView;
import android.widget.TextView;
import com.android.saynaa.activity.SaynaaActivity;
import com.android.saynaa.saynaajava.Saynaa;
import com.android.saynaa.saynaajava.SaynaaException;
import java.io.File;
import java.lang.reflect.Method;
import java.util.ArrayList;
import java.util.Collections;
import java.util.Comparator;
import java.util.HashMap;
import java.util.List;
import java.util.Map;
import java.util.Set;

public class SaynaaAdapter extends BaseAdapter implements Filterable {
  public interface SaynaaViewFactory {
    View createView(LayoutInflater inflater, ViewGroup parent, Map<String, Object> row);
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

  public SaynaaAdapter(Context context, Object layoutSpec, Object data) {
    this(context, resolveLayoutResId(layoutSpec), coerceDataList(data), resolveViewFactory(context, layoutSpec));
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

  public void setData(Object data) {
    setData(coerceDataList(data));
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

  public void addAll(Object items) {
    addAll(coerceDataList(items));
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
    Map<String, Object> row = mData.get(position);

    // --- CRITICAL FIX ---
    // If we are using a custom factory (loadlayout), we cannot recycle
    // the view because the factory has no way to update existing views.
    if (mViewFactory != null) {
      convertView = null;
    }
    // --------------------

    if (convertView == null) {
      try {
        view = createRowView(parent, row);
        holder = new ViewHolder(view);
        view.setTag(holder);
      } catch (RuntimeException e) {
        Log.e("SaynaaAdapter", "Failed to compile row layout inside getView()", e);
        e.printStackTrace();
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

    if (mData == null || position < 0 || position >= mData.size() || row == null) {
      return view;
    }

    // Only attempt to bind data manually if we are using standard XML Layouts
    // (because the ViewFactory handles its own data binding upon creation)
    if (mViewFactory == null) {
      bindRowToHolder(holder, row);
    }

    if (mAnimationFactory != null && view != null) { // Fixed: check 'view', not 'convertView'
      Animation animation = mAnimationCache.get(view);
      if (animation == null) {
        animation = mAnimationFactory.createAnimation();
        if (animation != null) {
          mAnimationCache.put(view, animation);
        }
      }
      if (animation != null) {
        view.clearAnimation();
        view.startAnimation(animation);
      }
    }

    return view;
  }

  private void bindRowToHolder(ViewHolder holder, Map<String, Object> row) {
    for (Map.Entry<String, Object> entry : row.entrySet()) {
      String key = entry.getKey();
      Object value = entry.getValue();

      // Find the view that matches the Map key (via ID name or tag)
      View targetView = holder.find(key);

      if (targetView != null) {
        // Handle TextViews
        if (targetView instanceof TextView) {
          ((TextView) targetView).setText(value != null ? String.valueOf(value) : "");
        }
        // Handle ImageViews (if you are passing Bitmaps/Drawables)
        else if (targetView instanceof ImageView) {
          if (value instanceof Bitmap) {
            ((ImageView) targetView).setImageBitmap((Bitmap) value);
          } else if (value instanceof Drawable) {
            ((ImageView) targetView).setImageDrawable((Drawable) value);
          }
        }
        // Add other view types (e.g., CheckBox, ProgressBar) here if needed
      }
    }
  }

  @Override
  public Filter getFilter() {
    if (mFilter == null) {
      mFilter = new SaynaaArrayFilter();
    }
    return mFilter;
  }

  private static int resolveLayoutResId(Object layoutSpec) {
    if (layoutSpec instanceof Number) {
      return ((Number) layoutSpec).intValue();
    }
    return 0;
  }

  private static SaynaaViewFactory resolveViewFactory(Context context, Object layoutSpec) {
    if (layoutSpec instanceof SaynaaViewFactory) {
      return (SaynaaViewFactory) layoutSpec;
    }
    if (layoutSpec instanceof Map || layoutSpec instanceof List) {
      if (!(context instanceof SaynaaActivity)) {
        throw new IllegalArgumentException("SaynaaActivity is required to use loadlayout.");
      }
      return new SaynaaLayoutFactory(context, layoutSpec);
    }
    return null;
  }

  private static List<Map<String, Object>> coerceDataList(Object data) {
    if (data == null) {
      return new ArrayList<>();
    }
    if (data instanceof List) {
      return coerceList((List<?>) data);
    }
    if (data instanceof Map) {
      return coerceMap((Map<?, ?>) data);
    }
    return new ArrayList<>();
  }

  private static List<Map<String, Object>> coerceList(List<?> list) {
    List<Map<String, Object>> out = new ArrayList<>();
    if (list == null) {
      return out;
    }
    for (Object item : list) {
      Map<String, Object> row = coerceRow(item);
      if (row != null) {
        out.add(row);
      }
    }
    return out;
  }

  private static List<Map<String, Object>> coerceMap(Map<?, ?> map) {
    List<Map<String, Object>> out = new ArrayList<>();
    if (map == null || map.isEmpty()) {
      return out;
    }

    boolean allNumeric = true;
    List<Map.Entry<?, ?>> entries = new ArrayList<>(map.entrySet());
    for (Map.Entry<?, ?> entry : entries) {
      if (toNumericKey(entry.getKey()) == null) {
        allNumeric = false;
        break;
      }
    }

    if (allNumeric) {
      Collections.sort(entries, new Comparator<Map.Entry<?, ?>>() {
        @Override
        public int compare(Map.Entry<?, ?> left, Map.Entry<?, ?> right) {
          Long leftKey = toNumericKey(left.getKey());
          Long rightKey = toNumericKey(right.getKey());
          if (leftKey == null && rightKey == null) {
            return 0;
          }
          if (leftKey == null) {
            return 1;
          }
          if (rightKey == null) {
            return -1;
          }
          return Long.compare(leftKey, rightKey);
        }
      });
      for (Map.Entry<?, ?> entry : entries) {
        Map<String, Object> row = coerceRow(entry.getValue());
        if (row != null) {
          out.add(row);
        }
      }
      return out;
    }

    Map<String, Object> row = coerceRow(map);
    if (row != null) {
      out.add(row);
    }
    return out;
  }

  private static Object cleanValue(Object val) {
    if (val == null)
      return null;

    if (val instanceof String || val instanceof Number || val instanceof Boolean
        || val instanceof Bitmap || val instanceof Drawable) {
      return val;
    }

    if (val instanceof Map) {
      Map<?, ?> m = (Map<?, ?>) val;
      Map<String, Object> cleanMap = new HashMap<>();
      for (Map.Entry<?, ?> entry : m.entrySet()) {
        if (entry.getKey() != null) {
          cleanMap.put(String.valueOf(entry.getKey()), cleanValue(entry.getValue()));
        }
      }
      return cleanMap;
    }

    if (val instanceof List) {
      List<?> l = (List<?>) val;
      List<Object> cleanList = new ArrayList<>();
      for (Object item : l) {
        cleanList.add(cleanValue(item));
      }
      return cleanList;
    }

    return String.valueOf(val);
  }

  private static Map<String, Object> coerceRow(Object item) {
    if (item instanceof Map) {
      Map<String, Object> row = new HashMap<>();
      Map<?, ?> map = (Map<?, ?>) item;
      for (Map.Entry<?, ?> entry : map.entrySet()) {
        if (entry.getKey() == null) {
          continue;
        }
        row.put(String.valueOf(entry.getKey()), cleanValue(entry.getValue()));
      }
      return row;
    }

    if (item != null) {
      Map<String, Object> row = new HashMap<>();
      row.put("text", cleanValue(item));
      return row;
    }
    return null;
  }

  private static Long toNumericKey(Object key) {
    if (key == null)
      return null;
    if (key instanceof Number) {
      return ((Number) key).longValue();
    }

    String str = String.valueOf(key).trim();
    if (str.endsWith(".0")) {
      str = str.substring(0, str.length() - 2);
    }

    try {
      return Long.parseLong(str);
    } catch (NumberFormatException ignored) {
      return null;
    }
  }

  private View createRowView(ViewGroup parent, Map<String, Object> row) {
    if (mViewFactory != null) {
      View view = mViewFactory.createView(mInflater, parent, row);
      if (view == null) {
        throw new IllegalStateException("SaynaaViewFactory returned null.");
      }
      return view;
    }
    return mInflater.inflate(mLayoutResId, parent, false);
  }

  private static final class SaynaaLayoutFactory implements SaynaaViewFactory {
    private static final String LOADLAYOUT_ALIAS = "loadlayout";
    private int LOADLAYOUT_ID = -1;
    private final Object layoutSpec;
    private final SaynaaActivity activity;
    private final Saynaa saynaa;

    SaynaaLayoutFactory(Context context, Object layoutSpec) {
      this.layoutSpec = layoutSpec;
      this.activity = context instanceof SaynaaActivity ? (SaynaaActivity) context : null;
      this.saynaa = this.activity != null ? this.activity.getSaynaa() : null;
    }

    @Override
    public View createView(LayoutInflater inflater, ViewGroup parent, Map<String, Object> row) {
      if (activity == null || saynaa == null) {
        throw new IllegalStateException("SaynaaActivity is required to use loadlayout.");
      }
      Object result = callLoadlayout(layoutSpec, row);
      if (result instanceof View) {
        return (View) result;
      }
      throw new IllegalStateException("loadlayout did not return a View object. Got: " + result);
    }

    private Object callLoadlayout(Object layoutSpec, Map<String, Object> row) {
      if (!ensureLoadlayout()) {
        return null;
      }
      try {
        return saynaa.callFunctionById(LOADLAYOUT_ID, activity, layoutSpec, row);
      } catch (Exception e) {
        Log.e("SaynaaAdapter", "SaynaaException triggered during callGlobalFunction('loadlayout')", e);
        e.printStackTrace();
        return null;
      }
    }

    private boolean ensureLoadlayout() {
      if (LOADLAYOUT_ID >= 0) {
        return true;
      }
      try {
        LOADLAYOUT_ID = saynaa.getGlobalFunctionId(LOADLAYOUT_ALIAS);
        return LOADLAYOUT_ID >= 0;
      } catch (Exception e) {
        return false;
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
        }
      }

      try {
        Object tag = view.getTag();
        if (tag != null) {
          String name = String.valueOf(tag);
          if (name.length() > 0 && !name.startsWith("android.")) {
            views.put(name, view);
          }
        }
      } catch (Exception ignored) {
      }

      if (view instanceof ViewGroup) {
        ViewGroup group = (ViewGroup) view;
        for (int i = 0; i < group.getChildCount(); i++) {
          indexView(group.getChildAt(i));
        }
      }
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