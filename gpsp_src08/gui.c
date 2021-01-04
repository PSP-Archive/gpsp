/* gameplaySP
 *
 * Copyright (C) 2006 Exophase <exophase@gmail.com>
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public Licens e as
 * published by the Free Software Foundation; either version 2 of
 * the License, or (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA 02110-1301 USA
 */

#include <sys/stat.h>
#include <unistd.h>
#include <ctype.h>
#include <dirent.h>
#include "common.h"

#define MAX_PATH 1024

// Blatantly stolen and trimmed from MZX (megazeux.sourceforge.net)

#define FILE_LIST_ROWS 25
#define FILE_LIST_POSITION 5
#define DIR_LIST_POSITION 360

#ifdef PSP_BUILD

#define color16(red, green, blue)                                             \
  (blue << 11) | (green << 5) | red                                           \

#else

#define color16(red, green, blue)                                             \
  (red << 11) | (green << 5) | blue                                           \

#endif

#define COLOR_BG            color16(2, 8, 10)
#define COLOR_ROM_INFO      color16(22, 36, 26)
#define COLOR_ACTIVE_ITEM   color16(31, 63, 31)
#define COLOR_INACTIVE_ITEM color16(13, 40, 18)
#define COLOR_FRAMESKIP_BAR color16(15, 31, 31)
#define COLOR_HELP_TEXT     color16(16, 40, 24)

int sort_function(const void *dest_str_ptr, const void *src_str_ptr)
{
  char *dest_str = *((char **)dest_str_ptr);
  char *src_str = *((char **)src_str_ptr);

  if(src_str[0] == '.')
    return 1;

  if(dest_str[0] == '.')
    return -1;

  return strcasecmp(dest_str, src_str);
}

