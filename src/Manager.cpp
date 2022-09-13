#include "Manager.h"

namespace AnimObjectSwap
{
	RE::FormID Manager::GetFormID(const std::string& a_str)
	{
		if (a_str.contains("~"sv)) {
			if (auto splitID = string::split(a_str, "~"); splitID.size() == 2) {
				const auto formID = string::lexical_cast<RE::FormID>(splitID[0], true);
				const auto& modName = splitID[1];

				return RE::TESDataHandler::GetSingleton()->LookupFormID(formID, modName);
			}
		}
		if (const auto form = RE::TESForm::LookupByEditorID(a_str); form) {
			return form->GetFormID();
		}
		return static_cast<RE::FormID>(0);
	}

	bool Manager::LoadForms()
	{
		std::vector<std::string> configs;

		constexpr auto suffix = "_ANIO"sv;

		auto constexpr folder = R"(Data\)";
		for (const auto& entry : std::filesystem::directory_iterator(folder)) {
			if (entry.exists() && !entry.path().empty() && entry.path().extension() == ".ini"sv) {
				if (const auto path = entry.path().string(); path.rfind(suffix) != std::string::npos) {
					configs.push_back(path);
				}
			}
		}

		if (configs.empty()) {
			logger::warn("	No .ini files with {} suffix were found within the Data folder, aborting...", suffix);
			return false;
		}

		logger::info("	{} matching inis found", configs.size());

		std::ranges::sort(configs);

		for (auto& path : configs) {
			logger::info("	INI : {}", path);

			CSimpleIniA ini;
			ini.SetUnicode();
			ini.SetMultiKey();
			ini.SetAllowKeyOnly();

			if (const auto rc = ini.LoadFile(path.c_str()); rc < 0) {
				logger::error("	couldn't read INI");
				continue;
			}

			CSimpleIniA::TNamesDepend sections;
			ini.GetAllSections(sections);
			sections.sort(CSimpleIniA::Entry::LoadOrder());

			for (auto& [section, comment, keyOrder] : sections) {
				bool noConditions = true;
				ConditionalSwap conditionalSwap{};

				constexpr auto push_condition = [](const std::string& a_condition, FormIDStrVec& a_processedConditions) {
					if (const auto processedID = GetFormID(a_condition); processedID != 0) {
						a_processedConditions.push_back(processedID);
					} else {
						a_processedConditions.push_back(a_condition);
					}
				};

				if (string::icontains(section, "|")) {
					noConditions = false;

					auto conditions = string::split(string::split(section, "|")[1], ",");  // [ANIO|....]
					for (auto& condition : conditions) {
						if (condition.contains("+"sv)) {
							auto conditions_ALL = string::split(condition, "+");
							for (auto& condition_ALL : conditions_ALL) {
								push_condition(condition_ALL, conditionalSwap.conditions.ALL);
							}
						} else {
							auto id = condition.at(0);
							if (id == '-') {
								condition.erase(0, 1);
								push_condition(condition, conditionalSwap.conditions.NOT);
							} else if (id == '*') {
								condition.erase(0, 1);
								push_condition(condition, conditionalSwap.conditions.ANY);
							} else {
								push_condition(condition, conditionalSwap.conditions.MATCH);
							}
						}
					}
				}

				if (const auto values = ini.GetSection(section); values && !values->empty()) {
					for (const auto& key : *values | std::views::keys) {
						auto splitValue = string::split(key.pItem, "|");

						if (RE::FormID baseAnio = GetFormID(splitValue[0]); baseAnio != 0) {
							FormIDSet tempSwapAnimObjects{};

							auto swapAnioEntry = string::split(splitValue[1], ",");
							for (auto& swapAnioStr : swapAnioEntry) {
								if (RE::FormID swapAnio = GetFormID(swapAnioStr); swapAnio != 0) {
									if (noConditions) {
										_animObjects[baseAnio].insert(swapAnio);
									} else {
										tempSwapAnimObjects.insert(swapAnio);
									}
								} else {
									logger::error("				Unable to find swap animObject [{}] (invalid formID/editorID)", swapAnioStr);
								}
							}

							if (!noConditions) {
								conditionalSwap.swappedAnimObjects = tempSwapAnimObjects;
								_animObjectsConditional[baseAnio].push_back(conditionalSwap);
							}
						} else {
							logger::error("			Unable to find base animObject [{}] (invalid formID/editorID)", splitValue[0]);
						}
					}
				}
			}
		}

		logger::info("{:*^30}", "RESULT");

		logger::info("{} animobject swaps found", _animObjects.size());
		for (auto& animObject : _animObjects) {
			logger::info("	{} : {} variations", RE::TESForm::LookupByID(animObject.first)->GetFormEditorID(), animObject.second.size());
		}

		logger::info("{} conditional animobject swaps found", _animObjectsConditional.size());
		for (auto& animObject : _animObjectsConditional) {
			logger::info("	{} : {} conditional variations", RE::TESForm::LookupByID(animObject.first)->GetFormEditorID(), animObject.second.size());
		}

		return !_animObjects.empty() || !_animObjectsConditional.empty();
	}

