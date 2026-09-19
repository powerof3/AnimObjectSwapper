#include "RNG.h"

std::uint64_t AOS_RNG::get_form_seed(const RE::TESForm* a_form)
{
	if (a_form->IsDynamicForm()) {
		return a_form->GetFormID();
	}

	std::uint64_t result = 0;
	boost::hash_combine(result, a_form->GetLocalFormID());

	auto fileName = a_form->GetFile(0)->GetFilename();
	if (a_form->AsReference() && (a_form->GetFormID() & 0xFF000000) == 0) {
		fileName = "Skyrim.esm"sv;
	}
	boost::hash_combine(result, fileName);

	return result;
}

AOS_RNG::AOS_RNG(const Chance& a_chance, const RE::Actor* a_actor, const RE::TESObjectANIO* a_animObject, std::uint64_t a_entryHash) :
	type(a_chance.chanceType)
{
	const auto make_seed = [&](std::uint64_t a_seed) {
		std::uint64_t result = a_seed;
		if (a_animObject) {
			boost::hash_combine(result, get_form_seed(a_animObject));
		}
		boost::hash_combine(result, a_entryHash);
		boost::hash_combine(result, a_chance.seed);
		return result;
	};

	switch (type) {
	case CHANCE_TYPE::kActorHash:
		{
			if (a_actor) {
				seed = make_seed(get_form_seed(a_actor));
			} else {
				type = CHANCE_TYPE::kRandom;
				seed = a_chance.seed != 0 ? make_seed(0) : 0;
			}
		}
		break;
	case CHANCE_TYPE::kLocationHash:
		{
			const RE::TESForm* locOrCell = nullptr;
			if (a_actor) {
				if (const auto location = a_actor->GetCurrentLocation()) {
					locOrCell = location;
				} else {
					locOrCell = a_actor->GetParentCell();
				}
			}
			if (locOrCell && a_animObject) {
				seed = make_seed(get_form_seed(locOrCell));
			} else if (a_actor) {
				seed = make_seed(get_form_seed(a_actor));
			} else {
				type = CHANCE_TYPE::kRandom;
				seed = a_chance.seed != 0 ? make_seed(0) : 0;
			}
		}
		break;
	case CHANCE_TYPE::kRandom:
		{
			if (a_chance.seed != 0) {
				seed = make_seed(a_actor ? get_form_seed(a_actor) : 0);
			} else {
				seed = 0;
			}
		}
		break;
	default:
		break;
	}
}

AOS_RNG::AOS_RNG(const Chance& a_chance) :
	type(a_chance.chanceType),
	seed(a_chance.seed)
{}

Chance::Chance(const std::string& a_str)
{
	if (distribution::is_valid_entry(a_str)) {
		if (a_str.contains("chance")) {
			if (a_str.contains("chanceR")) {
				chanceType = CHANCE_TYPE::kRandom;
			} else if (a_str.contains("chanceL")) {
				chanceType = CHANCE_TYPE::kLocationHash;
			} else {
				chanceType = CHANCE_TYPE::kActorHash;
			}

			if (boost::cmatch match; boost::regex_search(a_str.c_str(), match, regex::generic)) {
				if (const auto chanceOptions = REX::STR::SPLIT(match[1].str(), ","); !chanceOptions.empty()) {
					chanceValue = REX::STR::TO_NUM<float>(chanceOptions[0]);
					seed = chanceOptions.size() > 1 ? REX::STR::TO_NUM<std::uint64_t>(chanceOptions[1]) : 0;
				}
			}
		}
	}
}

bool Chance::PassedChance(const RE::Actor* a_actor, const RE::TESObjectANIO* a_animObject, std::uint64_t a_entryHash) const
{
	if (chanceValue < 100.0f) {
		const AOS_RNG rng(*this, a_actor, a_animObject, a_entryHash);
		if (const auto rngValue = rng.generate<float>(0.0f, 100.0f); rngValue > chanceValue) {
			return false;
		}
	}
	return true;
}