s32 load_file(u8 **wildcards, u8 *result)
{
  DIR *current_dir;
  struct dirent *current_file;
  struct stat file_info;
  u8 current_dir_name[MAX_PATH];
  u8 current_dir_short[81];
  u32 current_dir_length;
  u32 total_filenames_allocated;
  u32 total_dirnames_allocated;
  u8 **file_list;
  u8 **dir_list;
  u32 num_files;
  u32 num_dirs;
  u8 *file_name;
  u32 file_name_length;
  u32 ext_pos = -1;
  u32 chosen_file, chosen_dir;
  u32 dialog_result = 1;
  s32 return_value = 1;
  u32 current_file_selection;
  u32 current_file_scroll_value;
  u32 current_dir_selection;
  u32 current_dir_scroll_value;
  u32 current_file_in_scroll;
  u32 current_dir_in_scroll;
  u32 current_file_number, current_dir_number;
  u32 current_column = 0;
  u32 repeat;
  u32 i;
  gui_action_type gui_action;

  while(return_value == 1)
  {
    current_file_selection = 0;
    current_file_scroll_value = 0;
    current_dir_selection = 0;
    current_dir_scroll_value = 0;
    current_file_in_scroll = 0;
    current_dir_in_scroll = 0;

    total_filenames_allocated = 32;
    total_dirnames_allocated = 32;
    file_list = (u8 **)malloc(sizeof(u8 *) * 32);
    dir_list = (u8 **)malloc(sizeof(u8 *) * 32);
    memset(file_list, 0, sizeof(u8 *) * 32);
    memset(dir_list, 0, sizeof(u8 *) * 32);

    num_files = 0;
    num_dirs = 0;
    chosen_file = 0;
    chosen_dir = 0;

    getcwd(current_dir_name, MAX_PATH);

    current_dir = opendir(current_dir_name);

    do
    {
      if(current_dir)
        current_file = readdir(current_dir);
      else
        current_file = NULL;

      if(current_file)
      {
        file_name = current_file->d_name;
        file_name_length = strlen(file_name);

        if((stat(file_name, &file_info) >= 0) &&
         ((file_name[0] != '.') || (file_name[1] == '.')))
        {
          if(S_ISDIR(file_info.st_mode))
          {
            dir_list[num_dirs] =
             (u8 *)malloc(file_name_length + 1);
            strcpy(dir_list[num_dirs], file_name);

            num_dirs++;
          }
          else
          {
            // Must match one of the wildcards, also ignore the .
            if(file_name_length >= 4)
            {
              if(file_name[file_name_length - 4] == '.')
                ext_pos = file_name_length - 4;
              else

              if(file_name[file_name_length - 3] == '.')
                ext_pos = file_name_length - 3;

              else
                ext_pos = 0;

              for(i = 0; wildcards[i] != NULL; i++)
              {
                if(!strcasecmp((file_name + ext_pos),
                 wildcards[i]))
                {
                  file_list[num_files] =
                   (u8 *)malloc(file_name_length + 1);

                  strcpy(file_list[num_files], file_name);

                  num_files++;
                  break;
                }
              }
            }
          }
        }

        if(num_files == total_filenames_allocated)
        {
          file_list = (u8 **)realloc(file_list, sizeof(u8 *) *
           total_filenames_allocated * 2);
          memset(file_list + total_filenames_allocated, 0,
           sizeof(u8 *) * total_filenames_allocated);
          total_filenames_allocated *= 2;
        }

        if(num_dirs == total_dirnames_allocated)
        {
          dir_list = (u8 **)realloc(dir_list, sizeof(u8 *) *
           total_dirnames_allocated * 2);
          memset(dir_list + total_dirnames_allocated, 0,
           sizeof(u8 *) * total_dirnames_allocated);
          total_dirnames_allocated *= 2;
        }
      }
    } while(current_file);

    qsort((void *)file_list, num_files, sizeof(u8 *), sort_function);
    qsort((void *)dir_list, num_dirs, sizeof(u8 *), sort_function);

    closedir(current_dir);

    current_dir_length = strlen(current_dir_name);

    if(current_dir_length > 80)
    {
      memcpy(current_dir_short, "...", 3);
      memcpy(current_dir_short + 3,
       current_dir_name + current_dir_length - 77, 77);
      current_dir_short[80] = 0;
    }
    else
    {
      memcpy(current_dir_short, current_dir_name,
       current_dir_length + 1);
    }

    repeat = 1;

    if(num_files == 0)
      current_column = 1;

    clear_screen(COLOR_BG);
    u8 print_buffer[81];

    while(repeat)
    {
      flip_screen();

      print_string(current_dir_short, COLOR_ACTIVE_ITEM, COLOR_BG, 0, 0);
      print_string("Press X to return to the main menu.",
       COLOR_HELP_TEXT, COLOR_BG, 20, 260);

      for(i = 0, current_file_number = i + current_file_scroll_value;
       i < FILE_LIST_ROWS; i++, current_file_number++)
      {
        if(current_file_number < num_files)
        {
          if((current_file_number == current_file_selection) &&
           (current_column == 0))
          {
            print_string(file_list[current_file_number], COLOR_ACTIVE_ITEM,
             COLOR_BG, FILE_LIST_POSITION, ((i + 1) * 10));
          }
          else
          {
            print_string(file_list[current_file_number], COLOR_INACTIVE_ITEM,
             COLOR_BG, FILE_LIST_POSITION, ((i + 1) * 10));
          }
        }
      }

      for(i = 0, current_dir_number = i + current_dir_scroll_value;
       i < FILE_LIST_ROWS; i++, current_dir_number++)
      {
        if(current_dir_number < num_dirs)
        {
          if((current_dir_number == current_dir_selection) &&
           (current_column == 1))
          {
            print_string(dir_list[current_dir_number], COLOR_ACTIVE_ITEM,
             COLOR_BG, DIR_LIST_POSITION, ((i + 1) * 10));
          }
          else
          {
            print_string(dir_list[current_dir_number], COLOR_INACTIVE_ITEM,
             COLOR_BG, DIR_LIST_POSITION, ((i + 1) * 10));
          }
        }
      }

      gui_action = get_gui_input();

      switch(gui_action)
      {
        case CURSOR_DOWN:
          if(current_column == 0)
          {
            if(current_file_selection < (num_files - 1))
            {
              current_file_selection++;
              if(current_file_in_scroll == (FILE_LIST_ROWS - 1))
              {
                clear_screen(COLOR_BG);
                current_file_scroll_value++;
              }
              else
              {
                current_file_in_scroll++;
              }
            }
          }
          else
          {
            if(current_dir_selection < (num_dirs - 1))
            {
              current_dir_selection++;
              if(current_dir_in_scroll == (FILE_LIST_ROWS - 1))
              {
                clear_screen(COLOR_BG);
                current_dir_scroll_value++;
              }
              else
              {
                current_dir_in_scroll++;
              }
            }
          }

          break;

        case CURSOR_UP:
          if(current_column == 0)
          {
            if(current_file_selection)
            {
              current_file_selection--;
              if(current_file_in_scroll == 0)
              {
                clear_screen(COLOR_BG);
                current_file_scroll_value--;
              }
              else
              {
                current_file_in_scroll--;
              }
            }
          }
          else
          {
            if(current_dir_selection)
            {
              current_dir_selection--;
              if(current_dir_in_scroll == 0)
              {
                clear_screen(COLOR_BG);
                current_dir_scroll_value--;
              }
              else
              {
                current_dir_in_scroll--;
              }
            }
          }
          break;

        case CURSOR_RIGHT:
          if(current_column == 0)
          {
            if(num_dirs != 0)
              current_column = 1;
          }
          break;

        case CURSOR_LEFT:
          if(current_column == 1)
          {
            if(num_files != 0)
              current_column = 0;
          }
          break;

        case CURSOR_SELECT:
          if(current_column == 1)
          {
            repeat = 0;
            chdir(dir_list[current_dir_selection]);
          }
          else
          {
            if(num_files != 0)
            {
              repeat = 0;
              return_value = 0;
              strcpy(result, file_list[current_file_selection]);
            }
          }
          break;

        case CURSOR_BACK:
#ifdef PSP_BUILD
          if(!strcmp(current_dir_name, "ms0:/PSP"))
            break;
#endif
          repeat = 0;
          chdir("..");
          break;

        case CURSOR_EXIT:
          return_value = -1;
          repeat = 0;
          break;
      }
    }

    for(i = 0; i < num_files; i++)
    {
      free(file_list[i]);
    }
    free(file_list);

    for(i = 0; i < num_dirs; i++)
    {
      free(dir_list[i]);
    }
    free(dir_list);
  }

  clear_screen(COLOR_BG);
  return return_value;
}


