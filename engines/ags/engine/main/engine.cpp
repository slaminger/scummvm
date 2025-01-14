/* ScummVM - Graphic Adventure Engine
 *
 * ScummVM is the legal property of its developers, whose names
 * are too numerous to list here. Please refer to the COPYRIGHT
 * file distributed with this source distribution.
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License
 * as published by the Free Software Foundation; either version 2
 * of the License, or (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA 02110-1301, USA.
 *
 */

//
// Engine initialization
//

#include "ags/shared/core/platform.h"
#include "ags/globals.h"
#include "ags/engine/main/mainheader.h"
#include "ags/engine/ac/asset_helper.h"
#include "ags/shared/ac/common.h"
#include "ags/engine/ac/character.h"
#include "ags/engine/ac/characterextras.h"
#include "ags/shared/ac/characterinfo.h"
#include "ags/engine/ac/draw.h"
#include "ags/engine/ac/game.h"
#include "ags/engine/ac/gamesetup.h"
#include "ags/shared/ac/gamesetupstruct.h"
#include "ags/engine/ac/global_character.h"
#include "ags/engine/ac/global_game.h"
#include "ags/engine/ac/gui.h"
#include "ags/engine/ac/lipsync.h"
#include "ags/engine/ac/objectcache.h"
#include "ags/engine/ac/path_helper.h"
#include "ags/engine/ac/sys_events.h"
#include "ags/engine/ac/roomstatus.h"
#include "ags/engine/ac/speech.h"
#include "ags/shared/ac/spritecache.h"
#include "ags/engine/ac/translation.h"
#include "ags/engine/ac/viewframe.h"
#include "ags/engine/ac/dynobj/scriptobject.h"
#include "ags/engine/ac/dynobj/scriptsystem.h"
#include "ags/shared/core/assetmanager.h"
#include "ags/engine/debugging/debug_log.h"
#include "ags/engine/debugging/debugger.h"
#include "ags/shared/debugging/out.h"
#include "ags/shared/font/agsfontrenderer.h"
#include "ags/shared/font/fonts.h"
#include "ags/engine/gfx/graphicsdriver.h"
#include "ags/engine/gfx/gfxdriverfactory.h"
#include "ags/engine/gfx/ddb.h"
#include "ags/engine/main/config.h"
#include "ags/engine/main/game_file.h"
#include "ags/engine/main/game_start.h"
#include "ags/engine/main/engine.h"
#include "ags/engine/main/engine_setup.h"
#include "ags/engine/main/graphics_mode.h"
#include "ags/engine/main/main.h"
#include "ags/engine/main/main_allegro.h"
#include "ags/engine/media/audio/audio_system.h"
#include "ags/engine/platform/util/pe.h"
#include "ags/shared/gfx/image.h"
#include "ags/shared/util/directory.h"
#include "ags/shared/util/error.h"
#include "ags/shared/util/misc.h"
#include "ags/shared/util/path.h"
#include "ags/globals.h"
#include "ags/ags.h"
#include "common/fs.h"

