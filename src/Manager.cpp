#include "Manager.h"

namespace AnimObjectSwap
{
	void Manager::LoadForms()
	{
		REX::INFO("{:*^30}", "INI");

		std::vector<std::string> configs = distribution::get_configs(R"(Data\)", "_ANIO"sv);

		if (configs.empty()) {
			REX::WARN("No .ini files with _ANIO suffix were found within the Data folder, aborting...");
			return;
		}

		REX::INFO("{} matching inis found...", configs.size());

		for (auto& path : configs) {
			REX::INFO("INI : {}", path);

			CSimpleIniA ini;
			ini.SetUnicode();
			ini.SetMultiKey();
			ini.SetAllowKeyOnly();

			if (const auto rc = ini.LoadFile(path.c_str()); rc < 0) {
				REX::ERROR("\tcouldn't read INI");
				continue;
			}

			CSimpleIniA::TNamesDepend sections;
			ini.GetAllSections(sections);
			sections.sort(CSimpleIniA::Entry::LoadOrder());

			for (auto& [_section, comment, keyOrder] : sections) {
				std::string section = _section;

				CSimpleIniA::TNamesDepend values;
				ini.GetAllKeys(section.c_str(), values);
				values.sort(CSimpleIniA::Entry::LoadOrder());

				if (values.empty()) {
					continue;
				}

				if (section.contains('|')) {
					auto splitSection = REX::STR::SPLIT(section, "|");  // [ANIO|conditions|traits]
					auto conditions = REX::STR::SPLIT(splitSection[1], ",");

					REX::INFO("\treading [{}] : {} conditions", splitSection[0], conditions.size());

					ConditionFilters processedConditions(
						path.substr(4) + "|" + splitSection[1] + (splitSection.size() > 2 ? "|" + splitSection[2] : ""),
						conditions,
						splitSection.size() > 2 ? splitSection[2] : std::string{});

					REX::INFO("\t\t\t{} anim object swaps found", values.size());
					for (const auto& key : values) {
						SwapAnioData::GetForms(path, key.pItem, [&](const RE::FormID a_baseID, SwapAnioData& a_swapData) {
							swapAnimObjectsConditional[a_baseID][processedConditions].emplace_back(a_swapData);
						});
					}
				} else {
					REX::INFO("\treading [{}]", section);
					REX::INFO("\t\t\t{} anim object swaps found", values.size());
					for (const auto& key : values) {
						SwapAnioData::GetForms(path, key.pItem, [&](const RE::FormID a_baseID, SwapAnioData& a_swapData) {
							swapAnimObjects[a_baseID].emplace_back(a_swapData);
						});
					}
				}
			}
		}

		REX::INFO("{:*^30}", "RESULT");

		REX::INFO("{} anim object swaps", swapAnimObjects.size());
		REX::INFO("{} conditional anim object swaps", swapAnimObjectsConditional.size());

		REX::INFO("{:*^30}", "CONFLICTS");

		bool hasConflicts = false;
		if (!swapAnimObjects.empty()) {
			for (auto& [baseID, swapDataVec] : swapAnimObjects) {
				if (swapDataVec.size() > 1) {
					const auto& winningRecord = swapDataVec.back();
					if (winningRecord.chance.chanceValue != 100) {  // ignore if winning record is randomized
						continue;
					}
					hasConflicts = true;
					auto winningForm = REX::STR::SPLIT(winningRecord.record, "|");
					REX::WARN("\t{}", winningForm[0]);
					REX::WARN("\t\twinning swap : {} ({})", winningForm[1], swapDataVec.back().path);
					REX::WARN("\t\t{} conflicts", swapDataVec.size() - 1);
					for (auto it = swapDataVec.rbegin() + 1; it != swapDataVec.rend(); ++it) {
						auto losingRecord = it->record.substr(it->record.find('|') + 1);
						REX::WARN("\t\t\t{} ({})", losingRecord, it->path);
					}
				}
			}
		}
		if (!hasConflicts) {
			REX::INFO("\tNo conflicts found");
		} else {
			RE::ConsoleLog::GetSingleton()->Print("[AOS] Conflicts found, check po3_AnimObjectSwapper.log in %s for more info\n", SKSE::log::log_directory()->string().c_str());
		}

		REX::INFO("{:*^30}", "END");
	}

	RE::TESObjectANIO* Manager::GetSwappedAnimObjectConditional(RE::Actor* a_actor, const RE::FormID a_animObjectID) const
	{
		if (const auto it = swapAnimObjectsConditional.find(a_animObjectID); it != swapAnimObjectsConditional.end()) {
			ConditionalInput input(a_actor);
			
			for (auto& [filters, swapDataVec] : it->second | std::ranges::views::reverse) {
				if (input.IsValid(filters)) {
					for (auto& swapData : swapDataVec | std::ranges::views::reverse) {
						if (const auto swapAnio = swapData.GetSwapAnio(a_actor)) {
							return swapAnio;
						}
					}
				}
			}
		}

		return nullptr;
	}

	RE::TESObjectANIO* Manager::GetSwappedAnimObject(RE::TESObjectREFR* a_user, RE::TESObjectANIO* a_animObject)
	{
		const auto baseANIO = a_animObject->GetFormID();
		const auto actor = a_user ? a_user->As<RE::Actor>() : nullptr;

		if (actor) {
			if (const auto swapAnio = GetSwappedAnimObjectConditional(actor, baseANIO)) {
				return swapAnio;
			}
		}

		if (const auto it = swapAnimObjects.find(baseANIO); it != swapAnimObjects.end()) {
			for (auto& swapData : it->second | std::ranges::views::reverse) {
				if (const auto swapAnio = swapData.GetSwapAnio(actor)) {
					return swapAnio;
				}
			}
		}

		return a_animObject;
	}
}
