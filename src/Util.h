#pragma once

namespace regex
{
	inline static boost::regex generic{ R"(\((.*?)\))" };  // chanceR(50,12345) -> "50,12345"
}

namespace util
{
	std::pair<RE::FormID, RE::TESForm*> GetFormWithID(const std::string& a_str, bool a_resolveForm);
	RE::FormID                          GetFormID(const std::string& a_str);
	RE::FormID                          GetANIOFormID(const std::string& a_str);

	FormIDOrSet GetSwapFormID(const std::string& a_str);
}
