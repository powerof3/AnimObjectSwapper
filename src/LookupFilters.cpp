#include "LookupFilters.h"

namespace AnimObjectSwap::Filter
{
	bool match_form_filter(RE::Actor* a_actor, RE::TESForm* a_form)
	{
		switch (a_form->GetFormType()) {
		case RE::FormType::NPC:
			return a_actor->GetActorBase() == a_form;
		case RE::FormType::Faction:
			{
				const auto faction = a_form->As<RE::TESFaction>();
				return a_actor->IsInFaction(faction);
			}
		case RE::FormType::Race:
			{
				const auto race = a_form->As<RE::TESRace>();
				return a_actor->GetRace() == race;
			}
		case RE::FormType::Keyword:
			{
				if (const auto keyword = a_form->As<RE::BGSKeyword>(); keyword) {
					if (a_actor->HasKeyword(keyword)) {
						return true;
					}
					auto inventory = a_actor->GetInventory();
					for (const auto& item : inventory | std::views::keys) {
						if (const auto keywordForm = item->As<RE::BGSKeywordForm>(); keywordForm && keywordForm->HasKeyword(keyword)) {
							return true;
						}
					}
				}
				return false;
			}
		case RE::FormType::Location:
			{
				const auto location = a_form->As<RE::BGSLocation>();
				const auto currentLocation = a_actor->GetCurrentLocation();

				return a_actor->GetCurrentLocation() == location || currentLocation && currentLocation->IsParent(location);
			}
		case RE::FormType::Spell:
			{
				const auto spell = a_form->As<RE::SpellItem>();
				return a_actor->HasSpell(spell);
			}
		case RE::FormType::FormList:
			{
				bool result = false;

				const auto list = a_form->As<RE::BGSListForm>();
				list->ForEachForm([&](RE::TESForm& a_formInList) {
					if (result = match_form_filter(a_actor, &a_formInList); result) {
						return RE::BSContainer::ForEachResult::kStop;
					}
					return RE::BSContainer::ForEachResult::kContinue;
				});

				return result;
			}
		default:
			if (const auto boundObj = a_form->As<RE::TESBoundObject>(); boundObj && boundObj->IsInventoryObject()) {
				auto inventory = a_actor->GetInventory();
				return std::ranges::any_of(inventory, [&](const auto& inv) {
					if (inv.first == boundObj) {
						return true;
					} else {
						const auto weapon = inv.first->As<RE::TESObjectWEAP>();
						return weapon && weapon->templateWeapon == boundObj;
					}
				});
			}
			return false;
		}
	}

	bool matches_filter(RE::Actor* a_actor, const FormIDStrVec& a_formIDStrVec, bool a_matchAll = false)
	{
		const auto match_filter = [&](const FormIDStr& a_formIDStr) {
			if (std::holds_alternative<RE::FormID>(a_formIDStr)) {
				if (auto form = RE::TESForm::LookupByID(std::get<RE::FormID>(a_formIDStr)); form) {
					return match_form_filter(a_actor, form);
				}
			} else {
				const auto& string = std::get<std::string>(a_formIDStr);
				if (string::icontains(string, ".nif") || string.contains('\\')) {
					const auto inventory = a_actor->GetInventory();
					return std::ranges::any_of(inventory, [&](const auto& inv) {
						const auto model = inv.first->As<RE::TESModel>();
						return model && string::icontains(model->model, string);
					});
				} else {
					if (a_actor->HasKeywordString(string)) {
						return true;
					}
					if (auto cell = a_actor->GetParentCell(); cell && Manager::GetEditorID(cell) == string) {
						return true;
					}
					const auto inventory = a_actor->GetInventory();
					return std::ranges::any_of(inventory, [&](const auto& inv) {
						const auto keywordForm = inv.first->As<RE::BGSKeywordForm>();
						return keywordForm && keywordForm->HasKeywordString(string);
					});
				}
			}
			return false;
		};

		if (a_matchAll) {
			return std::ranges::all_of(a_formIDStrVec, match_filter);
		} else {
			return std::ranges::any_of(a_formIDStrVec, match_filter);
		}
	}

	bool contains_filter(RE::Actor* a_actor, const FormIDStrVec& a_formIDStrVec)
	{
		return std::ranges::any_of(a_formIDStrVec, [&](const FormIDStr& a_formIDStr) {
			if (std::holds_alternative<std::string>(a_formIDStr)) {
				const auto& string = std::get<std::string>(a_formIDStr);
				if (string::icontains(string, ".nif") || string.contains('\\')) {
					const auto inventory = a_actor->GetInventory();
					return std::ranges::any_of(inventory, [&](const auto& inv) {
						const auto model = inv.first->As<RE::TESModel>();
						return model && string::icontains(model->model, string);
					});
				} else {
					if (const auto actorbase = a_actor->GetActorBase(); actorbase) {
						if (actorbase->ContainsKeyword(string)) {
							return true;
						}
						if (const auto edid = Manager::GetEditorID(actorbase); string::icontains(edid, string)) {
							return true;
						}
					}
					if (auto cell = a_actor->GetParentCell(); cell && string::icontains(Manager::GetEditorID(cell), string)) {
						return true;
					}
					const auto inventory = a_actor->GetInventory();
					return std::ranges::any_of(inventory, [&](const auto& inv) {
						const auto keywordForm = inv.first->As<RE::BGSKeywordForm>();
						if (keywordForm && keywordForm->ContainsKeywordString(string)) {
							return true;
						} else {
							const auto edid = Manager::GetEditorID(inv.first);
							return string::icontains(edid, string);
						}
					});
				}
			}
			return false;
		});
	}

	bool PassFilter(RE::Actor* a_actor, const Conditions& a_conditions)
	{
		if (!a_conditions.ALL.empty() && !matches_filter(a_actor, a_conditions.ALL, true)) {
			return false;
		}

		if (!a_conditions.NOT.empty() && matches_filter(a_actor, a_conditions.NOT)) {
			return false;
		}

		if (!a_conditions.MATCH.empty() && !matches_filter(a_actor, a_conditions.MATCH)) {
			return false;
		}

		if (!a_conditions.ANY.empty() && !contains_filter(a_actor, a_conditions.ANY)) {
			return false;
		}

		const auto& traits = a_conditions.traits;

		if (traits.sex != RE::SEX::kNone) {
            const auto actorbase = a_actor->GetActorBase();
		    if (actorbase && actorbase->GetSex() != traits.sex) {
				return false;
			}
		}

		if (traits.child && a_actor->IsChild() != *traits.child) {
			return false;
		}

		return true;
	}
}
