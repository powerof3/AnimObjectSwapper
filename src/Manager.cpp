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

		std::uint32_t fileIndex = 0;

		for (auto& path : configs) {
			fileIndex++;

			REX::INFO("INI : {}", path);

			CSimpleIniA ini;
			ini.SetUnicode();
			ini.SetMultiKey();
			ini.SetAllowKeyOnly();

			if (const auto rc = ini.LoadFile(path.c_str()); rc < 0) {
				REX::ERROR("\tcouldn't read INI");
				continue;
			}

			const auto configPath = std::make_shared<std::string>(path);

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

					REX::INFO("\treading [{}] : {} conditions", section, conditions.size());

					auto processedConditions = std::make_shared<const ConditionFilters>(
						conditions,
						splitSection.size() > 2 ? splitSection[2] : std::string{},  // traits
						fileIndex);

					REX::INFO("\t\t\t{} anim object swaps found", values.size());
					for (const auto& key : values) {
						SwapAnioData::GetForms(configPath, key.pItem, [&](const RE::FormID a_baseID, SwapAnioData& a_swapData) {
							swapAnimObjectsConditional[a_baseID][processedConditions].emplace_back(a_swapData);
						});
					}
				} else {
					REX::INFO("\treading [{}]", section);
					REX::INFO("\t\t\t{} anim object swaps found", values.size());
					for (const auto& key : values) {
						SwapAnioData::GetForms(configPath, key.pItem, [&](const RE::FormID a_baseID, SwapAnioData& a_swapData) {
							a_swapData.fileIndex = fileIndex;
							swapAnimObjects[a_baseID].emplace_back(a_swapData);
						});
					}
				}
			}
		}

		// sort normal AND conditional map so entries are evaluated top to bottom
		for (auto& swapDataVec : swapAnimObjects | std::views::values) {
			std::ranges::stable_sort(swapDataVec, [](const auto& a_lhs, const auto& a_rhs) {
				return a_lhs.fileIndex > a_rhs.fileIndex;
			});
		}

		for (auto& conditionalMap : swapAnimObjectsConditional | std::views::values) {
			conditionalMap.sort([](const auto& a_lhs, const auto& a_rhs) {
				return a_lhs.first->fileIndex > a_rhs.first->fileIndex;
			});
		}

		REX::INFO("{:*^30}", "RESULT");

		REX::INFO("{} anim object swaps", swapAnimObjects.size());
		for (auto& [baseID, swapDataVec] : swapAnimObjects) {
			auto base = RE::TESForm::LookupByID<RE::TESObjectANIO>(baseID);
			REX::INFO("\t{} [{:X}] : {} swaps", base->GetFormEditorID(), base->GetFormID(), swapDataVec.size());
		}

		REX::INFO("{} conditional anim object swaps", swapAnimObjectsConditional.size());
		for (auto& [baseID, swapDataMap] : swapAnimObjectsConditional) {
			auto base = RE::TESForm::LookupByID<RE::TESObjectANIO>(baseID);
			
			std::size_t size = 0;
			for (const auto& swapDataVec : swapDataMap | std::views::values) {
				size += swapDataVec.size();
			}

			REX::INFO("\t{} [{:X}] : {} swaps", base->GetFormEditorID(), base->GetFormID(), size);
		}

		REX::INFO("{:*^30}", "CONFLICTS");

		bool hasConflicts = false;
		if (!swapAnimObjects.empty()) {
			for (auto& swapDataVec : swapAnimObjects | std::views::values) {
				if (swapDataVec.size() > 1) {
					const auto& winningRecord = swapDataVec.front();
					if (winningRecord.chance.chanceValue != 100) {  // ignore if winning record is randomized
						continue;
					}
					hasConflicts = true;
					auto winningForm = REX::STR::SPLIT(winningRecord.record, "|");
					REX::WARN("\t{}", winningForm[0]);
					REX::WARN("\t\twinning swap : {} ({})", winningForm[1], *winningRecord.path);
					REX::WARN("\t\t{} conflicts", swapDataVec.size() - 1);
					for (auto it = swapDataVec.begin() + 1; it != swapDataVec.end(); ++it) {
						auto losingRecord = it->record.substr(it->record.find('|') + 1);
						REX::WARN("\t\t\t{} ({})", losingRecord, *it->path);
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

	RE::TESObjectANIO* Manager::GetSwappedAnimObjectConditional(RE::Actor* a_actor, const RE::TESObjectANIO* a_animObject) const
	{
		if (const auto it = swapAnimObjectsConditional.find(a_animObject->GetFormID()); it != swapAnimObjectsConditional.end()) {
			ConditionalInput input(a_actor);

			for (auto& [filters, swapDataVec] : it->second) {
				if (input.IsValid(*filters)) {
					for (auto& swapData : swapDataVec) {
						if (const auto swapAnio = swapData.GetSwapAnio(a_actor, a_animObject)) {
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
		const auto actor = a_user ? a_user->As<RE::Actor>() : nullptr;

		if (actor) {
			if (const auto swapAnio = GetSwappedAnimObjectConditional(actor, a_animObject)) {
				return swapAnio;
			}
		}

		if (const auto it = swapAnimObjects.find(a_animObject->GetFormID()); it != swapAnimObjects.end()) {
			for (auto& swapData : it->second) {
				if (const auto swapAnio = swapData.GetSwapAnio(actor, a_animObject)) {
					return swapAnio;
				}
			}
		}

		return a_animObject;
	}
}
