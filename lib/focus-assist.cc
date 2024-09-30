#include <napi.h>

enum API_RESULT
{
  NOT_SUPPORTED = -2,
  FAILED = -1,
  Off = 0,
  PRIORITY_ONLY = 1,
  ALARMS_ONLY = 2
};

enum PRIORITY_RESULT
{
  NO = 0,
  YES = 1
};

#ifdef _WIN32
  #include <atlbase.h>
  #include "quiethours_h.h"

  #pragma comment(lib, "ntdll.lib")

  #define NT_SUCCESS_STATUS(Status) (((NTSTATUS)(Status)) >= 0)

  const wchar_t* ALARMS_ONLY_ID = L"Microsoft.QuietHoursProfile.AlarmsOnly";
  const wchar_t* PRIORITY_ONLY_ID = L"Microsoft.QuietHoursProfile.PriorityOnly";
  const wchar_t* UNRESTRICTED_ID = L"Microsoft.QuietHoursProfile.Unrestricted";

  typedef struct _WNF_STATE_NAME
  {
    ULONG Data[2];
  } WNF_STATE_NAME, *PWNF_STATE_NAME;

  typedef struct _WNF_TYPE_ID
  {
    GUID TypeId;
  } WNF_TYPE_ID, *PWNF_TYPE_ID;

  typedef const WNF_TYPE_ID *PCWNF_TYPE_ID;
  typedef ULONG WNF_CHANGE_STAMP, *PWNF_CHANGE_STAMP;

  extern "C"
  NTSTATUS
  NTAPI
  NtQueryWnfStateData(
    _In_ PWNF_STATE_NAME StateName,
    _In_opt_ PWNF_TYPE_ID TypeId,
    _In_opt_ const VOID* ExplicitScope,
    _Out_ PWNF_CHANGE_STAMP ChangeStamp,
    _Out_writes_bytes_to_opt_(*BufferSize, *BufferSize) PVOID Buffer,
    _Inout_ PULONG BufferSize);

  LPWSTR stdStringToLPWSTR(const char *input)
  {
    const size_t cSize = strlen(input) + 1;
    wchar_t* wc = new wchar_t[cSize];
    mbstowcs(wc, input, cSize);
    return wc;
  }

  bool IsFailed(HRESULT hr)
  {
    return FAILED(hr);
  }

  int InternalGetFocusAssistWNF()
  {
    WNF_STATE_NAME WNF_SHEL_QUIETHOURS_ACTIVE_PROFILE_CHANGED{ 0xA3BF1C75, 0xD83063E };

    WNF_CHANGE_STAMP unused_change_stamp{};
    DWORD buffer = 0;
    ULONG buffer_size = sizeof(buffer);
    int status = -1;

    if (NT_SUCCESS_STATUS(::NtQueryWnfStateData(&WNF_SHEL_QUIETHOURS_ACTIVE_PROFILE_CHANGED, nullptr, nullptr, &unused_change_stamp, &buffer, &buffer_size)))
    {
      if (buffer >= 0 || buffer <= 2)
      {
        status = buffer;
      }
    }
    else
    {
      status = API_RESULT::FAILED;
    }

    return status;
  }

  int InternalGetFocusAssistCOM()
  {
    if (IsFailed(CoInitialize(nullptr))) { return API_RESULT::FAILED; }

    CComPtr<IQuietHoursSettings> quietHoursSettings;
    if (IsFailed(CoCreateInstance(CLSID_QuietHoursSettings, nullptr, CLSCTX_LOCAL_SERVER, IID_PPV_ARGS(&quietHoursSettings)))) { return API_RESULT::FAILED; }

    CComHeapPtr<wchar_t> profileId;
    if (IsFailed(quietHoursSettings->get_UserSelectedProfile(&profileId))) { return API_RESULT::FAILED; }

    const LPWSTR profileIdWS = static_cast<LPWSTR>(profileId);

    if (wcscmp(profileIdWS, PRIORITY_ONLY_ID) == 0)
    {
      return API_RESULT::PRIORITY_ONLY;
    }
    else if (wcscmp(profileIdWS, ALARMS_ONLY_ID) == 0)
    {
      return API_RESULT::ALARMS_ONLY;
    } 
    else
    {
      return API_RESULT::Off;
    }
  }

  int InternalIsPriority(const LPWSTR appUserModelId)
  {
    if (IsFailed(CoInitialize(nullptr))) { return API_RESULT::FAILED; }

    CComPtr<IQuietHoursSettings> quietHoursSettings;
    if (IsFailed(CoCreateInstance(CLSID_QuietHoursSettings, nullptr, CLSCTX_LOCAL_SERVER, IID_PPV_ARGS(&quietHoursSettings)))) { return API_RESULT::FAILED; }

    CComHeapPtr<wchar_t> profileId;
    if (IsFailed(quietHoursSettings->get_UserSelectedProfile(&profileId))) { return API_RESULT::FAILED; }


    std::wstring priorityOnlyIdWS(PRIORITY_ONLY_ID);
    LPWSTR lpPriorityOnlyIdWS = &priorityOnlyIdWS[0];

    CComPtr<IQuietHoursProfile> profile;
    if (IsFailed(quietHoursSettings->GetProfile(lpPriorityOnlyIdWS, &profile))) { return API_RESULT::FAILED; }

    uint32_t count = 0;
    {
      CComHeapPtr<LPWSTR> apps;
      if (IsFailed(profile->GetAllowedApps(&count, &apps))) { return API_RESULT::FAILED; }
      for (uint32_t i = 0; i < count; i++)
      {
        CComHeapPtr<wchar_t> aumid; // Wrapped for CoTaskMemFree on scope exit
        aumid.Attach(apps[i]);
        auto entry = static_cast<LPWSTR>(aumid);
        if (wcscmp(entry, appUserModelId) == 0)
          return PRIORITY_RESULT::YES;
      }
    }
    return PRIORITY_RESULT::NO;
  }
