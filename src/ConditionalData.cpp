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

	// sort filter according to how expensive it is
	std::int32_t FilterRule::GetFilterCost(bool a_allFilter) const
	{
		std::int32_t cost = 0;

		const auto calc_string_cost = [&](bool a_partialModifier) {
			if (isModelPath) {
				cost += (a_partialModifier ? 60 : 50);
			} else {
				cost += (a_partialModifier ? 40 : 30);
			}
		};

		std::visit(overload{
					   [&](RE::TESForm* a_form) {
						   if (a_form->Is(RE::FormType::FormList)) {
							   cost += 30;
						   } else if (a_form->Is(RE::FormType::Keyword) || a_form->IsInventoryObject()) {
							   cost += 20;
						   } else {
							   cost += 10;
						   }
					   },
					   [&](RE::FormID) { cost += 20; },
					   [&](const std::string&) { calc_string_cost(partialModifier); } },
			data);

		if (a_allFilter ? !excludeModifier : excludeModifier) {
			cost -= 1;
		}

		return cost;
	}

	FilterRule FilterRule::FromEntry(std::string a_entry)
	{
		const auto hasExcludeModifier = a_entry[0] == '-';

		if (hasExcludeModifier || a_entry[0] == '+') { // -*Guard
			a_entry.erase(0, 1);  // *Guard
		}

		const auto hasPartialModifier = !a_entry.empty() && a_entry[0] == '*';
		if (hasPartialModifier) {
			a_entry.erase(0, 1);  // Guard
		}

		const auto isModelPath = REX::STR::ICONTAINS(a_entry, ".nif") || a_entry.contains('\\') || a_entry.contains('/');
		if (isModelPath) {
			util::SanitizePath(a_entry);
		}

		return FilterRule(hasExcludeModifier, hasPartialModifier, a_entry, isModelPath);
	}

	FilterRule::FilterRule(const bool a_excludeModifier, const bool a_partialModifier, const std::string& a_value, bool a_isModelPath) :
		excludeModifier(a_excludeModifier),
		partialModifier(a_partialModifier),
		isModelPath(a_isModelPath)
	{
		if (a_partialModifier || a_isModelPath) {
			data = a_value;
			return;
		}
		if (const auto [processedID, form] = util::GetFormWithID(a_value, true); processedID != 0) {
			if (form && !form->IsDynamicForm()) {
				data = form;
			} else {
				data = processedID;
			}
		} else {
			REX::ERROR("\t\tFilter [{}] INFO - unable to find form, treating filter as string", a_value);
			data = a_value;
		}
	}

	ConditionFilters::ConditionFilters(std::vector<std::string>& a_conditions, const std::string& a_traits, std::uint32_t a_fileIndex) :
		fileIndex(a_fileIndex)
	{
		for (auto& condition : a_conditions) {
			REX::STR::TRIM(condition);
			if (!distribution::is_valid_entry(condition)) {
				continue;
			}
			if (condition.contains('+')) {
				// A, (X + Y + Z), B
				FilterGroup group;
				for (auto& ALLEntry : REX::STR::SPLIT(condition, "+")) {
					REX::STR::TRIM(ALLEntry);
					if (ALLEntry.empty()) {
						continue;
					}
					group.emplace_back(FilterRule::FromEntry(ALLEntry));
				}
				if (!group.empty()) {
					ALL.emplace_back(std::move(group));
				}
			} else {
				// A or *B or -C or -*D
				ANY.emplace_back(FilterRule::FromEntry(condition));
			}
		}

		std::ranges::stable_sort(ANY, {}, [&](const auto& f) { return f.GetFilterCost(false); });
		for (auto& group : ALL) {
			std::ranges::stable_sort(group, {}, [&](const auto& f) { return f.GetFilterCost(true); });
		}

		if (distribution::is_valid_entry(a_traits)) {
			traits = Traits(a_traits);
		}
	}

	const std::string& ConditionalInput::NPC::get_editorID() const
	{
		if (!editorID) {
			if (npc) {
				editorID.emplace(editorID::get_editorID(npc));
			} else {
				editorID.emplace("");
			}
		}
		return *editorID;
	}

	const Set<RE::TESBoundObject*>& ConditionalInput::GetInventory() const
	{
		if (!inventory) {
			Set<RE::TESBoundObject*> tempSet;
			const auto               actorInventory = actor->GetInventory([](RE::TESBoundObject& a_object) {
				return a_object.IsInventoryObject();
			},
				true);
			tempSet.reserve(actorInventory.size());
			for (const auto& [object, data] : actorInventory) {
				if (data.first <= 0) {
					continue;
				}
				tempSet.emplace(object);
				if (const auto weapon = object->As<RE::TESObjectWEAP>(); weapon && weapon->templateWeapon) {
					tempSet.emplace(weapon->templateWeapon);
				}
			}
			inventory.emplace(std::move(tempSet));
		}
		return *inventory;
	}

	const std::vector<ConditionalInput::NPC>& ConditionalInput::GetActorBases() const
	{
		if (actorbases.empty() && actorbase) {
			if (actorbase->baseTemplateForm) {
				actorbases.emplace_back(skyrim_cast<RE::TESNPC*>(actorbase->baseTemplateForm));
			}
			if (const auto extraLvlCreature = actor->extraList.GetByType<RE::ExtraLeveledCreature>()) {
				if (const auto originalBase = extraLvlCreature->originalBase) {
					actorbases.emplace_back(skyrim_cast<RE::TESNPC*>(originalBase));
				}
				if (const auto templateBase = extraLvlCreature->templateBase) {
					actorbases.emplace_back(skyrim_cast<RE::TESNPC*>(templateBase));
				}
			} else {
				actorbases.emplace_back(actorbase);
			}
		}
		return actorbases;
	}

	bool ConditionalInput::IsValid(RE::TESForm* a_form) const
	{
		if (a_form) {
			switch (a_form->GetFormType()) {
			case RE::FormType::NPC:
				return actorbase == a_form || std::ranges::any_of(GetActorBases(), [&](const auto& ID) { return ID == a_form; });
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
						return std::ranges::any_of(GetInventory(), [&](const auto& object) {
							const auto keywordForm = object->template As<RE::BGSKeywordForm>();
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
					return GetInventory().contains(boundObj);
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

	bool ConditionalInput::IsValid(const std::string& a_string, bool a_isModelPath) const
	{
		if (a_isModelPath) {
			return std::ranges::any_of(GetInventory(), [&](const auto& object) {
				const auto model = object->template As<RE::TESModel>();
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
		return std::ranges::any_of(GetInventory(), [&](const auto& object) {
			const auto keywordForm = object->template As<RE::BGSKeywordForm>();
			return keywordForm && keywordForm->HasKeywordString(a_string);
		});
	}

	bool ConditionalInput::IsValid(const ConditionData& a_data, bool a_isModelPath) const
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
						   result = IsValid(a_string, a_isModelPath);
					   } },
			a_data);

		return result;
	}

	bool ConditionalInput::IsAnyValid(const std::string& a_string, bool a_isModelPath) const
	{
		if (a_isModelPath) {
			return std::ranges::any_of(GetInventory(), [&](const auto& object) {
				const auto model = object->template As<RE::TESModel>();
				return model && REX::STR::ICONTAINS(model->model, a_string);
			});
		}
		if (actorbase) {
			if (actorbase->ContainsKeyword(a_string) || std::ranges::any_of(GetActorBases(), [&](const auto& npc) { return npc.contains(a_string); })) {
				return true;
			}
		}
		if (currentCell && REX::STR::ICONTAINS(currentCell->GetFormEditorID(), a_string)) {
			return true;
		}
		return std::ranges::any_of(GetInventory(), [&](const auto& object) {
			if (const auto keywordForm = object->template As<RE::BGSKeywordForm>(); keywordForm && keywordForm->ContainsKeywordString(a_string)) {
				return true;
			}
			return REX::STR::ICONTAINS(editorID::get_editorID(object), a_string);
		});
	}

	bool ConditionalInput::IsValid(const FilterRule& a_rule) const
	{
		return a_rule.partialModifier ? IsAnyValid(std::get<std::string>(a_rule.data), a_rule.isModelPath) : IsValid(a_rule.data, a_rule.isModelPath);
	}

	bool ConditionalInput::IsValid(const ConditionFilters& a_filters) const
	{
		const auto& traits = a_filters.traits;

		if (traits.sex != RE::SEX::kNone) {
			if (actorbase && actorbase->GetSex() != traits.sex) {
				return false;
			}
		}

		if (traits.child && actor->IsChild() != *traits.child) {
			return false;
		}

		const auto matches_all = [&](const FilterGroup& a_group) {
			for (const auto& f : a_group) {
				if (f.excludeModifier) {
					if (IsValid(f)) {
						return false;
					}
				} else if (!IsValid(f)) {
					return false;
				}
			}
			return true;
		};

		const auto matches_any = [&](const std::vector<FilterRule>& a_group) {
			bool hasExact = false;
			bool exactPassed = false;

			bool hasPartial = false;
			bool partialPassed = false;

			for (const auto& f : a_group) {
				if (f.excludeModifier) {
					if (IsValid(f)) {
						return false;
					}
					continue;
				}
				if (f.partialModifier) {
					hasPartial = true;
					if (!partialPassed && IsValid(f)) {
						partialPassed = true;
					}
				} else {
					hasExact = true;
					if (!exactPassed && IsValid(f)) {
						exactPassed = true;
					}
				}
			}

			return (!hasExact || exactPassed) && (!hasPartial || partialPassed);
		};

		// ALL filters; at least one filter group must match (X+Y+Z or A+B+C)
		if (!a_filters.ALL.empty()) {
			bool any_group_matched = false;
			for (const auto& group : a_filters.ALL) {
				if (matches_all(group)) {
					any_group_matched = true;
					break;
				}
			}
			if (!any_group_matched) {
				return false;
			}
		}

		if (!a_filters.ANY.empty() && !matches_any(a_filters.ANY)) {
			return false;
		}

		return true;
	}
}
