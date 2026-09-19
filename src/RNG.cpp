#include "RNG.h"

AOS_RNG::AOS_RNG(const RNGBase& a_base) :
	RNGBase(a_base)
{}

AOS_RNG::AOS_RNG(const RNGBase& a_base, std::uint64_t a_entryHash) :
	RNGBase(a_base)
{
	std::uint64_t mixSeed = seed;
	boost::hash_combine(mixSeed, a_entryHash);
	rng.emplace(XoshiroCpp::Xoshiro256StarStar(mixSeed));
}

void AOS_RNG::Seed(const RE::Actor* a_actor, const RE::TESObjectANIO* a_animObject, std::uint64_t a_entryHash)
{
	if (rng) {
		return;
	}

	const auto make_seed = [&](std::uint64_t a_seed) {
		std::uint64_t mixSeed = a_seed;
		if (a_animObject) {
			boost::hash_combine(mixSeed, get_form_seed(a_animObject));
		}
		boost::hash_combine(mixSeed, a_entryHash);
		boost::hash_combine(mixSeed, seed);
		return mixSeed;
	};

	switch (type) {
	case CHANCE_TYPE::kActorHash:
		{
			if (a_actor) {
				seed = make_seed(get_form_seed(a_actor));
			} else {
				type = CHANCE_TYPE::kRandom;
				seed = seed != 0 ? make_seed(0) : 0;
			}
		}
		break;
	case CHANCE_TYPE::kLocationHash:
		{
			const RE::TESForm* locOrCell = util::GetLocationOrCell(a_actor);
			if (locOrCell && a_animObject) {
				seed = make_seed(get_form_seed(locOrCell));
			} else if (a_actor) {
				seed = make_seed(get_form_seed(a_actor));
			} else {
				type = CHANCE_TYPE::kRandom;
				seed = seed != 0 ? make_seed(0) : 0;
			}
		}
		break;
	case CHANCE_TYPE::kRandom:
		{
			if (seed != 0) {
				seed = make_seed(a_actor ? get_form_seed(a_actor) : 0);
			}
		}
		break;
	default:
		break;
	}

	if (type == CHANCE_TYPE::kRandom && seed == 0) {
		rng.emplace(XoshiroCpp::Xoshiro256StarStar(std::chrono::high_resolution_clock::now().time_since_epoch().count()));
	} else {
		rng.emplace(XoshiroCpp::Xoshiro256StarStar(seed));
	}
}

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

RNGParams::RNGParams(const std::string& a_str)
{
	if (distribution::is_valid_entry(a_str)) {
		if (a_str.contains("chanceR")) {
			type = CHANCE_TYPE::kRandom;
		} else if (a_str.contains("chanceL")) {
			type = CHANCE_TYPE::kLocationHash;
		} else if (a_str.contains("chance")) {
			type = CHANCE_TYPE::kActorHash;
		} else {
			type = CHANCE_TYPE::kNone;
		}
		if (type != CHANCE_TYPE::kNone) {
			if (boost::cmatch match; boost::regex_search(a_str.c_str(), match, regex::generic)) {
				if (const auto chanceOptions = REX::STR::SPLIT(match[1].str(), ","); !chanceOptions.empty()) {
					chanceValue = REX::STR::TO_NUM<float>(chanceOptions[0]);
					seed = chanceOptions.size() > 1 ? REX::STR::TO_NUM<std::uint64_t>(chanceOptions[1]) : 0;
				}
			}
		}
	}
}

bool RNGParams::PassedChance(AOS_RNG& a_rng, const RE::Actor* a_actor, const RE::TESObjectANIO* a_animObject, std::uint64_t a_entryHash) const
{
	if (chanceValue < 100.0f) {
		a_rng.Seed(a_actor, a_animObject, a_entryHash);
		if (const auto rngValue = a_rng.Generate<float>(0.0f, 100.0f); rngValue > chanceValue) {
			return false;
		}
	}
	return true;
}
