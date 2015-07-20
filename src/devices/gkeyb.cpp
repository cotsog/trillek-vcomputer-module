/**
 * \brief       Virtual Computer generic keyboard
 * \file        gkeyb.cpp
 * \copyright   LGPL v3
 *
 * Generic Western/Latin Keyboard
 * @see https://github.com/trillek-team/trillek-computer/blob/master/Keyboard.md
 */

#include "devices/gkeyb.hpp"
#include "vs_fix.hpp"

namespace trillek {
namespace computer {
namespace gkeyboard {

GKeyboardDev::GKeyboardDev () : int_msg(0), do_int(false) {
    keybuffer.clear();
}

GKeyboardDev::~GKeyboardDev() {
}

void GKeyboardDev::Reset () {
    a = 0;
    b = 0;
    c = 0;

    keybuffer.clear();

    int_msg = 0;
    do_int  = false;
}

bool GKeyboardDev::DoesInterrupt(Word& msg) {
    if (do_int && int_msg != 0x0000) {
        msg = int_msg;
        return true;
    }
    return false;
}

/**
 * Sends (writes to CMD register) a command to the device
 * @param cmd Command value to send
 */
void GKeyboardDev::SendCMD (Word cmd) {
    switch (cmd) {
    case 0x0000: // CLR_BUFFER
        keybuffer.clear();
        break;

    case 0x0001: // PULL_KEY
        if ( !keybuffer.empty() ) {
            // keyevent = ((status & 7) << 24) | (keycode << 16) | scancode;
            auto tmp = keybuffer.front();
            keybuffer.pop_front();
            c = tmp >> 24;
            b = tmp & 0xFFFF;
            a = (tmp >> 16) & 0xFF;
        }
        else {
            a = b = c = 0;
        }
        break;

    case 0x0002: // PUSH_KEY
        if (keybuffer.size() < BSIZE) {
            DWord keyevent = ( (c & 7) << 24 ) | ( (a & 0xFF) << 16 ) | b;
            keybuffer.push_front(keyevent);
        }
        break;

    case 0x0003: // SET_INT
        int_msg = a;
        break;

    default:
        break;
    } // switch
}     // SendCMD

void GKeyboardDev::IACK () {
    do_int = false; // Acepted, so we can forgot now of sending it again
}


void GKeyboardDev::GetState (DeviceState* out) const {
    Device::GetState(out);

    out->set_a(this->a);
    out->set_b(this->b);
    out->set_c(this->c);

    out->clear_properties();

    // int_msg
    auto int_msg = out->add_properties();
    int_msg->set_type(DeviceState_DeviceProperty_Type_INT);
    int_msg->set_name("int_msg");
    int_msg->set_ivalue(this->int_msg);
    // doing interrupt?
    auto do_int = out->add_properties();
    do_int->set_type(DeviceState_DeviceProperty_Type_BOOL);
    do_int->set_name("do_int");
    do_int->set_bvalue(this->do_int);

    if (! this->keybuffer.empty()) {
        auto keybuffer = out->add_properties();
        keybuffer->set_type(DeviceState_DeviceProperty_Type_RAW);
        keybuffer->set_name("keybuffer");

        // Copy data
        DWord* buff = new DWord[this->keybuffer.size()+1]();
        for( std::size_t i=0; i < this->keybuffer.size(); i++) {
            buff[i] = this->keybuffer[i];
        }
        keybuffer->set_rvalue((void*) buff, this->keybuffer.size()*4);
        delete[](buff);
    }
} // GetState

bool GKeyboardDev::SetState (const DeviceState* in) {
    if (! Device::SetState(in)) {
        return false;
    }

    this->a = in->a();
    this->b = in->b();
    this->c = in->c();

    this->keybuffer.clear();

    for(int i=0; i < in->properties_size(); i++) {
        auto prop = in->properties(i);
        if (0 == prop.name().compare("int_msg")) {
            // Interrupt message
            this->int_msg = prop.ivalue();
        } else if (0 == prop.name().compare("do_int")) {
            // doing interrupt?
            this->do_int = prop.bvalue();
        } else if (0 == prop.name().compare("keybuffer")) {
            DWord* buff = (DWord*) prop.rvalue().c_str();
            std::size_t len = prop.rvalue().length() / 4;
            for( std::size_t i=0; i < len; i++) {
                this->keybuffer.push_back( buff[i]);
            }
        }
    }

    return true;
} // SetState


} // End of namespace gkeyboard
} // End of namespace computer
} // End of namespace trillek
