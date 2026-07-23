commit cd724a0eeb742b943407995ca8b021581a6fdd2b
Author: Mohammed Abdifitaax (Mahdiware) <mahdiware@gmail.com>
Date:   Thu Jul 23 11:54:33 2026 +0300

    improved

commit cc623c97daf20a74f985f1f2e48460e0209f7653
Author: Mohammed Abdifitaax (Mahdiware) <mahdiware@gmail.com>
Date:   Thu Jul 23 11:42:51 2026 +0300

    improved path module

commit e759db57e726f1118710078088c310749d916e90
Author: Mohammed Abdifitaax (Mahdiware) <mahdiware@gmail.com>
Date:   Thu Jul 23 11:24:05 2026 +0300

    added some improvement

commit 1dad920840a8481e5444db38a4abdcb310dfff14
Author: Mahdiware <mahdiware@gmail.com>
Date:   Wed Jul 22 14:41:50 2026 +0300

    Update JavaModule.java

commit 74756f422af67e23bec5420391536334916b778a
Author: Mahdiware <mahdiware@gmail.com>
Date:   Wed Jul 22 14:33:29 2026 +0300

    Update build.gradle

commit 8dd4751eee551651a6da7c3c1e4993a416c57dfd
Author: Mahdiware <mahdiware@gmail.com>
Date:   Wed Jul 22 14:24:39 2026 +0300

    Update build.gradle

commit fbcd2b1cc03483ccab776e212a9e405339be4876
Author: Mohammed Abdifitaax (Mahdiware) <mahdiware@gmail.com>
Date:   Wed Jul 22 12:14:30 2026 +0300

    Implement layout_gravity parsing with gravity flags

commit 7e4a38d63bdcb15b8c9c1e9c82f117e16d7494c3
Author: Mohammed Abdifitaax (Mahdiware) <mahdiware@gmail.com>
Date:   Wed Jul 22 10:53:27 2026 +0300

    removed, androidx, and material to reduse app size

commit 1cb4a2332d445838d5e426c76a13e1cc51fa9f81
Author: Mohammed Abdifitaax (Mahdiware) <mahdiware@gmail.com>
Date:   Thu Jul 16 12:08:49 2026 +0300

    added AppCompatActivity to use FAB in IDE

commit 3129a9357460e628ec61c8321bae2f51605d3c0e
Author: Mahdiware <mahdiware@gmail.com>
Date:   Thu Jul 16 11:29:20 2026 +0300

    Update build.gradle

commit 6af34b0746eb2fc871d6a24251b68aa2a94b7f48
Author: Mahdiware <mahdiware@gmail.com>
Date:   Thu Jul 16 11:17:14 2026 +0300

    Update build.gradle

commit 646fe440acf566a32593336c5b0b4f92e5a7dc00
Author: Mohammed Abdifitaax (Mahdiware) <mahdiware@gmail.com>
Date:   Wed Jul 15 11:30:23 2026 +0300

    Zip module updated

commit 1ccb8be3408fad6a38b7e248b6ddc28ddc2f8a91
Author: Mohammed Abdifitaax (Mahdiware) <mahdiware@gmail.com>
Date:   Wed Jul 15 09:38:14 2026 +0300

    added Zip module, and freed allocated slots

commit 02af2855cff5a052626545f058a7141571002f78
Author: Mohammed Abdifitaax (Mahdiware) <mahdiware@gmail.com>
Date:   Tue Jul 14 11:36:10 2026 +0300

    refactor: remove debug logging statements for cleaner code

commit 953bf9e33d58a01113a10f2fc0e55532c524dadd
Author: Mohammed Abdifitaax (Mahdiware) <mahdiware@gmail.com>
Date:   Tue Jul 14 11:35:51 2026 +0300

    Refactor JNI bridge for improved slot management and error handling
    
    - Introduced new functions for slot allocation and management: `saynaa_getSlotCount`, `saynaa_nextSlot`, `saynaa_allocSlot`, and `saynaa_freeSlot`.
    - Updated existing JNI methods to utilize the new slot management functions, enhancing clarity and reducing potential errors.
    - Removed unused and redundant code related to Java class name resolution and global injection.
    - Improved error handling in callback invocation and Java instance creation.
    - Enhanced logging for better debugging and tracking of slot usage.
    - Cleaned up commented-out code and unnecessary function definitions to streamline the codebase.

commit 3e751984450bfddc399a65f3ab862b84acba8822
Author: Mohammed Abdifitaax (Mahdiware) <mahdiware@gmail.com>
Date:   Sat Jun 20 12:15:12 2026 +0300

    fix(java.call): re-enable automatic String conversion to SaynaaString for consistent VM type handling