typedef enum
{
  MENU_DISPLAY_SCALE,
  MENU_FRAMESKIP,
  MENU_FRAMESKIP_VARIATION,
  MENU_SAVESTATE_LOAD,
  MENU_SAVESTATE_SAVE,
  MENU_SAVESTATE_OPTIONS,
  MENU_BACKUP_INTERVAL,
  MENU_CONFIG_GAMEPAD,
  MENU_LOAD_GAME,
  MENU_RESTART_GAME,
  MENU_RETURN,
  MENU_EXIT,
} main_menu_option_type;

u32 menu_line_positions[] =
{
  40,
  50,
  60,
  80,
  90,
  100,
  120,
  130,
  150,
  160,
  170,
  190,
};

#define NUM_MENU_OPTIONS 12

u8 frameskip_variation_options[2][8] = { "uniform", "random " };
u8 screen_ratio_options[3][16] =
 { "unscaled 3:2   ", "scaled 3:2     ", "fullscreen 16:9" };

u8 update_backup_options[2][10] = { "Exit only", "Automatic" };

u32 savestate_menu_line_positions[] =
{
  100,
  110,
  130,
  150,
  180
};

#define NUM_SAVESTATE_MENU_OPTIONS 5

#define NUM_BUTTON_OPTIONS 16

u8 gamepad_config_buttons[][13] =
{
  "UP         ",
  "DOWN       ",
  "LEFT       ",
  "RIGHT      ",
  "A          ",
  "B          ",
  "L          ",
  "R          ",
  "START      ",
  "SELECT     ",
  "FRAMESKIP  ",
  "MENU       ",
  "FASTFORWARD",
  "LOAD STATE ",
  "SAVE STATE ",
  "NOTHING    "
};

u8 gamepad_help[][71] =
{
  "Up button on GBA d-pad.                                              ",
  "Down button on GBA d-pad.                                            ",
  "Left button on GBA d-pad.                                            ",
  "Right button on GBA d-pad.                                           ",
  "A button on GBA.                                                     ",
  "B button on GBA.                                                     ",
  "Left shoulder button on GBA.                                         ",
  "Right shoulder button on GBA.                                        ",
  "Start button on GBA.                                                 ",
  "Select button on GBA.                                                ",
  "Brings up frameskip adjust bar and menu access.                      ",
  "Jumps directly to the menu.                                          ",
  "Toggles fastforward on/off (don't expect it to do much or anything)  ",
  "Loads the game state from the current slot.                          ",
  "Saves the game state to the current slot.                            ",
  "Does nothing.                                                        "
};

#define GAMEPAD_MENU_WIDTH 15

u8 gamepad_menu_lines[12][16] =
{
  "D-pad up:     ",
  "D-pad down:   ",
  "D-pad left:   ",
  "D-pad right:  ",
  "Circle:       ",
  "Cross:        ",
  "Square:       ",
  "Triangle:     ",
  "Left Trigger: ",
  "Right Trigger:",
  "Start:        ",
  "Select:       "
};


