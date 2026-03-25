package com.android.saynaa.saynaajava;

public class PCallResult {
    public boolean success;
    public String message;
    public Object value;

    public PCallResult(boolean success, String message, Object value) {
        this.success = success;
        this.message = message;
        this.value = value;
    }
}