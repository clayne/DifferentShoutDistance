extern "C" DLLEXPORT bool SKSEAPI SKSEPlugin_Query(const SKSE::QueryInterface* a_skse, SKSE::PluginInfo* a_info)
{
#ifndef DEBUG
	auto sink = std::make_shared<spdlog::sinks::msvc_sink_mt>();
#else
	auto path = logger::log_directory();
	if (!path) {
		return false;
	}

	*path /= Version::PROJECT;
	*path += ".log"sv;
	auto sink = std::make_shared<spdlog::sinks::basic_file_sink_mt>(path->string(), true);
#endif

	auto log = std::make_shared<spdlog::logger>("global log"s, std::move(sink));

#ifndef DEBUG
	log->set_level(spdlog::level::trace);
#else
	log->set_level(spdlog::level::info);
	log->flush_on(spdlog::level::info);
#endif

	spdlog::set_default_logger(std::move(log));
	spdlog::set_pattern("%g(%#): [%^%l%$] %v"s);

	logger::info(FMT_STRING("{} v{}"), Version::PROJECT, Version::NAME);

	a_info->infoVersion = SKSE::PluginInfo::kVersion;
	a_info->name = Version::PROJECT.data();
	a_info->version = Version::MAJOR;

	if (a_skse->IsEditor()) {
		logger::critical("Loaded in editor, marking as incompatible"sv);
		return false;
	}

	const auto ver = a_skse->RuntimeVersion();
	if (ver < SKSE::RUNTIME_1_5_39) {
		logger::critical(FMT_STRING("Unsupported runtime version {}"), ver.string());
		return false;
	}

	return true;
}

class SetNewTypeHook
{
public:
	static void Hook()
	{
		_Usage1 = SKSE::GetTrampoline().write_call<5>(REL::ID(13629).address() + 0x130,
			CreateProjectile_14074B170_1);  // SkyrimSE.exe+16CDD0 -- placeatme
		_Usage2 = SKSE::GetTrampoline().write_call<5>(REL::ID(17693).address() + 0xe82,
			CreateProjectile_14074B170_2);  // SkyrimSE.exe+2360C2 -- TESObjectWEAP::Fire_140235240
		_Usage3 = SKSE::GetTrampoline().write_call<5>(REL::ID(33672).address() + 0x377,
			CreateProjectile_14074B170_3);  // SkyrimSE.exe+550A37 -- ActorMagicCaster::castProjectile
		_Usage4 = SKSE::GetTrampoline().write_call<5>(REL::ID(35450).address() + 0x20e,
			CreateProjectile_14074B170_4);  // SkyrimSE.exe+5A897E -- ChainExplosion
	}

private:
	static void set_custom_type(uint32_t handle)
	{
		RE::TESObjectREFRPtr _refr;
		RE::LookupReferenceByHandle(handle, _refr);
		if (auto proj = _refr.get() ? _refr.get()->As<RE::Projectile>() : nullptr) {
			//logger::info("{}", proj->range);
			proj->range = 500;
		}
	}

	static uint32_t* CreateProjectile_14074B170_1(uint32_t* handle, void* ldata)
	{
		handle = _Usage1(handle, ldata);
		set_custom_type(*handle);
		return handle;
	}
	static uint32_t* CreateProjectile_14074B170_2(uint32_t* handle, void* ldata)
	{
		handle = _Usage2(handle, ldata);
		set_custom_type(*handle);
		return handle;
	}
	static uint32_t* CreateProjectile_14074B170_3(uint32_t* handle, void* ldata)
	{
		handle = _Usage3(handle, ldata);
		set_custom_type(*handle);
		return handle;
	}
	static uint32_t* CreateProjectile_14074B170_4(uint32_t* handle, void* ldata)
	{
		handle = _Usage4(handle, ldata);
		set_custom_type(*handle);
		return handle;
	}

	using func_type = decltype(CreateProjectile_14074B170_1);

	static inline REL::Relocation<func_type> _Usage1;
	static inline REL::Relocation<func_type> _Usage2;
	static inline REL::Relocation<func_type> _Usage3;
	static inline REL::Relocation<func_type> _Usage4;
};

static void SKSEMessageHandler(SKSE::MessagingInterface::Message* message)
{
	switch (message->type) {
	case SKSE::MessagingInterface::kDataLoaded:
		SetNewTypeHook::Hook();

		break;
	}
}

extern "C" DLLEXPORT bool SKSEAPI SKSEPlugin_Load(const SKSE::LoadInterface* a_skse)
{
	auto g_messaging = reinterpret_cast<SKSE::MessagingInterface*>(a_skse->QueryInterface(SKSE::LoadInterface::kMessaging));
	if (!g_messaging) {
		logger::critical("Failed to load messaging interface! This error is fatal, plugin will not load.");
		return false;
	}

	logger::info("loaded");

	SKSE::Init(a_skse);
	SKSE::AllocTrampoline(1 << 10);

	g_messaging->RegisterListener("SKSE", SKSEMessageHandler);

	return true;
}
