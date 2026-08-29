#include "SwapData.h"

namespace AnimObjectSwap
{
	SwapAnioData::SwapAnioData(FormIDOrSet a_id, const Input& a_input) :
		formIDSet(std::move(a_id)),
		chance(a_input.chance),
		record(a_input.record),
		path(a_input.path)
	{}

	RE::TESObjectANIO* SwapAnioData::GetSwapAnio(const RE::Actor* a_actor) const
	{
		if (!chance.PassedChance(a_actor)) {
			return nullptr;
		}

		if (const auto formID = std::get_if<RE::FormID>(&formIDSet); formID) {
			return RE::TESForm::LookupByID<RE::TESObjectANIO>(*formID);
		} else {  // return random element from set
			auto&      set = std::get<FormIDSet>(formIDSet);
			const auto randIt = AOS_RNG(chance, a_actor).generate<std::size_t>(0, set.size() - 1);
			return RE::TESForm::LookupByID<RE::TESObjectANIO>(set[randIt]);
		}
	}

	void SwapAnioData::GetForms(const std::string& a_path, const std::string& a_str, std::function<void(RE::FormID, SwapAnioData&)> a_func)
	{
		constexpr auto swap_empty = [](const FormIDOrSet& a_set) {
			if (const auto formID = std::get_if<RE::FormID>(&a_set); formID) {
				return *formID == 0;
			} else {
				return std::get<FormIDSet>(a_set).empty();
			}
		};

		constexpr auto base_same_as_swap = [](RE::FormID a_baseID, const FormIDOrSet& a_set) {
			if (const auto formID = std::get_if<RE::FormID>(&a_set); formID) {
				return *formID == a_baseID;
			} else {
				return false;
			}
		};

		const auto formPair = REX::STR::SPLIT(a_str, "|");
		if (formPair.size() < 2) {
			REX::ERROR("\t\t\t\tfail : [{}] (invalid entry)", a_str);
			return;
		}

		if (const auto baseFormID = util::GetANIOFormID(formPair[0]); baseFormID != 0) {
			if (const auto swapFormID = util::GetSwapFormID(formPair[1]); !swap_empty(swapFormID)) {
				if (base_same_as_swap(baseFormID, swapFormID)) {
					REX::ERROR("\t\t\t\tfail : [{}] (BASE formID == SWAP formID)", a_str);
					return;
				}

				const Input input(
					formPair.size() > 2 ? formPair[2] : std::string{},  // chance
					a_str,
					a_path);
				SwapAnioData swapAnioData(swapFormID, input);

				a_func(baseFormID, swapAnioData);
			} else {
				REX::ERROR("\t\t\t\tfail : [{}] (SWAP formID not found or not an AnimObject)", a_str);
			}
		} else {
			REX::ERROR("\t\t\t\tfail : [{}] (BASE formID not found or not an AnimObject)", a_str);
		}
	}
}
