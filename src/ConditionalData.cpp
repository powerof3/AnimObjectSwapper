#include "ConditionalData.h"

namespace AnimObjectSwap
{
	Traits::Traits(const std::string& a_str)
	{
		const auto traits = REX::STR::SPLIT(a_str, ",");
		for (auto& trait : traits) {
			switch (REX::STR::CONST_HASH(trait)) {
			case "M"_h:
			case "-F"_h:
				sex = RE::SEX::kMale;
				break;
			case "F"_h:
			case "-M"_h:
				sex = RE::SEX::kFemale;
				break;
			case "C"_h:
				child = true;
				break;
			case "-C"_h:
				child = false;
				break;
			default:
				break;
			}
		}
	}

	ConditionFilters::ConditionFilters(std::string a_conditionID, std::vector<std::string>& a_conditions, const std::string& a_traits) :
		conditionID(std::move(a_conditionID))
	{
		NOT.reserve(a_conditions.size());
		MATCH.reserve(a_conditions.size());

		const auto push_filter = [](std::vector<ConditionData>& a_processed, std::string& a_condition) {
			if (const auto [processedID, form] = util::GetFormWithID(a_condition, true); processedID != 0) {
				if (form && !form->IsDynamicForm()) {
					a_processed.emplace_back(form);
				} else {
					a_processed.emplace_back(processedID);
				}
			} else {
				REX::ERROR("\t\tFilter [{}] INFO - unable to find form, treating filter as string", a_condition);
				a_processed.emplace_back(a_condition);
			}
		};

		for (auto& condition : a_conditions) {
			if (condition.empty()) {
				continue;
			}
			if (condition.contains('+')) {
				auto conditions_ALL = REX::STR::SPLIT(condition, "+");
				for (auto& condition_ALL : conditions_ALL) {
					push_filter(ALL, condition_ALL);
				}
			} else if (condition[0] == '-') {
				condition.erase(0, 1);
				push_filter(NOT, condition);
			} else if (condition[0] == '*') {
				condition.erase(0, 1);
				ANY.emplace_back(condition);
			} else {
				push_filter(MATCH, condition);
			}
		}

		if (distribution::is_valid_entry(a_traits)) {
			traits = Traits(a_traits);
		}
	}

	bool ConditionalInput::IsValid(RE::TESForm* a_form) const
	{
		if (a_form) {
			switch (a_form->GetFormType()) {
			case RE::FormType::NPC:
				return actorbase == a_form;
			case RE::FormType::Faction:
				{
					const auto faction = a_form->As<RE::TESFaction>();
					return actor->IsInFaction(faction);
				}
			case RE::FormType::Race:
				{
					const auto race = a_form->As<RE::TESRace>();
					return actor->GetRace() == race;
				}
			case RE::FormType::Keyword:
				{
					if (const auto keyword = a_form->As<RE::BGSKeyword>()) {
						if (actor->HasKeyword(keyword)) {
							return true;
						}
						return std::ranges::any_of(inventory | std::views::keys, [&](const auto& item) {
							const auto keywordForm = item->template As<RE::BGSKeywordForm>();
							return keywordForm && keywordForm->HasKeyword(keyword);
						});
					}
					return false;
				}
			case RE::FormType::Location:
				{
					const auto location = a_form->As<RE::BGSLocation>();
					return currentLocation && (currentLocation == location || currentLocation->IsParent(location));
				}
			case RE::FormType::Cell:
				return currentCell == a_form;
			case RE::FormType::Spell:
				{
					const auto spell = a_form->As<RE::SpellItem>();
					return actor->HasSpell(spell);
				}
			case RE::FormType::FormList:
				{
					bool result = false;

					const auto list = a_form->As<RE::BGSListForm>();
					list->ForEachForm([&](RE::TESForm* a_formInList) {
						if (result = IsValid(a_formInList); result) {
							return RE::BSContainer::ForEachResult::kStop;
						}
						return RE::BSContainer::ForEachResult::kContinue;
					});

					return result;
				}
			default:
				if (const auto boundObj = a_form->As<RE::TESBoundObject>(); boundObj && boundObj->IsInventoryObject()) {
					return std::ranges::any_of(inventory, [&](const auto& inv) {
						const auto& [count, entryData] = inv.second;
						if (count < 0) {
							return false;
						}
						if (inv.first == boundObj) {
							return true;
						}
						const auto weapon = inv.first->template As<RE::TESObjectWEAP>();
						return weapon && weapon->templateWeapon == boundObj;
					});
				}
				return false;
			}
		}

		return false;
	}