commit fd29b3b7db1c7cf92936bb720570ba2eb7d57787
Author: Mohammed Abdifitaax (Mahdiware) <mahdiware@gmail.com>
Date:   Fri Jun 19 17:40:48 2026 +0300

    forgeted to delete diff.tmp logs

commit 73c1e09fec426aeb50bea1d6a38954b575c621a7
Author: Mohammed Abdifitaax (Mahdiware) <mahdiware@gmail.com>
Date:   Fri Jun 19 17:32:16 2026 +0300

    fix(java-bridge): preserve java.lang.String objects and avoid implicit SaynaaString conversion
    
    Java String instances returned from constructors and method calls are
    now kept as JavaObject values instead of being automatically converted
    to SaynaaString. This allows Java methods such as getBytes() to be
    called correctly on String instances.

commit d42997358118b0e300a0360044573e6322ac4ec9
Author: Mohammed Abdifitaax (Mahdiware) <mahdiware@gmail.com>
Date:   Thu Jun 11 11:45:52 2026 +0300

    build(deps): add R8 library for D8 dex compiler support
    
    - added com.android.tools:r8 dependency
    - enables Java .class to .dex compilation inside IDE

commit e3bd4efebd86577f30070222228517d33b9675a3
Author: Mohammed Abdifitaax (Mahdiware) <mahdiware@gmail.com>
Date:   Thu Jun 11 11:30:43 2026 +0300

    chore(storage): add MANAGE_EXTERNAL_STORAGE for testing phase
    
    Temporary full file access enabled for development.
    Will be replaced with SAF (DocumentFile) based workspace system.

commit 3a8c6071b265a0770a73ef6e20f7a06d61c0abc2
Author: Mohammed Abdifitaax (Mahdiware) <mahdiware@gmail.com>
Date:   Thu Jun 11 11:12:28 2026 +0300

    refactor: remove duplicate callStatic usage and simplify JNI bridge
    
    - replaced java.callStatic with java.call where appropriate
    - removed unused jmethodID entries from JNI layer
    - moved methods from JavaBridge into reflection package

commit 292c82e065936c78802cfea6690780cc5235d434
Author: Mohammed Abdifitaax (Mahdiware) <mahdiware@gmail.com>
Date:   Thu Jun 11 10:07:32 2026 +0300

    added org.eclipse.jdt dependence, and storage permission

commit b617cb9125bb346ff46947b5c284ada3188c09df
Author: Mohammed Abdifitaax (Mahdiware) <mahdiware@gmail.com>
Date:   Wed Jun 10 11:43:23 2026 +0300

    chore: bump version to 2.0.1 (versionCode 201)

commit 976d6137f7ff2e2ba585ee8ca73c06f1f7f8f984
Author: Mohammed Abdifitaax (Mahdiware) <mahdiware@gmail.com>
Date:   Wed Jun 10 11:42:32 2026 +0300

    feat(reflection): introduce reflection engine and improve JavaBridge + JNI integration

commit f8713050bde3538c65509a5eb170df058aac8635
Author: Mohammed Abdifitaax (Mahdiware) <mahdiware@gmail.com>
Date:   Tue Jun 9 12:13:16 2026 +0300

    moved 'java' module form c/c++ to java

commit 6d2c135b29c9c260fbe4f2f4c36384affb3ccc52
Author: Mohammed Abdifitaax (Mahdiware) <mahdiware@gmail.com>
Date:   Sun Jun 7 12:09:42 2026 +0300

    updated, and enhaced

commit 5906d35effbaf991d577bdba95910cf4d39bfc81
Author: Mohammed Abdifitaax (Mahdiware) <mahdiware@gmail.com>
Date:   Fri Jun 5 09:37:00 2026 +0300

    Added `activity.getModule()` support for accessing the main module, enhanced `SaynaaObject`, introduced `SaynaaModule` inheriting from `SaynaaObject`, improved debug logging by printing string representations of messages to Logcat with INFO level, and included various runtime and stability improvements.

commit e0c06f3f4adcf771411a555a02908b131f92657a
Author: Mohammed Abdifitaax (Mahdiware) <mahdiware@gmail.com>
Date:   Wed May 27 10:30:14 2026 +0300

    added saynaa-lang last update, and added test ListView

commit 5c101db8a2453c74a0e420ad15e69f7565efcf2a
Author: Mohammed Abdifitaax (Mahdiware) <mahdiware@gmail.com>
Date:   Fri May 22 17:53:15 2026 +0300

    Update README to reflect new JNI source structure and additional demo scripts

