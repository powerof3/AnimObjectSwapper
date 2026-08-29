#include "Manager.h"
#include "LookupFilters.h"
#include "MergeMapperPluginAPI.h"

#include <SimpleIni.h>
#undef ERROR

namespace AnimObjectSwap
{
	RE::FormID Manager::GetFormID(const std::string& a_str)
	{
		if (const auto splitID = REX::STR::SPLIT(a_str, "~"); splitID.size() == 2) {
			RE::FormID resolvedID;

			const auto  formID = REX::STR::TO_NUM<RE::FormID>(splitID[0], true);
			const auto& modName = splitID[1];
			if (g_mergeMapperInterface) {
				const auto [mergedModName, mergedFormID] = g_mergeMapperInterface->GetNewFormID(modName.c_str(), formID);
				resolvedID = RE::TESDataHandler::GetSingleton()->LookupFormID(mergedFormID, mergedModName);
			} else {
				resolvedID = RE::TESDataHandler::GetSingleton()->LookupFormID(formID, modName);
			}

			return resolvedID;
		}
		if (REX::STR::IS_ONLY_HEX(a_str, true)) {
			const auto formID = REX::STR::TO_NUM<RE::FormID>(a_str, true);
			if (const auto form = RE::TESForm::LookupByID(formID)) {
				return formID;
			}
		}
		if (const auto form = RE::TESForm::LookupByEditorID(a_str)) {
			return form->GetFormID();
		}
		return static_cast<RE::FormID>(0);
	}

	bool Manager::LoadForms()
	{
		std::vector<std::string> configs = dist::get_configs(R"(Data\)", "_ANIO"sv);

		if (configs.empty()) {
			REX::WARN("No .ini files with _ANIO suffix were found within the Data folder, aborting...");
			return false;
		}

		REX::INFO("{} matching inis found", configs.size());

		std::ranges::sort(configs);

		for (auto& path : configs) {
			REX::INFO("\tINI : {}", path);

			CSimpleIniA ini;
			ini.SetUnicode();
			ini.SetMultiKey();
			ini.SetAllowKeyOnly();

			if (const auto rc = ini.LoadFile(path.c_str()); rc < 0) {
				REX::ERROR("\t\tcouldn't read INI");
				continue;
			}

			CSimpleIniA::TNamesDepend sections;
			ini.GetAllSections(sections);
			sections.sort(CSimpleIniA::Entry::LoadOrder());

			for (auto& [section, comment, keyOrder] : sections) {
				bool            noConditions = true;
				ConditionalSwap conditionalSwap{};

				constexpr auto push_filter = [](const std::string& a_condition, FormIDStrVec& a_processedFilters) {
					if (const auto processedID = GetFormID(a_condition); processedID != 0) {
						a_processedFilters.push_back(processedID);
					} else {
						REX::ERROR("\t\t\tFilter  [{}] INFO - unable to find form, treating filter as string", a_condition);
						a_processedFilters.push_back(a_condition);
					}
				};

				constexpr auto split_sub_string = [](const std::string& a_str, const std::string& a_delimiter = ",") {
					if (dist::is_valid_entry(a_str)) {
						return REX::STR::SPLIT(a_str, a_delimiter);
					}
					return std::vector<std::string>();
				};

				if (REX::STR::ICONTAINS(section, "|")) {
					noConditions = false;

					auto conditions = REX::STR::SPLIT(section, "|");  // [ANIO|FILTERS|TRAITS]
					auto size = conditions.size();

					if (size > 1) {
						auto filters = split_sub_string(conditions[1]);
						for (auto& filter : filters) {
							if (filter.contains("+"sv)) {
								auto filters_ALL = REX::STR::SPLIT(filter, "+");
								for (auto& filter_ALL : filters_ALL) {
									push_filter(filter_ALL, conditionalSwap.conditions.ALL);
								}
							} else {
								auto id = filter.at(0);
								if (id == '-') {
									filter.erase(0, 1);
									push_filter(filter, conditionalSwap.conditions.NOT);
								} else if (id == '*') {
									filter.erase(0, 1);
									conditionalSwap.conditions.ANY.push_back(filter);  // string
								} else {
									push_filter(filter, conditionalSwap.conditions.MATCH);
								}
							}
						}
					}

					if (size > 2) {
						const auto& traits = split_sub_string(conditions[2]);
						for (auto& trait : traits) {
							if (trait == "M" || trait == "-F") {
								conditionalSwap.conditions.traits.sex = RE::SEX::kMale;
							} else if (trait == "F" || trait == "-M") {
								conditionalSwap.conditions.traits.sex = RE::SEX::kFemale;
							} else if (trait == "C") {
								conditionalSwap.conditions.traits.child = true;
							} else if (trait == "-C") {
								conditionalSwap.conditions.traits.child = false;
							}
						}
					}
				}

				if (const auto values = ini.GetSection(section); values && !values->empty()) {
					for (const auto& key : *values | std::views::keys) {
						auto splitValue = REX::STR::SPLIT(key.pItem, "|");

						if (RE::FormID baseAnio = GetFormID(splitValue[0]); baseAnio != 0) {
							FormIDSet tempSwapAnimObjects{};

							auto swapAnioEntry = REX::STR::SPLIT(splitValue[1], ",");
							for (auto& swapAnioStr : swapAnioEntry) {
								if (RE::FormID swapAnio = GetFormID(swapAnioStr); swapAnio != 0) {
									if (noConditions) {
										_animObjects[baseAnio].insert(swapAnio);
									} else {
										tempSwapAnimObjects.insert(swapAnio);
									}
								} else {
									REX::ERROR("\t\t\tSwap ANIO [{}] FAIL (invalid formID/edid::get_editorID)", swapAnioStr);
								}
							}

							if (!noConditions) {
								conditionalSwap.swappedAnimObjects = tempSwapAnimObjects;
								_animObjectsConditional[baseAnio].push_back(conditionalSwap);
							}
						} else {
							REX::ERROR("\t\t\tBase ANIO [{}] FAIL (invalid formID/edid::get_editorID)", splitValue[0]);
						}
					}
				}
			}
		}

		REX::INFO("{:*^30}", "RESULT");

		REX::INFO("{} animobject swaps found", _animObjects.size());
		for (auto& animObject : _animObjects) {
			REX::INFO("\t{} : {} variations", RE::TESForm::LookupByID(animObject.first)->GetFormEditorID(), animObject.second.size());
		}

		REX::INFO("{} conditional animobject swaps found", _animObjectsConditional.size());
		for (auto& animObject : _animObjectsConditional) {
			REX::INFO("\t{} : {} conditional variations", RE::TESForm::LookupByID(animObject.first)->GetFormEditorID(), animObject.second.size());
		}

		return !_animObjects.empty() || !_animObjectsConditional.empty();
	}

	RE::TESObjectANIO* Manager::GetSwappedAnimObject(RE::TESObjectREFR* a_user, RE::TESObjectANIO* a_animObject)
	{
		const auto origFormID = a_animObject->GetFormID();

		if (const auto it = _animObjectsConditional.find(origFormID); it != _animObjectsConditional.end()) {
			if (const auto actor = a_user ? a_user->As<RE::Actor>() : nullptr; actor) {
				if (const auto result = std::ranges::find_if(it->second, [&](const auto& conditionalSwap) {
						return Filter::PassFilter(actor, conditionalSwap.conditions);
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
			auto randIt = REX::TRandom<std::size_t>().Generate(0, setEnd);

			return RE::TESForm::LookupByID<RE::TESObjectANIO>(*std::next(a_animObjects.begin(), randIt));
		}
	}
}
