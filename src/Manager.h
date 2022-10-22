#pragma once

namespace AnimObjectSwap
{
	template <class K, class D>
	using Map = robin_hood::unordered_flat_map<K, D>;
	using FormIDSet = robin_hood::unordered_flat_set<RE::FormID>;
	using FormIDMap = Map<RE::FormID, FormIDSet>;

	using FormIDStr = std::variant<RE::FormID, std::string>;
	using FormIDStrVec = std::vector<FormIDStr>;

	struct Traits
	{
		RE::SEX sex{ RE::SEX::kNone };
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
		FormIDSet swappedAnimObjects{};
	};

	class Manager
	{
	public:
		[[nodiscard]] static Manager* GetSingleton()
		{
			static Manager singleton;
			return std::addressof(singleton);
		}

		static std::string GetEditorID(const RE::TESForm* a_form);

		bool LoadForms();
		RE::TESObjectANIO* GetSwappedAnimObject(RE::TESObjectREFR* a_user, RE::TESObjectANIO* a_animObject);

	protected:
		Manager() = default;
		Manager(const Manager&) = delete;
		Manager(Manager&&) = delete;
		~Manager() = default;

		Manager& operator=(const Manager&) = delete;
		Manager& operator=(Manager&&) = delete;

	private:
		using _GetFormEditorID = const char* (*)(std::uint32_t);

		static RE::FormID GetFormID(const std::string& a_str);

		[[nodiscard]] RE::TESObjectANIO* GetSwappedAnimObject(const FormIDSet& a_animObject) const;

		FormIDMap _animObjects;
		Map<RE::FormID, std::vector<ConditionalSwap>> _animObjectsConditional;
	};
}
