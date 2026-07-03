#include "napi/native_api.h"
#include "hilog/log.h"
#include <string>
#include <deque>
#include <vector>
#include <mutex>

struct LogItem {
    LogType type;
    LogLevel level;
    unsigned int domain;
    std::string tag;
    std::string msg;
};

// on() 传入的过滤条件，has* 标记该维度是否参与过滤
struct LogFilter {
    bool hasLevel = false;
    int32_t level = 0;
    bool hasDomain = false;
    uint32_t domain = 0;
    bool hasTag = false;
    std::string tag;
    bool hasKeyword = false;
    std::string keyword;
};

struct Listener {
    napi_threadsafe_function tsfn = nullptr;
    napi_ref cbRef = nullptr;   // 保存回调引用，供 off() 按函数身份匹配
    LogFilter filter;
};

static std::mutex g_mutex;
static std::deque<LogItem> gLogCache;
static const size_t MAX_LOG_CNT = 500;
static std::vector<Listener> g_listeners;
static std::string g_ignoreTag = "$$LOG_CAPTURE";
static bool g_capturing = false;

static napi_value LogItemToJs(napi_env env, const LogItem& item)
{
    napi_value obj;
    napi_create_object(env, &obj);

    napi_value vType, vLevel, vDomain, vTag, vMsg;
    napi_create_int32(env, static_cast<int32_t>(item.type), &vType);
    napi_create_int32(env, static_cast<int32_t>(item.level), &vLevel);
    napi_create_uint32(env, item.domain, &vDomain);
    napi_create_string_utf8(env, item.tag.c_str(), NAPI_AUTO_LENGTH, &vTag);
    napi_create_string_utf8(env, item.msg.c_str(), NAPI_AUTO_LENGTH, &vMsg);

    napi_set_named_property(env, obj, "type", vType);
    napi_set_named_property(env, obj, "level", vLevel);
    napi_set_named_property(env, obj, "domain", vDomain);
    napi_set_named_property(env, obj, "tag", vTag);
    napi_set_named_property(env, obj, "msg", vMsg);
    return obj;
}

// 判断一条日志是否满足某个监听器的过滤条件
static bool MatchFilter(const LogFilter& f, const LogItem& item)
{
    if (f.hasLevel && static_cast<int32_t>(item.level) != f.level) {
        return false;
    }
    if (f.hasDomain && item.domain != f.domain) {
        return false;
    }
    if (f.hasTag && item.tag != f.tag) {
        return false;
    }
    if (f.hasKeyword && item.msg.find(f.keyword) == std::string::npos) {
        return false;
    }
    return true;
}

// 运行在 JS 线程：把日志转成 JS 对象并调用监听回调
static void CallJs(napi_env env, napi_value jsCallback, void* /*context*/, void* data)
{
    LogItem* item = static_cast<LogItem*>(data);
    if (env != nullptr && jsCallback != nullptr && item != nullptr) {
        napi_value undefined;
        napi_get_undefined(env, &undefined);
        napi_value arg = LogItemToJs(env, *item);
        napi_call_function(env, undefined, jsCallback, 1, &arg, nullptr);
    }
    delete item;
}

// 运行在 hilog 内部线程：只做拷贝、缓存与投递，不得调用 napi 接口
static void LogCaptureCallback(const LogType type,
                               const LogLevel level,
                               const unsigned int domain,
                               const char* tag,
                               const char* msg)
{
    std::lock_guard<std::mutex> lock(g_mutex);

    std::string tagStr = tag ? std::string(tag) : "";
    // 命中 ignoreTag 的日志直接丢弃：既不缓存也不通知，用于打断回调内打日志的循环
    if (!g_ignoreTag.empty() && tagStr == g_ignoreTag) {
        return;
    }

    // 只处理本应用 domain 范围 [0x0000, 0xFFFF]，其余（系统/三方）直接舍弃
    if (domain < 0x0000 || domain > 0xFFFF) {
        return;
    }

    LogItem item;
    item.type = type;
    item.level = level;
    item.domain = domain;
    item.tag = tagStr;
    item.msg = msg ? std::string(msg) : "";

    gLogCache.push_back(item);
    if (gLogCache.size() > MAX_LOG_CNT) {
        gLogCache.pop_front();
    }

    // 逐个匹配监听器的过滤条件，命中的才投递回 JS 线程
    for (const Listener& l : g_listeners) {
        if (l.tsfn == nullptr || !MatchFilter(l.filter, item)) {
            continue;
        }
        LogItem* payload = new LogItem(item);
        napi_status status = napi_call_threadsafe_function(l.tsfn, payload, napi_tsfn_nonblocking);
        if (status != napi_ok) {
            delete payload;
        }
    }
}

