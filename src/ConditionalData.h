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

	struct ConditionFilters
	{
	public:
		ConditionFilters() = default;
		ConditionFilters(std::string a_conditionID, std::vector<std::string>& a_conditions, const std::string& a_traits);

		bool operator==(const ConditionFilters& a_rhs) const
		{
			return conditionID == a_rhs.conditionID;
		}

		bool operator<(const ConditionFilters& a_rhs) const
		{
			return conditionID < a_rhs.conditionID;
		}

		// members
		std::string                conditionID{};  // path|conditions|traits
		std::vector<ConditionData> ALL{};
		std::vector<ConditionData> NOT{};
		std::vector<ConditionData> MATCH{};
		std::vector<std::string>   ANY{};
		Traits                     traits{};
	};

	template <class T>
	using ConditionalData = std::map<ConditionFilters, std::vector<T>>;

	struct ConditionalInput
	{
		explicit ConditionalInput(RE::Actor* a_actor) :
			actor(a_actor),
			actorbase(a_actor->GetActorBase()),
			currentCell(a_actor->GetParentCell()),
			currentLocation(a_actor->GetCurrentLocation()),
			inventory(a_actor->GetInventory())
		{
			if (actorbase) {
				actorbaseEDID = editorID::get_editorID(actorbase);
			}
		}

		[[nodiscard]] bool IsValid(RE::TESForm* a_form) const;
		[[nodiscard]] bool IsValid(RE::FormID a_formID) const;
		[[nodiscard]] bool IsValid(const std::string& a_string) const;

		[[nodiscard]] bool IsValid(const ConditionData& a_data) const;
		[[nodiscard]] bool IsAnyValid(const std::string& a_string) const;

		[[nodiscard]] bool IsValid(const ConditionFilters& a_filters) const;

		// members
		RE::Actor*                          actor;
		RE::TESNPC*                         actorbase;
		std::string                         actorbaseEDID;
		RE::TESObjectCELL*                  currentCell;
		RE::BGSLocation*                    currentLocation;
		RE::TESObjectREFR::InventoryItemMap inventory;
	};
}
