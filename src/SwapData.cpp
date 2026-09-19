#include "SwapData.h"

namespace AnimObjectSwap
{
	std::uint64_t SwapAnioData::Input::GenerateHash() const
	{
		std::uint64_t seed = 0;
		boost::hash_combine(seed, record);
		if (path) {
			boost::hash_combine(seed, *path);
		}
		return seed;
	}

	SwapAnioData::SwapAnioData(FormIDOrSet a_id, const Input& a_input) :
		formIDSet(std::move(a_id)),
		chance(a_input.chance),
		entryHash(a_input.GenerateHash()),
		record(a_input.record),
		path(a_input.path)
	{}

	RE::TESObjectANIO* SwapAnioData::GetSwapAnio(const RE::Actor* a_actor, const RE::TESObjectANIO* a_animObject) const
	{
		RE::TESObjectANIO* anio = nullptr;

		AOS_RNG rng(chance);
		if (!chance.PassedChance(rng, a_actor, a_animObject, entryHash)) {
			return anio;
		}

		if (const auto formID = std::get_if<RE::FormID>(&formIDSet); formID) {
			anio = RE::TESForm::LookupByID<RE::TESObjectANIO>(*formID);
		} else {
			if (auto& set = std::get<FormIDSet>(formIDSet); !set.empty()) {  // return random element from set
				rng.Seed(a_actor, a_animObject, entryHash);
				anio = RE::TESForm::LookupByID<RE::TESObjectANIO>(set[rng.Generate<std::size_t>(0, set.size() - 1)]);
			}
		}

		return anio;
	}

	void SwapAnioData::GetForms(const ConfigPath& a_path, const std::string& a_str, std::function<void(RE::FormID, SwapAnioData&)> a_func)
	{
		constexpr auto swap_empty = [](const FormIDOrSet& a_set) {
			if (const auto formID = std::get_if<RE::FormID>(&a_set); formID) {
				return *formID == 0;
			}
			return std::get<FormIDSet>(a_set).empty();
		};

		const auto formPair = REX::STR::SPLIT(a_str, "|");
		if (formPair.size() < 2) {
			REX::ERROR("\t\t\t\tfail : [{}] (invalid entry)", a_str);
			return;
		}

		RNGParams chance(formPair.size() > 2 ? formPair[2] : std::string{});

		if (const auto baseFormID = util::GetANIOFormID(formPair[0]); baseFormID != 0) {
			if (const auto swapFormID = util::GetSwapFormID(formPair[1]); !swap_empty(swapFormID)) {
				const Input  input(chance, a_str, a_path);
				SwapAnioData swapAnioData(swapFormID, input);

				a_func(baseFormID, swapAnioData);
			} else {
				REX::ERROR("\t\t\t\tfail : [{}] (SWAP formID not found or not an AnimObject)", a_str);
			}
		} else if (const auto baseFormIDs = util::GetANIOFormIDOrderedSet(formPair[0]); !baseFormIDs.empty()) {
			if (auto swapFormIDs = util::GetANIOFormIDOrderedSet(formPair[1]); !swapFormIDs.empty()) {
				// assign each baseFormID the same swapFormID
				if (swapFormIDs.size() == 1) {
					const Input input(chance, a_str, a_path);

					const auto swapFormID = *swapFormIDs.begin();
					for (auto itBaseFormID : baseFormIDs) {
						SwapAnioData swapAnioData(swapFormID, input);
						a_func(itBaseFormID, swapAnioData);
					}
					// randomly assign each baseFormID to a unique swapFormID
				} else if (swapFormIDs.size() >= baseFormIDs.size()) {
					const Input input(chance, a_str, a_path);

					auto rng = AOS_RNG(chance, input.GenerateHash());
					for (auto itBaseFormID : baseFormIDs) {
						const auto setEnd = std::distance(swapFormIDs.begin(), swapFormIDs.end()) - 1;
						const auto randIt = rng.Generate<std::int64_t>(0, setEnd);
						if (auto swapFormID = swapFormIDs.extract(*std::next(swapFormIDs.begin(), randIt))) {
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