	bool ConditionalInput::IsValid(const RE::FormID a_formID) const
	{
		return IsValid(RE::TESForm::LookupByID(a_formID));
	}

	bool ConditionalInput::IsValid(const std::string& a_string) const
	{
		// model path
		if (REX::STR::ICONTAINS(a_string, ".nif") || a_string.contains('\\')) {
			return std::ranges::any_of(inventory | std::views::keys, [&](const auto& item) {
				const auto model = item->template As<RE::TESModel>();
				return model && REX::STR::ICONTAINS(model->model, a_string);
			});
		}
		if (actor->HasKeywordString(a_string)) {
			return true;
		}
		if (currentCell && currentCell->GetFormEditorID() == a_string) {
			return true;
		}
		// inventory item keyword
		return std::ranges::any_of(inventory, [&](const auto& inv) {
			const auto& [count, entryData] = inv.second;
			if (count < 0) {
				return false;
			}
			const auto keywordForm = inv.first->template As<RE::BGSKeywordForm>();
			return keywordForm && keywordForm->HasKeywordString(a_string);
		});
	}

	bool ConditionalInput::IsValid(const ConditionData& a_data) const
	{
		bool result = false;

		std::visit(overload{
					   [&](RE::TESForm* a_form) {
						   result = IsValid(a_form);
					   },
					   [&](RE::FormID a_formID) {
						   result = IsValid(a_formID);
					   },
					   [&](const std::string& a_string) {
						   result = IsValid(a_string);
					   } },
			a_data);

		return result;
	}

	bool ConditionalInput::IsAnyValid(const std::string& a_string) const
	{
		if (REX::STR::ICONTAINS(a_string, ".nif") || a_string.contains('\\')) {
			return std::ranges::any_of(inventory | std::views::keys, [&](const auto& item) {
				const auto model = item->template As<RE::TESModel>();
				return model && REX::STR::ICONTAINS(model->model, a_string);
			});
		}
		if (actorbase) {
			if (actorbase->ContainsKeyword(a_string) || REX::STR::ICONTAINS(actorbaseEDID, a_string)) {
				return true;
			}
		}
		if (currentCell && REX::STR::ICONTAINS(currentCell->GetFormEditorID(), a_string)) {
			return true;
		}
		return std::ranges::any_of(inventory | std::views::keys, [&](const auto& item) {
			if (const auto keywordForm = item->template As<RE::BGSKeywordForm>(); keywordForm && keywordForm->ContainsKeywordString(a_string)) {
				return true;
			}
			return REX::STR::ICONTAINS(editorID::get_editorID(item), a_string);
		});
	}

	bool ConditionalInput::IsValid(const ConditionFilters& a_filters) const
	{
		if (!a_filters.ALL.empty()) {
			if (!std::ranges::all_of(a_filters.ALL, [this](const auto& data) { return IsValid(data); })) {
				return false;
			}
		}

		if (!a_filters.NOT.empty()) {
			if (std::ranges::any_of(a_filters.NOT, [this](const auto& data) { return IsValid(data); })) {
				return false;
			}
		}

		if (!a_filters.MATCH.empty()) {
			if (std::ranges::none_of(a_filters.MATCH, [this](const auto& data) { return IsValid(data); })) {
				return false;
			}
		}

		if (!a_filters.ANY.empty()) {
			if (std::ranges::none_of(a_filters.ANY, [this](const auto& str) { return IsAnyValid(str); })) {
				return false;
			}
		}

		const auto& traits = a_filters.traits;

		if (traits.sex != RE::SEX::kNone) {
			if (actorbase && actorbase->GetSex() != traits.sex) {
				return false;
			}
		}

		if (traits.child && actor->IsChild() != *traits.child) {
			return false;
		}

		return true;
	}
}
