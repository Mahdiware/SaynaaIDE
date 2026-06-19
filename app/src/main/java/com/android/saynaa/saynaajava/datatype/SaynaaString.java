package com.android.saynaa.saynaajava.datatype;

public class SaynaaString {

    private  byte[] mByte=new byte[0];

    public SaynaaString(String string) {
        mByte=string.getBytes();
    }

    public SaynaaString(byte[] string) {
        mByte=string;
    }

    public byte[] toByteArray() {
        return mByte;
    }

    public int length() {
        return mByte.length;
    }

    public char charAt(int index) {
        return (char) mByte[index];
    }

    public CharSequence subSequence(int start, int end) {
        return new String(mByte,start,end);
    }

    public String toString() {
        return new String(mByte);
    }
}
