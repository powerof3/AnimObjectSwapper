#pragma once

namespace AnimObjectSwap
{
	struct Traits
	{
		RE::SEX             sex{ RE::SEX::kNone };
		std::optional<bool> child{ std::nullopt };
	};

	struct Conditions
	{
		FormIDStrVec ALL{};
		FormIDStrVec NOT{};
		FormIDStrVec MATCH{};
		FormIDStrVec ANY{};

		Traits traits{};
	};

	struct ConditionalSwap
	{
		Conditions conditions{};
		FormIDSet  swappedAnimObjects{};
	};

	class Manager : public REX::TSingleton<Manager>
	{
	public:
		bool               LoadForms();
		RE::TESObjectANIO* GetSwappedAnimObject(RE::TESObjectREFR* a_user, RE::TESObjectANIO* a_animObject);

	private:
		static RE::FormID GetFormID(const std::string& a_str);

		[[nodiscard]] RE::TESObjectANIO* GetSwappedAnimObject(const FormIDSet& a_animObject) const;

		FormIDMap                                     _animObjects;
		Map<RE::FormID, std::vector<ConditionalSwap>> _animObjectsConditional;
	};
}
