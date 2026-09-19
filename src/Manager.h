#pragma once

#include "SwapData.h"

namespace AnimObjectSwap
{
	class Manager : public REX::TSingleton<Manager>
	{
	public:
		void LoadForms();

		RE::TESObjectANIO* GetSwappedAnimObject(RE::TESObjectREFR* a_user, RE::TESObjectANIO* a_animObject);

	private:
		RE::TESObjectANIO* GetSwappedAnimObjectConditional(RE::Actor* a_actor, const RE::TESObjectANIO* a_animObject) const;

		// members
		FormIDMap<SwapAnioDataVec>         swapAnimObjects{};
		FormIDMap<SwapAnioDataConditional> swapAnimObjectsConditional{};
	};
}
