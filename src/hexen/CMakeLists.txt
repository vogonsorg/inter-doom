if(COMPILE_HEXEN)
    add_library(hexen STATIC
                a_action.c
                                    am_data.h
                am_map.c            am_map.h
                ct_chat.c
                                    ct_chat.h
                d_net.c
                f_finale.c
                g_game.c
                                    h2def.h
                h2_main.c
                info.c              info.h
                in_lude.c
                m_random.c          m_random.h
                mn_menu.c
                p_acs.c
                p_anim.c
                p_ceilng.c
                p_doors.c
                p_enemy.c
                p_floor.c
                p_inter.c
                p_lights.c
                                    p_local.h
                p_map.c
                p_maputl.c
                p_mobj.c
                po_man.c
                p_plats.c
                p_pspr.c
                p_setup.c
                p_sight.c
                p_spec.c           p_spec.h
                p_switch.c
                p_telept.c
                p_things.c
                p_tick.c
                p_user.c
                r_bmaps.c          r_bmaps.h
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
                rd_psx.c            rd_psx.h
                rd_rushexen.c       rd_rushexen.h
                s_sound.c           s_sound.h
                sb_bar.c
                sc_man.c
                sn_sonix.c
                sounds.c            sounds.h
                st_start.c          st_start.h
                sv_save.c
                                    xddefs.h
    )
    set_target_properties(hexen PROPERTIES
        C_STANDARD "${RD_C_STANDARD}"
        C_STANDARD_REQUIRED ON
    )
    target_compile_definitions(hexen PRIVATE
        BUILD_HEXEN_VERSION="${BUILD_HEXEN_VERSION}"
        PACKAGE_PREFIX="${PACKAGE_PREFIX}"
        PROGRAM_PREFIX="${PROGRAM_PREFIX}"
    )
    target_link_libraries(hexen PRIVATE
        SDL2::SDL2
        SDL2::mixer
        SDL2::net
        Game
    )
    if(NOT NO_GIT_HASH)
        add_dependencies(hexen revision_check)
    endif()
endif()
