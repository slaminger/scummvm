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

#include "ags/engine/ac/game.h"
#include "ags/shared/ac/common.h"
#include "ags/shared/ac/view.h"
#include "ags/shared/ac/audiocliptype.h"
#include "ags/engine/ac/audiochannel.h"
#include "ags/engine/ac/character.h"
#include "ags/engine/ac/charactercache.h"
#include "ags/engine/ac/characterextras.h"
#include "ags/shared/ac/dialogtopic.h"
#include "ags/engine/ac/draw.h"
#include "ags/engine/ac/dynamicsprite.h"
#include "ags/engine/ac/event.h"
#include "ags/engine/ac/gamesetup.h"
#include "ags/shared/ac/gamesetupstruct.h"
#include "ags/engine/ac/gamestate.h"
#include "ags/engine/ac/global_audio.h"
#include "ags/engine/ac/global_character.h"
#include "ags/engine/ac/global_display.h"
#include "ags/engine/ac/global_game.h"
#include "ags/engine/ac/global_gui.h"
#include "ags/engine/ac/global_object.h"
#include "ags/engine/ac/global_translation.h"
#include "ags/engine/ac/gui.h"
#include "ags/engine/ac/hotspot.h"
#include "ags/engine/ac/lipsync.h"
#include "ags/engine/ac/mouse.h"
#include "ags/engine/ac/movelist.h"
#include "ags/engine/ac/objectcache.h"
#include "ags/engine/ac/overlay.h"
#include "ags/engine/ac/path_helper.h"
#include "ags/engine/ac/sys_events.h"
#include "ags/engine/ac/region.h"
#include "ags/engine/ac/richgamemedia.h"
#include "ags/engine/ac/room.h"
#include "ags/engine/ac/roomobject.h"
#include "ags/engine/ac/roomstatus.h"
#include "ags/engine/ac/runtime_defines.h"
#include "ags/engine/ac/screenoverlay.h"
#include "ags/shared/ac/spritecache.h"
#include "ags/engine/ac/string.h"
#include "ags/engine/ac/system.h"
#include "ags/engine/ac/timer.h"
#include "ags/engine/ac/translation.h"
#include "ags/engine/ac/dynobj/all_dynamicclasses.h"
#include "ags/engine/ac/dynobj/all_scriptclasses.h"
#include "ags/engine/ac/dynobj/cc_audiochannel.h"
#include "ags/engine/ac/dynobj/cc_audioclip.h"
#include "ags/engine/ac/dynobj/scriptcamera.h"
#include "ags/engine/debugging/debug_log.h"
#include "ags/engine/debugging/debugger.h"
#include "ags/shared/debugging/out.h"
#include "ags/engine/device/mousew32.h"
#include "ags/shared/font/fonts.h"
#include "ags/engine/game/savegame.h"
#include "ags/engine/game/savegame_components.h"
#include "ags/engine/game/savegame_internal.h"
#include "ags/engine/gui/animatingguibutton.h"
#include "ags/shared/gfx/bitmap.h"
#include "ags/engine/gfx/graphicsdriver.h"
#include "ags/engine/gfx/gfxfilter.h"
#include "ags/engine/gui/guidialog.h"
#include "ags/engine/main/engine.h"
#include "ags/engine/main/graphics_mode.h"
#include "ags/engine/main/main.h"
#include "ags/engine/media/audio/audio_system.h"
#include "ags/plugins/agsplugin.h"
#include "ags/plugins/plugin_engine.h"
#include "ags/shared/script/cc_error.h"
#include "ags/engine/script/runtimescriptvalue.h"
#include "ags/engine/script/script.h"
#include "ags/engine/script/script_runtime.h"
#include "ags/shared/util/alignedstream.h"
#include "ags/shared/util/directory.h"
#include "ags/shared/util/filestream.h" // TODO: needed only because plugins expect file handle
#include "ags/shared/util/path.h"
#include "ags/shared/util/string_utils.h"
#include "ags/engine/ac/keycode.h"
#include "ags/shared/debugging/out.h"
#include "ags/engine/script/script_api.h"
#include "ags/engine/script/script_runtime.h"
#include "ags/globals.h"
#include "ags/ags.h"
#include "common/memstream.h"

