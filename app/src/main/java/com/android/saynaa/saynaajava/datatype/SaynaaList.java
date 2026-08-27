package com.android.saynaa.saynaajava.datatype;

import android.content.*;
import android.util.Log;
import com.android.saynaa.saynaajava.*;
import java.util.*;

public final class SaynaaList extends SaynaaObject implements List {
  private static final String TAG = "SaynaaList";
  public SaynaaList(Saynaa saynaa, int type, int handleId) {
    super(saynaa, type, handleId);
  }

  @Override
  public void clear() {
    // TODO: Implement this method
  }

  @Override
  public boolean isEmpty() {
    // TODO: Implement this method
    return false;
  }

  @Override
  public boolean remove(Object p1) {
    // TODO: Implement this method
    return false;
  }

  @Override
  public int size() {
    // TODO: Implement this method
    int len = saynaa.getListSize(handleId);
    return len;
  }

  @Override
  public void add(int p1, Object p2) {
    // TODO: Implement this method
    int valueSlot = saynaa.nextSlot();
    if (!JavaBridge.pushToSlot(saynaa, valueSlot, p2)) {
      saynaa.freeSlot(valueSlot);
      Log.e(TAG, "Failed to push value to slot: " + valueSlot);
      return;
    }
    boolean result = saynaa.listInsert(handleId, p1, valueSlot);
    saynaa.freeSlot(valueSlot);
  }

  @Override
  public boolean add(Object p1) {
    // TODO: Implement this method
    int valueSlot = saynaa.nextSlot();
    if (!JavaBridge.pushToSlot(saynaa, valueSlot, p1)) {
      saynaa.freeSlot(valueSlot);
      Log.e(TAG, "Failed to push value to slot: " + valueSlot);
      return false;
    }
    boolean result = saynaa.listInsert(handleId, size(), valueSlot);
    saynaa.freeSlot(valueSlot);
    return result;
  }

  @Override
  public Object set(int p1, Object p2) {
    Object oldValue = get(p1);
  
    // TODO: Implement this method
    int valueSlot = saynaa.nextSlot();
    if (!JavaBridge.pushToSlot(saynaa, valueSlot, p2)) {
      saynaa.freeSlot(valueSlot);
      Log.e(TAG, "Failed to push value to slot: " + valueSlot);
      return oldValue;
    }
    boolean result = saynaa.listReplace(handleId, p1, valueSlot);
    saynaa.freeSlot(valueSlot);
    return oldValue;
  }

  @Override
  public boolean addAll(int p1, Collection p2) {
    // TODO: Implement this method
    return false;
  }

  @Override
  public boolean addAll(Collection p1) {
    // TODO: Implement this method
    return false;
  }

  @Override
  public boolean contains(Object p1) {
    // TODO: Implement this method
    return false;
  }

  @Override
  public boolean containsAll(Collection p1) {
    // TODO: Implement this method
    return false;
  }

  @Override
  public Object get(int p1) {
    // TODO: Implement this method
    int retSlot = saynaa.nextSlot();
    if (saynaa.listGetToSlot(handleId, p1, retSlot)) {
      Object value = JavaBridge.slotToJava(saynaa, retSlot);
      saynaa.freeSlot(retSlot);
      return value;
    }
    Log.e(TAG, "Failed to get list element at index: " + p1 + ", handleId=" + handleId);
    saynaa.freeSlot(retSlot);
    return null;
  }

  @Override
  public int indexOf(Object p1) {
    // TODO: Implement this method
    return 0;
  }

  @Override
  public Iterator iterator() {
    // TODO: Implement this method
    return null;
  }

  @Override
  public int lastIndexOf(Object p1) {
    // TODO: Implement this method
    return 0;
  }

  @Override
  public ListIterator listIterator() {
    // TODO: Implement this method
    return null;
  }

  @Override
  public ListIterator listIterator(int p1) {
    // TODO: Implement this method
    return null;
  }

  @Override
  public Object remove(int p1) {
    // TODO: Implement this method
    return null;
  }

  @Override
  public boolean removeAll(Collection p1) {
    // TODO: Implement this method
    return false;
  }

  @Override
  public boolean retainAll(Collection p1) {
    // TODO: Implement this method
    return false;
  }

  @Override
  public List subList(int p1, int p2) {
    // TODO: Implement this method
    return null;
  }

  @Override
  public Object[] toArray() {
    // TODO: Implement this method

    Object[] out = new Object[size()];
    for (int i = 0; i < size(); i++) {
      out[i] = get(i);
    }
    return out;
  }

  @Override
  public Object[] toArray(Object[] p1) {
    // TODO: Implement this method
    return null;
  }
}