#include "Manager.h"
#include "LookupFilters.h"
#include "MergeMapperPluginAPI.h"

namespace AnimObjectSwap
{
	RE::FormID Manager::GetFormID(const std::string& a_str)
	{
		if (a_str.contains("~"sv)) {
			if (auto splitID = string::split(a_str, "~"); splitID.size() == 2) {
				const auto formID = string::lexical_cast<RE::FormID>(splitID[0], true);
				const auto& modName = splitID[1];
				if (g_mergeMapperInterface) {
					const auto [mergedModName, mergedFormID] = g_mergeMapperInterface->GetNewFormID(modName.c_str(), formID);
					return RE::TESDataHandler::GetSingleton()->LookupFormID(mergedFormID, (const char*)mergedModName);
				} else {
					return RE::TESDataHandler::GetSingleton()->LookupFormID(formID, modName);
				}
			}
		}
		if (const auto form = RE::TESForm::LookupByEditorID(a_str); form) {
			return form->GetFormID();
		}
		return static_cast<RE::FormID>(0);
	}

	std::string Manager::GetEditorID(const RE::TESForm* a_form)
	{
		switch (a_form->GetFormType()) {
		case RE::FormType::Keyword:
		case RE::FormType::LocationRefType:
		case RE::FormType::Action:
		case RE::FormType::MenuIcon:
		case RE::FormType::Global:
		case RE::FormType::HeadPart:
		case RE::FormType::Race:
		case RE::FormType::Sound:
		case RE::FormType::Script:
		case RE::FormType::Navigation:
		case RE::FormType::Cell:
		case RE::FormType::WorldSpace:
		case RE::FormType::Land:
		case RE::FormType::NavMesh:
		case RE::FormType::Dialogue:
		case RE::FormType::Quest:
		case RE::FormType::Idle:
		case RE::FormType::AnimatedObject:
		case RE::FormType::ImageAdapter:
		case RE::FormType::VoiceType:
		case RE::FormType::Ragdoll:
		case RE::FormType::DefaultObject:
		case RE::FormType::MusicType:
		case RE::FormType::StoryManagerBranchNode:
		case RE::FormType::StoryManagerQuestNode:
		case RE::FormType::StoryManagerEventNode:
		case RE::FormType::SoundRecord:
			return a_form->GetFormEditorID();
		default:
			{
				static auto tweaks = GetModuleHandle(L"po3_Tweaks");
				static auto func = reinterpret_cast<_GetFormEditorID>(GetProcAddress(tweaks, "GetFormEditorID"));
				if (func) {
					return func(a_form->formID);
				}
				return std::string();
			}
		}
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
								conditionalSwap.conditions.ANY.push_back(condition);
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
									logger::error("{}				Unable to find swap animObject [{}] (invalid formID/editorID)", path, swapAnioStr);
								}
							}

							if (!noConditions) {
								conditionalSwap.swappedAnimObjects = tempSwapAnimObjects;
								_animObjectsConditional[baseAnio].push_back(conditionalSwap);
							}
						} else {
							logger::error("{}			Unable to find base animObject [{}] (invalid formID/editorID)", path, splitValue[0]);
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
			auto randIt = stl::RNG::GetSingleton()->Generate<std::size_t>(0, setEnd);

			return RE::TESForm::LookupByID<RE::TESObjectANIO>(*std::next(a_animObjects.begin(), randIt));
		}
	}
}