commit b150ef99f75ac07478ecc6f43bf9676c2e5082f2
Author: Mohammed Abdifitaax (Mahdiware) <mahdiware@gmail.com>
Date:   Fri May 22 17:28:27 2026 +0300

    Refactor Saynaa module and improve bytecode handling
    
    - Removed SaynaaOnClickListener class as it was no longer needed.
    - Updated FileUtil to increment asset copy version from 12 to 13.
    - Enhanced saynaa_compiler.c with new functions for constant folding and improved binary operation handling.
    - Added checksum validation to bytecode writing in saynaa_bytecode.c.
    - Introduced global variable preservation and restoration in saynaa_module.c to maintain state across module executions.
    - Improved error handling and code readability across various files.

commit b45df7a625724790ddbd84b09e97e52a6d766339
Author: Mohammed Abdifitaax (Mahdiware) <mahdiware@gmail.com>
Date:   Wed Apr 15 17:35:20 2026 +0300

    saynaa-updated, GC fixed

commit 9aefd608e68f9ca8591e91e1a136dc30e27d5e4e
Author: Mohammed Abdifitaax (Mahdiware) <mahdiware@gmail.com>
Date:   Wed Apr 15 17:22:09 2026 +0300

    saynaa-updated

commit 749d1ec44947de64b44e750b3d6229e8cebe21c4
Author: Mohammed Abdifitaax (Mahdiware) <mahdiware@gmail.com>
Date:   Tue Apr 14 16:52:26 2026 +0300

    release testing

commit c6737c95bf9ae83bc1363eec1671cfc25a7275d2
Author: Mohammed Abdifitaax (Mahdiware) <mahdiware@gmail.com>
Date:   Tue Apr 14 15:09:35 2026 +0300

    release

commit 03ac031c25b8f41aebae86ed530077261286c346
Author: Mohammed Abdifitaax (Mahdiware) <mahdiware@gmail.com>
Date:   Mon Apr 6 10:37:09 2026 +0300

    disabled: arm computed goto crash

commit eac7fc3364e6919c7713e039886e1951f38edc36
Author: Mohammed Abdifitaax (Mahdiware) <mahdiware@gmail.com>
Date:   Sat Apr 4 15:11:19 2026 +0300

    build.yml updated

commit 1d34db9bf3bf747520983ae97b82daae48d92f60
Author: Mohammed Abdifitaax (Mahdiware) <mahdiware@gmail.com>
Date:   Sat Apr 4 15:05:58 2026 +0300

    saynaa-lang updated

commit a24304c5ef1b4a322951e2b1e6d89a0afb4aa28f
Author: Mohammed Abdifitaax (Mahdiware) <mahdiware@gmail.com>
Date:   Sat Apr 4 11:13:37 2026 +0300

    saynaa-lang updated

commit 8a422feb3ca16d54709217af78d59dc487f8ce23
Author: Mohammed Abdifitaax (Mahdiware) <mahdiware@gmail.com>
Date:   Sat Apr 4 09:10:08 2026 +0300

    Refactor SaynaaState and JNI integration
    
    - Added new methods to SaynaaState for executing files and strings, invoking callbacks, and managing global variables.
    - Introduced synchronized access to ensure thread safety in SaynaaState methods.
    - Updated JNI exports to include new functions for getting global function IDs and calling functions by ID with argument slots.
    - Enhanced logging in JNI functions to improve debugging and error tracking.
    - Removed deprecated JNI function for executing snippets with views.
    - Cleaned up JNI code to ensure proper resource management and exception handling.
    - Deleted old architecture-specific shared libraries (libandroid-saynaa.so) as part of the cleanup.

commit 695ab7310c1381eb85417d49bfca3e5b31cba50b
Author: Mohammed Abdifitaax (Mahdiware) <mahdiware@gmail.com>
Date:   Fri Mar 27 06:52:23 2026 +0300

    updated saynaa-lang

commit d3e90eccb0cb69b8fb7c6e8803f4f7de82284eb2
Author: Mohammed Abdifitaax (Mahdiware) <mahdiware@gmail.com>
Date:   Thu Mar 26 12:13:17 2026 +0300

    feat: add run function and update layout handling in main.sa; improve text display in main1.sa; enhance logging in jni_exports.c

commit 67b2de8049a3d0f5a28c6568d207e98fec766614
Author: Mohammed Abdifitaax (Mahdiware) <mahdiware@gmail.com>
Date:   Thu Mar 26 10:52:37 2026 +0300

    feat: implement dynamic class loading with Dex support and add example Hello class

