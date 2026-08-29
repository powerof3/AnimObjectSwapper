#pragma once

#include "ConditionalData.h"
#include "RNG.h"

namespace AnimObjectSwap
{
	class SwapAnioData
	{
	public:
		struct Input
		{
			std::string chance;
			std::string record;
			std::string path;
		};

		SwapAnioData() = delete;
		SwapAnioData(FormIDOrSet a_id, const Input& a_input);

		RE::TESObjectANIO* GetSwapAnio(const RE::Actor* a_actor) const;
		static void        GetForms(const std::string& a_path, const std::string& a_str, std::function<void(RE::FormID, SwapAnioData&)> a_func);

		// members
		FormIDOrSet formIDSet{};
		Chance      chance{};

		// used for logging conflicts
		std::string record{};
		std::string path{};
	};

	using SwapAnioDataVec = std::vector<SwapAnioData>;
	using SwapAnioDataConditional = ConditionalData<SwapAnioData>;
}
