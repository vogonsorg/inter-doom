if(COMPILE_HERETIC)
    add_library(heretic STATIC
                am_map.c            am_map.h
                ct_chat.c           ct_chat.h
                deh_ammo.c
                deh_frame.c
                deh_htext.c
                deh_htic.c          deh_htic.h
                deh_sound.c
                deh_thing.c
                deh_weapon.c
                d_main.c
                d_net.c
                                    doomdata.h
                                    doomdef.h
                f_finale.c
                g_game.c
                info.c              info.h
                in_lude.c
                m_random.c          m_random.h
                mn_menu.c
                                    p_action.h
                p_ceilng.c
                p_doors.c
                p_enemy.c
                p_fix.c             p_fix.h
                p_floor.c
                p_inter.c
                p_lights.c
                                    p_local.h
                p_map.c
                p_maputl.c
                p_mobj.c
                p_plats.c
                p_pspr.c
                p_saveg.c
                p_setup.c
                p_sight.c
                p_spec.c            p_spec.h
                p_switch.c
                p_telept.c
                p_tick.c
                p_user.c
                r_bmaps.c           r_bmaps.h
                r_bsp.c
                r_data.c
                r_draw.c
                                    r_local.h
                r_main.c
                r_plane.c
                r_segs.c
                r_swirl.c           r_swirl.h
                r_things.c
                rd_lang.c           rd_lang.h
                sb_bar.c
                sounds.c            sounds.h
                s_sound.c           s_sound.h
    )
    set_target_properties(heretic PROPERTIES
        C_STANDARD "${RD_C_STANDARD}"
        C_STANDARD_REQUIRED ON
    )
    target_compile_definitions(heretic PRIVATE
        BUILD_HERETIC_VERSION="${BUILD_HERETIC_VERSION}"
        PACKAGE_PREFIX="${PACKAGE_PREFIX}"
        PROGRAM_PREFIX="${PROGRAM_PREFIX}"
    )
    target_link_libraries(heretic PRIVATE
        textscreen
        SDL2::SDL2
        SDL2::mixer
        SDL2::net
        Game
    )
    if(NOT NO_GIT_HASH)
        add_dependencies(heretic revision_check)
    endif()
endif()