namespace AGS3 {

using namespace AGS::Shared;
using namespace AGS::Engine;

#if AGS_PLATFORM_OS_IOS || AGS_PLATFORM_OS_ANDROID
extern int _G(psp_gfx_renderer);
#endif

extern color palette[256];

//=============================================================================
// Audio
//=============================================================================

void Game_StopAudio(int audioType) {
	if (((audioType < 0) || ((size_t)audioType >= _GP(game).audioClipTypes.size())) && (audioType != SCR_NO_VALUE))
		quitprintf("!game.StopAudio: invalid audio type %d", audioType);
	int aa;

	for (aa = 0; aa < MAX_SOUND_CHANNELS; aa++) {
		if (audioType == SCR_NO_VALUE) {
			stop_or_fade_out_channel(aa);
		} else {
			ScriptAudioClip *clip = AudioChannel_GetPlayingClip(&_G(scrAudioChannel)[aa]);
			if ((clip != nullptr) && (clip->type == audioType))
				stop_or_fade_out_channel(aa);
		}
	}

	remove_clips_of_type_from_queue(audioType);
}

int Game_IsAudioPlaying(int audioType) {
	if (((audioType < 0) || ((size_t)audioType >= _GP(game).audioClipTypes.size())) && (audioType != SCR_NO_VALUE))
		quitprintf("!game.IsAudioPlaying: invalid audio type %d", audioType);

	if (_GP(play).fast_forward)
		return 0;

	for (int aa = 0; aa < MAX_SOUND_CHANNELS; aa++) {
		ScriptAudioClip *clip = AudioChannel_GetPlayingClip(&_G(scrAudioChannel)[aa]);
		if (clip != nullptr) {
			if ((clip->type == audioType) || (audioType == SCR_NO_VALUE)) {
				return 1;
			}
		}
	}
	return 0;
}

void Game_SetAudioTypeSpeechVolumeDrop(int audioType, int volumeDrop) {
	if ((audioType < 0) || ((size_t)audioType >= _GP(game).audioClipTypes.size()))
		quitprintf("!game.SetAudioTypeVolume: invalid audio type: %d", audioType);

	Debug::Printf("game.SetAudioTypeSpeechVolumeDrop: type: %d, drop: %d", audioType, volumeDrop);
	_GP(game).audioClipTypes[audioType].volume_reduction_while_speech_playing = volumeDrop;
	update_volume_drop_if_voiceover();
}

void Game_SetAudioTypeVolume(int audioType, int volume, int changeType) {
	if ((volume < 0) || (volume > 100))
		quitprintf("!game.SetAudioTypeVolume: volume %d is not between 0..100", volume);
	if ((audioType < 0) || ((size_t)audioType >= _GP(game).audioClipTypes.size()))
		quitprintf("!game.SetAudioTypeVolume: invalid audio type: %d", audioType);

	Debug::Printf("game.SetAudioTypeVolume: type: %d, volume: %d, change: %d", audioType, volume, changeType);
	if ((changeType == VOL_CHANGEEXISTING) ||
	        (changeType == VOL_BOTH)) {
		AudioChannelsLock lock;
		for (int aa = 0; aa < MAX_SOUND_CHANNELS; aa++) {
			ScriptAudioClip *clip = AudioChannel_GetPlayingClip(&_G(scrAudioChannel)[aa]);
			if ((clip != nullptr) && (clip->type == audioType)) {
				auto *ch = lock.GetChannel(aa);
				if (ch)
					ch->set_volume_percent(volume);
			}
		}
	}

	if ((changeType == VOL_SETFUTUREDEFAULT) ||
	        (changeType == VOL_BOTH)) {
		_GP(play).default_audio_type_volumes[audioType] = volume;

		// update queued clip volumes
		update_queued_clips_volume(audioType, volume);
	}

}

int Game_GetMODPattern() {
	if (_G(current_music_type) != MUS_MOD)
		return -1;
	AudioChannelsLock lock;
	auto *music_ch = lock.GetChannelIfPlaying(SCHAN_MUSIC);
	return music_ch ? music_ch->get_pos() : -1;
}

//=============================================================================
// ---
//=============================================================================

int Game_GetDialogCount() {
	return _GP(game).numdialog;
}

void set_debug_mode(bool on) {
	_GP(play).debug_mode = on ? 1 : 0;
	debug_set_console(on);
}

void set_game_speed(int new_fps) {
	_G(frames_per_second) = new_fps;
	if (!isTimerFpsMaxed()) // if in maxed mode, don't update timer for now
		setTimerFps(new_fps);
}

void setup_for_dialog() {
	_G(cbuttfont) = _GP(play).normal_font;
	_G(acdialog_font) = _GP(play).normal_font;
	if (!_GP(play).mouse_cursor_hidden)
		ags_domouse(DOMOUSE_ENABLE);
	_G(oldmouse) = _G(cur_cursor);
	set_mouse_cursor(CURS_ARROW);
}
void restore_after_dialog() {
	set_mouse_cursor(_G(oldmouse));
	if (!_GP(play).mouse_cursor_hidden)
		ags_domouse(DOMOUSE_DISABLE);
	invalidate_screen();
}



String get_save_game_directory() {
	return _G(saveGameDirectory);
}

String get_save_game_suffix() {
	return _G(saveGameSuffix);
}

void set_save_game_suffix(const String &suffix) {
	_G(saveGameSuffix) = suffix;
}

String get_save_game_path(int slotNum) {
#if AGS_PLATFORM_SCUMMVM
	return Common::String::format("%s%s", SAVE_FOLDER_PREFIX,
		::AGS::g_vm->getSaveStateName(slotNum).c_str());
#else
	String filename;
	filename.Format(sgnametemplate, slotNum);
	String path = _G(_G(saveGameDirectory));
	path.Append(filename);
	path.Append(_G(saveGameSuffix));
	return path;
#endif
}

// Convert a path possibly containing path tags into acceptable save path
bool MakeSaveGameDir(const String &newFolder, ResolvedPath &rp) {
	rp = ResolvedPath();
	// don't allow absolute paths
	if (!is_relative_filename(newFolder))
		return false;

	String base_dir;
	String newSaveGameDir = FixSlashAfterToken(newFolder);

	if (newSaveGameDir.CompareLeft(UserSavedgamesRootToken, strlen(UserSavedgamesRootToken)) == 0) {
		if (_G(saveGameParent).IsEmpty()) {
			base_dir = PathOrCurDir(_G(platform)->GetUserSavedgamesDirectory());
			newSaveGameDir.ReplaceMid(0, strlen(UserSavedgamesRootToken), base_dir);
		} else {
			// If there is a custom save parent directory, then replace
			// not only root token, but also first subdirectory
			newSaveGameDir.ClipSection('/', 0, 1);
			if (!newSaveGameDir.IsEmpty())
				newSaveGameDir.PrependChar('/');
			newSaveGameDir.Prepend(_G(saveGameParent));
			base_dir = _G(saveGameParent);
		}
	} else {
		// Convert the path relative to installation folder into path relative to the
		// safe save path with default name
		if (_G(saveGameParent).IsEmpty()) {
			base_dir = PathOrCurDir(_G(platform)->GetUserSavedgamesDirectory());
			newSaveGameDir.Format("%s/%s/%s", base_dir.GetCStr(), _GP(game).saveGameFolderName, newFolder.GetCStr());
		} else {
			base_dir = _G(saveGameParent);
			newSaveGameDir.Format("%s/%s", _G(saveGameParent).GetCStr(), newFolder.GetCStr());
		}
		// For games made in the safe-path-aware versions of AGS, report a warning
		if (_GP(game).options[OPT_SAFEFILEPATHS]) {
			debug_script_warn("Attempt to explicitly set savegame location relative to the game installation directory ('%s') denied;\nPath will be remapped to the user documents directory: '%s'",
			                  newFolder.GetCStr(), newSaveGameDir.GetCStr());
		}
	}
	rp.BaseDir = Path::MakeTrailingSlash(base_dir);
	rp.FullPath = Path::MakeTrailingSlash(newSaveGameDir);
	return true;
}

bool SetCustomSaveParent(const String &path) {
	if (SetSaveGameDirectoryPath(path, true)) {
		_G(saveGameParent) = path;
		return true;
	}
	return false;
}

bool SetSaveGameDirectoryPath(const char *newFolder, bool explicit_path) {
#if AGS_PLATFORM_SCUMMVM
	return false;
#else
	if (!newFolder || newFolder[0] == 0)
		newFolder = ".";
	String newSaveGameDir;
	if (explicit_path) {
		newSaveGameDir = Path::MakeTrailingSlash(newFolder);
		if (!Directory::CreateDirectory(newSaveGameDir))
			return false;
	} else {
		ResolvedPath rp;
		if (!MakeSaveGameDir(newFolder, rp))
			return false;
		if (!Directory::CreateAllDirectories(rp.BaseDir, rp.FullPath)) {
			debug_script_warn("SetSaveGameDirectory: failed to create all subdirectories: %s", rp.FullPath.GetCStr());
			return false;
		}
		newSaveGameDir = rp.FullPath;
	}

	String newFolderTempFile = String::FromFormat("%s""agstmp.tmp", newSaveGameDir.GetCStr());
	if (!Shared::File::TestCreateFile(newFolderTempFile))
		return false;

	// copy the Restart Game file, if applicable
	String restartGamePath = String::FromFormat("%s""agssave.%d%s", _G(_G(saveGameDirectory)).GetCStr(), RESTART_POINT_SAVE_GAME_NUMBER, _G(saveGameSuffix).GetCStr());
	Stream *restartGameFile = Shared::File::OpenFileRead(restartGamePath);
	if (restartGameFile != nullptr) {
		long fileSize = restartGameFile->GetLength();
		char *mbuffer = (char *)malloc(fileSize);
		restartGameFile->Read(mbuffer, fileSize);
		delete restartGameFile;

		restartGamePath.Format("%s""agssave.%d%s", newSaveGameDir.GetCStr(), RESTART_POINT_SAVE_GAME_NUMBER, _G(saveGameSuffix).GetCStr());
		restartGameFile = Shared::File::CreateFile(restartGamePath);
		restartGameFile->Write(mbuffer, fileSize);
		delete restartGameFile;
		free(mbuffer);
	}

	_G(_G(saveGameDirectory)) = newSaveGameDir;
	return true;
#endif
}

int Game_SetSaveGameDirectory(const char *newFolder) {
	return SetSaveGameDirectoryPath(newFolder, false) ? 1 : 0;
}

const char *Game_GetSaveSlotDescription(int slnum) {
	String description;
	if (read_savedgame_description(get_save_game_path(slnum), description)) {
		return CreateNewScriptString(description);
	}
	return nullptr;
}


void restore_game_dialog() {
	can_run_delayed_command();
	if (_GP(thisroom).Options.SaveLoadDisabled) {
		DisplayMessage(983);
		return;
	}
	if (_G(inside_script)) {
		_G(curscript)->queue_action(ePSARestoreGameDialog, 0, "RestoreGameDialog");
		return;
	}
	setup_for_dialog();
	int toload = loadgamedialog();
	restore_after_dialog();
	if (toload >= 0) {
		try_restore_save(toload);
	}
}

void save_game_dialog() {
	if (_GP(thisroom).Options.SaveLoadDisabled == 1) {
		DisplayMessage(983);
		return;
	}
	if (_G(inside_script)) {
		_G(curscript)->queue_action(ePSASaveGameDialog, 0, "SaveGameDialog");
		return;
	}
	setup_for_dialog();
	int toload = savegamedialog();
	restore_after_dialog();
	if (toload >= 0)
		save_game(toload, get_gui_dialog_buffer());
}

void free_do_once_tokens() {
	_GP(play).do_once_tokens.resize(0);
}


// Free all the memory associated with the game
// TODO: call this when exiting the game (currently only called in RunAGSGame)
void unload_game_file() {
	close_translation();

	_GP(play).FreeViewportsAndCameras();

	_GP(characterScriptObjNames).clear();
	free(_G(charextra));
	free(_G(mls));
	free(_G(actsps));
	free(_G(actspsbmp));
	free(_G(actspswb));
	free(_G(actspswbbmp));
	free(_G(actspswbcache));

	if ((_G(gameinst) != nullptr) && (_G(gameinst)->pc != 0)) {
		quit("Error: unload_game called while script still running");
	} else {
		delete _G(gameinstFork);
		delete _G(gameinst);
		_G(gameinstFork) = nullptr;
		_G(gameinst) = nullptr;
	}

	_GP(gamescript).reset();

	if ((_G(dialogScriptsInst) != nullptr) && (_G(dialogScriptsInst)->pc != 0)) {
		quit("Error: unload_game called while dialog script still running");
	} else if (_G(dialogScriptsInst) != nullptr) {
		delete _G(dialogScriptsInst);
		_G(dialogScriptsInst) = nullptr;
	}

	_GP(dialogScriptsScript).reset();

	for (int i = 0; i < _G(numScriptModules); ++i) {
		delete _GP(moduleInstFork)[i];
		delete _GP(moduleInst)[i];
		_GP(scriptModules)[i].reset();
	}
	_GP(moduleInstFork).resize(0);
	_GP(moduleInst).resize(0);
	_GP(scriptModules).resize(0);
	_GP(repExecAlways).moduleHasFunction.resize(0);
	_GP(lateRepExecAlways).moduleHasFunction.resize(0);
	_GP(getDialogOptionsDimensionsFunc).moduleHasFunction.resize(0);
	_GP(renderDialogOptionsFunc).moduleHasFunction.resize(0);
	_GP(getDialogOptionUnderCursorFunc).moduleHasFunction.resize(0);
	_GP(runDialogOptionMouseClickHandlerFunc).moduleHasFunction.resize(0);
	_GP(runDialogOptionKeyPressHandlerFunc).moduleHasFunction.resize(0);
	_GP(runDialogOptionRepExecFunc).moduleHasFunction.resize(0);
	_G(numScriptModules) = 0;

	free(_G(views));
	_G(views) = nullptr;

	free(_G(charcache));
	_G(charcache) = nullptr;

	if (_G(splipsync) != nullptr) {
		for (int i = 0; i < _G(numLipLines); ++i) {
			free(_G(splipsync)[i].endtimeoffs);
			free(_G(splipsync)[i].frame);
		}
		free(_G(splipsync));
		_G(splipsync) = nullptr;
		_G(numLipLines) = 0;
		_G(curLipLine) = -1;
	}

	for (int i = 0; i < _GP(game).numdialog; ++i) {
		if (_G(dialog)[i].optionscripts != nullptr)
			free(_G(dialog)[i].optionscripts);
		_G(dialog)[i].optionscripts = nullptr;
	}
	free(_G(dialog));
	_G(dialog) = nullptr;
	delete[] _G(scrDialog);
	_G(scrDialog) = nullptr;

	for (int i = 0; i < _GP(game).numgui; ++i) {
		free(_G(guibg)[i]);
		_G(guibg)[i] = nullptr;
	}

	_GP(guiScriptObjNames).clear();
	free(_G(guibg));
	_GP(guis).clear();
	free(_G(scrGui));

	pl_stop_plugins();
	ccRemoveAllSymbols();
	ccUnregisterAllObjects();

	free_all_fonts();

	free_do_once_tokens();
	free(_GP(play).gui_draw_order);

	resetRoomStatuses();

	// free game struct last because it contains object counts
	_GP(game).Free();
}

const char *Game_GetGlobalStrings(int index) {
	if ((index < 0) || (index >= MAXGLOBALSTRINGS))
		quit("!game.GlobalStrings: invalid index");

	return CreateNewScriptString(_GP(play).globalstrings[index]);
}


char gamefilenamebuf[200];

// ** GetGameParameter replacement functions

int Game_GetInventoryItemCount() {
	// because of the dummy item 0, this is always one higher than it should be
	return _GP(game).numinvitems - 1;
}

int Game_GetFontCount() {
	return _GP(game).numfonts;
}

int Game_GetMouseCursorCount() {
	return _GP(game).numcursors;
}

int Game_GetCharacterCount() {
	return _GP(game).numcharacters;
}

int Game_GetGUICount() {
	return _GP(game).numgui;
}

int Game_GetViewCount() {
	return _GP(game).numviews;
}

int Game_GetUseNativeCoordinates() {
	return _GP(game).IsDataInNativeCoordinates() ? 1 : 0;
}

int Game_GetSpriteWidth(int spriteNum) {
	if (spriteNum < 0)
		return 0;

	if (!_GP(spriteset).DoesSpriteExist(spriteNum))
		return 0;

	return game_to_data_coord(_GP(game).SpriteInfos[spriteNum].Width);
}

int Game_GetSpriteHeight(int spriteNum) {
	if (spriteNum < 0)
		return 0;

	if (!_GP(spriteset).DoesSpriteExist(spriteNum))
		return 0;

	return game_to_data_coord(_GP(game).SpriteInfos[spriteNum].Height);
}

int Game_GetLoopCountForView(int viewNumber) {
	if ((viewNumber < 1) || (viewNumber > _GP(game).numviews))
		quit("!GetGameParameter: invalid view specified");

	return _G(views)[viewNumber - 1].numLoops;
}

int Game_GetRunNextSettingForLoop(int viewNumber, int loopNumber) {
	if ((viewNumber < 1) || (viewNumber > _GP(game).numviews))
		quit("!GetGameParameter: invalid view specified");
	if ((loopNumber < 0) || (loopNumber >= _G(views)[viewNumber - 1].numLoops))
		quit("!GetGameParameter: invalid loop specified");

	return (_G(views)[viewNumber - 1].loops[loopNumber].RunNextLoop()) ? 1 : 0;
}

int Game_GetFrameCountForLoop(int viewNumber, int loopNumber) {
	if ((viewNumber < 1) || (viewNumber > _GP(game).numviews))
		quit("!GetGameParameter: invalid view specified");
	if ((loopNumber < 0) || (loopNumber >= _G(views)[viewNumber - 1].numLoops))
		quit("!GetGameParameter: invalid loop specified");

	return _G(views)[viewNumber - 1].loops[loopNumber].numFrames;
}

ScriptViewFrame *Game_GetViewFrame(int viewNumber, int loopNumber, int frame) {
	if ((viewNumber < 1) || (viewNumber > _GP(game).numviews))
		quit("!GetGameParameter: invalid view specified");
	if ((loopNumber < 0) || (loopNumber >= _G(views)[viewNumber - 1].numLoops))
		quit("!GetGameParameter: invalid loop specified");
	if ((frame < 0) || (frame >= _G(views)[viewNumber - 1].loops[loopNumber].numFrames))
		quit("!GetGameParameter: invalid frame specified");

	ScriptViewFrame *sdt = new ScriptViewFrame(viewNumber - 1, loopNumber, frame);
	ccRegisterManagedObject(sdt, sdt);
	return sdt;
}

int Game_DoOnceOnly(const char *token) {
	for (int i = 0; i < (int)_GP(play).do_once_tokens.size(); i++) {
		if (_GP(play).do_once_tokens[i] == token) {
			return 0;
		}
	}
	_GP(play).do_once_tokens.push_back(token);
	return 1;
}

int Game_GetTextReadingSpeed() {
	return _GP(play).text_speed;
}

void Game_SetTextReadingSpeed(int newTextSpeed) {
	if (newTextSpeed < 1)
		quitprintf("!_GP(game).TextReadingSpeed: %d is an invalid speed", newTextSpeed);

	_GP(play).text_speed = newTextSpeed;
}

int Game_GetMinimumTextDisplayTimeMs() {
	return _GP(play).text_min_display_time_ms;
}

void Game_SetMinimumTextDisplayTimeMs(int newTextMinTime) {
	_GP(play).text_min_display_time_ms = newTextMinTime;
}

int Game_GetIgnoreUserInputAfterTextTimeoutMs() {
	return _GP(play).ignore_user_input_after_text_timeout_ms;
}

void Game_SetIgnoreUserInputAfterTextTimeoutMs(int newValueMs) {
	_GP(play).ignore_user_input_after_text_timeout_ms = newValueMs;
}

const char *Game_GetFileName() {
	return CreateNewScriptString(_GP(ResPaths).GamePak.Name);
}

const char *Game_GetName() {
	return CreateNewScriptString(_GP(play).game_name);
}

void Game_SetName(const char *newName) {
	strncpy(_GP(play).game_name, newName, 99);
	_GP(play).game_name[99] = 0;
	::AGS::g_vm->set_window_title(_GP(play).game_name);
}

int Game_GetSkippingCutscene() {
	if (_GP(play).fast_forward) {
		return 1;
	}
	return 0;
}

int Game_GetInSkippableCutscene() {
	if (_GP(play).in_cutscene) {
		return 1;
	}
	return 0;
}

int Game_GetColorFromRGB(int red, int grn, int blu) {
	if ((red < 0) || (red > 255) || (grn < 0) || (grn > 255) ||
	        (blu < 0) || (blu > 255))
		quit("!GetColorFromRGB: colour values must be 0-255");

	if (_GP(game).color_depth == 1) {
		return makecol8(red, grn, blu);
	}

	int agscolor = ((blu >> 3) & 0x1f);
	agscolor += ((grn >> 2) & 0x3f) << 5;
	agscolor += ((red >> 3) & 0x1f) << 11;
	return agscolor;
}

const char *Game_InputBox(const char *msg) {
	char buffer[STD_BUFFER_SIZE];
	sc_inputbox(msg, buffer);
	return CreateNewScriptString(buffer);
}

const char *Game_GetLocationName(int x, int y) {
	char buffer[STD_BUFFER_SIZE];
	GetLocationName(x, y, buffer);
	return CreateNewScriptString(buffer);
}

const char *Game_GetGlobalMessages(int index) {
	if ((index < 500) || (index >= MAXGLOBALMES + 500)) {
		return nullptr;
	}
	char buffer[STD_BUFFER_SIZE];
	buffer[0] = 0;
	replace_tokens(get_translation(get_global_message(index)), buffer, STD_BUFFER_SIZE);
	return CreateNewScriptString(buffer);
}

int Game_GetSpeechFont() {
	return _GP(play).speech_font;
}
int Game_GetNormalFont() {
	return _GP(play).normal_font;
}

const char *Game_GetTranslationFilename() {
	char buffer[STD_BUFFER_SIZE];
	GetTranslationName(buffer);
	return CreateNewScriptString(buffer);
}

int Game_ChangeTranslation(const char *newFilename) {
	if ((newFilename == nullptr) || (newFilename[0] == 0)) {
		close_translation();
		strcpy(_G(transFileName), "");
		_GP(usetup).translation = "";
		return 1;
	}

	String oldTransFileName;
	oldTransFileName = _G(transFileName);

	if (init_translation(newFilename, oldTransFileName.LeftSection('.'), false)) {
		_GP(usetup).translation = newFilename;
		return 1;
	} else {
		strcpy(_G(transFileName), oldTransFileName);
		return 0;
	}
}

ScriptAudioClip *Game_GetAudioClip(int index) {
	if (index < 0 || (size_t)index >= _GP(game).audioClips.size())
		return nullptr;
	return &_GP(game).audioClips[index];
}

ScriptCamera *Game_GetCamera() {
	return _GP(play).GetScriptCamera(0);
}

int Game_GetCameraCount() {
	return _GP(play).GetRoomCameraCount();
}

ScriptCamera *Game_GetAnyCamera(int index) {
	return _GP(play).GetScriptCamera(index);
}

void Game_SimulateKeyPress(int key_) {
	int platformKey = GetKeyForKeyPressCb(key_);
	platformKey = PlatformKeyFromAgsKey(platformKey);
	if (platformKey >= 0) {
		simulate_keypress(platformKey);
	}
}

//=============================================================================

// save game functions



void serialize_bitmap(const Shared::Bitmap *thispic, Stream *out) {
	if (thispic != nullptr) {
		out->WriteInt32(thispic->GetWidth());
		out->WriteInt32(thispic->GetHeight());
		out->WriteInt32(thispic->GetColorDepth());
		for (int cc = 0; cc < thispic->GetHeight(); cc++) {
			switch (thispic->GetColorDepth()) {
			case 8:
			// CHECKME: originally, AGS does not use real BPP here, but simply divides color depth by 8;
			// therefore 15-bit bitmaps are saved only partially? is this a bug? or?
			case 15:
				out->WriteArray(&thispic->GetScanLine(cc)[0], thispic->GetWidth(), 1);
				break;
			case 16:
				out->WriteArrayOfInt16((const int16_t *)&thispic->GetScanLine(cc)[0], thispic->GetWidth());
				break;
			case 32:
				out->WriteArrayOfInt32((const int32_t *)&thispic->GetScanLine(cc)[0], thispic->GetWidth());
				break;
			}
		}
	}
}

// On Windows we could just use IIDFromString but this is platform-independant
void convert_guid_from_text_to_binary(const char *guidText, unsigned char *buffer) {
	guidText++; // skip {
	for (int bytesDone = 0; bytesDone < 16; bytesDone++) {
		if (*guidText == '-')
			guidText++;

		char tempString[3];
		tempString[0] = guidText[0];
		tempString[1] = guidText[1];
		tempString[2] = 0;
		int thisByte = 0;
		sscanf(tempString, "%X", (unsigned int *)&thisByte);

		buffer[bytesDone] = thisByte;
		guidText += 2;
	}

	// Swap bytes to give correct GUID order
	unsigned char temp;
	temp = buffer[0];
	buffer[0] = buffer[3];
	buffer[3] = temp;
	temp = buffer[1];
	buffer[1] = buffer[2];
	buffer[2] = temp;
	temp = buffer[4];
	buffer[4] = buffer[5];
	buffer[5] = temp;
	temp = buffer[6];
	buffer[6] = buffer[7];
	buffer[7] = temp;
}

Bitmap *read_serialized_bitmap(Stream *in) {
	Bitmap *thispic;
	int picwid = in->ReadInt32();
	int pichit = in->ReadInt32();
	int piccoldep = in->ReadInt32();
	thispic = BitmapHelper::CreateBitmap(picwid, pichit, piccoldep);
	if (thispic == nullptr)
		return nullptr;
	for (int vv = 0; vv < pichit; vv++) {
		switch (piccoldep) {
		case 8:
		// CHECKME: originally, AGS does not use real BPP here, but simply divides color depth by 8
		case 15:
			in->ReadArray(thispic->GetScanLineForWriting(vv), picwid, 1);
			break;
		case 16:
			in->ReadArrayOfInt16((int16_t *)thispic->GetScanLineForWriting(vv), picwid);
			break;
		case 32:
			in->ReadArrayOfInt32((int32_t *)thispic->GetScanLineForWriting(vv), picwid);
			break;
		}
	}

	return thispic;
}

void skip_serialized_bitmap(Stream *in) {
	int picwid = in->ReadInt32();
	int pichit = in->ReadInt32();
	int piccoldep = in->ReadInt32();
	// CHECKME: originally, AGS does not use real BPP here, but simply divides color depth by 8
	int bpp = piccoldep / 8;
	in->Seek(picwid * pichit * bpp);
}

long write_screen_shot_for_vista(Stream *out, Bitmap *screenshot) {
	// Save the screenshot to a memory stream so we can access the raw data
	Common::MemoryWriteStreamDynamic bitmap(DisposeAfterUse::YES);
	screenshot->SaveToFile(bitmap, palette);

	update_polled_stuff_if_runtime();

	// Write the bitmap to the output stream
	out->Write(bitmap.getData(), bitmap.size());

	return bitmap.size();
}

void WriteGameSetupStructBase_Aligned(Stream *out) {
	AlignedStream align_s(out, Shared::kAligned_Write);
	_GP(game).GameSetupStructBase::WriteToFile(&align_s);
}

#define MAGICNUMBER 0xbeefcafe

void create_savegame_screenshot(Bitmap *&screenShot) {
	// WORKAROUND: AGS originally only creates savegames if the game flags
	// that it supports it. But we want it all the time for ScummVM GMM
	if (/*_GP(game).options[OPT_SAVESCREENSHOT] */true) {
		// Render the view without any UI elements
		int old_flags = _G(debug_flags);
		_G(debug_flags) |= DBG_NOIFACE;
		construct_game_scene(true);

		int usewid = data_to_game_coord(_GP(play).screenshot_width);
		int usehit = data_to_game_coord(_GP(play).screenshot_height);
		const Rect &viewport = _GP(play).GetMainViewport();
		if (usewid > viewport.GetWidth())
			usewid = viewport.GetWidth();
		if (usehit > viewport.GetHeight())
			usehit = viewport.GetHeight();

		if ((_GP(play).screenshot_width < 16) || (_GP(play).screenshot_height < 16))
			quit("!Invalid game.screenshot_width/height, must be from 16x16 to screen res");

		screenShot = CopyScreenIntoBitmap(usewid, usehit);

		// Restore original screen
		_G(debug_flags) = old_flags;
		construct_game_scene(true);
	}
}

void save_game(int slotn, const char *descript) {
	// dont allow save in rep_exec_always, because we dont save
	// the state of blocked scripts
	can_run_delayed_command();

	if (_G(inside_script)) {
		strcpy(_G(curscript)->postScriptSaveSlotDescription[_G(curscript)->queue_action(ePSASaveGame, slotn, "SaveGameSlot")], descript);
		return;
	}

	if (_G(platform)->GetDiskFreeSpaceMB() < 2) {
		Display("ERROR: There is not enough disk space free to save the game. Clear some disk space and try again.");
		return;
	}

	VALIDATE_STRING(descript);
	String nametouse;
	nametouse = get_save_game_path(slotn);

	Bitmap *screenShot = nullptr;

	// Screenshot
	create_savegame_screenshot(screenShot);

	Shared::PStream out = StartSavegame(nametouse, descript, screenShot);
	if (out == nullptr)
		quit("save_game: unable to open savegame file for writing");

	update_polled_stuff_if_runtime();

	// Actual dynamic game data is saved here
	SaveGameState(out);

	if (screenShot != nullptr) {
		int screenShotOffset = out->GetPosition() - sizeof(RICH_GAME_MEDIA_HEADER);
		int screenShotSize = write_screen_shot_for_vista(out.get(), screenShot);

		update_polled_stuff_if_runtime();

		out->Seek(12, kSeekBegin);
		out->WriteInt32(screenShotOffset);
		out->Seek(4);
		out->WriteInt32(screenShotSize);

		delete screenShot;
	}
}

HSaveError restore_game_head_dynamic_values(Stream *in, RestoredData &r_data) {
	r_data.FPS = in->ReadInt32();
	r_data.CursorMode = in->ReadInt32();
	r_data.CursorID = in->ReadInt32();
	SavegameComponents::ReadLegacyCameraState(in, r_data);
	set_loop_counter(in->ReadInt32());
	return HSaveError::None();
}

void restore_game_spriteset(Stream *in) {
	// ensure the sprite set is at least as large as it was
	// when the game was saved
	_GP(spriteset).EnlargeTo(in->ReadInt32() - 1); // they saved top_index + 1
	// get serialized dynamic sprites
	int sprnum = in->ReadInt32();
	while (sprnum) {
		unsigned char spriteflag = in->ReadByte();
		add_dynamic_sprite(sprnum, read_serialized_bitmap(in));
		_GP(game).SpriteInfos[sprnum].Flags = spriteflag;
		sprnum = in->ReadInt32();
	}
}

HSaveError restore_game_scripts(Stream *in, const PreservedParams &pp, RestoredData &r_data) {
	// read the global script data segment
	int gdatasize = in->ReadInt32();
	if (pp.GlScDataSize != gdatasize) {
		return new SavegameError(kSvgErr_GameContentAssertion, "Mismatching size of global script data.");
	}
	r_data.GlobalScript.Len = gdatasize;
	r_data.GlobalScript.Data.reset(new char[gdatasize]);
	in->Read(r_data.GlobalScript.Data.get(), gdatasize);

	if (in->ReadInt32() != _G(numScriptModules)) {
		return new SavegameError(kSvgErr_GameContentAssertion, "Mismatching number of script modules.");
	}
	r_data.ScriptModules.resize(_G(numScriptModules));
	for (int i = 0; i < _G(numScriptModules); ++i) {
		size_t module_size = in->ReadInt32();
		if (pp.ScMdDataSize[i] != (int)module_size) {
			return new SavegameError(kSvgErr_GameContentAssertion, String::FromFormat("Mismatching size of script module data, module %d.", i));
		}
		r_data.ScriptModules[i].Len = module_size;
		r_data.ScriptModules[i].Data.reset(new char[module_size]);
		in->Read(r_data.ScriptModules[i].Data.get(), module_size);
	}
	return HSaveError::None();
}

void ReadRoomStatus_Aligned(RoomStatus *roomstat, Stream *in) {
	AlignedStream align_s(in, Shared::kAligned_Read);
	roomstat->ReadFromFile_v321(&align_s);
}

void restore_game_room_state(Stream *in) {
	int vv;

	_G(displayed_room) = in->ReadInt32();

	// read the room state for all the rooms the player has been in
	RoomStatus *roomstat;
	int beenhere;
	for (vv = 0; vv < MAX_ROOMS; vv++) {
		beenhere = in->ReadByte();
		if (beenhere) {
			roomstat = getRoomStatus(vv);
			roomstat->beenhere = beenhere;

			if (roomstat->beenhere) {
				ReadRoomStatus_Aligned(roomstat, in);
				if (roomstat->tsdatasize > 0) {
					roomstat->tsdata = (char *)malloc(roomstat->tsdatasize + 8); // JJS: Why allocate 8 additional bytes?
					in->Read(&roomstat->tsdata[0], roomstat->tsdatasize);
				}
			}
		}
	}
}

void ReadGameState_Aligned(Stream *in, RestoredData &r_data) {
	AlignedStream align_s(in, Shared::kAligned_Read);
	_GP(play).ReadFromSavegame(&align_s, kGSSvgVersion_OldFormat, r_data);
}

void restore_game_play_ex_data(Stream *in) {
	char rbuffer[200];
	for (size_t i = 0; i < _GP(play).do_once_tokens.size(); ++i) {
		StrUtil::ReadCStr(rbuffer, in, sizeof(rbuffer));
		_GP(play).do_once_tokens[i] = rbuffer;
	}

	in->ReadArrayOfInt32(&_GP(play).gui_draw_order[0], _GP(game).numgui);
}

void restore_game_play(Stream *in, RestoredData &r_data) {
	int screenfadedout_was = _GP(play).screen_is_faded_out;
	int roomchanges_was = _GP(play).room_changes;
	// make sure the pointer is preserved
	int32_t *gui_draw_order_was = _GP(play).gui_draw_order;

	ReadGameState_Aligned(in, r_data);
	r_data.Cameras[0].Flags = r_data.Camera0_Flags;

	_GP(play).screen_is_faded_out = screenfadedout_was;
	_GP(play).room_changes = roomchanges_was;
	_GP(play).gui_draw_order = gui_draw_order_was;

	restore_game_play_ex_data(in);
}

void ReadMoveList_Aligned(Stream *in) {
	AlignedStream align_s(in, Shared::kAligned_Read);
	for (int i = 0; i < _GP(game).numcharacters + MAX_ROOM_OBJECTS + 1; ++i) {
		_G(mls)[i].ReadFromFile_Legacy(&align_s);

		align_s.Reset();
	}
}

void ReadGameSetupStructBase_Aligned(Stream *in) {
	AlignedStream align_s(in, Shared::kAligned_Read);
	_GP(game).GameSetupStructBase::ReadFromFile(&align_s);
}

void ReadCharacterExtras_Aligned(Stream *in) {
	AlignedStream align_s(in, Shared::kAligned_Read);
	for (int i = 0; i < _GP(game).numcharacters; ++i) {
		_G(charextra)[i].ReadFromFile(&align_s);
		align_s.Reset();
	}
}

void restore_game_palette(Stream *in) {
	in->SafeReadArray(&palette[0], PALETTE_COUNT);
}

void restore_game_dialogs(Stream *in) {
	for (int vv = 0; vv < _GP(game).numdialog; vv++)
		in->ReadArrayOfInt32(&_G(dialog)[vv].optionflags[0], MAXTOPICOPTIONS);
}

void restore_game_more_dynamic_values(Stream *in) {
	_G(mouse_on_iface) = in->ReadInt32();
	in->ReadInt32(); // mouse_on_iface_button
	in->ReadInt32(); // mouse_pushed_iface
	_G(ifacepopped) = in->ReadInt32();
	_G(game_paused) = in->ReadInt32();
}

void ReadAnimatedButtons_Aligned(Stream *in) {
	AlignedStream align_s(in, Shared::kAligned_Read);
	for (int i = 0; i < _G(numAnimButs); ++i) {
		_G(animbuts)[i].ReadFromFile(&align_s);
		align_s.Reset();
	}
}

HSaveError restore_game_gui(Stream *in, int numGuisWas) {
	HError err = GUI::ReadGUI(_GP(guis), in, true);
	if (!err)
		return new SavegameError(kSvgErr_GameObjectInitFailed, err);
	_GP(game).numgui = _GP(guis).size();

	if (numGuisWas != _GP(game).numgui) {
		return new SavegameError(kSvgErr_GameContentAssertion, "Mismatching number of GUI.");
	}

	_G(numAnimButs) = in->ReadInt32();
	ReadAnimatedButtons_Aligned(in);
	return HSaveError::None();
}

HSaveError restore_game_audiocliptypes(Stream *in) {
	if (in->ReadInt32() != (int)_GP(game).audioClipTypes.size()) {
		return new SavegameError(kSvgErr_GameContentAssertion, "Mismatching number of Audio Clip Types.");
	}

	for (size_t i = 0; i < _GP(game).audioClipTypes.size(); ++i) {
		_GP(game).audioClipTypes[i].ReadFromFile(in);
	}
	return HSaveError::None();
}

void restore_game_thisroom(Stream *in, RestoredData &r_data) {
	in->ReadArrayOfInt16(r_data.RoomLightLevels, MAX_ROOM_REGIONS);
	in->ReadArrayOfInt32(r_data.RoomTintLevels, MAX_ROOM_REGIONS);
	in->ReadArrayOfInt16(r_data.RoomZoomLevels1, MAX_WALK_AREAS + 1);
	in->ReadArrayOfInt16(r_data.RoomZoomLevels2, MAX_WALK_AREAS + 1);
}

void restore_game_ambientsounds(Stream *in, RestoredData &r_data) {
	for (int i = 0; i < MAX_SOUND_CHANNELS; ++i) {
		_GP(ambient)[i].ReadFromFile(in);
	}

	for (int bb = 1; bb < MAX_SOUND_CHANNELS; bb++) {
		if (_GP(ambient)[bb].channel == 0)
			r_data.DoAmbient[bb] = 0;
		else {
			r_data.DoAmbient[bb] = _GP(ambient)[bb].num;
			_GP(ambient)[bb].channel = 0;
		}
	}
}

void ReadOverlays_Aligned(Stream *in) {
	AlignedStream align_s(in, Shared::kAligned_Read);
	for (auto &over : _GP(screenover)) {
		over.ReadFromFile(&align_s, 0);
		align_s.Reset();
	}
}

void restore_game_overlays(Stream *in) {
	_GP(screenover).resize(in->ReadInt32());
	ReadOverlays_Aligned(in);
	for (auto &over : _GP(screenover)) {
		if (over.hasSerializedBitmap)
			over.pic = read_serialized_bitmap(in);
	}
}

void restore_game_dynamic_surfaces(Stream *in, RestoredData &r_data) {
	// load into a temp array since ccUnserialiseObjects will destroy
	// it otherwise
	r_data.DynamicSurfaces.resize(MAX_DYNAMIC_SURFACES);
	for (int i = 0; i < MAX_DYNAMIC_SURFACES; ++i) {
		if (in->ReadInt8() == 0) {
			r_data.DynamicSurfaces[i] = nullptr;
		} else {
			r_data.DynamicSurfaces[i] = read_serialized_bitmap(in);
		}
	}
}

void restore_game_displayed_room_status(Stream *in, RestoredData &r_data) {
	int bb;
	for (bb = 0; bb < MAX_ROOM_BGFRAMES; bb++)
		r_data.RoomBkgScene[bb].reset();

	if (_G(displayed_room) >= 0) {

		for (bb = 0; bb < MAX_ROOM_BGFRAMES; bb++) {
			r_data.RoomBkgScene[bb] = nullptr;
			if (_GP(play).raw_modified[bb]) {
				r_data.RoomBkgScene[bb].reset(read_serialized_bitmap(in));
			}
		}
		bb = in->ReadInt32();

		if (bb)
			_G(raw_saved_screen) = read_serialized_bitmap(in);

		// get the current troom, in case they save in room 600 or whatever
		ReadRoomStatus_Aligned(&_GP(troom), in);

		if (_GP(troom).tsdatasize > 0) {
			_GP(troom).tsdata = (char *)malloc(_GP(troom).tsdatasize + 5);
			in->Read(&_GP(troom).tsdata[0], _GP(troom).tsdatasize);
		} else
			_GP(troom).tsdata = nullptr;
	}
}

HSaveError restore_game_globalvars(Stream *in) {
	if (in->ReadInt32() != _G(numGlobalVars)) {
		return new SavegameError(kSvgErr_GameContentAssertion, "Restore game error: mismatching number of Global Variables.");
	}

	for (int i = 0; i < _G(numGlobalVars); ++i) {
		_G(globalvars)[i].Read(in);
	}
	return HSaveError::None();
}

HSaveError restore_game_views(Stream *in) {
	if (in->ReadInt32() != _GP(game).numviews) {
		return new SavegameError(kSvgErr_GameContentAssertion, "Mismatching number of Views.");
	}

	for (int bb = 0; bb < _GP(game).numviews; bb++) {
		for (int cc = 0; cc < _G(views)[bb].numLoops; cc++) {
			for (int dd = 0; dd < _G(views)[bb].loops[cc].numFrames; dd++) {
				_G(views)[bb].loops[cc].frames[dd].sound = in->ReadInt32();
				_G(views)[bb].loops[cc].frames[dd].pic = in->ReadInt32();
			}
		}
	}
	return HSaveError::None();
}

HSaveError restore_game_audioclips_and_crossfade(Stream *in, RestoredData &r_data) {
	if (in->ReadInt32() != (int)_GP(game).audioClips.size()) {
		return new SavegameError(kSvgErr_GameContentAssertion, "Mismatching number of Audio Clips.");
	}

	for (int i = 0; i <= MAX_SOUND_CHANNELS; ++i) {
		RestoredData::ChannelInfo &chan_info = r_data.AudioChans[i];
		chan_info.Pos = 0;
		chan_info.ClipID = in->ReadInt32();
		if (chan_info.ClipID >= 0) {
			if (chan_info.ClipID >= (int)_GP(game).audioClips.size()) {
				return new SavegameError(kSvgErr_GameObjectInitFailed, "Invalid audio clip index.");
			}

			chan_info.Pos = in->ReadInt32();
			if (chan_info.Pos < 0)
				chan_info.Pos = 0;
			chan_info.Priority = in->ReadInt32();
			chan_info.Repeat = in->ReadInt32();
			chan_info.Vol = in->ReadInt32();
			chan_info.Pan = in->ReadInt32();
			chan_info.VolAsPercent = in->ReadInt32();
			chan_info.PanAsPercent = in->ReadInt32();
			chan_info.Speed = 1000;
			if (_G(loaded_game_file_version) >= kGameVersion_340_2)
				chan_info.Speed = in->ReadInt32();
		}
	}
	_G(crossFading) = in->ReadInt32();
	_G(crossFadeVolumePerStep) = in->ReadInt32();
	_G(crossFadeStep) = in->ReadInt32();
	_G(crossFadeVolumeAtStart) = in->ReadInt32();
	return HSaveError::None();
}

HSaveError restore_game_data(Stream *in, SavegameVersion svg_version, const PreservedParams &pp, RestoredData &r_data) {
	int vv;

	HSaveError err = restore_game_head_dynamic_values(in, r_data);
	if (!err)
		return err;
	restore_game_spriteset(in);

	update_polled_stuff_if_runtime();

	err = restore_game_scripts(in, pp, r_data);
	if (!err)
		return err;
	restore_game_room_state(in);
	restore_game_play(in, r_data);
	ReadMoveList_Aligned(in);

	// save pointer members before reading
	char *gswas = _GP(game).globalscript;
	ccScript *compsc = _GP(game).compiled_script;
	CharacterInfo *chwas = _GP(game).chars;
	WordsDictionary *olddict = _GP(game).dict;
	char *mesbk[MAXGLOBALMES];
	int numchwas = _GP(game).numcharacters;
	for (vv = 0; vv < MAXGLOBALMES; vv++) mesbk[vv] = _GP(game).messages[vv];
	int numdiwas = _GP(game).numdialog;
	int numinvwas = _GP(game).numinvitems;
	int numviewswas = _GP(game).numviews;
	int numGuisWas = _GP(game).numgui;

	ReadGameSetupStructBase_Aligned(in);

	// Delete unneeded data
	// TODO: reorganize this (may be solved by optimizing safe format too)
	delete [] _GP(game).load_messages;
	_GP(game).load_messages = nullptr;

	if (_GP(game).numdialog != numdiwas) {
		return new SavegameError(kSvgErr_GameContentAssertion, "Mismatching number of Dialogs.");
	}
	if (numchwas != _GP(game).numcharacters) {
		return new SavegameError(kSvgErr_GameContentAssertion, "Mismatching number of Characters.");
	}
	if (numinvwas != _GP(game).numinvitems) {
		return new SavegameError(kSvgErr_GameContentAssertion, "Mismatching number of Inventory Items.");
	}
	if (_GP(game).numviews != numviewswas) {
		return new SavegameError(kSvgErr_GameContentAssertion, "Mismatching number of Views.");
	}

	_GP(game).ReadFromSaveGame_v321(in, gswas, compsc, chwas, olddict, mesbk);

	// Modified custom properties are read separately to keep existing save format
	_GP(play).ReadCustomProperties_v340(in);

	ReadCharacterExtras_Aligned(in);
	restore_game_palette(in);
	restore_game_dialogs(in);
	restore_game_more_dynamic_values(in);
	err = restore_game_gui(in, numGuisWas);
	if (!err)
		return err;
	err = restore_game_audiocliptypes(in);
	if (!err)
		return err;
	restore_game_thisroom(in, r_data);
	restore_game_ambientsounds(in, r_data);
	restore_game_overlays(in);

	update_polled_stuff_if_runtime();

	restore_game_dynamic_surfaces(in, r_data);

	update_polled_stuff_if_runtime();

	restore_game_displayed_room_status(in, r_data);
	err = restore_game_globalvars(in);
	if (!err)
		return err;
	err = restore_game_views(in);
	if (!err)
		return err;

	if (in->ReadInt32() != (int32)(MAGICNUMBER + 1)) {
		return new SavegameError(kSvgErr_InconsistentFormat, "MAGICNUMBER not found before Audio Clips.");
	}

	err = restore_game_audioclips_and_crossfade(in, r_data);
	if (!err)
		return err;

	auto pluginFileHandle = AGSE_RESTOREGAME;
	pl_set_file_handle(pluginFileHandle, in);
	pl_run_plugin_hooks(AGSE_RESTOREGAME, pluginFileHandle);
	pl_clear_file_handle();
	if (in->ReadInt32() != (int32)MAGICNUMBER)
		return new SavegameError(kSvgErr_InconsistentPlugin);

	// save the new room music vol for later use
	r_data.RoomVolume = (RoomVolumeMod)in->ReadInt32();

	if (ccUnserializeAllObjects(in, &_GP(ccUnserializer))) {
		return new SavegameError(kSvgErr_GameObjectInitFailed,
		                         String::FromFormat("Managed pool deserialization failed: %s.", _G(ccErrorString).GetCStr()));
	}

	// preserve legacy music type setting
	_G(current_music_type) = in->ReadInt32();

	return HSaveError::None();
}

int gameHasBeenRestored = 0;
int oldeip;

bool read_savedgame_description(const String &savedgame, String &description) {
	SavegameDescription desc;
	if (OpenSavegame(savedgame, desc, kSvgDesc_UserText)) {
		description = desc.UserText;
		return true;
	}
	return false;
}

bool read_savedgame_screenshot(const String &savedgame, int &want_shot) {
	want_shot = 0;

	SavegameDescription desc;
	HSaveError err = OpenSavegame(savedgame, desc, kSvgDesc_UserImage);
	if (!err)
		return false;

	if (desc.UserImage.get()) {
		int slot = _GP(spriteset).GetFreeIndex();
		if (slot > 0) {
			// add it into the sprite set
			add_dynamic_sprite(slot, ReplaceBitmapWithSupportedFormat(desc.UserImage.release()));
			want_shot = slot;
		}
	}
	return true;
}

HSaveError load_game(int slotNumber, bool &data_overwritten) {
	data_overwritten = false;
	gameHasBeenRestored++;

	oldeip = _G(our_eip);
	_G(our_eip) = 2050;

	HSaveError err;
	SavegameSource src;
	SavegameDescription desc;

	String path = get_save_game_path(slotNumber);
	err = OpenSavegame(path, src, desc, kSvgDesc_EnvInfo);

	// saved in incompatible enviroment
	if (!err)
		return err;
	// CHECKME: is this color depth test still essential? if yes, is there possible workaround?
	else if (desc.ColorDepth != _GP(game).GetColorDepth())
		return new SavegameError(kSvgErr_DifferentColorDepth, String::FromFormat("Running: %d-bit, saved in: %d-bit.", _GP(game).GetColorDepth(), desc.ColorDepth));

	// saved with different game file
	if (Path::ComparePaths(desc.MainDataFilename, _GP(ResPaths).GamePak.Name)) {
		// [IKM] 2012-11-26: this is a workaround, indeed.
		// Try to find wanted game's executable; if it does not exist,
		// continue loading savedgame in current game, and pray for the best
		get_install_dir_path(gamefilenamebuf, desc.MainDataFilename);
		if (Shared::File::TestReadFile(gamefilenamebuf)) {
			RunAGSGame(desc.MainDataFilename, 0, 0);
			_G(load_new_game_restore) = slotNumber;
			return HSaveError::None();
		}
		Shared::Debug::Printf(kDbgMsg_Warn, "WARNING: the saved game '%s' references game file '%s', but it cannot be found in the current directory. Trying to restore in the running game instead.",
		                      path.GetCStr(), desc.MainDataFilename.GetCStr());
	}

	// do the actual restore
	err = RestoreGameState(src.InputStream, src.Version);
	data_overwritten = true;
	if (!err)
		return err;
	src.InputStream.reset();
	_G(our_eip) = oldeip;

	// ensure keyboard buffer is clean
	ags_clear_input_buffer();
	// call "After Restore" event callback
	run_on_event(GE_RESTORE_GAME, RuntimeScriptValue().SetInt32(slotNumber));
	return HSaveError::None();
}

bool try_restore_save(int slot) {
	bool data_overwritten;
	HSaveError err = load_game(slot, data_overwritten);
	if (!err) {
		String error = String::FromFormat("Unable to restore the saved game.\n%s",
		                                  err->FullMessage().GetCStr());
		// currently AGS cannot properly revert to stable state if some of the
		// game data was released or overwritten by the data from save file,
		// this is why we tell engine to shutdown if that happened.
		if (data_overwritten)
			quitprintf(error);
		else
			Display(error);
		return false;
	}
	return true;
}

bool is_in_cutscene() {
	return _GP(play).in_cutscene > 0;
}

CutsceneSkipStyle get_cutscene_skipstyle() {
	return static_cast<CutsceneSkipStyle>(_GP(play).in_cutscene);
}

void start_skipping_cutscene() {
	_GP(play).fast_forward = 1;
	// if a drop-down icon bar is up, remove it as it will pause the game
	if (_G(ifacepopped) >= 0)
		remove_popup_interface(_G(ifacepopped));

	// if a text message is currently displayed, remove it
	if (_G(is_text_overlay) > 0)
		remove_screen_overlay(OVER_TEXTMSG);

}

bool check_skip_cutscene_keypress(int kgn) {

	CutsceneSkipStyle skip = get_cutscene_skipstyle();
	if (skip == eSkipSceneAnyKey || skip == eSkipSceneKeyMouse ||
	        (kgn == 27 && (skip == eSkipSceneEscOnly || skip == eSkipSceneEscOrRMB))) {
		start_skipping_cutscene();
		return true;
	}
	return false;
}

bool check_skip_cutscene_mclick(int mbut) {
	CutsceneSkipStyle skip = get_cutscene_skipstyle();
	if (skip == eSkipSceneMouse || skip == eSkipSceneKeyMouse ||
	        (mbut == RIGHT && skip == eSkipSceneEscOrRMB)) {
		start_skipping_cutscene();
		return true;
	}
	return false;
}

// Helper functions used by StartCutscene/EndCutscene, but also
// by SkipUntilCharacterStops
void initialize_skippable_cutscene() {
	_GP(play).end_cutscene_music = -1;
}

void stop_fast_forwarding() {
	// when the skipping of a cutscene comes to an end, update things
	_GP(play).fast_forward = 0;
	setpal();
	if (_GP(play).end_cutscene_music >= 0)
		newmusic(_GP(play).end_cutscene_music);

	{
		AudioChannelsLock lock;

		// Restore actual volume of sounds
		for (int aa = 0; aa <= MAX_SOUND_CHANNELS; aa++) {
			auto *ch = lock.GetChannelIfPlaying(aa);
			if (ch) {
				ch->set_mute(false);
			}
		}
	} // -- AudioChannelsLock

	update_music_volume();
}

// allowHotspot0 defines whether Hotspot 0 returns LOCTYPE_HOTSPOT
// or whether it returns 0
int __GetLocationType(int xxx, int yyy, int allowHotspot0) {
	_G(getloctype_index) = 0;
	// If it's not in ProcessClick, then return 0 when over a GUI
	if ((GetGUIAt(xxx, yyy) >= 0) && (_G(getloctype_throughgui) == 0))
		return 0;

	_G(getloctype_throughgui) = 0;

	const int scrx = xxx;
	const int scry = yyy;
	VpPoint vpt = _GP(play).ScreenToRoomDivDown(xxx, yyy);
	if (vpt.second < 0)
		return 0;
	xxx = vpt.first.X;
	yyy = vpt.first.Y;
	if ((xxx >= _GP(thisroom).Width) | (xxx < 0) | (yyy < 0) | (yyy >= _GP(thisroom).Height))
		return 0;

	// check characters, objects and walkbehinds, work out which is
	// foremost visible to the player
	int charat = is_pos_on_character(xxx, yyy);
	int hsat = get_hotspot_at(xxx, yyy);
	int objat = GetObjectIDAtScreen(scrx, scry);

	data_to_game_coords(&xxx, &yyy);

	int wbat = _GP(thisroom).WalkBehindMask->GetPixel(xxx, yyy);

	if (wbat <= 0) wbat = 0;
	else wbat = _G(croom)->walkbehind_base[wbat];

	int winner = 0;
	// if it's an Ignore Walkbehinds object, then ignore the walkbehind
	if ((objat >= 0) && ((_G(objs)[objat].flags & OBJF_NOWALKBEHINDS) != 0))
		wbat = 0;
	if ((charat >= 0) && ((_GP(game).chars[charat].flags & CHF_NOWALKBEHINDS) != 0))
		wbat = 0;

	if ((charat >= 0) && (objat >= 0)) {
		if ((wbat > _G(obj_lowest_yp)) && (wbat > _G(char_lowest_yp)))
			winner = LOCTYPE_HOTSPOT;
		else if (_G(obj_lowest_yp) > _G(char_lowest_yp))
			winner = LOCTYPE_OBJ;
		else
			winner = LOCTYPE_CHAR;
	} else if (charat >= 0) {
		if (wbat > _G(char_lowest_yp))
			winner = LOCTYPE_HOTSPOT;
		else
			winner = LOCTYPE_CHAR;
	} else if (objat >= 0) {
		if (wbat > _G(obj_lowest_yp))
			winner = LOCTYPE_HOTSPOT;
		else
			winner = LOCTYPE_OBJ;
	}

	if (winner == 0) {
		if (hsat >= 0)
			winner = LOCTYPE_HOTSPOT;
	}

	if ((winner == LOCTYPE_HOTSPOT) && (!allowHotspot0) && (hsat == 0))
		winner = 0;

	if (winner == LOCTYPE_HOTSPOT)
		_G(getloctype_index) = hsat;
	else if (winner == LOCTYPE_CHAR)
		_G(getloctype_index) = charat;
	else if (winner == LOCTYPE_OBJ)
		_G(getloctype_index) = objat;

	return winner;
}

// Called whenever game looses input focus
void display_switch_out() {
	_G(switched_away) = true;
	ags_clear_input_buffer();
	// Always unlock mouse when switching out from the game
	_GP(mouse).UnlockFromWindow();
	_G(platform)->DisplaySwitchOut();
	_G(platform)->ExitFullscreenMode();
}

void display_switch_out_suspend() {
	// this is only called if in SWITCH_PAUSE mode
	//debug_script_warn("display_switch_out");
	display_switch_out();

	_G(switching_away_from_game)++;

	_G(platform)->PauseApplication();

	// allow background running temporarily to halt the sound
	if (set_display_switch_mode(SWITCH_BACKGROUND) == -1)
		set_display_switch_mode(SWITCH_BACKAMNESIA);

	{
		// stop the sound stuttering
		AudioChannelsLock lock;
		for (int i = 0; i <= MAX_SOUND_CHANNELS; i++) {
			auto *ch = lock.GetChannelIfPlaying(i);
			if (ch) {
				ch->pause();
			}
		}
	} // -- AudioChannelsLock

	_G(platform)->Delay(1000);

	// restore the callbacks
	SetMultitasking(0);

	_G(switching_away_from_game)--;
}

// Called whenever game gets input focus
void display_switch_in() {
	_G(switched_away) = false;
	if (_G(gfxDriver)) {
		DisplayMode mode = _G(gfxDriver)->GetDisplayMode();
		if (!mode.Windowed)
			_G(platform)->EnterFullscreenMode(mode);
	}
	_G(platform)->DisplaySwitchIn();
	ags_clear_input_buffer();
	// If auto lock option is set, lock mouse to the game window
	if (_GP(usetup).mouse_auto_lock && _GP(scsystem).windowed)
		_GP(mouse).TryLockToWindow();
}

void display_switch_in_resume() {
	display_switch_in();

	{
		AudioChannelsLock lock;
		for (int i = 0; i <= MAX_SOUND_CHANNELS; i++) {
			auto *ch = lock.GetChannelIfPlaying(i);
			if (ch) {
				ch->resume();
			}
		}
	} // -- AudioChannelsLock

	// clear the screen if necessary
	if (_G(gfxDriver) && _G(gfxDriver)->UsesMemoryBackBuffer())
		_G(gfxDriver)->ClearRectangle(0, 0, _GP(game).GetGameRes().Width - 1, _GP(game).GetGameRes().Height - 1, nullptr);

	_G(platform)->ResumeApplication();
}

void replace_tokens(const char *srcmes, char *destm, int maxlen) {
	int indxdest = 0, indxsrc = 0;
	const char *srcp;
	char *destp;
	while (srcmes[indxsrc] != 0) {
		srcp = &srcmes[indxsrc];
		destp = &destm[indxdest];
		if ((strncmp(srcp, "@IN", 3) == 0) | (strncmp(srcp, "@GI", 3) == 0)) {
			int tokentype = 0;
			if (srcp[1] == 'I') tokentype = 1;
			else tokentype = 2;
			int inx = atoi(&srcp[3]);
			srcp++;
			indxsrc += 2;
			while (srcp[0] != '@') {
				if (srcp[0] == 0) quit("!Display: special token not terminated");
				srcp++;
				indxsrc++;
			}
			char tval[10];
			if (tokentype == 1) {
				if ((inx < 1) | (inx >= _GP(game).numinvitems))
					quit("!Display: invalid inv item specified in @IN@");
				snprintf(tval, sizeof(tval), "%d", _G(playerchar)->inv[inx]);
			} else {
				if ((inx < 0) | (inx >= MAXGSVALUES))
					quit("!Display: invalid global int index speicifed in @GI@");
				snprintf(tval, sizeof(tval), "%d", GetGlobalInt(inx));
			}
			strcpy(destp, tval);
			indxdest += strlen(tval);
		} else {
			destp[0] = srcp[0];
			indxdest++;
			indxsrc++;
		}
		if (indxdest >= maxlen - 3)
			break;
	}
	destm[indxdest] = 0;
}

const char *get_global_message(int msnum) {
	if (_GP(game).messages[msnum - 500] == nullptr)
		return "";
	return get_translation(_GP(game).messages[msnum - 500]);
}

void get_message_text(int msnum, char *buffer, char giveErr) {
	int maxlen = 9999;
	if (!giveErr)
		maxlen = MAX_MAXSTRLEN;

	if (msnum >= 500) {

		if ((msnum >= MAXGLOBALMES + 500) || (_GP(game).messages[msnum - 500] == nullptr)) {
			if (giveErr)
				quit("!DisplayGlobalMessage: message does not exist");
			buffer[0] = 0;
			return;
		}
		buffer[0] = 0;
		replace_tokens(get_translation(_GP(game).messages[msnum - 500]), buffer, maxlen);
		return;
	} else if (msnum < 0 || (size_t)msnum >= _GP(thisroom).MessageCount) {
		if (giveErr)
			quit("!DisplayMessage: Invalid message number to display");
		buffer[0] = 0;
		return;
	}

	buffer[0] = 0;
	replace_tokens(get_translation(_GP(thisroom).Messages[msnum]), buffer, maxlen);
}

bool unserialize_audio_script_object(int index, const char *objectType, const char *serializedData, int dataSize) {
	if (strcmp(objectType, "AudioChannel") == 0) {
		_GP(ccDynamicAudio).Unserialize(index, serializedData, dataSize);
	} else if (strcmp(objectType, "AudioClip") == 0) {
		_GP(ccDynamicAudioClip).Unserialize(index, serializedData, dataSize);
	} else {
		return false;
	}
	return true;
}

//=============================================================================
//
// Script API Functions
//
//=============================================================================

// int  (int audioType);
RuntimeScriptValue Sc_Game_IsAudioPlaying(const RuntimeScriptValue *params, int32_t param_count) {
	API_SCALL_INT_PINT(Game_IsAudioPlaying);
}

// void (int audioType, int volumeDrop)
RuntimeScriptValue Sc_Game_SetAudioTypeSpeechVolumeDrop(const RuntimeScriptValue *params, int32_t param_count) {
	API_SCALL_VOID_PINT2(Game_SetAudioTypeSpeechVolumeDrop);
}

// void (int audioType, int volume, int changeType)
RuntimeScriptValue Sc_Game_SetAudioTypeVolume(const RuntimeScriptValue *params, int32_t param_count) {
	API_SCALL_VOID_PINT3(Game_SetAudioTypeVolume);
}

// void (int audioType)
RuntimeScriptValue Sc_Game_StopAudio(const RuntimeScriptValue *params, int32_t param_count) {
	API_SCALL_VOID_PINT(Game_StopAudio);
}

// int (const char *newFilename)
RuntimeScriptValue Sc_Game_ChangeTranslation(const RuntimeScriptValue *params, int32_t param_count) {
	API_SCALL_INT_POBJ(Game_ChangeTranslation, const char);
}

// int (const char *token)
RuntimeScriptValue Sc_Game_DoOnceOnly(const RuntimeScriptValue *params, int32_t param_count) {
	API_SCALL_INT_POBJ(Game_DoOnceOnly, const char);
}

// int (int red, int grn, int blu)
RuntimeScriptValue Sc_Game_GetColorFromRGB(const RuntimeScriptValue *params, int32_t param_count) {
	API_SCALL_INT_PINT3(Game_GetColorFromRGB);
}

// int (int viewNumber, int loopNumber)
RuntimeScriptValue Sc_Game_GetFrameCountForLoop(const RuntimeScriptValue *params, int32_t param_count) {
	API_SCALL_INT_PINT2(Game_GetFrameCountForLoop);
}

// const char* (int x, int y)
RuntimeScriptValue Sc_Game_GetLocationName(const RuntimeScriptValue *params, int32_t param_count) {
	API_CONST_SCALL_OBJ_PINT2(const char, _GP(myScriptStringImpl), Game_GetLocationName);
}

// int (int viewNumber)
RuntimeScriptValue Sc_Game_GetLoopCountForView(const RuntimeScriptValue *params, int32_t param_count) {
	API_SCALL_INT_PINT(Game_GetLoopCountForView);
}

// int ()
RuntimeScriptValue Sc_Game_GetMODPattern(const RuntimeScriptValue *params, int32_t param_count) {
	API_SCALL_INT(Game_GetMODPattern);
}

// int (int viewNumber, int loopNumber)
RuntimeScriptValue Sc_Game_GetRunNextSettingForLoop(const RuntimeScriptValue *params, int32_t param_count) {
	API_SCALL_INT_PINT2(Game_GetRunNextSettingForLoop);
}

// const char* (int slnum)
RuntimeScriptValue Sc_Game_GetSaveSlotDescription(const RuntimeScriptValue *params, int32_t param_count) {
	API_CONST_SCALL_OBJ_PINT(const char, _GP(myScriptStringImpl), Game_GetSaveSlotDescription);
}

// ScriptViewFrame* (int viewNumber, int loopNumber, int frame)
RuntimeScriptValue Sc_Game_GetViewFrame(const RuntimeScriptValue *params, int32_t param_count) {
	API_SCALL_OBJAUTO_PINT3(ScriptViewFrame, Game_GetViewFrame);
}

// const char* (const char *msg)
RuntimeScriptValue Sc_Game_InputBox(const RuntimeScriptValue *params, int32_t param_count) {
	API_CONST_SCALL_OBJ_POBJ(const char, _GP(myScriptStringImpl), Game_InputBox, const char);
}

// int (const char *newFolder)
RuntimeScriptValue Sc_Game_SetSaveGameDirectory(const RuntimeScriptValue *params, int32_t param_count) {
	API_SCALL_INT_POBJ(Game_SetSaveGameDirectory, const char);
}

// void (int evenAmbient);
RuntimeScriptValue Sc_StopAllSounds(const RuntimeScriptValue *params, int32_t param_count) {
	API_SCALL_VOID_PINT(StopAllSounds);
}

// int ()
RuntimeScriptValue Sc_Game_GetCharacterCount(const RuntimeScriptValue *params, int32_t param_count) {
	API_SCALL_INT(Game_GetCharacterCount);
}

// int ()
RuntimeScriptValue Sc_Game_GetDialogCount(const RuntimeScriptValue *params, int32_t param_count) {
	API_SCALL_INT(Game_GetDialogCount);
}

// const char *()
RuntimeScriptValue Sc_Game_GetFileName(const RuntimeScriptValue *params, int32_t param_count) {
	API_CONST_SCALL_OBJ(const char, _GP(myScriptStringImpl), Game_GetFileName);
}

// int ()
RuntimeScriptValue Sc_Game_GetFontCount(const RuntimeScriptValue *params, int32_t param_count) {
	API_SCALL_INT(Game_GetFontCount);
}

// const char* (int index)
RuntimeScriptValue Sc_Game_GetGlobalMessages(const RuntimeScriptValue *params, int32_t param_count) {
	API_CONST_SCALL_OBJ_PINT(const char, _GP(myScriptStringImpl), Game_GetGlobalMessages);
}

// const char* (int index)
RuntimeScriptValue Sc_Game_GetGlobalStrings(const RuntimeScriptValue *params, int32_t param_count) {
	API_CONST_SCALL_OBJ_PINT(const char, _GP(myScriptStringImpl), Game_GetGlobalStrings);
}

// void  (int index, char *newval);
RuntimeScriptValue Sc_SetGlobalString(const RuntimeScriptValue *params, int32_t param_count) {
	API_SCALL_VOID_PINT_POBJ(SetGlobalString, const char);
}

// int ()
RuntimeScriptValue Sc_Game_GetGUICount(const RuntimeScriptValue *params, int32_t param_count) {
	API_SCALL_INT(Game_GetGUICount);
}

// int ()
RuntimeScriptValue Sc_Game_GetIgnoreUserInputAfterTextTimeoutMs(const RuntimeScriptValue *params, int32_t param_count) {
	API_SCALL_INT(Game_GetIgnoreUserInputAfterTextTimeoutMs);
}

// void (int newValueMs)
RuntimeScriptValue Sc_Game_SetIgnoreUserInputAfterTextTimeoutMs(const RuntimeScriptValue *params, int32_t param_count) {
	API_SCALL_VOID_PINT(Game_SetIgnoreUserInputAfterTextTimeoutMs);
}

// int ()
RuntimeScriptValue Sc_Game_GetInSkippableCutscene(const RuntimeScriptValue *params, int32_t param_count) {
	API_SCALL_INT(Game_GetInSkippableCutscene);
}

// int ()
RuntimeScriptValue Sc_Game_GetInventoryItemCount(const RuntimeScriptValue *params, int32_t param_count) {
	API_SCALL_INT(Game_GetInventoryItemCount);
}

// int ()
RuntimeScriptValue Sc_Game_GetMinimumTextDisplayTimeMs(const RuntimeScriptValue *params, int32_t param_count) {
	API_SCALL_INT(Game_GetMinimumTextDisplayTimeMs);
}

// void (int newTextMinTime)
RuntimeScriptValue Sc_Game_SetMinimumTextDisplayTimeMs(const RuntimeScriptValue *params, int32_t param_count) {
	API_SCALL_VOID_PINT(Game_SetMinimumTextDisplayTimeMs);
}

// int ()
RuntimeScriptValue Sc_Game_GetMouseCursorCount(const RuntimeScriptValue *params, int32_t param_count) {
	API_SCALL_INT(Game_GetMouseCursorCount);
}

// const char *()
RuntimeScriptValue Sc_Game_GetName(const RuntimeScriptValue *params, int32_t param_count) {
	API_CONST_SCALL_OBJ(const char, _GP(myScriptStringImpl), Game_GetName);
}

// void (const char *newName)
RuntimeScriptValue Sc_Game_SetName(const RuntimeScriptValue *params, int32_t param_count) {
	API_SCALL_VOID_POBJ(Game_SetName, const char);
}

// int ()
RuntimeScriptValue Sc_Game_GetNormalFont(const RuntimeScriptValue *params, int32_t param_count) {
	API_SCALL_INT(Game_GetNormalFont);
}

// void  (int fontnum);
RuntimeScriptValue Sc_SetNormalFont(const RuntimeScriptValue *params, int32_t param_count) {
	API_SCALL_VOID_PINT(SetNormalFont);
}

// int ()
RuntimeScriptValue Sc_Game_GetSkippingCutscene(const RuntimeScriptValue *params, int32_t param_count) {
	API_SCALL_INT(Game_GetSkippingCutscene);
}

// int ()
RuntimeScriptValue Sc_Game_GetSpeechFont(const RuntimeScriptValue *params, int32_t param_count) {
	API_SCALL_INT(Game_GetSpeechFont);
}

// void  (int fontnum);
RuntimeScriptValue Sc_SetSpeechFont(const RuntimeScriptValue *params, int32_t param_count) {
	API_SCALL_VOID_PINT(SetSpeechFont);
}

// int (int spriteNum)
RuntimeScriptValue Sc_Game_GetSpriteWidth(const RuntimeScriptValue *params, int32_t param_count) {
	API_SCALL_INT_PINT(Game_GetSpriteWidth);
}

// int (int spriteNum)
RuntimeScriptValue Sc_Game_GetSpriteHeight(const RuntimeScriptValue *params, int32_t param_count) {
	API_SCALL_INT_PINT(Game_GetSpriteHeight);
}

// int ()
RuntimeScriptValue Sc_Game_GetTextReadingSpeed(const RuntimeScriptValue *params, int32_t param_count) {
	API_SCALL_INT(Game_GetTextReadingSpeed);
}

// void (int newTextSpeed)
RuntimeScriptValue Sc_Game_SetTextReadingSpeed(const RuntimeScriptValue *params, int32_t param_count) {
	API_SCALL_VOID_PINT(Game_SetTextReadingSpeed);
}

// const char* ()
RuntimeScriptValue Sc_Game_GetTranslationFilename(const RuntimeScriptValue *params, int32_t param_count) {
	API_CONST_SCALL_OBJ(const char, _GP(myScriptStringImpl), Game_GetTranslationFilename);
}

// int ()
RuntimeScriptValue Sc_Game_GetUseNativeCoordinates(const RuntimeScriptValue *params, int32_t param_count) {
	API_SCALL_INT(Game_GetUseNativeCoordinates);
}

// int ()
RuntimeScriptValue Sc_Game_GetViewCount(const RuntimeScriptValue *params, int32_t param_count) {
	API_SCALL_INT(Game_GetViewCount);
}

RuntimeScriptValue Sc_Game_GetAudioClipCount(const RuntimeScriptValue *params, int32_t param_count) {
	API_VARGET_INT(_GP(game).audioClips.size());
}

RuntimeScriptValue Sc_Game_GetAudioClip(const RuntimeScriptValue *params, int32_t param_count) {
	API_SCALL_OBJ_PINT(ScriptAudioClip, _GP(ccDynamicAudioClip), Game_GetAudioClip);
}

RuntimeScriptValue Sc_Game_IsPluginLoaded(const RuntimeScriptValue *params, int32_t param_count) {
	API_SCALL_BOOL_OBJ(pl_is_plugin_loaded, const char);
}

RuntimeScriptValue Sc_Game_PlayVoiceClip(const RuntimeScriptValue *params, int32_t param_count) {
	API_SCALL_OBJ_POBJ_PINT_PBOOL(ScriptAudioChannel, _GP(ccDynamicAudio), PlayVoiceClip, CharacterInfo);
}

RuntimeScriptValue Sc_Game_GetCamera(const RuntimeScriptValue *params, int32_t param_count) {
	API_SCALL_OBJAUTO(ScriptCamera, Game_GetCamera);
}

RuntimeScriptValue Sc_Game_GetCameraCount(const RuntimeScriptValue *params, int32_t param_count) {
	API_SCALL_INT(Game_GetCameraCount);
}

RuntimeScriptValue Sc_Game_GetAnyCamera(const RuntimeScriptValue *params, int32_t param_count) {
	API_SCALL_OBJAUTO_PINT(ScriptCamera, Game_GetAnyCamera);
}

RuntimeScriptValue Sc_Game_SimulateKeyPress(const RuntimeScriptValue *params, int32_t param_count) {
	API_SCALL_VOID_PINT(Game_SimulateKeyPress);
}

void RegisterGameAPI() {
	ccAddExternalStaticFunction("Game::IsAudioPlaying^1",                       Sc_Game_IsAudioPlaying);
	ccAddExternalStaticFunction("Game::SetAudioTypeSpeechVolumeDrop^2",         Sc_Game_SetAudioTypeSpeechVolumeDrop);
	ccAddExternalStaticFunction("Game::SetAudioTypeVolume^3",                   Sc_Game_SetAudioTypeVolume);
	ccAddExternalStaticFunction("Game::StopAudio^1",                            Sc_Game_StopAudio);
	ccAddExternalStaticFunction("Game::ChangeTranslation^1",                    Sc_Game_ChangeTranslation);
	ccAddExternalStaticFunction("Game::DoOnceOnly^1",                           Sc_Game_DoOnceOnly);
	ccAddExternalStaticFunction("Game::GetColorFromRGB^3",                      Sc_Game_GetColorFromRGB);
	ccAddExternalStaticFunction("Game::GetFrameCountForLoop^2",                 Sc_Game_GetFrameCountForLoop);
	ccAddExternalStaticFunction("Game::GetLocationName^2",                      Sc_Game_GetLocationName);
	ccAddExternalStaticFunction("Game::GetLoopCountForView^1",                  Sc_Game_GetLoopCountForView);
	ccAddExternalStaticFunction("Game::GetMODPattern^0",                        Sc_Game_GetMODPattern);
	ccAddExternalStaticFunction("Game::GetRunNextSettingForLoop^2",             Sc_Game_GetRunNextSettingForLoop);
	ccAddExternalStaticFunction("Game::GetSaveSlotDescription^1",               Sc_Game_GetSaveSlotDescription);
	ccAddExternalStaticFunction("Game::GetViewFrame^3",                         Sc_Game_GetViewFrame);
	ccAddExternalStaticFunction("Game::InputBox^1",                             Sc_Game_InputBox);
	ccAddExternalStaticFunction("Game::SetSaveGameDirectory^1",                 Sc_Game_SetSaveGameDirectory);
	ccAddExternalStaticFunction("Game::StopSound^1",                            Sc_StopAllSounds);
	ccAddExternalStaticFunction("Game::get_CharacterCount",                     Sc_Game_GetCharacterCount);
	ccAddExternalStaticFunction("Game::get_DialogCount",                        Sc_Game_GetDialogCount);
	ccAddExternalStaticFunction("Game::get_FileName",                           Sc_Game_GetFileName);
	ccAddExternalStaticFunction("Game::get_FontCount",                          Sc_Game_GetFontCount);
	ccAddExternalStaticFunction("Game::geti_GlobalMessages",                    Sc_Game_GetGlobalMessages);
	ccAddExternalStaticFunction("Game::geti_GlobalStrings",                     Sc_Game_GetGlobalStrings);
	ccAddExternalStaticFunction("Game::seti_GlobalStrings",                     Sc_SetGlobalString);
	ccAddExternalStaticFunction("Game::get_GUICount",                           Sc_Game_GetGUICount);
	ccAddExternalStaticFunction("Game::get_IgnoreUserInputAfterTextTimeoutMs",  Sc_Game_GetIgnoreUserInputAfterTextTimeoutMs);
	ccAddExternalStaticFunction("Game::set_IgnoreUserInputAfterTextTimeoutMs",  Sc_Game_SetIgnoreUserInputAfterTextTimeoutMs);
	ccAddExternalStaticFunction("Game::get_InSkippableCutscene",                Sc_Game_GetInSkippableCutscene);
	ccAddExternalStaticFunction("Game::get_InventoryItemCount",                 Sc_Game_GetInventoryItemCount);
	ccAddExternalStaticFunction("Game::get_MinimumTextDisplayTimeMs",           Sc_Game_GetMinimumTextDisplayTimeMs);
	ccAddExternalStaticFunction("Game::set_MinimumTextDisplayTimeMs",           Sc_Game_SetMinimumTextDisplayTimeMs);
	ccAddExternalStaticFunction("Game::get_MouseCursorCount",                   Sc_Game_GetMouseCursorCount);
	ccAddExternalStaticFunction("Game::get_Name",                               Sc_Game_GetName);
	ccAddExternalStaticFunction("Game::set_Name",                               Sc_Game_SetName);
	ccAddExternalStaticFunction("Game::get_NormalFont",                         Sc_Game_GetNormalFont);
	ccAddExternalStaticFunction("Game::set_NormalFont",                         Sc_SetNormalFont);
	ccAddExternalStaticFunction("Game::get_SkippingCutscene",                   Sc_Game_GetSkippingCutscene);
	ccAddExternalStaticFunction("Game::get_SpeechFont",                         Sc_Game_GetSpeechFont);
	ccAddExternalStaticFunction("Game::set_SpeechFont",                         Sc_SetSpeechFont);
	ccAddExternalStaticFunction("Game::geti_SpriteWidth",                       Sc_Game_GetSpriteWidth);
	ccAddExternalStaticFunction("Game::geti_SpriteHeight",                      Sc_Game_GetSpriteHeight);
	ccAddExternalStaticFunction("Game::get_TextReadingSpeed",                   Sc_Game_GetTextReadingSpeed);
	ccAddExternalStaticFunction("Game::set_TextReadingSpeed",                   Sc_Game_SetTextReadingSpeed);
	ccAddExternalStaticFunction("Game::get_TranslationFilename",                Sc_Game_GetTranslationFilename);
	ccAddExternalStaticFunction("Game::get_UseNativeCoordinates",               Sc_Game_GetUseNativeCoordinates);
	ccAddExternalStaticFunction("Game::get_ViewCount",                          Sc_Game_GetViewCount);
	ccAddExternalStaticFunction("Game::get_AudioClipCount",                     Sc_Game_GetAudioClipCount);
	ccAddExternalStaticFunction("Game::geti_AudioClips",                        Sc_Game_GetAudioClip);
	ccAddExternalStaticFunction("Game::IsPluginLoaded",                         Sc_Game_IsPluginLoaded);
	ccAddExternalStaticFunction("Game::PlayVoiceClip",                          Sc_Game_PlayVoiceClip);
	ccAddExternalStaticFunction("Game::SimulateKeyPress",                       Sc_Game_SimulateKeyPress);

	ccAddExternalStaticFunction("Game::get_Camera",                             Sc_Game_GetCamera);
	ccAddExternalStaticFunction("Game::get_CameraCount",                        Sc_Game_GetCameraCount);
	ccAddExternalStaticFunction("Game::geti_Cameras",                           Sc_Game_GetAnyCamera);

	/* ----------------------- Registering unsafe exports for plugins -----------------------*/

	ccAddExternalFunctionForPlugin("Game::IsAudioPlaying^1", (void *)Game_IsAudioPlaying);
	ccAddExternalFunctionForPlugin("Game::SetAudioTypeSpeechVolumeDrop^2", (void *)Game_SetAudioTypeSpeechVolumeDrop);
	ccAddExternalFunctionForPlugin("Game::SetAudioTypeVolume^3", (void *)Game_SetAudioTypeVolume);
	ccAddExternalFunctionForPlugin("Game::StopAudio^1", (void *)Game_StopAudio);
	ccAddExternalFunctionForPlugin("Game::ChangeTranslation^1", (void *)Game_ChangeTranslation);
	ccAddExternalFunctionForPlugin("Game::DoOnceOnly^1", (void *)Game_DoOnceOnly);
	ccAddExternalFunctionForPlugin("Game::GetColorFromRGB^3", (void *)Game_GetColorFromRGB);
	ccAddExternalFunctionForPlugin("Game::GetFrameCountForLoop^2", (void *)Game_GetFrameCountForLoop);
	ccAddExternalFunctionForPlugin("Game::GetLocationName^2", (void *)Game_GetLocationName);
	ccAddExternalFunctionForPlugin("Game::GetLoopCountForView^1", (void *)Game_GetLoopCountForView);
	ccAddExternalFunctionForPlugin("Game::GetMODPattern^0", (void *)Game_GetMODPattern);
	ccAddExternalFunctionForPlugin("Game::GetRunNextSettingForLoop^2", (void *)Game_GetRunNextSettingForLoop);
	ccAddExternalFunctionForPlugin("Game::GetSaveSlotDescription^1", (void *)Game_GetSaveSlotDescription);
	ccAddExternalFunctionForPlugin("Game::GetViewFrame^3", (void *)Game_GetViewFrame);
	ccAddExternalFunctionForPlugin("Game::InputBox^1", (void *)Game_InputBox);
	ccAddExternalFunctionForPlugin("Game::SetSaveGameDirectory^1", (void *)Game_SetSaveGameDirectory);
	ccAddExternalFunctionForPlugin("Game::StopSound^1", (void *)StopAllSounds);
	ccAddExternalFunctionForPlugin("Game::get_CharacterCount", (void *)Game_GetCharacterCount);
	ccAddExternalFunctionForPlugin("Game::get_DialogCount", (void *)Game_GetDialogCount);
	ccAddExternalFunctionForPlugin("Game::get_FileName", (void *)Game_GetFileName);
	ccAddExternalFunctionForPlugin("Game::get_FontCount", (void *)Game_GetFontCount);
	ccAddExternalFunctionForPlugin("Game::geti_GlobalMessages", (void *)Game_GetGlobalMessages);
	ccAddExternalFunctionForPlugin("Game::geti_GlobalStrings", (void *)Game_GetGlobalStrings);
	ccAddExternalFunctionForPlugin("Game::seti_GlobalStrings", (void *)SetGlobalString);
	ccAddExternalFunctionForPlugin("Game::get_GUICount", (void *)Game_GetGUICount);
	ccAddExternalFunctionForPlugin("Game::get_IgnoreUserInputAfterTextTimeoutMs", (void *)Game_GetIgnoreUserInputAfterTextTimeoutMs);
	ccAddExternalFunctionForPlugin("Game::set_IgnoreUserInputAfterTextTimeoutMs", (void *)Game_SetIgnoreUserInputAfterTextTimeoutMs);
	ccAddExternalFunctionForPlugin("Game::get_InSkippableCutscene", (void *)Game_GetInSkippableCutscene);
	ccAddExternalFunctionForPlugin("Game::get_InventoryItemCount", (void *)Game_GetInventoryItemCount);
	ccAddExternalFunctionForPlugin("Game::get_MinimumTextDisplayTimeMs", (void *)Game_GetMinimumTextDisplayTimeMs);
	ccAddExternalFunctionForPlugin("Game::set_MinimumTextDisplayTimeMs", (void *)Game_SetMinimumTextDisplayTimeMs);
	ccAddExternalFunctionForPlugin("Game::get_MouseCursorCount", (void *)Game_GetMouseCursorCount);
	ccAddExternalFunctionForPlugin("Game::get_Name", (void *)Game_GetName);
	ccAddExternalFunctionForPlugin("Game::set_Name", (void *)Game_SetName);
	ccAddExternalFunctionForPlugin("Game::get_NormalFont", (void *)Game_GetNormalFont);
	ccAddExternalFunctionForPlugin("Game::set_NormalFont", (void *)SetNormalFont);
	ccAddExternalFunctionForPlugin("Game::get_SkippingCutscene", (void *)Game_GetSkippingCutscene);
	ccAddExternalFunctionForPlugin("Game::get_SpeechFont", (void *)Game_GetSpeechFont);
	ccAddExternalFunctionForPlugin("Game::set_SpeechFont", (void *)SetSpeechFont);
	ccAddExternalFunctionForPlugin("Game::geti_SpriteWidth", (void *)Game_GetSpriteWidth);
	ccAddExternalFunctionForPlugin("Game::geti_SpriteHeight", (void *)Game_GetSpriteHeight);
	ccAddExternalFunctionForPlugin("Game::get_TextReadingSpeed", (void *)Game_GetTextReadingSpeed);
	ccAddExternalFunctionForPlugin("Game::set_TextReadingSpeed", (void *)Game_SetTextReadingSpeed);
	ccAddExternalFunctionForPlugin("Game::get_TranslationFilename", (void *)Game_GetTranslationFilename);
	ccAddExternalFunctionForPlugin("Game::get_UseNativeCoordinates", (void *)Game_GetUseNativeCoordinates);
	ccAddExternalFunctionForPlugin("Game::get_ViewCount", (void *)Game_GetViewCount);
	ccAddExternalFunctionForPlugin("Game::PlayVoiceClip", (void *)PlayVoiceClip);
}

void RegisterStaticObjects() {
	ccAddExternalStaticObject("game", &_GP(play), &GameStaticManager);
	ccAddExternalStaticObject("gs_globals", &_GP(play).globalvars[0], &GlobalStaticManager);
	ccAddExternalStaticObject("mouse", &_GP(scmouse), &GlobalStaticManager);
	ccAddExternalStaticObject("palette", &palette[0], &GlobalStaticManager);
	ccAddExternalStaticObject("system", &_GP(scsystem), &GlobalStaticManager);
	ccAddExternalStaticObject("savegameindex", &_GP(play).filenumbers[0], &GlobalStaticManager);
}

} // namespace AGS3