commit e485e4a156bfd554349f4d687029ae4f653e4a8a
Author: Mohammed Abdifitaax (Mahdiware) <mahdiware@gmail.com>
Date:   Thu Mar 26 10:30:09 2026 +0300

    feat: enhance layout definitions and remove unused main3 layout

commit cbe494410b31b9fec47dc309d231973c0cedf585
Author: Mohammed Abdifitaax (Mahdiware) <mahdiware@gmail.com>
Date:   Thu Mar 26 09:35:46 2026 +0300

    Refactor Saynaa bytecode handling and improve map functionality
    
    - Changed return type of bytecode functions from SaynaaBytecodeStatus to Result for consistency.
    - Updated bytecode payload validation to include version checks and improved error handling.
    - Introduced new MAP_APPEND opcode for appending values to maps.
    - Enhanced map structure to maintain insertion order of keys and support auto-incremented integer keys.
    - Added a human-readable status message function for Result codes.
    - Improved error handling in Java bridge close function to prevent re-entrant calls during teardown.
    - Refactored Java class resolution logic to ensure proper handling of nested class references.

commit 4a122ed544562c35c743afa2fcad0d2c184b15c2
Author: Mohammed Abdifitaax (Mahdiware) <mahdiware@gmail.com>
Date:   Wed Mar 25 17:50:34 2026 +0300

    feat: add CodeEditor component and language support with syntax analysis

commit 534f7f8d98ab33ab6b7d31f7c9a72b0fe557c60e
Author: Mohammed Abdifitaax (Mahdiware) <mahdiware@gmail.com>
Date:   Wed Mar 25 17:32:26 2026 +0300

    feat: add loadlayout functionality and new demo layout, remove unused activity references

commit 12c3c4e24bddfcf26a2fa99dc02f1a1779bdc514
Author: Mohammed Abdifitaax (Mahdiware) <mahdiware@gmail.com>
Date:   Wed Mar 25 10:47:50 2026 +0300

    huge update

commit ac431922f23c0393a6420c57b55038494871e072
Author: Mohammed Abdifitaax (Mahdiware) <mahdiware@gmail.com>
Date:   Fri Mar 20 11:08:27 2026 +0300

    feat: implement SaynaaApplication and SaynaaBroadcastReceiver, enhance crash handling and context management

commit 90df53be04a434fa06cf5a5bc831c3a6d2c6fc7d
Author: Mohammed Abdifitaax (Mahdiware) <mahdiware@gmail.com>
Date:   Tue Mar 17 17:12:24 2026 +0300

    ci: enhance Android build workflow with NDK support and improved caching

commit 21016526fb29994b3d07df7e832a9b555d634df0
Author: Mohammed Abdifitaax (Mahdiware) <mahdiware@gmail.com>
Date:   Tue Mar 17 17:01:48 2026 +0300

    refactor: improve error handling and logging in SaynaaActivity and Saynaa classes
    feat: add onNativeError method to handle native errors in SaynaaActivity
    build: add GitHub Actions workflow for Android build
    fix: update assets version marker and increment version for asset copying
    test: add bad syntax script to validate error handling

commit b6404ab5aaf209ff700644f623edd0ad06aa987e
Author: Mohammed Abdifitaax (Mahdiware) <mahdiware@gmail.com>
Date:   Tue Mar 17 11:56:02 2026 +0300

    Huge Update

commit 868043102bf94e1fd3bb000739f4439b3a3fe931
Author: Mohammed Abdifitaax (Mahdiware) <mahdiware@gmail.com>
Date:   Tue Mar 17 11:55:05 2026 +0300

    - huge update

commit fc63960282f1d2f8d4f99f5c4586b7a886743b55
Author: Mohammed Abdifitaax (Mahdiware) <mahdiware@gmail.com>
Date:   Sun Mar 15 16:27:23 2026 +0300

    saynaa-lang: upgraded

commit 8e235e4553ed0c5ab76a848e124839c5d1cbd6f7
Author: Mohammed Abdifitaax (Mahdiware) <mahdiware@gmail.com>
Date:   Thu Mar 12 09:54:45 2026 +0300

    docs: update README with callback example for button interaction

commit 64752344f581182c0973c71b0f9052657f648920
Author: Mohammed Abdifitaax (Mahdiware) <mahdiware@gmail.com>
Date:   Thu Mar 12 09:43:06 2026 +0300

    docs: add project screenshot to README

commit 4da36f06b87f2de40cf61e04299d9d10c9a859f2
Author: Mohammed Abdifitaax (Mahdiware) <mahdiware@gmail.com>
Date:   Thu Mar 12 09:27:47 2026 +0300

    SaynaaIDE: first commit
