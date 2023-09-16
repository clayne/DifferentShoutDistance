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

// change it to 0 or 1 to toggle drawing
#define DRAW 1

// TODO: think of it
const float WIDTH = 100;

// How many time increase things
const float K = 3.0f;

class LongerShoutsHook
{
public:
	static void Hook()
	{
		auto& trmpl = SKSE::GetTrampoline();

		// SkyrimSE.exe+735595 -- ConeProjectile::UpdateImpl
		_update_moving = trmpl.write_call<5>(REL::ID(42624).address() + 0x155, update_moving);

		// SkyrimSE.exe+74C6A1 -- Projectile::Update
		trmpl.write_call<6>(REL::ID(42942).address() + 0x2d1, update3d);

		_UpdateSelectedDownwardPass =
			REL::Relocation<uintptr_t>(REL::ID(RE::VTABLE_BSFadeNode[0])).write_vfunc(0x2d, UpdateSelectedDownwardPass);

		// SkyrimSE.exe+16CDD0 -- placeatme
		_Usage1 = trmpl.write_call<5>(REL::ID(13629).address() + 0x130, CreateProjectile_14074B170_1);
		// SkyrimSE.exe+2360C2 -- TESObjectWEAP::Fire_140235240
		_Usage2 = trmpl.write_call<5>(REL::ID(17693).address() + 0xe82, CreateProjectile_14074B170_2);
		// SkyrimSE.exe+550A37 -- ActorMagicCaster::castProjectile
		_Usage3 = trmpl.write_call<5>(REL::ID(33672).address() + 0x377, CreateProjectile_14074B170_3);
		// SkyrimSE.exe+5A897E -- ChainExplosion
		_Usage4 = trmpl.write_call<5>(REL::ID(35450).address() + 0x20e, CreateProjectile_14074B170_4);
	}

private:
	static void set_custom_type(uint32_t handle)
	{
		RE::TESObjectREFRPtr _refr;
		RE::LookupReferenceByHandle(handle, _refr);
		if (auto proj = _refr.get() ? _refr.get()->As<RE::Projectile>() : nullptr) {
			proj->range *= K;
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

	using CreateProjectile_type = decltype(CreateProjectile_14074B170_1);

	static inline REL::Relocation<CreateProjectile_type> _Usage1;
	static inline REL::Relocation<CreateProjectile_type> _Usage2;
	static inline REL::Relocation<CreateProjectile_type> _Usage3;
	static inline REL::Relocation<CreateProjectile_type> _Usage4;

	// Decrease cone angle
	static void update_moving(RE::ConeProjectile* proj, float dtime)
	{
		_update_moving(proj, dtime);

		{
			float DistanceMoved_1C0 = proj->distanceMoved;
			// TODO: calculate
			const float cone_len = WIDTH;

			proj->coneAngleTangent = cone_len / DistanceMoved_1C0;
		}

#if DRAW
		{
			float DistanceMoved_1C0 = proj->distanceMoved;
			float cone_len = DistanceMoved_1C0 * proj->coneAngleTangent + proj->initialCollisionSphereRadius;
			draw_sphere(proj->GetPosition(), cone_len, 5, 0);

			RE::NiPoint3 cast_dir = proj->linearVelocity;
			cast_dir.Unitize();
			RE::NiPoint3 right_dir = RE::NiPoint3(0, 0, -1).UnitCross(cast_dir);
			if (right_dir.SqrLength() == 0)
				right_dir = { 1, 0, 0 };
			RE::NiPoint3 up_dir = right_dir.Cross(cast_dir);

			auto p = RE::PlayerCharacter::GetSingleton();
			draw_line0(p->GetPosition(), proj->GetPosition() + right_dir * cone_len);
			draw_line0(p->GetPosition(), proj->GetPosition() + up_dir * cone_len);
			draw_line0(p->GetPosition(), proj->GetPosition() - right_dir * cone_len);
			draw_line0(p->GetPosition(), proj->GetPosition() - up_dir * cone_len);
		}
#endif  // DRAW
	}

	static inline REL::Relocation<decltype(update_moving)> _update_moving;

	static float GetSomeTimer()
	{
		REL::Relocation<float*> timer{ RELOCATION_ID(514188, 0) };
		return *timer;
	}

	// Slow updating 1
	static void update3d(RE::ConeProjectile* proj)
	{
		if (auto node = proj->Get3D2()) {
			RE::NiUpdateData data;
			data.time = GetSomeTimer() / K;
			data.flags.set(RE::NiUpdateData::Flag::kDirty);
			_generic_foo_<68900, void(RE::NiAVObject * a1, RE::NiUpdateData & data)>::eval(node, data);
		}
	}

	// Slow updating 2
	static void UpdateSelectedDownwardPass(RE::BSFadeNode* node, RE::NiUpdateData& data, std::uint32_t a_arg2)
	{
		if (auto owner = node->GetUserData(); owner && owner->formType == RE::FormType::ProjectileCone) {
			data.time /= K;
		}

		return _UpdateSelectedDownwardPass(node, data, a_arg2);
	}

	static inline REL::Relocation<decltype(UpdateSelectedDownwardPass)> _UpdateSelectedDownwardPass;
};

#if DRAW
class DebugAPIHook
{
public:
	static void Hook() { _Update = REL::Relocation<uintptr_t>(REL::ID(RE::VTABLE_PlayerCharacter[0])).write_vfunc(0xad, Update); }

private:
	static void Update(RE::PlayerCharacter* a, float delta)
	{
		_Update(a, delta);

		SKSE::GetTaskInterface()->AddUITask([]() { DebugAPI_IMPL::DebugAPI::Update(); });
	}

	static inline REL::Relocation<decltype(Update)> _Update;
};
#endif  // DRAW

class TestHook
{
public:
	static void Hook()
	{
		_sub_140265990 = SKSE::GetTrampoline().write_call<5>(REL::ID(42638).address() + 0xed,
			sub_140265990);  // SkyrimSE.exe+736a9d
	}

private:
	static char sub_140265990(RE::TESObjectCELL* cell, RE::NiPoint3* pos, float* Z)
	{
		auto ans = _sub_140265990(cell, pos, Z);
		auto pos1 = *pos;
		pos1.z = *Z;
		draw_line<Colors::RED>(*pos, pos1);
		return ans;
	}

	static inline REL::Relocation<decltype(sub_140265990)> _sub_140265990;
};

static void SKSEMessageHandler(SKSE::MessagingInterface::Message* message)
{
	switch (message->type) {
	case SKSE::MessagingInterface::kDataLoaded:
		LongerShoutsHook::Hook();
		TestHook::Hook();

#if DRAW
		DebugAPIHook::Hook();
#endif  // DRAW

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
