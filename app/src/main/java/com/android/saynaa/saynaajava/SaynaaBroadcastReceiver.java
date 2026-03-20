package com.android.saynaa.saynaajava;

import android.content.BroadcastReceiver;
import android.content.Context;
import android.content.Intent;

import java.lang.ref.WeakReference;

public class SaynaaBroadcastReceiver extends BroadcastReceiver {

    private WeakReference<OnReceiveListener> mListener;

    public SaynaaBroadcastReceiver(OnReceiveListener listener) {
        if (listener != null) {
            this.mListener = new WeakReference<>(listener);
        }
    }

    @Override
    public void onReceive(Context context, Intent intent) {
        if (mListener == null) return;

        OnReceiveListener listener = mListener.get();
        if (listener == null) return;

        if (intent == null) return;

        // Optional: filter actions here if needed
        // String action = intent.getAction();
        // if ("your.action.HERE".equals(action)) { ... }

        listener.onReceive(context, intent);
    }

    public interface OnReceiveListener {
        void onReceive(Context context, Intent intent);
    }
}