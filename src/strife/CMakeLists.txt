if(COMPILE_STRIFE)
    add_library(strife STATIC
            am_map.c        am_map.h
            deh_ammo.c
            deh_cheat.c
            deh_strife.c
            deh_frame.c
            deh_misc.c      deh_misc.h
            deh_ptr.c
            deh_sound.c
            deh_thing.c
            deh_weapon.c
            d_items.c       d_items.h
            d_main.c        d_main.h
            d_net.c
            doomdata.h
            doomdef.c       doomdef.h
            doomstat.c      doomstat.h
                            d_player.h
                            d_textur.h
                            d_think.h
            f_finale.c      f_finale.h
            f_wipe.c        f_wipe.h
            g_game.c        g_game.h
            hu_lib.c        hu_lib.h
            hu_stuff.c      hu_stuff.h
            info.c          info.h
            m_menu.c        m_menu.h
            m_random.c      m_random.h
            m_saves.c       m_saves.h
            p_ceilng.c
            p_dialog.c      p_dialog.h
            p_doors.c
            p_enemy.c
            p_floor.c
            p_inter.c       p_inter.h
            p_lights.c
                            p_local.h
            p_map.c
            p_maputl.c
            p_mobj.c        p_mobj.h
            p_plats.c
            p_pspr.c        p_pspr.h
            p_saveg.c       p_saveg.h
            p_setup.c       p_setup.h
            p_sight.c
            p_spec.c        p_spec.h
            p_switch.c
            p_telept.c
            p_tick.c        p_tick.h
            p_user.c
            r_bsp.c         r_bsp.h
            r_data.c        r_data.h
            r_defs.h
            r_draw.c        r_draw.h
                            r_local.h
            r_main.c        r_main.h
            r_plane.c       r_plane.h
            r_segs.c        r_segs.h
            r_sky.c         r_sky.h
                            r_state.h
            r_things.c      r_things.h
            rd_lang.c       rd_lang.h
            s_sound.c       s_sound.h
            sounds.c        sounds.h
            st_lib.c        st_lib.h
            st_stuff.c      st_stuff.h
            wi_stuff.c      wi_stuff.h
    )
    set_target_properties(strife PROPERTIES
        C_STANDARD "${RD_C_STANDARD}"
        C_STANDARD_REQUIRED ON
    )
    target_compile_definitions(strife PRIVATE
        BUILD_STRIFE_VERSION="${BUILD_STRIFE_VERSION}"
        PACKAGE_PREFIX="${PACKAGE_PREFIX}"
        PROGRAM_PREFIX="${PROGRAM_PREFIX}"
    )
    target_link_libraries(strife PRIVATE
        textscreen
        SDL2::SDL2
        SDL2::mixer
        SDL2::net
        Game
    )
    if(NOT NO_GIT_HASH)
        add_dependencies(strife revision_check)
    endif()
endif()
