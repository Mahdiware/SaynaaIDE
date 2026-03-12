package com.android.saynaa.saynaajava;

import android.view.View;
import java.lang.reflect.InvocationHandler;
import java.lang.reflect.Method;
import java.lang.reflect.Proxy;

public final class SaynaaProxyFactory {
  private SaynaaProxyFactory() {}

  public static Object createProxy(
      final Saynaa saynaa,
      final String interfaceName,
      final String methodName,
      final String script) {
    try {
      final Class<?> iface = Class.forName(interfaceName);
      InvocationHandler handler =
          new InvocationHandler() {
            @Override
            public Object invoke(Object proxy, Method method, Object[] args) {
              if ("toString".equals(method.getName()) && method.getParameterTypes().length == 0) {
                return "SaynaaProxy(" + interfaceName + ")";
              }
              if ("hashCode".equals(method.getName()) && method.getParameterTypes().length == 0) {
                return System.identityHashCode(proxy);
              }
              if ("equals".equals(method.getName()) && method.getParameterTypes().length == 1) {
                return proxy == (args == null ? null : args[0]);
              }

              if (saynaa != null && !saynaa.isClosed() && methodName != null && methodName.equals(method.getName())) {
                View eventView = null;
                if (args != null && args.length > 0 && args[0] instanceof View) {
                  eventView = (View) args[0];
                }
                saynaa.executeSnippetWithView(script == null ? "" : script, eventView);
              }

              Class<?> rt = method.getReturnType();
              if (rt == boolean.class)
                return false;
              if (rt == byte.class)
                return (byte) 0;
              if (rt == short.class)
                return (short) 0;
              if (rt == int.class)
                return 0;
              if (rt == long.class)
                return 0L;
              if (rt == float.class)
                return 0f;
              if (rt == double.class)
                return 0d;
              if (rt == char.class)
                return (char) 0;
              return null;
            }
          };

      return Proxy.newProxyInstance(iface.getClassLoader(), new Class<?>[] {iface}, handler);
    } catch (Throwable ignored) {
      return null;
    }
  }
}
