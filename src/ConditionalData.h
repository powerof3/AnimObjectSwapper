#pragma once

namespace AnimObjectSwap
{
	using ConditionData = std::variant<RE::TESForm*, RE::FormID, std::string>;

	struct Traits
	{
		Traits() = default;
		explicit Traits(const std::string& a_str);

		bool operator==(const Traits&) const = default;

		// members
		RE::SEX             sex{ RE::SEX::kNone };
		std::optional<bool> child{ std::nullopt };
	};

	// X / -X / *X / -*X
	struct FilterRule
	{
		FilterRule() = default;
		FilterRule(bool a_excludeModifier, bool a_partialModifier, const std::string& a_value, bool a_isModelPath);

		std::int32_t GetFilterCost(bool a_allFilter) const;

		// members
		bool          excludeModifier{ false };  // -
		bool          partialModifier{ false };  // *
		bool          isModelPath{ false };
		ConditionData data{};
	};

	using FilterGroup = std::vector<FilterRule>;  // Guard+*Mage+-Thief+-*Bandit

	struct ConditionFilters
	{
		ConditionFilters() = default;
		ConditionFilters(std::vector<std::string>& a_conditions, const std::string& a_traits, std::uint32_t a_fileIndex);

		// members
		std::vector<FilterGroup> ALL{};  // Guard+*Mage,-Thief+Horse
		FilterGroup              ANY{};  // Guard,*Mage,-Thief,-*Bandit
		Traits                   traits{};
		std::uint32_t            fileIndex{ 0 };
	};

	using ConditionFiltersPtr = std::shared_ptr<const ConditionFilters>;

	template <class T>
	using ConditionalData = InsertionMap<ConditionFiltersPtr, std::vector<T>>;

	struct ConditionalInput
	{
		explicit ConditionalInput(RE::Actor* a_actor) :
			actor(a_actor),
			actorbase(a_actor->GetActorBase()),
			currentCell(a_actor->GetParentCell()),
			currentLocation(a_actor->GetCurrentLocation())
		{}

		[[nodiscard]] bool IsValid(RE::TESForm* a_form) const;
		[[nodiscard]] bool IsValid(RE::FormID a_formID) const;
		[[nodiscard]] bool IsValid(const std::string& a_string, bool a_isModelPath) const;

		[[nodiscard]] bool IsValid(const ConditionData& a_data, bool a_isModelPath) const;
		[[nodiscard]] bool IsValid(const FilterRule& a_rule) const;
		[[nodiscard]] bool IsAnyValid(const std::string& a_string, bool a_isModelPath) const;

		[[nodiscard]] bool IsValid(const ConditionFilters& a_filters) const;

	private:
		struct ID
		{
			ID() = default;
			explicit ID(const RE::TESForm* a_base);
			~ID() = default;

			[[nodiscard]] bool contains(const std::string& a_str) const;

			bool operator==(const RE::TESFile* a_mod) const;
			bool operator==(const std::string& a_str) const;
			bool operator==(RE::FormID a_formID) const;

			RE::FormID  formID{ 0 };
			std::string editorID{};
		};

		[[nodiscard]] const Set<RE::TESBoundObject*>& GetInventory() const;
		[[nodiscard]] const std::vector<ID>&          GetActorBaseIDs() const;

		// members
		RE::Actor*         actor;
		RE::TESNPC*        actorbase;
		RE::TESObjectCELL* currentCell;
		RE::BGSLocation*   currentLocation;

		mutable std::optional<Set<RE::TESBoundObject*>> inventory{};
		mutable std::vector<ID>                         actorbaseIDs{};
	};
}
