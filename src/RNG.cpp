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

AOS_RNG::AOS_RNG(const Chance& a_chance, const RE::Actor* a_actor) :
	type(a_chance.chanceType)
{
	switch (type) {
	case CHANCE_TYPE::kActorHash:
		seed = get_form_seed(a_actor);
		break;
	case CHANCE_TYPE::kRandom:
		seed = a_chance.seed;
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
			if (a_str.contains("S")) {
				chanceType = CHANCE_TYPE::kActorHash;
			} else {
				chanceType = CHANCE_TYPE::kRandom;
			}

			if (boost::cmatch match; boost::regex_search(a_str.c_str(), match, regex::generic)) {
				const auto chanceOptions = REX::STR::SPLIT(match[1].str(), ",");
				chanceValue = REX::STR::TO_NUM<float>(chanceOptions[0]);
				seed = chanceOptions.size() > 1 ? REX::STR::TO_NUM<std::uint64_t>(chanceOptions[1]) : 0;
			}
		}
	}
}

bool Chance::PassedChance(const RE::Actor* a_actor) const
{
	if (chanceValue < 100.0f) {
		const AOS_RNG rng(*this, a_actor);
		if (const auto rngValue = rng.generate<float>(0.0f, 100.0f); rngValue > chanceValue) {
			return false;
		}
	}
	return true;
}
