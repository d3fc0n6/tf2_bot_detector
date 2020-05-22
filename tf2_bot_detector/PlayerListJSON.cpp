#include "PlayerListJSON.h"

#include <mh/text/string_insertion.hpp>
#include <nlohmann/json.hpp>

#include <filesystem>
#include <fstream>
#include <iomanip>

using namespace tf2_bot_detector;
using namespace std::string_literals;
using namespace std::string_view_literals;

static std::filesystem::path s_PlayerListPath("cfg/playerlist.json");

namespace tf2_bot_detector
{
	void to_json(nlohmann::json& j, const SteamID& d)
	{
		j = d.str();
	}
	void to_json(nlohmann::json& j, const PlayerAttributesList& d)
	{
		if (!j.is_array())
			j = nlohmann::json::array();
		else
			j.clear();

		if (d.HasAttribute(PlayerAttributes::Cheater))
			j.push_back("cheater");
		if (d.HasAttribute(PlayerAttributes::Suspicious))
			j.push_back("suspicious");
		if (d.HasAttribute(PlayerAttributes::Exploiter))
			j.push_back("exploiter");
		if (d.HasAttribute(PlayerAttributes::Racist))
			j.push_back("racist");
	}
	void to_json(nlohmann::json& j, const PlayerListData& d)
	{
		//j = json{ { "name", p.name }, { "address", p.address }, { "age", p.age } };

		j = nlohmann::json
		{
			{ "steamid", d.GetSteamID() },
			{ "attributes", d.m_Attributes }
		};
	}

	void from_json(const nlohmann::json& j, PlayerAttributesList& d)
	{
		d = {};

		if (!j.is_array())
			throw std::invalid_argument("json must be an array");

		for (const auto& attribute : j)
		{
			const auto& str = attribute.get<std::string_view>();
			if (str == "suspicious"sv)
				d.SetAttribute(PlayerAttributes::Suspicious);
			else if (str == "cheater"sv)
				d.SetAttribute(PlayerAttributes::Cheater);
			else if (str == "exploiter"sv)
				d.SetAttribute(PlayerAttributes::Exploiter);
			else if (str == "racist"sv)
				d.SetAttribute(PlayerAttributes::Racist);
			else
				throw std::runtime_error("Unknown player attribute type "s << std::quoted(str));
		}
	}
	void from_json(const nlohmann::json& j, PlayerListData& d)
	{
		d.m_Attributes = j.at("attributes").get<PlayerAttributesList>();

		if (auto lastSeen = j.find(""); lastSeen != j.end())
		{
			using clock = std::chrono::system_clock;
			using time_point = clock::time_point;
			using duration = clock::duration;
			d.m_LastSeenTime = clock::time_point(clock::duration(lastSeen->at("time").get<clock::duration::rep>()));
			d.m_LastSeenName = lastSeen->value<std::string>("player_name", "");
		}
	}
	void from_json(const nlohmann::json& j, SteamID& id)
	{
		id = SteamID(j.get<std::string_view>());
	}
}

PlayerListJSON::PlayerListJSON()
{
	LoadFile();
}

#if 0
static PlayerListData ParsePlayer(const nlohmann::json& json)
{
	PlayerListData retVal(SteamID(json.at("steamID").get<std::string>()));

	for (const auto& attribute : json.at("attributes"))
	{
		const auto& str = attribute.get<std::string_view>();
		if (str == "suspicious"sv)
			retVal.SetAttribute(PlayerAttributes::Suspicious);
		else if (str == "cheater"sv)
			retVal.SetAttribute(PlayerAttributes::Cheater);
		else if (str == "exploiter"sv)
			retVal.SetAttribute(PlayerAttributes::Exploiter);
		else if (str == "racist"sv)
			retVal.SetAttribute(PlayerAttributes::Racist);
		else
			throw std::runtime_error("Unknown player attribute type "s << std::quoted(str));
	}

	if (auto lastSeen = json.find(""); lastSeen != json.end())
	{
		using clock = std::chrono::system_clock;
		using time_point = clock::time_point;
		using duration = clock::duration;
		retVal.m_LastSeenTime = clock::time_point(clock::duration(lastSeen->at("time").get<clock::duration::rep>()));
		retVal.m_LastSeenName = lastSeen->value<std::string>("player_name", "");
	}

	return retVal;
}
#endif

