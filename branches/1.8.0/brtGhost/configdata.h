#ifndef CONFIGDATA_H
#define CONFIGDATA_H

class CBNetConfigContainer
{
public:
		string Server;
		string CDKeyROC;
		string CDKeyTFT;
		string ServerAlias;
		string Locale;
		string CountryAbbrev;
		string Country;
		int LocaleID;

		string UserName;
		string UserPassword;
		string FirstChannel;
		string RootAdmin;

		bool Whereis;

		string BNETCommandTrigger;

		bool HoldFriends;
		bool HoldClan;
		bool PublicCommands;

		string BNLSServer;
		int BNLSPort;
		int BNLSWardenCookie;

		unsigned char War3Version;
		BYTEARRAY EXEVersion;
		BYTEARRAY EXEVersionHash;
		string PasswordHashType;
		string PVPGNRealmName;
		uint32_t MaxMessageLength;
};

class CConfigData
{
private:
	boost::property_tree::ptree data;

public:
	uint32_t port_host, port_reconnect, port_command, port_admingame;

	// paths
	string war3path, replaypath, mappath, mapcfgpath, savegamepath;

	// system
	bool tft;
	int m_LANWar3Version;					// config value: LAN warcraft 3 version
	string logfile;
	int logmethod;
	char m_CommandTrigger;					// config value: the command trigger inside games
	string m_BindAddress;					// config value: the address to host games on
	string m_LanguageFile;					// config value: language file
	string m_VirtualHostName;				// config value: virtual host name
	bool m_HideIPAddresses;					// config value: hide IP addresses from players
	bool m_CheckMultipleIPUsage;			// config value: check for multiple IP address usage
	bool m_LCPings;							// config value: use LC style pings (divide actual pings by two)
	uint32_t m_Latency;						// config value: the latency (by default)
	bool m_TCPNoDelay;						// config value: use Nagle's algorithm or not
	uint32_t m_GameLoadedPrintout;			// config value: how many secs should Ghost wait to printout the GameLoaded msg
	bool m_NoRank;
	bool m_NoStatsDota;
	bool m_DontShowSDForAdmins;				// config value: Show !sd for admins or not
	bool m_WhisperAllMessages;
	bool m_ShowRealSlotCount;
	uint32_t m_OwnerAccess;
	uint32_t m_AdminAccess;
	string m_ScoreFormula;					// score formula, config value
	uint32_t m_ScoreMinGames;					// score min games, config value
	bool m_Refresh0Uptime;
	string m_ExternalIP;					// our external IP
	uint32_t m_bnetpacketdelaymediumpvpgn;
	uint32_t m_bnetpacketdelaybigpvpgn;
	uint32_t m_bnetpacketdelaymedium;
	uint32_t m_bnetpacketdelaybig;
//	bool m_patch23;							// config value: use for patch 1.23
	bool m_patch21;							// config value: use for patch 1.21
	bool m_channeljoingreets;
	bool m_channeljoinmessage;
	bool m_udp_dontroute;
	string m_udp_broadcasttarget;
	uint16_t m_newLatency;
	bool m_newTimer;
	uint16_t m_newTimerResolution;
	string m_channeljoinexceptions;
	string m_FakePings;
	bool m_autoinsultlobby;

	// dynamic latency
	bool m_UseDynamicLatency;
	bool m_DynamicLatency2xPingMax;
	uint32_t m_DynamicLatencyMaxToAdd;
	uint32_t m_DynamicLatencyAddedToPing;
	bool m_DynamicLatencyIncreasewhenLobby;