	bool Manager::PassFilter(RE::Actor* a_actor, const Conditions& a_conditions) const
	{
		const auto match_filter = [&](const FormIDStr& a_formIDStr) {
			if (std::holds_alternative<RE::FormID>(a_formIDStr)) {
				if (auto form = RE::TESForm::LookupByID(std::get<RE::FormID>(a_formIDStr)); form) {
					switch (form->GetFormType()) {
					case RE::FormType::NPC:
						return a_actor->GetActorBase() == form;
					case RE::FormType::Faction:
						{
							const auto faction = form->As<RE::TESFaction>();
							return a_actor->IsInFaction(faction);
						}
					case RE::FormType::Race:
						{
							const auto race = form->As<RE::TESRace>();
							return a_actor->GetRace() == race;
						}
					case RE::FormType::Keyword:
						{
							if (const auto keyword = form->As<RE::BGSKeyword>(); keyword) {
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
							const auto location = form->As<RE::BGSLocation>();
							return a_actor->GetCurrentLocation() == location;
						}
					default:
						if (const auto boundObj = form->As<RE::TESBoundObject>(); boundObj && boundObj->IsInventoryObject()) {
							auto inventory = a_actor->GetInventory();
							for (const auto& item : inventory | std::views::keys) {
								if (item == boundObj) {
									return true;
								}
								if (const auto weapon = item->As<RE::TESObjectWEAP>(); weapon) {
									if (weapon->templateWeapon == boundObj) {
										return true;
									}
								}
							}
						}
						return false;
					}
				}
			} else {
				const auto& string = std::get<std::string>(a_formIDStr);
				if (string::icontains(string, ".nif") || string.contains('\\')) {
					auto inventory = a_actor->GetInventory();
					for (const auto& item : inventory | std::views::keys) {
						if (const auto model = item->As<RE::TESModel>(); model && string::icontains(model->model, string)) {
							return true;
						}
					}
				} else {
					if (a_actor->HasKeywordString(string)) {
						return true;
					}
					auto inventory = a_actor->GetInventory();
					for (const auto& item : inventory | std::views::keys) {
						if (const auto keywordForm = item->As<RE::BGSKeywordForm>(); keywordForm && keywordForm->HasKeywordString(string)) {
							return true;
						}
					}
				}
			}
			return false;
		};

		const auto match_filters = [&](const FormIDStrVec& a_formIDStrVec, bool a_matchAll = false) {
			if (a_matchAll) {
				return std::ranges::all_of(a_formIDStrVec, match_filter);
			} else {
				return std::ranges::any_of(a_formIDStrVec, match_filter);
			}
		};

		const auto contains_filters = [&](const FormIDStrVec& a_formIDStrVec) {
			return std::ranges::any_of(a_formIDStrVec, [&](const FormIDStr& a_formIDStr) {
				if (std::holds_alternative<std::string>(a_formIDStr)) {
					const auto& string = std::get<std::string>(a_formIDStr);
					if (string::icontains(string, ".nif") || string.contains('\\')) {
						auto inventory = a_actor->GetInventory();
						for (const auto& item : inventory | std::views::keys) {
							if (const auto model = item->As<RE::TESModel>(); model && string::icontains(model->model, string)) {
								return true;
							}
						}
					} else {
						if (const auto actorbase = a_actor->GetActorBase(); actorbase && actorbase->ContainsKeyword(string)) {
							return true;
						}
						auto inventory = a_actor->GetInventory();
						for (const auto& item : inventory | std::views::keys) {
							if (const auto keywordForm = item->As<RE::BGSKeywordForm>(); keywordForm && keywordForm->ContainsKeywordString(string)) {
								return true;
							}
						}
					}
				}
				return false;
			});
		};

		if (!a_conditions.ALL.empty() && !match_filters(a_conditions.ALL, true)) {
			return false;
		}

		if (!a_conditions.NOT.empty() && match_filters(a_conditions.NOT)) {
			return false;
		}

		if (!a_conditions.MATCH.empty() && !match_filters(a_conditions.MATCH)) {
			return false;
		}

		if (!a_conditions.ANY.empty() && !contains_filters(a_conditions.ANY)) {
			return false;
		}

		return true;
	}

	RE::TESObjectANIO* Manager::GetSwappedAnimObject(RE::TESObjectREFR* a_user, RE::TESObjectANIO* a_animObject)
	{
		const auto origFormID = a_animObject->GetFormID();

		if (const auto it = _animObjectsConditional.find(origFormID); it != _animObjectsConditional.end()) {
			if (const auto actor = a_user ? a_user->As<RE::Actor>() : nullptr; actor) {
				if (const auto result = std::ranges::find_if(it->second, [&](const auto& conditionalSwap) {
						return PassFilter(actor, conditionalSwap.conditions);
					});
					result != it->second.end()) {
					return GetSwappedAnimObject(result->swappedAnimObjects);
				}
			}
		}

		if (const auto it = _animObjects.find(origFormID); it != _animObjects.end()) {
			if (const auto& swapANIO = it->second; !swapANIO.empty()) {
				return GetSwappedAnimObject(swapANIO);
			}
		}

		return a_animObject;
	}

	RE::TESObjectANIO* Manager::GetSwappedAnimObject(const FormIDSet& a_animObjects) const
	{
		if (a_animObjects.size() == 1) {
			return RE::TESForm::LookupByID<RE::TESObjectANIO>(*a_animObjects.begin());
		} else {
			// return random element from set

			auto setEnd = std::distance(a_animObjects.begin(), a_animObjects.end()) - 1;
			auto randIt = stl::RNG::GetSingleton()->Generate<std::size_t>(0, setEnd);

			return RE::TESForm::LookupByID<RE::TESObjectANIO>(*std::next(a_animObjects.begin(), randIt));
		}
	}
}