namespace AGS3 {

using namespace AGS::Shared;
using namespace AGS::Engine;

extern color palette[256];

#define ALLEGRO_KEYBOARD_HANDLER

bool engine_init_allegro() {
	Debug::Printf(kDbgMsg_Info, "Initializing allegro");

	_G(our_eip) = -199;
	// Initialize allegro
	set_uformat(U_ASCII);
	if (install_allegro()) {
		const char *al_err = get_allegro_error();
		const char *user_hint = _G(platform)->GetAllegroFailUserHint();
		_G(platform)->DisplayAlert("Unable to initialize Allegro system driver.\n%s\n\n%s",
		                       al_err[0] ? al_err : "Allegro library provided no further information on the problem.",
		                       user_hint);
		return false;
	}
	return true;
}

void engine_setup_allegro() {
	// Setup allegro using constructed config string
	const char *al_config_data = "[mouse]\n"
	                             "mouse_accel_factor = 0\n";
	override_config_data(al_config_data, ustrsize(al_config_data));
}

void engine_setup_window() {
	Debug::Printf(kDbgMsg_Info, "Setting up window");

	_G(our_eip) = -198;
	//set_window_title("Adventure Game Studio");
	_G(our_eip) = -197;

	_G(platform)->SetGameWindowIcon();
}

// Starts up setup application, if capable.
// Returns TRUE if should continue running the game, otherwise FALSE.
bool engine_run_setup(const String &exe_path, ConfigTree &cfg, int &app_res) {
	app_res = EXIT_NORMAL;
#if AGS_PLATFORM_OS_WINDOWS
	{
		String cfg_file = find_user_cfg_file();
		if (cfg_file.IsEmpty()) {
			app_res = EXIT_ERROR;
			return false;
		}

		Debug::Printf(kDbgMsg_Info, "Running Setup");

		ConfigTree cfg_out;
		SetupReturnValue res = _G(platform)->RunSetup(cfg, cfg_out);
		if (res != kSetup_Cancel) {
			if (!IniUtil::Merge(cfg_file, cfg_out)) {
				_G(platform)->DisplayAlert("Unable to write to the configuration file (error code 0x%08X).\n%s",
				                       _G(platform)->GetLastSystemError(), _G(platform)->GetDiskWriteAccessTroubleshootingText());
			}
		}
		if (res != kSetup_RunGame)
			return false;

		// TODO: investigate if the full program restart may (should) be avoided

		// Just re-reading the config file seems to cause a caching
		// problem on Win9x, so let's restart the process.
		allegro_exit();
		char quotedpath[MAX_PATH];
		snprintf(quotedpath, MAX_PATH, "\"%s\"", exe_path.GetCStr());
		_spawnl(_P_OVERLAY, exe_path, quotedpath, NULL);
	}
#endif
	return true;
}

void engine_force_window() {
	// Force to run in a window, override the config file
	// TODO: actually overwrite config tree instead
	if (_G(force_window) == 1) {
		_GP(usetup).Screen.DisplayMode.Windowed = true;
		_GP(usetup).Screen.DisplayMode.ScreenSize.SizeDef = kScreenDef_ByGameScaling;
	} else if (_G(force_window) == 2) {
		_GP(usetup).Screen.DisplayMode.Windowed = false;
		_GP(usetup).Screen.DisplayMode.ScreenSize.SizeDef = kScreenDef_MaxDisplay;
	}
}

String find_game_data_in_directory(const String &path) {
	Common::String test_file;
	String first_nonstd_fn;

	Common::FSNode folder(path);
	Common::FSList files;
	if (folder.getChildren(files, Common::FSNode::kListFilesOnly)) {
		// Select first found data file; files with standart names (*.ags) have
		// higher priority over files with custom names.
		for (Common::FSList::iterator it = files.begin(); it != files.end(); ++it) {
			test_file = it->getName();

			// Add a bit of sanity and do not parse contents of the 10k-files-large
			// digital sound libraries.
			// NOTE: we could certainly benefit from any kind of flag in file lib
			// that would tell us this is the main lib without extra parsing.
			if (test_file.hasSuffixIgnoreCase(".vox"))
				continue;

			// *.ags is a standart cross-platform file pattern for AGS games,
			// ac2game.dat is a legacy file name for very old games,
			// *.exe is a MS Win executable; it is included to this case because
			// users often run AGS ports with Windows versions of games.
			bool is_std_name = test_file.hasSuffixIgnoreCase(".ags") ||
				test_file.equalsIgnoreCase("ac2game.dat") ||
				test_file.hasSuffixIgnoreCase(".exe");
			if (is_std_name || first_nonstd_fn.IsEmpty()) {
				test_file = it->getName();

				if (IsMainGameLibrary(test_file)) {
					if (is_std_name) {
						return test_file;
					} else
						first_nonstd_fn = test_file;
				}
			}
		}
	}

	return first_nonstd_fn;
}

bool search_for_game_data_file(String &filename, String &search_path) {
	Debug::Printf("Looking for the game data file");
	// 1. From command line argument, treated as a directory
	if (!_G(cmdGameDataPath).IsEmpty()) {
		// set from cmd arg (do any conversions if needed)
		filename = _G(cmdGameDataPath);
		if (!filename.IsEmpty() && Path::IsDirectory(filename)) {
			search_path = filename;
			filename = find_game_data_in_directory(search_path);
		}
	}
	// 2.2. Search in the provided data dir
	else if (!_GP(usetup).data_files_dir.IsEmpty()) {
		search_path = _GP(usetup).data_files_dir;
		filename = find_game_data_in_directory(search_path);
	}
	// 3. Look in known locations
	else {
#ifdef DEPRECATED
		// 3.1. Look for attachment in the running executable
		//
		// this will use argument zero, the executable's name
		filename = GetPathFromCmdArg(0);
		if (filename.IsEmpty() || !Shared::AssetManager::IsDataFile(filename)) {
			// 3.2 Look in current directory
			search_path = Directory::GetCurrentDirectory();
			filename = find_game_data_in_directory(search_path);
			if (filename.IsEmpty()) {
				// 3.3 Look in executable's directory (if it's different from current dir)
				if (Path::ComparePaths(_G(appDirectory), search_path)) {
					search_path = _G(appDirectory);
					filename = find_game_data_in_directory(search_path);
				}
			}
		}
#endif
	}

	// Finally, store game file's absolute path, or report error
	if (filename.IsEmpty()) {
		Debug::Printf(kDbgMsg_Error, "Game data file could not be found. Search path used: '%s'", search_path.GetCStr());
		return false;
	}
	filename = Path::MakeAbsolutePath(filename);
	Debug::Printf(kDbgMsg_Info, "Located game data file: %s", filename.GetCStr());
	return true;
}

// Try to initialize main game package found at the given path
bool engine_try_init_gamedata(String gamepak_path) {
	// Search for an available game package in the known locations
	AssetError err = AssetManager::SetDataFile(gamepak_path);
	if (err != kAssetNoError) {
		_G(platform)->DisplayAlert("ERROR: The game data is missing, is of unsupported format or corrupt.\nFile: '%s'", gamepak_path.GetCStr());
		return false;
	}
	return true;
}

void engine_init_fonts() {
	Debug::Printf(kDbgMsg_Info, "Initializing TTF renderer");

	init_font_renderer();
}

void engine_init_mouse() {
	int res = minstalled();
	if (res < 0)
		Debug::Printf(kDbgMsg_Info, "Initializing mouse: failed");
	else
		Debug::Printf(kDbgMsg_Info, "Initializing mouse: number of buttons reported is %d", res);
	_GP(mouse).SetSpeed(_GP(usetup).mouse_speed);
}

void engine_locate_speech_pak() {
	_GP(play).want_speech = -2;

	if (!_GP(usetup).no_speech_pack) {
		String speech_file = "speech.vox";
		String speech_filepath = find_assetlib(speech_file);
		if (!speech_filepath.IsEmpty()) {
			Debug::Printf("Initializing speech vox");
			if (AssetManager::SetDataFile(speech_filepath) != Shared::kAssetNoError) {
				_G(platform)->DisplayAlert("Unable to read voice pack, file could be corrupted or of unknown format.\nSpeech voice-over will be disabled.");
				AssetManager::SetDataFile(_GP(ResPaths).GamePak.Path); // switch back to the main data pack
				return;
			}
			// TODO: why is this read right here??? move this to InitGameState!
			Stream *speechsync = AssetManager::OpenAsset("syncdata.dat");
			if (speechsync != nullptr) {
				// this game has voice lip sync
				int lipsync_fmt = speechsync->ReadInt32();
				if (lipsync_fmt != 4) {
					Debug::Printf(kDbgMsg_Info, "Unknown speech lip sync format (%d).\nLip sync disabled.", lipsync_fmt);
				} else {
					_G(numLipLines) = speechsync->ReadInt32();
					_G(splipsync) = (SpeechLipSyncLine *)malloc(sizeof(SpeechLipSyncLine) * _G(numLipLines));
					for (int ee = 0; ee < _G(numLipLines); ee++) {
						_G(splipsync)[ee].numPhonemes = speechsync->ReadInt16();
						speechsync->Read(_G(splipsync)[ee].filename, 14);
						_G(splipsync)[ee].endtimeoffs = (int32_t *)malloc(_G(splipsync)[ee].numPhonemes * sizeof(int32_t));
						speechsync->ReadArrayOfInt32(_G(splipsync)[ee].endtimeoffs, _G(splipsync)[ee].numPhonemes);
						_G(splipsync)[ee].frame = (short *)malloc(_G(splipsync)[ee].numPhonemes * sizeof(short));
						speechsync->ReadArrayOfInt16(_G(splipsync)[ee].frame, _G(splipsync)[ee].numPhonemes);
					}
				}
				delete speechsync;
			}
			AssetManager::SetDataFile(_GP(ResPaths).GamePak.Path); // switch back to the main data pack
			Debug::Printf(kDbgMsg_Info, "Voice pack found and initialized.");
			_GP(play).want_speech = 1;
		} else if (Path::ComparePaths(_GP(ResPaths).DataDir, get_voice_install_dir()) != 0) {
			// If we have custom voice directory set, we will enable voice-over even if speech.vox does not exist
			Debug::Printf(kDbgMsg_Info, "Voice pack was not found, but voice installation directory is defined: enabling voice-over.");
			_GP(play).want_speech = 1;
		}
		_GP(ResPaths).SpeechPak.Name = speech_file;
		_GP(ResPaths).SpeechPak.Path = speech_filepath;
	}
}

void engine_locate_audio_pak() {
	_GP(play).separate_music_lib = 0;
	String music_file = _GP(game).GetAudioVOXName();
	String music_filepath = find_assetlib(music_file);
	if (!music_filepath.IsEmpty()) {
		if (AssetManager::SetDataFile(music_filepath) == kAssetNoError) {
			AssetManager::SetDataFile(_GP(ResPaths).GamePak.Path);
			Debug::Printf(kDbgMsg_Info, "%s found and initialized.", music_file.GetCStr());
			_GP(play).separate_music_lib = 1;
			_GP(ResPaths).AudioPak.Name = music_file;
			_GP(ResPaths).AudioPak.Path = music_filepath;
		} else {
			_G(platform)->DisplayAlert("Unable to initialize digital audio pack '%s', file could be corrupt or of unsupported format.",
			                       music_file.GetCStr());
		}
	}
}

void engine_init_keyboard() {
#ifdef ALLEGRO_KEYBOARD_HANDLER
	Debug::Printf(kDbgMsg_Info, "Initializing keyboard");

	install_keyboard();
#endif
#if AGS_PLATFORM_OS_LINUX
//	setlocale(LC_NUMERIC, "C"); // needed in X platform because install keyboard affects locale of printfs
#endif
}

void engine_init_timer() {
	Debug::Printf(kDbgMsg_Info, "Install timer");

	skipMissedTicks();
}

void engine_init_audio() {
#if !AGS_PLATFORM_SCUMMVM
	if (_GP(usetup).audio_backend != 0) {
		Debug::Printf("Initializing audio");
		audio_core_init(); // audio core system
	}
#endif
	_G(our_eip) = -181;

	if (_GP(usetup).audio_backend == 0) {
		// all audio is disabled
		// and the voice mode should not go to Voice Only
		_GP(play).want_speech = -2;
		_GP(play).separate_music_lib = 0;
	}
}

void engine_init_debug() {
	if ((_G(debug_flags) & (~DBG_DEBUGMODE)) > 0) {
		_G(platform)->DisplayAlert("Engine debugging enabled.\n"
		                       "\nNOTE: You have selected to enable one or more engine debugging options.\n"
		                       "These options cause many parts of the game to behave abnormally, and you\n"
		                       "may not see the game as you are used to it. The point is to test whether\n"
		                       "the engine passes a point where it is crashing on you normally.\n"
		                       "[Debug flags enabled: 0x%02X]", _G(debug_flags));
	}
}

void engine_init_rand() {
	_GP(play).randseed = g_system->getMillis();
	::AGS::g_vm->setRandomNumberSeed(_GP(play).randseed);
}

void engine_init_pathfinder() {
	init_pathfinder(_G(loaded_game_file_version));
}

void engine_pre_init_gfx() {
	//Debug::Printf("Initialize gfx");

	//_G(platform)->InitialiseAbufAtStartup();
}

int engine_load_game_data() {
	Debug::Printf("Load game data");
	_G(our_eip) = -17;
	HError err = load_game_file();
	if (!err) {
		_G(proper_exit) = 1;
		_G(platform)->FinishedUsingGraphicsMode();
		display_game_file_error(err);
		return EXIT_ERROR;
	}
	return 0;
}

int engine_check_register_game() {
	if (_G(justRegisterGame)) {
		_G(platform)->RegisterGameWithGameExplorer();
		_G(proper_exit) = 1;
		return EXIT_NORMAL;
	}

	if (_G(justUnRegisterGame)) {
		_G(platform)->UnRegisterGameWithGameExplorer();
		_G(proper_exit) = 1;
		return EXIT_NORMAL;
	}

	return 0;
}

void engine_init_title() {
	_G(our_eip) = -91;
	::AGS::g_vm->set_window_title(_GP(game).gamename);
	Debug::Printf(kDbgMsg_Info, "Game title: '%s'", _GP(game).gamename);
}

void engine_init_directories() {
	Debug::Printf(kDbgMsg_Info, "Data directory: %s", _GP(usetup).data_files_dir.GetCStr());
	if (!_GP(usetup).install_dir.IsEmpty())
		Debug::Printf(kDbgMsg_Info, "Optional install directory: %s", _GP(usetup).install_dir.GetCStr());
	if (!_GP(usetup).install_audio_dir.IsEmpty())
		Debug::Printf(kDbgMsg_Info, "Optional audio directory: %s", _GP(usetup).install_audio_dir.GetCStr());
	if (!_GP(usetup).install_voice_dir.IsEmpty())
		Debug::Printf(kDbgMsg_Info, "Optional voice-over directory: %s", _GP(usetup).install_voice_dir.GetCStr());
	if (!_GP(usetup).user_data_dir.IsEmpty())
		Debug::Printf(kDbgMsg_Info, "User data directory: %s", _GP(usetup).user_data_dir.GetCStr());
	if (!_GP(usetup).shared_data_dir.IsEmpty())
		Debug::Printf(kDbgMsg_Info, "Shared data directory: %s", _GP(usetup).shared_data_dir.GetCStr());

	_GP(ResPaths).DataDir = _GP(usetup).data_files_dir;
	_GP(ResPaths).GamePak.Path = _GP(usetup).main_data_filepath;
	_GP(ResPaths).GamePak.Name = Shared::Path::get_filename(_GP(usetup).main_data_filepath);

	set_install_dir(_GP(usetup).install_dir, _GP(usetup).install_audio_dir, _GP(usetup).install_voice_dir);
	if (!_GP(usetup).install_dir.IsEmpty()) {
		// running in debugger: don't redirect to the game exe folder (_Debug)
		// TODO: find out why we need to do this (and do we?)
		_GP(ResPaths).DataDir = ".";
	}

	// if end-user specified custom save path, use it
	bool res = false;
	if (!_GP(usetup).user_data_dir.IsEmpty()) {
		res = SetCustomSaveParent(_GP(usetup).user_data_dir);
		if (!res) {
			Debug::Printf(kDbgMsg_Warn, "WARNING: custom user save path failed, using default system paths");
			res = false;
		}
	}
	// if there is no custom path, or if custom path failed, use default system path
	if (!res) {
		char newDirBuffer[MAX_PATH];
		sprintf(newDirBuffer, "%s/%s", UserSavedgamesRootToken, _GP(game).saveGameFolderName);
		SetSaveGameDirectoryPath(newDirBuffer);
	}
}

#if AGS_PLATFORM_OS_ANDROID
extern char android_base_directory[256];
#endif // AGS_PLATFORM_OS_ANDROID

int check_write_access() {
#if AGS_PLATFORM_SCUMMVM
	return true;
#else

	if (_G(platform)->GetDiskFreeSpaceMB() < 2)
		return 0;

	_G(our_eip) = -1895;

	// The Save Game Dir is the only place that we should write to
	String svg_dir = get_save_game_directory();
	String tempPath = String::FromFormat("%s""tmptest.tmp", svg_dir.GetCStr());
	Stream *temp_s = Shared::File::CreateFile(tempPath);
	if (!temp_s)
		// TODO: move this somewhere else (Android platform driver init?)
#if AGS_PLATFORM_OS_ANDROID
	{
//		put_backslash(android_base_directory);
		tempPath.Format("%s""tmptest.tmp", android_base_directory);
		temp_s = Shared::File::CreateFile(tempPath);
		if (temp_s == NULL) return 0;
		else SetCustomSaveParent(android_base_directory);
	}
#else
		return 0;
#endif // AGS_PLATFORM_OS_ANDROID

	_G(our_eip) = -1896;

	temp_s->Write("just to test the drive free space", 30);
	delete temp_s;

	_G(our_eip) = -1897;

	if (::remove(tempPath))
		return 0;

	return 1;
#endif
}

int engine_check_disk_space() {
	Debug::Printf(kDbgMsg_Info, "Checking for disk space");

	if (check_write_access() == 0) {
		_G(platform)->DisplayAlert("Unable to write in the savegame directory.\n%s", _G(platform)->GetDiskWriteAccessTroubleshootingText());
		_G(proper_exit) = 1;
		return EXIT_ERROR;
	}

	return 0;
}

int engine_check_font_was_loaded() {
	if (!font_first_renderer_loaded()) {
		_G(platform)->DisplayAlert("No game fonts found. At least one font is required to run the game.");
		_G(proper_exit) = 1;
		return EXIT_ERROR;
	}

	return 0;
}

// Do the preload graphic if available
void show_preload() {
	color temppal[256];
	Bitmap *splashsc = BitmapHelper::CreateRawBitmapOwner(load_pcx("preload.pcx", temppal));
	if (splashsc != nullptr) {
		Debug::Printf("Displaying preload image");
		if (splashsc->GetColorDepth() == 8)
			set_palette_range(temppal, 0, 255, 0);
		if (_G(gfxDriver)->UsesMemoryBackBuffer())
			_G(gfxDriver)->GetMemoryBackBuffer()->Clear();

		const Rect &view = _GP(play).GetMainViewport();
		Bitmap *tsc = BitmapHelper::CreateBitmapCopy(splashsc, _GP(game).GetColorDepth());
		if (!_G(gfxDriver)->HasAcceleratedTransform() && view.GetSize() != tsc->GetSize()) {
			Bitmap *stretched = new Bitmap(view.GetWidth(), view.GetHeight(), tsc->GetColorDepth());
			stretched->StretchBlt(tsc, RectWH(0, 0, view.GetWidth(), view.GetHeight()));
			delete tsc;
			tsc = stretched;
		}
		IDriverDependantBitmap *ddb = _G(gfxDriver)->CreateDDBFromBitmap(tsc, false, true);
		ddb->SetStretch(view.GetWidth(), view.GetHeight());
		_G(gfxDriver)->ClearDrawLists();
		_G(gfxDriver)->DrawSprite(0, 0, ddb);
		render_to_screen();
		_G(gfxDriver)->DestroyDDB(ddb);
		delete splashsc;
		delete tsc;
		_G(platform)->Delay(500);
	}
}

int engine_init_sprites() {
	Debug::Printf(kDbgMsg_Info, "Initialize sprites");

	HError err = _GP(spriteset).InitFile(SpriteCache::DefaultSpriteFileName, SpriteCache::DefaultSpriteIndexName);
	if (!err) {
		_G(platform)->FinishedUsingGraphicsMode();
		allegro_exit();
		_G(proper_exit) = 1;
		_G(platform)->DisplayAlert("Could not load sprite set file %s\n%s",
		                       SpriteCache::DefaultSpriteFileName,
		                       err->FullMessage().GetCStr());
		return EXIT_ERROR;
	}

	return 0;
}

void engine_init_game_settings() {
	_G(our_eip) = -7;
	Debug::Printf("Initialize game settings");

	int ee;

	for (ee = 0; ee < MAX_ROOM_OBJECTS + _GP(game).numcharacters; ee++)
		_G(actsps)[ee] = nullptr;

	for (ee = 0; ee < 256; ee++) {
		if (_GP(game).paluses[ee] != PAL_BACKGROUND)
			palette[ee] = _GP(game).defpal[ee];
	}

	for (ee = 0; ee < _GP(game).numcursors; ee++) {
		// The cursor graphics are assigned to _G(mousecurs)[] and so cannot
		// be removed from memory
		if (_GP(game).mcurs[ee].pic >= 0)
			_GP(spriteset).Precache(_GP(game).mcurs[ee].pic);

		// just in case they typed an invalid view number in the editor
		if (_GP(game).mcurs[ee].view >= _GP(game).numviews)
			_GP(game).mcurs[ee].view = -1;

		if (_GP(game).mcurs[ee].view >= 0)
			precache_view(_GP(game).mcurs[ee].view);
	}
	// may as well preload the character gfx
	if (_G(playerchar)->view >= 0)
		precache_view(_G(playerchar)->view);

	for (ee = 0; ee < MAX_ROOM_OBJECTS; ee++)
		_G(objcache)[ee].image = nullptr;

	/*  dummygui.guiId = -1;
	dummyguicontrol.guin = -1;
	dummyguicontrol.objn = -1;*/

	_G(our_eip) = -6;
	//  _GP(game).chars[0].talkview=4;
	//init_language_text(_GP(game).langcodes[0]);

	for (ee = 0; ee < MAX_ROOM_OBJECTS; ee++) {
		_G(scrObj)[ee].id = ee;
		// 64 bit: Using the id instead
		// _G(scrObj)[ee].obj = NULL;
	}

	for (ee = 0; ee < _GP(game).numcharacters; ee++) {
		memset(&_GP(game).chars[ee].inv[0], 0, MAX_INV * sizeof(short));
		_GP(game).chars[ee].activeinv = -1;
		_GP(game).chars[ee].following = -1;
		_GP(game).chars[ee].followinfo = 97 | (10 << 8);
		_GP(game).chars[ee].idletime = 20; // can be overridden later with SetIdle or summink
		_GP(game).chars[ee].idleleft = _GP(game).chars[ee].idletime;
		_GP(game).chars[ee].transparency = 0;
		_GP(game).chars[ee].baseline = -1;
		_GP(game).chars[ee].walkwaitcounter = 0;
		_GP(game).chars[ee].z = 0;
		_G(charextra)[ee].xwas = INVALID_X;
		_G(charextra)[ee].zoom = 100;
		if (_GP(game).chars[ee].view >= 0) {
			// set initial loop to 0
			_GP(game).chars[ee].loop = 0;
			// or to 1 if they don't have up/down frames
			if (_G(views)[_GP(game).chars[ee].view].loops[0].numFrames < 1)
				_GP(game).chars[ee].loop = 1;
		}
		_G(charextra)[ee].process_idle_this_time = 0;
		_G(charextra)[ee].invorder_count = 0;
		_G(charextra)[ee].slow_move_counter = 0;
		_G(charextra)[ee].animwait = 0;
	}
	// multiply up gui positions
	_G(guibg) = (Bitmap **)malloc(sizeof(Bitmap *) * _GP(game).numgui);
	_G(guibgbmp) = (IDriverDependantBitmap **)malloc(sizeof(IDriverDependantBitmap *) * _GP(game).numgui);
	for (ee = 0; ee < _GP(game).numgui; ee++) {
		_G(guibg)[ee] = nullptr;
		_G(guibgbmp)[ee] = nullptr;
	}

	_G(our_eip) = -5;
	for (ee = 0; ee < _GP(game).numinvitems; ee++) {
		if (_GP(game).invinfo[ee].flags & IFLG_STARTWITH) _G(playerchar)->inv[ee] = 1;
		else _G(playerchar)->inv[ee] = 0;
	}
	_GP(play).score = 0;
	_GP(play).sierra_inv_color = 7;
	// copy the value set by the editor
	if (_GP(game).options[OPT_GLOBALTALKANIMSPD] >= 0) {
		_GP(play).talkanim_speed = _GP(game).options[OPT_GLOBALTALKANIMSPD];
		_GP(game).options[OPT_GLOBALTALKANIMSPD] = 1;
	} else {
		_GP(play).talkanim_speed = -_GP(game).options[OPT_GLOBALTALKANIMSPD] - 1;
		_GP(game).options[OPT_GLOBALTALKANIMSPD] = 0;
	}
	_GP(play).inv_item_wid = 40;
	_GP(play).inv_item_hit = 22;
	_GP(play).messagetime = -1;
	_GP(play).disabled_user_interface = 0;
	_GP(play).gscript_timer = -1;
	_GP(play).debug_mode = _GP(game).options[OPT_DEBUGMODE];
	_GP(play).inv_top = 0;
	_GP(play).inv_numdisp = 0;
	_GP(play).obsolete_inv_numorder = 0;
	_GP(play).text_speed = 15;
	_GP(play).text_min_display_time_ms = 1000;
	_GP(play).ignore_user_input_after_text_timeout_ms = 500;
	_GP(play).ClearIgnoreInput();
	_GP(play).lipsync_speed = 15;
	_GP(play).close_mouth_speech_time = 10;
	_GP(play).disable_antialiasing = 0;
	_GP(play).rtint_enabled = false;
	_GP(play).rtint_level = 0;
	_GP(play).rtint_light = 0;
	_GP(play).text_speed_modifier = 0;
	_GP(play).text_align = kHAlignLeft;
	// Make the default alignment to the right with right-to-left text
	if (_GP(game).options[OPT_RIGHTLEFTWRITE])
		_GP(play).text_align = kHAlignRight;

	_GP(play).speech_bubble_width = get_fixed_pixel_size(100);
	_GP(play).bg_frame = 0;
	_GP(play).bg_frame_locked = 0;
	_GP(play).bg_anim_delay = 0;
	_GP(play).anim_background_speed = 0;
	_GP(play).silent_midi = 0;
	_GP(play).current_music_repeating = 0;
	_GP(play).skip_until_char_stops = -1;
	_GP(play).get_loc_name_last_time = -1;
	_GP(play).get_loc_name_save_cursor = -1;
	_GP(play).restore_cursor_mode_to = -1;
	_GP(play).restore_cursor_image_to = -1;
	_GP(play).ground_level_areas_disabled = 0;
	_GP(play).next_screen_transition = -1;
	_GP(play).temporarily_turned_off_character = -1;
	_GP(play).inv_backwards_compatibility = 0;
	_GP(play).gamma_adjustment = 100;
	_GP(play).do_once_tokens.resize(0);
	_GP(play).music_queue_size = 0;
	_GP(play).shakesc_length = 0;
	_GP(play).wait_counter = 0;
	_GP(play).key_skip_wait = SKIP_NONE;
	_GP(play).cur_music_number = -1;
	_GP(play).music_repeat = 1;
	_GP(play).music_master_volume = 100 + LegacyMusicMasterVolumeAdjustment;
	_GP(play).digital_master_volume = 100;
	_GP(play).screen_flipped = 0;
	_GP(play).cant_skip_speech = user_to_internal_skip_speech((SkipSpeechStyle)_GP(game).options[OPT_NOSKIPTEXT]);
	_GP(play).sound_volume = 255;
	_GP(play).speech_volume = 255;
	_GP(play).normal_font = 0;
	_GP(play).speech_font = 1;
	_GP(play).speech_text_shadow = 16;
	_GP(play).screen_tint = -1;
	_GP(play).bad_parsed_word[0] = 0;
	_GP(play).swap_portrait_side = 0;
	_GP(play).swap_portrait_lastchar = -1;
	_GP(play).swap_portrait_lastlastchar = -1;
	_GP(play).in_conversation = 0;
	_GP(play).skip_display = 3;
	_GP(play).no_multiloop_repeat = 0;
	_GP(play).in_cutscene = 0;
	_GP(play).fast_forward = 0;
	_GP(play).totalscore = _GP(game).totalscore;
	_GP(play).roomscript_finished = 0;
	_GP(play).no_textbg_when_voice = 0;
	_GP(play).max_dialogoption_width = get_fixed_pixel_size(180);
	_GP(play).no_hicolor_fadein = 0;
	_GP(play).bgspeech_game_speed = 0;
	_GP(play).bgspeech_stay_on_display = 0;
	_GP(play).unfactor_speech_from_textlength = 0;
	_GP(play).mp3_loop_before_end = 70;
	_GP(play).speech_music_drop = 60;
	_GP(play).room_changes = 0;
	_GP(play).check_interaction_only = 0;
	_GP(play).replay_hotkey_unused = -1;  // StartRecording: not supported.
	_GP(play).dialog_options_x = 0;
	_GP(play).dialog_options_y = 0;
	_GP(play).min_dialogoption_width = 0;
	_GP(play).disable_dialog_parser = 0;
	_GP(play).ambient_sounds_persist = 0;
	_GP(play).screen_is_faded_out = 0;
	_GP(play).player_on_region = 0;
	_GP(play).top_bar_backcolor = 8;
	_GP(play).top_bar_textcolor = 16;
	_GP(play).top_bar_bordercolor = 8;
	_GP(play).top_bar_borderwidth = 1;
	_GP(play).top_bar_ypos = 25;
	_GP(play).top_bar_font = -1;
	_GP(play).screenshot_width = 160;
	_GP(play).screenshot_height = 100;
	_GP(play).speech_text_align = kHAlignCenter;
	_GP(play).auto_use_walkto_points = 1;
	_GP(play).inventory_greys_out = 0;
	_GP(play).skip_speech_specific_key = 0;
	_GP(play).abort_key = 324;  // Alt+X
	_GP(play).fade_to_red = 0;
	_GP(play).fade_to_green = 0;
	_GP(play).fade_to_blue = 0;
	_GP(play).show_single_dialog_option = 0;
	_GP(play).keep_screen_during_instant_transition = 0;
	_GP(play).read_dialog_option_colour = -1;
	_GP(play).speech_portrait_placement = 0;
	_GP(play).speech_portrait_x = 0;
	_GP(play).speech_portrait_y = 0;
	_GP(play).speech_display_post_time_ms = 0;
	_GP(play).dialog_options_highlight_color = DIALOG_OPTIONS_HIGHLIGHT_COLOR_DEFAULT;
	_GP(play).speech_has_voice = false;
	_GP(play).speech_voice_blocking = false;
	_GP(play).speech_in_post_state = false;
	_GP(play).narrator_speech = _GP(game).playercharacter;
	_GP(play).crossfading_out_channel = 0;
	_GP(play).speech_textwindow_gui = _GP(game).options[OPT_TWCUSTOM];
	if (_GP(play).speech_textwindow_gui == 0)
		_GP(play).speech_textwindow_gui = -1;
	strcpy(_GP(play).game_name, _GP(game).gamename);
	_GP(play).lastParserEntry[0] = 0;
	_GP(play).follow_change_room_timer = 150;
	for (ee = 0; ee < MAX_ROOM_BGFRAMES; ee++)
		_GP(play).raw_modified[ee] = 0;
	_GP(play).game_speed_modifier = 0;
	if (_G(debug_flags) & DBG_DEBUGMODE)
		_GP(play).debug_mode = 1;
	gui_disabled_style = convert_gui_disabled_style(_GP(game).options[OPT_DISABLEOFF]);
	_GP(play).shake_screen_yoff = 0;

	memset(&_GP(play).walkable_areas_on[0], 1, MAX_WALK_AREAS + 1);
	memset(&_GP(play).script_timers[0], 0, MAX_TIMERS * sizeof(int32_t));
	memset(&_GP(play).default_audio_type_volumes[0], -1, MAX_AUDIO_TYPES * sizeof(int32_t));

	// reset graphical script vars (they're still used by some games)
	for (ee = 0; ee < MAXGLOBALVARS; ee++)
		_GP(play).globalvars[ee] = 0;

	for (ee = 0; ee < MAXGLOBALSTRINGS; ee++)
		_GP(play).globalstrings[ee][0] = 0;

	if (!_GP(usetup).translation.IsEmpty())
		init_translation(_GP(usetup).translation, "", true);

	update_invorder();
	_G(displayed_room) = -10;

	_G(currentcursor) = 0;
	_G(our_eip) = -4;
	_G(mousey) = 100; // stop icon bar popping up

	// We use same variable to read config and be used at runtime for now,
	// so update it here with regards to game design option
	_GP(usetup).RenderAtScreenRes =
	    (_GP(game).options[OPT_RENDERATSCREENRES] == kRenderAtScreenRes_UserDefined && _GP(usetup).RenderAtScreenRes) ||
	    _GP(game).options[OPT_RENDERATSCREENRES] == kRenderAtScreenRes_Enabled;
}

void engine_setup_scsystem_auxiliary() {
	// ScriptSystem::aci_version is only 10 chars long
	strncpy(_GP(scsystem).aci_version, _G(EngineVersion).LongString, 10);
	if (_GP(usetup).override_script_os >= 0) {
		_GP(scsystem).os = _GP(usetup).override_script_os;
	} else {
		_GP(scsystem).os = _G(platform)->GetSystemOSID();
	}
}

void engine_prepare_to_start_game() {
	Debug::Printf("Prepare to start game");

	engine_setup_scsystem_auxiliary();
#if AGS_PLATFORM_SCUMMVM
	clear_sound_cache();
#endif

#if AGS_PLATFORM_OS_ANDROID
	if (psp_load_latest_savegame)
		selectLatestSavegame();
#endif
}

// TODO: move to test unit
Bitmap *test_allegro_bitmap;
IDriverDependantBitmap *test_allegro_ddb;
void allegro_bitmap_test_init() {
	test_allegro_bitmap = nullptr;
	// Switched the test off for now
	//test_allegro_bitmap = AllegroBitmap::CreateBitmap(320,200,32);
}

// Only allow searching around for game data on desktop systems;
// otherwise use explicit argument either from program wrapper, command-line
// or read from default config.
#if AGS_PLATFORM_OS_WINDOWS || AGS_PLATFORM_OS_LINUX || AGS_PLATFORM_OS_MACOS
#define AGS_SEARCH_FOR_GAME_ON_LAUNCH
#endif

// Define location of the game data either using direct settings or searching
// for the available resource packs in common locations
HError define_gamedata_location_checkall(const String &exe_path) {
	// First try if they provided a startup option
	if (!_G(cmdGameDataPath).IsEmpty()) {
		// If not a valid path - bail out
		if (!Path::IsFileOrDir(_G(cmdGameDataPath)))
			return new Error(String::FromFormat("Defined game location is not a valid path.\nPath: '%s'", _G(cmdGameDataPath).GetCStr()));
		// Switch working dir to this path to be able to look for config and other assets there
		Directory::SetCurrentDirectory(Path::GetDirectoryPath(_G(cmdGameDataPath)));
		// If it's a file, then keep it and proceed
		if (Path::IsFile(_G(cmdGameDataPath))) {
			_GP(usetup).main_data_filepath = _G(cmdGameDataPath);
			return HError::None();
		}
	}
	// Read game data location from the default config file.
	// This is an optional setting that may instruct which game file to use as a primary asset library.
	ConfigTree cfg;
	String def_cfg_file = find_default_cfg_file(exe_path);
	IniUtil::Read(def_cfg_file, cfg);
	read_game_data_location(cfg);
	if (!_GP(usetup).main_data_filename.IsEmpty())
		return HError::None();

#if defined (AGS_SEARCH_FOR_GAME_ON_LAUNCH)
	// No direct filepath provided, search in common locations.
	String path, search_path;
	if (search_for_game_data_file(path, search_path)) {
		_GP(usetup).main_data_filepath = path;
		return HError::None();
	}
	return new Error("Engine was not able to find any compatible game data.",
	                 search_path.IsEmpty() ? String() : String::FromFormat("Searched in: %s", search_path.GetCStr()));
#else
	return new Error("The game location was not defined by startup settings.");
#endif
}

// Define location of the game data
bool define_gamedata_location(const String &exe_path) {
	HError err = define_gamedata_location_checkall(exe_path);
	if (!err) {
		_G(platform)->DisplayAlert("ERROR: Unable to determine game data.\n%s", err->FullMessage().GetCStr());
		main_print_help();
		return false;
	}

	// On success: set all the necessary path and filename settings,
	// derive missing ones from available.
	if (_GP(usetup).main_data_filename.IsEmpty()) {
		_GP(usetup).main_data_filename = Shared::Path::get_filename(_GP(usetup).main_data_filepath);
	} else if (_GP(usetup).main_data_filepath.IsEmpty()) {
		if (_GP(usetup).data_files_dir.IsEmpty() || !is_relative_filename(_GP(usetup).main_data_filename))
			_GP(usetup).main_data_filepath = _GP(usetup).main_data_filename;
		else
			_GP(usetup).main_data_filepath = Path::ConcatPaths(_GP(usetup).data_files_dir, _GP(usetup).main_data_filename);
	}
	if (_GP(usetup).data_files_dir.IsEmpty())
		_GP(usetup).data_files_dir = Path::GetDirectoryPath(_GP(usetup).main_data_filepath);
	return true;
}

// Find and preload main game data
bool engine_init_gamedata(const String &exe_path) {
	Debug::Printf(kDbgMsg_Info, "Initializing game data");
	if (!define_gamedata_location(exe_path))
		return false;
	if (!engine_try_init_gamedata(_GP(usetup).main_data_filepath))
		return false;

	// Pre-load game name and savegame folder names from data file
	// TODO: research if that is possible to avoid this step and just
	// read the full head game data at this point. This might require
	// further changes of the order of initialization.
	HError err = preload_game_data();
	if (!err) {
		display_game_file_error(err);
		return false;
	}
	return true;
}

void engine_read_config(const String &exe_path, ConfigTree &cfg) {
	// Read default configuration file
	String def_cfg_file = find_default_cfg_file(exe_path);
	IniUtil::Read(def_cfg_file, cfg);

	// Disabled on Windows because people were afraid that this config could be mistakenly
	// created by some installer and screw up their games. Until any kind of solution is found.
	String user_global_cfg_file;
#if !AGS_PLATFORM_SCUMMVM
#if !AGS_PLATFORM_OS_WINDOWS
	// Read user global configuration file
	user_global_cfg_file = find_user_global_cfg_file();
	if (Path::ComparePaths(user_global_cfg_file, def_cfg_file) != 0)
		IniUtil::Read(user_global_cfg_file, cfg);
#endif

	// Read user configuration file
	String user_cfg_file = find_user_cfg_file();
	if (Path::ComparePaths(user_cfg_file, def_cfg_file) != 0 &&
	        Path::ComparePaths(user_cfg_file, user_global_cfg_file) != 0)
		IniUtil::Read(user_cfg_file, cfg);
#endif

	// Apply overriding options from mobile port settings
	// TODO: normally, those should be instead stored in the same config file in a uniform way
	// NOTE: the variable is historically called "ignore" but we use it in "override" meaning here
	if (_G(psp_ignore_acsetup_cfg_file))
		override_config_ext(cfg);
}

// Gathers settings from all available sources into single ConfigTree
void engine_prepare_config(ConfigTree &cfg, const String &exe_path, const ConfigTree &startup_opts) {
	Debug::Printf(kDbgMsg_Info, "Setting up game configuration");
	// Read configuration files
	engine_read_config(exe_path, cfg);
	// Merge startup options in
	for (const auto &sectn : startup_opts)
		for (const auto &opt : sectn._value)
			cfg[sectn._key][opt._key] = opt._value;

	// Add "meta" config settings to let setup application(s)
	// display correct properties to the user
	INIwriteint(cfg, "misc", "defaultres", _GP(game).GetResolutionType());
	INIwriteint(cfg, "misc", "letterbox", _GP(game).options[OPT_LETTERBOX]);
	INIwriteint(cfg, "misc", "game_width", _GP(game).GetDefaultRes().Width);
	INIwriteint(cfg, "misc", "game_height", _GP(game).GetDefaultRes().Height);
	INIwriteint(cfg, "misc", "gamecolordepth", _GP(game).color_depth * 8);
	if (_GP(game).options[OPT_RENDERATSCREENRES] != kRenderAtScreenRes_UserDefined) {
		// force enabled/disabled
		INIwriteint(cfg, "graphics", "render_at_screenres", _GP(game).options[OPT_RENDERATSCREENRES] == kRenderAtScreenRes_Enabled);
		INIwriteint(cfg, "disabled", "render_at_screenres", 1);
	}
}

// Applies configuration to the running game
void engine_set_config(const ConfigTree cfg) {
	config_defaults();
	apply_config(cfg);
	post_config();
}

//
// --tell command support: printing engine/game info by request
//

static bool print_info_needs_game(const std::set<String> &keys) {
	return keys.count("all") > 0 || keys.count("config") > 0 || keys.count("configpath") > 0 ||
	       keys.count("data") > 0;
}

static void engine_print_info(const std::set<String> &keys, const String &exe_path, ConfigTree *user_cfg) {
	const bool all = keys.count("all") > 0;
	ConfigTree data;
	if (all || keys.count("engine") > 0) {
		data["engine"]["name"] = get_engine_name();
		data["engine"]["version"] = get_engine_version();
	}
	if (all || keys.count("graphicdriver") > 0) {
		StringV drv;
		AGS::Engine::GetGfxDriverFactoryNames(drv);
		for (size_t i = 0; i < drv.size(); ++i) {
			data["graphicdriver"][String::FromFormat("%u", i)] = drv[i];
		}
	}
	if (all || keys.count("configpath") > 0) {
		String def_cfg_file = find_default_cfg_file(exe_path);
		String gl_cfg_file = find_user_global_cfg_file();
		String user_cfg_file = find_user_cfg_file();
		data["config-path"]["default"] = def_cfg_file;
		data["config-path"]["global"] = gl_cfg_file;
		data["config-path"]["user"] = user_cfg_file;
	}
	if ((all || keys.count("config") > 0) && user_cfg) {
		for (const auto &sectn : *user_cfg) {
			String cfg_sectn = String::FromFormat("config@%s", sectn._key.GetCStr());
			for (const auto &opt : sectn._value)
				data[cfg_sectn][opt._key] = opt._value;
		}
	}
	if (all || keys.count("data") > 0) {
		data["data"]["gamename"] = _GP(game).gamename;
		data["data"]["version"] = String::FromFormat("%d", _G(loaded_game_file_version));
		data["data"]["compiledwith"] = _GP(game).compiled_with;
		data["data"]["basepack"] = _GP(usetup).main_data_filepath;
	}
	String full;
	IniUtil::WriteToString(full, data);
	_G(platform)->WriteStdOut("%s", full.GetCStr());
}

// TODO: this function is still a big mess, engine/system-related initialization
// is mixed with game-related data adjustments. Divide it in parts, move game
// data init into either InitGameState() or other game method as appropriate.
int initialize_engine(const ConfigTree &startup_opts) {
	if (_G(engine_pre_init_callback)) {
		_G(engine_pre_init_callback)();
	}

	//-----------------------------------------------------
	// Install backend
	if (!engine_init_allegro())
		return EXIT_ERROR;

	//-----------------------------------------------------
	// Locate game data and assemble game config
	const String exe_path = _G(global_argv)[0];
	if (_G(justTellInfo) && !print_info_needs_game(_G(tellInfoKeys))) {
		engine_print_info(_G(tellInfoKeys), exe_path, nullptr);
		return EXIT_NORMAL;
	}

	if (!engine_init_gamedata(exe_path))
		return EXIT_ERROR;
	ConfigTree cfg;
	engine_prepare_config(cfg, exe_path, startup_opts);
	if (_G(justTellInfo)) {
		engine_print_info(_G(tellInfoKeys), exe_path, &cfg);
		return EXIT_NORMAL;
	}
	// Test if need to run built-in setup program (where available)
	if (_G(justRunSetup)) {
		int res;
		if (!engine_run_setup(exe_path, cfg, res))
			return res;
	}
	// Set up game options from user config
	engine_set_config(cfg);
	engine_setup_allegro();
	engine_force_window();

	_G(our_eip) = -190;

	//-----------------------------------------------------
	// Init data paths and other directories, locate general data files
	engine_init_directories();

	_G(our_eip) = -191;

	engine_locate_speech_pak();

	_G(our_eip) = -192;

	engine_locate_audio_pak();

	_G(our_eip) = -193;

	//-----------------------------------------------------
	// Begin setting up systems
	engine_setup_window();

	_G(our_eip) = -194;

	engine_init_fonts();

	_G(our_eip) = -195;

	engine_init_keyboard();

	_G(our_eip) = -196;

	engine_init_mouse();

	_G(our_eip) = -197;

	engine_init_timer();

	_G(our_eip) = -198;

	engine_init_audio();

	_G(our_eip) = -199;

	engine_init_debug();

	_G(our_eip) = -10;

	engine_init_rand();

	engine_init_pathfinder();

	set_game_speed(40);

	_G(our_eip) = -20;
	_G(our_eip) = -19;

	int res = engine_load_game_data();
	if (res != 0)
		return res;

	res = engine_check_register_game();
	if (res != 0)
		return res;

	engine_init_title();

	_G(our_eip) = -189;

	res = engine_check_disk_space();
	if (res != 0)
		return res;

	// Make sure that at least one font was loaded in the process of loading
	// the game data.
	// TODO: Fold this check into engine_load_game_data()
	res = engine_check_font_was_loaded();
	if (res != 0)
		return res;

	_G(our_eip) = -179;

	engine_init_resolution_settings(_GP(game).GetGameRes());

	// Attempt to initialize graphics mode
	if (!engine_try_set_gfxmode_any(_GP(usetup).Screen))
		return EXIT_ERROR;

	SetMultitasking(0);

	// [ER] 2014-03-13
	// Hide the system cursor via allegro
	show_os_cursor(MOUSE_CURSOR_NONE);

	show_preload();

	res = engine_init_sprites();
	if (res != 0)
		return res;

	engine_init_game_settings();

	engine_prepare_to_start_game();

	allegro_bitmap_test_init();

	initialize_start_and_play_game(_G(override_start_room), _G(loadSaveGameOnStartup));

	return EXIT_NORMAL;
}

bool engine_try_set_gfxmode_any(const ScreenSetup &setup) {
	engine_shutdown_gfxmode();

	const Size init_desktop = get_desktop_size();
	if (!graphics_mode_init_any(_GP(game).GetGameRes(), setup, ColorDepthOption(_GP(game).GetColorDepth())))
		return false;

	engine_post_gfxmode_setup(init_desktop);
	return true;
}

bool engine_try_switch_windowed_gfxmode() {
	if (!_G(gfxDriver) || !_G(gfxDriver)->IsModeSet())
		return false;

	// Keep previous mode in case we need to revert back
	DisplayMode old_dm = _G(gfxDriver)->GetDisplayMode();
	GameFrameSetup old_frame = graphics_mode_get_render_frame();

	// Release engine resources that depend on display mode
	engine_pre_gfxmode_release();

	Size init_desktop = get_desktop_size();
	bool switch_to_windowed = !old_dm.Windowed;
	ActiveDisplaySetting setting = graphics_mode_get_last_setting(switch_to_windowed);
	DisplayMode last_opposite_mode = setting.Dm;
	GameFrameSetup use_frame_setup = setting.FrameSetup;

	// If there are saved parameters for given mode (fullscreen/windowed)
	// then use them, if there are not, get default setup for the new mode.
	bool res;
	if (last_opposite_mode.IsValid()) {
		res = graphics_mode_set_dm(last_opposite_mode);
	} else {
		// we need to clone from initial config, because not every parameter is set by graphics_mode_get_defaults()
		DisplayModeSetup dm_setup = _GP(usetup).Screen.DisplayMode;
		dm_setup.Windowed = !old_dm.Windowed;
		graphics_mode_get_defaults(dm_setup.Windowed, dm_setup.ScreenSize, use_frame_setup);
		res = graphics_mode_set_dm_any(_GP(game).GetGameRes(), dm_setup, old_dm.ColorDepth, use_frame_setup);
	}

	// Apply corresponding frame render method
	if (res)
		res = graphics_mode_set_render_frame(use_frame_setup);

	if (!res) {
		// If failed, try switching back to previous gfx mode
		res = graphics_mode_set_dm(old_dm) &&
		      graphics_mode_set_render_frame(old_frame);
	}

	if (res) {
		// If succeeded (with any case), update engine objects that rely on
		// active display mode.
		if (_G(gfxDriver)->GetDisplayMode().Windowed)
			init_desktop = get_desktop_size();
		engine_post_gfxmode_setup(init_desktop);
	}
	ags_clear_input_buffer();
	return res;
}

void engine_shutdown_gfxmode() {
	if (!_G(gfxDriver))
		return;

	engine_pre_gfxsystem_shutdown();
	graphics_mode_shutdown();
}

const char *get_engine_name() {
	return "Adventure Game Studio run-time engine";
}

const char *get_engine_version() {
	return _G(EngineVersion).LongString.GetCStr();
}

void engine_set_pre_init_callback(t_engine_pre_init_callback callback) {
	_G(engine_pre_init_callback) = callback;
}

} // namespace AGS3
