package com.android.saynaa.saynaajava;

public class CPtr {
  public boolean equals(Object other) {
    if (other == null)
      return false;
    if (other == this)
      return true;
    if (CPtr.class != other.getClass())
      return false;
    return pointer == ((CPtr) other).pointer;
  }

  private long pointer;

  protected long getPointer() {
    return pointer;
  }

  void setPointer(long pointer) {
    this.pointer = pointer;
  }

  CPtr() {
  }
}