	// games
	bool forceloadingame;
	uint32_t m_MaxGames;
	uint32_t m_SpoofChecks;					// config value: do automatic spoof checks or not
	bool m_RequireSpoofChecks;				// config value: require spoof checks or not
	bool m_RefreshMessages;					// config value: display refresh messages or not (by default)
	bool m_AutoLock;						// config value: auto lock games when the owner is present
	bool m_AutoSave;						// config value: auto save before someone disconnects
	uint32_t m_DropVoteTime;       			// config value: accept drop votes after this amount of seconds
	uint32_t m_AutoKickPing;				// config value: auto kick players with ping higher than this
	uint32_t m_SyncLimit;					// config value: the maximum number of packets a player can fall out of sync before starting the lag screen (by default)
	bool m_VoteKickAllowed;					// config value: if votekicks are allowed or not
	uint32_t m_VoteKickPercentage;			// config value: percentage of players required to vote yes for a votekick to pass
	string m_DefaultMap;					// config value: default map (map.cfg)
	string m_GameNameContainString;			// config value: if no empty, game name must contain this string
	uint32_t m_MatchMakingMethod;			// config value: the matchmaking method
	bool m_ForceAutoBalanceTeams;			// 
	bool m_Verbose;							// config value: show all info or just some
	bool m_RelayChatCommands;				// config value: show/hide issued commands
	bool m_forceautohclindota;				//
	bool m_AutoStartDotaGames;
	bool m_BlueIsOwner;
	bool m_BlueCanHCL;
	double m_AllowedScores;
	bool m_AllowNullScoredPlayers;
	bool m_UpdateDotaEloAfterGame;
	bool m_UpdateDotaScoreAfterGame;
	uint32_t m_minFFtime;					// config value: min time to enable !ff command.
	bool m_AddCreatorAsFriendOnHost;		// config value: add the creator as friend on hosting a game
	bool m_AutoHclFromGameName;				// config value: auto set HCL based on gamename, ignore map_defaulthcl
//	bool m_UsersCanHost;
//	bool m_SafeCanHost;
	bool m_NormalCountdown;
	uint32_t m_gameoverbasefallen;			// config value: initiate game over timer when x seconds have passed since world tree/frozen throne has fallen
	uint32_t m_gameoverminpercent;			// config value: initiate game over timer when percent of people remaining is less than.
	uint32_t m_gameoverminplayers;			// config value: initiate game over timer when there are less than this number of players.
	uint32_t m_gameovermaxteamdifference;	// config value: initiate game over timer if team unbalance is greater than this.
	bool m_ShuffleSlotsOnStart;
	bool m_ShowCountryNotAllowed;
	bool m_ShowScoresOnJoin;
	bool m_ShowNotesOnJoin;
	uint32_t m_AutoRehostDelay;
	bool m_RehostIfNameTaken;
	bool m_EndReq2ndTeamAccept;
	string m_DeniedCountries;
	string m_AllowedCountries;
	uint32_t m_gamestateinhouse;
	bool m_LobbyAnnounceUnoccupied;
	bool m_detectwtf;
	uint16_t m_LobbyTimeLimit;
	uint16_t m_LobbyTimeLimitMax;
	bool m_broadcastinlan;
	bool m_onlyownerscanstart;
	uint16_t m_MaxHostCounter;
	bool m_dropifdesync;
	bool m_HoldPlayersForRMK;

	// autohost
	double m_AutoHostAllowedScores;
	string m_AutoHostGameName;				// the base game name to auto host with
	string m_AutoHostMapCFG;				// the map config to auto host with
	string m_AutoHostOwner;
	string m_AutoHostServer;
	uint32_t m_AutoHostMaximumGames;		// maximum number of games to auto host
	bool m_AutoHostLocal;
	bool m_AutoHostAllowStart;
	string m_AutoHostCountries;
	uint32_t m_AutoHostAutoStartPlayers;	// when using auto hosting auto start the game when this many players have joined

	// replays
	uint32_t m_ReplayWar3Version;			// config value: replay warcraft 3 version (for saving replays)
	uint32_t m_ReplayBuildNumber;			// config value: replay build number (for saving replays)

	// admins
	bool m_LocalAdminMessages;				// config value: send local admin messages or not
	bool m_AdminMessages;					// config value: send admin messages or not
	bool m_PlaceAdminsHigherOnlyInDota;
	bool m_LanAdmins;						// config value: LAN people who join the game are considered admins
	bool m_LanRootAdmins;					// config value: LAN people who join the game are considered rootadmins
	bool m_LocalAdmins;						// config value: Local(localhost or GArena) people who join the game are considered admins
	bool m_NonAdminCommands;      			// config value: non admin commands available or not
	bool m_DetourAllMessagesToAdmins;
	string m_RootAdmins;
	bool m_AdminsCantUnbanRootadminBans;
	bool m_onlyownerscanswapadmins;

	// files
	string m_IPBlackListFile;
	string m_MOTDFile;
	string m_GameLoadedFile;
	string m_GameOverFile;