static napi_value FetchLogs(napi_env env, napi_callback_info info)
{
    // 可选参数 clear：为 true 时读取后清空缓冲区，默认 false（非破坏性读取）
    size_t argc = 1;
    napi_value args[1] = { nullptr };
    napi_get_cb_info(env, info, &argc, args, nullptr, nullptr);
    bool clear = false;
    if (argc >= 1 && args[0] != nullptr) {
        napi_valuetype vt = napi_undefined;
        napi_typeof(env, args[0], &vt);
        if (vt == napi_boolean) {
            napi_get_value_bool(env, args[0], &clear);
        }
    }

    std::lock_guard<std::mutex> lock(g_mutex);
    napi_value arr;
    napi_create_array(env, &arr);
    for (size_t i = 0; i < gLogCache.size(); i++) {
        napi_set_element(env, arr, i, LogItemToJs(env, gLogCache[i]));
    }
    if (clear) {
        gLogCache.clear();
    }
    return arr;
}

static napi_value ClearLogs(napi_env env, napi_callback_info info)
{
    std::lock_guard<std::mutex> lock(g_mutex);
    gLogCache.clear();
    return nullptr;
}

// 读取字符串属性
static bool GetStringProp(napi_env env, napi_value obj, const char* key, std::string& out)
{
    napi_value v = nullptr;
    if (napi_get_named_property(env, obj, key, &v) != napi_ok || v == nullptr) {
        return false;
    }
    napi_valuetype vt = napi_undefined;
    napi_typeof(env, v, &vt);
    if (vt != napi_string) {
        return false;
    }
    size_t len = 0;
    napi_get_value_string_utf8(env, v, nullptr, 0, &len);
    out.resize(len);
    napi_get_value_string_utf8(env, v, &out[0], len + 1, &len);
    return true;
}

// 读取整数属性（level / domain 允许 number 传入）
static bool GetInt32Prop(napi_env env, napi_value obj, const char* key, int32_t& out)
{
    napi_value v = nullptr;
    if (napi_get_named_property(env, obj, key, &v) != napi_ok || v == nullptr) {
        return false;
    }
    napi_valuetype vt = napi_undefined;
    napi_typeof(env, v, &vt);
    if (vt != napi_number) {
        return false;
    }
    napi_get_value_int32(env, v, &out);
    return true;
}

// 从 JS 选项对象解析过滤条件
static LogFilter ParseFilter(napi_env env, napi_value optObj)
{
    LogFilter f;
    if (optObj == nullptr) {
        return f;
    }
    napi_valuetype vt = napi_undefined;
    napi_typeof(env, optObj, &vt);
    if (vt != napi_object) {
        return f;
    }
    int32_t iv = 0;
    if (GetInt32Prop(env, optObj, "level", iv)) {
        f.hasLevel = true;
        f.level = iv;
    }
    if (GetInt32Prop(env, optObj, "domain", iv)) {
        f.hasDomain = true;
        f.domain = static_cast<uint32_t>(iv);
    }
    if (GetStringProp(env, optObj, "tag", f.tag)) {
        f.hasTag = true;
    }
    if (GetStringProp(env, optObj, "keyword", f.keyword)) {
        f.hasKeyword = true;
    }
    return f;
}

// setCapture(enable: boolean, ignoreTag?: string) 控制底层日志回调的开关
static napi_value SetCapture(napi_env env, napi_callback_info info)
{
    size_t argc = 2;
    napi_value args[2] = { nullptr, nullptr };
    napi_get_cb_info(env, info, &argc, args, nullptr, nullptr);

    bool enable = false;
    if (argc >= 1 && args[0] != nullptr) {
        napi_get_value_bool(env, args[0], &enable);
    }

    bool stateChanged = false;
    std::string ignoreTag;
    {
        std::lock_guard<std::mutex> lock(g_mutex);
        if (argc >= 2 && args[1] != nullptr) {
            napi_valuetype vt = napi_undefined;
            napi_typeof(env, args[1], &vt);
            if (vt == napi_string) {
                std::string tag;
                size_t len = 0;
                napi_get_value_string_utf8(env, args[1], nullptr, 0, &len);
                tag.resize(len);
                napi_get_value_string_utf8(env, args[1], &tag[0], len + 1, &len);
                g_ignoreTag = tag;
            }
        }

        if (enable && !g_capturing) {
            OH_LOG_SetCallback(LogCaptureCallback);
            g_capturing = true;
            stateChanged = true;
        } else if (!enable && g_capturing) {
            OH_LOG_SetCallback(nullptr);
            g_capturing = false;
            stateChanged = true;
        }
        ignoreTag = g_ignoreTag;
    }

    // 必须在释放锁之后打印：日志打印会同步触发 LogCaptureCallback，
    // 而回调内部同样要获取 g_mutex，持锁打印会造成自死锁（LIFECYCLE_TIMEOUT）。
    // 注意：这里用 OH_LOG_Print（宏，映射到 __OH_LOG_Print）而非 OH_LOG_PrintMsg。
    // 部分真机的 libhilog_ndk.z.so 未导出 OH_LOG_PrintMsg，链接该符号会导致
    // 整个 .so 在设备上 relocating failed，模块加载为 undefined。
    if (stateChanged) {
        OH_LOG_Print(LOG_APP, LOG_INFO, 0xFF00, ignoreTag.c_str(),
                     "log capture %{public}s, ignoreTag=%{public}s",
                     enable ? "enabled" : "disabled", ignoreTag.c_str());
    }
    return nullptr;
}

