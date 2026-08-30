#pragma once

#include "ConditionalData.h"
#include "RNG.h"

namespace AnimObjectSwap
{
	using ConfigPath = std::shared_ptr<std::string>;

	class SwapAnioData
	{
	public:
		struct Input
		{
			Chance      chance;
			std::string record;
			ConfigPath  path;
		};

		SwapAnioData() = delete;
		SwapAnioData(FormIDOrSet a_id, const Input& a_input);

		RE::TESObjectANIO* GetSwapAnio(const RE::Actor* a_actor, RE::TESObjectANIO* a_animObject) const;
		static void        GetForms(const ConfigPath& a_path, const std::string& a_str, std::function<void(RE::FormID, SwapAnioData&)> a_func);

		// members
		FormIDOrSet formIDSet{};
		Chance      chance{};

		// used for logging conflicts
		std::string record{};
		ConfigPath  path{};
	};

	using SwapAnioDataVec = std::vector<SwapAnioData>;
	using SwapAnioDataConditional = ConditionalData<SwapAnioData>;
}