	// map_download
	uint32_t m_AllowDownloads;				// config value: allow map downloads or not
	bool m_PingDuringDownloads;				// config value: ping during map downloads or not
	bool m_ShowDownloadsInfo;				// config value: show info on downloads in progress
	uint32_t m_totaldownloadspeed;			// config value: total download speed allowed per all clients
	uint32_t m_clientdownloadspeed;			// config value: max download speed per client
	uint32_t m_maxdownloaders;				// config value: max clients allowed to download at once
	bool m_AdminsAndSafeCanDownload;
	bool m_ShowDownloadsInfoTime;

	// replay
	bool issavereplays;
	int32_t m_ReplayTimeShift;

	// features
	bool gproxy_enable;
	uint32_t m_ReconnectWaitTime;
	bool m_EnableBnetCommandInChannel;		// config value: if 0 - bot don't responding any commands on bnet channel (only whisper)
	bool m_BrtServerEnable;

	// admin game
	bool m_AdminGameCreate;					// config value: create the admin game or not
	string m_AdminGamePassword;				// config value: the admin game password
	string m_AdminGameMap;					// config value: the admin game map config to use

	// ban_and_warn
//	uint32_t m_BanMethod;					// config value: ban method (ban by name/ip/both)
	bool m_ReplaceBanWithWarn;
	uint32_t m_IPBanning;					// config value: handle ip bans
	uint32_t m_Banning;						// config value: handle name bans
	uint32_t m_BanTheWarnedPlayerQuota;		// number of warns needed to ban the player
	uint32_t m_BanTimeOfWarnedPlayer;		// number of days the tempban from the warns will last
	bool m_AutoWarnEarlyLeavers;			// config value: warn people who leave the hosted game early
	bool m_SafelistedBanImmunity;
	bool m_KickUnknownFromChannel;			// config value: kick unknown people from channel
	bool m_KickBannedFromChannel;			// config value: kick banned people from channel
	bool m_BanBannedFromChannel;			// config value: ban banned people from channel
	bool m_NotifyBannedPlayers;				// config value: send message to banned players that they have been banned
	bool m_RootAdminsSpoofCheck;			// config value: root admins need to spoof check or not.
	bool m_AdminsSpoofCheck;				// config value: admins need to spoof check or not.
	bool m_TwoLinesBanAnnouncement;			// config value: announce bans+reason on two lines, otherwise on one
	bool m_UnbanRemovesChannelBans;
	bool m_AutoBan;							// if we have auto ban on by default or not				
	uint32_t m_AutoBanTeamDiffMax;			// if we have more then x number of players more then other team
	uint32_t m_AutoBanTimer;				// time in mins the auto ban will stay on in game.
	bool m_AutoBanAll;						// ban even if it does not make game uneven
	uint32_t m_AutoBanFirstXLeavers;			// bans the first x leavers reguardless of even or not.
	uint32_t m_AutoBanGameLoading;			// Ban if leave during loading
	uint32_t m_AutoBanCountDown;			// Ban if leave during game start countdown.
	uint32_t m_AutoBanGameEndMins;			// Ban if not left around x mins of game end time.
	bool m_AdminsLimitedUnban;
	uint32_t m_InformAboutWarnsPrintout;	// config value: how many secs should ghost wait to printout the warn count to each player.
	bool m_SafeLobbyImmunity;
	uint32_t m_TBanLastTime;				// number of days to tempban when tbanlast
	uint32_t m_BanLastTime;					// number of days to tempban when banlast
	uint32_t m_BanTime;						// number of days to tempban when banning
	uint32_t m_WarnTimeOfWarnedPlayer;		// number of days the warn will last
	uint32_t m_GameNumToForgetAWarn;		// number of games till the first of the warns gets forgotten
	bool m_doautowarn;

	// auto censor
	string m_CensorWords;
	bool m_CensorMute;
	bool m_CensorMuteAdmins;
	uint32_t m_CensorMuteFirstSeconds;
	uint32_t m_CensorMuteSecondSeconds;
	uint32_t m_CensorMuteExcessiveSeconds;

	// wtv
	string m_wtvPath;
	string m_wtvPlayerName;
	bool m_wtv;

	// databases
	string m_DBType;
	string m_Sqlite3_file;

	string m_MySql_Server;
	string m_MySql_Database;
	string m_MySql_User;
	string m_MySql_Password;
	uint32_t m_MySql_Port;
	uint32_t m_MySql_BotID;

	// b.net configs
	vector<CBNetConfigContainer> m_BNETcfg;

public:
	CConfigData(void);
	~CConfigData(void);

	bool Parse( const string& nFileName );
	bool Save( const string& nFileName );
};

#endif