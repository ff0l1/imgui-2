#pragma once

class c_variables
{
public:

    struct
    {
        int menu_key = 0x2D;
        bool streamproof = true;
        int active_tab = 0;
        int active_subtab[6] = { 0, 0, 0, 0, 0, 0 };
        float tab_transition_alpha = 1.0f;
        float subtab_transition_alpha = 1.0f;

        bool show_playerlist = true;
        bool show_active_hotkeys = true;
        bool show_watermark = true;
    } gui;

    struct
    {
        bool enabled = false;

        bool box = false;
        float box_color[4] = { 214.0f / 255.0f, 120.0f / 255.0f, 52.0f / 255.0f, 1.0f };
        float box_thickness = 2.0f;
        int box_style = 0;
        bool box_gradient = false;
        float box_color_secondary[4] = { 1.0f, 0.5f, 0.8f, 1.0f };
        bool box_filled = false;
        float box_filled_top_color[4] = { 1.0f, 0.0f, 0.0f, 0.3f };
        float box_filled_bottom_color[4] = { 0.0f, 0.0f, 1.0f, 0.3f };

        bool box_glow = false;
        float box_glow_color[4] = { 0.5f, 0.0f, 1.0f, 0.5f };
        float box_glow_strength = 1.0f;

        bool name = false;
        float name_color[4] = { 1.0f, 1.0f, 1.0f, 1.0f };
        float name_size = 12.0f;
        int name_flags = 0;

        bool health = false;
        float health_width = 3.0f;
        float health_color[4] = { 0.0f, 1.0f, 0.0f, 1.0f };

        bool health_text = false;
        float health_text_color[4] = { 1.0f, 1.0f, 1.0f, 1.0f };

        bool flags_enabled = false;
        bool flag_options[10] = { false };
        float flags_spacing = 12.0f;
        float flags_font_size = 10.0f;

        bool distance = false;
        float distance_color[4] = { 1.0f, 1.0f, 1.0f, 1.0f };

        bool weapon = false;
        float weapon_color[4] = { 1.0f, 1.0f, 0.0f, 1.0f };

        bool radar_enabled = false;
        bool radar_show_names = true;
        bool radar_show_distance = true;
        float radar_size = 200.0f;
        float radar_range = 150.0f;
        float radar_alpha = 0.9f;

        bool oov_arrows = false;
        int oov_arrow_style = 0;
        float oov_arrow_size = 20.0f;
        float oov_arrow_distance = 100.0f;
        float oov_arrow_color[4] = { 1.0f, 1.0f, 1.0f, 1.0f };

        bool snaplines = false;
        int snaplines_position = 1;
        float snaplines_color[4] = { 1.0f, 1.0f, 1.0f, 0.8f };
        float snaplines_thickness = 1.0f;
        bool snaplines_gradient = false;
        float snaplines_gradient_color[4] = { 0.5f, 0.5f, 1.0f, 0.3f };
    } esp;

    struct
    {
        bool enabled = false;

        bool aim_key_active = false;
        int aim_key = -1;
        int aim_key_mode = 1;

        bool fov_check = true;
        float fov_radius = 100.0f;

        bool fov_circle_enabled = true;
        int fov_style = 0;
        float fov_color[4] = { 214.0f / 255.0f, 120.0f / 255.0f, 52.0f / 255.0f, 0.6f };
        bool fov_filled = false;

        bool crosshair_enabled = true;
        float crosshair_size = 8.0f;
        float crosshair_color[4] = { 1.0f, 1.0f, 1.0f, 0.8f };

        float smoothness = 5.0f;
        bool aim_at_head = true;
        bool aim_at_torso = false;

        bool show_target_indicator = true;
        bool draw_line_to_target = false;
        float target_line_color[4] = { 1.0f, 0.2f, 0.2f, 0.6f };

        bool target_team = false;
        bool target_knocked = false;
        float max_distance = 500.0f;

        bool teamcheck = true;

        bool pf_silent_enabled = false;
        bool pf_silent_auto_shoot = false;
        float pf_silent_smoothness = 1.0f;
        bool pf_silent_prediction = false;
        float pf_silent_prediction_amount = 0.5f;

        bool trigger_enabled = false;
        bool trigger_always = false;
        int trigger_key = 0;
        int trigger_key_mode = 1;
        bool trigger_active = false;
        float trigger_delay_ms = 40.0f;

    } aimbot;

    struct
    {
        bool world_ambient_enabled = false;
        float world_ambient_color[3] = { 1.0f, 1.0f, 1.0f };
        float world_outdoor_ambient_color[3] = { 1.0f, 1.0f, 1.0f };

        bool world_atmosphere_enabled = false;
        float world_atmos_color[3] = { 0.75f, 0.85f, 1.0f };
        float world_atmos_decay[3] = { 0.4f, 0.4f, 0.4f };
        float world_atmos_density = 0.0f;
        float world_atmos_glare = 0.0f;
        float world_atmos_haze = 0.0f;
        float world_atmos_offset = 0.0f;

        bool world_fog_enabled = false;
        float world_fog_start = 0.0f;
        float world_fog_end = 100000.0f;
        float world_fog_color[3] = { 0.75f, 0.75f, 0.75f };

        bool world_brightness_enabled = false;
        float world_brightness = 5.0f;

        bool world_exposure_enabled = false;
        float world_exposure = 0.0f;

        bool world_fov_enabled = false;
        float world_fov = 70.0f;

        bool world_shadows_disabled = false;

        bool world_clocktime_enabled = false;
        float world_clocktime_value = 12.0f;

        bool particles_enabled = false;
        int particle_style = 0;
        int particle_count = 300;
        float particle_speed = 1.0f;
        float particle_wind = 1.0f;
        float particle_glow = 1.0f;

    } world;

    struct
    {
        bool kill_effects_enabled = false;
        int kill_effects_type = 1;
        int kill_effects_particle_count = 50;
        float kill_effects_lifetime = 3.0f;
        bool kill_effects_glow = true;
        bool kill_effects_test = false;

        bool hitbox_expander_enabled = false;
        float hitbox_expander_size = 2.0f;
        bool hitbox_expander_no_collide = true;
        bool hitbox_expander_transparent = false;
        float hitbox_expander_transparency = 0.7f;
        bool hitbox_expander_visualize = false;

        bool pf_fly = false;
        int pf_fly_key = 'G';
        int pf_fly_key_mode = 1;
        float pf_fly_speed = 25.0f;

        bool jump_power = false;
        float jump_power_value = 50.0f;

        bool third_person = false;
        float third_person_distance = 15.0f;

    } misc;

};

inline c_variables vars;
