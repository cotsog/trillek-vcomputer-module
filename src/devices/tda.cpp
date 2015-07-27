/**
 * \brief       Virtual Computer Text Display Adapter
 * \file        tda.cpp
 * \copyright   LGPL v3
 *
 * Nya Elektriska Text Display Adapter
 * @see https://github.com/trillek-team/trillek-computer/blob/master/TDA.md
 */

#include "devices/tda.hpp"
#include "vs_fix.hpp"

#include <algorithm>
#include <cassert>

namespace trillek {
namespace computer {
namespace tda {

void TDAtoRGBATexture (const TDAScreen& screen, DWord* texture) {
    static unsigned frames = 0;
    TDAtoRGBATexture(screen, texture, frames);
}

void TDAtoBGRATexture (const TDAScreen& screen, DWord* texture) {
    static unsigned frames = 0;
    TDAtoBGRATexture(screen, texture, frames);
}

void TDAtoRGBATexture (const TDAScreen& screen, DWord* texture, unsigned& frames) {
    assert(texture != nullptr);

    const Byte* font = ROM_FONT;
    if (screen.user_font) {
        font = (Byte*) screen.font_buffer;
    }

    // TODO Rewrite this to be more efficient and cache friendly, as now
    // writes "jumping" in the output texture

    // Reads one by one each character of the text buffer
    for (unsigned row = 0; row < HEIGHT_CHARS; row++) {
        for (unsigned col = 0; col < WIDTH_CHARS; col++) {

            std::size_t addr = col + (WIDTH_CHARS * row);
            Byte c         = screen.txt_buffer[addr]; // character

            // Get Ink (fg) and Paper (bg) colors
            DWord fg = (screen.txt_buffer[addr] >> 8) & 0x0F; // Bits 8-11
            DWord bg = (screen.txt_buffer[addr] >> 12)& 0x0F; // bits 12-15

            // Paint the texture
            Byte pixels;
            for (unsigned y = 0; y < 8; y++) {
                pixels = font[c*8 + y];
                for (unsigned x = 0; x < 8; x++) {
                    addr = x + col*8 + ( 40*8 * (y + row*8) ); // Addres of the
                                                               // pixel in the
                                                               // buffer
                    if ( ( pixels & ( 1 << (7-x) ) ) != 0 ) {
                        // Active, uses the Ink (fg)
                        texture[addr] = PALETTE[fg];
                    }
                    else {
                        // Unactive, uses the Paper (bg)
                        texture[addr] = PALETTE[bg];
                    }
                }
            }
        }
    } // End for

    if ( screen.cursor) {
        if (frames++ < 8) {
            // Draw the cursor only when is necesary
            if (screen.cur_start <= screen.cur_end) {
                unsigned char col = screen.cur_col;
                unsigned char row = screen.cur_row;
                DWord color = PALETTE[screen.cur_color]; // Color
                if (row < 30 && col < 40) {
                    // Paints the cursor
                    std::size_t addr = col + (WIDTH_CHARS * row);
                    for (unsigned y = screen.cur_start ; y <= screen.cur_end; y++) {
                        for (unsigned x = 0; x < 8; x++) {
                            addr = x + col*8 + ( 40*8 * (y + row*8) ); // Addres of the
                                                                       // pixel in the
                                                                       // buffer
                            texture[addr] = color;
                        }
                    }
                }
            }
        } else if (frames++ < 16) {
            // Do nothing
        } else {
            frames = 0; // Reset it
        }
    }

} // TDAtoRGBATexture

void TDAtoBGRATexture (const TDAScreen& screen, DWord* texture, unsigned& frames) {
    assert(texture != nullptr);
    TDAtoRGBATexture (screen, texture, frames);

    // We interchanged B and R components
    for (unsigned i=0; i < 320*240 ; i++) {
        DWord g_a   = texture[i] & 0xFF00FF00;
        DWord red   = texture[i] & 0x000000FF;
        DWord blue  = texture[i] & 0x00FF0000;
        texture[i]  = g_a | (red << 16) | (blue >> 16);
    }
} // TDAtoBGRATexture
TDADev::TDADev () : buffer_ptr(0), font_ptr(0), vsync_msg(0), do_vsync(false),
                    cursor(false), blink(false) {
}

TDADev::~TDADev() {
}

void TDADev::Reset () {
    this->buffer_ptr = 0;
    this->font_ptr   = 0;
    this->vsync_msg  = 0;
    this->a          = 0;
    this->b          = 0;
    this->d          = 0;
    this->e          = 0;
    this->do_vsync   = false;
    this->cursor     = false;
    this->blink      = false;
}

void TDADev::SendCMD (Word cmd) {
    DWord tmp;

    switch (cmd) {
    case 0x0000: // Map Buffer
        tmp = ( (b << 16) | a );
        if ( tmp + TXT_BUFFER_SIZE < vcomp->RamSize() ) {
            buffer_ptr = tmp;
        }
        break;

    case 0x0001: // Map Font
        tmp = ( (b << 16) | a );
        if ( tmp + FONT_BUFFER_SIZE <= vcomp->RamSize() ) {
            font_ptr = tmp;
        }
        else if ( tmp - 0x100000 + FONT_BUFFER_SIZE <= vcomp->RomSize()) {
            font_ptr = tmp;
        }
        break;

    case 0x0002: // Set Int
        vsync_msg = a;
        break;

    default:
        break;
    } // switch
}     // SendCMD

bool TDADev::DoesInterrupt(Word& msg) {
    if (do_vsync && vsync_msg != 0x0000) {
        msg = vsync_msg;
        return true;
    }
    return false;
}

void TDADev::IACK () {
    do_vsync = false; // Acepted, so we can forgot now of sending it again
}

void TDADev::GetState (DeviceState* out) const {
    Device::GetState(out);

    out->set_a(this->a);
    out->set_b(this->b);
    out->set_d(this->d);
    out->set_e(this->e);

    out->clear_properties();

    // int_msg
    auto int_msg = out->add_properties();
    int_msg->set_type(DeviceState_DeviceProperty_Type_INT);
    int_msg->set_name("vsync_msg");
    int_msg->set_ivalue(this->vsync_msg);

    // doing interrupt?
    auto do_int = out->add_properties();
    do_int->set_type(DeviceState_DeviceProperty_Type_BOOL);
    do_int->set_name("do_vsync");
    do_int->set_bvalue(this->do_vsync);

    // Cursor enabled?
    auto cursor = out->add_properties();
    cursor->set_type(DeviceState_DeviceProperty_Type_BOOL);
    cursor->set_name("cursor");
    cursor->set_bvalue(this->cursor);

    // Blink enabled?
    auto blink = out->add_properties();
    blink->set_type(DeviceState_DeviceProperty_Type_BOOL);
    blink->set_name("blink");
    blink->set_bvalue(this->blink);

    // buffer_ptr
    if (this->buffer_ptr != 0) {
        auto b_ptr = out->add_properties();
        b_ptr->set_type(DeviceState_DeviceProperty_Type_INT);
        b_ptr->set_name("buffer_ptr");
        b_ptr->set_ivalue(this->buffer_ptr);
    }

    // font_ptr
    if (this->font_ptr != 0) {
        auto font_ptr = out->add_properties();
        font_ptr->set_type(DeviceState_DeviceProperty_Type_INT);
        font_ptr->set_name("font_ptr");
        font_ptr->set_ivalue(this->font_ptr);
    }

} // GetState

bool TDADev::SetState (const DeviceState* in) {
    if (! Device::SetState(in)) {
        return false;
    }

    this->a = in->a();
    this->b = in->b();
    this->d = in->d();
    this->e = in->e();

    for(int i=0; i < in->properties_size(); i++) {
        auto prop = in->properties(i);
        if (0 == prop.name().compare("vsync_msg")) {
            // Interrupt message
            this->vsync_msg = prop.ivalue();

        } else if (0 == prop.name().compare("do_vsync")) {
            // doing interrupt?
            this->do_vsync = prop.bvalue();

        } else if (0 == prop.name().compare("cursor")) {
            this->cursor = prop.bvalue();

        } else if (0 == prop.name().compare("blink")) {
            this->blink = prop.bvalue();

        } else if (0 == prop.name().compare("buffer_ptr")) {
            this->buffer_ptr = prop.ivalue();

        } else if (0 == prop.name().compare("font_ptr")) {
            this->buffer_ptr = prop.ivalue();
        }
    }

    return true;
} // SetState

bool TDADev::IsSyncDev() const {
    return false;
}

} // End of namespace tda
} // End of namespace computer
} // End of namespace trillek