u8 menu_help[][351] =
{
  "Determines how the GBA screen is resized in relation to the entire   \n"
  "screen. Select unscaled 3:2 for GBA resolution, scaled 3:2 for GBA   \n"
  "aspect ratio scaled to fill the height of the PSP screen, and        \n"
  "fullscreen to fill the entire PSP screen.                            \n"
  "                                                                      ",

  "Select how many frames to render vs. how many to skip.               \n"
  "A frameskip value of N will render 1 out of every N + 1 frames, so   \n"
  "select frameskip 0 to render every frame. Higher frameskip values    \n"
  "will usually improve speed, with diminishing returns as N increases. \n"
  "                                                                      ",

  "If objects in the game flicker at a regular rate certain frameskip   \n"
  "values may cause them to normally disappear. Change this value to    \n"
  "'random' to avoid this. Do not use otherwise, as it tends to make the\n"
  "image quality worse, especially in high motion games.                \n"
  "                                                                      ",

  "Select to load the game state from the current slot for this game, if\n"
  "it exists (see the extended menu for more information)               \n"
  "Press left + right to change the current slot.                       \n"
  "                                                                     \n"
  "                                                                      ",

  "Select to save the game state from the current slot for this game.   \n"
  "See the extended menu for more information.                          \n"
  "Press left + right to change the current slot.                       \n"
  "                                                                     \n"
  "                                                                      ",

  "Select to enter a menu for loading, saving, and viewing the          \n"
  "currently active savestate for this game (or to load a savestate     \n"
  "file from another game)                                              \n"
  "                                                                     \n"
  "                                                                      ",

  "Determines when in-game save files should be written back to         \n"
  "memstick. If set to 'automatic' writebacks will occur shortly after  \n"
  "the game's backup is altered. On 'exit only' it will only be written \n"
  "back when you exit from this menu (NOT from using the home button).  \n"
  "Use the latter with extreme care.                                     ",

  "Select to change the in-game behavior of the PSP buttons.            \n"
  "                                                                     \n"
  "                                                                     \n"
  "                                                                     \n"
  "                                                                      ",

  "Select to load a new game.                                           \n"
  "                                                                     \n"
  "                                                                     \n"
  "                                                                     \n"
  "                                                                      ",

  "Select to restart the currently running game.                        \n"
  "                                                                     \n"
  "                                                                     \n"
  "                                                                     \n"
  "                                                                      ",

  "Select to resume gameplay.                                           \n"
  "                                                                     \n"
  "                                                                     \n"
  "                                                                     \n"
  "                                                                      ",

  "Select to exit gpSP.                                                 \n"
  "                                                                     \n"
  "                                                                     \n"
  "                                                                     \n"
  "                                                                      "
};


u8 savestate_menu_help[][71] =
{
  "Restore gameplay from the savestate stored in the current slot.      \n",
  "Save the gameplay state to the current savestate slot.               \n",
  "Restore gameplay from a savestate file.                              \n",
  "Change the current savestate slot.                                   \n",
  "Return to the main menu.                                             \n"
};

u32 gamepad_config_line_to_psp_button[] =
 { 8, 6, 7, 9, 1, 2, 3, 0, 4, 5, 11, 10 };

s32 load_game_config_file()
{
  u8 game_config_filename[512];
  u32 file_loaded = 0;
  change_ext(gamepak_filename, game_config_filename, ".cfg");

  file_open(game_config_file, game_config_filename, read);

  if(file_check_valid(game_config_file))
  {
    u32 file_size = file_length(game_config_filename, game_config_file);

    // Sanity check: File size must be the right size
    if(file_size == 12)
    {
      u32 file_options[file_size / 4];

      file_read_array(game_config_file, file_options);
      frameskip = file_options[0] % 100;
      frameskip_low = file_options[1] % 10;
      random_skip = file_options[2] & 1;

      file_close(game_config_file);
      file_loaded = 1;
    }
  }

  if(file_loaded)
    return 0;

  frameskip = 0;
  frameskip_low = 0;
  random_skip = 0;
  return -1;
}

s32 load_config_file()
{
  u8 config_path[512];
  #ifdef PSP_BUILD
    sprintf(config_path, "%s/%s", main_path, GPSP_CONFIG_FILENAME);
  #else
    sprintf(config_path, "%s\\%s", main_path, GPSP_CONFIG_FILENAME);
  #endif

  file_open(config_file, config_path, read);

  if(file_check_valid(config_file))
  {
    u32 file_size = file_length(config_path, config_file);

    // Sanity check: File size must be the right size
    if(file_size == 56)
    {
      u32 file_options[file_size / 4];
      u32 i;
      s32 menu_button = -1;
      file_read_array(config_file, file_options);

      screen_scale = file_options[0] % 3;
      update_backup_flag = file_options[1] % 2;

      // Sanity check: Make sure there's a MENU or FRAMESKIP
      // key, if not assign to triangle

      for(i = 0; i < 12; i++)
      {
        if((gamepad_config_map[i] == BUTTON_ID_MENU) ||
         (gamepad_config_map[i] == BUTTON_ID_FRAMESKIP))
        {
          menu_button = i;
        }
        gamepad_config_map[i] = file_options[2 + i] %
         (BUTTON_ID_NONE + 1);
      }

      if(menu_button == -1)
      {
        gamepad_config_map[0] = BUTTON_ID_FRAMESKIP;
      }

      file_close(config_file);
    }

    return 0;
  }

  return -1;
}

s32 save_game_config_file()
{
  u8 game_config_filename[512];
  change_ext(gamepak_filename, game_config_filename, ".cfg");

  file_open(game_config_file, game_config_filename, write);

  if(file_check_valid(game_config_file))
  {
    u32 file_options[3];

    file_options[0] = frameskip;
    file_options[1] = frameskip_low;
    file_options[2] = random_skip;

    file_write_array(game_config_file, file_options);
    file_close(game_config_file);

    return 0;
  }

  return -1;
}

