#include "SwapData.h"

namespace AnimObjectSwap
{
	SwapAnioData::SwapAnioData(FormIDOrSet a_id, const Input& a_input) :
		formIDSet(std::move(a_id)),
		chance(a_input.chance),
		record(a_input.record),
		path(a_input.path)
	{}

	RE::TESObjectANIO* SwapAnioData::GetSwapAnio(const RE::Actor* a_actor, RE::TESObjectANIO* a_animObject) const
	{
		RE::TESObjectANIO* anio = nullptr;
		
		if (!chance.PassedChance(a_actor, a_animObject)) {
			return anio;
		}

		if (const auto formID = std::get_if<RE::FormID>(&formIDSet); formID) {
			anio = RE::TESForm::LookupByID<RE::TESObjectANIO>(*formID);
		} else {
			if (auto& set = std::get<FormIDSet>(formIDSet); !set.empty()) {  // return random element from set
				const auto randIt = AOS_RNG(chance, a_actor, a_animObject).generate<std::size_t>(0, set.size() - 1);
				anio = RE::TESForm::LookupByID<RE::TESObjectANIO>(set[randIt]);
			}
		}

		return anio;
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
		} else if (const auto baseFormIDs = util::GetANIOFormIDOrderedSet(formPair[0]); !baseFormIDs.empty()) {
			if (auto swapFormIDs = util::GetANIOFormIDOrderedSet(formPair[1]); !swapFormIDs.empty()) {
				auto chance = formPair.size() > 2 ? formPair[2] : std::string{};

				// assign each baseFormID the same swapFormID
				if (swapFormIDs.size() == 1) {
					const auto swapFormID = *swapFormIDs.begin();
					for (auto itBaseFormID : baseFormIDs) {
						if (itBaseFormID == swapFormID) {
							REX::ERROR("\t\t\t\tfail : [{}] (BASE formID == SWAP formID)", a_str);
							continue;
						}
						const Input  input(chance, a_str, a_path);
						SwapAnioData swapAnioData(swapFormID, input);

						a_func(itBaseFormID, swapAnioData);
					}

					// randomly assign each baseFormID to a unique swapFormID
				} else if (swapFormIDs.size() >= baseFormIDs.size()) {
					const auto a_chance = Chance(chance);
					auto       a_rng = AOS_RNG(a_chance);

					for (auto itBaseFormID : baseFormIDs) {
						const auto setEnd = std::distance(swapFormIDs.begin(), swapFormIDs.end()) - 1;
						const auto randIt = a_rng.generate<std::int64_t>(0, setEnd);
						auto       swapFormID = swapFormIDs.extract(*std::next(swapFormIDs.begin(), randIt));
						if (swapFormID) {
							const Input  input(std::string{}, a_str, a_path);
							SwapAnioData swapAnioData(swapFormID.value(), input);

							a_func(itBaseFormID, swapAnioData);
						}
					}
				} else {
					REX::ERROR("\t\t\t\tfail : [{}] (SWAP formID set size must be 1 OR equal/greater than BASE formID set size)", a_str);
				}
			} else {
				REX::ERROR("\t\t\t\tfail : [{}] (SWAP formID set not found or not AnimObjects)", a_str);
			}
		} else {
			REX::ERROR("\t\t\t\tfail : [{}] (BASE formID not found or not an AnimObject)", a_str);
		}
	}
}
