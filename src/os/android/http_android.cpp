// Android HTTP(S) backend via java.net.HttpURLConnection (TLS provided by the platform).
#include "../os_internal.h"
#include "jni_env.h"

namespace mdv::os::detail {

namespace {

std::string toStdString(JNIEnv* env, jstring s)
{
    if (!s)
        return {};
    const char* utf = env->GetStringUTFChars(s, nullptr);
    std::string out(utf);
    env->ReleaseStringUTFChars(s, utf);
    return out;
}

} // namespace

net::HttpResponse httpGetBlocking(const std::string& url)
{
    net::HttpResponse response;
    android::ScopedJniEnv env;
    if (!env) {
        response.error = "JNI unavailable";
        return response;
    }
    // Local references are released in bulk when this frame is popped.
    env->PushLocalFrame(32);

    auto fail = [&](const char* what) {
        env.clearException();
        response.error = what;
        env->PopLocalFrame(nullptr);
        return response;
    };

    jclass urlClass = env->FindClass("java/net/URL");
    jobject jurl = env->NewObject(urlClass, env->GetMethodID(urlClass, "<init>", "(Ljava/lang/String;)V"),
                                  env->NewStringUTF(url.c_str()));
    if (env.clearException() || !jurl)
        return fail("Malformed URL");

    jobject conn = env->CallObjectMethod(jurl, env->GetMethodID(urlClass, "openConnection", "()Ljava/net/URLConnection;"));
    if (env.clearException() || !conn)
        return fail("Could not open connection");

    jclass httpClass = env->FindClass("java/net/HttpURLConnection");
    env->CallVoidMethod(conn, env->GetMethodID(httpClass, "setInstanceFollowRedirects", "(Z)V"), JNI_TRUE);
    env->CallVoidMethod(conn, env->GetMethodID(httpClass, "setConnectTimeout", "(I)V"), 15000);
    env->CallVoidMethod(conn, env->GetMethodID(httpClass, "setRequestProperty", "(Ljava/lang/String;Ljava/lang/String;)V"),
                        env->NewStringUTF("User-Agent"), env->NewStringUTF("MDViewer/1.0"));

    response.status = env->CallIntMethod(conn, env->GetMethodID(httpClass, "getResponseCode", "()I"));
    if (env.clearException())
        return fail("Connection failed");

    response.contentType = toStdString(
        env.get(), static_cast<jstring>(env->CallObjectMethod(conn, env->GetMethodID(httpClass, "getContentType", "()Ljava/lang/String;"))));
    env.clearException();

    const char* streamGetter = response.status >= 400 ? "getErrorStream" : "getInputStream";
    jobject stream = env->CallObjectMethod(conn, env->GetMethodID(httpClass, streamGetter, "()Ljava/io/InputStream;"));
    if (env.clearException())
        stream = nullptr;

    if (stream) {
        jclass streamClass = env->FindClass("java/io/InputStream");
        jmethodID read = env->GetMethodID(streamClass, "read", "([B)I");
        jbyteArray buffer = env->NewByteArray(16 * 1024);
        for (;;) {
            jint n = env->CallIntMethod(stream, read, buffer);
            if (env.clearException()) {
                response.error = "Read failed";
                break;
            }
            if (n <= 0)
                break;
            size_t offset = response.body.size();
            response.body.resize(offset + size_t(n));
            env->GetByteArrayRegion(buffer, 0, n, reinterpret_cast<jbyte*>(response.body.data() + offset));
        }
        env->CallVoidMethod(stream, env->GetMethodID(streamClass, "close", "()V"));
        env.clearException();
    }
    env->CallVoidMethod(conn, env->GetMethodID(httpClass, "disconnect", "()V"));
    env.clearException();
    env->PopLocalFrame(nullptr);
    return response;
}

} // namespace mdv::os::detail
