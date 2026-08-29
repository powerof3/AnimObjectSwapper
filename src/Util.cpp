#include "Util.h"

namespace util
{
	std::pair<RE::FormID, RE::TESForm*> GetFormWithID(const std::string& a_str, bool a_resolveForm)
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

			return { resolvedID, (a_resolveForm && resolvedID != 0) ? RE::TESForm::LookupByID(resolvedID) : nullptr };
		}
		if (REX::STR::IS_ONLY_HEX(a_str, true)) {
			const auto formID = REX::STR::TO_NUM<RE::FormID>(a_str, true);
			const auto form = RE::TESForm::LookupByID(formID);
			return { form ? formID : static_cast<RE::FormID>(0), form };
		}
		if (const auto form = RE::TESForm::LookupByEditorID(a_str)) {
			return { form->GetFormID(), form };
		}
		return {};
	}

	RE::FormID GetFormID(const std::string& a_str)
	{
		return GetFormWithID(a_str, false).first;
	}

	RE::FormID GetANIOFormID(const std::string& a_str)
	{
		const auto& [formID, form] = GetFormWithID(a_str, true);
		return form && form->Is(RE::FormType::AnimatedObject) ? formID : static_cast<RE::FormID>(0);
	}

	FormIDOrSet GetSwapFormID(const std::string& a_str)
	{
		if (a_str.contains(",")) {
			FormIDSet  set;
			const auto IDStrs = REX::STR::SPLIT(a_str, ",");
			set.reserve(IDStrs.size());
			for (auto& IDStr : IDStrs) {
				if (auto formID = GetANIOFormID(IDStr); formID != 0) {
					set.emplace_back(formID);
				} else {
					REX::ERROR("\t\t\tfailed to process {} (SWAP formID not found or not an AnimObject)", IDStr);
				}
			}
			std::ranges::sort(set);
			const auto dupes = std::ranges::unique(set);
			set.erase(dupes.begin(), dupes.end());
			return set;
		} else {
			return GetANIOFormID(a_str);
		}
	}
}
