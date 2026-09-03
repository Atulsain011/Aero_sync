# Keep all JNI native bridge methods and classes
-keep class com.aerosync.app.nativebridge.** { *; }
-keepclassmembers class com.aerosync.app.nativebridge.** {
    native <methods>;
    public <methods>;
    *;
}

# Keep all service and data classes
-keep class com.aerosync.app.service.** { *; }
-keep class com.aerosync.app.data.** { *; }
-keep class com.aerosync.app.viewmodel.** { *; }

# Keep Kotlin Coroutines and Reflection
-keepattributes *Annotation*,Signature,InnerClasses,EnclosingMethod