#endif

Napi::Number GetFocusAssist(const Napi::CallbackInfo& info) {
  Napi::Env env = info.Env();
  
  #ifdef _WIN32
    int status;
    try
    {
      status = InternalGetFocusAssistWNF();
    }
    catch (...)
    {
      status = API_RESULT::FAILED;
    }

    // In case one unofficial API fails us, lets try another one. 
    if (status == API_RESULT::FAILED)
    {
      try
      {
        status = InternalGetFocusAssistCOM();
      }
      catch (...)
      {
        status = API_RESULT::FAILED;
      }
    }

    auto message = Napi::Number::New(env, status);
  #else
    auto message = Napi::Number::New(env, API_RESULT::NOT_SUPPORTED);
  #endif

  return Napi::Number::New(env, message);
}

Napi::Number IsPriority(const Napi::CallbackInfo& info) {
  Napi::Env env = info.Env();

  #ifdef _WIN32
    if (info.Length() < 1 || !info[0].IsString())
    {
      Napi::Error::New(env, "Invalid arguments, expected argument is: AppUserModelId");
      return Napi::Number::New(env, -1);;
    }
    
    auto p0 = info[0].ToString();
    const char *p0Str = p0.As<Napi::String>().Utf8Value().c_str();
    const LPWSTR aumid = stdStringToLPWSTR(p0Str);
    
    int status;
    try
    {
      status = InternalIsPriority(aumid);
    }
    catch (...)
    {
      status = API_RESULT::FAILED;
    }
    auto message = Napi::Number::New(env, status);
  #else
    auto message = Napi::Number::New(env, API_RESULT::NOT_SUPPORTED);
  #endif

  return Napi::Number::New(env, message);
}

Napi::Object Init(Napi::Env env, Napi::Object exports) {
  exports.Set(Napi::String::New(env, "getFocusAssist"),
              Napi::Function::New(env, GetFocusAssist));

  exports.Set(Napi::String::New(env, "isPriority"),
              Napi::Function::New(env, IsPriority));

  return exports;
}

NODE_API_MODULE(focusassist, Init)