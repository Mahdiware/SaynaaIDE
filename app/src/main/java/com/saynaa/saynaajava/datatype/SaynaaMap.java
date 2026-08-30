package com.saynaa.saynaajava.datatype;

import android.content.*;
import com.saynaa.saynaajava.*;
import java.util.*;

public final class SaynaaMap<K, V> extends SaynaaObject implements Map<K, V> {
  public SaynaaMap(Saynaa saynaa, int type, int handleId) {
    super(saynaa, type, handleId);
  }

  @Override
  public void clear() {
    // TODO: Implement this method
  }

  @Override
  public boolean containsKey(Object key) {
    // TODO: Implement this method
    return false;
  }

  @Override
  public boolean containsValue(Object value) {
    // TODO: Implement this method
    return false;
  }

  @Override
  public Set<Entry<K, V>> entrySet() {
    // TODO: Implement this method
    HashSet<Entry<K, V>> sets = new HashSet<Entry<K, V>>();
    // push();
    // L.pushNil();
    // while (L.next(-2) != 0) {
    //   try {
    //     sets.add(new SaynaaEntry<K, V>((K) L.toJavaObject(-2), (V) L.toJavaObject(-1)));
    //   } catch (LuaException e) {
    //   }
    //   L.pop(1);
    // }
    // L.pop(1);
    return sets;
  }

  @Override
  public V get(Object key) {
    // TODO: Implement this method
    int keySlot = saynaa.nextSlot();
    int valueSlot = saynaa.nextSlot();

    if (!JavaBridge.pushToSlot(saynaa, keySlot, key)) {
      saynaa.freeSlot(keySlot);
      saynaa.freeSlot(valueSlot);
      return null;
    }

    if (!saynaa.mapGetToSlots(handleId, keySlot, valueSlot)) {
      saynaa.freeSlot(keySlot);
      saynaa.freeSlot(valueSlot);
      return null;
    }

    V value = (V) JavaBridge.slotToJava(saynaa, valueSlot);
    saynaa.freeSlot(keySlot);
    saynaa.freeSlot(valueSlot);

    return value;
  }

  @Override
  public boolean isEmpty() {
    // TODO: Implement this method
    return false;
  }

  @Override
  public Set<K> keySet() {
    // TODO: Implement this method
    HashSet<K> sets = new HashSet<K>();
    // push();
    // L.pushNil();
    // while (L.next(-2) != 0) {
    //   try {
    //     sets.add((K) L.toJavaObject(-2));
    //   } catch (LuaException e) {
    //   }
    //   L.pop(1);
    // }
    // L.pop(1);
    return sets;
  }

  @Override
  public V put(K key, V value) {
    // TODO: Implement this method
    int keySlot = saynaa.nextSlot();
    int valueSlot = saynaa.nextSlot();

    if (!JavaBridge.pushToSlot(saynaa, keySlot, key)) {
      saynaa.freeSlot(keySlot);
      saynaa.freeSlot(valueSlot);
      return null;
    }

    if (!JavaBridge.pushToSlot(saynaa, valueSlot, value)) {
      saynaa.freeSlot(keySlot);
      saynaa.freeSlot(valueSlot);
      return null;
    }

    if (!saynaa.mapSet(handleId, keySlot, valueSlot)) {
      saynaa.freeSlot(keySlot);
      saynaa.freeSlot(valueSlot);
      return null;
    }

    saynaa.freeSlot(keySlot);
    saynaa.freeSlot(valueSlot);

    return null;
  }

  @Override
  public void putAll(Map p1) {
    // TODO: Implement this method
  }

  @Override
  public V remove(Object key) {
    // TODO: Implement this method
    return null;
  }

  public boolean isList() {
    // TODO: Implement this method
    return false;
  }

  public int length() {
    // TODO: Implement this method
    return 0;
  }

  @Override
  public int size() {
    // TODO: Implement this method
    return saynaa.getMapSize(handleId);
  }

  @Override
  public Collection<V> values() {
    ArrayList<V> sets = new ArrayList<>();
    // push();
    // L.pushNil();
    // while (L.next(-2) != 0) {
    //   try {
    //     sets.add((V) L.toJavaObject(-1));
    //   } catch (LuaException e) {
    //   }
    //   L.pop(1);
    // }
    // L.pop(1);
    return sets;
  }

  public class SaynaaEntry<K, V> implements Entry<K, V> {
    private K mKey;

    private V mValue;

    @Override
    public K getKey() {
      // TODO: Implement this method
      return mKey;
    }

    @Override
    public V getValue() {
      // TODO: Implement this method
      return mValue;
    }

    public V setValue(V value) {
      // TODO: Implement this method
      V old = mValue;
      mValue = value;
      return old;
    }

    public SaynaaEntry(K k, V v) {
      mKey = k;
      mValue = v;
    }
  }
}