// on(cb, options?) 添加一个监听器，可多次调用添加多个
static napi_value On(napi_env env, napi_callback_info info)
{
    size_t argc = 2;
    napi_value args[2] = { nullptr, nullptr };
    napi_get_cb_info(env, info, &argc, args, nullptr, nullptr);
    if (argc < 1 || args[0] == nullptr) {
        return nullptr;
    }

    Listener listener;
    listener.filter = ParseFilter(env, argc >= 2 ? args[1] : nullptr);
    napi_create_reference(env, args[0], 1, &listener.cbRef);

    napi_value resourceName;
    napi_create_string_utf8(env, "logCaptureListener", NAPI_AUTO_LENGTH, &resourceName);
    napi_create_threadsafe_function(env, args[0], nullptr, resourceName, 0, 1,
                                    nullptr, nullptr, nullptr, CallJs, &listener.tsfn);

    std::lock_guard<std::mutex> lock(g_mutex);
    g_listeners.push_back(listener);
    return nullptr;
}

// 释放单个监听器持有的资源
static void ReleaseListener(napi_env env, Listener& l)
{
    if (l.tsfn != nullptr) {
        napi_release_threadsafe_function(l.tsfn, napi_tsfn_release);
        l.tsfn = nullptr;
    }
    if (l.cbRef != nullptr) {
        napi_delete_reference(env, l.cbRef);
        l.cbRef = nullptr;
    }
}

// off(cb?) 移除指定监听；不传参数则移除全部
static napi_value Off(napi_env env, napi_callback_info info)
{
    size_t argc = 1;
    napi_value cb = nullptr;
    napi_get_cb_info(env, info, &argc, &cb, nullptr, nullptr);

    std::lock_guard<std::mutex> lock(g_mutex);

    bool removeAll = (argc < 1 || cb == nullptr);
    for (auto it = g_listeners.begin(); it != g_listeners.end();) {
        bool match = removeAll;
        if (!removeAll) {
            napi_value registered = nullptr;
            napi_get_reference_value(env, it->cbRef, &registered);
            bool eq = false;
            if (registered != nullptr) {
                napi_strict_equals(env, registered, cb, &eq);
            }
            match = eq;
        }
        if (match) {
            ReleaseListener(env, *it);
            it = g_listeners.erase(it);
        } else {
            ++it;
        }
    }
    return nullptr;
}

// getIgnoreTag() 返回当前的 ignoreTag，供 JS 侧打日志时复用
static napi_value GetIgnoreTag(napi_env env, napi_callback_info info)
{
    std::lock_guard<std::mutex> lock(g_mutex);
    napi_value result;
    napi_create_string_utf8(env, g_ignoreTag.c_str(), NAPI_AUTO_LENGTH, &result);
    return result;
}

EXTERN_C_START
static napi_value Init(napi_env env, napi_value exports) {
    napi_property_descriptor desc[] = {
        { "setCapture", nullptr, SetCapture, nullptr, nullptr, nullptr, napi_default, nullptr },
        { "getIgnoreTag", nullptr, GetIgnoreTag, nullptr, nullptr, nullptr, napi_default, nullptr },
        { "on", nullptr, On, nullptr, nullptr, nullptr, napi_default, nullptr },
        { "off", nullptr, Off, nullptr, nullptr, nullptr, napi_default, nullptr },
        { "fetchLogs", nullptr, FetchLogs, nullptr, nullptr, nullptr, napi_default, nullptr },
        { "clearLogs", nullptr, ClearLogs, nullptr, nullptr, nullptr, napi_default, nullptr },
    };
    napi_define_properties(env, exports, sizeof(desc) / sizeof(desc[0]), desc);
    return exports;
}
EXTERN_C_END

NAPI_MODULE(logcapture, Init)
