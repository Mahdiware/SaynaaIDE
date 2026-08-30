package com.saynaa.saynaajava;

import android.util.Log;
import android.view.View;
import com.saynaa.saynaajava.reflection.ReflectionFinder;
import java.lang.reflect.InvocationHandler;
import java.lang.reflect.Method;
import java.lang.reflect.Proxy;

public final class SaynaaProxyFactory {
  private static final String TAG = "SaynaaProxyFactory";

  private SaynaaProxyFactory() {
  }

  private static Object defaultReturnFor(Class<?> returnType) {
    if (returnType == void.class || returnType == Void.class)
      return null;
    if (Number.class.isAssignableFrom(returnType))
      return 0;
    if (returnType == boolean.class || returnType == Boolean.class)
      return false;
    if (returnType == byte.class || returnType == Byte.class)
      return (byte) 0;
    if (returnType == short.class || returnType == Short.class)
      return (short) 0;
    if (returnType == int.class || returnType == Integer.class)
      return 0;
    if (returnType == long.class || returnType == Long.class)
      return 0L;
    if (returnType == float.class || returnType == Float.class)
      return 0f;
    if (returnType == double.class || returnType == Double.class)
      return 0d;
    if (returnType == char.class || returnType == Character.class)
      return (char) 0;
    return null;
  }

  private static void sendProxyError(Saynaa saynaa, String methodName, Throwable t) {
    if (saynaa != null && saynaa.getContext() instanceof SaynaaContext) {
      Exception ex = t instanceof Exception ? (Exception) t : new SaynaaException(t);
      ((SaynaaContext) saynaa.getContext()).sendError(methodName, ex);
      return;
    }
    Log.e(TAG, "Proxy error: " + methodName, t);
  }

  public static Object createProxy(final Saynaa saynaa, final String interfaceName,
      final String methodName, final String functionName) {
    try {
      final Class<?> iface = ReflectionFinder.findClass(interfaceName);
      if (iface == null) {
        return null;
      }
      final int[] functionId = new int[] {-1};
      InvocationHandler handler = new InvocationHandler() {
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

          Class<?> rt = method.getReturnType();
          boolean wildcard = "*".equals(methodName);
          boolean matches = wildcard || (methodName != null && methodName.equals(method.getName()));
          if (saynaa != null && !saynaa.isClosed() && matches) {
            try {
              if (functionId[0] < 0 && functionName != null && !functionName.isEmpty()) {
                functionId[0] = saynaa.getGlobalFunctionId(functionName);
              }
              if (functionId[0] >= 0) {
                saynaa.callFunctionByIdWithArgs(functionId[0], args);
              }
            } catch (Throwable t) {
              sendProxyError(saynaa, method.getName(), t);
              return defaultReturnFor(rt);
            }
          }

          return defaultReturnFor(rt);
        }
      };

      return Proxy.newProxyInstance(iface.getClassLoader(), new Class<?>[] {iface}, handler);
    } catch (Throwable ignored) {
      return null;
    }
  }
}
