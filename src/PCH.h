#pragma once

#define WIN32_LEAN_AND_MEAN
#define NOMINMAX

#include <ranges>
#include <set>

#include "RE/Skyrim.h"
#include "REX/REX.h"
#include "SKSE/SKSE.h"

#include <MergeMapperPluginAPI.h>

#include <boost/regex.hpp>
#include <boost/unordered/unordered_flat_map.hpp>
#include <boost/unordered/unordered_flat_set.hpp>
#include <spdlog/sinks/basic_file_sink.h>
#include <xbyak/xbyak.h>

#include <ClibUtil/distribution.hpp>
#include <ClibUtil/editorID.hpp>

#include <SimpleIni.h>
#undef ERROR

namespace distribution = clib_util::distribution;
namespace editorID = clib_util::editorID;

using namespace std::literals;
using namespace REX::STR::literals;

// for visting variants
template <class... Ts>
struct overload : Ts...
{
	using Ts::operator()...;
};

template <class K, class D, class H = boost::hash<K>, class KEqual = std::equal_to<K>>
using Map = boost::unordered_flat_map<K, D, H, KEqual>;

template <class K, class H = boost::hash<K>, class KEqual = std::equal_to<K>>
using Set = boost::unordered_flat_set<K, H, KEqual>;

template <class T>
using OrderedSet = std::set<T>;
using FormIDOrderedSet = OrderedSet<RE::FormID>;

using FormIDSet = std::vector<RE::FormID>;
using FormIDOrSet = std::variant<RE::FormID, FormIDSet>;

template <class T>
using FormIDMap = Map<RE::FormID, T>;

template <class K, class D>
class InsertionMap
{
public:
	D& operator[](const K& a_key)
	{
		for (auto& [key, data] : _map) {
			if (a_key == key) {
				return data;
			}
		}
		return _map.emplace_back(a_key, D{}).second;
	}

	template <class Comp>
	void sort(Comp&& a_comp)
	{
		std::ranges::stable_sort(_map, std::forward<Comp>(a_comp));
	}

	[[nodiscard]] auto begin() { return _map.begin(); }
	[[nodiscard]] auto end() { return _map.end(); }
	[[nodiscard]] auto begin() const { return _map.begin(); }
	[[nodiscard]] auto end() const { return _map.end(); }

	[[nodiscard]] bool        empty() const { return _map.empty(); }
	[[nodiscard]] std::size_t size() const { return _map.size(); }

private:
	std::vector<std::pair<K, D>> _map{};
};

namespace stl
{
	template <class T>
	void write_thunk_call(std::uintptr_t a_src)
	{
		auto& trampoline = REL::GetTrampoline();
		T::func = trampoline.write_call<5>(a_src, T::thunk);
	}
}

#ifdef SKYRIM_AE
#	define OFFSET(se, ae) ae
#else
#	define OFFSET(se, ae) se
#endif

#include "Util.h"
#include "Version.h"
