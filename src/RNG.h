#pragma once

#include <XoshiroCpp.hpp>

enum class CHANCE_TYPE
{
	kRandom,
	kActorHash,
	kLocationHash
};

struct RNGBase
{
	RNGBase() = default;

	CHANCE_TYPE   type{ CHANCE_TYPE::kRandom };
	std::uint64_t seed{ 0 };
};

struct AOS_RNG : RNGBase
{
	AOS_RNG(const RNGBase& a_base);
	AOS_RNG(const RNGBase& a_base, const std::string& a_entry);
	AOS_RNG(const RNGBase& a_base, const RE::Actor* a_actor, const RE::TESObjectANIO* a_animObject, std::uint64_t a_entryHash);

	void Seed(const RE::Actor* a_actor, const RE::TESObjectANIO* a_animObject, std::uint64_t a_entryHash);

	template <class T>
	T Generate(T a_min, T a_max)
	{
		if constexpr (std::is_integral_v<T>) {
			return std::uniform_int_distribution<T>(a_min, a_max)(*rng);
		} else {
			return std::uniform_real_distribution<T>(a_min, a_max)(*rng);
		}
	}

private:
	static std::uint64_t get_form_seed(const RE::TESForm* a_form);

	std::optional<XoshiroCpp::Xoshiro256StarStar> rng{};
};

struct RNGParams : RNGBase
{
	RNGParams() = default;
	explicit RNGParams(const std::string& a_str);

	bool PassedChance(AOS_RNG& a_rng, const RE::Actor* a_actor, const RE::TESObjectANIO* a_animObject, std::uint64_t a_entryHash) const;

	// members
	float chanceValue{ 100.0f };
};