s32 save_config_file()
{
  u8 config_path[512];
  #ifdef PSP_BUILD
    sprintf(config_path, "%s/%s", main_path, GPSP_CONFIG_FILENAME);
  #else
    sprintf(config_path, "%s\\%s", main_path, GPSP_CONFIG_FILENAME);
  #endif

  file_open(config_file, config_path, write);

  save_game_config_file();

  if(file_check_valid(config_file))
  {
    u32 file_options[14];
    u32 i;

    file_options[0] = screen_scale;
    file_options[1] = update_backup_flag;

    for(i = 0; i < 12; i++)
    {
      file_options[2 + i] = gamepad_config_map[i];
    }

    file_write_array(config_file, file_options);
    file_close(config_file);

    return 0;
  }

  return -1;
}

typedef enum
{
  MAIN_MENU,
  GAMEPAD_MENU,
  SAVESTATE_MENU,
  FRAMESKIP_MENU,
  CHEAT_MENU
} menu_type;

u32 current_savestate_slot = 0;

void get_savestate_snapshot(u8 *savestate_filename)
{
  u16 snapshot_buffer[240 * 160];
  u8 savestate_timestamp_string[80];

  file_open(savestate_file, savestate_filename, read);

  if(file_check_valid(savestate_file))
  {
    u8 weekday_strings[7][11] =
    {
      "Sunday", "Monday", "Tuesday", "Wednesday",
      "Thursday", "Friday", "Saturday"
    };
    time_t savestate_time_flat;
    struct tm *current_time;
    file_read_array(savestate_file, snapshot_buffer);
    file_read_variable(savestate_file, savestate_time_flat);

    file_close(savestate_file);

    current_time = localtime(&savestate_time_flat);
    sprintf(savestate_timestamp_string,
     "%s  %02d/%02d/%04d  %02d:%02d:%02d                ",
     weekday_strings[current_time->tm_wday], current_time->tm_mon + 1,
     current_time->tm_mday, current_time->tm_year + 1900,
     current_time->tm_hour, current_time->tm_min, current_time->tm_sec);

    savestate_timestamp_string[40] = 0;
    print_string(savestate_timestamp_string, COLOR_HELP_TEXT, COLOR_BG,
     10, 40);
  }
  else
  {
    memset(snapshot_buffer, 0, 240 * 160 * 2);
    print_string_ext("No savestate exists for this slot.",
     0xFFFF, 0x0000, 15, 75, snapshot_buffer, 240);
    print_string("---------- --/--/---- --:--:--          ", COLOR_HELP_TEXT,
     COLOR_BG, 10, 40);
  }
  blit_to_screen(snapshot_buffer, 240, 160, 230, 40);
}

void get_savestate_filename(u32 slot, u8 *name_buffer)
{
  u8 savestate_ext[16];

  sprintf(savestate_ext, "%d.svs", slot);
  change_ext(gamepak_filename, name_buffer, savestate_ext);

  get_savestate_snapshot(name_buffer);
}

void get_savestate_filename_noshot(u32 slot, u8 *name_buffer)
{
  u8 savestate_ext[16];

  sprintf(savestate_ext, "%d.svs", slot);
  change_ext(gamepak_filename, name_buffer, savestate_ext);
}

