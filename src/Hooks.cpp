#include "Hooks.h"
#include "Manager.h"

namespace AnimObjectSwap::Hooks
{
	struct LoadAndAttachAddon
	{
		static RE::NiAVObject* thunk(RE::TESModel* a_model, RE::BIPED_OBJECT a_bipedObj, RE::TESObjectREFR* a_actor, RE::BSTSmartPointer<RE::BipedAnim>& a_biped, RE::NiAVObject* a_root)
		{
			RE::TESModel* model = a_model;

			if (const auto animObject = stl::adjust_pointer<RE::TESObjectANIO>(a_model->GetAsModelTextureSwap(), -0x20); animObject) {
				if (const auto swappedAnimObject = Manager::GetSingleton()->GetSwappedAnimObject(a_actor, animObject)) {
					model = swappedAnimObject;
				}
			}

			return func(model, a_bipedObj, a_actor, a_biped, a_root);
		}
		static inline REL::Relocation<decltype(thunk)> func;
	};

	void Install()
	{
		REL::Relocation<std::uintptr_t> target{ RELOCATION_ID(42420, 43576), OFFSET(0x22A, 0x21F) };  //AnimationObjects::Load
		stl::write_thunk_call<LoadAndAttachAddon>(target.address());

	    logger::info("Installed LoadAndAttachAddon hook"sv);
	}
}
