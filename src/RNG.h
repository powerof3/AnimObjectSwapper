#pragma once

enum class CHANCE_TYPE
{
	kRandom,
	kActorHash,
	kLocationHash
};

struct Chance
{
public:
	Chance() = default;
	Chance(const std::string& a_str);

	bool PassedChance(const RE::Actor* a_actor, RE::TESObjectANIO* a_animObject) const;

	// members
	CHANCE_TYPE   chanceType{ CHANCE_TYPE::kRandom };
	float         chanceValue{ 100.0f };
	std::uint64_t seed{ 0 };
};

struct AOS_RNG
{
public:
	AOS_RNG() = default;
	AOS_RNG(const Chance& a_chance, const RE::Actor* a_actor, RE::TESObjectANIO* a_animObject);
	explicit AOS_RNG(const Chance& a_chance);

	template <class T>
	T generate(T a_min, T a_max) const
	{
		if (type == CHANCE_TYPE::kRandom && seed == 0) {
			return REX::TRandom<T>().Generate(a_min, a_max);
		}
		return REX::TRandom<T>(seed).Generate(a_min, a_max);
	}

	// members
	CHANCE_TYPE   type{ CHANCE_TYPE::kRandom };
	std::uint64_t seed{ 0 };

private:
	static std::uint64_t get_form_seed(const RE::TESForm* a_form);
};
