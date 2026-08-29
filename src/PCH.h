#pragma once

#define NOMINMAX

#include <ranges>

#include "RE/Skyrim.h"
#include "REX/REX.h"
#include "SKSE/SKSE.h"

#include <boost/unordered/unordered_flat_map.hpp>
#include <boost/unordered/unordered_flat_set.hpp>
#include <spdlog/sinks/basic_file_sink.h>
#include <xbyak/xbyak.h>

#include <ClibUtil/distribution.hpp>
#include <ClibUtil/editorID.hpp>

using namespace std::literals;
namespace dist = clib_util::distribution;
namespace edid = clib_util::editorID;

template <class K, class D, class H = boost::hash<K>, class KEqual = std::equal_to<K>>
using Map = boost::unordered_flat_map<K, D, H, KEqual>;

template <class K, class H = boost::hash<K>, class KEqual = std::equal_to<K>>
using Set = boost::unordered_flat_set<K, H, KEqual>;

using FormIDSet = Set<RE::FormID>;
using FormIDMap = Map<RE::FormID, FormIDSet>;

using FormIDStr = std::variant<RE::FormID, std::string>;
using FormIDStrVec = std::vector<FormIDStr>;

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

#include "Version.h"
