package com.android.saynaa.saynaajava;

public class SaynaaException extends Exception {
  private static final long serialVersionUID = 1L;

  public SaynaaException(String message) {
    super(message);
  }

  public SaynaaException(Throwable cause) {
    super(cause);
  }
}