u32 menu(u16 *original_screen)
{
  u8 menu_lines[NUM_MENU_OPTIONS][41];
  u8 print_buffer[81];
  u32 current_option = 0;
  u32 savestate_slot = 0;
  gui_action_type gui_action;
  menu_type current_menu = MAIN_MENU;
  u32 i;
  u32 repeat = 1;
  video_scale_type display_mode = screen_scale;
  u32 return_value = 0;
  u32 first_load = 0;
  u8 savestate_ext[16];
  u8 current_savestate_filename[512];

  SDL_LockMutex(sound_mutex);
  SDL_PauseAudio(1);
  SDL_UnlockMutex(sound_mutex);

  if(gamepak_filename[0] == 0)
  {
    first_load = 1;
    memset(original_screen, 0x00, 240 * 160 * 2);
    print_string_ext("No game loaded yet.", 0xFFFF, 0x0000,
     60, 75,original_screen, 240);
  }

  video_resolution_large();

  clear_screen(COLOR_BG);
  blit_to_screen(original_screen, 240, 160, 230, 40);

  while(repeat)
  {
    flip_screen();

    strncpy(print_buffer, gamepak_filename, 80);
    print_string(print_buffer, COLOR_ROM_INFO, COLOR_BG, 10, 10);
    sprintf(print_buffer, "%s  %s  %s", gamepak_title,
     gamepak_code, gamepak_maker);
    print_string(print_buffer, COLOR_ROM_INFO, COLOR_BG, 10, 20);

    switch(current_menu)
    {
      case GAMEPAD_MENU:
        blit_to_screen(original_screen, 240, 160, 230, 40);
        print_string("PSP to GBA gamepad configuration:",
         COLOR_ACTIVE_ITEM, COLOR_BG, 10, 40);
        for(i = 0; i < 12; i++)
        {
          if(i == current_option)
          {
            print_string(gamepad_menu_lines[i], COLOR_ACTIVE_ITEM,
             COLOR_BG, 10, (i * 10) + 60);
            print_string(gamepad_config_buttons[gamepad_config_map[
             gamepad_config_line_to_psp_button[i]]],
             COLOR_ACTIVE_ITEM, COLOR_BG, 10 + (GAMEPAD_MENU_WIDTH * 6),
             (i * 10) + 60);
          }
          else
          {
            print_string(gamepad_menu_lines[i], COLOR_INACTIVE_ITEM,
             COLOR_BG, 10, (i * 10) + 60);
            print_string(gamepad_config_buttons[gamepad_config_map[
             gamepad_config_line_to_psp_button[i]]],
             COLOR_INACTIVE_ITEM, COLOR_BG, 10 + (GAMEPAD_MENU_WIDTH * 6),
             (i * 10) + 60);
          }
        }

        if(current_option == 12)
        {
          print_string("Return to the main configurator."
           "                                     ", COLOR_HELP_TEXT,
           COLOR_BG, 30, 240);
          print_string("Back", COLOR_ACTIVE_ITEM, COLOR_BG, 10, 190);
        }
        else
        {
          print_string(gamepad_help[gamepad_config_map[
           gamepad_config_line_to_psp_button[current_option]]],
           COLOR_HELP_TEXT, COLOR_BG, 30, 240);
          print_string("Back", COLOR_INACTIVE_ITEM, COLOR_BG, 10, 190);
        }
        break;

      case SAVESTATE_MENU:
        print_string("Savestate options:",
         COLOR_ACTIVE_ITEM, COLOR_BG, 10, 70);

        sprintf(menu_lines[0], "Load savestate from current slot");
        sprintf(menu_lines[1], "Save savestate to current slot");
        sprintf(menu_lines[2], "Load savestate from file");
        sprintf(menu_lines[3], "Current savestate slot: %d",
         current_savestate_slot);
        sprintf(menu_lines[4], "Back");

        for(i = 0; i < NUM_SAVESTATE_MENU_OPTIONS; i++)
        {
          if(i == current_option)
          {
            print_string(menu_lines[i], COLOR_ACTIVE_ITEM, COLOR_BG, 10,
             savestate_menu_line_positions[i]);
          }
          else
          {
            print_string(menu_lines[i], COLOR_INACTIVE_ITEM, COLOR_BG, 10,
             savestate_menu_line_positions[i]);
          }
        }

        print_string(savestate_menu_help[current_option], COLOR_HELP_TEXT,
         COLOR_BG, 30, 210);
        break;

      case MAIN_MENU:
        sprintf(menu_lines[MENU_DISPLAY_SCALE], "Display: %s",
         screen_ratio_options[display_mode]);
        if(frameskip_low > 0)
        {
          sprintf(menu_lines[MENU_FRAMESKIP],
           "Current frameskip: %d/%d", frameskip_low, frameskip);
        }
        else
        {
          sprintf(menu_lines[MENU_FRAMESKIP], "Current frameskip: %d   ",
           frameskip);
        }
        sprintf(menu_lines[MENU_FRAMESKIP_VARIATION],
         "Frameskip variation: %s",
         frameskip_variation_options[random_skip]);
        sprintf(menu_lines[MENU_SAVESTATE_LOAD],
         "Load state from slot %d", current_savestate_slot);
        sprintf(menu_lines[MENU_SAVESTATE_SAVE],
         "Save state to slot %d", current_savestate_slot);
        sprintf(menu_lines[MENU_SAVESTATE_OPTIONS], "Savestate options");
        sprintf(menu_lines[MENU_BACKUP_INTERVAL], "Update backup: %s",
         update_backup_options[update_backup_flag]);

        sprintf(menu_lines[MENU_CONFIG_GAMEPAD], "Configure gamepad input");

        sprintf(menu_lines[MENU_LOAD_GAME], "Load new game");
        sprintf(menu_lines[MENU_RESTART_GAME],
         "Restart currently running game");
        sprintf(menu_lines[MENU_RETURN], "Return to currently running game");
        sprintf(menu_lines[MENU_EXIT], "Exit gpSP");

        for(i = 0; i < NUM_MENU_OPTIONS; i++)
        {
          if(i == current_option)
          {
            print_string(menu_lines[i], COLOR_ACTIVE_ITEM, COLOR_BG, 10,
             menu_line_positions[i]);
          }
          else
          {
            print_string(menu_lines[i], COLOR_INACTIVE_ITEM, COLOR_BG, 10,
             menu_line_positions[i]);
          }
        }

        print_string(menu_help[current_option], COLOR_HELP_TEXT,
         COLOR_BG, 30, 210);
        break;
    }

    gui_action = get_gui_input();

    switch(current_menu)
    {
      case GAMEPAD_MENU:
        switch(gui_action)
        {
          case CURSOR_DOWN:
            current_option = (current_option + 1) % 13;
            break;

          case CURSOR_UP:
            if(current_option)
              current_option--;
            else
              current_option = 12;
            break;

          case CURSOR_RIGHT:
            gamepad_config_map[gamepad_config_line_to_psp_button[
             current_option]] =
            (gamepad_config_map[gamepad_config_line_to_psp_button[
             current_option]] + 1) % NUM_BUTTON_OPTIONS;
            break;

          case CURSOR_LEFT:
            if((gamepad_config_map[gamepad_config_line_to_psp_button[
             current_option]]))
            {
              gamepad_config_map[gamepad_config_line_to_psp_button[
               current_option]]--;
            }
            else
            {
              gamepad_config_map[gamepad_config_line_to_psp_button[
               current_option]] = NUM_BUTTON_OPTIONS - 1;
            }

            break;
  
          case CURSOR_EXIT:
            current_menu = MAIN_MENU;
            current_option = 0;
            clear_screen(COLOR_BG);
            blit_to_screen(original_screen, 240, 160, 230, 40);
            break;

          case CURSOR_SELECT:
            switch(current_option)
            {
              case 12:
                current_menu = MAIN_MENU;
                current_option = 0;
                clear_screen(COLOR_BG);
                blit_to_screen(original_screen, 240, 160, 230, 40);
                break;
            }
            break;
        }
        break;

      case SAVESTATE_MENU:
        switch(gui_action)
        {
          case CURSOR_DOWN:
            current_option = (current_option + 1) % NUM_SAVESTATE_MENU_OPTIONS;
            break;

          case CURSOR_UP:
            if(current_option)
              current_option--;
            else
              current_option = NUM_SAVESTATE_MENU_OPTIONS - 1;
            break;

          case CURSOR_RIGHT:
            current_savestate_slot = (current_savestate_slot + 1) % 10;
            get_savestate_filename(current_savestate_slot,
             current_savestate_filename);
            break;

          case CURSOR_LEFT:
            if(current_savestate_slot == 0)
              current_savestate_slot = 9;
            else
              current_savestate_slot--;

            get_savestate_filename(current_savestate_slot,
             current_savestate_filename);
            break;

          case CURSOR_EXIT:
            current_menu = MAIN_MENU;
            current_option = 0;
            clear_screen(COLOR_BG);
            blit_to_screen(original_screen, 240, 160, 230, 40);
            break;

          case CURSOR_SELECT:
            switch(current_option)
            {
              case 0:
                if(!first_load)
                {
                  load_state(current_savestate_filename);
                  return_value = 1;
                  repeat = 0;
                }
                break;

              case 1:
                if(!first_load)
                {
                  save_state(current_savestate_filename, original_screen);
                  get_savestate_filename(current_savestate_slot,
                   current_savestate_filename);
                }
                break;

              case 2:
              {
                u8 *file_ext[] = { ".svs", NULL };
                u8 load_filename[512];
                if(load_file(file_ext, load_filename) != -1)
                {
                  load_state(load_filename);
                  return_value = 1;
                  repeat = 0;
                }
                else
                {
                  clear_screen(COLOR_BG);
                  blit_to_screen(original_screen, 240, 160, 230, 40);
                }
                break;
              }

              case 4:
                current_menu = MAIN_MENU;
                current_option = 0;
                clear_screen(COLOR_BG);
                blit_to_screen(original_screen, 240, 160, 230, 40);
                break;
            }
            break;
        }
        break;

      case MAIN_MENU:
        switch(gui_action)
        {
          case CURSOR_DOWN:
            current_option = (current_option + 1) % NUM_MENU_OPTIONS;
            break;

          case CURSOR_UP:
            if(current_option)
              current_option--;
            else
              current_option = NUM_MENU_OPTIONS - 1;
            break;
  
          case CURSOR_RIGHT:
            switch(current_option)
            {
              case MENU_DISPLAY_SCALE:
                display_mode = (display_mode + 1) % 3;
                break;

              case MENU_FRAMESKIP:
                if(frameskip_low > 0)
                {
                  frameskip_low--;
                  if(frameskip_low == 0)
                    frameskip = 0;
                  else
                    frameskip = frameskip_low + 1;
                }
                else

                if(frameskip != 99)
                {
                  frameskip++;
                }
                break;
  
              case MENU_FRAMESKIP_VARIATION:
                random_skip ^= 1;
                break;

              case MENU_BACKUP_INTERVAL:
                update_backup_flag ^= 1;
                break;

              case MENU_SAVESTATE_LOAD:
              case MENU_SAVESTATE_SAVE:
                current_savestate_slot = (current_savestate_slot + 1) % 10;
                break;
            }
            break;

          case CURSOR_LEFT:
            switch(current_option)
            {
              case MENU_DISPLAY_SCALE:
                if(display_mode)
                  display_mode--;
                else
                  display_mode = 2;
                break;

              case MENU_FRAMESKIP:
                if(frameskip == 0)
                {
                  frameskip_low = 1;
                  frameskip = 2;
                }
                else

                if(frameskip_low > 0)
                {
                  if(frameskip_low < 8)
                  {
                    frameskip_low++;
                    frameskip = frameskip_low + 1;
                  }
                }
                else
                {
                  frameskip--;
                }
                break;

              case MENU_FRAMESKIP_VARIATION:
                random_skip ^= 1;
                break;

              case MENU_BACKUP_INTERVAL:
                update_backup_flag ^= 1;
                break;

              case MENU_SAVESTATE_LOAD:
              case MENU_SAVESTATE_SAVE:
                if(current_savestate_slot == 0)
                  current_savestate_slot = 9;
                else
                  current_savestate_slot--;
                break;
            }
            break;

          case CURSOR_EXIT:
            if(!first_load)
              repeat = 0;
            break;

          case CURSOR_SELECT:
            switch(current_option)
            {
              case MENU_SAVESTATE_LOAD:
                if(!first_load)
                {
                  get_savestate_filename_noshot(current_savestate_slot,
                   current_savestate_filename);
                  load_state(current_savestate_filename);
                  return_value = 1;
                  repeat = 0;
                }
                break;

              case MENU_SAVESTATE_SAVE:
                if(!first_load)
                {
                  get_savestate_filename_noshot(current_savestate_slot,
                   current_savestate_filename);
                  save_state(current_savestate_filename, original_screen);
                }
                break;

              case MENU_SAVESTATE_OPTIONS:
                current_menu = SAVESTATE_MENU;
                current_option = 0;
                clear_screen(COLOR_BG);

                get_savestate_filename(current_savestate_slot,
                 current_savestate_filename);
                break;

              case MENU_CONFIG_GAMEPAD:
                current_menu = GAMEPAD_MENU;
                current_option = 0;
                clear_screen(COLOR_BG);
                blit_to_screen(original_screen, 240, 160, 230, 40);
                break;

              case MENU_LOAD_GAME:
              {
                u8 *file_ext[] = { ".gba", ".bin", ".zip", NULL };
                u8 load_filename[512];
                save_game_config_file();
                if(load_file(file_ext, load_filename) != -1)
                {
                  if(load_gamepak(load_filename) == -1)
                  {
                    quit();
                  }
                  reset_gba();
                  return_value = 1;
                  repeat = 0;
                  reg[CHANGED_PC_STATUS] = 1;
                }
                else
                {
                  clear_screen(COLOR_BG);
                  blit_to_screen(original_screen, 240, 160, 230, 40);
                }
                break;
              }

              case MENU_RESTART_GAME:
                if(!first_load)
                {
                  reset_gba();
                  reg[CHANGED_PC_STATUS] = 1;
                  return_value = 1;
                  repeat = 0;
                }
                break;

              case MENU_RETURN:
                if(!first_load)
                  repeat = 0;
                break;

              case MENU_EXIT:
              {
                save_config_file();
                quit();
              }
            }
            break;
        }
        break;
    }
  }

  clear_screen(0x0000);
  set_gba_resolution(display_mode);
  video_resolution_small();

  SDL_PauseAudio(0);

  return return_value;
}

