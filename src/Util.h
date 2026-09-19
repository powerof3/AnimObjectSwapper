#pragma once

namespace regex
{
	inline boost::regex generic{ R"(\((.*?)\))" };  // chanceR(50,12345) -> "50,12345"
}

namespace util
{
	void SanitizePath(std::string& a_string);

	std::pair<RE::FormID, RE::TESForm*> GetFormWithID(const std::string& a_str, bool a_resolveForm);
	RE::FormID                          GetFormID(const std::string& a_str);
	RE::FormID                          GetANIOFormID(const std::string& a_str);

	FormIDOrSet      GetSwapFormID(const std::string& a_str);
	FormIDOrderedSet GetANIOFormIDOrderedSet(const std::string& a_str);

	RE::TESForm* GetLocationOrCell(const RE::Actor* a_actor);
}
