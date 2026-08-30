# Saynaa Android

Saynaa Android is an Android application runtime for the Saynaa programming language. It combines the Saynaa VM with a JNI-based Android bridge so Saynaa scripts can create views, call Java APIs, register listener proxies, and build Android UI directly on-device.

## Screenshot of the demo app running on an Android device:
![Screenshot one of Saynaa Android demo app](assets/screenshot1.png)
![Screenshot Two of Saynaa Android demo app](assets/screenshot2.png)
![Screenshot Three of Saynaa Android demo app](assets/screenshot3.png)


## Highlights

- Run Saynaa scripts inside an Android app.
- Access Android and Java APIs through the `java` module.
- Build native Android UI from Saynaa code.
- Register Java interface callbacks with either `java.createProxy(...)` or interface class-call syntax such as `View.OnClickListener({...})`.
- Use bundled scripts such as [app/src/main/assets/main.sa](app/src/main/assets/main.sa)
- Ship native bridge code through JNI and NDK build integration.

## Project layout

- [app](app) — Android application module.
- [app/src/main/assets](app/src/main/assets) — bundled Saynaa scripts and examples.
- [app/src/main/jni/saynaajava](app/src/main/jni/saynaajava) — JNI bridge between Saynaa and Android/Java.
- [app/src/main/jni/saynaajava/src](app/src/main/jni/saynaajava/src) — native source tree, split into `api`, `bridge`, `internal`, `wrappers`, and `main.c`.
- [app/src/main/jni/saynaajava/include](app/src/main/jni/saynaajava/include) — public/native headers used by the JNI bridge.

## Requirements

- Android SDK with API 34.
- Android NDK `26.1.10909125`.
- Java 8-compatible toolchain.
- Gradle available locally, or the project wrapper after initial download succeeds.

Current Android config from [app/build.gradle](app/build.gradle):

- `applicationId`: `com.saynaa`
- `minSdkVersion`: `21`
- `targetSdkVersion`: `34`
- `compileSdkVersion`: `34`
- supported ABIs: `x86`, `x86_64`

## Build

Build the debug APK:

- `gradle :app:assembleDebug`

Or use the wrapper:

- `./gradlew :app:assembleDebug`

The debug APK is generated under [app/build/outputs/apk](app/build/outputs/apk).

## Install and run

### Install the debug build on a connected device:

- `adb install -r app/build/outputs/apk/debug/app-debug.apk`

or

- `gradle :app:installDebug`

### Launch the app:

- `adb shell am start -n com.saynaa/.activity.MainActivity`

### View logs:

- `adb logcat | grep saynaajava`

### One Command:
- `gradle :app:assembleDebug && adb uninstall com.saynaa ; adb install -r app/build/outputs/apk/debug/app-debug.apk && adb logcat -c && adb shell am start -n com.saynaa/.activity.MainActivity && sleep 4 && adb logcat -d | grep -E "saynaajava|SaynaaMain|AndroidRuntime|Saynaa|MainActivity"`

or

- `adb logcat -c && gradle :app:installDebug && adb shell am start -n com.saynaa/.activity.SaynaaActivity -a android.intent.action.VIEW -d file:///data/user/0/com.saynaa/files/proxy_test.sa && sleep 2 && adb logcat -d | grep -E "Saynaa execution failed|Saynaa execution finished|Expected statement end" | tail -n 30`

## How scripting works

The app executes Saynaa source and exposes Android integration through built-in functions plus the `java` module.

Typical script flow:

- use the injected `activity` global directly
- import the `java` module
- bind Java classes with `java.bindClass(...)`
- create Android objects
- register interface callbacks with `java.createProxy(...)` or interface class-call syntax

See the main example in [app/src/main/assets/main.sa](app/src/main/assets/main.sa).

## `java` bridge quick example

```sa
import java

TextView = java.bindClass("android.widget.TextView")

view = TextView(activity)
view.setText("Hello from Saynaa")
activity.setContentView(view)
```

Callback example:

```sa
import java

Button = java.bindClass("android.widget.Button")
LinearLayout = java.bindClass("android.widget.LinearLayout")
View = java.bindClass("android.view.View")

click_cb = {
	onClick: function(v)
		v.setText("Button clicked")
		print("clicked")
	end
}

button = Button(activity)
button.setText("Click me")
button.setOnClickListener(View.OnClickListener(click_cb))

layout = LinearLayout(activity)
layout.setOrientation(LinearLayout.VERTICAL)
layout.addView(button)

activity.setContentView(layout)
```

## Architecture summary

- Java side hosts the Android app and reflection utilities.
- JNI bridge converts values between Saynaa and Java.
- Saynaa scripts use built-ins and the `java` module to drive Android behavior.
- wrapper classes such as `JavaClass`, `JavaObject`, and `JavaMethod` are created internally by the bridge.

## Notes

- The public scripting surface is the `java` module.
- The JNI source tree now lives under `app/src/main/jni/saynaajava/src`, while the Java runtime classes live under `app/src/main/java/com/saynaa/saynaajava`.
- The project includes reference folders used during development; the active runtime for users is Saynaa.

## Status

The repository currently builds successfully with:

- `gradle :app:assembleDebug`

If you want to extend the runtime, the safest starting points are the assets in [app/src/main/assets](app/src/main/assets) and the bridge code in [app/src/main/jni/saynaajava](app/src/main/jni/saynaajava).