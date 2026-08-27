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
import android.widget.AbsListView;
import android.widget.BaseAdapter;
import android.widget.Filter;
import android.widget.Filterable;
import android.widget.ImageView;
import android.widget.TextView;
import com.android.saynaa.activity.SaynaaActivity;
import com.android.saynaa.saynaajava.Saynaa;
import com.android.saynaa.saynaajava.SaynaaException;
import com.android.saynaa.saynaajava.datatype.*;
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
  private int mLayoutResId = 0;

  private final SaynaaMap mItems;
  private SaynaaList mData;
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
  private SaynaaClass LoadLayout;

  @SuppressLint("HandlerLeak")
  private final Handler mHandler = new Handler() {
    @Override
    public void handleMessage(Message msg) {
      if (msg.what == 0) {
        notifyDataSetChanged();
        return;
      }

      try {
        // List<Map<String, Object>> filtered = new ArrayList<>();
        // if (mRowFilter != null) {
        //   mRowFilter.filter(mData, filtered, mPrefix);
        // } else {
        //   filtered.addAll(mData);
        // }
        // mData = filtered;
        notifyDataSetChanged();
      } catch (Exception e) {
        throw new RuntimeException("SaynaaAdapter filter failed", e);
      }
    }
  };

  public SaynaaAdapter(Context context, SaynaaMap layoutSpec, SaynaaList data) {
    if (context == null) {
      throw new IllegalArgumentException("context == null");
    }

    Object globalObject = ((SaynaaActivity) context).getSaynaa().getGlobal("LoadLayout");
    if (globalObject instanceof SaynaaClass) {
      LoadLayout = (SaynaaClass) globalObject;
    } else {
      throw new IllegalStateException("LoadLayout class not found in Saynaa environment.");
    }

    mContext = context;
    mResources = context.getResources();
    mInflater = LayoutInflater.from(context);

    mItems = layoutSpec;
    mData = data;
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

  public void setData(int index, Object data) {
    mData.clear();
    if (data != null) {
      mData.set(index, data);
    }
    notifyDataSetChanged();
  }

  public List<Map<String, Object>> getData() {
    return mData;
  }

  public void add(Map<String, Object> item) {
    mData.add(item);
    if (mNotifyOnChange) {
      notifyDataSetChanged();
    }
  }

  public void addAll(List<Map<String, Object>> items) {
    if (items != null && !items.isEmpty()) {
      mData.addAll(items);
      if (mNotifyOnChange) {
        notifyDataSetChanged();
      }
    }
  }

  public void addAll(Object items) {
    addAll(items);
  }

  public void insert(int position, Map<String, Object> item) {
    mData.add(position, item);
    if (mNotifyOnChange) {
      notifyDataSetChanged();
    }
  }

  public void remove(int position) {
    mData.remove(position);
    if (mNotifyOnChange) {
      notifyDataSetChanged();
    }
  }

  public void clear() {
    mData.clear();
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
    SaynaaInstance holder;

    Object row = mData.get(position);

    if (convertView == null) {
      try {
        holder = LoadLayout.newInstance(mContext);
        view = (View) holder.call("createView", mItems, AbsListView.class);
        view.setTag(holder);
      } catch (RuntimeException e) {
        Log.e("SaynaaAdapter", "Failed to compile row layout inside getView()", e);
        return new View(mContext);
      }
    } else {
      view = convertView;
      holder = (SaynaaInstance) view.getTag();
    }

    // ALWAYS bind, even when updating, to prevent stale data on recycled views
    holder.call("bind", row);

    // Key animation by position rather than the recycled view instance
    if (mAnimationFactory != null) {
      Animation animation = mAnimationCache.get(position);
      if (animation == null) {
        animation = mAnimationFactory.createAnimation();
        if (animation != null) {
          //gmAnimationCache.put(position, animation);
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
        // mData = (List<Map<String, Object>>) results.values;
        if (results.count > 0) {
          notifyDataSetChanged();
        } else {
          notifyDataSetInvalidated();
        }
      }
    }
  }
}