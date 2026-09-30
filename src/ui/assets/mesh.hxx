#pragma once

namespace egui {

    inline const unsigned char egui_model_obj[] = {
        0x00
    };

    inline bool egui_model_embedded() { return sizeof(egui_model_obj) > 64; }

}