u32 adjust_frameskip(u32 button_id)
{
  u8 fs_string[41];
  u32 return_value = 0;
  gui_action_type gui_action;
  u16 *original_screen = copy_screen();

  SDL_LockMutex(sound_mutex);
  SDL_PauseAudio(1);
  SDL_UnlockMutex(sound_mutex);

  do
  {
    flip_screen();
    if(frameskip_low > 0)
    {
      sprintf(fs_string, "Current frameskip: %d/%d (up/dwn to alter)",
       frameskip_low, frameskip);
    }
    else
    {
      sprintf(fs_string, "Current frameskip: %02d (up/down to alter)",
       frameskip);
    }
    print_string(fs_string, COLOR_ACTIVE_ITEM, COLOR_FRAMESKIP_BAR, 0, 0);
    print_string("      Press right for more options      ",
     COLOR_ACTIVE_ITEM, COLOR_FRAMESKIP_BAR, 0, 150);

    gui_action = get_gui_input_fs_hold(button_id);

    switch(gui_action)
    {
      case CURSOR_DOWN:
        if(frameskip == 0)
        {
          frameskip_low = 1;
          frameskip = 2;
        }
        else

        if(frameskip_low > 0)
        {
          if(frameskip_low < 8)
          {
            frameskip_low++;
            frameskip = frameskip_low + 1;
          }
        }
        else
        {
          frameskip--;
        }
        break;

      case CURSOR_UP:
        if(frameskip_low > 0)
        {
          frameskip_low--;
          if(frameskip_low == 0)
            frameskip = 0;
          else
            frameskip = frameskip_low + 1;
        }
        else

        if(frameskip != 99)
        {
          frameskip++;
        }
        break;

      case CURSOR_RIGHT:
        return_value = menu(original_screen);
        gui_action = CURSOR_BACK;
        break;
    }
  } while(gui_action != CURSOR_BACK);

  SDL_PauseAudio(0);

  free(original_screen);

  return return_value;
}