void PlayerListJSON::LoadFile()
{
	nlohmann::json json;
	{
		std::ifstream file(s_PlayerListPath);
		file >> json;
	}

	m_Players.clear();

	const auto& players = json.at("players");
	for (const auto& player : json.at("players"))
	{
		const SteamID steamID(player.at("steamid").get<std::string>());
		PlayerListData parsed(steamID);
		player.get_to(parsed);
		m_Players.emplace(steamID, std::move(parsed));
	}
}

void PlayerListJSON::SaveFile() const
{
	nlohmann::json json =
	{
		{ "$schema", "./schema/playerlist.schema.json" }
	};

	auto& players = json["players"];
	for (const auto& pair : m_Players)
		players.push_back(pair.second);

	// Make sure we successfully serialize BEFORE we destroy our file
	auto jsonString = json.dump(1, '\t', true);
	{
		std::ofstream file(s_PlayerListPath);
		file << jsonString << '\n';
	}
}

const PlayerListData* PlayerListJSON::FindPlayerData(const SteamID& id) const
{
	if (auto found = m_Players.find(id); found != m_Players.end())
		return &found->second;

	return nullptr;
}

const PlayerAttributesList* PlayerListJSON::FindPlayerAttributes(const SteamID& id) const
{
	if (auto found = FindPlayerData(id))
		return &found->m_Attributes;

	return nullptr;
}

ModifyPlayerResult PlayerListJSON::ModifyPlayer(const SteamID& id,
	ModifyPlayerAction(*func)(PlayerListData& data, void* userData), void* userData)
{
	return ModifyPlayer(id, reinterpret_cast<ModifyPlayerAction(*)(PlayerListData&, const void*)>(func),
		static_cast<const void*>(userData));
}

ModifyPlayerResult PlayerListJSON::ModifyPlayer(const SteamID& id,
	ModifyPlayerAction(*func)(PlayerListData& data, const void* userData), const void* userData)
{
	PlayerListData* data = nullptr;
	if (auto found = m_Players.find(id); found != m_Players.end())
		data = &found->second;
	else
		data = &m_Players.emplace(id, PlayerListData(id)).first->second;

	const auto action = func(*data, userData);
	if (action == ModifyPlayerAction::Modified)
	{
		SaveFile();
		return ModifyPlayerResult::FileSaved;
	}
	else if (action == ModifyPlayerAction::NoChanges)
	{
		return ModifyPlayerResult::NoChanges;
	}
	else
	{
		throw std::runtime_error("Unexpected ModifyPlayerAction "s << +std::underlying_type_t<ModifyPlayerAction>(action));
	}
}

PlayerListData::PlayerListData(const SteamID& id) :
	m_SteamID(id)
{
}

bool PlayerAttributesList::HasAttribute(PlayerAttributes attribute) const
{
	switch (attribute)
	{
	case PlayerAttributes::Cheater:     return m_Cheater;
	case PlayerAttributes::Suspicious:  return m_Suspicious;
	case PlayerAttributes::Exploiter:   return m_Exploiter;
	case PlayerAttributes::Racist:      return m_Racist;
	}

	throw std::runtime_error("Unknown PlayerAttributes value "s << +std::underlying_type_t<PlayerAttributes>(attribute));
}

bool PlayerAttributesList::SetAttribute(PlayerAttributes attribute, bool set)
{
#undef HELPER
#define HELPER(value) \
	{ \
		auto old = (value); \
		(value) = set; \
		return old != (value); \
	}

	switch (attribute)
	{
	case PlayerAttributes::Cheater:     HELPER(m_Cheater);
	case PlayerAttributes::Suspicious:  HELPER(m_Suspicious);
	case PlayerAttributes::Exploiter:   HELPER(m_Exploiter);
	case PlayerAttributes::Racist:      HELPER(m_Racist);

	default:
		throw std::runtime_error("Unknown PlayerAttributes value "s << +std::underlying_type_t<PlayerAttributes>(attribute));
	}
}